# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6119/7577 lines (80.76%)

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
|  912588 |   161 | `static void GenStateInitBlock(` |
|       - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   166 | `	void *pUserData      /* Upper layer private data */` |
|       - |   167 | `	)` |
|       5 |   168 | `{` |
|       - |   169 | `	/* Initialize block fields */` |
|  912593 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  912593 |   171 | `	pBlock->pUserData   = pUserData;` |
|  912593 |   172 | `	pBlock->pGen        = pGen;` |
|  912593 |   173 | `	pBlock->iFlags      = iType;` |
|  912593 |   174 | `	pBlock->pParent     = 0;` |
|  912593 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  912593 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  912593 |   177 | `}` |
|       - |   178 | `/*` |
|       - |   179 | ` * Allocate a new block instance.` |
|       - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   182 | ` * processing on failure.` |
|       - |   183 | ` */` |
|  908744 |   184 | `static sxi32 GenStateEnterBlock(` |
|       - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   190 | `	)` |
|       5 |   191 | `{` |
|       - |   192 | `	GenBlock *pBlock;` |
|       - |   193 | `	/* Allocate a new block instance */` |
|  908749 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  908749 |   195 | `	if( pBlock == 0 ){` |
|       - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   198 | `		 */` |
|     ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   200 | `		/* Abort processing immediately */` |
|     ! 0 |   201 | `		return SXERR_ABORT;` |
|       - |   202 | `	}` |
|       - |   203 | `	/* Zero the structure */` |
|  908749 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  908749 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   206 | `	/* Link to the parent block */` |
|  908749 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   208 | `	/* Mark as the current block */` |
|  908749 |   209 | `	pGen->pCurrent = pBlock;` |
|  908749 |   210 | `	if( ppBlock ){` |
|       - |   211 | `		/* Write a pointer to the new instance */` |
|  439567 |   212 | `		*ppBlock = pBlock;` |
|  219781 |   213 | `	}` |
|  908749 |   214 | `	return SXRET_OK;` |
|  454377 |   215 | `}` |
|       - |   216 | `/*` |
|       - |   217 | ` * Release block fields without freeing the whole instance.` |
|       - |   218 | ` */` |
|  908736 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   220 | `{` |
|  908741 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  908741 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  908741 |   223 | `}` |
|       - |   224 | `/*` |
|       - |   225 | ` * Release a block.` |
|       - |   226 | ` */` |
|  908736 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   228 | `{` |
|  908741 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  908741 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   231 | `	/* Free the instance */` |
|  908741 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  908741 |   233 | `}` |
|       - |   234 | `/*` |
|       - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   236 | ` */` |
|  908736 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   238 | `{` |
|  908741 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  908741 |   240 | `	if( pBlock == 0 ){` |
|       - |   241 | `		/* No more block to pop */` |
|     ! 0 |   242 | `		return SXERR_EMPTY;` |
|       - |   243 | `	}` |
|       - |   244 | `	/* Point to the upper block */` |
|  908741 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  908741 |   246 | `	if( ppBlock ){` |
|       - |   247 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   248 | `		*ppBlock = pBlock;` |
|     ! 0 |   249 | `	}else{` |
|       - |   250 | `		/* Safely release the block */` |
|  908741 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   252 | `	}` |
|  908741 |   253 | `	return SXRET_OK;` |
|  454373 |   254 | `}` |
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
|  259724 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   266 | `{` |
|       - |   267 | `	JumpFixup sJumpFix;` |
|       - |   268 | `	sxi32 rc;` |
|       - |   269 | `	/* Init the JumpFixup structure */` |
|  259729 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  259729 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   272 | `	/* Insert in the jump fixup table */` |
|  259729 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  259729 |   274 | `	return rc;` |
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
|  633388 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   288 | `{` |
|       - |   289 | `	JumpFixup *aFix;` |
|       - |   290 | `	VmInstr *pInstr;` |
|       - |   291 | `	sxu32 nFixed;` |
|       - |   292 | `	sxu32 n;` |
|       - |   293 | `	/* Point to the jump fixup table */` |
|  633393 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   295 | `	/* Fix the desired jumps */` |
| 1140217 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  506829 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   298 | `			/* Already fixed */` |
|  200115 |   299 | `			continue;` |
|       - |   300 | `		}` |
|  306719 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   302 | `			/* Not of our interest */` |
|   46997 |   303 | `			continue;` |
|       - |   304 | `		}` |
|       - |   305 | `		/* Point to the instruction to fix */` |
|  259727 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  259727 |   307 | `		if( pInstr ){` |
|  259727 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  259727 |   309 | `			nFixed++;` |
|       - |   310 | `			/* Mark as fixed */` |
|  259727 |   311 | `			aFix[n].nJumpType = -1;` |
|  129861 |   312 | `		}` |
|  129866 |   313 | `	}` |
|       - |   314 | `	/* Total number of fixed jumps */` |
|  633393 |   315 | `	return nFixed;` |
|       5 |   316 | `}` |
|       - |   317 | `/*` |
|       - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   319 | ` * The goto statement can be used to jump to another section` |
|       - |   320 | ` * in the program.` |
|       - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   322 | ` * statement for more information.` |
|       - |   323 | ` */` |
|  259218 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   325 | `{` |
|       - |   326 | `	JumpFixup *pJump,*aJumps;` |
|       - |   327 | `	Label *pLabel,*aLabel;` |
|       - |   328 | `	VmInstr *pInstr;` |
|       - |   329 | `	sxi32 rc;` |
|       - |   330 | `	sxu32 n;` |
|       - |   331 | `	/* Point to the goto table */` |
|  259223 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   333 | `	/* Fix */` |
|  259369 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  259221 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  259353 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   362 | `			/* Emit a warning */` |
|      40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   365 | `		}` |
|      71 |   366 | `	}` |
|  259221 |   367 | `	return SXRET_OK;` |
|  129614 |   368 | `}` |
|       - |   369 | `/*` |
|       - |   370 | ` * Check if a given token value is installed in the literal table.` |
|       - |   371 | ` */` |
|  825860 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   373 | `{` |
|       - |   374 | `	SyHashEntry *pEntry;` |
|  825865 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  825865 |   376 | `	if( pEntry == 0 ){` |
|  372373 |   377 | `		return SXERR_NOTFOUND;` |
|       - |   378 | `	}` |
|  453497 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  453497 |   380 | `	return SXRET_OK;` |
|  412935 |   381 | `}` |
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
|  372368 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   393 | `{` |
|  372373 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  372373 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  186184 |   396 | `	}` |
|  372373 |   397 | `	return SXRET_OK;` |
|       5 |   398 | `}` |
|       - |   399 | `/*` |
|       - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   401 | ` * in the constant table.` |
|       - |   402 | ` */` |
|  135064 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   404 | `{` |
|       - |   405 | `	ph7_value *pObj;` |
|  135069 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   407 | `	/* Reserve a new constant */` |
|  135069 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  135069 |   409 | `	if( pObj == 0 ){` |
|     ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   411 | `		return 0;` |
|       - |   412 | `	}` |
|  135069 |   413 | `	*pIdx = nIdx;` |
|       - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   416 | `	 */` |
|  135069 |   417 | `	return pObj;` |
|   67537 |   418 | `}` |
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
|  516138 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   434 | `{` |
|       - |   435 | `	VmCallArgMap *pMap;` |
|  516143 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|      39 |   437 | `	if( p3 == 0 ){` |
|      35 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      35 |   439 | `		if( pMap == 0 ) return 0;` |
|      35 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      35 |   441 | `		p3 = (void *)pMap;` |
|      16 |   442 | `	}` |
|      39 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      39 |   444 | `	return p3;` |
|  258074 |   445 | `}` |
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
|  135976 |   504 | `static int GenStateFindBadNumericSeparator(` |
|       - |   505 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   506 | `{` |
|  135981 |   507 | `	const char *z = pRaw->zString;` |
|  135981 |   508 | `	sxu32 n = pRaw->nByte;` |
|  135981 |   509 | `	int base = 10;` |
|       - |   510 | `	sxu32 i, start;` |
|  135981 |   511 | `	if( n < 2 ) return 0;` |
|   11407 |   512 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   513 | `		base = 16;` |
|   11372 |   514 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   515 | `		base = 2;` |
|     139 |   516 | `	}` |
|   41529 |   517 | `	for( i = 0; i < n; ++i ){` |
|   30141 |   518 | `		if( z[i] != '_' ) continue;` |
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
|   11393 |   535 | `	return 0;` |
|   67993 |   536 | `}` |
|       - |   537 | `/*` |
|       - |   538 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   539 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   540 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   541 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   542 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   543 | ` * so callers can bail from the current construct).` |
|       - |   544 | ` */` |
|  135976 |   545 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   546 | `{` |
|  135981 |   547 | `	const char *zBad = 0;` |
|  135981 |   548 | `	sxu32 nBad = 0;` |
|       - |   549 | `	SyString sBad;` |
|       - |   550 | `	sxi32 rc;` |
|  135981 |   551 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  135967 |   552 | `		return SXRET_OK;` |
|       - |   553 | `	}` |
|      18 |   554 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   555 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   556 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   557 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   558 | `		return SXERR_ABORT;` |
|       - |   559 | `	}` |
|      18 |   560 | `	return SXERR_SYNTAX;` |
|   67993 |   561 | `}` |
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
|  135962 |   578 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   579 | `	SyMemBackend *pAlloc,` |
|       - |   580 | `	const SyString *pToken,` |
|       - |   581 | `	char *zScratch, sxu32 nScratch,` |
|       - |   582 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   583 | `{` |
|       - |   584 | `	sxu32 i, j;` |
|  135967 |   585 | `	int hasUnderscore = 0;` |
|       - |   586 | `	char *zBuf;` |
|  135967 |   587 | `	*pzAlloc = 0;` |
|  288597 |   588 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  152887 |   589 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   76320 |   590 | `	}` |
|  135967 |   591 | `	if( !hasUnderscore ){` |
|  135715 |   592 | `		SyStringDupPtr(pOut, pToken);` |
|  135715 |   593 | `		return SXRET_OK;` |
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
|   67986 |   610 | `}` |
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
|  135948 |   627 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   628 | `{` |
|  135953 |   629 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  135953 |   630 | `	sxu32 nIdx = 0;` |
|       - |   631 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  135953 |   632 | `	char *zAlloc = 0;` |
|       - |   633 | `	SyString sNum;` |
|       - |   634 | `	sxi32 rc;` |
|   67974 |   635 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  135953 |   636 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  135953 |   637 | `	if( rc != SXRET_OK ){` |
|      14 |   638 | `		return rc;` |
|       - |   639 | `	}` |
|  203912 |   640 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   67969 |   641 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  135943 |   642 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   643 | `		return SXERR_ABORT;` |
|       - |   644 | `	}` |
|  135943 |   645 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   646 | `		ph7_value *pObj;` |
|       - |   647 | `		sxi64 iValue;` |
|  135069 |   648 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  135069 |   649 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  135069 |   650 | `		if( pObj == 0 ){` |
|     ! 0 |   651 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   652 | `			return SXERR_ABORT;` |
|       - |   653 | `		}` |
|  135069 |   654 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   67537 |   655 | `	}else{` |
|       - |   656 | `		/* Real number */` |
|       - |   657 | `		ph7_value *pObj;` |
|       - |   658 | `		/* Reserve a new constant */` |
|     879 |   659 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     879 |   660 | `		if( pObj == 0 ){` |
|     ! 0 |   661 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   662 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   663 | `			return SXERR_ABORT;` |
|       - |   664 | `		}` |
|     879 |   665 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     879 |   666 | `		PH7_MemObjToReal(pObj);` |
|       - |   667 | `	}` |
|  135943 |   668 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   669 | `	/* Emit the load constant instruction */` |
|  135943 |   670 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   671 | `	/* Node successfully compiled */` |
|  135943 |   672 | `	return SXRET_OK;` |
|   67979 |   673 | `}` |
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
|  107100 |   685 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   686 | `{` |
|  107105 |   687 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   688 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   689 | `	ph7_value *pObj;` |
|       - |   690 | `	sxu32 nIdx;` |
|  107105 |   691 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   692 | `	/* Delimit the string */` |
|  107105 |   693 | `	zIn  = pStr->zString;` |
|  107105 |   694 | `	zEnd = &zIn[pStr->nByte];` |
|  107105 |   695 | `	if( zIn >= zEnd ){` |
|       - |   696 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   697 | `		 * rather than reserving a new object each time. */` |
|    7873 |   698 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    7873 |   699 | `		return SXRET_OK;` |
|       - |   700 | `	}` |
|   99237 |   701 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   702 | `		/* Already processed,emit the load constant instruction` |
|       - |   703 | `		 * and return.` |
|       - |   704 | `		 */` |
|   37719 |   705 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   37719 |   706 | `		return SXRET_OK;` |
|       - |   707 | `	}` |
|       - |   708 | `	/* Reserve a new constant */` |
|   61523 |   709 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   61523 |   710 | `	if( pObj == 0 ){` |
|     ! 0 |   711 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   712 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   713 | `		return SXERR_ABORT;` |
|       - |   714 | `	}` |
|   61523 |   715 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   716 | `	/* Compile the node */` |
|   61577 |   717 | `	for(;;){` |
|  123159 |   718 | `		if( zIn >= zEnd ){` |
|       - |   719 | `			/* End of input */` |
|   61523 |   720 | `			break;` |
|       - |   721 | `		}` |
|   61641 |   722 | `		zCur = zIn;` |
| 1053395 |   723 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  991759 |   724 | `			zIn++;` |
|       5 |   725 | `		}` |
|   61641 |   726 | `		if( zIn > zCur ){` |
|       - |   727 | `			/* Append raw contents*/` |
|   61617 |   728 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   30806 |   729 | `		}` |
|   61641 |   730 | `		zIn++;` |
|   61641 |   731 | `		if( zIn < zEnd ){` |
|     141 |   732 | `			if( zIn[0] == '\\' ){` |
|       - |   733 | `				/* A literal backslash */` |
|      28 |   734 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     127 |   735 | `			}else if( zIn[0] == '\'' ){` |
|       - |   736 | `				/* A single quote */` |
|      11 |   737 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |   738 | `			}else{` |
|       - |   739 | `				/* verbatim copy */` |
|     104 |   740 | `				zIn--;` |
|     104 |   741 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     104 |   742 | `				zIn++;` |
|       - |   743 | `			}` |
|      69 |   744 | `		}` |
|       - |   745 | `		/* Advance the stream cursor */` |
|   61641 |   746 | `		zIn++;` |
|       5 |   747 | `	}` |
|       - |   748 | `	/* Emit the load constant instruction */` |
|   61523 |   749 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   61523 |   750 | `	if( pStr->nByte < 1024 ){` |
|       - |   751 | `		/* Install in the literal table */` |
|   61523 |   752 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   30759 |   753 | `	}` |
|       - |   754 | `	/* Node successfully compiled */` |
|   61523 |   755 | `	return SXRET_OK;` |
|   53555 |   756 | `}` |
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
|     114 |   775 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       5 |   776 | `{` |
|     119 |   777 | `	SyString *pIn = &pGen->pIn->sData;` |
|     119 |   778 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |   779 | `	const char *zPrefix;` |
|       - |   780 | `	const char *z, *zEnd;` |
|       - |   781 | `	char *zBuf, *zDst;` |
|     119 |   782 | `	if( nIndent == 0 ){` |
|       - |   783 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      73 |   784 | `		*pOut = *pIn;` |
|      73 |   785 | `		return SXRET_OK;` |
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
|      62 |   853 | `}` |
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
|      48 |   868 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       3 |   869 | `{` |
|       - |   870 | `	SyString sStripped;` |
|       - |   871 | `	SyString *pStr;` |
|       - |   872 | `	ph7_value *pObj;` |
|       - |   873 | `	sxu32 nIdx;` |
|       - |   874 | `	sxi32 rc;` |
|      51 |   875 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      51 |   876 | `	if( rc != SXRET_OK ){` |
|       6 |   877 | `		return rc;` |
|       - |   878 | `	}` |
|      46 |   879 | `	pStr = &sStripped;` |
|      46 |   880 | `	nIdx = 0; /* Prevent compiler warning */` |
|      46 |   881 | `	if( pStr->nByte <= 0 ){` |
|       - |   882 | `		/* Empty string,load NULL */` |
|       7 |   883 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |   884 | `		return SXRET_OK;` |
|       - |   885 | `	}` |
|       - |   886 | `	/* Reserve a new constant */` |
|      40 |   887 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      40 |   888 | `	if( pObj == 0 ){` |
|     ! 0 |   889 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   890 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   891 | `		return SXERR_ABORT;` |
|       - |   892 | `	}` |
|       - |   893 | `	/* No processing is done here, simply a memcpy() operation */` |
|      40 |   894 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |   895 | `	/* Emit the load constant instruction */` |
|      40 |   896 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   897 | `	/* Node successfully compiled */` |
|      40 |   898 | `	return SXRET_OK;` |
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
|    2382 |   922 | `static sxi32 GenStateProcessStringExpression(` |
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
|    2387 |   933 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   934 | `	/* Preallocate some slots */` |
|    2387 |   935 | `	SySetAlloc(&sToken,0x08);` |
|       - |   936 | `	/* Tokenize the text */` |
|    2387 |   937 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   938 | `	/* Swap delimiter */` |
|    2387 |   939 | `	pTmpIn  = pGen->pIn;` |
|    2387 |   940 | `	pTmpEnd = pGen->pEnd;` |
|    2387 |   941 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2387 |   942 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   943 | `	/* Compile the expression */` |
|    2387 |   944 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   945 | `	/* Restore token stream */` |
|    2387 |   946 | `	pGen->pIn  = pTmpIn;` |
|    2387 |   947 | `	pGen->pEnd = pTmpEnd;` |
|       - |   948 | `	/* Release the token set */` |
|    2387 |   949 | `	SySetRelease(&sToken);` |
|       - |   950 | `	/* Compilation result */` |
|    2387 |   951 | `	return rc;` |
|       5 |   952 | `}` |
|       - |   953 | `/*` |
|       - |   954 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   955 | ` */` |
|   27042 |   956 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   957 | `{` |
|       - |   958 | `	ph7_value *pConstObj;` |
|   27047 |   959 | `	sxu32 nIdx = 0;` |
|       - |   960 | `	/* Reserve a new constant */` |
|   27047 |   961 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   27047 |   962 | `	if( pConstObj == 0 ){` |
|     ! 0 |   963 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   964 | `		return 0;` |
|       - |   965 | `	}` |
|   27047 |   966 | `	(*pCount)++;` |
|   27047 |   967 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   968 | `	/* Emit the load constant instruction */` |
|   27047 |   969 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   27047 |   970 | `	return pConstObj;` |
|   13526 |   971 | `}` |
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
|       - |  1000 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|       - |  1001 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |  1002 | ` *  \\ backslash` |
|       - |  1003 | ` *  \$ dollar sign` |
|       - |  1004 | ` *  \" double-quote` |
|       - |  1005 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|       - |  1006 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|       - |  1007 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |  1008 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|       - |  1009 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|       - |  1010 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |  1011 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|       - |  1012 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |  1013 | ` * See string parsing for details.` |
|       - |  1014 | ` */` |
|       - |  1015 | `/*` |
|       - |  1016 | ` * Line number of an escape sequence inside the string body being compiled:` |
|       - |  1017 | ` * the token's line plus every newline before the escape (php reports the` |
|       - |  1018 | ` * escape's own line, not the string's opening line). A heredoc body starts` |
|       - |  1019 | ` * on the line after the '<<<' marker, hence the +1.` |
|       - |  1020 | ` */` |
|       6 |  1021 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|       3 |  1022 | `{` |
|       9 |  1023 | `	const char *z = pGen->pIn->sData.zString;` |
|       9 |  1024 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|      15 |  1025 | `	for( ; z < zPos ; z++ ){` |
|       9 |  1026 | `		if( z[0] == '\n' ){` |
|     ! 0 |  1027 | `			nLine++;` |
|     ! 0 |  1028 | `		}` |
|       6 |  1029 | `	}` |
|       9 |  1030 | `	return nLine;` |
|       3 |  1031 | `}` |
|       - |  1032 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|       - |  1033 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|   25468 |  1034 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|       5 |  1035 | `{` |
|   25473 |  1036 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1037 | `	const char *zIn,*zCur,*zEnd;` |
|   25473 |  1038 | `	ph7_value *pObj = 0;` |
|       - |  1039 | `	sxi32 iCons;` |
|       - |  1040 | `	sxi32 rc;` |
|       - |  1041 | `	/* Delimit the string */` |
|   25473 |  1042 | `	zIn  = pStr->zString;` |
|   25473 |  1043 | `	zEnd = &zIn[pStr->nByte];` |
|   25473 |  1044 | `	if( zIn >= zEnd ){` |
|       - |  1045 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1046 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1047 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1048 | `		 */` |
|     303 |  1049 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     303 |  1050 | `		return SXRET_OK;` |
|       - |  1051 | `	}` |
|   25175 |  1052 | `	zCur = 0;` |
|       - |  1053 | `	/* Compile the node */` |
|   25175 |  1054 | `	iCons = 0;` |
|   13776 |  1055 | `	for(;;){` |
|   41571 |  1056 | `		zCur = zIn;` |
|  186479 |  1057 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  147295 |  1058 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      69 |  1059 | `				break;` |
|  147167 |  1060 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2258 |  1061 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1130 |  1062 | `					break;` |
|       - |  1063 | `			}` |
|  144913 |  1064 | `			zIn++;` |
|       5 |  1065 | `		}` |
|   41571 |  1066 | `		if( zIn > zCur ){` |
|   18609 |  1067 | `			if( pObj == 0 ){` |
|   18091 |  1068 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   18091 |  1069 | `				if( pObj == 0 ){` |
|     ! 0 |  1070 | `					return SXERR_ABORT;` |
|       - |  1071 | `				}` |
|    9043 |  1072 | `			}` |
|   18609 |  1073 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    9302 |  1074 | `		}` |
|   41571 |  1075 | `		if( zIn >= zEnd ){` |
|   25173 |  1076 | `			break;` |
|       - |  1077 | `		}` |
|   16403 |  1078 | `		if( zIn[0] == '\\' ){` |
|   14021 |  1079 | `			const char *zPtr = 0;` |
|       - |  1080 | `			sxu32 n;` |
|   14021 |  1081 | `			zIn++;` |
|   14021 |  1082 | `			if( pObj == 0 ){` |
|    8961 |  1083 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    8961 |  1084 | `				if( pObj == 0 ){` |
|     ! 0 |  1085 | `					return SXERR_ABORT;` |
|       - |  1086 | `				}` |
|    4478 |  1087 | `			}` |
|   14021 |  1088 | `			if( zIn >= zEnd ){` |
|       - |  1089 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|       3 |  1090 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       3 |  1091 | `				break;` |
|       - |  1092 | `			}` |
|   14019 |  1093 | `			n = sizeof(char); /* size of conversion */` |
|   14019 |  1094 | `			switch( zIn[0] ){` |
|      11 |  1095 | `			case '$':` |
|       - |  1096 | `				/* Dollar sign */` |
|      25 |  1097 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|      25 |  1098 | `				break;` |
|      56 |  1099 | `			case '\\':` |
|       - |  1100 | `				/* A literal backslash */` |
|     117 |  1101 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     117 |  1102 | `				break;` |
|       1 |  1103 | `			case 'e':` |
|       - |  1104 | `				/* Escape (ESC) ASCII code 27 */` |
|       3 |  1105 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|       3 |  1106 | `				break;` |
|       4 |  1107 | `			case 'f':` |
|       - |  1108 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       9 |  1109 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       9 |  1110 | `				break;` |
|    6461 |  1111 | `			case 'n':` |
|       - |  1112 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   12927 |  1113 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   12927 |  1114 | `				break;` |
|      19 |  1115 | `			case 'r':` |
|       - |  1116 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      43 |  1117 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      43 |  1118 | `				break;` |
|      25 |  1119 | `			case 't':` |
|       - |  1120 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      55 |  1121 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      55 |  1122 | `				break;` |
|       3 |  1123 | `			case 'v':` |
|       - |  1124 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 |  1125 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 |  1126 | `				break;` |
|     112 |  1127 | `			case '"':` |
|     229 |  1128 | `				if( bHeredoc ){` |
|       - |  1129 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|       5 |  1130 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|       3 |  1131 | `				}else{` |
|       - |  1132 | `					/* Double quote */` |
|     225 |  1133 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|       - |  1134 | `				}` |
|     229 |  1135 | `				break;` |
|      24 |  1136 | `			case '0': case '1': case '2': case '3':` |
|       - |  1137 | `			case '4': case '5': case '6': case '7': {` |
|       - |  1138 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|       - |  1139 | `				 * warns and wraps to the low byte, matching php 8. */` |
|      50 |  1140 | `				int c = 0;` |
|       - |  1141 | `				char cOut;` |
|     144 |  1142 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|     122 |  1143 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|      14 |  1144 | `						break;` |
|       - |  1145 | `					}` |
|      96 |  1146 | `					c = c * 8 + (zPtr[0] - '0');` |
|      49 |  1147 | `				}` |
|      50 |  1148 | `				if( c > 0xFF ){` |
|       - |  1149 | `					SyString sSeq;` |
|       3 |  1150 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|       3 |  1151 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - |  1152 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|       3 |  1153 | `					c &= 0xFF;` |
|       1 |  1154 | `				}` |
|      50 |  1155 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|      50 |  1156 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|      50 |  1157 | `				n = (sxu32)(zPtr-zIn);` |
|      50 |  1158 | `				break;` |
|       - |  1159 | `			}` |
|     270 |  1160 | `			case 'x':` |
|     809 |  1161 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|       - |  1162 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|     537 |  1163 | `					int c = SyHexToint(zIn[1]);` |
|       - |  1164 | `					char cOut;` |
|     537 |  1165 | `					n += sizeof(char);` |
|     537 |  1166 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|     533 |  1167 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|     533 |  1168 | `						n += sizeof(char);` |
|     266 |  1169 | `					}` |
|     537 |  1170 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|     537 |  1171 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|     269 |  1172 | `				}else{` |
|       - |  1173 | `					/* Not an escape: keep the backslash, as php does */` |
|       5 |  1174 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|       - |  1175 | `				}` |
|     541 |  1176 | `				break;` |
|       9 |  1177 | `			case 'u':` |
|      25 |  1178 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|      22 |  1179 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|       - |  1180 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|       - |  1181 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|       - |  1182 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|       - |  1183 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|       - |  1184 | `					 * followed by {$...} curly interpolation. */` |
|      15 |  1185 | `					sxu32 nCp = 0;` |
|      15 |  1186 | `					zPtr = &zIn[2];` |
|      59 |  1187 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|      46 |  1188 | `						if( nCp <= 0x10FFFF ){` |
|       - |  1189 | `							/* stop accumulating once out of range: keeps a long` |
|       - |  1190 | `							 * digit run from wrapping sxu32 */` |
|      46 |  1191 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|      22 |  1192 | `						}` |
|      46 |  1193 | `						zPtr++;` |
|       2 |  1194 | `					}` |
|      15 |  1195 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|       - |  1196 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|       - |  1197 | `						 * malformed sequence so later errors are still reported. */` |
|       3 |  1198 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - |  1199 | `							"Invalid UTF-8 codepoint escape sequence");` |
|       3 |  1200 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  1201 | `							return SXERR_ABORT;` |
|       - |  1202 | `						}` |
|       3 |  1203 | `						n = (sxu32)(zPtr-zIn);` |
|       3 |  1204 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|       3 |  1205 | `							n += sizeof(char);` |
|       1 |  1206 | `						}` |
|       3 |  1207 | `						break;` |
|       - |  1208 | `					}` |
|      12 |  1209 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|      12 |  1210 | `					if( nCp > 0x10FFFF ){` |
|       3 |  1211 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - |  1212 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|       3 |  1213 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  1214 | `							return SXERR_ABORT;` |
|       - |  1215 | `						}` |
|       3 |  1216 | `						break;` |
|       - |  1217 | `					}` |
|       - |  1218 | `					{` |
|       - |  1219 | `						char zUtf[4];` |
|       9 |  1220 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|       9 |  1221 | `						SX_WRITE_UTF8(zOut,nCp);` |
|       9 |  1222 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|       - |  1223 | `					}` |
|       5 |  1224 | `				}else{` |
|       - |  1225 | `					/* Not an escape: keep the backslash, as php does */` |
|       7 |  1226 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|       - |  1227 | `				}` |
|      15 |  1228 | `				break;` |
|      12 |  1229 | `			default:` |
|       - |  1230 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|       - |  1231 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|       - |  1232 | `				 * in the source buffer — one batched append. */` |
|      25 |  1233 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|      24 |  1234 | `				break;` |
|       - |  1235 | `			}` |
|       - |  1236 | `			/* Advance the stream cursor */` |
|   14019 |  1237 | `			zIn += n;` |
|   14019 |  1238 | `			continue;` |
|       - |  1239 | `		}` |
|    2387 |  1240 | `		if( zIn[0] == '{' ){` |
|       - |  1241 | `			/* Curly syntax */` |
|       - |  1242 | `			const char *zExpr;` |
|     135 |  1243 | `			sxi32 iNest = 1;` |
|     135 |  1244 | `			zIn++;` |
|     135 |  1245 | `			zExpr = zIn;` |
|       - |  1246 | `			/* Synchronize with the next closing curly braces */` |
|    1383 |  1247 | `			while( zIn < zEnd ){` |
|    1383 |  1248 | `				if( zIn[0] == '{' ){` |
|       - |  1249 | `					/* Increment nesting level */` |
|       9 |  1250 | `					iNest++;` |
|    1379 |  1251 | `				}else if(zIn[0] == '}' ){` |
|       - |  1252 | `					/* Decrement nesting level */` |
|     143 |  1253 | `					iNest--;` |
|     143 |  1254 | `					if( iNest <= 0 ){` |
|     135 |  1255 | `						break;` |
|       - |  1256 | `					}` |
|       4 |  1257 | `				}` |
|    1251 |  1258 | `				zIn++;` |
|       3 |  1259 | `			}` |
|       - |  1260 | `			/* Process the expression */` |
|     135 |  1261 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     135 |  1262 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1263 | `				return SXERR_ABORT;` |
|       - |  1264 | `			}` |
|     135 |  1265 | `			if( rc != SXERR_EMPTY ){` |
|     135 |  1266 | `				++iCons;` |
|      66 |  1267 | `			}` |
|     135 |  1268 | `			if( zIn < zEnd ){` |
|       - |  1269 | `				/* Jump the trailing curly */` |
|     135 |  1270 | `				zIn++;` |
|      66 |  1271 | `			}` |
|      69 |  1272 | `		}else{` |
|       - |  1273 | `			/* Simple syntax */` |
|    2255 |  1274 | `			const char *zExpr = zIn;` |
|       - |  1275 | `			/* Assemble variable name */` |
|    1150 |  1276 | `			for(;;){` |
|       - |  1277 | `				/* Jump leading dollars */` |
|    4555 |  1278 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2255 |  1279 | `					zIn++;` |
|       5 |  1280 | `				}` |
|    1150 |  1281 | `				for(;;){` |
|   12275 |  1282 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8825 |  1283 | `						zIn++;` |
|       5 |  1284 | `					}` |
|    2305 |  1285 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1286 | `						/* UTF-8 stream */` |
|     ! 0 |  1287 | `						zIn++;` |
|     ! 0 |  1288 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1289 | `							zIn++;` |
|     ! 0 |  1290 | `						}` |
|     ! 0 |  1291 | `						continue;` |
|       - |  1292 | `					}` |
|    2305 |  1293 | `					break;` |
|     ! 0 |  1294 | `				}` |
|    2305 |  1295 | `				if( zIn >= zEnd ){` |
|     226 |  1296 | `					break;` |
|       - |  1297 | `				}` |
|    2083 |  1298 | `				if( zIn[0] == '[' ){` |
|      12 |  1299 | `					sxi32 iSquare = 1;` |
|      12 |  1300 | `					zIn++;` |
|      28 |  1301 | `					while( zIn < zEnd ){` |
|      28 |  1302 | `						if( zIn[0] == '[' ){` |
|     ! 0 |  1303 | `							iSquare++;` |
|      28 |  1304 | `						}else if (zIn[0] == ']' ){` |
|      12 |  1305 | `							iSquare--;` |
|      12 |  1306 | `							if( iSquare <= 0 ){` |
|      12 |  1307 | `								break;` |
|       - |  1308 | `							}` |
|     ! 0 |  1309 | `						}` |
|      18 |  1310 | `						zIn++;` |
|       2 |  1311 | `					}` |
|      12 |  1312 | `					if( zIn < zEnd ){` |
|      12 |  1313 | `						zIn++;` |
|       5 |  1314 | `					}` |
|      12 |  1315 | `					break;` |
|    2073 |  1316 | `				}else if(zIn[0] == '{' ){` |
|       6 |  1317 | `					sxi32 iCurly = 1;` |
|       6 |  1318 | `					zIn++;` |
|      18 |  1319 | `					while( zIn < zEnd ){` |
|      16 |  1320 | `						if( zIn[0] == '{' ){` |
|     ! 0 |  1321 | `							iCurly++;` |
|      16 |  1322 | `						}else if (zIn[0] == '}' ){` |
|       3 |  1323 | `							iCurly--;` |
|       3 |  1324 | `							if( iCurly <= 0 ){` |
|       3 |  1325 | `								break;` |
|       - |  1326 | `							}` |
|     ! 0 |  1327 | `						}` |
|      14 |  1328 | `						zIn++;` |
|       2 |  1329 | `					}` |
|       6 |  1330 | `					if( zIn < zEnd ){` |
|       3 |  1331 | `						zIn++;` |
|       1 |  1332 | `					}` |
|       6 |  1333 | `					break;` |
|    2069 |  1334 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1335 | `					/* Member access operator '->' */` |
|      53 |  1336 | `					zIn += 2;` |
|    2044 |  1337 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1338 | `					/* Static member access operator '::' */` |
|     ! 0 |  1339 | `					zIn += 2;` |
|     ! 0 |  1340 | `				}else{` |
|    1012 |  1341 | `					break;` |
|       - |  1342 | `				}` |
|       3 |  1343 | `			}` |
|       - |  1344 | `			/* Process the expression */` |
|    2255 |  1345 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2255 |  1346 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1347 | `				return SXERR_ABORT;` |
|       - |  1348 | `			}` |
|    2255 |  1349 | `			if( rc != SXERR_EMPTY ){` |
|    2253 |  1350 | `				++iCons;` |
|    1124 |  1351 | `			}` |
|       - |  1352 | `		}` |
|       - |  1353 | `		/* Invalidate the previously used constant */` |
|    2387 |  1354 | `		pObj = 0;` |
|       5 |  1355 | `	}/*for(;;)*/` |
|   25175 |  1356 | `	if( iCons > 1 ){` |
|       - |  1357 | `		/* Concatenate all compiled constants */` |
|    1759 |  1358 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     877 |  1359 | `	}` |
|       - |  1360 | `	/* Node successfully compiled */` |
|   25175 |  1361 | `	return SXRET_OK;` |
|   12739 |  1362 | `}` |
|       - |  1363 | `/*` |
|       - |  1364 | ` * Compile a double quoted string.` |
|       - |  1365 | ` *  See the block-comment above for more information.` |
|       - |  1366 | ` */` |
|   25406 |  1367 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1368 | `{` |
|       - |  1369 | `	sxi32 rc;` |
|   25411 |  1370 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|   12703 |  1371 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1372 | `	/* Compilation result */` |
|   25411 |  1373 | `	return rc;` |
|       5 |  1374 | `}` |
|       - |  1375 | `/*` |
|       - |  1376 | ` * Compile a Heredoc string.` |
|       - |  1377 | ` *  See the block-comment above for more information.` |
|       - |  1378 | ` */` |
|      66 |  1379 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1380 | `{` |
|       - |  1381 | `	SyString sOrig, sStripped;` |
|       - |  1382 | `	sxi32 rc;` |
|      71 |  1383 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      71 |  1384 | `	if( rc != SXRET_OK ){` |
|       6 |  1385 | `		return rc;` |
|       - |  1386 | `	}` |
|       - |  1387 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|       - |  1388 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|       - |  1389 | `	 * Restore before returning so downstream code that references pIn is` |
|       - |  1390 | `	 * unaffected, including on the error path. */` |
|      65 |  1391 | `	sOrig = pGen->pIn->sData;` |
|      65 |  1392 | `	pGen->pIn->sData = sStripped;` |
|      65 |  1393 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|      65 |  1394 | `	pGen->pIn->sData = sOrig;` |
|      31 |  1395 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      65 |  1396 | `	return rc;` |
|      38 |  1397 | `}` |
|       - |  1398 | `/*` |
|       - |  1399 | ` * Compile an array entry whether it is a key or a value.` |
|       - |  1400 | ` *  Notes on array entries.` |
|       - |  1401 | ` *  According to the PHP language reference manual` |
|       - |  1402 | ` *  An array can be created by the array() language construct.` |
|       - |  1403 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - |  1404 | ` *  array(  key =>  value` |
|       - |  1405 | ` *    , ...` |
|       - |  1406 | ` *    )` |
|       - |  1407 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - |  1408 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - |  1409 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - |  1410 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - |  1411 | ` *  contain integer and string indices.` |
|       - |  1412 | ` *  A value can be any PHP type.` |
|       - |  1413 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - |  1414 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - |  1415 | ` *  is specified, that value will be overwritten.` |
|       - |  1416 | ` */` |
|   23700 |  1417 | `static sxi32 GenStateCompileArrayEntry(` |
|       - |  1418 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  1419 | `	SyToken *pIn,        /* Token stream */` |
|       - |  1420 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - |  1421 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - |  1422 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - |  1423 | `	)` |
|       5 |  1424 | `{` |
|       - |  1425 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  1426 | `	sxi32 rc;` |
|       - |  1427 | `	/* Swap token stream */` |
|   23705 |  1428 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1429 | `	/* Compile the expression*/` |
|   23705 |  1430 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1431 | `	/* Restore token stream */` |
|   23705 |  1432 | `	RE_SWAP_DELIMITER(pGen);` |
|   23705 |  1433 | `	return rc;` |
|       5 |  1434 | `}` |
|       - |  1435 | `/*` |
|       - |  1436 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - |  1437 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1438 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1439 | ` * error message.` |
|       - |  1440 | ` * See the routine responible of compiling the array language construct` |
|       - |  1441 | ` * for more inforation.` |
|       - |  1442 | ` */` |
|      36 |  1443 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  1444 | `{` |
|      41 |  1445 | `	sxi32 rc = SXRET_OK;` |
|      41 |  1446 | `	if( pRoot->pOp ){` |
|      19 |  1447 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 |  1448 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      17 |  1449 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  1450 | `			/* Unexpected expression */` |
|      14 |  1451 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1452 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      14 |  1453 | `			if( rc != SXERR_ABORT ){` |
|      14 |  1454 | `				rc = SXERR_INVALID;` |
|       5 |  1455 | `			}` |
|      10 |  1456 | `		}` |
|      31 |  1457 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1458 | `		/* Unexpected expression */` |
|       3 |  1459 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1460 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 |  1461 | `		if( rc != SXERR_ABORT ){` |
|       3 |  1462 | `			rc = SXERR_INVALID;` |
|       1 |  1463 | `		}` |
|       1 |  1464 | `	}` |
|      41 |  1465 | `	return rc;` |
|       5 |  1466 | `}` |
|       - |  1467 | `/*` |
|       - |  1468 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|       - |  1469 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|       - |  1470 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|       - |  1471 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|       - |  1472 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|       - |  1473 | ` */` |
|   26222 |  1474 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 |  1475 | `{` |
|   26227 |  1476 | `	SyToken *pCur = pStart;` |
|   26227 |  1477 | `	sxi32 iNest = 0;` |
|   74439 |  1478 | `	while( pCur < pEnd ){` |
|   54119 |  1479 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    5903 |  1480 | `			return pCur;` |
|       - |  1481 | `		}` |
|       - |  1482 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1483 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - |  1484 | `		 * not an entry separator. Skip past the signature.` |
|       - |  1485 | `		 */` |
|   48221 |  1486 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      95 |  1487 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      95 |  1488 | `			SyToken *pFn = pCur;` |
|      92 |  1489 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|     ! 0 |  1490 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3 |  1491 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1492 | `				pFn = &pCur[1];` |
|     ! 0 |  1493 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1494 | `			}` |
|      95 |  1495 | `			if( nKw == PH7_TKWRD_FN ){` |
|       5 |  1496 | `				pCur = pFn + 1; /* past 'fn' */` |
|       5 |  1497 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1498 | `					pCur++;` |
|     ! 0 |  1499 | `				}` |
|       5 |  1500 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       5 |  1501 | `					pCur++;` |
|       5 |  1502 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1503 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       5 |  1504 | `					if( pCur < pEnd ){` |
|       5 |  1505 | `						pCur++;` |
|       2 |  1506 | `					}` |
|       2 |  1507 | `				}` |
|       5 |  1508 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 |  1509 | `					pCur++;` |
|     ! 0 |  1510 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|     ! 0 |  1511 | `						&& pCur->sData.nByte == 1` |
|     ! 0 |  1512 | `						&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 |  1513 | `						pCur++;` |
|     ! 0 |  1514 | `					}` |
|     ! 0 |  1515 | `					if( pCur < pEnd` |
|     ! 0 |  1516 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  1517 | `						pCur++;` |
|     ! 0 |  1518 | `					}` |
|     ! 0 |  1519 | `				}` |
|       - |  1520 | `				/* The rest of the entry is the arrow-function body — no outer` |
|       - |  1521 | `				 * key to extract. */` |
|       5 |  1522 | `				return pEnd;` |
|       - |  1523 | `			}` |
|       - |  1524 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|       - |  1525 | `			 * entry separator. Skip past the full match span. */` |
|      91 |  1526 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|       3 |  1527 | `				pCur++; /* past 'match' */` |
|       3 |  1528 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       3 |  1529 | `					pCur++;` |
|       3 |  1530 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1531 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       3 |  1532 | `					if( pCur < pEnd ){` |
|       3 |  1533 | `						pCur++;` |
|       1 |  1534 | `					}` |
|       1 |  1535 | `				}` |
|       3 |  1536 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|       3 |  1537 | `					pCur++;` |
|       3 |  1538 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1539 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|       3 |  1540 | `					if( pCur < pEnd ){` |
|       3 |  1541 | `						pCur++;` |
|       1 |  1542 | `					}` |
|       1 |  1543 | `				}` |
|       3 |  1544 | `				continue;` |
|       - |  1545 | `			}` |
|      43 |  1546 | `		}` |
|   48215 |  1547 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     413 |  1548 | `			iNest++;` |
|   48010 |  1549 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1550 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - |  1551 | `			 * parser will shortly detect any syntax error. */` |
|     413 |  1552 | `			iNest--;` |
|     205 |  1553 | `		}` |
|   48215 |  1554 | `		pCur++;` |
|       5 |  1555 | `	}` |
|   20325 |  1556 | `	return pEnd;` |
|   13116 |  1557 | `}` |
|       - |  1558 | `/*` |
|       - |  1559 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1560 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1561 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1562 | ` */` |
|   33864 |  1563 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1564 | `{` |
|       - |  1565 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1566 | `	SyToken *pKey,*pCur;` |
|   33869 |  1567 | `	sxi32 iEmitRef = 0;` |
|   33869 |  1568 | `	sxi32 iSpread = 0;` |
|   33869 |  1569 | `	sxi32 nPair = 0;` |
|       - |  1570 | `	sxi32 rc;` |
|   33869 |  1571 | `	xValidator = 0;` |
|   27769 |  1572 | `	for(;;){` |
|       - |  1573 | `		/* Jump leading commas */` |
|   63059 |  1574 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    7521 |  1575 | `			pGen->pIn++;` |
|       5 |  1576 | `		}` |
|   55543 |  1577 | `		pCur = pGen->pIn;` |
|   55543 |  1578 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1579 | `			/* No more entry to process */` |
|   33853 |  1580 | `			break;` |
|       - |  1581 | `		}` |
|   21695 |  1582 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1583 | `			continue;` |
|       - |  1584 | `		}` |
|       - |  1585 | `		/* Compile the key if available */` |
|   21695 |  1586 | `		pKey = pCur;` |
|   21695 |  1587 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   21695 |  1588 | `		rc = SXERR_EMPTY;` |
|   21695 |  1589 | `		if( pCur < pGen->pIn ){` |
|    1771 |  1590 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1591 | `				/* Missing value */` |
|      13 |  1592 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      13 |  1593 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1594 | `					return SXERR_ABORT;` |
|       - |  1595 | `				}` |
|      13 |  1596 | `				return SXRET_OK;` |
|       - |  1597 | `			}` |
|       - |  1598 | `			/* Compile the expression holding the key */` |
|    1761 |  1599 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1600 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1761 |  1601 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1602 | `				return SXERR_ABORT;` |
|       - |  1603 | `			}` |
|    1761 |  1604 | `			pCur++; /* Jump the '=>' operator */` |
|   20807 |  1605 | `		}else if( pKey == pCur ){` |
|       - |  1606 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1607 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1608 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1609 | `		}else{` |
|       - |  1610 | `			/* Reset back the cursor and point to the entry value */` |
|   19929 |  1611 | `			pCur = pKey;` |
|       - |  1612 | `		}` |
|   21685 |  1613 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1614 | `			/* No available key,load NULL */` |
|   19931 |  1615 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    9963 |  1616 | `		}` |
|   21685 |  1617 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1618 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      45 |  1619 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      45 |  1620 | `			iEmitRef = 1;` |
|      45 |  1621 | `			pCur++; /* Jump the '&' token */` |
|      45 |  1622 | `			if( pCur >= pGen->pIn ){` |
|       - |  1623 | `				/* Missing value */` |
|       3 |  1624 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 |  1625 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1626 | `					return SXERR_ABORT;` |
|       - |  1627 | `				}` |
|       3 |  1628 | `				return SXRET_OK;` |
|       - |  1629 | `			}` |
|      19 |  1630 | `		}` |
|       - |  1631 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|       - |  1632 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|       - |  1633 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|       - |  1634 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|       - |  1635 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|   21683 |  1636 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   21683 |  1637 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|       - |  1638 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|       - |  1639 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|       - |  1640 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|       - |  1641 | `			 * output is engine-portable. */` |
|       6 |  1642 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|       - |  1643 | `				"syntax error, unexpected token \"...\"");` |
|       6 |  1644 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1645 | `				return SXERR_ABORT;` |
|       - |  1646 | `			}` |
|       6 |  1647 | `			return SXRET_OK;` |
|       - |  1648 | `		}` |
|       - |  1649 | `		/* Compile indice value */` |
|   21679 |  1650 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   21679 |  1651 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1652 | `			return SXERR_ABORT;` |
|       - |  1653 | `		}` |
|   21679 |  1654 | `		if( iSpread ){` |
|       - |  1655 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      64 |  1656 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   21648 |  1657 | `		}else if( iEmitRef ){` |
|       - |  1658 | `			/* Emit the load reference instruction */` |
|      41 |  1659 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1660 | `		}` |
|   21679 |  1661 | `		xValidator = 0;` |
|   21679 |  1662 | `		iEmitRef = 0;` |
|   21679 |  1663 | `		iSpread = 0;` |
|   21679 |  1664 | `		nPair++;` |
|       5 |  1665 | `	}` |
|       - |  1666 | `	/* Emit the load map instruction */` |
|   33853 |  1667 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1668 | `	/* Node successfully compiled */` |
|   33853 |  1669 | `	return SXRET_OK;` |
|   16937 |  1670 | `}` |
|       - |  1671 | `/*` |
|       - |  1672 | ` * Compile the 'array' language construct.` |
|       - |  1673 | ` *	 According to the PHP language reference manual` |
|       - |  1674 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1675 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1676 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1677 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1678 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1679 | ` */` |
|   32578 |  1680 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1681 | `{` |
|       - |  1682 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   32583 |  1683 | `	pGen->pIn += 2;` |
|   32583 |  1684 | `	pGen->pEnd--;` |
|   16289 |  1685 | `	SXUNUSED(iCompileFlag);` |
|   32583 |  1686 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1687 | `}` |
|       - |  1688 | `/*` |
|       - |  1689 | ` * Compile the PHP 8.5 clone(...) call form:` |
|       - |  1690 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|       - |  1691 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|       - |  1692 | ` *                                              property updates as scope-aware writes` |
|       - |  1693 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|       - |  1694 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|       - |  1695 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|       - |  1696 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|       - |  1697 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|       - |  1698 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|       - |  1699 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|       - |  1700 | ` */` |
|      22 |  1701 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1702 | `{` |
|       - |  1703 | `	SyToken *pIn,*pEnd,*pNext;` |
|      24 |  1704 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|      24 |  1705 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|      24 |  1706 | `	int nArg = 0;` |
|       - |  1707 | `	sxi32 rc;` |
|      11 |  1708 | `	SXUNUSED(iCompileFlag);` |
|       - |  1709 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|      24 |  1710 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|      24 |  1711 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|       - |  1712 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|      24 |  1713 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|     ! 0 |  1714 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  1715 | `			"clone(...) first-class callable form is not yet supported");` |
|       - |  1716 | `	}` |
|       - |  1717 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|      62 |  1718 | `	while( pIn < pEnd ){` |
|      40 |  1719 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|      40 |  1720 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|     ! 0 |  1721 | `			break;` |
|       - |  1722 | `		}` |
|      40 |  1723 | `		pArgStart = pIn;` |
|      40 |  1724 | `		pArgEnd   = pNext;` |
|       - |  1725 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|       - |  1726 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|      41 |  1727 | `		if( (pArgEnd - pArgStart) >= 2` |
|      37 |  1728 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      23 |  1729 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|       5 |  1730 | `			pName = pArgStart;` |
|       5 |  1731 | `			pArgStart += 2;` |
|       2 |  1732 | `		}` |
|      40 |  1733 | `		if( pName ){` |
|       - |  1734 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|       - |  1735 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|       4 |  1736 | `			if( pName->sData.nByte == sizeof("object")-1` |
|       4 |  1737 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|       3 |  1738 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|       4 |  1739 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|       3 |  1740 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|       3 |  1741 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|       2 |  1742 | `			}else{` |
|     ! 0 |  1743 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|     ! 0 |  1744 | `					"Unknown named parameter $%z",&pName->sData);` |
|       1 |  1745 | `			}` |
|      38 |  1746 | `		}else if( nArg == 0 ){` |
|      22 |  1747 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|      25 |  1748 | `		}else if( nArg == 1 ){` |
|      15 |  1749 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|       8 |  1750 | `		}else{` |
|     ! 0 |  1751 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|       - |  1752 | `				"clone() expects at most 2 arguments");` |
|       - |  1753 | `		}` |
|      40 |  1754 | `		nArg++;` |
|      40 |  1755 | `		pIn = pNext;` |
|      40 |  1756 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|      17 |  1757 | `			pIn++; /* step over the argument separator */` |
|       8 |  1758 | `		}` |
|       2 |  1759 | `	}` |
|      24 |  1760 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|     ! 0 |  1761 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  1762 | `			"clone() expects at least 1 argument, 0 given");` |
|       - |  1763 | `	}` |
|       - |  1764 | `	/* Object argument -> clone (+ __clone()). */` |
|      24 |  1765 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|      24 |  1766 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1767 | `		return SXERR_ABORT;` |
|       - |  1768 | `	}` |
|      24 |  1769 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|       - |  1770 | `	/* Property updates (evaluated after __clone runs). */` |
|      24 |  1771 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|      17 |  1772 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|      17 |  1773 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1774 | `			return SXERR_ABORT;` |
|       - |  1775 | `		}` |
|      17 |  1776 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|       8 |  1777 | `	}` |
|      24 |  1778 | `	return SXRET_OK;` |
|      13 |  1779 | `}` |
|       - |  1780 | `/*` |
|       - |  1781 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1782 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1783 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1784 | ` */` |
|    1286 |  1785 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1786 | `{` |
|       - |  1787 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    1291 |  1788 | `	pGen->pIn++;` |
|    1291 |  1789 | `	pGen->pEnd--;` |
|     643 |  1790 | `	SXUNUSED(iCompileFlag);` |
|    1291 |  1791 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1792 | `}` |
|       - |  1793 | `/*` |
|       - |  1794 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1795 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1796 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1797 | ` * error message.` |
|       - |  1798 | ` * See the routine responible of compiling the list language construct` |
|       - |  1799 | ` * for more inforation.` |
|       - |  1800 | ` */` |
|     190 |  1801 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  1802 | `{` |
|     195 |  1803 | `	sxi32 rc = SXRET_OK;` |
|     195 |  1804 | `	if( pRoot->pOp ){` |
|       4 |  1805 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|       2 |  1806 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1807 | `				/* Unexpected expression */` |
|     ! 0 |  1808 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1809 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1810 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1811 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1812 | `				}` |
|       1 |  1813 | `		}` |
|     193 |  1814 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1815 | `		/* Unexpected expression */` |
|       6 |  1816 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1817 | `			"list(): Expecting a variable not an expression");` |
|       6 |  1818 | `		if( rc != SXERR_ABORT ){` |
|       6 |  1819 | `			rc = SXERR_INVALID;` |
|       2 |  1820 | `		}` |
|       2 |  1821 | `	}` |
|     195 |  1822 | `	return rc;` |
|       5 |  1823 | `}` |
|       - |  1824 | `/*` |
|       - |  1825 | ` * Compile the 'list' language construct.` |
|       - |  1826 | ` *  According to the PHP language reference` |
|       - |  1827 | ` *  list(): Assign variables as if they were an array.` |
|       - |  1828 | ` *  list() is used to assign a list of variables in one operation.` |
|       - |  1829 | ` *  Description` |
|       - |  1830 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - |  1831 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - |  1832 | ` *   list() is used to assign a list of variables in one operation.` |
|       - |  1833 | ` *  Parameters` |
|       - |  1834 | ` *   $varname: A variable.` |
|       - |  1835 | ` *  Return Values` |
|       - |  1836 | ` *   The assigned array.` |
|       - |  1837 | ` */` |
|       - |  1838 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - |  1839 | `struct NestedListEntry {` |
|       - |  1840 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - |  1841 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - |  1842 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - |  1843 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - |  1844 | `};` |
|       - |  1845 | `/*` |
|       - |  1846 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|       - |  1847 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|       - |  1848 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|       - |  1849 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|       - |  1850 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|       - |  1851 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|       - |  1852 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|       - |  1853 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|       - |  1854 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|       - |  1855 | ` */` |
|      28 |  1856 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|       2 |  1857 | `{` |
|       - |  1858 | `	SyToken *pNext;` |
|       - |  1859 | `	sxi32 rc;` |
|      66 |  1860 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       - |  1861 | `		SyToken *pArrow,*pTarget;` |
|       - |  1862 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|      38 |  1863 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|      38 |  1864 | `		pTarget = &pArrow[1];` |
|      38 |  1865 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|       - |  1866 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|       - |  1867 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|     ! 0 |  1868 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1869 | `				"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1870 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1871 | `		}` |
|       - |  1872 | `		/* DUP the source array (it is on the stack top) */` |
|      38 |  1873 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1874 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|      38 |  1875 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|      38 |  1876 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1877 | `			return SXERR_ABORT;` |
|       - |  1878 | `		}` |
|       - |  1879 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|       - |  1880 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|       - |  1881 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|       - |  1882 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|       - |  1883 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|       - |  1884 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|      38 |  1885 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|      38 |  1886 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|      34 |  1887 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|      18 |  1888 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|       - |  1889 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|       - |  1890 | `			 * Treat source[key] as the inner body's source, then drop the` |
|       - |  1891 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|       5 |  1892 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|       5 |  1893 | `			SyToken *pSavedIn = pGen->pIn;` |
|       5 |  1894 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       5 |  1895 | `			pGen->pIn = pTarget;` |
|       5 |  1896 | `			pGen->pEnd = pNext;` |
|       5 |  1897 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|       2 |  1898 | `			             : PH7_CompileList(&(*pGen),0);` |
|       5 |  1899 | `			pGen->pIn = pSavedIn;` |
|       5 |  1900 | `			pGen->pEnd = pSavedEnd;` |
|       5 |  1901 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1902 | `				return SXERR_ABORT;` |
|       - |  1903 | `			}` |
|       5 |  1904 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       3 |  1905 | `		}else{` |
|       - |  1906 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|       - |  1907 | `			 * is already on the stack as the value; compiling the target appends` |
|       - |  1908 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|       - |  1909 | `			 * assignment does. */` |
|       - |  1910 | `			VmInstr *pInstr;` |
|      34 |  1911 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|      34 |  1912 | `			sxi32 iP1 = 0, iP2 = 0;` |
|      34 |  1913 | `			void *p3 = 0;` |
|      34 |  1914 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|       - |  1915 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      34 |  1916 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  1917 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1918 | `			}` |
|      34 |  1919 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|      34 |  1920 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|       3 |  1921 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|      33 |  1922 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       3 |  1923 | `					iVmOp = PH7_OP_STORE_IDX;` |
|       3 |  1924 | `					iP1 = pInstr->iP1;` |
|       3 |  1925 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       2 |  1926 | `				}else{` |
|      30 |  1927 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|      30 |  1928 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - |  1929 | `				}` |
|      16 |  1930 | `			}` |
|      34 |  1931 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - |  1932 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|       - |  1933 | `			 * source array is back on top for the next entry. */` |
|      34 |  1934 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       - |  1935 | `		}` |
|      38 |  1936 | `		pGen->pIn = &pNext[1];` |
|       2 |  1937 | `	}` |
|      30 |  1938 | `	return SXRET_OK;` |
|      16 |  1939 | `}` |
|       - |  1940 | `/*` |
|       - |  1941 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1942 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1943 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1944 | ` */` |
|     116 |  1945 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       5 |  1946 | `{` |
|       - |  1947 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1948 | `	SyToken *pNext;` |
|       - |  1949 | `	SyToken *pClassifyIn;` |
|     121 |  1950 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|       - |  1951 | `	sxi32 nExpr;` |
|       - |  1952 | `	sxi32 rc;` |
|       - |  1953 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|       - |  1954 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|       - |  1955 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|       - |  1956 | `	 * list. */` |
|     121 |  1957 | `	pClassifyIn = pGen->pIn;` |
|     341 |  1958 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     225 |  1959 | `		if( pGen->pIn >= pNext ){` |
|      13 |  1960 | `			nEmpty++;` |
|     219 |  1961 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|      38 |  1962 | `			nKeyed++;` |
|      20 |  1963 | `		}else{` |
|     177 |  1964 | `			nPositional++;` |
|       - |  1965 | `		}` |
|     225 |  1966 | `		pGen->pIn = &pNext[1];` |
|       5 |  1967 | `	}` |
|     121 |  1968 | `	pGen->pIn = pClassifyIn;` |
|     121 |  1969 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|     ! 0 |  1970 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1971 | `			"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1972 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1973 | `	}` |
|     121 |  1974 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|     ! 0 |  1975 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1976 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|     ! 0 |  1977 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1978 | `	}` |
|     121 |  1979 | `	if( nKeyed > 0 ){` |
|      30 |  1980 | `		return GenStateCompileKeyedListBody(pGen);` |
|       - |  1981 | `	}` |
|      93 |  1982 | `	nExpr = 0;` |
|      93 |  1983 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     277 |  1984 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     189 |  1985 | `		if( pGen->pIn < pNext ){` |
|       - |  1986 | `			/* Check for nested list() */` |
|     177 |  1987 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  1988 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  1989 | `				/* Record this nested list for post-processing */` |
|       3 |  1990 | `				SyToken *pListEnd = 0;` |
|       3 |  1991 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  1992 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  1993 | `				}` |
|       3 |  1994 | `				if( pListEnd ){` |
|       - |  1995 | `					struct NestedListEntry sEntry;` |
|       3 |  1996 | `					sEntry.nIndex = nExpr;` |
|       3 |  1997 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  1998 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  1999 | `					sEntry.isShort = 0;` |
|       3 |  2000 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  2001 | `				}` |
|       - |  2002 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  2003 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     176 |  2004 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  2005 | `				/* Nested short destructuring [...] */` |
|      13 |  2006 | `				SyToken *pBracketEnd = 0;` |
|      13 |  2007 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  2008 | `				if( pBracketEnd ){` |
|       - |  2009 | `					struct NestedListEntry sEntry;` |
|      13 |  2010 | `					sEntry.nIndex = nExpr;` |
|      13 |  2011 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  2012 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  2013 | `					sEntry.isShort = 1;` |
|      13 |  2014 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  2015 | `				}` |
|       - |  2016 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  2017 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  2018 | `			}else{` |
|       - |  2019 | `				/* Compile the expression holding the variable */` |
|     163 |  2020 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     163 |  2021 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  2022 | `					SySetRelease(&sNested);` |
|     ! 0 |  2023 | `					return SXRET_OK;` |
|       - |  2024 | `				}` |
|       - |  2025 | `			}` |
|      91 |  2026 | `		}else{` |
|       - |  2027 | `			/* Empty entry,load NULL */` |
|      13 |  2028 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  2029 | `		}` |
|     189 |  2030 | `		nExpr++;` |
|       - |  2031 | `		/* Advance the stream cursor */` |
|     189 |  2032 | `		pGen->pIn = &pNext[1];` |
|       5 |  2033 | `	}` |
|       - |  2034 | `	/* Emit the LOAD_LIST instruction */` |
|      93 |  2035 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  2036 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  2037 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  2038 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  2039 | `	 */` |
|      93 |  2040 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  2041 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  2042 | `		sxu32 i;` |
|      27 |  2043 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  2044 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  2045 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  2046 | `			ph7_value *pIdx;` |
|       - |  2047 | `			sxu32 nConstIdx;` |
|       - |  2048 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  2049 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  2050 | `			/* Push the integer index for this nested entry */` |
|      15 |  2051 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  2052 | `			if( pIdx == 0 ){` |
|     ! 0 |  2053 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2054 | `				SySetRelease(&sNested);` |
|     ! 0 |  2055 | `				return SXERR_ABORT;` |
|       - |  2056 | `			}` |
|      15 |  2057 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  2058 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  2059 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  2060 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  2061 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  2062 | `			 */` |
|      15 |  2063 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  2064 | `			/* Recursively compile the inner list */` |
|      15 |  2065 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  2066 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  2067 | `			if( apNested[i].isShort ){` |
|      13 |  2068 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  2069 | `			}else{` |
|       3 |  2070 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  2071 | `			}` |
|      15 |  2072 | `			pGen->pIn = pSavedIn;` |
|      15 |  2073 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  2074 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2075 | `				SySetRelease(&sNested);` |
|     ! 0 |  2076 | `				return SXERR_ABORT;` |
|       - |  2077 | `			}` |
|       - |  2078 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  2079 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  2080 | `		}` |
|       6 |  2081 | `	}` |
|      93 |  2082 | `	SySetRelease(&sNested);` |
|       - |  2083 | `	/* Node successfully compiled */` |
|      93 |  2084 | `	return SXRET_OK;` |
|      63 |  2085 | `}` |
|      38 |  2086 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2087 | `{` |
|       - |  2088 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      43 |  2089 | `	pGen->pIn += 2;` |
|      43 |  2090 | `	pGen->pEnd--;` |
|      19 |  2091 | `	SXUNUSED(iCompileFlag);` |
|      43 |  2092 | `	return GenStateCompileListBody(pGen);` |
|       5 |  2093 | `}` |
|      78 |  2094 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2095 | `{` |
|       - |  2096 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      82 |  2097 | `	pGen->pIn++;` |
|      82 |  2098 | `	pGen->pEnd--;` |
|      39 |  2099 | `	SXUNUSED(iCompileFlag);` |
|      82 |  2100 | `	return GenStateCompileListBody(pGen);` |
|       4 |  2101 | `}` |
|       - |  2102 | `/* Forward declarations */` |
|       - |  2103 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  2104 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  2105 | `static int GenStateIsReadonly(SyToken *pTok);` |
|       - |  2106 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|       - |  2107 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|       - |  2108 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  2109 | `/*` |
|       - |  2110 | ` * Compile an annoynmous function or a closure.` |
|       - |  2111 | ` * According to the PHP language reference` |
|       - |  2112 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  2113 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  2114 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  2115 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  2116 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  2117 | ` *  Example Anonymous function variable assignment example` |
|       - |  2118 | ` * <?php` |
|       - |  2119 | ` * $greet = function($name)` |
|       - |  2120 | ` * {` |
|       - |  2121 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  2122 | ` * };` |
|       - |  2123 | ` * $greet('World');` |
|       - |  2124 | ` * $greet('PHP');` |
|       - |  2125 | ` * ?>` |
|       - |  2126 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  2127 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  2128 | ` */` |
|     324 |  2129 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2130 | `{` |
|       - |  2131 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  2132 | `	char zName[512];         /* Unique lambda name */` |
|       - |  2133 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  2134 | `							  * one thread is allowed to compile the script.` |
|       - |  2135 | `						      */` |
|       - |  2136 | `	SyString sName;` |
|       - |  2137 | `	sxu32 nLen;` |
|       - |  2138 | `	sxi32 rc;` |
|     162 |  2139 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2140 |  |
|     329 |  2141 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     329 |  2142 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  2143 | `		pGen->pIn++;` |
|     ! 0 |  2144 | `	}` |
|       - |  2145 | `	/* Generate a unique name */` |
|     329 |  2146 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  2147 | `	/* Make sure the generated name is unique */` |
|     329 |  2148 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  2149 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  2150 | `	}` |
|     329 |  2151 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  2152 | `	/* Compile the lambda body */` |
|     329 |  2153 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     329 |  2154 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2155 | `		return SXERR_ABORT;` |
|       - |  2156 | `	}` |
|       - |  2157 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|       - |  2158 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|       - |  2159 | `	 * the handler wraps either in a Closure instance. */` |
|     329 |  2160 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       - |  2161 | `	/* Node successfully compiled */` |
|     329 |  2162 | `	return SXRET_OK;` |
|     167 |  2163 | `}` |
|       - |  2164 | `/*` |
|       - |  2165 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  2166 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  2167 | ` * enclosing arrow level, or has already been captured.` |
|       - |  2168 | ` */` |
|     186 |  2169 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  2170 | `	ph7_gen_state *pGen,` |
|       - |  2171 | `	ph7_vm_func *pFunc,` |
|       - |  2172 | `	const char *zName,` |
|       - |  2173 | `	sxu32 nByte,` |
|       - |  2174 | `	SyString *aShadow,` |
|       - |  2175 | `	sxu32 nShadow)` |
|       3 |  2176 | `{` |
|       - |  2177 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2178 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  2179 | `	sxu32 n, nEnv;` |
|       - |  2180 | `	char *zDup;` |
|     189 |  2181 | `	if( nByte == 0 ){` |
|     ! 0 |  2182 | `		return SXRET_OK;` |
|       - |  2183 | `	}` |
|     186 |  2184 | `	if( nByte == sizeof("this")-1` |
|     102 |  2185 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  2186 | `		return SXRET_OK;` |
|       - |  2187 | `	}` |
|     235 |  2188 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     174 |  2189 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     168 |  2190 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|     129 |  2191 | `			return SXRET_OK;` |
|       - |  2192 | `		}` |
|      26 |  2193 | `	}` |
|      59 |  2194 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      59 |  2195 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      87 |  2196 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  2197 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  2198 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  2199 | `			return SXRET_OK;` |
|       - |  2200 | `		}` |
|      15 |  2201 | `	}` |
|      59 |  2202 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      59 |  2203 | `	if( zDup == 0 ){` |
|     ! 0 |  2204 | `		return SXERR_ABORT;` |
|       - |  2205 | `	}` |
|      59 |  2206 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      59 |  2207 | `	sEnv.iFlags = 0;` |
|      59 |  2208 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      59 |  2209 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      59 |  2210 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      59 |  2211 | `	return SXRET_OK;` |
|      96 |  2212 | `}` |
|       - |  2213 | `/*` |
|       - |  2214 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  2215 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  2216 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  2217 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  2218 | ` */` |
|      46 |  2219 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  2220 | `	ph7_gen_state *pGen,` |
|       - |  2221 | `	ph7_vm_func *pFunc,` |
|       - |  2222 | `	const char *zIn,` |
|       - |  2223 | `	const char *zEnd,` |
|       - |  2224 | `	SyString *aShadow,` |
|       - |  2225 | `	sxu32 nShadow)` |
|       2 |  2226 | `{` |
|       - |  2227 | `	sxi32 rc;` |
|     342 |  2228 | `	while( zIn < zEnd ){` |
|     296 |  2229 | `		if( zIn[0] == '\\' ){` |
|       5 |  2230 | `			zIn++;` |
|       5 |  2231 | `			if( zIn < zEnd ){` |
|       5 |  2232 | `				zIn++;` |
|       2 |  2233 | `			}` |
|       5 |  2234 | `			continue;` |
|       - |  2235 | `		}` |
|     290 |  2236 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      22 |  2237 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      20 |  2238 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  2239 | `			const char *zName;` |
|      22 |  2240 | `			zIn++; /* skip '$' */` |
|      22 |  2241 | `			zName = zIn;` |
|      74 |  2242 | `			while( zIn < zEnd ){` |
|      70 |  2243 | `				unsigned char c = (unsigned char)zIn[0];` |
|      70 |  2244 | `				if( c >= 0xc0 ){` |
|     ! 0 |  2245 | `					zIn++;` |
|     ! 0 |  2246 | `					while( zIn < zEnd` |
|     ! 0 |  2247 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  2248 | `						zIn++;` |
|     ! 0 |  2249 | `					}` |
|     ! 0 |  2250 | `					continue;` |
|       - |  2251 | `				}` |
|      70 |  2252 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      18 |  2253 | `					break;` |
|       - |  2254 | `				}` |
|      54 |  2255 | `				zIn++;` |
|       2 |  2256 | `			}` |
|      22 |  2257 | `			if( zIn > zName ){` |
|      32 |  2258 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      20 |  2259 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      22 |  2260 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2261 | `					return SXERR_ABORT;` |
|       - |  2262 | `				}` |
|      10 |  2263 | `			}` |
|      22 |  2264 | `			continue;` |
|       - |  2265 | `		}` |
|     272 |  2266 | `		zIn++;` |
|       2 |  2267 | `	}` |
|      48 |  2268 | `	return SXRET_OK;` |
|      25 |  2269 | `}` |
|       - |  2270 | `/*` |
|       - |  2271 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  2272 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  2273 | ` *   - plain $<id> pairs` |
|       - |  2274 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  2275 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  2276 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  2277 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  2278 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  2279 | ` *     are never mistakenly captured.` |
|       - |  2280 | ` */` |
|     234 |  2281 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  2282 | `	ph7_gen_state *pGen,` |
|       - |  2283 | `	ph7_vm_func *pFunc,` |
|       - |  2284 | `	SyToken *pStart,` |
|       - |  2285 | `	SyToken *pEnd,` |
|       - |  2286 | `	SyString *aShadow,` |
|       - |  2287 | `	sxu32 nShadow)` |
|       4 |  2288 | `{` |
|     238 |  2289 | `	SyToken *pScan = pStart;` |
|       - |  2290 | `	sxi32 rc;` |
|    1156 |  2291 | `	while( pScan < pEnd ){` |
|     922 |  2292 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      71 |  2293 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|      23 |  2294 | `				pScan->sData.zString,` |
|      46 |  2295 | `				pScan->sData.zString + pScan->sData.nByte,` |
|      23 |  2296 | `				aShadow,nShadow);` |
|      48 |  2297 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2298 | `				return SXERR_ABORT;` |
|       - |  2299 | `			}` |
|      48 |  2300 | `			pScan++;` |
|      48 |  2301 | `			continue;` |
|       - |  2302 | `		}` |
|     876 |  2303 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      24 |  2304 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      24 |  2305 | `			SyToken *pFnKw = pScan;` |
|      22 |  2306 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2307 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       2 |  2308 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2309 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2310 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2311 | `			}` |
|      24 |  2312 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2313 | `				SyToken *pInnerSigStart;` |
|       - |  2314 | `				SyToken *pInnerSigEnd;` |
|       - |  2315 | `				SyToken *pInnerBodyEnd;` |
|       - |  2316 | `				SyString *aInnerShadow;` |
|       - |  2317 | `				sxu32 nInnerShadow;` |
|       - |  2318 | `				sxu32 nInnerParamMax;` |
|       - |  2319 | `				SyToken *p;` |
|       - |  2320 | `				int iNestInner;` |
|      19 |  2321 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2322 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2323 | `					pScan++;` |
|     ! 0 |  2324 | `				}` |
|      19 |  2325 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2326 | `					pScan++;` |
|     ! 0 |  2327 | `					continue;` |
|       - |  2328 | `				}` |
|      19 |  2329 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2330 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2331 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2332 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2333 | `					pScan = pEnd;` |
|     ! 0 |  2334 | `					continue;` |
|       - |  2335 | `				}` |
|       - |  2336 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2337 | `				nInnerParamMax = 0;` |
|      57 |  2338 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2339 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2340 | `						nInnerParamMax++;` |
|       6 |  2341 | `					}` |
|      20 |  2342 | `				}` |
|      19 |  2343 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2344 | `					&pGen->pVm->sAllocator,` |
|      18 |  2345 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2346 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2347 | `					return SXERR_ABORT;` |
|       - |  2348 | `				}` |
|      19 |  2349 | `				nInnerShadow = 0;` |
|      25 |  2350 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2351 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2352 | `				}` |
|      57 |  2353 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2354 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2355 | `						continue;` |
|       - |  2356 | `					}` |
|      13 |  2357 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2358 | `						break;` |
|       - |  2359 | `					}` |
|      13 |  2360 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2361 | `						continue;` |
|       - |  2362 | `					}` |
|      13 |  2363 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2364 | `				}` |
|      19 |  2365 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2366 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2367 | `					pScan++;` |
|     ! 0 |  2368 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2369 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2370 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2371 | `						pScan++;` |
|     ! 0 |  2372 | `					}` |
|     ! 0 |  2373 | `					if( pScan < pEnd` |
|     ! 0 |  2374 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2375 | `						pScan++;` |
|     ! 0 |  2376 | `					}` |
|     ! 0 |  2377 | `				}` |
|      19 |  2378 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2379 | `					pScan++; /* past '=>' */` |
|       9 |  2380 | `				}` |
|      19 |  2381 | `				pInnerBodyEnd = pScan;` |
|      19 |  2382 | `				iNestInner = 0;` |
|     131 |  2383 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2384 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2385 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2386 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2387 | `						break;` |
|       - |  2388 | `					}` |
|     113 |  2389 | `					if( pInnerBodyEnd->nType &` |
|       - |  2390 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2391 | `						iNestInner++;` |
|     112 |  2392 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2393 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2394 | `						iNestInner--;` |
|       1 |  2395 | `					}` |
|     113 |  2396 | `					pInnerBodyEnd++;` |
|       1 |  2397 | `				}` |
|       - |  2398 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2399 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2400 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2401 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2402 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2403 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2404 | `				 *` |
|       - |  2405 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2406 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2407 | `				 * range after the '=' sign. */` |
|       - |  2408 | `				{` |
|      19 |  2409 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2410 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2411 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2412 | `						SyToken *pEq = 0;` |
|      13 |  2413 | `						int iNestArg = 0;` |
|      49 |  2414 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2415 | `							if( iNestArg == 0` |
|      39 |  2416 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2417 | `								break;` |
|       - |  2418 | `							}` |
|      37 |  2419 | `							if( pArgEnd->nType &` |
|       - |  2420 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2421 | `								iNestArg++;` |
|      37 |  2422 | `							}else if( pArgEnd->nType &` |
|       - |  2423 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2424 | `								iNestArg--;` |
|     ! 0 |  2425 | `							}` |
|      36 |  2426 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2427 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2428 | `								pEq = pArgEnd;` |
|       3 |  2429 | `							}` |
|      37 |  2430 | `							pArgEnd++;` |
|       1 |  2431 | `						}` |
|      13 |  2432 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2433 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2434 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2435 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2436 | `								return SXERR_ABORT;` |
|       - |  2437 | `							}` |
|       3 |  2438 | `						}` |
|      13 |  2439 | `						pArgStart = pArgEnd;` |
|      12 |  2440 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2441 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2442 | `							pArgStart++;` |
|       1 |  2443 | `						}` |
|       1 |  2444 | `					}` |
|       - |  2445 | `				}` |
|      28 |  2446 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2447 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2448 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2449 | `					return SXERR_ABORT;` |
|       - |  2450 | `				}` |
|      19 |  2451 | `				pScan = pInnerBodyEnd;` |
|      19 |  2452 | `				continue;` |
|       - |  2453 | `			}` |
|       2 |  2454 | `		}` |
|     858 |  2455 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     692 |  2456 | `			pScan++;` |
|     692 |  2457 | `			continue;` |
|       - |  2458 | `		}` |
|       - |  2459 | `		{` |
|       - |  2460 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     169 |  2461 | `			SyToken *pDollar = pScan;` |
|     249 |  2462 | `			while( &pDollar[1] < pEnd` |
|     169 |  2463 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2464 | `				pDollar++;` |
|     ! 0 |  2465 | `			}` |
|     169 |  2466 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2467 | `				break;` |
|       - |  2468 | `			}` |
|     169 |  2469 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2470 | `				pScan = pDollar + 1;` |
|     ! 0 |  2471 | `				continue;` |
|       - |  2472 | `			}` |
|     252 |  2473 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     166 |  2474 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      83 |  2475 | `				aShadow,nShadow);` |
|     169 |  2476 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2477 | `				return SXERR_ABORT;` |
|       - |  2478 | `			}` |
|     169 |  2479 | `			pScan = pDollar + 2;` |
|       - |  2480 | `		}` |
|       3 |  2481 | `	}` |
|     238 |  2482 | `	return SXRET_OK;` |
|     121 |  2483 | `}` |
|       - |  2484 | `/*` |
|       - |  2485 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2486 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2487 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2488 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2489 | ` * $this is also made available.` |
|       - |  2490 | ` */` |
|     216 |  2491 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2492 | `{` |
|       - |  2493 | `	ph7_vm_func *pFunc;` |
|       - |  2494 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2495 | `	GenBlock *pBlock;` |
|       - |  2496 | `	SySet *pInstrContainer;` |
|       - |  2497 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2498 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2499 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2500 | `	SyToken *pSavedEnd;` |
|       - |  2501 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2502 | `	char zName[512];` |
|       - |  2503 | `	static int iCnt = 1;` |
|       - |  2504 | `	char *zDup;` |
|       - |  2505 | `	sxu32 nLen;` |
|       - |  2506 | `	sxu32 nLine;` |
|     221 |  2507 | `	sxi32 iFlags = 0;` |
|     221 |  2508 | `	int bStatic = 0;` |
|       - |  2509 | `	sxi32 rc;` |
|       - |  2510 | `	sxu32 n;` |
|     108 |  2511 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2512 |  |
|     221 |  2513 | `	nLine = pGen->pIn->nLine;` |
|       - |  2514 | `	/* Optional 'static' prefix */` |
|     216 |  2515 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     221 |  2516 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2517 | `		bStatic = 1;` |
|       3 |  2518 | `		pGen->pIn++;` |
|       1 |  2519 | `	}` |
|       - |  2520 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     216 |  2521 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     221 |  2522 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2523 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2524 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2525 | `		return SXERR_SYNTAX;` |
|       - |  2526 | `	}` |
|     221 |  2527 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2528 | `	/* Optional '&' — return by reference */` |
|     221 |  2529 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2530 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2531 | `		pGen->pIn++;` |
|     ! 0 |  2532 | `	}` |
|       - |  2533 | `	/* Expect '(' */` |
|     221 |  2534 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2535 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2536 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2537 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2538 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2539 | `		}else{` |
|     ! 0 |  2540 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2541 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2542 | `		}` |
|       3 |  2543 | `		return SXERR_SYNTAX;` |
|       - |  2544 | `	}` |
|     219 |  2545 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2546 | `	/* Delimit the parameter list */` |
|     219 |  2547 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     219 |  2548 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2549 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2550 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2551 | `		return SXERR_SYNTAX;` |
|       - |  2552 | `	}` |
|       - |  2553 | `	/* Allocate the function state */` |
|     217 |  2554 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     217 |  2555 | `	if( pFunc == 0 ){` |
|     ! 0 |  2556 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2557 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2558 | `		return SXERR_ABORT;` |
|       - |  2559 | `	}` |
|       - |  2560 | `	/* Generate a unique lambda name */` |
|     217 |  2561 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     289 |  2562 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      74 |  2563 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2564 | `	}` |
|     217 |  2565 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     217 |  2566 | `	if( zDup == 0 ){` |
|     ! 0 |  2567 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2568 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2569 | `		return SXERR_ABORT;` |
|       - |  2570 | `	}` |
|     217 |  2571 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2572 | `	/* Collect function arguments */` |
|     217 |  2573 | `	if( pGen->pIn < pSigEnd ){` |
|     106 |  2574 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|     106 |  2575 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2576 | `			return SXERR_ABORT;` |
|       - |  2577 | `		}` |
|      51 |  2578 | `	}` |
|       - |  2579 | `	/* Point past ')' and parse optional return type */` |
|     217 |  2580 | `	pGen->pIn = &pSigEnd[1];` |
|     217 |  2581 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     217 |  2582 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2583 | `		return SXERR_ABORT;` |
|     217 |  2584 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2585 | `		return SXERR_SYNTAX;` |
|       - |  2586 | `	}` |
|       - |  2587 | `	/* Expect '=>' */` |
|     217 |  2588 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2589 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2590 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2591 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2592 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2593 | `		}else{` |
|     ! 0 |  2594 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2595 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2596 | `		}` |
|       3 |  2597 | `		return SXERR_SYNTAX;` |
|       - |  2598 | `	}` |
|     214 |  2599 | `	pGen->pIn++; /* Jump '=>' */` |
|     214 |  2600 | `	pBodyStart = pGen->pIn;` |
|     214 |  2601 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2602 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2603 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2604 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2605 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     214 |  2606 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2607 | `	{` |
|     214 |  2608 | `		SyString *aShadow = 0;` |
|     214 |  2609 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     214 |  2610 | `		if( nShadow > 0 ){` |
|     103 |  2611 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|     100 |  2612 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|     103 |  2613 | `			if( aShadow == 0 ){` |
|     ! 0 |  2614 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2615 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2616 | `				return SXERR_ABORT;` |
|       - |  2617 | `			}` |
|     229 |  2618 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     129 |  2619 | `				aShadow[n] = aArgs[n].sName;` |
|      66 |  2620 | `			}` |
|      50 |  2621 | `		}` |
|     319 |  2622 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|     105 |  2623 | `			aShadow,nShadow);` |
|     214 |  2624 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2625 | `			return SXERR_ABORT;` |
|       - |  2626 | `		}` |
|       - |  2627 | `	}` |
|       - |  2628 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2629 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2630 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2631 | `	 * $this. */` |
|     214 |  2632 | `	if( !bStatic ){` |
|       - |  2633 | `		char *zThisDup;` |
|     212 |  2634 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     212 |  2635 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2636 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2637 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2638 | `			return SXERR_ABORT;` |
|       - |  2639 | `		}` |
|     212 |  2640 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     212 |  2641 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     212 |  2642 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     212 |  2643 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     212 |  2644 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|     104 |  2645 | `	}` |
|       - |  2646 | `	/* Arrow functions are always closures */` |
|     214 |  2647 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2648 | `	/* Compile the body expression as an implicit return */` |
|     319 |  2649 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     105 |  2650 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     214 |  2651 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2652 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2653 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2654 | `		return SXERR_ABORT;` |
|       - |  2655 | `	}` |
|     214 |  2656 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     214 |  2657 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     214 |  2658 | `	pSavedEnd = pGen->pEnd;` |
|     214 |  2659 | `	pGen->pIn = pBodyStart;` |
|     214 |  2660 | `	pGen->pEnd = pBodyEnd;` |
|     214 |  2661 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     214 |  2662 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2663 | `		return SXERR_ABORT;` |
|       - |  2664 | `	}` |
|       - |  2665 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2666 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2667 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2668 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     214 |  2669 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     214 |  2670 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     214 |  2671 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     214 |  2672 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     214 |  2673 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2674 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     214 |  2675 | `	pGen->pIn = pBodyEnd;` |
|     214 |  2676 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2677 | `	/* Emit the load-closure instruction */` |
|     214 |  2678 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     214 |  2679 | `	return SXRET_OK;` |
|     113 |  2680 | `}` |
|       - |  2681 | `/*` |
|       - |  2682 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2683 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2684 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2685 | ` * expression's value.` |
|       - |  2686 | ` */` |
|     346 |  2687 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2688 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  2689 | `{` |
|       - |  2690 | `	SySet *pInstrContainer;` |
|       - |  2691 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2692 | `	GenBlock *pArmBlock;` |
|       - |  2693 | `	sxi32 rc;` |
|     349 |  2694 | `	pTmpIn  = pGen->pIn;` |
|     349 |  2695 | `	pTmpEnd = pGen->pEnd;` |
|     349 |  2696 | `	pGen->pIn  = pStart;` |
|     349 |  2697 | `	pGen->pEnd = pStop;` |
|     349 |  2698 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     349 |  2699 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2700 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2701 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2702 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2703 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2704 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     522 |  2705 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2706 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     349 |  2707 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2708 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2709 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2710 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2711 | `		return SXERR_ABORT;` |
|       - |  2712 | `	}` |
|     349 |  2713 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     349 |  2714 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     349 |  2715 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     349 |  2716 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     349 |  2717 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     349 |  2718 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     349 |  2719 | `	pGen->pIn  = pTmpIn;` |
|     349 |  2720 | `	pGen->pEnd = pTmpEnd;` |
|     349 |  2721 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2722 | `		return SXERR_ABORT;` |
|       - |  2723 | `	}` |
|     349 |  2724 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2725 | `		return SXERR_EMPTY;` |
|       - |  2726 | `	}` |
|     349 |  2727 | `	return SXRET_OK;` |
|     176 |  2728 | `}` |
|       - |  2729 | `/*` |
|       - |  2730 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2731 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2732 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2733 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2734 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2735 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2736 | ` */` |
|       - |  2737 | `/*` |
|       - |  2738 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2739 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2740 | ` * caller can bail out of the current expression.` |
|       - |  2741 | ` */` |
|       2 |  2742 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2743 | `{` |
|       - |  2744 | `	va_list ap;` |
|       - |  2745 | `	sxi32 rc;` |
|       - |  2746 | `	SyBlob sMsg;` |
|       3 |  2747 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2748 | `	va_start(ap,zFmt);` |
|       3 |  2749 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2750 | `	va_end(ap);` |
|       3 |  2751 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2752 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2753 | `	SyBlobRelease(&sMsg);` |
|       3 |  2754 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2755 | `		return SXERR_ABORT;` |
|       - |  2756 | `	}` |
|       3 |  2757 | `	return SXERR_SYNTAX;` |
|       2 |  2758 | `}` |
|       - |  2759 | `/*` |
|       - |  2760 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2761 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2762 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2763 | ` */` |
|     348 |  2764 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  2765 | `{` |
|     352 |  2766 | `	SyToken *pCur = pStart;` |
|     352 |  2767 | `	int iNest = 0;` |
|     814 |  2768 | `	while( pCur < pEnd ){` |
|     780 |  2769 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2770 | `			iNest++;` |
|     774 |  2771 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2772 | `			iNest--;` |
|     762 |  2773 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     317 |  2774 | `			return pCur;` |
|       - |  2775 | `		}` |
|     466 |  2776 | `		pCur++;` |
|       4 |  2777 | `	}` |
|      37 |  2778 | `	return pEnd;` |
|     178 |  2779 | `}` |
|      70 |  2780 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2781 | `{` |
|       - |  2782 | `	ph7_match *pMatch;` |
|       - |  2783 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      75 |  2784 | `	int bHasDefault = 0;` |
|       - |  2785 | `	sxu32 nLine;` |
|       - |  2786 | `	sxi32 rc;` |
|      35 |  2787 | `	SXUNUSED(iCompileFlag);` |
|      75 |  2788 | `	nLine = pGen->pIn->nLine;` |
|      75 |  2789 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2790 | `	/* Expect '(' */` |
|      75 |  2791 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2792 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2793 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2794 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2795 | `	}` |
|      75 |  2796 | `	pGen->pIn++; /* Jump '(' */` |
|      75 |  2797 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      75 |  2798 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2799 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2800 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2801 | `	}` |
|      75 |  2802 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2803 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2804 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2805 | `	}` |
|       - |  2806 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      75 |  2807 | `	pSavedEnd = pGen->pEnd;` |
|      75 |  2808 | `	pGen->pEnd = pSubjEnd;` |
|      75 |  2809 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      75 |  2810 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2811 | `		return SXERR_ABORT;` |
|       - |  2812 | `	}` |
|      75 |  2813 | `	pGen->pEnd = pSavedEnd;` |
|      75 |  2814 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2815 | `	/* Expect '{' */` |
|      75 |  2816 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2817 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2818 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2819 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2820 | `	}` |
|      75 |  2821 | `	pGen->pIn++; /* Jump '{' */` |
|      75 |  2822 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      75 |  2823 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2824 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2825 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2826 | `	}` |
|       - |  2827 | `	/* Allocate ph7_match container */` |
|      75 |  2828 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      75 |  2829 | `	if( pMatch == 0 ){` |
|     ! 0 |  2830 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2831 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2832 | `		return SXERR_ABORT;` |
|       - |  2833 | `	}` |
|      75 |  2834 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      75 |  2835 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2836 | `	/* Iterate arms */` |
|     253 |  2837 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2838 | `		ph7_match_arm sArm;` |
|       - |  2839 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     186 |  2840 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     186 |  2841 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     186 |  2842 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     186 |  2843 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2844 | `		/* 'default' arm? */` |
|     182 |  2845 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 |  2846 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2847 | `			if( bHasDefault ){` |
|       3 |  2848 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2849 | `					"Match expressions may only contain one default arm");` |
|       4 |  2850 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2851 | `			}` |
|      20 |  2852 | `			sArm.bDefault = 1;` |
|      20 |  2853 | `			bHasDefault = 1;` |
|      20 |  2854 | `			pGen->pIn++;` |
|      20 |  2855 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2856 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2857 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2858 | `			}` |
|      20 |  2859 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2860 | `		}else{` |
|       - |  2861 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     166 |  2862 | `			pCondStart = pGen->pIn;` |
|     166 |  2863 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2864 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     174 |  2865 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2866 | `				SySet sCondBc;` |
|       9 |  2867 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2868 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2869 | `						"syntax error, empty match condition expression");` |
|       - |  2870 | `				}` |
|       9 |  2871 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2872 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2873 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2874 | `					return SXERR_ABORT;` |
|       - |  2875 | `				}` |
|       9 |  2876 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2877 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2878 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2879 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2880 | `			}` |
|     166 |  2881 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2882 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2883 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2884 | `			}` |
|     163 |  2885 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2886 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2887 | `					"syntax error, empty match condition expression");` |
|       - |  2888 | `			}` |
|       - |  2889 | `			{` |
|       - |  2890 | `				SySet sCondBc;` |
|     163 |  2891 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     163 |  2892 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     163 |  2893 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2894 | `					return SXERR_ABORT;` |
|       - |  2895 | `				}` |
|     163 |  2896 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2897 | `			}` |
|     163 |  2898 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2899 | `		}` |
|       - |  2900 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     181 |  2901 | `		pResStart = pGen->pIn;` |
|     181 |  2902 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     181 |  2903 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2904 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2905 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2906 | `		}` |
|     181 |  2907 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     181 |  2908 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2909 | `			return SXERR_ABORT;` |
|       - |  2910 | `		}` |
|     181 |  2911 | `		pGen->pIn = pResEnd;` |
|     181 |  2912 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     149 |  2913 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2914 | `		}` |
|     181 |  2915 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       3 |  2916 | `	}` |
|      69 |  2917 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      69 |  2918 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      69 |  2919 | `	return SXRET_OK;` |
|      40 |  2920 | `}` |
|       - |  2921 | `/*` |
|       - |  2922 | ` * Compile a backtick quoted string.` |
|       - |  2923 | ` */` |
|       4 |  2924 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2925 | `{` |
|       - |  2926 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2927 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2928 | `	 */` |
|       8 |  2929 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2930 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2931 | `		ph7_lib_version()` |
|       - |  2932 | `		);` |
|       - |  2933 | `	/* Load NULL */` |
|       6 |  2934 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2935 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2936 | `	/* Node successfully compiled */` |
|       6 |  2937 | `	return SXRET_OK;` |
|       2 |  2938 | `}` |
|       - |  2939 | `/*` |
|       - |  2940 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2941 | ` * construct.` |
|       - |  2942 | ` */` |
|      82 |  2943 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2944 | `{` |
|       - |  2945 | `	SyString *pName;` |
|       - |  2946 | `	sxu32 nKeyID;` |
|       - |  2947 | `	sxi32 rc;` |
|       - |  2948 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      87 |  2949 | `	pName = &pGen->pIn->sData;` |
|      87 |  2950 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      87 |  2951 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      87 |  2952 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2953 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2954 | `		/* Compile arguments one after one */` |
|       9 |  2955 | `		pTmp = pGen->pEnd;` |
|       - |  2956 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2957 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2958 | `		 *  mean that the following expression is valid:` |
|       - |  2959 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2960 | `		 */` |
|       9 |  2961 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2962 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2963 | `			if( pGen->pIn < pNext ){` |
|       9 |  2964 | `				pGen->pEnd = pNext;` |
|       9 |  2965 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2966 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2967 | `					return SXERR_ABORT;` |
|       - |  2968 | `				}` |
|       9 |  2969 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2970 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2971 | `					 * without the overhead of a function call.` |
|       - |  2972 | `					 * This is a very powerful optimization that improve` |
|       - |  2973 | `					 * performance greatly.` |
|       - |  2974 | `					 */` |
|       9 |  2975 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2976 | `				}` |
|       4 |  2977 | `			}` |
|       - |  2978 | `			/* Jump trailing commas */` |
|       9 |  2979 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2980 | `				pNext++;` |
|     ! 0 |  2981 | `			}` |
|       9 |  2982 | `			pGen->pIn = pNext;` |
|       1 |  2983 | `		}` |
|       - |  2984 | `		/* Restore token stream */` |
|       9 |  2985 | `		pGen->pEnd = pTmp;` |
|       5 |  2986 | `	}else{` |
|      79 |  2987 | `		sxi32 nArg = 0;` |
|      79 |  2988 | `		sxu32 nIdx = 0;` |
|      79 |  2989 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      79 |  2990 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2991 | `			return SXERR_ABORT;` |
|      79 |  2992 | `		}else if(rc != SXERR_EMPTY ){` |
|      79 |  2993 | `			nArg = 1;` |
|      37 |  2994 | `		}` |
|      79 |  2995 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2996 | `			ph7_value *pObj;` |
|       - |  2997 | `			/* Emit the call instruction */` |
|      31 |  2998 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      31 |  2999 | `			if( pObj == 0 ){` |
|     ! 0 |  3000 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3001 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3002 | `				return SXERR_ABORT;` |
|       - |  3003 | `			}` |
|      31 |  3004 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  3005 | `			/* Install in the literal table */` |
|      31 |  3006 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      13 |  3007 | `		}` |
|       - |  3008 | `		/* Emit the call instruction */` |
|      79 |  3009 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      79 |  3010 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  3011 | `	}` |
|       - |  3012 | `	/* Node successfully compiled */` |
|      87 |  3013 | `	return SXRET_OK;` |
|      46 |  3014 | `}` |
|       - |  3015 | `/*` |
|       - |  3016 | ` * Compile a node holding a variable declaration.` |
|       - |  3017 | ` * According to the PHP language reference` |
|       - |  3018 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  3019 | ` *  The variable name is case-sensitive.` |
|       - |  3020 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  3021 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3022 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  3023 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  3024 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  3025 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  3026 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  3027 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  3028 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  3029 | ` *  the chapter on Expressions.` |
|       - |  3030 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  3031 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  3032 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  3033 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  3034 | ` *  is being assigned (the source variable).` |
|       - |  3035 | ` */` |
| 1229156 |  3036 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3037 | `{` |
| 1229161 |  3038 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3039 | `	sxi32 iVv;` |
|       - |  3040 | `	sxi32 iP1;` |
|       - |  3041 | `	void *p3;` |
|       - |  3042 | `	sxi32 rc;` |
| 1229161 |  3043 | `	iVv = -1; /* Variable variable counter */` |
| 2458329 |  3044 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1229173 |  3045 | `		pGen->pIn++;` |
| 1229173 |  3046 | `		iVv++;` |
|       5 |  3047 | `	}` |
| 1229161 |  3048 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  3049 | `		/* Invalid variable name */` |
|     ! 0 |  3050 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  3051 | `		if( rc == SXERR_ABORT ){` |
|       - |  3052 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3053 | `			return SXERR_ABORT;` |
|       - |  3054 | `		}` |
|     ! 0 |  3055 | `		return SXRET_OK;` |
|       - |  3056 | `	}` |
| 1229161 |  3057 | `	p3  = 0;` |
| 1229161 |  3058 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  3059 | `		/* Dynamic variable creation */` |
|      21 |  3060 | `		pGen->pIn++;  /* Jump the open curly */` |
|      21 |  3061 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      21 |  3062 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3063 | `			/* Empty expression */` |
|       3 |  3064 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  3065 | `			return SXRET_OK;` |
|       - |  3066 | `		}` |
|       - |  3067 | `		/* Compile the expression holding the variable name */` |
|      18 |  3068 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      18 |  3069 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3070 | `			return SXERR_ABORT;` |
|      18 |  3071 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  3072 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  3073 | `			return SXRET_OK;` |
|       - |  3074 | `		}` |
|       8 |  3075 | `	}else{` |
|       - |  3076 | `		SyHashEntry *pEntry;` |
|       - |  3077 | `		SyString *pName;` |
| 1229143 |  3078 | `		char *zName = 0;` |
|       - |  3079 | `		/* Extract variable name */` |
| 1229143 |  3080 | `		pName = &pGen->pIn->sData;` |
|       - |  3081 | `		/* Advance the stream cursor */` |
| 1229143 |  3082 | `		pGen->pIn++;` |
| 1229143 |  3083 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1229143 |  3084 | `		if( pEntry == 0 ){` |
|       - |  3085 | `			/* Duplicate name */` |
|  177075 |  3086 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  177075 |  3087 | `			if( zName == 0 ){` |
|     ! 0 |  3088 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3089 | `				return SXERR_ABORT;` |
|       - |  3090 | `			}` |
|       - |  3091 | `			/* Install in the hashtable */` |
|  177075 |  3092 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   88540 |  3093 | `		}else{` |
|       - |  3094 | `			/* Name already available */` |
| 1052073 |  3095 | `			zName = (char *)pEntry->pUserData;` |
|       - |  3096 | `		}` |
| 1229143 |  3097 | `		p3 = (void *)zName;` |
|       - |  3098 | `	}` |
| 1229157 |  3099 | `	iP1 = 0;` |
| 1229157 |  3100 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  479047 |  3101 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  3102 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  479029 |  3103 | `			iP1 = 1;` |
|  239512 |  3104 | `		}` |
|  239521 |  3105 | `	}` |
|       - |  3106 | `	/* Emit the load instruction */` |
| 1229157 |  3107 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1229169 |  3108 | `	while( iVv > 0 ){` |
|      13 |  3109 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  3110 | `		iVv--;` |
|       1 |  3111 | `	}` |
|       - |  3112 | `	/* Node successfully compiled */` |
| 1229157 |  3113 | `	return SXRET_OK;` |
|  614583 |  3114 | `}` |
|       - |  3115 | `/*` |
|       - |  3116 | ` * Load a literal.` |
|       - |  3117 | ` */` |
|  848734 |  3118 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  3119 | `{` |
|  848739 |  3120 | `	SyToken *pToken = pGen->pIn;` |
|       - |  3121 | `	ph7_value *pObj;` |
|       - |  3122 | `	SyString *pStr;` |
|       - |  3123 | `	sxu32 nIdx;` |
|       - |  3124 | `	/* Extract token value */` |
|  848739 |  3125 | `	pStr = &pToken->sData;` |
|       - |  3126 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  848739 |  3127 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  179851 |  3128 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  3129 | `			/* NULL constant are always indexed at 0 */` |
|   66087 |  3130 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   66087 |  3131 | `			return SXRET_OK;` |
|  113769 |  3132 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  3133 | `			/* TRUE constant are always indexed at 1 */` |
|     903 |  3134 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     903 |  3135 | `			return SXRET_OK;` |
|       5 |  3136 | `		}` |
|  782753 |  3137 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  114854 |  3138 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  3139 | `			/* FALSE constant are always indexed at 2 */` |
|   50561 |  3140 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   50561 |  3141 | `			return SXRET_OK;` |
|  678560 |  3142 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  120446 |  3143 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  3144 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|   11543 |  3145 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   11543 |  3146 | `			if( pObj == 0 ){` |
|     ! 0 |  3147 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3148 | `				return SXERR_ABORT;` |
|       - |  3149 | `			}` |
|   11543 |  3150 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  3151 | `			/* Emit the load constant instruction */` |
|   11543 |  3152 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   11543 |  3153 | `			return SXRET_OK;` |
|  626242 |  3154 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   38886 |  3155 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  3156 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       8 |  3157 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       8 |  3158 | `			if( pObj == 0 ){` |
|     ! 0 |  3159 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3160 | `				return SXERR_ABORT;` |
|       - |  3161 | `			}` |
|       8 |  3162 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  3163 | `				SyString sNs;` |
|       8 |  3164 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  3165 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       5 |  3166 | `			}else{` |
|     ! 0 |  3167 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  3168 | `			}` |
|       8 |  3169 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       8 |  3170 | `			return SXRET_OK;` |
|  625660 |  3171 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   16285 |  3172 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  617513 |  3173 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   21464 |  3174 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  3175 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  3176 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  3177 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  3178 | `				/* Point to the upper block */` |
|      11 |  3179 | `				pBlock = pBlock->pParent;` |
|       1 |  3180 | `			}` |
|      11 |  3181 | `			if( pBlock == 0 ){` |
|       - |  3182 | `				/* Called in the global scope,load NULL */` |
|       5 |  3183 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  3184 | `			}else{` |
|       - |  3185 | `				/* Extract the target function/method */` |
|       7 |  3186 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  3187 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  3188 | `					/* Not a class method,Load null */` |
|       3 |  3189 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  3190 | `				}else{` |
|       5 |  3191 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  3192 | `					if( pObj == 0 ){` |
|     ! 0 |  3193 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3194 | `						return SXERR_ABORT;` |
|       - |  3195 | `					}` |
|       5 |  3196 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  3197 | `					/* Emit the load constant instruction */` |
|       5 |  3198 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  3199 | `				}` |
|       - |  3200 | `			}` |
|      11 |  3201 | `			return SXRET_OK;` |
|       - |  3202 | `	}` |
|       - |  3203 | `	/* Query literal table */` |
|  719649 |  3204 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  3205 | `		ph7_value *pLitObj;` |
|       - |  3206 | `		/* Unknown literal,install it in the literal table */` |
|  306527 |  3207 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  306527 |  3208 | `		if( pLitObj == 0 ){` |
|     ! 0 |  3209 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3210 | `			return SXERR_ABORT;` |
|       - |  3211 | `		}` |
|  306527 |  3212 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  306527 |  3213 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  153261 |  3214 | `	}` |
|       - |  3215 | `	/* Emit the load constant instruction */` |
|  719649 |  3216 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  719649 |  3217 | `	return SXRET_OK;` |
|  424372 |  3218 | `}` |
|       - |  3219 | `/*` |
|       - |  3220 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  3221 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  3222 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  3223 | ` * Otherwise, load the simple literal directly.` |
|       - |  3224 | ` */` |
|  852626 |  3225 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  3226 | `{` |
|       - |  3227 | `	sxi32 rc;` |
|  852631 |  3228 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3229 | `		return SXRET_OK;` |
|       - |  3230 | `	}` |
|       - |  3231 | `	/* Check if this is a multi-token namespace path */` |
|  852631 |  3232 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  3233 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|    3897 |  3234 | `		SyBlob *pWorker = &pGen->sWorker;` |
|    3897 |  3235 | `		int isAbsolute = 0;` |
|    3897 |  3236 | `		SyBlobReset(pWorker);` |
|       - |  3237 | `		/* Check for leading backslash (absolute path) */` |
|    3897 |  3238 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|    3895 |  3239 | `			isAbsolute = 1;` |
|    3895 |  3240 | `			pGen->pIn++; /* Skip leading backslash */` |
|    1945 |  3241 | `		}` |
|       - |  3242 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|    3897 |  3243 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  3244 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  3245 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  3246 | `		}` |
|       - |  3247 | `		/* Collect all path components */` |
|    4005 |  3248 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    4005 |  3249 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      59 |  3250 | `				SyBlobAppend(pWorker,"\\",1);` |
|      32 |  3251 | `			}else{` |
|    3951 |  3252 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  3253 | `			}` |
|    4005 |  3254 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|    3897 |  3255 | `				pGen->pIn++;` |
|    3897 |  3256 | `				break;` |
|       - |  3257 | `			}` |
|     113 |  3258 | `			pGen->pIn++;` |
|       5 |  3259 | `		}` |
|    3897 |  3260 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  3261 | `			ph7_value *pObj;` |
|       - |  3262 | `			SyString sPath;` |
|       - |  3263 | `			sxu32 nIdx;` |
|    3897 |  3264 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  3265 | `			/* Install in the literal table */` |
|    3897 |  3266 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|    3869 |  3267 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    3869 |  3268 | `				if( pObj == 0 ){` |
|     ! 0 |  3269 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3270 | `					return SXERR_ABORT;` |
|       - |  3271 | `				}` |
|    3869 |  3272 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|    3869 |  3273 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|    1932 |  3274 | `			}` |
|       - |  3275 | `			/* Emit the load constant instruction.` |
|       - |  3276 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  3277 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|    5843 |  3278 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|    1946 |  3279 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|    1946 |  3280 | `				nIdx,0,0);` |
|    3897 |  3281 | `			return SXRET_OK;` |
|       - |  3282 | `		}` |
|     ! 0 |  3283 | `	}` |
|       - |  3284 | `	/* Single-token literal: load directly */` |
|  848739 |  3285 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  848739 |  3286 | `	return rc;` |
|  426318 |  3287 | `}` |
|       - |  3288 | `/*` |
|       - |  3289 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  3290 | ` */` |
|       - |  3291 | `/*` |
|       - |  3292 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|       - |  3293 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|       - |  3294 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|       - |  3295 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|       - |  3296 | ` */` |
|     ! 0 |  3297 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|     ! 0 |  3298 | `{` |
|     ! 0 |  3299 | `	SXUNUSED(iCompileFlag);` |
|     ! 0 |  3300 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|       - |  3301 | `		"Cannot use the first-class callable syntax '...' here");` |
|     ! 0 |  3302 | `	return SXERR_SYNTAX;` |
|     ! 0 |  3303 | `}` |
|  852626 |  3304 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3305 | `{` |
|       - |  3306 | `	sxi32 rc;` |
|  852631 |  3307 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  852631 |  3308 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3309 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3310 | `		return rc;` |
|       - |  3311 | `	}` |
|       - |  3312 | `	/* Node successfully compiled */` |
|  852631 |  3313 | `	return SXRET_OK;` |
|  426318 |  3314 | `}` |
|       - |  3315 | `/*` |
|       - |  3316 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3317 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3318 | ` */` |
|       8 |  3319 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3320 | `{` |
|       - |  3321 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3322 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3323 | `		pGen->pIn++;` |
|       1 |  3324 | `	}` |
|       9 |  3325 | `	return SXRET_OK;` |
|       1 |  3326 | `}` |
|       - |  3327 | `/*` |
|       - |  3328 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3329 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3330 | ` */` |
|     134 |  3331 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3332 | `{` |
|     139 |  3333 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      34 |  3334 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3335 | `			return TRUE;` |
|      32 |  3336 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       6 |  3337 | `			return TRUE;` |
|       3 |  3338 | `		}` |
|     121 |  3339 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3340 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3341 | `			return TRUE;` |
|       - |  3342 | `		}` |
|     ! 0 |  3343 | `	}` |
|       - |  3344 | `	/* Not a reserved constant */` |
|     131 |  3345 | `	return FALSE;` |
|      72 |  3346 | `}` |
|       - |  3347 | `/*` |
|       - |  3348 | ` * Compile the 'const' statement.` |
|       - |  3349 | ` * According to the PHP language reference` |
|       - |  3350 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3351 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3352 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3353 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3354 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3355 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3356 | ` *  Syntax` |
|       - |  3357 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3358 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3359 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3360 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3361 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3362 | ` *  to get a list of all defined constants.` |
|       - |  3363 | ` *` |
|       - |  3364 | ` * Symisc eXtension.` |
|       - |  3365 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3366 | ` *  would allow only simple scalar value.` |
|       - |  3367 | ` *  Example` |
|       - |  3368 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3369 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3370 | ` */` |
|      38 |  3371 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3372 | `{` |
|       - |  3373 | `	SySet *pConsCode,*pInstrContainer;` |
|      43 |  3374 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3375 | `	SyString *pName;` |
|       - |  3376 | `	sxi32 rc;` |
|      43 |  3377 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      43 |  3378 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3379 | `		/* Invalid constant name */` |
|       9 |  3380 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       9 |  3381 | `		if( rc == SXERR_ABORT ){` |
|       - |  3382 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3383 | `			return SXERR_ABORT;` |
|       - |  3384 | `		}` |
|       9 |  3385 | `		goto Synchronize;` |
|       - |  3386 | `	}` |
|       - |  3387 | `	/* Peek constant name */` |
|      37 |  3388 | `	pName = &pGen->pIn->sData;` |
|       - |  3389 | `	/* Make sure the constant name isn't reserved */` |
|      37 |  3390 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3391 | `		/* Reserved constant */` |
|      10 |  3392 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|      10 |  3393 | `		if( rc == SXERR_ABORT ){` |
|       - |  3394 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3395 | `			return SXERR_ABORT;` |
|       - |  3396 | `		}` |
|      10 |  3397 | `		goto Synchronize;` |
|       - |  3398 | `	}` |
|      28 |  3399 | `	pGen->pIn++;` |
|      28 |  3400 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3401 | `		/* Invalid statement*/` |
|       6 |  3402 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3403 | `		if( rc == SXERR_ABORT ){` |
|       - |  3404 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3405 | `			return SXERR_ABORT;` |
|       - |  3406 | `		}` |
|       6 |  3407 | `		goto Synchronize;` |
|       - |  3408 | `	}` |
|      22 |  3409 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3410 | `	/* Allocate a new constant value container */` |
|      22 |  3411 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      22 |  3412 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3413 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3414 | `		return SXERR_ABORT;` |
|       - |  3415 | `	}` |
|      22 |  3416 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3417 | `	/* Swap bytecode container */` |
|      22 |  3418 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      22 |  3419 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3420 | `	/* Compile constant value */` |
|      22 |  3421 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3422 | `	/* Emit the done instruction */` |
|      22 |  3423 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      22 |  3424 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      22 |  3425 | `	if( rc == SXERR_ABORT ){` |
|       - |  3426 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3427 | `		return SXERR_ABORT;` |
|       - |  3428 | `	}` |
|      22 |  3429 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3430 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3431 | `	{` |
|       - |  3432 | `		SyBlob sFQN;` |
|       - |  3433 | `		SyString sFQNStr;` |
|      22 |  3434 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      22 |  3435 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      22 |  3436 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      22 |  3437 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      22 |  3438 | `		SyBlobRelease(&sFQN);` |
|       - |  3439 | `	}` |
|      22 |  3440 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3441 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3442 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3443 | `	}` |
|      22 |  3444 | `	return SXRET_OK;` |
|       9 |  3445 | `Synchronize:` |
|       - |  3446 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3447 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      42 |  3448 | `		pGen->pIn++;` |
|       4 |  3449 | `	}` |
|      22 |  3450 | `	return SXRET_OK;` |
|      24 |  3451 | `}` |
|       - |  3452 | `/*` |
|       - |  3453 | ` * Compile the 'continue' statement.` |
|       - |  3454 | ` * According to the PHP language reference` |
|       - |  3455 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3456 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3457 | ` *  iteration.` |
|       - |  3458 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3459 | ` *  the purposes of continue.` |
|       - |  3460 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3461 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3462 | ` *  Note:` |
|       - |  3463 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3464 | ` */` |
|       - |  3465 | `/*` |
|       - |  3466 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3467 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3468 | ` * break/continue crosses a try boundary.` |
|       - |  3469 | ` *` |
|       - |  3470 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3471 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3472 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3473 | ` */` |
|    3990 |  3474 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3475 | `{` |
|    3995 |  3476 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    3995 |  3477 | `	int nInlineTry = 0;` |
|   23443 |  3478 | `	while( pBlock && pBlock != pTarget ){` |
|   19453 |  3479 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       6 |  3480 | `			if( pBlock->pUserData ){` |
|       - |  3481 | `				/* A try block with an exception context. In a generator its catch/finally` |
|       - |  3482 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|       - |  3483 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|       - |  3484 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|       6 |  3485 | `				if( pGen->bInGenerator ){` |
|       3 |  3486 | `					nInlineTry++;` |
|       2 |  3487 | `				}else{` |
|       3 |  3488 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       - |  3489 | `				}` |
|       4 |  3490 | `			}else{` |
|       - |  3491 | `				/* A catch/finally block compiled into a separate bytecode container` |
|       - |  3492 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|     ! 0 |  3493 | `				break;` |
|       - |  3494 | `			}` |
|       2 |  3495 | `		}` |
|   19453 |  3496 | `		pBlock = pBlock->pParent;` |
|       5 |  3497 | `	}` |
|    3995 |  3498 | `	return nInlineTry;` |
|       5 |  3499 | `}` |
|    3892 |  3500 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3501 | `{` |
|       - |  3502 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3503 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3504 | `	sxu32 nLineLocal;` |
|       - |  3505 | `	sxi32 rc;` |
|    3897 |  3506 | `	nLineLocal = pGen->pIn->nLine;` |
|    3897 |  3507 | `	iLevel = 0;` |
|       - |  3508 | `	/* Jump the 'continue' keyword */` |
|    3897 |  3509 | `	pGen->pIn++;` |
|    3897 |  3510 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3511 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3512 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3513 | `		 */` |
|       - |  3514 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3515 | `		char *zAlloc = 0;` |
|       - |  3516 | `		SyString sNum;` |
|      17 |  3517 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3518 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3519 | `			return SXERR_ABORT;` |
|       - |  3520 | `		}` |
|      17 |  3521 | `		if( rc == SXRET_OK ){` |
|      20 |  3522 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3523 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3524 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3525 | `				return SXERR_ABORT;` |
|       - |  3526 | `			}` |
|      14 |  3527 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3528 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3529 | `		}` |
|      17 |  3530 | `		if( iLevel < 2 ){` |
|       3 |  3531 | `			iLevel = 0;` |
|       1 |  3532 | `		}` |
|      17 |  3533 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3534 | `	}` |
|       - |  3535 | `	/* Point to the target loop */` |
|    3897 |  3536 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3897 |  3537 | `	if( pLoop == 0 ){` |
|       - |  3538 | `		/* Illegal continue */` |
|      12 |  3539 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      12 |  3540 | `		if( rc == SXERR_ABORT ){` |
|       - |  3541 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3542 | `			return SXERR_ABORT;` |
|       - |  3543 | `		}` |
|       7 |  3544 | `	}else{` |
|    3887 |  3545 | `		sxu32 nInstrIdx = 0;` |
|       - |  3546 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    3887 |  3547 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|       - |  3548 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|       - |  3549 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    3887 |  3550 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    3887 |  3551 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3552 | `			/* According to the PHP language reference manual` |
|       - |  3553 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3554 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3555 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3556 | `			 */` |
|       5 |  3557 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|       5 |  3558 | `			if( rc == SXRET_OK ){` |
|       5 |  3559 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3560 | `			}` |
|       3 |  3561 | `		}else{` |
|       - |  3562 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3883 |  3563 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3883 |  3564 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3565 | `				JumpFixup sJumpFix;` |
|       - |  3566 | `				/* Post-continue */` |
|      14 |  3567 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3568 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3569 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3570 | `			}` |
|       - |  3571 | `		}` |
|       - |  3572 | `	}` |
|    3897 |  3573 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3574 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3575 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3576 | `	}` |
|       - |  3577 | `	/* Statement successfully compiled */` |
|    3897 |  3578 | `	return SXRET_OK;` |
|    1951 |  3579 | `}` |
|       - |  3580 | `/*` |
|       - |  3581 | ` * Compile the 'break' statement.` |
|       - |  3582 | ` * According to the PHP language reference` |
|       - |  3583 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3584 | ` *  structure.` |
|       - |  3585 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3586 | ` *  enclosing structures are to be broken out of.` |
|       - |  3587 | ` */` |
|     124 |  3588 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3589 | `{` |
|       - |  3590 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3591 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3592 | `	sxi32 rc;` |
|     129 |  3593 | `	iLevel = 0;` |
|       - |  3594 | `	/* Jump the 'break' keyword */` |
|     129 |  3595 | `	pGen->pIn++;` |
|     129 |  3596 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3597 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3598 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3599 | `		 */` |
|       - |  3600 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      18 |  3601 | `		char *zAlloc = 0;` |
|       - |  3602 | `		SyString sNum;` |
|      18 |  3603 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      18 |  3604 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3605 | `			return SXERR_ABORT;` |
|       - |  3606 | `		}` |
|      18 |  3607 | `		if( rc == SXRET_OK ){` |
|      21 |  3608 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3609 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      15 |  3610 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3611 | `				return SXERR_ABORT;` |
|       - |  3612 | `			}` |
|      15 |  3613 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      15 |  3614 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3615 | `		}` |
|      18 |  3616 | `		if( iLevel < 2 ){` |
|       3 |  3617 | `			iLevel = 0;` |
|       1 |  3618 | `		}` |
|      18 |  3619 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3620 | `	}` |
|       - |  3621 | `	/* Extract the target loop */` |
|     129 |  3622 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     129 |  3623 | `	if( pLoop == 0 ){` |
|       - |  3624 | `		/* Illegal break */` |
|      19 |  3625 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      19 |  3626 | `		if( rc == SXERR_ABORT ){` |
|       - |  3627 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3628 | `			return SXERR_ABORT;` |
|       - |  3629 | `		}` |
|      11 |  3630 | `	}else{` |
|       - |  3631 | `		sxu32 nInstrIdx;` |
|       - |  3632 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     113 |  3633 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|       - |  3634 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     113 |  3635 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     113 |  3636 | `		if( rc == SXRET_OK ){` |
|       - |  3637 | `			/* Fix the jump later when the jump destination is resolved */` |
|     113 |  3638 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      54 |  3639 | `		}` |
|       - |  3640 | `	}` |
|     129 |  3641 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3642 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3643 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3644 | `	}` |
|       - |  3645 | `	/* Statement successfully compiled */` |
|     129 |  3646 | `	return SXRET_OK;` |
|      67 |  3647 | `}` |
|       - |  3648 | `/*` |
|       - |  3649 | ` * Compile or record a label.` |
|       - |  3650 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3651 | ` * Example` |
|       - |  3652 | ` *  goto LABEL;` |
|       - |  3653 | ` *   echo 'Foo';` |
|       - |  3654 | ` *  LABEL:` |
|       - |  3655 | ` *   echo 'Bar';` |
|       - |  3656 | ` */` |
|     112 |  3657 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  3658 | `{` |
|       - |  3659 | `	GenBlock *pBlock;` |
|       - |  3660 | `	Label sLabel;` |
|       - |  3661 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     117 |  3662 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     117 |  3663 | `	if( pBlock ){` |
|       - |  3664 | `		sxi32 rc;` |
|       8 |  3665 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3666 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       6 |  3667 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3668 | `			return SXERR_ABORT;` |
|       - |  3669 | `		}` |
|       4 |  3670 | `	}else{` |
|     113 |  3671 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3672 | `		char *zDup;` |
|       - |  3673 | `		/* Initialize label fields */` |
|     113 |  3674 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3675 | `		/* Duplicate label name */` |
|     113 |  3676 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     113 |  3677 | `		if( zDup == 0 ){` |
|     ! 0 |  3678 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3679 | `			return SXERR_ABORT;` |
|       - |  3680 | `		}` |
|     113 |  3681 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     113 |  3682 | `		sLabel.bRef  = FALSE;` |
|     113 |  3683 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     113 |  3684 | `		pBlock = pGen->pCurrent;` |
|     221 |  3685 | `		while( pBlock ){` |
|     133 |  3686 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      24 |  3687 | `				break;` |
|       - |  3688 | `			}` |
|       - |  3689 | `			/* Point to the upper block */` |
|     113 |  3690 | `			pBlock = pBlock->pParent;` |
|       5 |  3691 | `		}` |
|     113 |  3692 | `		if( pBlock ){` |
|      24 |  3693 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      14 |  3694 | `		}else{` |
|      93 |  3695 | `			sLabel.pFunc = 0;` |
|       - |  3696 | `		}` |
|       - |  3697 | `		/* Insert in label set */` |
|     113 |  3698 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3699 | `	}` |
|     117 |  3700 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     117 |  3701 | `	return SXRET_OK;` |
|      61 |  3702 | `}` |
|       - |  3703 | `/*` |
|       - |  3704 | ` * Compile the so hated 'goto' statement.` |
|       - |  3705 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3706 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3707 | ` * a compiler it has to do this.` |
|       - |  3708 | ` * According to the PHP language reference manual` |
|       - |  3709 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3710 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3711 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3712 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3713 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3714 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3715 | ` *   of a multi-level break` |
|       - |  3716 | ` */` |
|     152 |  3717 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  3718 | `{` |
|       - |  3719 | `	JumpFixup sJump;` |
|       - |  3720 | `	sxi32 rc;` |
|     157 |  3721 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     157 |  3722 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3723 | `		/* Missing label */` |
|     ! 0 |  3724 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3725 | `		if( rc == SXERR_ABORT ){` |
|       - |  3726 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3727 | `			return SXERR_ABORT;` |
|       - |  3728 | `		}` |
|     ! 0 |  3729 | `		return SXRET_OK;` |
|       - |  3730 | `	}` |
|     157 |  3731 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       6 |  3732 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       6 |  3733 | `		if( rc == SXERR_ABORT ){` |
|       - |  3734 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3735 | `			return SXERR_ABORT;` |
|       - |  3736 | `		}` |
|       4 |  3737 | `	}else{` |
|     153 |  3738 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3739 | `		GenBlock *pBlock;` |
|       - |  3740 | `		char *zDup;` |
|       - |  3741 | `		/* Prepare the jump destination */` |
|     153 |  3742 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     153 |  3743 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3744 | `		/* Duplicate label name */` |
|     153 |  3745 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     153 |  3746 | `		if( zDup == 0 ){` |
|     ! 0 |  3747 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3748 | `			return SXERR_ABORT;` |
|       - |  3749 | `		}` |
|     153 |  3750 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     153 |  3751 | `		pBlock = pGen->pCurrent;` |
|     315 |  3752 | `		while( pBlock ){` |
|     199 |  3753 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      36 |  3754 | `				break;` |
|       - |  3755 | `			}` |
|       - |  3756 | `			/* Point to the upper block */` |
|     167 |  3757 | `			pBlock = pBlock->pParent;` |
|       5 |  3758 | `		}` |
|     153 |  3759 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       8 |  3760 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       8 |  3761 | `			if( rc == SXERR_ABORT ){` |
|       - |  3762 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3763 | `				return SXERR_ABORT;` |
|       - |  3764 | `			}` |
|       3 |  3765 | `		}` |
|     153 |  3766 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      29 |  3767 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      16 |  3768 | `		}else{` |
|     127 |  3769 | `			sJump.pFunc = 0;` |
|       - |  3770 | `		}` |
|       - |  3771 | `		/* Emit the unconditional jump */` |
|     153 |  3772 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     153 |  3773 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3774 | `		}` |
|       - |  3775 | `	}` |
|     157 |  3776 | `	pGen->pIn++; /* Jump the label name */` |
|     157 |  3777 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3778 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3779 | `	}` |
|       - |  3780 | `	/* Statement successfully compiled */` |
|     157 |  3781 | `	return SXRET_OK;` |
|      81 |  3782 | `}` |
|       - |  3783 | `/*` |
|       - |  3784 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3785 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3786 | ` * failure.` |
|       - |  3787 | ` */` |
|      20 |  3788 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       2 |  3789 | `{` |
|       - |  3790 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3791 | `	sxu32 nRawObj;` |
|      10 |  3792 | `	sxu32 nObjIdx;` |
|       - |  3793 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3794 | `	 * a PHP block.` |
|       - |  3795 | `	 */` |
|      10 |  3796 | `Consume:` |
|      22 |  3797 | `	nRawObj = nObjIdx = 0;` |
|      22 |  3798 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3799 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3800 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3801 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3802 | `			return SXERR_ABORT;` |
|       - |  3803 | `		}` |
|       - |  3804 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3805 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3806 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3807 | `		++nRawObj;` |
|     ! 0 |  3808 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3809 | `	}` |
|      22 |  3810 | `	if( nRawObj > 0 ){` |
|       - |  3811 | `		/* Emit the consume instruction */` |
|     ! 0 |  3812 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3813 | `	}` |
|      22 |  3814 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3815 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3816 | `		/* Reset the token set */` |
|     ! 0 |  3817 | `		SySetReset(pTokenSet);` |
|       - |  3818 | `		/* Tokenize input */` |
|     ! 0 |  3819 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3820 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3821 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3822 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3823 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3824 | `		/* Advance the stream cursor */` |
|     ! 0 |  3825 | `		pGen->pRawIn++;` |
|       - |  3826 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3827 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3828 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3829 | `			sxi32 rc;` |
|       - |  3830 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3831 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3832 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3833 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3834 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3835 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3836 | `				return SXERR_ABORT;` |
|     ! 0 |  3837 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3838 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3839 | `			}` |
|     ! 0 |  3840 | `			goto Consume;` |
|       - |  3841 | `		}` |
|     ! 0 |  3842 | `	}else{` |
|       - |  3843 | `		/* No more chunks to process */` |
|      22 |  3844 | `		pGen->pIn = pGen->pEnd;` |
|      22 |  3845 | `		return SXERR_EOF;` |
|       - |  3846 | `	}` |
|     ! 0 |  3847 | `	return SXRET_OK;` |
|      12 |  3848 | `}` |
|       - |  3849 | `/*` |
|       - |  3850 | ` * Compile a PHP block.` |
|       - |  3851 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3852 | ` * optionally delimited by braces {}.` |
|       - |  3853 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3854 | ` * and this function takes care of generating the appropriate error` |
|       - |  3855 | ` * message.` |
|       - |  3856 | ` */` |
|  470642 |  3857 | `static sxi32 PH7_CompileBlock(` |
|       - |  3858 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3859 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3860 | `	)` |
|       5 |  3861 | `{` |
|       - |  3862 | `	sxi32 rc;` |
|       - |  3863 | `	sxu32 nLine;` |
|  470647 |  3864 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  469187 |  3865 | `		nLine = pGen->pIn->nLine;` |
|  469187 |  3866 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  469187 |  3867 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3868 | `			return SXERR_ABORT;` |
|       - |  3869 | `		}` |
|  469187 |  3870 | `		pGen->pIn++;` |
|       - |  3871 | `		/* Compile until we hit the closing braces '}' */` |
|  641165 |  3872 | `		for(;;){` |
| 1282335 |  3873 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      22 |  3874 | `				rc = GenStateNextChunk(&(*pGen));` |
|      22 |  3875 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3876 | `			 	   return SXERR_ABORT;` |
|       - |  3877 | `				}` |
|      22 |  3878 | `				if( rc == SXERR_EOF ){` |
|       - |  3879 | `					/* No more token to process. Missing closing braces */` |
|      22 |  3880 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      22 |  3881 | `					break;` |
|       - |  3882 | `				}` |
|     ! 0 |  3883 | `			}` |
| 1282315 |  3884 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3885 | `				/* Closing braces found,break immediately*/` |
|  469167 |  3886 | `				pGen->pIn++;` |
|  469167 |  3887 | `				break;` |
|       - |  3888 | `			}` |
|       - |  3889 | `			/* Compile a single statement */` |
|  813153 |  3890 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  813153 |  3891 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3892 | `				return SXERR_ABORT;` |
|       - |  3893 | `			}` |
|       5 |  3894 | `		}` |
|  469187 |  3895 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  236056 |  3896 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3897 | `		pGen->pIn++;` |
|     ! 0 |  3898 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3899 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3900 | `			return SXERR_ABORT;` |
|       - |  3901 | `		}` |
|       - |  3902 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3903 | `		for(;;){` |
|     ! 0 |  3904 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3905 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3906 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3907 | `			 	   return SXERR_ABORT;` |
|       - |  3908 | `				}` |
|     ! 0 |  3909 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3910 | `					/* No more token to process */` |
|     ! 0 |  3911 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3912 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3913 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3914 | `					}` |
|     ! 0 |  3915 | `					break;` |
|       - |  3916 | `				}` |
|     ! 0 |  3917 | `			}` |
|     ! 0 |  3918 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3919 | `				sxi32 nKwrd;` |
|       - |  3920 | `				/* Keyword found */` |
|     ! 0 |  3921 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3922 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3923 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3924 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3925 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3926 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3927 | `						}` |
|     ! 0 |  3928 | `						break;` |
|       - |  3929 | `				}` |
|     ! 0 |  3930 | `			}` |
|       - |  3931 | `			/* Compile a single statement */` |
|     ! 0 |  3932 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3933 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3934 | `				return SXERR_ABORT;` |
|       - |  3935 | `			}` |
|     ! 0 |  3936 | `		}` |
|     ! 0 |  3937 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3938 | `	}else{` |
|       - |  3939 | `		/* Compile a single statement */` |
|    1465 |  3940 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1465 |  3941 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3942 | `			return SXERR_ABORT;` |
|       - |  3943 | `		}` |
|       - |  3944 | `	}` |
|       - |  3945 | `	/* Jump trailing semi-colons ';' */` |
|  470647 |  3946 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3947 | `		pGen->pIn++;` |
|     ! 0 |  3948 | `	}` |
|  470647 |  3949 | `	return SXRET_OK;` |
|  235326 |  3950 | `}` |
|       - |  3951 | `/*` |
|       - |  3952 | ` * Compile the gentle 'while' statement.` |
|       - |  3953 | ` * According to the PHP language reference` |
|       - |  3954 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3955 | ` *  The basic form of a while statement is:` |
|       - |  3956 | ` *  while (expr)` |
|       - |  3957 | ` *   statement` |
|       - |  3958 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3959 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3960 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3961 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3962 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3963 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3964 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3965 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3966 | ` *  while (expr):` |
|       - |  3967 | ` *    statement` |
|       - |  3968 | ` *   endwhile;` |
|       - |  3969 | ` */` |
|   15508 |  3970 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3971 | `{` |
|   15513 |  3972 | `	GenBlock *pWhileBlock = 0;` |
|   15513 |  3973 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3974 | `	sxu32 nFalseJump;` |
|       - |  3975 | `	sxu32 nLine;` |
|       - |  3976 | `	sxi32 rc;` |
|   15513 |  3977 | `	nLine = pGen->pIn->nLine;` |
|       - |  3978 | `	/* Jump the 'while' keyword */` |
|   15513 |  3979 | `	pGen->pIn++;` |
|   15513 |  3980 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3981 | `		/* Syntax error */` |
|     ! 0 |  3982 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3983 | `		if( rc == SXERR_ABORT ){` |
|       - |  3984 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3985 | `			return SXERR_ABORT;` |
|       - |  3986 | `		}` |
|     ! 0 |  3987 | `		goto Synchronize;` |
|       - |  3988 | `	}` |
|       - |  3989 | `	/* Jump the left parenthesis '(' */` |
|   15513 |  3990 | `	pGen->pIn++;` |
|       - |  3991 | `	/* Create the loop block */` |
|   15513 |  3992 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   15513 |  3993 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3994 | `		return SXERR_ABORT;` |
|       - |  3995 | `	}` |
|       - |  3996 | `	/* Delimit the condition */` |
|   15513 |  3997 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   15513 |  3998 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3999 | `		/* Empty expression */` |
|       3 |  4000 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  4001 | `		if( rc == SXERR_ABORT ){` |
|       - |  4002 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4003 | `			return SXERR_ABORT;` |
|       - |  4004 | `		}` |
|       1 |  4005 | `	}` |
|       - |  4006 | `	/* Swap token streams */` |
|   15513 |  4007 | `	pTmp = pGen->pEnd;` |
|   15513 |  4008 | `	pGen->pEnd = pEnd;` |
|       - |  4009 | `	/* Compile the expression */` |
|   15513 |  4010 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   15513 |  4011 | `	if( rc == SXERR_ABORT ){` |
|       - |  4012 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4013 | `		return SXERR_ABORT;` |
|       - |  4014 | `	}` |
|       - |  4015 | `	/* Update token stream */` |
|   15513 |  4016 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  4017 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4018 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4019 | `			return SXERR_ABORT;` |
|       - |  4020 | `		}` |
|     ! 0 |  4021 | `		pGen->pIn++;` |
|     ! 0 |  4022 | `	}` |
|       - |  4023 | `	/* Synchronize pointers */` |
|   15513 |  4024 | `	pGen->pIn  = &pEnd[1];` |
|   15513 |  4025 | `	pGen->pEnd = pTmp;` |
|       - |  4026 | `	/* Emit the false jump */` |
|   15513 |  4027 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4028 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   15513 |  4029 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  4030 | `	/* Compile the loop body */` |
|   15513 |  4031 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   15513 |  4032 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4033 | `		return SXERR_ABORT;` |
|       - |  4034 | `	}` |
|       - |  4035 | `	/* Emit the unconditional jump to the start of the loop */` |
|   15513 |  4036 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  4037 | `	/* Fix all jumps now the destination is resolved */` |
|   15513 |  4038 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4039 | `	/* Release the loop block */` |
|   15513 |  4040 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4041 | `	/* Statement successfully compiled */` |
|   15513 |  4042 | `	return SXRET_OK;` |
|     ! 0 |  4043 | `Synchronize:` |
|       - |  4044 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4045 | `	 * compiling this erroneous block.` |
|       - |  4046 | `	 */` |
|     ! 0 |  4047 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4048 | `		pGen->pIn++;` |
|     ! 0 |  4049 | `	}` |
|     ! 0 |  4050 | `	return SXRET_OK;` |
|    7759 |  4051 | `}` |
|       - |  4052 | `/*` |
|       - |  4053 | ` * Compile the ugly do..while() statement.` |
|       - |  4054 | ` * According to the PHP language reference` |
|       - |  4055 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  4056 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  4057 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  4058 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  4059 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  4060 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  4061 | ` *  would end immediately).` |
|       - |  4062 | ` *  There is just one syntax for do-while loops:` |
|       - |  4063 | ` *  <?php` |
|       - |  4064 | ` *  $i = 0;` |
|       - |  4065 | ` *  do {` |
|       - |  4066 | ` *   echo $i;` |
|       - |  4067 | ` *  } while ($i > 0);` |
|       - |  4068 | ` * ?>` |
|       - |  4069 | ` */` |
|       2 |  4070 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  4071 | `{` |
|       3 |  4072 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  4073 | `	GenBlock *pDoBlock = 0;` |
|       - |  4074 | `	sxu32 nLine;` |
|       - |  4075 | `	sxi32 rc;` |
|       3 |  4076 | `	nLine = pGen->pIn->nLine;` |
|       - |  4077 | `	/* Jump the 'do' keyword */` |
|       3 |  4078 | `	pGen->pIn++;` |
|       - |  4079 | `	/* Create the loop block */` |
|       3 |  4080 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  4081 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4082 | `		return SXERR_ABORT;` |
|       - |  4083 | `	}` |
|       - |  4084 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  4085 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  4086 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  4087 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4088 | `		return SXERR_ABORT;` |
|       - |  4089 | `	}` |
|       3 |  4090 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4091 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  4092 | `	}` |
|       3 |  4093 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  4094 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  4095 | `			/* Missing 'while' statement */` |
|       3 |  4096 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  4097 | `			if( rc == SXERR_ABORT ){` |
|       - |  4098 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4099 | `				return SXERR_ABORT;` |
|       - |  4100 | `			}` |
|       3 |  4101 | `			goto Synchronize;` |
|       - |  4102 | `	}` |
|       - |  4103 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  4104 | `	pGen->pIn++;` |
|     ! 0 |  4105 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4106 | `		/* Syntax error */` |
|     ! 0 |  4107 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  4108 | `		if( rc == SXERR_ABORT ){` |
|       - |  4109 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4110 | `			return SXERR_ABORT;` |
|       - |  4111 | `		}` |
|     ! 0 |  4112 | `		goto Synchronize;` |
|       - |  4113 | `	}` |
|       - |  4114 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  4115 | `	pGen->pIn++;` |
|       - |  4116 | `	/* Delimit the condition */` |
|     ! 0 |  4117 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  4118 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4119 | `		/* Empty expression */` |
|     ! 0 |  4120 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  4121 | `		if( rc == SXERR_ABORT ){` |
|       - |  4122 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4123 | `			return SXERR_ABORT;` |
|       - |  4124 | `		}` |
|     ! 0 |  4125 | `		goto Synchronize;` |
|       - |  4126 | `	}` |
|       - |  4127 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  4128 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  4129 | `		JumpFixup *aPost;` |
|       - |  4130 | `		VmInstr *pInstr;` |
|       - |  4131 | `		sxu32 nJumpDest;` |
|       - |  4132 | `		sxu32 n;` |
|     ! 0 |  4133 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  4134 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  4135 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  4136 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  4137 | `			if( pInstr ){` |
|       - |  4138 | `				/* Fix */` |
|     ! 0 |  4139 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  4140 | `			}` |
|     ! 0 |  4141 | `		}` |
|     ! 0 |  4142 | `	}` |
|       - |  4143 | `	/* Swap token streams */` |
|     ! 0 |  4144 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  4145 | `	pGen->pEnd = pEnd;` |
|       - |  4146 | `	/* Compile the expression */` |
|     ! 0 |  4147 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4148 | `	if( rc == SXERR_ABORT ){` |
|       - |  4149 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4150 | `		return SXERR_ABORT;` |
|       - |  4151 | `	}` |
|       - |  4152 | `	/* Update token stream */` |
|     ! 0 |  4153 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  4154 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4155 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4156 | `			return SXERR_ABORT;` |
|       - |  4157 | `		}` |
|     ! 0 |  4158 | `		pGen->pIn++;` |
|     ! 0 |  4159 | `	}` |
|     ! 0 |  4160 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  4161 | `	pGen->pEnd = pTmp;` |
|       - |  4162 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  4163 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  4164 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  4165 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4166 | `	/* Release the loop block */` |
|     ! 0 |  4167 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4168 | `	/* Statement successfully compiled */` |
|     ! 0 |  4169 | `	return SXRET_OK;` |
|       1 |  4170 | `Synchronize:` |
|       - |  4171 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4172 | `	 * compiling this erroneous block.` |
|       - |  4173 | `	 */` |
|       3 |  4174 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4175 | `		pGen->pIn++;` |
|     ! 0 |  4176 | `	}` |
|       3 |  4177 | `	return SXRET_OK;` |
|       2 |  4178 | `}` |
|       - |  4179 | `/*` |
|       - |  4180 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  4181 | ` * According to the PHP language reference` |
|       - |  4182 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  4183 | ` *  The syntax of a for loop is:` |
|       - |  4184 | ` *  for (expr1; expr2; expr3)` |
|       - |  4185 | ` *   statement` |
|       - |  4186 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  4187 | ` *  the beginning of the loop.` |
|       - |  4188 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  4189 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  4190 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  4191 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  4192 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  4193 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  4194 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  4195 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  4196 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  4197 | ` *  of using the for truth expression.` |
|       - |  4198 | ` */` |
|   15508 |  4199 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  4200 | `{` |
|   15513 |  4201 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   15513 |  4202 | `	GenBlock *pForBlock = 0;` |
|       - |  4203 | `	sxu32 nFalseJump;` |
|       - |  4204 | `	sxu32 nLine;` |
|       - |  4205 | `	sxi32 rc;` |
|   15513 |  4206 | `	nLine = pGen->pIn->nLine;` |
|       - |  4207 | `	/* Jump the 'for' keyword */` |
|   15513 |  4208 | `	pGen->pIn++;` |
|   15513 |  4209 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4210 | `		/* Syntax error */` |
|     ! 0 |  4211 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  4212 | `		if( rc == SXERR_ABORT ){` |
|       - |  4213 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4214 | `			return SXERR_ABORT;` |
|       - |  4215 | `		}` |
|     ! 0 |  4216 | `		return SXRET_OK;` |
|       - |  4217 | `	}` |
|       - |  4218 | `	/* Jump the left parenthesis '(' */` |
|   15513 |  4219 | `	pGen->pIn++;` |
|       - |  4220 | `	/* Delimit the init-expr;condition;post-expr */` |
|   15513 |  4221 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   15513 |  4222 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4223 | `		/* Empty expression */` |
|     ! 0 |  4224 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  4225 | `		if( rc == SXERR_ABORT ){` |
|       - |  4226 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4227 | `			return SXERR_ABORT;` |
|       - |  4228 | `		}` |
|       - |  4229 | `		/* Synchronize */` |
|     ! 0 |  4230 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4231 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4232 | `			pGen->pIn++;` |
|     ! 0 |  4233 | `		}` |
|     ! 0 |  4234 | `		return SXRET_OK;` |
|       - |  4235 | `	}` |
|       - |  4236 | `	/* Swap token streams */` |
|   15513 |  4237 | `	pTmp = pGen->pEnd;` |
|   15513 |  4238 | `	pGen->pEnd = pEnd;` |
|       - |  4239 | `	/* Compile initialization expressions if available */` |
|   15513 |  4240 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4241 | `	/* Pop operand lvalues */` |
|   15513 |  4242 | `	if( rc == SXERR_ABORT ){` |
|       - |  4243 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4244 | `		return SXERR_ABORT;` |
|   15513 |  4245 | `	}else if( rc != SXERR_EMPTY ){` |
|   15511 |  4246 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7753 |  4247 | `	}` |
|   15513 |  4248 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4249 | `		/* Syntax error */` |
|     ! 0 |  4250 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4251 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  4252 | `		if( rc == SXERR_ABORT ){` |
|       - |  4253 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4254 | `			return SXERR_ABORT;` |
|       - |  4255 | `		}` |
|     ! 0 |  4256 | `		return SXRET_OK;` |
|       - |  4257 | `	}` |
|       - |  4258 | `	/* Jump the trailing ';' */` |
|   15513 |  4259 | `	pGen->pIn++;` |
|       - |  4260 | `	/* Create the loop block */` |
|   15513 |  4261 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   15513 |  4262 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4263 | `		return SXERR_ABORT;` |
|       - |  4264 | `	}` |
|       - |  4265 | `	/* Deffer continue jumps */` |
|   15513 |  4266 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  4267 | `	/* Compile the condition */` |
|   15513 |  4268 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   15513 |  4269 | `	if( rc == SXERR_ABORT ){` |
|       - |  4270 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4271 | `		return SXERR_ABORT;` |
|   15513 |  4272 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  4273 | `		/* Emit the false jump */` |
|   15511 |  4274 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4275 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   15511 |  4276 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    7753 |  4277 | `	}` |
|   15513 |  4278 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4279 | `		/* Syntax error */` |
|       6 |  4280 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4281 | `			"for: Expected ';' after conditionals expressions");` |
|       6 |  4282 | `		if( rc == SXERR_ABORT ){` |
|       - |  4283 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4284 | `			return SXERR_ABORT;` |
|       - |  4285 | `		}` |
|       6 |  4286 | `		return SXRET_OK;` |
|       - |  4287 | `	}` |
|       - |  4288 | `	/* Jump the trailing ';' */` |
|   15509 |  4289 | `	pGen->pIn++;` |
|       - |  4290 | `	/* Save the post condition stream */` |
|   15509 |  4291 | `	pPostStart = pGen->pIn;` |
|       - |  4292 | `	/* Compile the loop body */` |
|   15509 |  4293 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   15509 |  4294 | `	pGen->pEnd = pTmp;` |
|   15509 |  4295 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   15509 |  4296 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4297 | `		return SXERR_ABORT;` |
|       - |  4298 | `	}` |
|       - |  4299 | `	/* Fix post-continue jumps */` |
|   15509 |  4300 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  4301 | `		JumpFixup *aPost;` |
|       - |  4302 | `		VmInstr *pInstr;` |
|       - |  4303 | `		sxu32 nJumpDest;` |
|       - |  4304 | `		sxu32 n;` |
|      14 |  4305 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  4306 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  4307 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  4308 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  4309 | `			if( pInstr ){` |
|       - |  4310 | `				/* Fix jump */` |
|      14 |  4311 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  4312 | `			}` |
|       8 |  4313 | `		}` |
|       6 |  4314 | `	}` |
|       - |  4315 | `	/* compile the post-expressions if available */` |
|   15509 |  4316 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4317 | `		pPostStart++;` |
|     ! 0 |  4318 | `	}` |
|   15509 |  4319 | `	if( pPostStart < pEnd ){` |
|       - |  4320 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   15509 |  4321 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   15509 |  4322 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   15509 |  4323 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4324 | `			/* Syntax error */` |
|     ! 0 |  4325 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4326 | `			if( rc == SXERR_ABORT ){` |
|       - |  4327 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4328 | `				return SXERR_ABORT;` |
|       - |  4329 | `			}` |
|     ! 0 |  4330 | `			return SXRET_OK;` |
|       - |  4331 | `		}` |
|   15509 |  4332 | `		RE_SWAP_DELIMITER(pGen);` |
|   15509 |  4333 | `		if( rc == SXERR_ABORT ){` |
|       - |  4334 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4335 | `			return SXERR_ABORT;` |
|   15509 |  4336 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4337 | `			/* Pop operand lvalue */` |
|   15509 |  4338 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7752 |  4339 | `		}` |
|    7752 |  4340 | `	}` |
|       - |  4341 | `	/* Emit the unconditional jump to the start of the loop */` |
|   15509 |  4342 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4343 | `	/* Fix all jumps now the destination is resolved */` |
|   15509 |  4344 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4345 | `	/* Release the loop block */` |
|   15509 |  4346 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4347 | `	/* Statement successfully compiled */` |
|   15509 |  4348 | `	return SXRET_OK;` |
|    7759 |  4349 | `}` |
|       - |  4350 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4351 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4352 | ` * are allowed.` |
|       - |  4353 | ` */` |
|    8328 |  4354 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4355 | `{` |
|    8333 |  4356 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    8333 |  4357 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4358 | `		/* Unexpected expression */` |
|     ! 0 |  4359 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4360 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4361 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4362 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4363 | `		}` |
|     ! 0 |  4364 | `	}` |
|    8333 |  4365 | `	return rc;` |
|       5 |  4366 | `}` |
|       - |  4367 | `/*` |
|       - |  4368 | ` * Compile the 'foreach' statement.` |
|       - |  4369 | ` * According to the PHP language reference` |
|       - |  4370 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4371 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4372 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4373 | ` *  is a minor but useful extension of the first:` |
|       - |  4374 | ` *  foreach (array_expression as $value)` |
|       - |  4375 | ` *    statement` |
|       - |  4376 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4377 | ` *   statement` |
|       - |  4378 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4379 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4380 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4381 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4382 | ` *  to the variable $key on each loop.` |
|       - |  4383 | ` *  Note:` |
|       - |  4384 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4385 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4386 | ` *  Note:` |
|       - |  4387 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4388 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4389 | ` *  or after the foreach without resetting it.` |
|       - |  4390 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4391 | ` *  of copying the value.` |
|       - |  4392 | ` */` |
|    4288 |  4393 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4394 | `{` |
|    4293 |  4395 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    4293 |  4396 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    4293 |  4397 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4398 | `	ph7_foreach_info *pInfo;` |
|       - |  4399 | `	sxu32 nFalseJump;` |
|       - |  4400 | `	VmInstr *pInstr;` |
|       - |  4401 | `	sxu32 nLine;` |
|       - |  4402 | `	sxi32 rc;` |
|    4293 |  4403 | `	nLine = pGen->pIn->nLine;` |
|       - |  4404 | `	/* Jump the 'foreach' keyword */` |
|    4293 |  4405 | `	pGen->pIn++;` |
|    4293 |  4406 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4407 | `		/* Syntax error */` |
|     ! 0 |  4408 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4409 | `		if( rc == SXERR_ABORT ){` |
|       - |  4410 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4411 | `			return SXERR_ABORT;` |
|       - |  4412 | `		}` |
|     ! 0 |  4413 | `		goto Synchronize;` |
|       - |  4414 | `	}` |
|       - |  4415 | `	/* Jump the left parenthesis '(' */` |
|    4293 |  4416 | `	pGen->pIn++;` |
|       - |  4417 | `	/* Create the loop block */` |
|    4293 |  4418 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    4293 |  4419 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4420 | `		return SXERR_ABORT;` |
|       - |  4421 | `	}` |
|       - |  4422 | `	/* Delimit the expression */` |
|    4293 |  4423 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    4293 |  4424 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4425 | `		/* Empty expression */` |
|     ! 0 |  4426 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4427 | `		if( rc == SXERR_ABORT ){` |
|       - |  4428 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4429 | `			return SXERR_ABORT;` |
|       - |  4430 | `		}` |
|       - |  4431 | `		/* Synchronize */` |
|     ! 0 |  4432 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4433 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4434 | `			pGen->pIn++;` |
|     ! 0 |  4435 | `		}` |
|     ! 0 |  4436 | `		return SXRET_OK;` |
|       - |  4437 | `	}` |
|       - |  4438 | `	/* Compile the array expression */` |
|    4293 |  4439 | `	pCur = pGen->pIn;` |
|   29427 |  4440 | `	while( pCur < pEnd ){` |
|   29427 |  4441 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    4307 |  4442 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    4307 |  4443 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4444 | `				/* Break with the first 'as' found */` |
|    4293 |  4445 | `				break;` |
|       - |  4446 | `			}` |
|       7 |  4447 | `		}` |
|       - |  4448 | `		/* Advance the stream cursor */` |
|   25139 |  4449 | `		pCur++;` |
|       5 |  4450 | `	}` |
|    4293 |  4451 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4452 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4453 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4454 | `		if( rc == SXERR_ABORT ){` |
|       - |  4455 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4456 | `			return SXERR_ABORT;` |
|       - |  4457 | `		}` |
|     ! 0 |  4458 | `		goto Synchronize;` |
|       - |  4459 | `	}` |
|       - |  4460 | `	/* Swap token streams */` |
|    4293 |  4461 | `	pTmp = pGen->pEnd;` |
|    4293 |  4462 | `	pGen->pEnd = pCur;` |
|    4293 |  4463 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    4293 |  4464 | `	if( rc == SXERR_ABORT ){` |
|       - |  4465 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4466 | `		return SXERR_ABORT;` |
|       - |  4467 | `	}` |
|       - |  4468 | `	/* Update token stream */` |
|    4293 |  4469 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4470 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4471 | `		if( rc == SXERR_ABORT ){` |
|       - |  4472 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4473 | `			return SXERR_ABORT;` |
|       - |  4474 | `		}` |
|     ! 0 |  4475 | `		pGen->pIn++;` |
|     ! 0 |  4476 | `	}` |
|    4293 |  4477 | `	pCur++; /* Jump the 'as' keyword */` |
|    4293 |  4478 | `	pGen->pIn = pCur;` |
|    4293 |  4479 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4480 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4481 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4482 | `			return SXERR_ABORT;` |
|       - |  4483 | `		}` |
|     ! 0 |  4484 | `	}` |
|       - |  4485 | `	/* Create the foreach context */` |
|    4293 |  4486 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    4293 |  4487 | `	if( pInfo == 0 ){` |
|     ! 0 |  4488 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4489 | `		return SXERR_ABORT;` |
|       - |  4490 | `	}` |
|       - |  4491 | `	/* Zero the structure */` |
|    4293 |  4492 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4493 | `	/* Initialize structure fields */` |
|    4293 |  4494 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4495 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - |  4496 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - |  4497 | `	 * '=>'. */` |
|    4293 |  4498 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    4293 |  4499 | `	if( pCur < pEnd ){` |
|       - |  4500 | `		/* Compile the expression holding the key name */` |
|    4065 |  4501 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4502 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4503 | `			if( rc == SXERR_ABORT ){` |
|       - |  4504 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4505 | `				return SXERR_ABORT;` |
|       - |  4506 | `			}` |
|     ! 0 |  4507 | `		}else{` |
|    4065 |  4508 | `			pGen->pEnd = pCur;` |
|    4065 |  4509 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    4065 |  4510 | `			if( rc == SXERR_ABORT ){` |
|       - |  4511 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4512 | `				return SXERR_ABORT;` |
|       - |  4513 | `			}` |
|    4065 |  4514 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    4065 |  4515 | `			if( pInstr->p3 ){` |
|       - |  4516 | `				/* Record key name */` |
|    4065 |  4517 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    2030 |  4518 | `			}` |
|    4065 |  4519 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4520 | `		}` |
|    4065 |  4521 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    2030 |  4522 | `	}` |
|    4293 |  4523 | `	pGen->pEnd = pEnd;` |
|    4293 |  4524 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4525 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4526 | `		if( rc == SXERR_ABORT ){` |
|       - |  4527 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4528 | `			return SXERR_ABORT;` |
|       - |  4529 | `		}` |
|     ! 0 |  4530 | `		goto Synchronize;` |
|       - |  4531 | `	}` |
|    4293 |  4532 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4533 | `		pGen->pIn++;` |
|       - |  4534 | `		/* Pass by reference  */` |
|      11 |  4535 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4536 | `	}` |
|       - |  4537 | `	/* Check if the value target is list() */` |
|    4293 |  4538 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4539 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4540 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4541 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4542 | `		 */` |
|       - |  4543 | `		static int iForeachListCnt = 0;` |
|       - |  4544 | `		char zTmp[128];` |
|       - |  4545 | `		sxu32 nLen;` |
|       - |  4546 | `		char *zDup;` |
|      10 |  4547 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4548 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4549 | `		if( zDup == 0 ){` |
|     ! 0 |  4550 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4551 | `			return SXERR_ABORT;` |
|       - |  4552 | `		}` |
|      10 |  4553 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4554 | `		/* Save list() token boundaries */` |
|      10 |  4555 | `		pListStart = pGen->pIn;` |
|       - |  4556 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4557 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4558 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4559 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4560 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4561 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4562 | `				return SXERR_ABORT;` |
|       - |  4563 | `			}` |
|       3 |  4564 | `			goto Synchronize;` |
|       - |  4565 | `		}` |
|       7 |  4566 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4567 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4568 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4569 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4570 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4571 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4572 | `				return SXERR_ABORT;` |
|       - |  4573 | `			}` |
|     ! 0 |  4574 | `			goto Synchronize;` |
|       - |  4575 | `		}` |
|       7 |  4576 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4577 | `		pListEnd = pGen->pIn;` |
|       7 |  4578 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    4288 |  4579 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4580 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4581 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4582 | `		 */` |
|       - |  4583 | `		static int iForeachShortListCnt = 0;` |
|       - |  4584 | `		char zTmp[128];` |
|       - |  4585 | `		sxu32 nLen;` |
|       - |  4586 | `		char *zDup;` |
|      13 |  4587 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|      13 |  4588 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      13 |  4589 | `		if( zDup == 0 ){` |
|     ! 0 |  4590 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4591 | `			return SXERR_ABORT;` |
|       - |  4592 | `		}` |
|      13 |  4593 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4594 | `		/* Save [...] token boundaries */` |
|      13 |  4595 | `		pListStart = pGen->pIn;` |
|       - |  4596 | `		/* Advance past [...] */` |
|      13 |  4597 | `		pGen->pIn++; /* Jump '[' */` |
|      13 |  4598 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|      13 |  4599 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4600 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4601 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4602 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4603 | `				return SXERR_ABORT;` |
|       - |  4604 | `			}` |
|     ! 0 |  4605 | `			goto Synchronize;` |
|       - |  4606 | `		}` |
|      13 |  4607 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|      13 |  4608 | `		pListEnd = pGen->pIn;` |
|      13 |  4609 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       7 |  4610 | `	}else{` |
|       - |  4611 | `		/* Compile the expression holding the value name */` |
|    4273 |  4612 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    4273 |  4613 | `		if( rc == SXERR_ABORT ){` |
|       - |  4614 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4615 | `			return SXERR_ABORT;` |
|       - |  4616 | `		}` |
|    4273 |  4617 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    4273 |  4618 | `		if( pInstr->p3 ){` |
|       - |  4619 | `			/* Record value name */` |
|    4273 |  4620 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    2134 |  4621 | `		}` |
|       - |  4622 | `	}` |
|       - |  4623 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    4291 |  4624 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4625 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    4291 |  4626 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4627 | `	/* Record the first instruction to execute */` |
|    4291 |  4628 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4629 | `	/* Emit the FOREACH_STEP instruction */` |
|    4291 |  4630 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4631 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    4291 |  4632 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4633 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    4291 |  4634 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4635 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4636 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4637 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4638 | `		 */` |
|      19 |  4639 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4640 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4641 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4642 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4643 | `		 */` |
|      19 |  4644 | `		pSavedIn = pGen->pIn;` |
|      19 |  4645 | `		pSavedEnd = pGen->pEnd;` |
|      19 |  4646 | `		pGen->pIn = pListStart;` |
|      19 |  4647 | `		pGen->pEnd = pListEnd;` |
|      19 |  4648 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|      13 |  4649 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  4650 | `		}else{` |
|       7 |  4651 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4652 | `		}` |
|      19 |  4653 | `		pGen->pIn = pSavedIn;` |
|      19 |  4654 | `		pGen->pEnd = pSavedEnd;` |
|      19 |  4655 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4656 | `			return SXERR_ABORT;` |
|       - |  4657 | `		}` |
|       - |  4658 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      19 |  4659 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       9 |  4660 | `	}` |
|       - |  4661 | `	/* Compile the loop body */` |
|    4291 |  4662 | `	pGen->pIn = &pEnd[1];` |
|    4291 |  4663 | `	pGen->pEnd = pTmp;` |
|    4291 |  4664 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    4291 |  4665 | `	if( rc == SXERR_ABORT ){` |
|       - |  4666 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4667 | `		return SXERR_ABORT;` |
|       - |  4668 | `	}` |
|       - |  4669 | `	/* Emit the unconditional jump to the start of the loop */` |
|    4291 |  4670 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4671 | `	/* Fix all jumps now the destination is resolved */` |
|    4291 |  4672 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4673 | `	/* Release the loop block */` |
|    4291 |  4674 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4675 | `	/* Statement successfully compiled */` |
|    4291 |  4676 | `	return SXRET_OK;` |
|       1 |  4677 | `Synchronize:` |
|       - |  4678 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4679 | `	 * compiling this erroneous block.` |
|       - |  4680 | `	 */` |
|       3 |  4681 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4682 | `		pGen->pIn++;` |
|     ! 0 |  4683 | `	}` |
|       3 |  4684 | `	return SXRET_OK;` |
|    2149 |  4685 | `}` |
|       - |  4686 | `/*` |
|       - |  4687 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4688 | ` * According to the PHP language reference` |
|       - |  4689 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4690 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4691 | ` *  that is similar to that of C:` |
|       - |  4692 | ` *  if (expr)` |
|       - |  4693 | ` *   statement` |
|       - |  4694 | ` *  else construct:` |
|       - |  4695 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4696 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4697 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4698 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4699 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4700 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4701 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4702 | ` *  elseif` |
|       - |  4703 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4704 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4705 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4706 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4707 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4708 | ` *   <?php` |
|       - |  4709 | ` *    if ($a > $b) {` |
|       - |  4710 | ` *     echo "a is bigger than b";` |
|       - |  4711 | ` *    } elseif ($a == $b) {` |
|       - |  4712 | ` *     echo "a is equal to b";` |
|       - |  4713 | ` *    } else {` |
|       - |  4714 | ` *     echo "a is smaller than b";` |
|       - |  4715 | ` *    }` |
|       - |  4716 | ` *    ?>` |
|       - |  4717 | ` */` |
|  160212 |  4718 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4719 | `{` |
|  160217 |  4720 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  160217 |  4721 | `	GenBlock *pCondBlock = 0;` |
|       - |  4722 | `	sxu32 nJumpIdx;` |
|       - |  4723 | `	sxu32 nKeyID;` |
|       - |  4724 | `	sxi32 rc;` |
|       - |  4725 | `	/* Jump the 'if' keyword */` |
|  160217 |  4726 | `	pGen->pIn++;` |
|  160217 |  4727 | `	pToken = pGen->pIn;` |
|       - |  4728 | `	/* Create the conditional block */` |
|  160217 |  4729 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  160217 |  4730 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4731 | `		return SXERR_ABORT;` |
|       - |  4732 | `	}` |
|       - |  4733 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   87853 |  4734 | `	for(;;){` |
|  175711 |  4735 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4736 | `			/* Syntax error */` |
|     ! 0 |  4737 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4738 | `				pToken--;` |
|     ! 0 |  4739 | `			}` |
|     ! 0 |  4740 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4741 | `			if( rc == SXERR_ABORT ){` |
|       - |  4742 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4743 | `				return SXERR_ABORT;` |
|       - |  4744 | `			}` |
|     ! 0 |  4745 | `			goto Synchronize;` |
|       - |  4746 | `		}` |
|       - |  4747 | `		/* Jump the left parenthesis '(' */` |
|  175711 |  4748 | `		pToken++;` |
|       - |  4749 | `		/* Delimit the condition */` |
|  175711 |  4750 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  175711 |  4751 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4752 | `			/* Syntax error */` |
|     ! 0 |  4753 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4754 | `				pToken--;` |
|     ! 0 |  4755 | `			}` |
|     ! 0 |  4756 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4757 | `			if( rc == SXERR_ABORT ){` |
|       - |  4758 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4759 | `				return SXERR_ABORT;` |
|       - |  4760 | `			}` |
|     ! 0 |  4761 | `			goto Synchronize;` |
|       - |  4762 | `		}` |
|       - |  4763 | `		/* Swap token streams */` |
|  175711 |  4764 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4765 | `		/* Compile the condition */` |
|  175711 |  4766 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4767 | `		/* Update token stream */` |
|  175711 |  4768 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4769 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4770 | `			pGen->pIn++;` |
|     ! 0 |  4771 | `		}` |
|  175711 |  4772 | `		pGen->pIn  = &pEnd[1];` |
|  175711 |  4773 | `		pGen->pEnd = pTmp;` |
|  175711 |  4774 | `		if( rc == SXERR_ABORT ){` |
|       - |  4775 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4776 | `			return SXERR_ABORT;` |
|       - |  4777 | `		}` |
|       - |  4778 | `		/* Emit the false jump */` |
|  175711 |  4779 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4780 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  175711 |  4781 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4782 | `		/* Compile the body */` |
|  175711 |  4783 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  175711 |  4784 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4785 | `			return SXERR_ABORT;` |
|       - |  4786 | `		}` |
|  175711 |  4787 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   48947 |  4788 | `			break;` |
|       - |  4789 | `		}` |
|       - |  4790 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   77827 |  4791 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   77827 |  4792 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   50319 |  4793 | `			break;` |
|       - |  4794 | `		}` |
|       - |  4795 | `		/* Emit the unconditional jump */` |
|   27513 |  4796 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4797 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   27513 |  4798 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   27513 |  4799 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   19707 |  4800 | `			pToken = &pGen->pIn[1];` |
|   19707 |  4801 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    7728 |  4802 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    6012 |  4803 | `					break;` |
|       - |  4804 | `			}` |
|    7693 |  4805 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3844 |  4806 | `		}` |
|   15499 |  4807 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4808 | `		/* Synchronize cursors */` |
|   15499 |  4809 | `		pToken = pGen->pIn;` |
|       - |  4810 | `		/* Fix the false jump */` |
|   15499 |  4811 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4812 | `	} /* For(;;) */` |
|       - |  4813 | `	/* Fix the false jump */` |
|  160217 |  4814 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  160217 |  4815 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   62328 |  4816 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4817 | `			/* Compile the else block */` |
|   12019 |  4818 | `			pGen->pIn++;` |
|   12019 |  4819 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   12019 |  4820 | `			if( rc == SXERR_ABORT ){` |
|       - |  4821 |  |
|     ! 0 |  4822 | `				return SXERR_ABORT;` |
|       - |  4823 | `			}` |
|    6007 |  4824 | `	}` |
|  160217 |  4825 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4826 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  160217 |  4827 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4828 | `	/* Release the conditional block */` |
|  160217 |  4829 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4830 | `	/* Statement successfully compiled */` |
|  160217 |  4831 | `	return SXRET_OK;` |
|     ! 0 |  4832 | `Synchronize:` |
|       - |  4833 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4834 | `	 */` |
|     ! 0 |  4835 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4836 | `		pGen->pIn++;` |
|     ! 0 |  4837 | `	}` |
|     ! 0 |  4838 | `	return SXRET_OK;` |
|   80111 |  4839 | `}` |
|       - |  4840 | `/*` |
|       - |  4841 | ` * Compile the global construct.` |
|       - |  4842 | ` * According to the PHP language reference` |
|       - |  4843 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4844 | ` *  to be used in that function.` |
|       - |  4845 | ` *  Example #1 Using global` |
|       - |  4846 | ` *  <?php` |
|       - |  4847 | ` *   $a = 1;` |
|       - |  4848 | ` *   $b = 2;` |
|       - |  4849 | ` *   function Sum()` |
|       - |  4850 | ` *   {` |
|       - |  4851 | ` *    global $a, $b;` |
|       - |  4852 | ` *    $b = $a + $b;` |
|       - |  4853 | ` *   }` |
|       - |  4854 | ` *   Sum();` |
|       - |  4855 | ` *   echo $b;` |
|       - |  4856 | ` *  ?>` |
|       - |  4857 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4858 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4859 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4860 | ` */` |
|      36 |  4861 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 |  4862 | `{` |
|      41 |  4863 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4864 | `	sxi32 nExpr;` |
|       - |  4865 | `	sxi32 rc;` |
|       - |  4866 | `	/* Jump the 'global' keyword */` |
|      41 |  4867 | `	pGen->pIn++;` |
|      41 |  4868 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4869 | `		/* Nothing to process */` |
|     ! 0 |  4870 | `		return SXRET_OK;` |
|       - |  4871 | `	}` |
|      41 |  4872 | `	pTmp = pGen->pEnd;` |
|      41 |  4873 | `	nExpr = 0;` |
|      87 |  4874 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      51 |  4875 | `		if( pGen->pIn < pNext ){` |
|      51 |  4876 | `			pGen->pEnd = pNext;` |
|      51 |  4877 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4878 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4879 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4880 | `					return SXERR_ABORT;` |
|       - |  4881 | `				}` |
|     ! 0 |  4882 | `			}else{` |
|      51 |  4883 | `				pGen->pIn++;` |
|      51 |  4884 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4885 | `					/* Emit a warning */` |
|     ! 0 |  4886 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4887 | `				}else{` |
|      51 |  4888 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 |  4889 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4890 | `						return SXERR_ABORT;` |
|      51 |  4891 | `					}else if(rc != SXERR_EMPTY ){` |
|      51 |  4892 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      51 |  4893 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4894 | `							/* Variable name, not a constant */` |
|      51 |  4895 | `							pLast->iP1 = 0;` |
|      23 |  4896 | `						}` |
|      51 |  4897 | `						nExpr++;` |
|      23 |  4898 | `					}` |
|       - |  4899 | `				}` |
|       - |  4900 | `			}` |
|      23 |  4901 | `		}` |
|       - |  4902 | `		/* Next expression in the stream */` |
|      51 |  4903 | `		pGen->pIn = pNext;` |
|       - |  4904 | `		/* Jump trailing commas */` |
|      61 |  4905 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  4906 | `			pGen->pIn++;` |
|       5 |  4907 | `		}` |
|       5 |  4908 | `	}` |
|       - |  4909 | `	/* Restore token stream */` |
|      41 |  4910 | `	pGen->pEnd = pTmp;` |
|      41 |  4911 | `	if( nExpr > 0 ){` |
|       - |  4912 | `		/* Emit the uplink instruction */` |
|      41 |  4913 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  4914 | `	}` |
|      41 |  4915 | `	return SXRET_OK;` |
|      23 |  4916 | `}` |
|       - |  4917 | `/*` |
|       - |  4918 | ` * Compile the return statement.` |
|       - |  4919 | ` * According to the PHP language reference` |
|       - |  4920 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4921 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4922 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4923 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4924 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4925 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4926 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4927 | ` *  from within the main script file, then script execution end.` |
|       - |  4928 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4929 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4930 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4931 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4932 | ` */` |
|  255328 |  4933 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4934 | `{` |
|  255333 |  4935 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4936 | `	sxi32 rc;` |
|  255333 |  4937 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  255333 |  4938 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|       - |  4939 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|       - |  4940 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|       - |  4941 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|       - |  4942 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|       - |  4943 | `	 * normally below so token processing stays consistent. */` |
|  657523 |  4944 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  402195 |  4945 | `		pFuncBlock = pFuncBlock->pParent;` |
|       5 |  4946 | `	}` |
|  255328 |  4947 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  255301 |  4948 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|       3 |  4949 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  4950 | `			"A never-returning function must not return");` |
|       3 |  4951 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4952 | `			return SXERR_ABORT;` |
|       - |  4953 | `		}` |
|       1 |  4954 | `	}` |
|       - |  4955 | `	/* Jump the 'return' keyword */` |
|  255333 |  4956 | `	pGen->pIn++;` |
|  255333 |  4957 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4958 | `		/* Compile the expression */` |
|  255303 |  4959 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  255303 |  4960 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4961 | `			return SXERR_ABORT;` |
|  255303 |  4962 | `		}else if(rc != SXERR_EMPTY ){` |
|  255303 |  4963 | `			nRet = 1;` |
|  127649 |  4964 | `		}` |
|  127649 |  4965 | `	}` |
|       - |  4966 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|       - |  4967 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|       - |  4968 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|       - |  4969 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  255333 |  4970 | `	if( pGen->bInGenerator ){` |
|      29 |  4971 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      29 |  4972 | `		return SXRET_OK;` |
|       - |  4973 | `	}` |
|       - |  4974 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4975 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4976 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4977 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4978 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  255307 |  4979 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  255307 |  4980 | `	return SXRET_OK;` |
|  127669 |  4981 | `}` |
|       - |  4982 | `/*` |
|       - |  4983 | ` * Compile a yield expression.` |
|       - |  4984 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4985 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4986 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4987 | ` */` |
|     328 |  4988 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4989 | `{` |
|       - |  4990 | `	SyToken *pTmp, *pSplit;` |
|     333 |  4991 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     333 |  4992 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4993 | `	sxi32 rc;` |
|     164 |  4994 | `	(void)iCompileFlag;` |
|       - |  4995 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     333 |  4996 | `	pGen->pIn++;` |
|       - |  4997 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4998 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - |  4999 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - |  5000 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - |  5001 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     358 |  5002 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     194 |  5003 | `		&& pGen->pIn->sData.nByte == 4` |
|      66 |  5004 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      64 |  5005 | `		pGen->pIn++; /* Skip 'from' */` |
|      64 |  5006 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      64 |  5007 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5008 | `			return SXERR_ABORT;` |
|       - |  5009 | `		}` |
|      64 |  5010 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  5011 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 |  5012 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - |  5013 | `				"Missing expression after 'yield from'");` |
|     ! 0 |  5014 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5015 | `				return SXERR_ABORT;` |
|       - |  5016 | `			}` |
|     ! 0 |  5017 | `		}` |
|      64 |  5018 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      64 |  5019 | `		return SXRET_OK;` |
|       - |  5020 | `	}` |
|     273 |  5021 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5022 | `		/* Bare yield — no value */` |
|       3 |  5023 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       3 |  5024 | `		return SXRET_OK;` |
|       - |  5025 | `	}` |
|       - |  5026 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     271 |  5027 | `	pSplit = 0;` |
|       - |  5028 | `	{` |
|     271 |  5029 | `		SyToken *pCur = pGen->pIn;` |
|     271 |  5030 | `		sxi32 nNest = 0;` |
|     569 |  5031 | `		while( pCur < pGen->pEnd ){` |
|     317 |  5032 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  5033 | `				nNest++;` |
|     316 |  5034 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  5035 | `				nNest--;` |
|     314 |  5036 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  5037 | `				pSplit = pCur;` |
|      16 |  5038 | `				break;` |
|       - |  5039 | `			}` |
|     303 |  5040 | `			pCur++;` |
|       5 |  5041 | `		}` |
|       - |  5042 | `	}` |
|     271 |  5043 | `	pTmp = pGen->pEnd;` |
|     271 |  5044 | `	if( pSplit ){` |
|       - |  5045 | `		/* yield $key => $value */` |
|      16 |  5046 | `		pGen->pEnd = pSplit;` |
|      16 |  5047 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  5048 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  5049 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  5050 | `		pGen->pEnd = pTmp;` |
|      16 |  5051 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  5052 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  5053 | `		iP1 = 1;` |
|      16 |  5054 | `		iP2 = 1;` |
|       9 |  5055 | `	}else{` |
|       - |  5056 | `		/* yield $value */` |
|     257 |  5057 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     257 |  5058 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     257 |  5059 | `		if( rc != SXERR_EMPTY ){` |
|     257 |  5060 | `			iP1 = 1;` |
|     126 |  5061 | `		}` |
|       - |  5062 | `	}` |
|     271 |  5063 | `	pGen->pEnd = pTmp;` |
|     271 |  5064 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     271 |  5065 | `	return SXRET_OK;` |
|     169 |  5066 | `}` |
|       - |  5067 | `/*` |
|       - |  5068 | ` * Compile the die/exit language construct.` |
|       - |  5069 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  5070 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  5071 | ` */` |
|     122 |  5072 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  5073 | `{` |
|     127 |  5074 | `	sxi32 nExpr = 0;` |
|       - |  5075 | `	sxi32 rc;` |
|       - |  5076 | `	/* Jump the die/exit keyword */` |
|     127 |  5077 | `	pGen->pIn++;` |
|     127 |  5078 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  5079 | `		/* Compile the expression */` |
|     127 |  5080 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     127 |  5081 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5082 | `			return SXERR_ABORT;` |
|     127 |  5083 | `		}else if(rc != SXERR_EMPTY ){` |
|     127 |  5084 | `			nExpr = 1;` |
|      61 |  5085 | `		}` |
|      61 |  5086 | `	}` |
|       - |  5087 | `	/* Emit the HALT instruction */` |
|     127 |  5088 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     127 |  5089 | `	return SXRET_OK;` |
|      66 |  5090 | `}` |
|       - |  5091 | `/*` |
|       - |  5092 | ` * Compile the 'echo' language construct.` |
|       - |  5093 | ` */` |
|   14976 |  5094 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  5095 | `{` |
|   14981 |  5096 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  5097 | `	sxi32 rc;` |
|       - |  5098 | `	/* Jump the 'echo' keyword */` |
|   14981 |  5099 | `	pGen->pIn++;` |
|       - |  5100 | `	/* Compile arguments one after one */` |
|   14981 |  5101 | `	pTmp = pGen->pEnd;` |
|   34745 |  5102 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   19769 |  5103 | `		if( pGen->pIn < pNext ){` |
|   19769 |  5104 | `			pGen->pEnd = pNext;` |
|   19769 |  5105 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   19769 |  5106 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5107 | `				return SXERR_ABORT;` |
|   19769 |  5108 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  5109 | `				/* Emit the consume instruction */` |
|   19745 |  5110 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    9870 |  5111 | `			}` |
|    9882 |  5112 | `		}` |
|       - |  5113 | `		/* Jump trailing commas */` |
|   24557 |  5114 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    4793 |  5115 | `			pNext++;` |
|       5 |  5116 | `		}` |
|   19769 |  5117 | `		pGen->pIn = pNext;` |
|       5 |  5118 | `	}` |
|       - |  5119 | `	/* Restore token stream */` |
|   14981 |  5120 | `	pGen->pEnd = pTmp;` |
|   14981 |  5121 | `	return SXRET_OK;` |
|    7493 |  5122 | `}` |
|       - |  5123 | `/*` |
|       - |  5124 | ` * Compile the static statement.` |
|       - |  5125 | ` * According to the PHP language reference` |
|       - |  5126 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  5127 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  5128 | ` *  when program execution leaves this scope.` |
|       - |  5129 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  5130 | ` * Symisc eXtension.` |
|       - |  5131 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  5132 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  5133 | ` *  Example` |
|       - |  5134 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  5135 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  5136 | ` */` |
|       8 |  5137 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       3 |  5138 | `{` |
|       - |  5139 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  5140 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  5141 | `	GenBlock *pBlock;` |
|       - |  5142 | `	SyString *pName;` |
|       - |  5143 | `	char *zDup;` |
|       - |  5144 | `	sxu32 nLine;` |
|       - |  5145 | `	sxi32 rc;` |
|       - |  5146 | `	/* Jump the static keyword */` |
|      11 |  5147 | `	nLine = pGen->pIn->nLine;` |
|      11 |  5148 | `	pGen->pIn++;` |
|       - |  5149 | `	/* Extract the enclosing function if any */` |
|      11 |  5150 | `	pBlock = pGen->pCurrent;` |
|      19 |  5151 | `	while( pBlock ){` |
|      19 |  5152 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|      11 |  5153 | `			break;` |
|       - |  5154 | `		}` |
|       - |  5155 | `		/* Point to the upper block */` |
|      11 |  5156 | `		pBlock = pBlock->pParent;` |
|       3 |  5157 | `	}` |
|      11 |  5158 | `	if( pBlock == 0 ){` |
|       - |  5159 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  5160 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  5161 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  5162 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5163 | `				return SXERR_ABORT;` |
|       - |  5164 | `			}` |
|     ! 0 |  5165 | `			goto Synchronize;` |
|       - |  5166 | `		}` |
|       - |  5167 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  5168 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  5169 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5170 | `			return SXERR_ABORT;` |
|     ! 0 |  5171 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  5172 | `			/* Emit the POP instruction */` |
|     ! 0 |  5173 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  5174 | `		}` |
|     ! 0 |  5175 | `		return SXRET_OK;` |
|       - |  5176 | `	}` |
|      11 |  5177 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  5178 | `	/* Make sure we are dealing with a valid statement */` |
|      11 |  5179 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       6 |  5180 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  5181 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  5182 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5183 | `				return SXERR_ABORT;` |
|       - |  5184 | `			}` |
|       3 |  5185 | `			goto Synchronize;` |
|       - |  5186 | `	}` |
|       8 |  5187 | `	pGen->pIn++;` |
|       - |  5188 | `	/* Extract variable name */` |
|       8 |  5189 | `	pName = &pGen->pIn->sData;` |
|       8 |  5190 | `	pGen->pIn++; /* Jump the var name */` |
|       8 |  5191 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  5192 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  5193 | `		goto Synchronize;` |
|       - |  5194 | `	}` |
|       - |  5195 | `	/* Initialize the structure describing the static variable */` |
|       8 |  5196 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       8 |  5197 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  5198 | `	/* Duplicate variable name */` |
|       8 |  5199 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       8 |  5200 | `	if( zDup == 0 ){` |
|     ! 0 |  5201 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  5202 | `		return SXERR_ABORT;` |
|       - |  5203 | `	}` |
|       8 |  5204 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  5205 | `	/* Check if we have an expression to compile */` |
|       8 |  5206 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  5207 | `		SySet *pInstrContainer;` |
|       - |  5208 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  5209 | `		 * Static variable can take any complex expression including function` |
|       - |  5210 | `		 * call as their initialization value.` |
|       - |  5211 | `		 * Example:` |
|       - |  5212 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  5213 | `		 */` |
|       8 |  5214 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  5215 | `		/* Swap bytecode container */` |
|       8 |  5216 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       8 |  5217 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  5218 | `		/* Compile the expression */` |
|       8 |  5219 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5220 | `		/* Emit the done instruction */` |
|       8 |  5221 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  5222 | `		/* Restore default bytecode container */` |
|       8 |  5223 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       3 |  5224 | `	}` |
|       - |  5225 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       8 |  5226 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       8 |  5227 | `	return SXRET_OK;` |
|       1 |  5228 | `Synchronize:` |
|       - |  5229 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  5230 | `	 * statement.` |
|       - |  5231 | `	 */` |
|       5 |  5232 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  5233 | `		pGen->pIn++;` |
|       1 |  5234 | `	}` |
|       3 |  5235 | `	return SXRET_OK;` |
|       7 |  5236 | `}` |
|       - |  5237 | `/*` |
|       - |  5238 | ` * Compile the var statement.` |
|       - |  5239 | ` * Symisc Extension:` |
|       - |  5240 | ` *      var statement can be used outside of a class definition.` |
|       - |  5241 | ` */` |
|       4 |  5242 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  5243 | `{` |
|       - |  5244 | `	sxu32 nLine;` |
|       - |  5245 | `	sxi32 rc;` |
|       5 |  5246 | `	nLine = pGen->pIn->nLine;` |
|       - |  5247 | `	/* Jump the 'var' keyword */` |
|       5 |  5248 | `	pGen->pIn++;` |
|       5 |  5249 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  5250 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  5251 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  5252 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  5253 | `			pGen->pIn++;` |
|     ! 0 |  5254 | `		}` |
|     ! 0 |  5255 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5256 | `			return SXERR_ABORT;` |
|       - |  5257 | `		}` |
|     ! 0 |  5258 | `	}else{` |
|       - |  5259 | `		/* Compile the expression */` |
|       5 |  5260 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  5261 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5262 | `			return SXERR_ABORT;` |
|       5 |  5263 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  5264 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  5265 | `		}` |
|       - |  5266 | `	}` |
|       5 |  5267 | `	return SXRET_OK;` |
|       3 |  5268 | `}` |
|       - |  5269 | `/*` |
|       - |  5270 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  5271 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  5272 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  5273 | ` */` |
|       - |  5274 | `/*` |
|       - |  5275 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  5276 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  5277 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  5278 | ` * qualified name and updates the instruction's operand index.` |
|       - |  5279 | ` *` |
|       - |  5280 | ` * Resolution order:` |
|       - |  5281 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  5282 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  5283 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  5284 | ` *` |
|       - |  5285 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  5286 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  5287 | ` * Returns the (possibly new) literal index.` |
|       - |  5288 | ` */` |
|  495838 |  5289 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  5290 | `{` |
|       - |  5291 | `	ph7_value *pLit;` |
|       - |  5292 | `	const char *zLit;` |
|       - |  5293 | `	SyString sQualified;` |
|       - |  5294 | `	sxu32 nLit;` |
|       - |  5295 | `	sxu32 k;` |
|       - |  5296 | `	sxu32 nNewIdx;` |
|       - |  5297 | `	int hasNsSep;` |
|       - |  5298 | `	SyHashEntry *pImport;` |
|       - |  5299 | `	ph7_value *pNew;` |
|  495843 |  5300 | `	if( pFromImport ){` |
|  474487 |  5301 | `		*pFromImport = 0;` |
|  237241 |  5302 | `	}` |
|  495843 |  5303 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  495843 |  5304 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  5305 | `		return nOrigIdx;` |
|       - |  5306 | `	}` |
|  495843 |  5307 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  495843 |  5308 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  5309 | `	/* Skip if already qualified (contains backslash) */` |
|  495843 |  5310 | `	hasNsSep = 0;` |
| 5471109 |  5311 | `	for( k = 0; k < nLit; k++ ){` |
| 4975279 |  5312 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2487638 |  5313 | `	}` |
|  495843 |  5314 | `	if( hasNsSep ){` |
|      10 |  5315 | `		return nOrigIdx;` |
|       - |  5316 | `	}` |
|       - |  5317 | `	/* Check use imports first (works even outside namespaces) */` |
|  495835 |  5318 | `	SyBlobReset(&pGen->sWorker);` |
|  495835 |  5319 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  495835 |  5320 | `	if( pImport ){` |
|      41 |  5321 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  5322 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  5323 | `		if( pFromImport ){` |
|      18 |  5324 | `			*pFromImport = 1;` |
|       8 |  5325 | `		}` |
|      23 |  5326 | `	}else{` |
|  495799 |  5327 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  495709 |  5328 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  5329 | `		}` |
|       - |  5330 | `		/* Prepend current namespace */` |
|      95 |  5331 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  5332 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  5333 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  5334 | `	}` |
|       - |  5335 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  5336 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  5337 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  5338 | `		return nNewIdx; /* Already interned */` |
|       - |  5339 | `	}` |
|      79 |  5340 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  5341 | `	if( pNew == 0 ){` |
|     ! 0 |  5342 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  5343 | `	}` |
|      79 |  5344 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  5345 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  5346 | `	return nNewIdx;` |
|  247924 |  5347 | `}` |
|       - |  5348 | `/*` |
|       - |  5349 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  5350 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  5351 | ` */` |
|  104862 |  5352 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5353 | `{` |
|       - |  5354 | `	SyHashEntry *pImport;` |
|       - |  5355 | `	/* Check use imports first */` |
|  104867 |  5356 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|  104867 |  5357 | `	if( pImport ){` |
|      19 |  5358 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      19 |  5359 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      19 |  5360 | `		return;` |
|       - |  5361 | `	}` |
|       - |  5362 | `	/* Prepend current namespace if active */` |
|  104851 |  5363 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  5364 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  5365 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  5366 | `	}` |
|  104851 |  5367 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   52436 |  5368 | `}` |
|       - |  5369 | `/*` |
|       - |  5370 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5371 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5372 | ` * The caller must release pOut when done.` |
|       - |  5373 | ` */` |
|  155242 |  5374 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5375 | `{` |
|  155247 |  5376 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|    3907 |  5377 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|    3907 |  5378 | `		SyBlobAppend(pOut,"\\",1);` |
|    1951 |  5379 | `	}` |
|  155247 |  5380 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  155247 |  5381 | `}` |
|       - |  5382 | `/*` |
|       - |  5383 | ` * Compile a namespace statement` |
|       - |  5384 | ` * According to the PHP language reference manual` |
|       - |  5385 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5386 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5387 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5388 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5389 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5390 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5391 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5392 | ` *  programming world.` |
|       - |  5393 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5394 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5395 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5396 | ` *  classes/functions/constants.` |
|       - |  5397 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5398 | ` *  readability of source code.` |
|       - |  5399 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5400 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5401 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5402 | ` *       class MyClass {}` |
|       - |  5403 | ` *       function myfunction() {}` |
|       - |  5404 | ` *       const MYCONST = 1;` |
|       - |  5405 | ` *       $a = new MyClass;` |
|       - |  5406 | ` *       $c = new \my\name\MyClass;` |
|       - |  5407 | ` *       $a = strlen('hi');` |
|       - |  5408 | ` *       $d = namespace\MYCONST;` |
|       - |  5409 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5410 | ` *       echo constant($d);` |
|       - |  5411 | ` * NOTE` |
|       - |  5412 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5413 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5414 | ` */` |
|       - |  5415 | `/*` |
|       - |  5416 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5417 | ` */` |
|      14 |  5418 | `static const char * TokenTypeName(sxu32 nType)` |
|       3 |  5419 | `{` |
|      17 |  5420 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      11 |  5421 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      11 |  5422 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      11 |  5423 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      11 |  5424 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      11 |  5425 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5426 | `	return "token";` |
|      10 |  5427 | `}` |
|    3950 |  5428 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5429 | `{` |
|       - |  5430 | `	sxu32 nLine;` |
|       - |  5431 | `	sxi32 rc;` |
|    3955 |  5432 | `	nLine = pGen->pIn->nLine;` |
|    3955 |  5433 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5434 | `	/* Reset namespace and clear previous use imports */` |
|    3955 |  5435 | `	SyBlobReset(&pGen->sNamespace);` |
|    3955 |  5436 | `	SyHashRelease(&pGen->hUseImports);` |
|    3955 |  5437 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|    3955 |  5438 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|    3955 |  5439 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|    3955 |  5440 | `	SyHashRelease(&pGen->hUseConstImports);` |
|    3955 |  5441 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|    3955 |  5442 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5443 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5444 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5445 | `		return SXRET_OK;` |
|       - |  5446 | `	}` |
|    3955 |  5447 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5448 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5449 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5450 | `		return SXRET_OK;` |
|       - |  5451 | `	}` |
|    3955 |  5452 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5453 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5454 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5455 | `		return SXRET_OK;` |
|       - |  5456 | `	}` |
|       - |  5457 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|    7947 |  5458 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|    3997 |  5459 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5460 | `			/* Append backslash separator */` |
|      26 |  5461 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      26 |  5462 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5463 | `			}` |
|      15 |  5464 | `		}else{` |
|       - |  5465 | `			/* Append identifier */` |
|    3975 |  5466 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5467 | `		}` |
|    3997 |  5468 | `		pGen->pIn++;` |
|       5 |  5469 | `	}` |
|       - |  5470 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5471 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5472 | `	{` |
|    3955 |  5473 | `		char *zNsDup = 0;` |
|    3955 |  5474 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|    5927 |  5475 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    3948 |  5476 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|    1974 |  5477 | `		}` |
|    3955 |  5478 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5479 | `	}` |
|    3955 |  5480 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5481 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5482 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5483 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5484 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5485 | `			return SXERR_ABORT;` |
|       - |  5486 | `		}` |
|       2 |  5487 | `	}` |
|    3955 |  5488 | `	return SXRET_OK;` |
|    1980 |  5489 | `}` |
|       - |  5490 | `/*` |
|       - |  5491 | ` * Compile the 'use' statement` |
|       - |  5492 | ` * According to the PHP language reference manual` |
|       - |  5493 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5494 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5495 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5496 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5497 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5498 | ` *  a function or constant is not supported.` |
|       - |  5499 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5500 | ` * NOTE` |
|       - |  5501 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5502 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5503 | ` */` |
|      72 |  5504 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5505 | `{` |
|       - |  5506 | `	sxu32 nLine;` |
|       - |  5507 | `	sxi32 rc;` |
|       - |  5508 | `	SyBlob sPath;` |
|       - |  5509 | `	SyString sAlias;` |
|       - |  5510 | `	SyToken *pLast;` |
|       - |  5511 | `	char *zDup;` |
|       - |  5512 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5513 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5514 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      77 |  5515 | `	nLine = pGen->pIn->nLine;` |
|      77 |  5516 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5517 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      77 |  5518 | `	iUseType = 0;` |
|      77 |  5519 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5520 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5521 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5522 | `			iUseType = 1;` |
|      16 |  5523 | `			pGen->pIn++;` |
|      23 |  5524 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5525 | `			iUseType = 2;` |
|      16 |  5526 | `			pGen->pIn++;` |
|       7 |  5527 | `		}` |
|      14 |  5528 | `	}` |
|       - |  5529 | `	/* Select target hash tables based on import type */` |
|      77 |  5530 | `	switch( iUseType ){` |
|       7 |  5531 | `		case 1:` |
|      16 |  5532 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5533 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5534 | `			break;` |
|       7 |  5535 | `		case 2:` |
|      16 |  5536 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5537 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5538 | `			break;` |
|      22 |  5539 | `		default:` |
|      49 |  5540 | `			pGenHash = &pGen->hUseImports;` |
|      49 |  5541 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      44 |  5542 | `			break;` |
|       - |  5543 | `	}` |
|      77 |  5544 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5545 | `	/* Process one or more use declarations separated by commas */` |
|      37 |  5546 | `	for(;;){` |
|      79 |  5547 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5548 | `			break;` |
|       - |  5549 | `		}` |
|      79 |  5550 | `		SyBlobReset(&sPath);` |
|      79 |  5551 | `		pLast = 0;` |
|       - |  5552 | `		/* Collect the full namespace path */` |
|     269 |  5553 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     195 |  5554 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     135 |  5555 | `				pLast = pGen->pIn;` |
|     135 |  5556 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5557 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5558 | `				}` |
|     135 |  5559 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      65 |  5560 | `			}` |
|     195 |  5561 | `			pGen->pIn++;` |
|       5 |  5562 | `		}` |
|      79 |  5563 | `		if( pLast == 0 ){` |
|       - |  5564 | `			/* Empty path */` |
|       6 |  5565 | `			break;` |
|       - |  5566 | `		}` |
|       - |  5567 | `		/* Default alias is the last component of the path */` |
|      75 |  5568 | `		sAlias = pLast->sData;` |
|       - |  5569 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      70 |  5570 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      50 |  5571 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      24 |  5572 | `			pGen->pIn++; /* Jump 'as' */` |
|      24 |  5573 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      24 |  5574 | `				sAlias = pGen->pIn->sData;` |
|      24 |  5575 | `				pGen->pIn++;` |
|      10 |  5576 | `			}` |
|      10 |  5577 | `		}` |
|       - |  5578 | `		/* Check for duplicate import alias (per-type) */` |
|      75 |  5579 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5580 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5581 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5582 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5583 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5584 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5585 | `				return SXERR_ABORT;` |
|       - |  5586 | `			}` |
|       2 |  5587 | `		}` |
|       - |  5588 | `		/* Register the import: alias -> FQN.` |
|       - |  5589 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5590 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5591 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     110 |  5592 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      70 |  5593 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      75 |  5594 | `		if( zDup ){` |
|      75 |  5595 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      75 |  5596 | `			if( pVmHash ){` |
|       - |  5597 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5598 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      47 |  5599 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      47 |  5600 | `				if( zAliasDup ){` |
|      47 |  5601 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      21 |  5602 | `				}` |
|      21 |  5603 | `			}` |
|      75 |  5604 | `			if( iUseType == 2 ){` |
|       - |  5605 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5606 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5607 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5608 | `				if( zAliasDup ){` |
|       - |  5609 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5610 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5611 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5612 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5613 | `					if( azPair ){` |
|      16 |  5614 | `						azPair[0] = zAliasDup;` |
|      16 |  5615 | `						azPair[1] = zDup;` |
|      16 |  5616 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5617 | `					}` |
|       7 |  5618 | `				}` |
|       7 |  5619 | `			}` |
|      35 |  5620 | `		}` |
|       - |  5621 | `		/* Check for comma (multiple use declarations) */` |
|      75 |  5622 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5623 | `			pGen->pIn++;` |
|       2 |  5624 | `		}else{` |
|      39 |  5625 | `			break;` |
|       - |  5626 | `		}` |
|       1 |  5627 | `	}` |
|      77 |  5628 | `	SyBlobRelease(&sPath);` |
|      77 |  5629 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5630 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5631 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5632 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5633 | `			return SXERR_ABORT;` |
|       - |  5634 | `		}` |
|       1 |  5635 | `	}` |
|      77 |  5636 | `	return SXRET_OK;` |
|      41 |  5637 | `}` |
|       - |  5638 | `/*` |
|       - |  5639 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5640 | ` *` |
|       - |  5641 | ` * According to the PHP language reference manual.` |
|       - |  5642 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5643 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5644 | ` *  declare (directive)` |
|       - |  5645 | ` *   statement` |
|       - |  5646 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5647 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5648 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5649 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5650 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5651 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5652 | ` * <?php` |
|       - |  5653 | ` * // these are the same:` |
|       - |  5654 | ` * // you can use this:` |
|       - |  5655 | ` * declare(ticks=1) {` |
|       - |  5656 | ` *   // entire script here` |
|       - |  5657 | ` * }` |
|       - |  5658 | ` * // or you can use this:` |
|       - |  5659 | ` * declare(ticks=1);` |
|       - |  5660 | ` * // entire script here` |
|       - |  5661 | ` * ?>` |
|       - |  5662 | ` *` |
|       - |  5663 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5664 | ` */` |
|       - |  5665 | `/*` |
|       - |  5666 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5667 | ` */` |
|      72 |  5668 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5669 | `{` |
|     109 |  5670 | `	return SyStringLength(pName) == nWant` |
|      72 |  5671 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5672 | `}` |
|       - |  5673 |  |
|      42 |  5674 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5675 | `{` |
|      47 |  5676 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      47 |  5677 | `	SyToken *pBodyEnd = 0;` |
|       - |  5678 | `	SyToken *pBodyStart;` |
|       - |  5679 | `	SyToken *pCursor;` |
|       - |  5680 | `	int bHasStrictTypes;` |
|       - |  5681 | `	int bBlockForm;` |
|       - |  5682 | `	int bPlacementOk;` |
|       - |  5683 | `	sxi32 rc;` |
|      47 |  5684 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      47 |  5685 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 |  5686 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       6 |  5687 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5688 | `			return SXERR_ABORT;` |
|       - |  5689 | `		}` |
|       6 |  5690 | `		goto Synchro;` |
|       - |  5691 | `	}` |
|      43 |  5692 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      43 |  5693 | `	pBodyStart = pGen->pIn;` |
|       - |  5694 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      43 |  5695 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      43 |  5696 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5697 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5698 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5699 | `			return SXERR_ABORT;` |
|       - |  5700 | `		}` |
|     ! 0 |  5701 | `		return SXRET_OK;` |
|       - |  5702 | `	}` |
|       - |  5703 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5704 | `	 * now delimits the comma-separated directive list. */` |
|      43 |  5705 | `	pGen->pIn = &pBodyEnd[1];` |
|      43 |  5706 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5707 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5708 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5709 | `			return SXERR_ABORT;` |
|       - |  5710 | `		}` |
|     ! 0 |  5711 | `	}` |
|      43 |  5712 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      43 |  5713 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      43 |  5714 | `	bHasStrictTypes = 0;` |
|       - |  5715 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5716 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5717 | `	 * directive appears anywhere in the list, before validating values. */` |
|      43 |  5718 | `	pCursor = pBodyStart;` |
|      55 |  5719 | `	while( pCursor < pBodyEnd ){` |
|      51 |  5720 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      43 |  5721 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      39 |  5722 | `				bHasStrictTypes = 1;` |
|      39 |  5723 | `				break;` |
|       - |  5724 | `			}` |
|       2 |  5725 | `		}` |
|      14 |  5726 | `		pCursor++;` |
|       2 |  5727 | `	}` |
|      43 |  5728 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5729 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5730 | `			"strict_types declaration must not use block mode");` |
|       3 |  5731 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5732 | `		return SXRET_OK;` |
|       - |  5733 | `	}` |
|      41 |  5734 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5735 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5736 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5737 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5738 | `		return SXRET_OK;` |
|       - |  5739 | `	}` |
|       - |  5740 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      37 |  5741 | `	pCursor = pBodyStart;` |
|      69 |  5742 | `	while( pCursor < pBodyEnd ){` |
|       - |  5743 | `		SyToken *pNameTok;` |
|       - |  5744 | `		SyToken *pEqTok;` |
|       - |  5745 | `		SyToken *pValTok;` |
|       - |  5746 | `		SyString *pDirName;` |
|       - |  5747 | `		int bIsStrict;` |
|       - |  5748 | `		int iStrictValue;` |
|      39 |  5749 | `		pNameTok = pCursor;` |
|      39 |  5750 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5751 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5752 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5753 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5754 | `			return SXRET_OK;` |
|       - |  5755 | `		}` |
|      39 |  5756 | `		pEqTok = pNameTok + 1;` |
|      39 |  5757 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5758 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5759 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5760 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5761 | `			return SXRET_OK;` |
|       - |  5762 | `		}` |
|      39 |  5763 | `		pValTok = pEqTok + 1;` |
|      39 |  5764 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5765 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5766 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5767 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5768 | `			return SXRET_OK;` |
|       - |  5769 | `		}` |
|      39 |  5770 | `		pDirName = &pNameTok->sData;` |
|      39 |  5771 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      39 |  5772 | `		if( bIsStrict ){` |
|       - |  5773 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5774 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      35 |  5775 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5776 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5777 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5778 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5779 | `				return SXRET_OK;` |
|       - |  5780 | `			}` |
|      35 |  5781 | `			iStrictValue = -1;` |
|      35 |  5782 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      35 |  5783 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      35 |  5784 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      35 |  5785 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      33 |  5786 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      15 |  5787 | `			}` |
|      35 |  5788 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5789 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5790 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5791 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5792 | `				return SXRET_OK;` |
|       - |  5793 | `			}` |
|      32 |  5794 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      18 |  5795 | `		}else{` |
|       - |  5796 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5797 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5798 | `			 * behavior don't regress. */` |
|       8 |  5799 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5800 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5801 | `				ph7_lib_version()` |
|       - |  5802 | `				);` |
|       - |  5803 | `		}` |
|      36 |  5804 | `		pCursor = pValTok + 1;` |
|       - |  5805 | `		/* Consume separating comma (or end). */` |
|      36 |  5806 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5807 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5808 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5809 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5810 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5811 | `				return SXRET_OK;` |
|       - |  5812 | `			}` |
|       3 |  5813 | `			pCursor++;` |
|       1 |  5814 | `		}` |
|       4 |  5815 | `	}` |
|       - |  5816 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5817 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5818 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      34 |  5819 | `	return SXRET_OK;` |
|       2 |  5820 | `Synchro:` |
|       - |  5821 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 |  5822 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 |  5823 | `		pGen->pIn++;` |
|       2 |  5824 | `	}` |
|       6 |  5825 | `	return SXRET_OK;` |
|      26 |  5826 | `}` |
|       - |  5827 | `/*` |
|       - |  5828 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5829 | ` * as follows:` |
|       - |  5830 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5831 | ` * {` |
|       - |  5832 | ` *   return "Making a cup of $type.\n";` |
|       - |  5833 | ` * }` |
|       - |  5834 | ` * Symisc eXtension.` |
|       - |  5835 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5836 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5837 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5838 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5839 | ` *      {` |
|       - |  5840 | ` *       var_dump($a);` |
|       - |  5841 | ` *      }` |
|       - |  5842 | ` *     //call test without args` |
|       - |  5843 | ` *      test();` |
|       - |  5844 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5845 | ` *      Example:` |
|       - |  5846 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5847 | ` * 3 -) Function overloading!!` |
|       - |  5848 | ` *      Example:` |
|       - |  5849 | ` *      function foo($a) {` |
|       - |  5850 | ` *   	  return $a.PHP_EOL;` |
|       - |  5851 | ` *	    }` |
|       - |  5852 | ` *	    function foo($a, $b) {` |
|       - |  5853 | ` *   	  return $a + $b;` |
|       - |  5854 | ` *	    }` |
|       - |  5855 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5856 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5857 | ` *      // Same arg` |
|       - |  5858 | ` *	   function foo(string $a)` |
|       - |  5859 | ` *	   {` |
|       - |  5860 | ` *	     echo "a is a string\n";` |
|       - |  5861 | ` *	     var_dump($a);` |
|       - |  5862 | ` *	   }` |
|       - |  5863 | ` *	  function foo(int $a)` |
|       - |  5864 | ` *	  {` |
|       - |  5865 | ` *	    echo "a is integer\n";` |
|       - |  5866 | ` *	    var_dump($a);` |
|       - |  5867 | ` *	  }` |
|       - |  5868 | ` *	  function foo(array $a)` |
|       - |  5869 | ` *	  {` |
|       - |  5870 | ` * 	    echo "a is an array\n";` |
|       - |  5871 | ` * 	    var_dump($a);` |
|       - |  5872 | ` *	  }` |
|       - |  5873 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5874 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5875 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5876 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5877 | ` * introduced by the PH7 engine.` |
|       - |  5878 | ` */` |
|   80798 |  5879 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5880 | `{` |
|       - |  5881 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5882 | `	SySet *pInstrContainer;` |
|       - |  5883 | `	sxi32 rc;` |
|       - |  5884 | `	/* Swap token stream */` |
|   80803 |  5885 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   80803 |  5886 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   80803 |  5887 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5888 | `	/* Compile the expression holding the argument value */` |
|   80803 |  5889 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5890 | `	/* Emit the done instruction */` |
|   80803 |  5891 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   80803 |  5892 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   80803 |  5893 | `	RE_SWAP_DELIMITER(pGen);` |
|   80803 |  5894 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5895 | `		return SXERR_ABORT;` |
|       - |  5896 | `	}` |
|   80803 |  5897 | `	return SXRET_OK;` |
|   40404 |  5898 | `}` |
|       - |  5899 | `/*` |
|       - |  5900 | ` * Collect function arguments one after one.` |
|       - |  5901 | ` * According to the PHP language reference manual.` |
|       - |  5902 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5903 | ` * list of expressions.` |
|       - |  5904 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5905 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5906 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5907 | ` * for more information.` |
|       - |  5908 | ` * Example #1 Passing arrays to functions` |
|       - |  5909 | ` * <?php` |
|       - |  5910 | ` * function takes_array($input)` |
|       - |  5911 | ` * {` |
|       - |  5912 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5913 | ` * }` |
|       - |  5914 | ` * ?>` |
|       - |  5915 | ` * Making arguments be passed by reference` |
|       - |  5916 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5917 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5918 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5919 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5920 | ` * to the argument name in the function definition:` |
|       - |  5921 | ` * Example #2 Passing function parameters by reference` |
|       - |  5922 | ` * <?php` |
|       - |  5923 | ` * function add_some_extra(&$string)` |
|       - |  5924 | ` * {` |
|       - |  5925 | ` *   $string .= 'and something extra.';` |
|       - |  5926 | ` * }` |
|       - |  5927 | ` * $str = 'This is a string, ';` |
|       - |  5928 | ` * add_some_extra($str);` |
|       - |  5929 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5930 | ` * ?>` |
|       - |  5931 | ` *` |
|       - |  5932 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5933 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5934 | ` * on these extension.` |
|       - |  5935 | ` */` |
|  113014 |  5936 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5937 | `{` |
|       - |  5938 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5939 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5940 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5941 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5942 | `	sxi32 rc;` |
|       - |  5943 |  |
|  113019 |  5944 | `	pIn = pGen->pIn;` |
|  113019 |  5945 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5946 | `	/* Process arguments one after one */` |
|  146077 |  5947 | `	for(;;){` |
|  292159 |  5948 | `		if( pIn >= pEnd ){` |
|       - |  5949 | `			/* No more arguments to process */` |
|  113003 |  5950 | `			break;` |
|       - |  5951 | `		}` |
|  179161 |  5952 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  179161 |  5953 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  179161 |  5954 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  179161 |  5955 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5956 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  5957 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  5958 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  5959 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  5960 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  5961 | `		{` |
|  179161 |  5962 | `			int bReadonly = 0, bVisSeen = 0;` |
|  179161 |  5963 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  179161 |  5964 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  5965 | `				bReadonly = 1;` |
|       3 |  5966 | `				pIn++;` |
|       1 |  5967 | `			}` |
|  179161 |  5968 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   69501 |  5969 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   69501 |  5970 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      79 |  5971 | `					bVisSeen = 1;` |
|      79 |  5972 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|     105 |  5973 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      34 |  5974 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      79 |  5975 | `					pIn++;` |
|      79 |  5976 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      18 |  5977 | `						bReadonly = 1;` |
|      18 |  5978 | `						pIn++;` |
|       7 |  5979 | `					}` |
|      37 |  5980 | `				}` |
|   34748 |  5981 | `			}` |
|  179161 |  5982 | `			if( bVisSeen \|\| bReadonly ){` |
|      81 |  5983 | `				if( !bCtorCtx ){` |
|       6 |  5984 | `					if( bAbstractCtx ){` |
|       3 |  5985 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5986 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5987 | `					}else{` |
|       3 |  5988 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5989 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5990 | `					}` |
|       6 |  5991 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5992 | `						return SXERR_ABORT;` |
|       - |  5993 | `					}` |
|       6 |  5994 | `					return SXERR_SYNTAX;` |
|       - |  5995 | `				}` |
|      77 |  5996 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      77 |  5997 | `				sArg.iPromoteVis = iVis;` |
|      77 |  5998 | `				if( bReadonly ){` |
|      20 |  5999 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       8 |  6000 | `				}` |
|      36 |  6001 | `			}` |
|       - |  6002 | `		}` |
|       - |  6003 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  223573 |  6004 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  135933 |  6005 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   90783 |  6006 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   84971 |  6007 | `			sxu32 nLineLocal = pIn->nLine;` |
|   84971 |  6008 | `			sxi32 iTFlags = 0;` |
|   84971 |  6009 | `			pGen->pIn = pIn;` |
|   84971 |  6010 | `			rc = GenStateParseUnionTypeDecl(` |
|   42483 |  6011 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   42483 |  6012 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  6013 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  6014 | `				/* bAllowVoid */ 0,` |
|   42483 |  6015 | `						nLineLocal);` |
|   84971 |  6016 | `			pIn = pGen->pIn;` |
|   84971 |  6017 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6018 | `				return SXERR_ABORT;` |
|   84971 |  6019 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  6020 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  6021 | `				return SXERR_SYNTAX;` |
|   84969 |  6022 | `			}else if( rc == SXERR_SYNTAX ){` |
|      11 |  6023 | `				if( pIn < pEnd ){` |
|      15 |  6024 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  6025 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       4 |  6026 | `						&pIn->sData);` |
|       7 |  6027 | `				}else{` |
|     ! 0 |  6028 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  6029 | `						"syntax error, unexpected end of file");` |
|       - |  6030 | `				}` |
|      11 |  6031 | `				return SXERR_SYNTAX;` |
|       - |  6032 | `			}` |
|   84961 |  6033 | `			sArg.iFlags \|= iTFlags;` |
|   42478 |  6034 | `		}` |
|  179147 |  6035 | `		if( pIn >= pEnd ){` |
|     ! 0 |  6036 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  6037 | `			return rc;` |
|       - |  6038 | `		}` |
|  179147 |  6039 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  6040 | `			/* Pass by reference,record that */` |
|    3881 |  6041 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3881 |  6042 | `			pIn++;` |
|    1938 |  6043 | `		}` |
|  179147 |  6044 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  6045 | `			/* Variadic parameter: ...$args */` |
|    3901 |  6046 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    3901 |  6047 | `			pIn++;` |
|    1948 |  6048 | `		}` |
|  179147 |  6049 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6050 | `			/* Invalid argument */` |
|     ! 0 |  6051 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  6052 | `			return rc;` |
|       - |  6053 | `		}` |
|  179147 |  6054 | `		pIn++; /* Jump the dollar sign */` |
|       - |  6055 | `		/* Copy argument name */` |
|  179147 |  6056 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  179147 |  6057 | `		if( zDup == 0 ){` |
|     ! 0 |  6058 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  6059 | `			return SXERR_ABORT;` |
|       - |  6060 | `		}` |
|  179147 |  6061 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  179147 |  6062 | `		pIn++;` |
|  179147 |  6063 | `		if( pIn < pEnd ){` |
|  108497 |  6064 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  6065 | `				SyToken *pDefend;` |
|   80805 |  6066 | `				sxi32 iNest = 0;` |
|   80805 |  6067 | `				pIn++; /* Jump the equal sign */` |
|   80805 |  6068 | `				pDefend = pIn;` |
|       - |  6069 | `				/* Process the default value associated with this argument */` |
|  169305 |  6070 | `				while( pDefend < pEnd ){` |
|  126961 |  6071 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   38461 |  6072 | `						break;` |
|       - |  6073 | `					}` |
|   88505 |  6074 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  6075 | `						/* Increment nesting level */` |
|    3855 |  6076 | `						iNest++;` |
|   86580 |  6077 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  6078 | `						/* Decrement nesting level */` |
|    3855 |  6079 | `						iNest--;` |
|    1925 |  6080 | `					}` |
|   88505 |  6081 | `					pDefend++;` |
|       5 |  6082 | `				}` |
|   80805 |  6083 | `				if( pIn >= pDefend ){` |
|       3 |  6084 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  6085 | `					return rc;` |
|       - |  6086 | `				}` |
|       - |  6087 | `				/* Process default value */` |
|   80803 |  6088 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   80803 |  6089 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  6090 | `					return rc;` |
|       - |  6091 | `				}` |
|       - |  6092 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|       - |  6093 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|       - |  6094 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|       - |  6095 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|       - |  6096 | `				 * arg-type check lets null through. */` |
|   88489 |  6097 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|   63476 |  6098 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|   63475 |  6099 | `					&& &pIn[1] == pDefend` |
|   44230 |  6100 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|   34611 |  6101 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|   21153 |  6102 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|   15387 |  6103 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|    7691 |  6104 | `				}` |
|       - |  6105 | `				/* Point beyond the default value */` |
|   80803 |  6106 | `				pIn = pDefend;` |
|   40399 |  6107 | `			}` |
|  108495 |  6108 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  6109 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  6110 | `				return rc;` |
|       - |  6111 | `			}` |
|  108495 |  6112 | `			pIn++; /* Jump the trailing comma */` |
|   54245 |  6113 | `		}` |
|       - |  6114 | `		/* Append argument signature */` |
|  179145 |  6115 | `		if( sArg.nType > 0 ){` |
|   84905 |  6116 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  6117 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|   15449 |  6118 | `				int marker = 'o';` |
|   15449 |  6119 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|   15449 |  6120 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    7727 |  6121 | `			}else{` |
|       - |  6122 | `				int c;` |
|   69461 |  6123 | `				c = 'n'; /* cc warning */` |
|       - |  6124 | `				/* Type leading character */` |
|   69461 |  6125 | `				switch(sArg.nType){` |
|       4 |  6126 | `				case MEMOBJ_HASHMAP:` |
|       - |  6127 | `					/* Hashmap aka 'array' */` |
|       9 |  6128 | `					c = 'h';` |
|       9 |  6129 | `					break;` |
|    9697 |  6130 | `				case MEMOBJ_INT:` |
|       - |  6131 | `					/* Integer */` |
|   19399 |  6132 | `					c = 'i';` |
|   19399 |  6133 | `					break;` |
|       2 |  6134 | `				case MEMOBJ_BOOL:` |
|       - |  6135 | `					/* Bool */` |
|       5 |  6136 | `					c = 'b';` |
|       5 |  6137 | `					break;` |
|       5 |  6138 | `				case MEMOBJ_REAL:` |
|       - |  6139 | `					/* Float */` |
|      12 |  6140 | `					c = 'f';` |
|      12 |  6141 | `					break;` |
|   25012 |  6142 | `				case MEMOBJ_STRING:` |
|       - |  6143 | `					/* String */` |
|   50029 |  6144 | `					c = 's';` |
|   50029 |  6145 | `					break;` |
|       7 |  6146 | `				case MEMOBJ_OBJ:` |
|       - |  6147 | `					/* Object */` |
|      16 |  6148 | `					c = 'o';` |
|      14 |  6149 | `					break;` |
|       1 |  6150 | `				default:` |
|       2 |  6151 | `					break;` |
|       - |  6152 | `				}` |
|   69461 |  6153 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  6154 | `			}` |
|   42455 |  6155 | `		}else{` |
|       - |  6156 | `			/* No type is associated with this parameter which mean` |
|       - |  6157 | `			 * that this function is not condidate for overloading.` |
|       - |  6158 | `			 */` |
|   94245 |  6159 | `			SyBlobRelease(&sSig);` |
|       - |  6160 | `		}` |
|       - |  6161 | `		/* Save in the argument set */` |
|  179145 |  6162 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  6163 | `	}` |
|  113003 |  6164 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  6165 | `		/* Save function signature */` |
|   54103 |  6166 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   27049 |  6167 | `	}` |
|  113003 |  6168 | `	return SXRET_OK;` |
|   56512 |  6169 | `}` |
|       - |  6170 | `/*` |
|       - |  6171 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|       - |  6172 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|       - |  6173 | ` * the enclosing function. Returns the token just past the nested construct.` |
|       - |  6174 | ` */` |
|      20 |  6175 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|       3 |  6176 | `{` |
|      23 |  6177 | `	sxi32 iParen = 0;` |
|      23 |  6178 | `	pIn++; /* past 'function'/'fn' */` |
|       - |  6179 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|       - |  6180 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|       - |  6181 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|      83 |  6182 | `	while( pIn < pEnd ){` |
|      83 |  6183 | `		sxu32 t = pIn->nType;` |
|      83 |  6184 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|      63 |  6185 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|      43 |  6186 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|      23 |  6187 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|      63 |  6188 | `		pIn++;` |
|       3 |  6189 | `	}` |
|      23 |  6190 | `	if( pIn >= pEnd ){ return pIn; }` |
|       - |  6191 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|       - |  6192 | `	{` |
|      23 |  6193 | `		sxi32 d = 0;` |
|     211 |  6194 | `		while( pIn < pEnd ){` |
|     211 |  6195 | `			sxu32 t = pIn->nType;` |
|     211 |  6196 | `			if( t & PH7_TK_OCB ){ d++; }` |
|     187 |  6197 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|     191 |  6198 | `			pIn++;` |
|       3 |  6199 | `		}` |
|       - |  6200 | `	}` |
|      23 |  6201 | `	return pIn;` |
|      13 |  6202 | `}` |
|       - |  6203 | `/*` |
|       - |  6204 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|       - |  6205 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|       - |  6206 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|       - |  6207 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|       - |  6208 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|       - |  6209 | ` * detached-mini-program path untouched.` |
|       - |  6210 | ` */` |
|       - |  6211 | `/*` |
|       - |  6212 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|       - |  6213 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|       - |  6214 | ` * mixed, object.` |
|       - |  6215 | ` */` |
|      28 |  6216 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|       3 |  6217 | `{` |
|       - |  6218 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|       - |  6219 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|       - |  6220 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|       - |  6221 | `	};` |
|       - |  6222 | `	sxu32 i;` |
|      31 |  6223 | `	if( nName > 0 && zName[0] == '\\' ){` |
|     ! 0 |  6224 | `		zName++;` |
|     ! 0 |  6225 | `		nName--;` |
|     ! 0 |  6226 | `	}` |
|      63 |  6227 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|      59 |  6228 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|      27 |  6229 | `			return 1;` |
|       - |  6230 | `		}` |
|      17 |  6231 | `	}` |
|       5 |  6232 | `	return 0;` |
|      17 |  6233 | `}` |
|       - |  6234 | `/*` |
|       - |  6235 | ` * One atom of a generator's declared return type: is it a supertype of` |
|       - |  6236 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|       - |  6237 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|       - |  6238 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|       - |  6239 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|       - |  6240 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|       - |  6241 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|       - |  6242 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|       - |  6243 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|       - |  6244 | ` * both rather than fatal on valid code (divergence recorded in PLAN.md).` |
|       - |  6245 | ` */` |
|      26 |  6246 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|       4 |  6247 | `{` |
|      30 |  6248 | `	if( nType == MEMOBJ_OBJ ){` |
|     ! 0 |  6249 | ``		return 1; /* bare `object` */`` |
|       - |  6250 | `	}` |
|      30 |  6251 | `	if( nType != SXU32_HIGH ){` |
|       3 |  6252 | `		return 0; /* scalar/array/void/never/null/... */` |
|       - |  6253 | `	}` |
|      27 |  6254 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|      23 |  6255 | `		return 1;` |
|       - |  6256 | `	}` |
|       - |  6257 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|       - |  6258 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|       - |  6259 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|       - |  6260 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|       - |  6261 | `	{` |
|       - |  6262 | `		SyBlob sFQN;` |
|       - |  6263 | `		int bOk;` |
|       5 |  6264 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       5 |  6265 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|       5 |  6266 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|       5 |  6267 | `		SyBlobRelease(&sFQN);` |
|       5 |  6268 | `		return bOk;` |
|       - |  6269 | `	}` |
|      17 |  6270 | `}` |
|       - |  6271 | `/*` |
|       - |  6272 | ` * php 8: a generator function may only declare a return type that is a` |
|       - |  6273 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|       - |  6274 | ` * group qualifies only if every member does. Anything else is php's exact` |
|       - |  6275 | ` * compile-time fatal "Generator return type must be a supertype of` |
|       - |  6276 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|       - |  6277 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|       - |  6278 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|       - |  6279 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|       - |  6280 | ` */` |
|     212 |  6281 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|       5 |  6282 | `{` |
|     217 |  6283 | `	int bOk = 0;` |
|       - |  6284 | `	sxu32 nLine;` |
|       - |  6285 | `	sxi32 rc;` |
|     217 |  6286 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|     191 |  6287 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|       - |  6288 | `	}` |
|      30 |  6289 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|     ! 0 |  6290 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|     ! 0 |  6291 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|       - |  6292 | `		sxu32 i,j;` |
|     ! 0 |  6293 | `		for( i = 0; i < n && !bOk; i++ ){` |
|       - |  6294 | `			int bGroupOk;` |
|     ! 0 |  6295 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|     ! 0 |  6296 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|       - |  6297 | `			}` |
|     ! 0 |  6298 | `			bGroupOk = 1;` |
|     ! 0 |  6299 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|     ! 0 |  6300 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|     ! 0 |  6301 | `					bGroupOk = 0;` |
|     ! 0 |  6302 | `					break;` |
|       - |  6303 | `				}` |
|     ! 0 |  6304 | `			}` |
|     ! 0 |  6305 | `			bOk = bGroupOk;` |
|     ! 0 |  6306 | `		}` |
|     ! 0 |  6307 | `	}else{` |
|      30 |  6308 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|       - |  6309 | `	}` |
|      30 |  6310 | `	if( bOk ){` |
|      27 |  6311 | `		return SXRET_OK;` |
|       - |  6312 | `	}` |
|       - |  6313 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|       - |  6314 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|       - |  6315 | `	 * token of this stream — its line is the function's closing brace. php` |
|       - |  6316 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|       - |  6317 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|       3 |  6318 | `	nLine = pGen->pIn[-1].nLine;` |
|       - |  6319 | `	{` |
|       3 |  6320 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|       3 |  6321 | `		if( sGiven.nByte < 1 ){` |
|     ! 0 |  6322 | `			sGiven = pFunc->sReturnClass;` |
|     ! 0 |  6323 | `		}` |
|       3 |  6324 | `		if( sGiven.nByte < 1 ){` |
|       - |  6325 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|       - |  6326 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|       - |  6327 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|     ! 0 |  6328 | `			const char *zScalar =` |
|     ! 0 |  6329 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|     ! 0 |  6330 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|     ! 0 |  6331 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|     ! 0 |  6332 | `		}` |
|       3 |  6333 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  6334 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|       - |  6335 | `	}` |
|       3 |  6336 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|     111 |  6337 | `}` |
|  241090 |  6338 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|       5 |  6339 | `{` |
|  241095 |  6340 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|  241095 |  6341 | `	SyToken *pEnd = pGen->pEnd;` |
|  241095 |  6342 | `	sxi32 iDepth = 0;` |
|  241095 |  6343 | `	int bStarted = 0;` |
| 8005399 |  6344 | `	while( pIn < pEnd ){` |
| 8005399 |  6345 | `		sxu32 t = pIn->nType;` |
| 8005399 |  6346 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 7544281 |  6347 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 7083479 |  6348 | `		if( t & PH7_TK_KEYWORD ){` |
|  561895 |  6349 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  561895 |  6350 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  561683 |  6351 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|       - |  6352 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|  280829 |  6353 | `		}` |
| 7083247 |  6354 | `		pIn++;` |
|       5 |  6355 | `	}` |
|  240883 |  6356 | `	return FALSE;` |
|  120550 |  6357 | `}` |
|       - |  6358 | `/*` |
|       - |  6359 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  6360 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  6361 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  6362 | ` */` |
|  241090 |  6363 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  6364 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  6365 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  6366 | `	)` |
|       5 |  6367 | `{` |
|       - |  6368 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  6369 | `	GenBlock *pBlock;` |
|       - |  6370 | `	sxu32 nGotoOfft;` |
|       - |  6371 | `	sxi32 rc;` |
|       - |  6372 | `	/* Attach the new function */` |
|  241095 |  6373 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  241095 |  6374 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6375 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  6376 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6377 | `		return SXERR_ABORT;` |
|       - |  6378 | `	}` |
|  241095 |  6379 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  6380 | `	/* Swap bytecode containers */` |
|  241095 |  6381 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  241095 |  6382 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  6383 | `	/* Emit constructor property promotion prologue:` |
|       - |  6384 | `	 *   $this->NAME = $NAME;` |
|       - |  6385 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  6386 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  6387 | `	{` |
|  241095 |  6388 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  6389 | `		sxu32 i;` |
|  389333 |  6390 | `		for( i = 0; i < nArg; i++ ){` |
|  148243 |  6391 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  6392 | `			char *zSrc;` |
|       - |  6393 | `			sxu32 nSrc,nName;` |
|       - |  6394 | `			SySet sToken;` |
|       - |  6395 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  6396 | `			sxi32 rcPromote;` |
|  148243 |  6397 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  148181 |  6398 | `				continue;` |
|       - |  6399 | `			}` |
|       - |  6400 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  6401 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  6402 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  6403 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  6404 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      67 |  6405 | `			nName = SyStringLength(&pArg->sName);` |
|      67 |  6406 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      67 |  6407 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      67 |  6408 | `			if( zSrc == 0 ){` |
|     ! 0 |  6409 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6410 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6411 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  6412 | `				return SXERR_ABORT;` |
|       - |  6413 | `			}` |
|       - |  6414 | `			{` |
|      67 |  6415 | `				char *z = zSrc;` |
|      67 |  6416 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      67 |  6417 | `				z += sizeof("$this->")-1;` |
|      67 |  6418 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      67 |  6419 | `				z += nName;` |
|      67 |  6420 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      67 |  6421 | `				z += sizeof(" = $")-1;` |
|      67 |  6422 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      67 |  6423 | `				z += nName;` |
|      67 |  6424 | `				*z = 0;` |
|       - |  6425 | `			}` |
|      67 |  6426 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      67 |  6427 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      67 |  6428 | `			pTmpIn = pGen->pIn;` |
|      67 |  6429 | `			pTmpEnd = pGen->pEnd;` |
|      67 |  6430 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      67 |  6431 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      67 |  6432 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      67 |  6433 | `			pGen->pIn = pTmpIn;` |
|      67 |  6434 | `			pGen->pEnd = pTmpEnd;` |
|      67 |  6435 | `			SySetRelease(&sToken);` |
|      67 |  6436 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  6437 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6438 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6439 | `				return SXERR_ABORT;` |
|       - |  6440 | `			}` |
|       - |  6441 | `			/* Discard the assignment result — this is a statement expression. */` |
|      67 |  6442 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      36 |  6443 | `		}` |
|       - |  6444 | `	}` |
|       - |  6445 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|       - |  6446 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|       - |  6447 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|       - |  6448 | `	 * generator — and vice versa — is classified independently. */` |
|       - |  6449 | `	{` |
|  241095 |  6450 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|  241095 |  6451 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|       - |  6452 | `		/* Compile the body */` |
|  241095 |  6453 | `		PH7_CompileBlock(&(*pGen),0);` |
|  241095 |  6454 | `		pGen->bInGenerator = bSavedGen;` |
|       - |  6455 | `	}` |
|       - |  6456 | `	/* Fix exception jumps now the destination is resolved */` |
|  241095 |  6457 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  6458 | `	/* Emit the final return if not yet done */` |
|  241095 |  6459 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  6460 | `	/* Fix gotos jumps now the destination is resolved */` |
|  241095 |  6461 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  6462 | `		rc = SXERR_ABORT;` |
|     ! 0 |  6463 | `	}` |
|  241095 |  6464 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  6465 | `	/* Restore the default container */` |
|  241095 |  6466 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  6467 | `	/* Leave function block */` |
|  241095 |  6468 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  241095 |  6469 | `	if( rc == SXERR_ABORT ){` |
|       - |  6470 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6471 | `		return SXERR_ABORT;` |
|       - |  6472 | `	}` |
|       - |  6473 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  6474 | `	{` |
|  241095 |  6475 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  6476 | `		sxu32 i;` |
| 4733917 |  6477 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 4493039 |  6478 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     217 |  6479 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     217 |  6480 | `				break;` |
|       - |  6481 | `			}` |
| 2246416 |  6482 | `		}` |
|       - |  6483 | `	}` |
|  241095 |  6484 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|       - |  6485 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     217 |  6486 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|     ! 0 |  6487 | `			return SXERR_ABORT;` |
|       - |  6488 | `		}` |
|     106 |  6489 | `	}` |
|       - |  6490 | `	/* All done, function body compiled */` |
|  241095 |  6491 | `	return SXRET_OK;` |
|  120550 |  6492 | `}` |
|       - |  6493 | `/*` |
|       - |  6494 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  6495 | ` * According to the PHP language reference manual.` |
|       - |  6496 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  6497 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  6498 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  6499 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  6500 | ` *  Functions need not be defined before they are referenced.` |
|       - |  6501 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  6502 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  6503 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  6504 | ` *  calls with over 32-64 recursion levels.` |
|       - |  6505 | ` *` |
|       - |  6506 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  6507 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  6508 | ` * on these extension.` |
|       - |  6509 | ` */` |
|       - |  6510 | `/*` |
|       - |  6511 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  6512 | ` */` |
|     532 |  6513 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  6514 | `{` |
|       - |  6515 | `	sxu32 i;` |
|    1507 |  6516 | `	for( i = 0; i < n; i++ ){` |
|    1293 |  6517 | `		int a = zA[i], b = zB[i];` |
|    1293 |  6518 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|    1293 |  6519 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|    1293 |  6520 | `		if( a != b ) return a - b;` |
|     490 |  6521 | `	}` |
|     219 |  6522 | `	return 0;` |
|     271 |  6523 | `}` |
|       - |  6524 | `/*` |
|       - |  6525 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  6526 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  6527 | ` * (which are positive bit values stored in sxu32).` |
|       - |  6528 | ` */` |
|       - |  6529 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  6530 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  6531 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  6532 |  |
|       - |  6533 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|       - |  6534 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|       - |  6535 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|       - |  6536 |  |
|       - |  6537 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  6538 | `struct PhlTypeAtom {` |
|       - |  6539 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  6540 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  6541 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  6542 | `	sxu32 nCanon;` |
|       - |  6543 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|       - |  6544 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|       - |  6545 | `};` |
|       - |  6546 |  |
|       - |  6547 | `/*` |
|       - |  6548 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  6549 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  6550 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  6551 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  6552 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  6553 | ` * already be consumed by the caller.` |
|       - |  6554 | ` */` |
|   85930 |  6555 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  6556 | `{` |
|   85935 |  6557 | `	SyToken *pIn = pGen->pIn;` |
|   85935 |  6558 | `	SyZero(pOut, sizeof(*pOut));` |
|   85935 |  6559 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   85935 |  6560 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6561 | `		return SXERR_SYNTAX;` |
|       - |  6562 | `	}` |
|       - |  6563 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   85935 |  6564 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  6565 | `		pIn++;` |
|       8 |  6566 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6567 | `			return SXERR_SYNTAX;` |
|       - |  6568 | `		}` |
|       3 |  6569 | `	}` |
|   85935 |  6570 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6571 | `		return SXERR_SYNTAX;` |
|       - |  6572 | `	}` |
|   85935 |  6573 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   70065 |  6574 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   70065 |  6575 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      34 |  6576 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   70050 |  6577 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      75 |  6578 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   70000 |  6579 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   19685 |  6580 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   60125 |  6581 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   50207 |  6582 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   25184 |  6583 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      39 |  6584 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      65 |  6585 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  6586 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      35 |  6587 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|      12 |  6588 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      21 |  6589 | `			pOut->nType = SXU32_HIGH;` |
|      21 |  6590 | `			pOut->sClass = pIn->sData;` |
|      12 |  6591 | `		}else{` |
|       3 |  6592 | `			return SXERR_SYNTAX;` |
|       - |  6593 | `		}` |
|   70063 |  6594 | `		pIn++;` |
|   35034 |  6595 | `	}else{` |
|       - |  6596 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6597 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   15875 |  6598 | `		SyString *pT = &pIn->sData;` |
|   15875 |  6599 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      32 |  6600 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      32 |  6601 | `			pIn++;` |
|   15861 |  6602 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     165 |  6603 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     165 |  6604 | `			pIn++;` |
|   15767 |  6605 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|      24 |  6606 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|      24 |  6607 | `			pIn++;` |
|      14 |  6608 | `		}else{` |
|       - |  6609 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   15667 |  6610 | `			SyToken *pFirst = pIn;` |
|   15667 |  6611 | `			SyToken *pLast = pIn;` |
|   15667 |  6612 | `			pOut->nType = SXU32_HIGH;` |
|   15667 |  6613 | `			pOut->sClass = pIn->sData;` |
|   15667 |  6614 | `			pIn++;` |
|   23496 |  6615 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   15670 |  6616 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6617 | `				pLast = &pIn[1];` |
|       3 |  6618 | `				pIn += 2;` |
|       1 |  6619 | `			}` |
|   15667 |  6620 | `			if( pLast != pFirst ){` |
|       3 |  6621 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6622 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6623 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6624 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6625 | `			}` |
|       - |  6626 | `		}` |
|       - |  6627 | `	}` |
|   85933 |  6628 | `	pGen->pIn = pIn;` |
|   85933 |  6629 | `	return SXRET_OK;` |
|   42970 |  6630 | `}` |
|       - |  6631 |  |
|       - |  6632 | `/*` |
|       - |  6633 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6634 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6635 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6636 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6637 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6638 | ` */` |
|   85764 |  6639 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6640 | `{` |
|       - |  6641 | `	int i;` |
|   85769 |  6642 | `	int nNonNull = 0;` |
|   85769 |  6643 | `	int bAnyIntersection = 0;` |
|       - |  6644 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   85769 |  6645 | `	sxu32 nMaxGroup = 0;` |
| 2830217 |  6646 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  171673 |  6647 | `	for( i = 0; i < nAtoms; i++ ){` |
|   85909 |  6648 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   85881 |  6649 | `			nNonNull++;` |
|   85881 |  6650 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   85881 |  6651 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   85881 |  6652 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   42938 |  6653 | `			}` |
|   42938 |  6654 | `		}` |
|   42957 |  6655 | `	}` |
|  171631 |  6656 | `	for( i = 0; i < nAtoms; i++ ){` |
|   85887 |  6657 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      24 |  6658 | `			bAnyIntersection = 1;` |
|      24 |  6659 | `			break;` |
|       - |  6660 | `		}` |
|   42936 |  6661 | `	}` |
|   85769 |  6662 | `	if( bAnyIntersection ){` |
|       - |  6663 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|       - |  6664 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|       - |  6665 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|      24 |  6666 | `		sxu32 g, nGroups = 0;` |
|      24 |  6667 | `		int bFirstGroup = 1;` |
|      48 |  6668 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|      48 |  6669 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|      28 |  6670 | `			int bFirstMember = 1;` |
|       - |  6671 | `			int bWrap;` |
|      28 |  6672 | `			if( aGroupCount[g] == 0 ) continue;` |
|       - |  6673 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|       - |  6674 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|       - |  6675 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|       - |  6676 | `			 * parens, matching PHP's canonical text. */` |
|      38 |  6677 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|      28 |  6678 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|      28 |  6679 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      84 |  6680 | `			for( i = 0; i < nAtoms; i++ ){` |
|      60 |  6681 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|      48 |  6682 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|      48 |  6683 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      46 |  6684 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      25 |  6685 | `				}else{` |
|       3 |  6686 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6687 | `				}` |
|      48 |  6688 | `				bFirstMember = 0;` |
|      26 |  6689 | `			}` |
|      28 |  6690 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|      28 |  6691 | `			bFirstGroup = 0;` |
|      16 |  6692 | `		}` |
|      24 |  6693 | `		if( bNullable ){` |
|     ! 0 |  6694 | `			SyBlobAppend(pBlob, "\|", 1);` |
|     ! 0 |  6695 | `			SyBlobAppend(pBlob, "null", 4);` |
|     ! 0 |  6696 | `		}` |
|      64 |  6697 | `		return;` |
|       - |  6698 | `	}` |
|   85749 |  6699 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6700 | `		/* Shorthand: ?T */` |
|      84 |  6701 | `		for( i = 0; i < nAtoms; i++ ){` |
|      84 |  6702 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      84 |  6703 | `			SyBlobAppend(pBlob, "?", 1);` |
|      84 |  6704 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      22 |  6705 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      13 |  6706 | `			}else{` |
|      66 |  6707 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6708 | `			}` |
|      84 |  6709 | `			return;` |
|     ! 0 |  6710 | `		}` |
|     ! 0 |  6711 | `	}` |
|       - |  6712 | `	{` |
|   85669 |  6713 | `		int bFirst = 1;` |
|       - |  6714 | `		/* 1) Classes in declaration order */` |
|  171437 |  6715 | `		for( i = 0; i < nAtoms; i++ ){` |
|   85773 |  6716 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   15625 |  6717 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   15625 |  6718 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   15625 |  6719 | `				bFirst = 0;` |
|    7810 |  6720 | `			}` |
|   42889 |  6721 | `		}` |
|       - |  6722 | `		/* 2) Built-ins in canonical order */` |
|       - |  6723 | `		{` |
|       - |  6724 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6725 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6726 | `			int k;` |
|  599653 |  6727 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  958529 |  6728 | `				for( i = 0; i < nAtoms; i++ ){` |
|  514505 |  6729 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   69965 |  6730 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   69965 |  6731 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   69965 |  6732 | `						bFirst = 0;` |
|   69965 |  6733 | `						break;` |
|       - |  6734 | `					}` |
|  222275 |  6735 | `				}` |
|  256997 |  6736 | `			}` |
|       - |  6737 | `		}` |
|       - |  6738 | `		/* 3) null suffix */` |
|   85669 |  6739 | `		if( bNullable ){` |
|      19 |  6740 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      19 |  6741 | `			SyBlobAppend(pBlob, "null", 4);` |
|       8 |  6742 | `		}` |
|       - |  6743 | `	}` |
|   42887 |  6744 | `}` |
|       - |  6745 |  |
|       - |  6746 | `/*` |
|       - |  6747 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|       - |  6748 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|       - |  6749 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|       - |  6750 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|       - |  6751 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|       - |  6752 | ` * whether it was parenthesized.` |
|       - |  6753 | ` *` |
|       - |  6754 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|       - |  6755 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|       - |  6756 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|       - |  6757 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|       - |  6758 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|       - |  6759 | ` */` |
|   85908 |  6760 | `static sxi32 GenStateParsePart(` |
|       - |  6761 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|       - |  6762 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|       5 |  6763 | `{` |
|       - |  6764 | `	sxi32 rc;` |
|   85913 |  6765 | `	int nMembers = 0;` |
|   85913 |  6766 | `	int bParen = 0;` |
|   85913 |  6767 | `	*pnMembers = 0;` |
|   85913 |  6768 | `	*pbParen = 0;` |
|   85913 |  6769 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       6 |  6770 | `		bParen = 1;` |
|       6 |  6771 | `		pGen->pIn++; /* skip '(' */` |
|       2 |  6772 | `	}` |
|   42954 |  6773 | `	for(;;){` |
|   85935 |  6774 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6775 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6776 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6777 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6778 | `		}` |
|   85935 |  6779 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   85935 |  6780 | `		if( rc != SXRET_OK ){` |
|       3 |  6781 | `			return rc;` |
|       - |  6782 | `		}` |
|   85933 |  6783 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   85933 |  6784 | `		(*pnAtoms)++;` |
|   85933 |  6785 | `		nMembers++;` |
|       - |  6786 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   85933 |  6787 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      30 |  6788 | `			SyToken *pNext = &pGen->pIn[1];` |
|      26 |  6789 | `			if( pNext < pGen->pEnd` |
|      30 |  6790 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      26 |  6791 | `				pGen->pIn++; /* skip '&' */` |
|      26 |  6792 | `				continue;` |
|       - |  6793 | `			}` |
|       2 |  6794 | `		}` |
|   85911 |  6795 | `		break;` |
|     ! 0 |  6796 | `	}` |
|   85911 |  6797 | `	if( bParen ){` |
|       6 |  6798 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6799 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6800 | `				"Malformed DNF type: expecting ')'");` |
|     ! 0 |  6801 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6802 | `		}` |
|       6 |  6803 | `		pGen->pIn++; /* skip ')' */` |
|       6 |  6804 | `		if( nMembers < 2 ){` |
|     ! 0 |  6805 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6806 | `				"Parenthesized type must be an intersection of at least two types");` |
|     ! 0 |  6807 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6808 | `		}` |
|       2 |  6809 | `	}` |
|   85911 |  6810 | `	*pnMembers = nMembers;` |
|   85911 |  6811 | `	*pbParen = bParen;` |
|   85911 |  6812 | `	return SXRET_OK;` |
|   42959 |  6813 | `}` |
|       - |  6814 |  |
|       - |  6815 | `/*` |
|       - |  6816 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6817 | ` *` |
|       - |  6818 | ` * Outputs:` |
|       - |  6819 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6820 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6821 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6822 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6823 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6824 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6825 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6826 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6827 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6828 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6829 | ` *` |
|       - |  6830 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6831 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6832 | ` */` |
|   85780 |  6833 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6834 | `	ph7_gen_state *pGen,` |
|       - |  6835 | `	sxu32 *pnType,` |
|       - |  6836 | `	SyString *pClass,` |
|       - |  6837 | `	SySet *pAlts,` |
|       - |  6838 | `	sxi32 *piTypeFlags,` |
|       - |  6839 | `	SyString *pTypeText,` |
|       - |  6840 | `	int iNullableFlag,` |
|       - |  6841 | `	int iUnionFlag,` |
|       - |  6842 | `	int bAllowVoid,` |
|       - |  6843 | `	sxu32 nLine` |
|       5 |  6844 | `){` |
|       - |  6845 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   85785 |  6846 | `	int nAtoms = 0;` |
|   85785 |  6847 | `	int bShortNullable = 0;` |
|   85785 |  6848 | `	int bExplicitNull = 0;` |
|       - |  6849 | `	sxi32 rc;` |
|   85785 |  6850 | `	*pnType = 0;` |
|   85785 |  6851 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   85785 |  6852 | `	*piTypeFlags = 0;` |
|   85785 |  6853 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6854 |  |
|   85785 |  6855 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6856 | `		return SXRET_OK;` |
|       - |  6857 | `	}` |
|       - |  6858 | ``	/* Optional `?` shorthand prefix */`` |
|   85780 |  6859 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      75 |  6860 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      74 |  6861 | `		bShortNullable = 1;` |
|      74 |  6862 | `		pGen->pIn++;` |
|      74 |  6863 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6864 | `			return SXERR_SYNTAX;` |
|       - |  6865 | `		}` |
|      35 |  6866 | `	}` |
|       - |  6867 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|       - |  6868 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|       - |  6869 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|       - |  6870 | `	{` |
|       - |  6871 | `		int nMembers, bParen;` |
|   85785 |  6872 | `		sxu32 iGroup = 0;` |
|   85785 |  6873 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   85785 |  6874 | `		if( rc != SXRET_OK ){` |
|       4 |  6875 | `			return rc;` |
|       - |  6876 | `		}` |
|       - |  6877 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|       - |  6878 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|       - |  6879 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|       - |  6880 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|       - |  6881 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|  128862 |  6882 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   85978 |  6883 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     135 |  6884 | `			if( bShortNullable ){` |
|       - |  6885 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6886 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6887 | `				 * already reported" so callers skip their own error emission. */` |
|       3 |  6888 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6889 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6890 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6891 | `			}` |
|     133 |  6892 | `			if( nMembers >= 2 && !bParen ){` |
|     ! 0 |  6893 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|       - |  6894 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6895 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6896 | `			}` |
|     133 |  6897 | ``			pGen->pIn++; /* skip `\|` */`` |
|     133 |  6898 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|     133 |  6899 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  6900 | `				return rc;` |
|       - |  6901 | `			}` |
|       5 |  6902 | `		}` |
|   85781 |  6903 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|     ! 0 |  6904 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6905 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6906 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6907 | `		}` |
|       - |  6908 | `	}` |
|       - |  6909 | `	/* Validation pass.` |
|       - |  6910 | `	 *` |
|       - |  6911 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6912 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6913 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6914 | `	 */` |
|       - |  6915 | `	{` |
|       - |  6916 | `		int i, j;` |
|   85781 |  6917 | `		int bHasNonNull = 0;` |
|   85781 |  6918 | `		int bAnyIntersection = 0;` |
|       - |  6919 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|       - |  6920 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|       - |  6921 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 2830613 |  6922 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  171707 |  6923 | `		for( i = 0; i < nAtoms; i++ ){` |
|   85931 |  6924 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   42968 |  6925 | `		}` |
|  171661 |  6926 | `		for( i = 0; i < nAtoms; i++ ){` |
|   85907 |  6927 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   42945 |  6928 | `		}` |
|       - |  6929 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|       - |  6930 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   85781 |  6931 | `		if( bShortNullable && bAnyIntersection ){` |
|     ! 0 |  6932 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6933 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|     ! 0 |  6934 | `			return SXERR_SYNTAX;` |
|       - |  6935 | `		}` |
|  171693 |  6936 | `		for( i = 0; i < nAtoms; i++ ){` |
|       - |  6937 | `			/* Intersection members must be class/interface types (PHP rejects` |
|       - |  6938 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|       - |  6939 | ``			 * `true`/`false` in an intersection). */`` |
|   85929 |  6940 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      46 |  6941 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|      46 |  6942 | `				if( bClassLike ){` |
|      44 |  6943 | `					SyString *pC = &aAtoms[i].sClass;` |
|      40 |  6944 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|      40 |  6945 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|      40 |  6946 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|      44 |  6947 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|     ! 0 |  6948 | `						bClassLike = 0;` |
|     ! 0 |  6949 | `					}` |
|      20 |  6950 | `				}` |
|      46 |  6951 | `				if( !bClassLike ){` |
|       - |  6952 | `					const char *zName; sxu32 nName;` |
|       3 |  6953 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6954 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6955 | `					}else{` |
|       3 |  6956 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|       - |  6957 | `					}` |
|       4 |  6958 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6959 | `						"Type %.*s cannot be part of an intersection type",` |
|       1 |  6960 | `						(int)nName, zName);` |
|       3 |  6961 | `					return SXERR_SYNTAX;` |
|       - |  6962 | `				}` |
|      20 |  6963 | `			}` |
|   85927 |  6964 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     165 |  6965 | `				if( nAtoms > 1 ){` |
|       3 |  6966 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6967 | `						"Void can only be used as a standalone type");` |
|       3 |  6968 | `					return SXERR_SYNTAX;` |
|       - |  6969 | `				}` |
|     163 |  6970 | `				if( !bAllowVoid ){` |
|     ! 0 |  6971 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6972 | `						"void cannot be used here");` |
|     ! 0 |  6973 | `					return SXERR_SYNTAX;` |
|       - |  6974 | `				}` |
|     163 |  6975 | `				if( bShortNullable ){` |
|     ! 0 |  6976 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6977 | `						"Void type cannot be nullable");` |
|     ! 0 |  6978 | `					return SXERR_SYNTAX;` |
|       - |  6979 | `				}` |
|      79 |  6980 | `			}` |
|   85925 |  6981 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6982 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|       - |  6983 | `				 * type (never = the function does not return). Mirrors the void` |
|       - |  6984 | `				 * validation above; accepted here and enforced at compile time` |
|       - |  6985 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|      24 |  6986 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|       - |  6987 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|       - |  6988 | `					 * same as any other non-standalone use. */` |
|       5 |  6989 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6990 | `						"never can only be used as a standalone type");` |
|       5 |  6991 | `					return SXERR_SYNTAX;` |
|       - |  6992 | `				}` |
|      19 |  6993 | `				if( !bAllowVoid ){` |
|       - |  6994 | `					/* Return-only: params call with bAllowVoid=0. */` |
|       3 |  6995 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6996 | `						"never cannot be used as a parameter type");` |
|       3 |  6997 | `					return SXERR_SYNTAX;` |
|       - |  6998 | `				}` |
|       7 |  6999 | `			}` |
|   85919 |  7000 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      32 |  7001 | `				bExplicitNull = 1;` |
|      18 |  7002 | `			}else{` |
|   85891 |  7003 | `				bHasNonNull = 1;` |
|       - |  7004 | `			}` |
|       - |  7005 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|       - |  7006 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|       - |  7007 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|       - |  7008 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|       - |  7009 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   86105 |  7010 | `			for( j = 0; j < i; j++ ){` |
|     193 |  7011 | `				int bDup = 0;` |
|     193 |  7012 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|     369 |  7013 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|     188 |  7014 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|     193 |  7015 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|     185 |  7016 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      47 |  7017 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      40 |  7018 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      42 |  7019 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|      17 |  7020 | `								aAtoms[j].sClass.zString,` |
|      34 |  7021 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  7022 | `							bDup = 1;` |
|     ! 0 |  7023 | `						}` |
|      25 |  7024 | `					}else{` |
|       3 |  7025 | `						bDup = 1;` |
|       - |  7026 | `					}` |
|      21 |  7027 | `				}` |
|     185 |  7028 | `				if( bDup ){` |
|       - |  7029 | `					const char *zName;` |
|       - |  7030 | `					sxu32 nName;` |
|       3 |  7031 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  7032 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  7033 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  7034 | `					}else{` |
|       3 |  7035 | `						zName = aAtoms[i].zCanon;` |
|       3 |  7036 | `						nName = aAtoms[i].nCanon;` |
|       - |  7037 | `					}` |
|       4 |  7038 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  7039 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  7040 | `					return SXERR_SYNTAX;` |
|       - |  7041 | `				}` |
|      94 |  7042 | `			}` |
|   42961 |  7043 | `		}` |
|   85769 |  7044 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  7045 | `			if( bShortNullable ){` |
|       - |  7046 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  7047 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  7048 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  7049 | `				return SXERR_SYNTAX;` |
|       - |  7050 | `			}` |
|       - |  7051 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  7052 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  7053 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  7054 | `			 * atom, so set it here. */` |
|       7 |  7055 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  7056 | `		}` |
|       - |  7057 | `	}` |
|       - |  7058 | `	/* Compute nullability flag */` |
|   85769 |  7059 | `	if( bShortNullable \|\| bExplicitNull ){` |
|     100 |  7060 | `		*piTypeFlags \|= iNullableFlag;` |
|      48 |  7061 | `	}` |
|       - |  7062 | `	/* Build canonical type text */` |
|   85769 |  7063 | `	if( pTypeText ){` |
|       - |  7064 | `		SyBlob sBlob;` |
|   85769 |  7065 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  128617 |  7066 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   42882 |  7067 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   85769 |  7068 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  128393 |  7069 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   85592 |  7070 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   85597 |  7071 | `			if( zDup ){` |
|   85597 |  7072 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   42796 |  7073 | `			}` |
|   42796 |  7074 | `		}` |
|   85769 |  7075 | `		SyBlobRelease(&sBlob);` |
|   42882 |  7076 | `	}` |
|       - |  7077 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  7078 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  7079 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  7080 | `	{` |
|   85769 |  7081 | `		int nNonNull = 0;` |
|   85769 |  7082 | `		int iNonNullIdx = -1;` |
|       - |  7083 | `		int i;` |
|  171673 |  7084 | `		for( i = 0; i < nAtoms; i++ ){` |
|   85909 |  7085 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   85881 |  7086 | `				nNonNull++;` |
|   85881 |  7087 | `				iNonNullIdx = i;` |
|   42938 |  7088 | `			}` |
|   42957 |  7089 | `		}` |
|   85769 |  7090 | `		if( nNonNull <= 1 ){` |
|       - |  7091 | `			/* Fast path: store as single type. */` |
|   85671 |  7092 | `			if( iNonNullIdx >= 0 ){` |
|   85665 |  7093 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   85665 |  7094 | `				if( pA->nType == SXU32_HIGH ){` |
|   23396 |  7095 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    7797 |  7096 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   15599 |  7097 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   15599 |  7098 | `					*pnType = SXU32_HIGH;` |
|   15599 |  7099 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   77868 |  7100 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     163 |  7101 | `					*pnType = MEMOBJ_VOID;` |
|   69992 |  7102 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|      16 |  7103 | `					*pnType = MEMOBJ_NEVER;` |
|       9 |  7104 | `				}else{` |
|   69899 |  7105 | `					*pnType = pA->nType;` |
|       - |  7106 | `				}` |
|   42830 |  7107 | `			}` |
|   42838 |  7108 | `		}else{` |
|       - |  7109 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|     103 |  7110 | `			*piTypeFlags \|= iUnionFlag;` |
|     329 |  7111 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  7112 | `				ph7_type_alt sAlt;` |
|     231 |  7113 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     221 |  7114 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     221 |  7115 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|     221 |  7116 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|     134 |  7117 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      43 |  7118 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      91 |  7119 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      91 |  7120 | `					sAlt.nType = SXU32_HIGH;` |
|      91 |  7121 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      48 |  7122 | `				}else{` |
|     135 |  7123 | `					sAlt.nType = aAtoms[i].nType;` |
|     135 |  7124 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  7125 | `				}` |
|     221 |  7126 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|     113 |  7127 | `			}` |
|       - |  7128 | `		}` |
|       - |  7129 | `	}` |
|   85769 |  7130 | `	return SXRET_OK;` |
|   42895 |  7131 | `}` |
|       - |  7132 |  |
|       - |  7133 | `/*` |
|       - |  7134 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  7135 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  7136 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  7137 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  7138 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  7139 | `` *          and union types `: T\|U`.`` |
|       - |  7140 | ` */` |
|  341326 |  7141 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  7142 | `{` |
|  341331 |  7143 | `	sxi32 iFlags = 0;` |
|       - |  7144 | `	sxi32 rc;` |
|       - |  7145 | `	sxu32 nLine;` |
|  341331 |  7146 | `	pFunc->nReturnType = 0;` |
|  341331 |  7147 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  341331 |  7148 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|       - |  7149 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|       - |  7150 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|       - |  7151 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|       - |  7152 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|       - |  7153 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|  341331 |  7154 | `	SySetReset(&pFunc->aReturnUnion);` |
|  341331 |  7155 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|  341331 |  7156 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  340741 |  7157 | `		return SXRET_OK;` |
|       - |  7158 | `	}` |
|     595 |  7159 | `	pGen->pIn++; /* Skip ':' */` |
|     595 |  7160 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7161 | `		return SXRET_OK;` |
|       - |  7162 | `	}` |
|     595 |  7163 | `	nLine = pGen->pIn->nLine;` |
|     595 |  7164 | `	rc = GenStateParseUnionTypeDecl(` |
|     295 |  7165 | `		pGen,` |
|     295 |  7166 | `		&pFunc->nReturnType,` |
|     295 |  7167 | `		&pFunc->sReturnClass,` |
|     295 |  7168 | `		&pFunc->aReturnUnion,` |
|       - |  7169 | `		&iFlags,` |
|     295 |  7170 | `		&pFunc->sReturnTypeName,` |
|       - |  7171 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|       - |  7172 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|       - |  7173 | `		/* iUnionFlag */ 0,` |
|       - |  7174 | `		/* bAllowVoid */ 1,` |
|     295 |  7175 | `		nLine);` |
|     595 |  7176 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7177 | `		return SXERR_ABORT;` |
|       - |  7178 | `	}` |
|     595 |  7179 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  7180 | `		/* Error already reported */` |
|     ! 0 |  7181 | `		return SXERR_SYNTAX;` |
|       - |  7182 | `	}` |
|     595 |  7183 | `	if( rc == SXERR_SYNTAX ){` |
|       8 |  7184 | `		if( pGen->pIn < pGen->pEnd ){` |
|      11 |  7185 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  7186 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       6 |  7187 | `				&pGen->pIn->sData);` |
|       5 |  7188 | `		}else{` |
|     ! 0 |  7189 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  7190 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  7191 | `		}` |
|       8 |  7192 | `		return SXERR_SYNTAX;` |
|       - |  7193 | `	}` |
|     589 |  7194 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     589 |  7195 | `	return SXRET_OK;` |
|  170668 |  7196 | `}` |
|       - |  7197 |  |
|   51604 |  7198 | `static sxi32 GenStateCompileFunc(` |
|       - |  7199 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7200 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  7201 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  7202 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  7203 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  7204 | `	)` |
|       5 |  7205 | `{` |
|       - |  7206 | `	ph7_vm_func *pFunc;` |
|       - |  7207 | `	SyToken *pEnd;` |
|       - |  7208 | `	sxu32 nLine;` |
|       - |  7209 | `	char *zName;` |
|       - |  7210 | `	sxi32 rc;` |
|       - |  7211 | `	/* Extract line number */` |
|   51609 |  7212 | `	nLine = pGen->pIn->nLine;` |
|       - |  7213 | `	/* Jump the left parenthesis '(' */` |
|   51609 |  7214 | `	pGen->pIn++;` |
|       - |  7215 | `	/* Delimit the function signature */` |
|   51609 |  7216 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   51609 |  7217 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7218 | `		/* Syntax error */` |
|       9 |  7219 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       9 |  7220 | `		if( rc == SXERR_ABORT ){` |
|       - |  7221 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7222 | `			return SXERR_ABORT;` |
|       - |  7223 | `		}` |
|       9 |  7224 | `		pGen->pIn = pGen->pEnd;` |
|       9 |  7225 | `		return SXRET_OK;` |
|       - |  7226 | `	}` |
|       - |  7227 | `	/* Create the function state */` |
|   51603 |  7228 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   51603 |  7229 | `	if( pFunc == 0 ){` |
|     ! 0 |  7230 | `		goto OutOfMem;` |
|       - |  7231 | `	}` |
|       - |  7232 | `	/* Build the function name, prepending namespace if active */` |
|   51610 |  7233 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  7234 | `		SyBlob sFQN;` |
|       - |  7235 | `		sxu32 nLen;` |
|      16 |  7236 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  7237 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  7238 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  7239 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  7240 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  7241 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  7242 | `		SyBlobRelease(&sFQN);` |
|      16 |  7243 | `		if( zName == 0 ){` |
|     ! 0 |  7244 | `			goto OutOfMem;` |
|       - |  7245 | `		}` |
|      16 |  7246 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  7247 | `	}else{` |
|   51589 |  7248 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   51589 |  7249 | `		if( zName == 0 ){` |
|     ! 0 |  7250 | `			goto OutOfMem;` |
|       - |  7251 | `		}` |
|   51589 |  7252 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  7253 | `	}` |
|   51603 |  7254 | `	if( pGen->pIn < pEnd ){` |
|       - |  7255 | `		/* Collect function arguments */` |
|   35531 |  7256 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   35531 |  7257 | `		if( rc == SXERR_ABORT ){` |
|       - |  7258 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7259 | `			return SXERR_ABORT;` |
|       - |  7260 | `		}` |
|   17763 |  7261 | `	}` |
|       - |  7262 | `	/* Point past ')' and parse optional return type ': type' */` |
|   51603 |  7263 | `	pGen->pIn = &pEnd[1];` |
|       - |  7264 | `	{` |
|   51603 |  7265 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   51603 |  7266 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7267 | `			return SXERR_ABORT;` |
|   51603 |  7268 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       8 |  7269 | `			return SXERR_SYNTAX;` |
|       - |  7270 | `		}` |
|       - |  7271 | `	}` |
|   51597 |  7272 | `	if( bHandleClosure ){` |
|       - |  7273 | `		ph7_vm_func_closure_env sEnv;` |
|     329 |  7274 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     324 |  7275 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     179 |  7276 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      29 |  7277 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  7278 | `				/* Closure,record environment variable */` |
|      29 |  7279 | `				pGen->pIn++;` |
|      29 |  7280 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  7281 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  7282 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7283 | `						return SXERR_ABORT;` |
|       - |  7284 | `					}` |
|     ! 0 |  7285 | `				}` |
|      29 |  7286 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  7287 | `				/* Compile until we hit the first closing parenthesis */` |
|      57 |  7288 | `				while( pGen->pIn < pGen->pEnd ){` |
|      57 |  7289 | `					int iFlagsLocal = 0;` |
|      57 |  7290 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      29 |  7291 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      29 |  7292 | `						break;` |
|       - |  7293 | `					}` |
|      33 |  7294 | `					nLineLocal = pGen->pIn->nLine;` |
|      33 |  7295 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  7296 | `						/* Pass by reference,record that */` |
|     ! 0 |  7297 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  7298 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  7299 | `							);` |
|     ! 0 |  7300 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  7301 | `						pGen->pIn++;` |
|     ! 0 |  7302 | `					}` |
|      28 |  7303 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      33 |  7304 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7305 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  7306 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  7307 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  7308 | `								return SXERR_ABORT;` |
|       - |  7309 | `							}` |
|       - |  7310 | `							/* Find the closing parenthesis */` |
|     ! 0 |  7311 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  7312 | `								pGen->pIn++;` |
|     ! 0 |  7313 | `							}` |
|     ! 0 |  7314 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  7315 | `								pGen->pIn++;` |
|     ! 0 |  7316 | `							}` |
|     ! 0 |  7317 | `							break;` |
|       - |  7318 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  7319 | `					}else{` |
|       - |  7320 | `						SyString *pNameLocal;` |
|       - |  7321 | `						char *zDup;` |
|       - |  7322 | `						/* Duplicate variable name */` |
|      33 |  7323 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      33 |  7324 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      33 |  7325 | `						if( zDup ){` |
|       - |  7326 | `							/* Zero the structure */` |
|      33 |  7327 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      33 |  7328 | `							sEnv.iFlags = iFlagsLocal;` |
|      33 |  7329 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      33 |  7330 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      33 |  7331 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  7332 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  7333 | `									got_this = 1;` |
|     ! 0 |  7334 | `							}` |
|       - |  7335 | `							/* Save imported variable */` |
|      33 |  7336 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      19 |  7337 | `						}else{` |
|     ! 0 |  7338 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7339 | `							 return SXERR_ABORT;` |
|       - |  7340 | `						}` |
|       - |  7341 | `					}` |
|      33 |  7342 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      39 |  7343 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7344 | `						/* Ignore trailing commas */` |
|       7 |  7345 | `						pGen->pIn++;` |
|       1 |  7346 | `					}` |
|       5 |  7347 | `				}` |
|      29 |  7348 | `				if( !got_this ){` |
|       - |  7349 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  7350 | `					 * available to the closure environment.` |
|       - |  7351 | `					 */` |
|      29 |  7352 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      29 |  7353 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      29 |  7354 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      29 |  7355 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      29 |  7356 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      12 |  7357 | `				}` |
|      29 |  7358 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  7359 | `					/* Mark as closure */` |
|      29 |  7360 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|      12 |  7361 | `				}` |
|       - |  7362 | `				/* php 7.1+: the return type follows the use clause —` |
|       - |  7363 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|       - |  7364 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|       - |  7365 | `				 * so an unconditional call would wipe a type parsed at the` |
|       - |  7366 | `				 * legacy pre-use position. */` |
|      29 |  7367 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|       7 |  7368 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|       7 |  7369 | `					if( rcRt2 == SXERR_ABORT ){` |
|     ! 0 |  7370 | `						return SXERR_ABORT;` |
|       7 |  7371 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|     ! 0 |  7372 | `						return SXERR_SYNTAX;` |
|       - |  7373 | `					}` |
|       3 |  7374 | `				}` |
|      12 |  7375 | `		}` |
|     162 |  7376 | `	}` |
|       - |  7377 | `	/* Compile the body */` |
|   51597 |  7378 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   51597 |  7379 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7380 | `		return SXERR_ABORT;` |
|       - |  7381 | `	}` |
|   51597 |  7382 | `	if( ppFunc ){` |
|     329 |  7383 | `		*ppFunc = pFunc;` |
|     162 |  7384 | `	}` |
|   51597 |  7385 | `	rc = SXRET_OK;` |
|   51597 |  7386 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  7387 | `		/* Finally register the function */` |
|   51573 |  7388 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   25784 |  7389 | `	}` |
|   51597 |  7390 | `	if( rc == SXRET_OK ){` |
|   51597 |  7391 | `		return SXRET_OK;` |
|       - |  7392 | `	}` |
|       - |  7393 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  7394 | `OutOfMem:` |
|       - |  7395 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  7396 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  7397 | `	 */` |
|     ! 0 |  7398 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  7399 | `	return SXERR_ABORT;` |
|   25807 |  7400 | `}` |
|       - |  7401 | `/*` |
|       - |  7402 | ` * Compile a standard PHP function.` |
|       - |  7403 | ` *  Refer to the block-comment above for more information.` |
|       - |  7404 | ` */` |
|   51288 |  7405 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  7406 | `{` |
|       - |  7407 | `	SyString *pName;` |
|       - |  7408 | `	sxi32 iFlags;` |
|       - |  7409 | `	sxu32 nLine;` |
|       - |  7410 | `	sxi32 rc;` |
|       - |  7411 |  |
|   51293 |  7412 | `	nLine = pGen->pIn->nLine;` |
|   51293 |  7413 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   51293 |  7414 | `	iFlags = 0;` |
|   51293 |  7415 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7416 | `		/* Return by reference,remember that */` |
|      10 |  7417 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7418 | `		/* Jump the '&' token */` |
|      10 |  7419 | `		pGen->pIn++;` |
|       4 |  7420 | `	}` |
|   51293 |  7421 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7422 | `		/* Invalid function name */` |
|       7 |  7423 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       7 |  7424 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7425 | `			return SXERR_ABORT;` |
|       - |  7426 | `		}` |
|       - |  7427 | `		/* Sychronize with the next semi-colon or braces*/` |
|      21 |  7428 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      15 |  7429 | `			pGen->pIn++;` |
|       1 |  7430 | `		}` |
|       7 |  7431 | `		return SXRET_OK;` |
|       - |  7432 | `	}` |
|   51287 |  7433 | `	pName = &pGen->pIn->sData;` |
|   51287 |  7434 | `	nLine = pGen->pIn->nLine;` |
|       - |  7435 | `	/* Jump the function name */` |
|   51287 |  7436 | `	pGen->pIn++;` |
|   51287 |  7437 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7438 | `		/* Syntax error */` |
|       3 |  7439 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  7440 | `		if( rc == SXERR_ABORT ){` |
|       - |  7441 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7442 | `			return SXERR_ABORT;` |
|       - |  7443 | `		}` |
|       - |  7444 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  7445 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  7446 | `			pGen->pIn++;` |
|     ! 0 |  7447 | `		}` |
|       3 |  7448 | `		return SXRET_OK;` |
|       - |  7449 | `	}` |
|       - |  7450 | `	/* Compile function body */` |
|   51285 |  7451 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   51285 |  7452 | `	return rc;` |
|   25649 |  7453 | `}` |
|       - |  7454 | `/*` |
|       - |  7455 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  7456 | ` * According to the PHP language reference manual` |
|       - |  7457 | ` *  Visibility:` |
|       - |  7458 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  7459 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  7460 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  7461 | ` *  Members declared protected can be accessed only within the class` |
|       - |  7462 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  7463 | ` *  may only be accessed by the class that defines the member.` |
|       - |  7464 | ` */` |
|  371046 |  7465 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  7466 | `{` |
|  371051 |  7467 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   23183 |  7468 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  347873 |  7469 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   50031 |  7470 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  7471 | `	}` |
|       - |  7472 | `	/* Assume public by default */` |
|  297847 |  7473 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  185528 |  7474 | `}` |
|       - |  7475 | `/*` |
|       - |  7476 | ` * Compile a class constant.` |
|       - |  7477 | ` * According to the PHP language reference manual` |
|       - |  7478 | ` *  Class Constants` |
|       - |  7479 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  7480 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  7481 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  7482 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  7483 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  7484 | ` *   It's also possible for interfaces to have constants.` |
|       - |  7485 | ` * Symisc eXtension.` |
|       - |  7486 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  7487 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7488 | ` *  Example:` |
|       - |  7489 | ` *   class Test{` |
|       - |  7490 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7491 | ` *   };` |
|       - |  7492 | ` *   var_dump(TEST::MyConst);` |
|       - |  7493 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7494 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7495 | ` */` |
|       - |  7496 | `/*` |
|       - |  7497 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  7498 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  7499 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  7500 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  7501 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  7502 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  7503 | ` */` |
|     100 |  7504 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  7505 | `{` |
|       - |  7506 | `	SyToken *p0, *p1;` |
|     105 |  7507 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7508 | `		return 0;` |
|       - |  7509 | `	}` |
|     105 |  7510 | `	p0 = pGen->pIn;` |
|       - |  7511 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|     105 |  7512 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  7513 | `		return 1;` |
|       - |  7514 | `	}` |
|     105 |  7515 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  7516 | `		return 1;` |
|       - |  7517 | `	}` |
|       - |  7518 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  7519 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  7520 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|     101 |  7521 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     101 |  7522 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|     101 |  7523 | `		if( p1 ){` |
|     101 |  7524 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      34 |  7525 | `				return 1;` |
|       - |  7526 | `			}` |
|      70 |  7527 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  7528 | `				return 1;` |
|       - |  7529 | `			}` |
|      31 |  7530 | `		}` |
|      31 |  7531 | `	}` |
|      66 |  7532 | `	return 0;` |
|      55 |  7533 | `}` |
|       - |  7534 | `/*` |
|       - |  7535 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|       - |  7536 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|       - |  7537 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|       - |  7538 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|       - |  7539 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|       - |  7540 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|       - |  7541 | ` * Peek only; never consumes tokens.` |
|       - |  7542 | ` */` |
|      24 |  7543 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|       4 |  7544 | `{` |
|      28 |  7545 | `	SyToken *p = pGen->pIn;` |
|      39 |  7546 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      20 |  7547 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|       3 |  7548 | `		p++; /* skip leading unary sign(s) */` |
|       1 |  7549 | `	}` |
|      28 |  7550 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|      23 |  7551 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|       - |  7552 | `	}` |
|       6 |  7553 | `	p++;` |
|       - |  7554 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|       6 |  7555 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|      16 |  7556 | `}` |
|       - |  7557 | `/*` |
|       - |  7558 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|       - |  7559 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|       - |  7560 | `` * `$o->new`), not a `new` expression.`` |
|       - |  7561 | ` */` |
|       6 |  7562 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|       3 |  7563 | `{` |
|       - |  7564 | `	sxi32 iOp;` |
|       9 |  7565 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|     ! 0 |  7566 | `		return 0;` |
|       - |  7567 | `	}` |
|       9 |  7568 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       9 |  7569 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|       6 |  7570 | `}` |
|       - |  7571 | `/*` |
|       - |  7572 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|       - |  7573 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|       - |  7574 | ` * interface-constant and (instance/static) property-default initializers` |
|       - |  7575 | ` * ("New expressions are not supported in this context") while still allowing it` |
|       - |  7576 | ` * in global constants, parameter defaults and static-local initializers (which` |
|       - |  7577 | ` * are compiled by different functions and left untouched). The scan is` |
|       - |  7578 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|       - |  7579 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|       - |  7580 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|       - |  7581 | ` *` |
|       - |  7582 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|       - |  7583 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|       - |  7584 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|       - |  7585 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|       - |  7586 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|       - |  7587 | ` */` |
|   23672 |  7588 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|       5 |  7589 | `{` |
|   23677 |  7590 | `	SyToken *p = pGen->pIn;` |
|   23677 |  7591 | `	int iDepth = 0;` |
|   71223 |  7592 | `	while( p < pGen->pEnd ){` |
|   71223 |  7593 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   23669 |  7594 | `			break; /* end of this initializer */` |
|       - |  7595 | `		}` |
|   47559 |  7596 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   23787 |  7597 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      10 |  7598 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|       - |  7599 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|       - |  7600 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|       - |  7601 | `			 * expression. */` |
|       3 |  7602 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|       3 |  7603 | `			p++;` |
|       3 |  7604 | `			if( bArrow ){` |
|       - |  7605 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|       - |  7606 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|       3 |  7607 | `				int iBase = iDepth;` |
|      17 |  7608 | `				while( p < pGen->pEnd ){` |
|      17 |  7609 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       5 |  7610 | `						iDepth++;` |
|      15 |  7611 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       5 |  7612 | `						if( iDepth <= iBase ){` |
|     ! 0 |  7613 | `							break; /* closes an enclosing group, not the fn's own */` |
|       - |  7614 | `						}` |
|       5 |  7615 | `						iDepth--;` |
|      11 |  7616 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|       3 |  7617 | `						break;` |
|       - |  7618 | `					}` |
|      15 |  7619 | `					p++;` |
|       1 |  7620 | `				}` |
|       2 |  7621 | `			}else{` |
|       - |  7622 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|       - |  7623 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|       - |  7624 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|       - |  7625 | `				 * then skip the balanced brace block. */` |
|     ! 0 |  7626 | `				int iLocal = 0;` |
|     ! 0 |  7627 | `				while( p < pGen->pEnd ){` |
|     ! 0 |  7628 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|     ! 0 |  7629 | `						break; /* body brace */` |
|       - |  7630 | `					}` |
|     ! 0 |  7631 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  7632 | `						iLocal++;` |
|     ! 0 |  7633 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  7634 | `						if( iLocal > 0 ){` |
|     ! 0 |  7635 | `							iLocal--;` |
|     ! 0 |  7636 | `						}` |
|     ! 0 |  7637 | `					}` |
|     ! 0 |  7638 | `					p++;` |
|     ! 0 |  7639 | `				}` |
|     ! 0 |  7640 | `				if( p < pGen->pEnd ){` |
|     ! 0 |  7641 | `					int iBrace = 0; /* p is on the body '{' */` |
|     ! 0 |  7642 | `					while( p < pGen->pEnd ){` |
|     ! 0 |  7643 | `						if( p->nType & PH7_TK_OCB ){` |
|     ! 0 |  7644 | `							iBrace++;` |
|     ! 0 |  7645 | `						}else if( p->nType & PH7_TK_CCB ){` |
|     ! 0 |  7646 | `							iBrace--;` |
|     ! 0 |  7647 | `							if( iBrace == 0 ){` |
|     ! 0 |  7648 | `								p++;` |
|     ! 0 |  7649 | `								break;` |
|       - |  7650 | `							}` |
|     ! 0 |  7651 | `						}` |
|     ! 0 |  7652 | `						p++;` |
|     ! 0 |  7653 | `					}` |
|     ! 0 |  7654 | `				}` |
|       - |  7655 | `			}` |
|       3 |  7656 | `			continue;` |
|       - |  7657 | `		}` |
|   47557 |  7658 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      69 |  7659 | `			iDepth++;` |
|   47525 |  7660 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      67 |  7661 | `			if( iDepth > 0 ){` |
|      67 |  7662 | `				iDepth--;` |
|      31 |  7663 | `			}` |
|   47462 |  7664 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|   23647 |  7665 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|       - |  7666 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|       - |  7667 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|       - |  7668 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|      11 |  7669 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|      11 |  7670 | `				return 1;` |
|       - |  7671 | `			}` |
|     ! 0 |  7672 | `		}` |
|   47549 |  7673 | `		p++;` |
|       5 |  7674 | `	}` |
|   23669 |  7675 | `	return 0;` |
|   11841 |  7676 | `}` |
|       - |  7677 | `/*` |
|       - |  7678 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  7679 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  7680 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  7681 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  7682 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  7683 | ` * share the same backing.` |
|       - |  7684 | ` */` |
|     214 |  7685 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  7686 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  7687 | `{` |
|     219 |  7688 | `	pAttr->nType = nType;` |
|     219 |  7689 | `	pAttr->sClass = *pClass;` |
|     219 |  7690 | `	pAttr->sTypeName = *pTypeName;` |
|     219 |  7691 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7692 | `		sxu32 i;` |
|      67 |  7693 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      47 |  7694 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      47 |  7695 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      26 |  7696 | `		}` |
|      10 |  7697 | `	}` |
|     219 |  7698 | `}` |
|     100 |  7699 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7700 | `{` |
|     105 |  7701 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7702 | `	SySet *pInstrContainer;` |
|       - |  7703 | `	ph7_class_attr *pCons;` |
|       - |  7704 | `	SyString *pName;` |
|       - |  7705 | `	sxi32 rc;` |
|     105 |  7706 | `	sxu32 nType = 0;` |
|       - |  7707 | `	SyString sTypeClass;` |
|       - |  7708 | `	SyString sTypeText;` |
|       - |  7709 | `	SySet aUnionAlts;` |
|     105 |  7710 | `	sxi32 iTypeFlags = 0;` |
|     105 |  7711 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|     105 |  7712 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|     105 |  7713 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7714 | `	/* Extract visibility level */` |
|     105 |  7715 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7716 | `	/* Mark as constant */` |
|     105 |  7717 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|     105 |  7718 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  7719 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  7720 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|     124 |  7721 | `	if( GenStateClassConstHasType(pGen) ){` |
|      61 |  7722 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      38 |  7723 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  7724 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  7725 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  7726 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  7727 | `		 * and success paths release. */` |
|      42 |  7728 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7729 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7730 | `			goto Synchronize;` |
|      42 |  7731 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7732 | `			return SXERR_ABORT;` |
|      42 |  7733 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7734 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7735 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  7736 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7737 | `				return SXERR_ABORT;` |
|       - |  7738 | `			}` |
|     ! 0 |  7739 | `			goto Synchronize;` |
|       - |  7740 | `		}` |
|      42 |  7741 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      19 |  7742 | `	}` |
|      50 |  7743 | `loop:` |
|     107 |  7744 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7745 | `		/* Invalid constant name */` |
|     ! 0 |  7746 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  7747 | `		if( rc == SXERR_ABORT ){` |
|       - |  7748 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7749 | `			return SXERR_ABORT;` |
|       - |  7750 | `		}` |
|     ! 0 |  7751 | `		goto Synchronize;` |
|       - |  7752 | `	}` |
|       - |  7753 | `	/* Peek constant name */` |
|     107 |  7754 | `	pName = &pGen->pIn->sData;` |
|       - |  7755 | `	/* Make sure the constant name isn't reserved */` |
|     107 |  7756 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  7757 | `		/* Reserved constant name */` |
|     ! 0 |  7758 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  7759 | `		if( rc == SXERR_ABORT ){` |
|       - |  7760 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7761 | `			return SXERR_ABORT;` |
|       - |  7762 | `		}` |
|     ! 0 |  7763 | `		goto Synchronize;` |
|       - |  7764 | `	}` |
|       - |  7765 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|     107 |  7766 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      61 |  7767 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      38 |  7768 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      19 |  7769 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      42 |  7770 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7771 | `			return SXERR_ABORT;` |
|      42 |  7772 | `		}else if( rc != SXRET_OK ){` |
|       3 |  7773 | `			goto Synchronize;` |
|       - |  7774 | `		}` |
|      18 |  7775 | `	}` |
|       - |  7776 | `	/* Advance the stream cursor */` |
|     105 |  7777 | `	pGen->pIn++;` |
|     105 |  7778 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  7779 | `		/* Invalid declaration */` |
|     ! 0 |  7780 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  7781 | `		if( rc == SXERR_ABORT ){` |
|       - |  7782 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7783 | `			return SXERR_ABORT;` |
|       - |  7784 | `		}` |
|     ! 0 |  7785 | `		goto Synchronize;` |
|       - |  7786 | `	}` |
|     105 |  7787 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  7788 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|       - |  7789 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|       - |  7790 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|       - |  7791 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|     112 |  7792 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|      39 |  7793 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|       8 |  7794 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7795 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|       2 |  7796 | `			&pClass->sName,pName,&sTypeText);` |
|       6 |  7797 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7798 | `			return SXERR_ABORT;` |
|       - |  7799 | `		}` |
|       6 |  7800 | `		goto Synchronize;` |
|       - |  7801 | `	}` |
|       - |  7802 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|       - |  7803 | `	 * constant initializer ("New expressions are not supported in this context").` |
|       - |  7804 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|     101 |  7805 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|       5 |  7806 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7807 | `			"New expressions are not supported in this context");` |
|       5 |  7808 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7809 | `			return SXERR_ABORT;` |
|       - |  7810 | `		}` |
|       5 |  7811 | `		goto Synchronize;` |
|       - |  7812 | `	}` |
|       - |  7813 | `	/* Allocate a new class attribute */` |
|      97 |  7814 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      97 |  7815 | `	if( pCons == 0 ){` |
|     ! 0 |  7816 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7817 | `		return SXERR_ABORT;` |
|       - |  7818 | `	}` |
|      97 |  7819 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      35 |  7820 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      16 |  7821 | `	}` |
|       - |  7822 | `	/* Swap bytecode container */` |
|      97 |  7823 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 |  7824 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  7825 | `	/* Compile constant value.` |
|       - |  7826 | `	 */` |
|      97 |  7827 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      97 |  7828 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  7829 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  7830 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7831 | `			return SXERR_ABORT;` |
|       - |  7832 | `		}` |
|       1 |  7833 | `	}` |
|       - |  7834 | `	/* Emit the done instruction */` |
|      97 |  7835 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      97 |  7836 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      97 |  7837 | `	if( rc == SXERR_ABORT ){` |
|       - |  7838 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7839 | `		return SXERR_ABORT;` |
|       - |  7840 | `	}` |
|       - |  7841 | `	/* All done,install the constant */` |
|      97 |  7842 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      97 |  7843 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7844 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7845 | `		return SXERR_ABORT;` |
|       - |  7846 | `	}` |
|      97 |  7847 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7848 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  7849 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  7850 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7851 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7852 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7853 | `				pTok--;` |
|     ! 0 |  7854 | `			}` |
|     ! 0 |  7855 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7856 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  7857 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7858 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7859 | `				return SXERR_ABORT;` |
|       - |  7860 | `			}` |
|     ! 0 |  7861 | `		}else{` |
|       3 |  7862 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  7863 | `				goto loop;` |
|       - |  7864 | `			}` |
|       - |  7865 | `		}` |
|     ! 0 |  7866 | `	}` |
|      95 |  7867 | `	SySetRelease(&aUnionAlts);` |
|      95 |  7868 | `	return SXRET_OK;` |
|       5 |  7869 | `Synchronize:` |
|      13 |  7870 | `	SySetRelease(&aUnionAlts);` |
|       - |  7871 | `	/* Synchronize with the first semi-colon */` |
|      45 |  7872 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      35 |  7873 | `		pGen->pIn++;` |
|       3 |  7874 | `	}` |
|      13 |  7875 | `	return SXERR_CORRUPT;` |
|      55 |  7876 | `}` |
|       - |  7877 | `/*` |
|       - |  7878 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  7879 | ` * According to the PHP language reference manual` |
|       - |  7880 | ` *  Properties` |
|       - |  7881 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  7882 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  7883 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  7884 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  7885 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  7886 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  7887 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  7888 | ` * Symisc eXtension.` |
|       - |  7889 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  7890 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7891 | ` *  Example:` |
|       - |  7892 | ` *   class Test{` |
|       - |  7893 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7894 | ` *   };` |
|       - |  7895 | ` *   var_dump(TEST::myVar);` |
|       - |  7896 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7897 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7898 | ` */` |
|       - |  7899 | `/*` |
|       - |  7900 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  7901 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  7902 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  7903 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  7904 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  7905 | ` */` |
|  201062 |  7906 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  7907 | `{` |
|  201067 |  7908 | `	SyToken *p = pStart;` |
|  201067 |  7909 | `	int bFirst = 1;` |
|  201067 |  7910 | `	if( p >= pEnd ) return 0;` |
|       - |  7911 | ``	/* Optional nullable `?` shorthand. */`` |
|  201067 |  7912 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      19 |  7913 | `		p++;` |
|      19 |  7914 | `		if( p >= pEnd ) return 0;` |
|       8 |  7915 | `	}` |
|       - |  7916 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  7917 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  7918 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  7919 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|  100531 |  7920 | `	for(;;){` |
|  201085 |  7921 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  7922 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  7923 | `			p++;` |
|       9 |  7924 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  7925 | `			if( p >= pEnd ) return 0;` |
|       3 |  7926 | `			p++; /* skip ')' */` |
|       2 |  7927 | `		}else{` |
|       - |  7928 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - |  7929 | ``			 * then any `&`-joined intersection members. */`` |
|  201083 |  7930 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  201083 |  7931 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7932 | `				return 0;` |
|       - |  7933 | `			}` |
|       - |  7934 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - |  7935 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - |  7936 | `			 * may still appear at the initial dispatch site). */` |
|  201083 |  7937 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  201037 |  7938 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  201110 |  7939 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|   11762 |  7940 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  200881 |  7941 | `					return 0;` |
|       - |  7942 | `				}` |
|      78 |  7943 | `			}` |
|     207 |  7944 | `			p++;` |
|     209 |  7945 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7946 | `				p += 2;` |
|       1 |  7947 | `			}` |
|     306 |  7948 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     210 |  7949 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7950 | `				p++; /* skip '&' */` |
|       3 |  7951 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       3 |  7952 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       3 |  7953 | `				p++;` |
|       3 |  7954 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  7955 | `					p += 2;` |
|     ! 0 |  7956 | `				}` |
|       1 |  7957 | `			}` |
|       - |  7958 | `		}` |
|     209 |  7959 | `		bFirst = 0;` |
|     204 |  7960 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      23 |  7961 | `			&& p->sData.zString[0] == '\|' ){` |
|      23 |  7962 | ``			p++; /* next `\|`-separated part */`` |
|      23 |  7963 | `			continue;` |
|       - |  7964 | `		}` |
|     191 |  7965 | `		break;` |
|     ! 0 |  7966 | `	}` |
|     191 |  7967 | `	if( p >= pEnd ) return 0;` |
|     191 |  7968 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|  100536 |  7969 | `}` |
|       - |  7970 |  |
|       - |  7971 | `/*` |
|       - |  7972 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  7973 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  7974 | ` * if not). Recognized forms:` |
|       - |  7975 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  7976 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  7977 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  7978 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  7979 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  7980 | ` * on unrecoverable error.` |
|       - |  7981 | ` *` |
|       - |  7982 | ` * When a type is parsed:` |
|       - |  7983 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  7984 | ` *   *pClass is set to the class name (for class types)` |
|       - |  7985 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  7986 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  7987 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  7988 | ` */` |
|     186 |  7989 | `static sxi32 GenStateParsePropertyType(` |
|       - |  7990 | `	ph7_gen_state *pGen,` |
|       - |  7991 | `	sxu32 *pnType,` |
|       - |  7992 | `	SyString *pClass,` |
|       - |  7993 | `	sxi32 *piTypeFlags,` |
|       - |  7994 | `	SyString *pTypeText,` |
|       - |  7995 | `	SySet *pAlts` |
|       5 |  7996 | `){` |
|     191 |  7997 | `	sxi32 iFlags = 0;` |
|       - |  7998 | `	sxi32 rc;` |
|     191 |  7999 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  8000 | `		return SXRET_OK;` |
|       - |  8001 | `	}` |
|       - |  8002 | `	/* If the first token is '$', there's no type */` |
|     191 |  8003 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  8004 | `		return SXRET_OK;` |
|       - |  8005 | `	}` |
|     191 |  8006 | `	rc = GenStateParseUnionTypeDecl(` |
|      93 |  8007 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  8008 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  8009 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  8010 | `		/* bAllowVoid */ 0,` |
|     186 |  8011 | `		pGen->pIn->nLine);` |
|     191 |  8012 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8013 | `		return rc;` |
|       - |  8014 | `	}` |
|       - |  8015 | `	/* Verify next token is '$' (start of property name) */` |
|     191 |  8016 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  8017 | `		return SXERR_SYNTAX;` |
|       - |  8018 | `	}` |
|     191 |  8019 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     191 |  8020 | `	return SXRET_OK;` |
|      98 |  8021 | `}` |
|       - |  8022 |  |
|       - |  8023 | `/*` |
|       - |  8024 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  8025 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  8026 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  8027 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  8028 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  8029 | ` * by the type parser itself before reaching here.` |
|       - |  8030 | ` *` |
|       - |  8031 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  8032 | ` * use in the error message.` |
|       - |  8033 | ` */` |
|     346 |  8034 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  8035 | `	sxu32 nType,` |
|       - |  8036 | `	const SyString *pClass,` |
|       - |  8037 | `	const char **pzName,` |
|       - |  8038 | `	sxu32 *pnName)` |
|       5 |  8039 | `{` |
|       - |  8040 | `	const char *z;` |
|       - |  8041 | `	sxu32 n;` |
|     351 |  8042 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     297 |  8043 | `		return 0;` |
|       - |  8044 | `	}` |
|      59 |  8045 | `	z = pClass->zString;` |
|      59 |  8046 | `	n = pClass->nByte;` |
|      59 |  8047 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  8048 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  8049 | `	}` |
|       - |  8050 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  8051 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  8052 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      52 |  8053 | `	return 0;` |
|     178 |  8054 | `}` |
|       - |  8055 |  |
|       - |  8056 | `/*` |
|       - |  8057 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  8058 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  8059 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  8060 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  8061 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  8062 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  8063 | ` *` |
|       - |  8064 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  8065 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  8066 | ` */` |
|     288 |  8067 | `static sxi32 GenStateValidateMemberType(` |
|       - |  8068 | `	ph7_gen_state *pGen,` |
|       - |  8069 | `	ph7_class *pClass,` |
|       - |  8070 | `	const SyString *pMemberName,` |
|       - |  8071 | `	sxu32 nType,` |
|       - |  8072 | `	const SyString *pTypeClass,` |
|       - |  8073 | `	const SyString *pTypeText,` |
|       - |  8074 | `	SySet *pUnionAlts,` |
|       - |  8075 | `	const char *zErrFmt,` |
|       - |  8076 | `	sxu32 nLine)` |
|       5 |  8077 | `{` |
|     293 |  8078 | `	const char *zBad = 0;` |
|     293 |  8079 | `	sxu32 nBad = 0;` |
|       - |  8080 | `	SyString sFallback;` |
|       - |  8081 | `	const SyString *pBad;` |
|       - |  8082 | `	sxi32 rc;` |
|     293 |  8083 | `	int bDisallowed = 0;` |
|     293 |  8084 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  8085 | `		bDisallowed = 1;` |
|     291 |  8086 | `	}else if( pUnionAlts ){` |
|       - |  8087 | `		sxu32 i;` |
|      89 |  8088 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      63 |  8089 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      63 |  8090 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  8091 | `				bDisallowed = 1;` |
|       3 |  8092 | `				break;` |
|       - |  8093 | `			}` |
|      33 |  8094 | `		}` |
|      14 |  8095 | `	}` |
|     293 |  8096 | `	if( !bDisallowed ){` |
|     287 |  8097 | `		return SXRET_OK;` |
|       - |  8098 | `	}` |
|       - |  8099 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  8100 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  8101 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  8102 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  8103 | `		pBad = pTypeText;` |
|       5 |  8104 | `	}else{` |
|     ! 0 |  8105 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  8106 | `		pBad = &sFallback;` |
|       - |  8107 | `	}` |
|      11 |  8108 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  8109 | `		zErrFmt,` |
|       3 |  8110 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  8111 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8112 | `		return SXERR_ABORT;` |
|       - |  8113 | `	}` |
|       8 |  8114 | `	return SXERR_SYNTAX;` |
|     149 |  8115 | `}` |
|       - |  8116 | `/*` |
|       - |  8117 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  8118 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  8119 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  8120 | ` * than promoted to a lexer keyword.` |
|       - |  8121 | ` */` |
| 1786628 |  8122 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  8123 | `{` |
| 1823259 |  8124 | `	return (pTok->nType & PH7_TK_ID)` |
|  929940 |  8125 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1823254 |  8126 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  8127 | `}` |
|   81434 |  8128 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  8129 | `{` |
|   81439 |  8130 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8131 | `	ph7_class_attr *pAttr;` |
|       - |  8132 | `	SyString *pName;` |
|       - |  8133 | `	sxi32 rc;` |
|   81439 |  8134 | `	sxu32 nType = 0;` |
|       - |  8135 | `	SyString sTypeClass;` |
|       - |  8136 | `	SyString sTypeText;` |
|       - |  8137 | `	SySet aUnionAlts;` |
|   81439 |  8138 | `	sxi32 iTypeFlags = 0;` |
|   81439 |  8139 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   81439 |  8140 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   81439 |  8141 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  8142 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  8143 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  8144 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   81439 |  8145 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  8146 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  8147 | `	}` |
|       - |  8148 | `	/* Extract visibility level */` |
|   81439 |  8149 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  8150 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   81532 |  8151 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     191 |  8152 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     191 |  8153 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  8154 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  8155 | `			goto Synchronize;` |
|     191 |  8156 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  8157 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8158 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  8159 | `				&pGen->pIn->sData);` |
|     ! 0 |  8160 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8161 | `				return SXERR_ABORT;` |
|       - |  8162 | `			}` |
|     ! 0 |  8163 | `			goto Synchronize;` |
|     191 |  8164 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  8165 | `			return SXERR_ABORT;` |
|       - |  8166 | `		}` |
|      93 |  8167 | `	}` |
|     ! 0 |  8168 | `loop:` |
|   81443 |  8169 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  8170 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  8171 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8172 | `			return SXERR_ABORT;` |
|       - |  8173 | `		}` |
|     ! 0 |  8174 | `		goto Synchronize;` |
|       - |  8175 | `	}` |
|   81443 |  8176 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   81443 |  8177 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  8178 | `		/* Invalid attribute name */` |
|     ! 0 |  8179 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  8180 | `		if( rc == SXERR_ABORT ){` |
|       - |  8181 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8182 | `			return SXERR_ABORT;` |
|       - |  8183 | `		}` |
|     ! 0 |  8184 | `		goto Synchronize;` |
|       - |  8185 | `	}` |
|       - |  8186 | `	/* Peek attribute name */` |
|   81443 |  8187 | `	pName = &pGen->pIn->sData;` |
|       - |  8188 | `	/* Advance the stream cursor */` |
|   81443 |  8189 | `	pGen->pIn++;` |
|   81443 |  8190 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  8191 | `		/* Invalid declaration */` |
|       3 |  8192 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  8193 | `		if( rc == SXERR_ABORT ){` |
|       - |  8194 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8195 | `			return SXERR_ABORT;` |
|       - |  8196 | `		}` |
|       3 |  8197 | `		goto Synchronize;` |
|       - |  8198 | `	}` |
|       - |  8199 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  8200 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   81441 |  8201 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  8202 | `		const char *zRoErr = 0;` |
|      39 |  8203 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  8204 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  8205 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  8206 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  8207 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  8208 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  8209 | `		}` |
|      39 |  8210 | `		if( zRoErr ){` |
|      13 |  8211 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  8212 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8213 | `				return SXERR_ABORT;` |
|       - |  8214 | `			}` |
|      13 |  8215 | `			goto Synchronize;` |
|       - |  8216 | `		}` |
|      12 |  8217 | `	}` |
|       - |  8218 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  8219 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  8220 | `	 * by the type parser. */` |
|   81431 |  8221 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     281 |  8222 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  8223 | `			&sTypeText,` |
|     184 |  8224 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      92 |  8225 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     189 |  8226 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8227 | `			return SXERR_ABORT;` |
|     189 |  8228 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  8229 | `			goto Synchronize;` |
|       - |  8230 | `		}` |
|      92 |  8231 | `	}` |
|       - |  8232 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   81431 |  8233 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  8234 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8235 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  8236 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8237 | `			return SXERR_ABORT;` |
|       - |  8238 | `		}` |
|       3 |  8239 | `		goto Synchronize;` |
|       - |  8240 | `	}` |
|       - |  8241 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|       - |  8242 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|       - |  8243 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|       - |  8244 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|       - |  8245 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|       - |  8246 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   81429 |  8247 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|       6 |  8248 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8249 | `			"New expressions are not supported in this context");` |
|       6 |  8250 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8251 | `			return SXERR_ABORT;` |
|       - |  8252 | `		}` |
|       6 |  8253 | `		goto Synchronize;` |
|       - |  8254 | `	}` |
|       - |  8255 | `	/* Allocate a new class attribute */` |
|   81425 |  8256 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   81425 |  8257 | `	if( pAttr == 0 ){` |
|     ! 0 |  8258 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  8259 | `		return SXERR_ABORT;` |
|       - |  8260 | `	}` |
|   81425 |  8261 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     187 |  8262 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      91 |  8263 | `	}` |
|   81425 |  8264 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  8265 | `		SySet *pInstrContainer;` |
|   23577 |  8266 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  8267 | `		/* Swap bytecode container */` |
|   23577 |  8268 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   23577 |  8269 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  8270 | `		/* Compile attribute value.` |
|       - |  8271 | `		 */` |
|   23577 |  8272 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   23577 |  8273 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  8274 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  8275 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8276 | `				return SXERR_ABORT;` |
|       - |  8277 | `			}` |
|     ! 0 |  8278 | `		}` |
|       - |  8279 | `		/* Emit the done instruction */` |
|   23577 |  8280 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   23577 |  8281 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   11786 |  8282 | `	}` |
|       - |  8283 | `	/* All done,install the attribute */` |
|   81425 |  8284 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   81425 |  8285 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8286 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8287 | `		return SXERR_ABORT;` |
|       - |  8288 | `	}` |
|   81425 |  8289 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  8290 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  8291 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  8292 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  8293 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  8294 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  8295 | `				pTok--;` |
|     ! 0 |  8296 | `			}` |
|     ! 0 |  8297 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8298 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8299 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  8300 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8301 | `				return SXERR_ABORT;` |
|       - |  8302 | `			}` |
|     ! 0 |  8303 | `		}else{` |
|       5 |  8304 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  8305 | `				goto loop;` |
|       - |  8306 | `			}` |
|       - |  8307 | `		}` |
|     ! 0 |  8308 | `	}` |
|   81421 |  8309 | `	SySetRelease(&aUnionAlts);` |
|   81421 |  8310 | `	return SXRET_OK;` |
|       9 |  8311 | `Synchronize:` |
|       - |  8312 | `	/* Synchronize with the first semi-colon */` |
|      56 |  8313 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      37 |  8314 | `		pGen->pIn++;` |
|       3 |  8315 | `	}` |
|      22 |  8316 | `	SySetRelease(&aUnionAlts);` |
|      22 |  8317 | `	return SXERR_CORRUPT;` |
|   40722 |  8318 | `}` |
|       - |  8319 | `/*` |
|       - |  8320 | ` * Compile a class method.` |
|       - |  8321 | ` *` |
|       - |  8322 | ` * Refer to the official documentation for more information` |
|       - |  8323 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  8324 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  8325 | ` * overloading and many more.` |
|       - |  8326 | ` */` |
|  289512 |  8327 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  8328 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  8329 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  8330 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  8331 | `	int doBody,          /* TRUE to process method body */` |
|       - |  8332 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  8333 | `	)` |
|       5 |  8334 | `{` |
|  289517 |  8335 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8336 | `	ph7_class_method *pMeth;` |
|       - |  8337 | `	sxi32 iFuncFlags;` |
|       - |  8338 | `	SyString *pName;` |
|       - |  8339 | `	SyToken *pEnd;` |
|       - |  8340 | `	sxi32 rc;` |
|       - |  8341 | `	/* Extract visibility level */` |
|  289517 |  8342 | `	iProtection = GetProtectionLevel(iProtection);` |
|  289517 |  8343 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  289517 |  8344 | `	iFuncFlags = 0;` |
|  289517 |  8345 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8346 | `		/* Invalid method name */` |
|     ! 0 |  8347 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  8348 | `		if( rc == SXERR_ABORT ){` |
|       - |  8349 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8350 | `			return SXERR_ABORT;` |
|       - |  8351 | `		}` |
|     ! 0 |  8352 | `		goto Synchronize;` |
|       - |  8353 | `	}` |
|  289517 |  8354 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  8355 | `		/* Return by reference,remember that */` |
|     ! 0 |  8356 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  8357 | `		/* Jump the '&' token */` |
|     ! 0 |  8358 | `		pGen->pIn++;` |
|     ! 0 |  8359 | `	}` |
|  289517 |  8360 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  8361 | `		/* Invalid method name */` |
|     ! 0 |  8362 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  8363 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8364 | `			return SXERR_ABORT;` |
|       - |  8365 | `		}` |
|     ! 0 |  8366 | `		goto Synchronize;` |
|       - |  8367 | `	}` |
|       - |  8368 | `	/* Peek method name */` |
|  289517 |  8369 | `	pName = &pGen->pIn->sData;` |
|  289517 |  8370 | `	nLine = pGen->pIn->nLine;` |
|       - |  8371 | `	/* Jump the method name */` |
|  289517 |  8372 | `	pGen->pIn++;` |
|  289517 |  8373 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  8374 | `		/* Abstract method */` |
|  100007 |  8375 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  8376 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8377 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  8378 | `				&pClass->sName,pName);` |
|     ! 0 |  8379 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8380 | `				return SXERR_ABORT;` |
|       - |  8381 | `			}` |
|     ! 0 |  8382 | `		}` |
|       - |  8383 | `		/* Assemble method signature only */` |
|  100007 |  8384 | `		doBody = FALSE;` |
|   50001 |  8385 | `	}` |
|  289517 |  8386 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  8387 | `		/* Syntax error */` |
|     ! 0 |  8388 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  8389 | `		if( rc == SXERR_ABORT ){` |
|       - |  8390 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8391 | `			return SXERR_ABORT;` |
|       - |  8392 | `		}` |
|     ! 0 |  8393 | `		goto Synchronize;` |
|       - |  8394 | `	}` |
|       - |  8395 | `	/* Allocate a new class_method instance */` |
|  289517 |  8396 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  289517 |  8397 | `	if( pMeth == 0 ){` |
|     ! 0 |  8398 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8399 | `		return SXERR_ABORT;` |
|       - |  8400 | `	}` |
|       - |  8401 | `	/* Jump the left parenthesis '(' */` |
|  289517 |  8402 | `	pGen->pIn++;` |
|  289517 |  8403 | `	pEnd = 0; /* cc warning */` |
|       - |  8404 | `	/* Delimit the method signature */` |
|  289517 |  8405 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  289517 |  8406 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8407 | `		/* Syntax error */` |
|       3 |  8408 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  8409 | `		if( rc == SXERR_ABORT ){` |
|       - |  8410 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8411 | `			return SXERR_ABORT;` |
|       - |  8412 | `		}` |
|       3 |  8413 | `		goto Synchronize;` |
|       - |  8414 | `	}` |
|       - |  8415 | `	{` |
|  289515 |  8416 | `		int bIsCtor = 0;` |
|  289515 |  8417 | `		int bAbstractCtor = 0;` |
|  422648 |  8418 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  171795 |  8419 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  277898 |  8420 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   23239 |  8421 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  8422 | `				bAbstractCtor = 1;` |
|       2 |  8423 | `			}else{` |
|   23237 |  8424 | `				bIsCtor = 1;` |
|       - |  8425 | `			}` |
|   11617 |  8426 | `		}` |
|  289515 |  8427 | `		if( pGen->pIn < pEnd ){` |
|       - |  8428 | `			/* Collect method arguments */` |
|   77391 |  8429 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   77391 |  8430 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8431 | `				return SXERR_ABORT;` |
|       - |  8432 | `			}` |
|   38693 |  8433 | `		}` |
|       - |  8434 | `	}` |
|       - |  8435 | `	/* Point past ')' and parse optional return type ': type' */` |
|  289515 |  8436 | `	pGen->pIn = &pEnd[1];` |
|       - |  8437 | `	{` |
|  289515 |  8438 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  289515 |  8439 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  8440 | `			return SXERR_ABORT;` |
|  289515 |  8441 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  8442 | `			goto Synchronize;` |
|       - |  8443 | `		}` |
|       - |  8444 | `	}` |
|       - |  8445 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  8446 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  8447 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  8448 | `	{` |
|  289515 |  8449 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  8450 | `		sxu32 i;` |
|  420837 |  8451 | `		for( i = 0; i < nArg; i++ ){` |
|  131337 |  8452 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  8453 | `			ph7_class_attr *pAttr;` |
|  131337 |  8454 | `			sxi32 iAttrFlags = 0;` |
|       - |  8455 | `			int bArgTyped;` |
|  131337 |  8456 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  131265 |  8457 | `				continue;` |
|       - |  8458 | `			}` |
|       - |  8459 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - |  8460 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - |  8461 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      53 |  8462 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      78 |  8463 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|      77 |  8464 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  8465 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8466 | `					"Cannot declare variadic promoted property");` |
|       3 |  8467 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8468 | `					return SXERR_ABORT;` |
|       - |  8469 | `				}` |
|       3 |  8470 | `				goto Synchronize;` |
|       - |  8471 | `			}` |
|       - |  8472 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  8473 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  8474 | `			 * appear as an alternative of a union type. */` |
|      75 |  8475 | `			if( bArgTyped ){` |
|     104 |  8476 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      66 |  8477 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      66 |  8478 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      33 |  8479 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      71 |  8480 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8481 | `					return SXERR_ABORT;` |
|      71 |  8482 | `				}else if( rc != SXRET_OK ){` |
|       6 |  8483 | `					goto Synchronize;` |
|       - |  8484 | `				}` |
|      31 |  8485 | `			}` |
|       - |  8486 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      71 |  8487 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  8488 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8489 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  8490 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8491 | `					return SXERR_ABORT;` |
|       - |  8492 | `				}` |
|       3 |  8493 | `				goto Synchronize;` |
|       - |  8494 | `			}` |
|      69 |  8495 | `			if( bArgTyped ){` |
|      65 |  8496 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      30 |  8497 | `			}` |
|      69 |  8498 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  8499 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  8500 | `			}` |
|      69 |  8501 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 |  8502 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 |  8503 | `			}` |
|      69 |  8504 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  8505 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  8506 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      26 |  8507 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  8508 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8509 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  8510 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8511 | `						return SXERR_ABORT;` |
|       - |  8512 | `					}` |
|       3 |  8513 | `					goto Synchronize;` |
|       - |  8514 | `				}` |
|      24 |  8515 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|      10 |  8516 | `			}` |
|      67 |  8517 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      67 |  8518 | `			if( pAttr == 0 ){` |
|     ! 0 |  8519 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8520 | `				return SXERR_ABORT;` |
|       - |  8521 | `			}` |
|      67 |  8522 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      65 |  8523 | `				pAttr->nType = pArg->nType;` |
|      65 |  8524 | `				pAttr->sClass = pArg->sClass;` |
|      65 |  8525 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      65 |  8526 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  8527 | `					sxu32 k;` |
|      20 |  8528 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 |  8529 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 |  8530 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 |  8531 | `					}` |
|       3 |  8532 | `				}` |
|      30 |  8533 | `			}` |
|      67 |  8534 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      67 |  8535 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8536 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8537 | `				return SXERR_ABORT;` |
|       - |  8538 | `			}` |
|      36 |  8539 | `		}` |
|       - |  8540 | `	}` |
|  289505 |  8541 | `	if( doBody ){` |
|       - |  8542 | `		/* Compile method body */` |
|  189503 |  8543 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  189503 |  8544 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8545 | `			return SXERR_ABORT;` |
|       - |  8546 | `		}` |
|   94754 |  8547 | `	}else{` |
|       - |  8548 | `		/* Only method signature is allowed */` |
|  100007 |  8549 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  8550 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8551 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  8552 | `				if( rc == SXERR_ABORT ){` |
|       - |  8553 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8554 | `					return SXERR_ABORT;` |
|       - |  8555 | `				}` |
|     ! 0 |  8556 | `				return SXERR_CORRUPT;` |
|       - |  8557 | `			}` |
|       - |  8558 | `	}` |
|       - |  8559 | `	/* All done,install the method */` |
|  289505 |  8560 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  289505 |  8561 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8562 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8563 | `		return SXERR_ABORT;` |
|       - |  8564 | `	}` |
|  289505 |  8565 | `	return SXRET_OK;` |
|       6 |  8566 | `Synchronize:` |
|       - |  8567 | `	/* Synchronize with the first semi-colon */` |
|      40 |  8568 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  8569 | `		pGen->pIn++;` |
|       4 |  8570 | `	}` |
|      16 |  8571 | `	return SXERR_CORRUPT;` |
|  144761 |  8572 | `}` |
|       - |  8573 | `/*` |
|       - |  8574 | ` * Compile an object interface.` |
|       - |  8575 | ` *  According to the PHP language reference manual` |
|       - |  8576 | ` *   Object Interfaces:` |
|       - |  8577 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  8578 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  8579 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  8580 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  8581 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  8582 | ` */` |
|   42368 |  8583 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  8584 | `{` |
|   42373 |  8585 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8586 | `	ph7_class *pClass,*pBase;` |
|       - |  8587 | `	SyToken *pEnd,*pTmp;` |
|       - |  8588 | `	SyString *pName;` |
|       - |  8589 | `	sxi32 nKwrd;` |
|       - |  8590 | `	sxi32 rc;` |
|       - |  8591 | `	/* Jump the 'interface' keyword */` |
|   42373 |  8592 | `	pGen->pIn++;` |
|       - |  8593 | `	/* Extract interface name */` |
|   42373 |  8594 | `	pName = &pGen->pIn->sData;` |
|       - |  8595 | `	/* Advance the stream cursor */` |
|   42373 |  8596 | `	pGen->pIn++;` |
|       - |  8597 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8598 | `		SyBlob sFQN;` |
|       - |  8599 | `		SyString sFQNStr;` |
|   42373 |  8600 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   42373 |  8601 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   42373 |  8602 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   42373 |  8603 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   42373 |  8604 | `		SyBlobRelease(&sFQN);` |
|       - |  8605 | `	}` |
|   42373 |  8606 | `	if( pClass == 0 ){` |
|     ! 0 |  8607 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8608 | `		return SXERR_ABORT;` |
|       - |  8609 | `	}` |
|       - |  8610 | `	/* Mark as an interface */` |
|   42373 |  8611 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  8612 | `	/* Assume no base class is given */` |
|   42373 |  8613 | `	pBase = 0;` |
|   42373 |  8614 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   11545 |  8615 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   11545 |  8616 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  8617 | `			SyBlob sResolved;` |
|       - |  8618 | `			SyString sBaseName;` |
|       - |  8619 | `			sxu32 nRefLine;` |
|       - |  8620 | `			/* Extract base interface */` |
|   11545 |  8621 | `			pGen->pIn++;` |
|   11545 |  8622 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   11545 |  8623 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   11545 |  8624 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8625 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  8626 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8627 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  8628 | `					pName);` |
|     ! 0 |  8629 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8630 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8631 | `					return SXERR_ABORT;` |
|       - |  8632 | `				}` |
|     ! 0 |  8633 | `				return SXRET_OK;` |
|       - |  8634 | `			}` |
|   17315 |  8635 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   11540 |  8636 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   11545 |  8637 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8638 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8639 | `			/* Only interfaces is allowed */` |
|   11545 |  8640 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8641 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8642 | `			}` |
|   11545 |  8643 | `			if( pBase == 0 ){` |
|     ! 0 |  8644 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8645 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  8646 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8647 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8648 | `					return SXERR_ABORT;` |
|       - |  8649 | `				}` |
|     ! 0 |  8650 | `			}` |
|   11545 |  8651 | `			SyBlobRelease(&sResolved);` |
|    5770 |  8652 | `		}` |
|    5770 |  8653 | `	}` |
|   42373 |  8654 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8655 | `		/* Syntax error */` |
|     ! 0 |  8656 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  8657 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8658 | `		if( rc == SXERR_ABORT ){` |
|       - |  8659 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8660 | `			return SXERR_ABORT;` |
|       - |  8661 | `		}` |
|     ! 0 |  8662 | `		return SXRET_OK;` |
|       - |  8663 | `	}` |
|   42373 |  8664 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   42373 |  8665 | `	pEnd = 0; /* cc warning */` |
|       - |  8666 | `	/* Delimit the interface body */` |
|   42373 |  8667 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   42373 |  8668 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8669 | `		/* Syntax error */` |
|     ! 0 |  8670 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  8671 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8672 | `		if( rc == SXERR_ABORT ){` |
|       - |  8673 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8674 | `			return SXERR_ABORT;` |
|       - |  8675 | `		}` |
|     ! 0 |  8676 | `		return SXRET_OK;` |
|       - |  8677 | `	}` |
|       - |  8678 | `	/* Swap token stream */` |
|   42373 |  8679 | `	pTmp = pGen->pEnd;` |
|   42373 |  8680 | `	pGen->pEnd = pEnd;` |
|       - |  8681 | `	/* Start the parse process` |
|       - |  8682 | `	 * Note (According to the PHP reference manual):` |
|       - |  8683 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  8684 | `	 *  Only 'public' visibility is allowed.` |
|       - |  8685 | `	 */` |
|   71180 |  8686 | `	for(;;){` |
|       - |  8687 | `		/* Jump leading/trailing semi-colons */` |
|  242357 |  8688 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   99997 |  8689 | `			pGen->pIn++;` |
|       5 |  8690 | `		}` |
|  142365 |  8691 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8692 | `			/* End of interface body */` |
|   42369 |  8693 | `			break;` |
|       - |  8694 | `		}` |
|  100001 |  8695 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8696 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8697 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  8698 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8699 | `			if( rc == SXERR_ABORT ){` |
|       - |  8700 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8701 | `				return SXERR_ABORT;` |
|       - |  8702 | `			}` |
|     ! 0 |  8703 | `			goto done;` |
|       - |  8704 | `		}` |
|       - |  8705 | `		/* Extract the current keyword */` |
|  100001 |  8706 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  100001 |  8707 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  8708 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  8709 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  8710 | `			const char *zKind = "member";` |
|       3 |  8711 | `			SyString *pMemberName = 0;` |
|       3 |  8712 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  8713 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  8714 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  8715 | `					zKind = "constant";` |
|       3 |  8716 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  8717 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  8718 | `					}` |
|       1 |  8719 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8720 | `					zKind = "method";` |
|     ! 0 |  8721 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  8722 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  8723 | `					}` |
|     ! 0 |  8724 | `				}` |
|       1 |  8725 | `			}` |
|       3 |  8726 | `			if( pMemberName ){` |
|       4 |  8727 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  8728 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  8729 | `			}else{` |
|     ! 0 |  8730 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8731 | `					"Access type for interface %s must be public",zKind);` |
|       - |  8732 | `			}` |
|       3 |  8733 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8734 | `				return SXERR_ABORT;` |
|       - |  8735 | `			}` |
|       3 |  8736 | `			goto done;` |
|       - |  8737 | `		}` |
|   99999 |  8738 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8739 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8740 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8741 | `			if( rc == SXERR_ABORT ){` |
|       - |  8742 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8743 | `				return SXERR_ABORT;` |
|       - |  8744 | `			}` |
|     ! 0 |  8745 | `			goto done;` |
|       - |  8746 | `		}` |
|   99999 |  8747 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  8748 | `			/* Advance the stream cursor */` |
|   99987 |  8749 | `			pGen->pIn++;` |
|   99987 |  8750 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8751 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8752 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8753 | `				if( rc == SXERR_ABORT ){` |
|       - |  8754 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8755 | `					return SXERR_ABORT;` |
|       - |  8756 | `				}` |
|     ! 0 |  8757 | `				goto done;` |
|       - |  8758 | `			}` |
|   99987 |  8759 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   99987 |  8760 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8761 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8762 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8763 | `				if( rc == SXERR_ABORT ){` |
|       - |  8764 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8765 | `					return SXERR_ABORT;` |
|       - |  8766 | `				}` |
|     ! 0 |  8767 | `				goto done;` |
|       - |  8768 | `			}` |
|   49991 |  8769 | `		}` |
|   99999 |  8770 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8771 | `			/* Parse constant */` |
|      10 |  8772 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|      10 |  8773 | `			if( rc != SXRET_OK ){` |
|       3 |  8774 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8775 | `					return SXERR_ABORT;` |
|       - |  8776 | `				}` |
|       3 |  8777 | `				goto done;` |
|       - |  8778 | `			}` |
|       4 |  8779 | `		}else{` |
|   99991 |  8780 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   99991 |  8781 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8782 | `				/* Static method,record that */` |
|   11537 |  8783 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  8784 | `				/* Advance the stream cursor */` |
|   11537 |  8785 | `				pGen->pIn++;` |
|   11532 |  8786 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|   11537 |  8787 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8788 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8789 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8790 | `						if( rc == SXERR_ABORT ){` |
|       - |  8791 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8792 | `							return SXERR_ABORT;` |
|       - |  8793 | `						}` |
|     ! 0 |  8794 | `						goto done;` |
|       - |  8795 | `				}` |
|    5766 |  8796 | `			}` |
|       - |  8797 | `			/* Process method signature (no body for interface methods) */` |
|   99991 |  8798 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   99991 |  8799 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8800 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8801 | `					return SXERR_ABORT;` |
|       - |  8802 | `				}` |
|     ! 0 |  8803 | `				goto done;` |
|       - |  8804 | `			}` |
|       - |  8805 | `		}` |
|       5 |  8806 | `	}` |
|       - |  8807 | `	/* Install the interface */` |
|   42369 |  8808 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   42369 |  8809 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  8810 | `		/* Inherit from the base interface */` |
|   11545 |  8811 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    5770 |  8812 | `	}` |
|   42369 |  8813 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8814 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8815 | `		return SXERR_ABORT;` |
|       - |  8816 | `	}` |
|   21182 |  8817 | `done:` |
|       - |  8818 | `	/* Point beyond the interface body */` |
|   42373 |  8819 | `	pGen->pIn  = &pEnd[1];` |
|   42373 |  8820 | `	pGen->pEnd = pTmp;` |
|   42373 |  8821 | `	return PH7_OK;` |
|   21189 |  8822 | `}` |
|       - |  8823 | `/*` |
|       - |  8824 | ` * Compile a user-defined class.` |
|       - |  8825 | ` * According to the PHP language reference manual` |
|       - |  8826 | ` *  class` |
|       - |  8827 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  8828 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  8829 | ` *  of the properties and methods belonging to the class.` |
|       - |  8830 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  8831 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  8832 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  8833 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  8834 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  8835 | ` *  (called "methods").` |
|       - |  8836 | ` */` |
|       - |  8837 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  8838 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  8839 | `struct TraitUseEntry {` |
|       - |  8840 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  8841 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  8842 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  8843 | `};` |
|       - |  8844 | `/*` |
|       - |  8845 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  8846 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  8847 | ` */` |
|  112776 |  8848 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8849 | `{` |
|       - |  8850 | `	ph7_class **apIface;` |
|       - |  8851 | `	sxu32 nIface,i;` |
|       - |  8852 | `	sxi32 rc;` |
|  112781 |  8853 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  8854 | `		return SXRET_OK;` |
|       - |  8855 | `	}` |
|  112781 |  8856 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  112781 |  8857 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  216831 |  8858 | `	for(i = 0; i < nIface; i++){` |
|  104055 |  8859 | `		ph7_class *pIface = apIface[i];` |
|       - |  8860 | `		SyHashEntry *pEntry;` |
|  104055 |  8861 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  277509 |  8862 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  173459 |  8863 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  8864 | `			ph7_class_method *pImplMeth;` |
|  173459 |  8865 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  8866 | `			/* Find the implementing method in the class */` |
|  173459 |  8867 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  173459 |  8868 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  8869 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  8870 | `			}` |
|       - |  8871 | `			/* Check visibility: interface methods must be implemented as public */` |
|  173445 |  8872 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  8873 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8874 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  8875 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  8876 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8877 | `					return SXERR_ABORT;` |
|       - |  8878 | `				}` |
|       1 |  8879 | `			}` |
|       - |  8880 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  8881 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  8882 | `			 */` |
|       - |  8883 | `			{` |
|  173445 |  8884 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  173445 |  8885 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  173445 |  8886 | `				int sigError = 0;` |
|  173445 |  8887 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  8888 | `					sigError = 1;` |
|  173444 |  8889 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  8890 | `					/* Extra parameters must all have default values */` |
|       6 |  8891 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  8892 | `					sxu32 k;` |
|       8 |  8893 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  8894 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  8895 | `							sigError = 1;` |
|       3 |  8896 | `							break;` |
|       - |  8897 | `						}` |
|       2 |  8898 | `					}` |
|       2 |  8899 | `				}` |
|  173445 |  8900 | `				if( sigError ){` |
|       - |  8901 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  8902 | `					ph7_vm_func_arg *aArgs;` |
|       - |  8903 | `					sxu32 j;` |
|       6 |  8904 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  8905 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  8906 | `					/* Build implementing method signature */` |
|       6 |  8907 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  8908 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  8909 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  8910 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  8911 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8912 | `					}` |
|       - |  8913 | `					/* Build interface method signature */` |
|       6 |  8914 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  8915 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  8916 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  8917 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  8918 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8919 | `					}` |
|       8 |  8920 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8921 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  8922 | `						&pClass->sName,pMName,` |
|       4 |  8923 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  8924 | `						&pIface->sName,pMName,` |
|       4 |  8925 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  8926 | `					SyBlobRelease(&sImplSig);` |
|       6 |  8927 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  8928 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8929 | `						return SXERR_ABORT;` |
|       - |  8930 | `					}` |
|       2 |  8931 | `				}` |
|       - |  8932 | `			}` |
|       5 |  8933 | `		}` |
|   52030 |  8934 | `	}` |
|  112781 |  8935 | `	return SXRET_OK;` |
|   56393 |  8936 | `}` |
|       - |  8937 | `/*` |
|       - |  8938 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  8939 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  8940 | ` */` |
|  112776 |  8941 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8942 | `{` |
|       - |  8943 | `	ph7_class_method *pMeth;` |
|       - |  8944 | `	SyHashEntry *pEntry;` |
|       - |  8945 | `	sxu32 nAbstract;` |
|       - |  8946 | `	SyBlob sMsg;` |
|       - |  8947 | `	sxi32 rc;` |
|       - |  8948 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|  112781 |  8949 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      37 |  8950 | `		return SXRET_OK;` |
|       - |  8951 | `	}` |
|       - |  8952 | `	/* Count abstract methods */` |
|  112749 |  8953 | `	nAbstract = 0;` |
|  112749 |  8954 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
| 1060175 |  8955 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  947431 |  8956 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  947431 |  8957 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  8958 | `			nAbstract++;` |
|       8 |  8959 | `		}` |
|       5 |  8960 | `	}` |
|  112749 |  8961 | `	if( nAbstract == 0 ){` |
|  112735 |  8962 | `		return SXRET_OK;` |
|       - |  8963 | `	}` |
|       - |  8964 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  8965 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  8966 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  8967 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  8968 | `		&pClass->sName,nAbstract,` |
|       7 |  8969 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  8970 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  8971 | `	/* Second pass: list methods with origins */` |
|       - |  8972 | `	{` |
|      18 |  8973 | `		sxu32 nListed = 0;` |
|      18 |  8974 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  8975 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  8976 | `			ph7_class *pOrigin = 0;` |
|       - |  8977 | `			SyString *pMName;` |
|      22 |  8978 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  8979 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  8980 | `				continue;` |
|       - |  8981 | `			}` |
|      20 |  8982 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  8983 | `			if( nListed > 0 ){` |
|       3 |  8984 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  8985 | `			}` |
|       - |  8986 | `			/* Find the origin of this abstract method.` |
|       - |  8987 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  8988 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  8989 | `			 * methods. Abstract class methods only win when the class` |
|       - |  8990 | `			 * itself declared the abstract method (not inherited from` |
|       - |  8991 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  8992 | `			 * class's namespace.` |
|       - |  8993 | `			 */` |
|       - |  8994 | `			{` |
|       - |  8995 | `				ph7_class **apIface;` |
|       - |  8996 | `				ph7_class **apTrait;` |
|       - |  8997 | `				ph7_class *pWalk;` |
|       - |  8998 | `				sxu32 i;` |
|       - |  8999 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  9000 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  9001 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  9002 | `				 */` |
|      20 |  9003 | `				if( pClass->pBase ){` |
|      11 |  9004 | `					pWalk = pClass->pBase;` |
|      19 |  9005 | `					while( pWalk ){` |
|       - |  9006 | `						ph7_class_method *pParentMeth;` |
|      13 |  9007 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  9008 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  9009 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  9010 | `							 * in this class's ancestor chain.` |
|       - |  9011 | `							 */` |
|      13 |  9012 | `							int fromIface = 0;` |
|      13 |  9013 | `							ph7_class *pAnc = pWalk;` |
|      17 |  9014 | `							while( pAnc ){` |
|       - |  9015 | `								ph7_class **apPI;` |
|       - |  9016 | `								sxu32 j;` |
|      15 |  9017 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  9018 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  9019 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  9020 | `										fromIface = 1;` |
|      10 |  9021 | `										break;` |
|       - |  9022 | `									}` |
|     ! 0 |  9023 | `								}` |
|      15 |  9024 | `								if( fromIface ) break;` |
|       6 |  9025 | `								pAnc = pAnc->pBase;` |
|       2 |  9026 | `							}` |
|      13 |  9027 | `							if( !fromIface ){` |
|       3 |  9028 | `								pOrigin = pWalk;` |
|       3 |  9029 | `								break;` |
|       - |  9030 | `							}` |
|       4 |  9031 | `						}` |
|      10 |  9032 | `						pWalk = pWalk->pBase;` |
|       2 |  9033 | `					}` |
|       4 |  9034 | `				}` |
|       - |  9035 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  9036 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  9037 | `				 */` |
|      20 |  9038 | `				if( !pOrigin ){` |
|      18 |  9039 | `					pWalk = pClass;` |
|      40 |  9040 | `					while( pWalk && !pOrigin ){` |
|      26 |  9041 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  9042 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  9043 | `							ph7_class *pIface = apIface[i];` |
|      16 |  9044 | `							ph7_class *pDeepest = 0;` |
|      28 |  9045 | `							while( pIface ){` |
|      16 |  9046 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  9047 | `									pDeepest = pIface;` |
|       6 |  9048 | `								}` |
|      16 |  9049 | `								pIface = pIface->pBase;` |
|       4 |  9050 | `							}` |
|      16 |  9051 | `							if( pDeepest ){` |
|      16 |  9052 | `								pOrigin = pDeepest;` |
|      16 |  9053 | `								break;` |
|       - |  9054 | `							}` |
|     ! 0 |  9055 | `						}` |
|      26 |  9056 | `						pWalk = pWalk->pBase;` |
|       4 |  9057 | `					}` |
|       7 |  9058 | `				}` |
|       - |  9059 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  9060 | `				if( !pOrigin ){` |
|       3 |  9061 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  9062 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  9063 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  9064 | `							pOrigin = pClass;` |
|       3 |  9065 | `							break;` |
|       - |  9066 | `						}` |
|     ! 0 |  9067 | `					}` |
|       1 |  9068 | `				}` |
|       - |  9069 | `			}` |
|      20 |  9070 | `			if( pOrigin ){` |
|      20 |  9071 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  9072 | `			}else{` |
|       - |  9073 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  9074 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  9075 | `			}` |
|      20 |  9076 | `			nListed++;` |
|       4 |  9077 | `		}` |
|       - |  9078 | `	}` |
|      18 |  9079 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  9080 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  9081 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  9082 | `	SyBlobRelease(&sMsg);` |
|      18 |  9083 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9084 | `		return SXERR_ABORT;` |
|       - |  9085 | `	}` |
|      18 |  9086 | `	return SXRET_OK;` |
|   56393 |  9087 | `}` |
|       - |  9088 | `/*` |
|       - |  9089 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  9090 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  9091 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  9092 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  9093 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  9094 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  9095 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  9096 | ` */` |
|  109088 |  9097 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  9098 | `{` |
|  109093 |  9099 | `	int isAbsolute = 0;` |
|  109093 |  9100 | `	SyToken *pStart = pGen->pIn;` |
|       - |  9101 | `	SyBlob sName;` |
|  109093 |  9102 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|    4295 |  9103 | `		isAbsolute = 1;` |
|    4295 |  9104 | `		pGen->pIn++;` |
|    2145 |  9105 | `	}` |
|  109093 |  9106 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       8 |  9107 | `		pGen->pIn = pStart;` |
|       8 |  9108 | `		return SXERR_INVALID;` |
|       - |  9109 | `	}` |
|  109087 |  9110 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|  109087 |  9111 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|  109087 |  9112 | `	pGen->pIn++;` |
|  163644 |  9113 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   54567 |  9114 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      16 |  9115 | `		SyBlobAppend(&sName,"\\",1);` |
|      16 |  9116 | `		pGen->pIn++;` |
|      16 |  9117 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      16 |  9118 | `		pGen->pIn++;` |
|       2 |  9119 | `	}` |
|  109087 |  9120 | `	if( isAbsolute ){` |
|    4293 |  9121 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    2149 |  9122 | `	}else{` |
|       - |  9123 | `		SyString sRaw;` |
|  104799 |  9124 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|  104799 |  9125 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  9126 | `	}` |
|  109087 |  9127 | `	SyBlobRelease(&sName);` |
|  109087 |  9128 | `	return SXRET_OK;` |
|   54549 |  9129 | `}` |
|       - |  9130 | `/*` |
|       - |  9131 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  9132 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  9133 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  9134 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  9135 | ` * either direction cannot run unbounded.` |
|       - |  9136 | ` */` |
|       - |  9137 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   11712 |  9138 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  9139 | `{` |
|       - |  9140 | `	ph7_class **apParent;` |
|       - |  9141 | `	sxu32 n;` |
|   19619 |  9142 | `	while( pInterface ){` |
|   15605 |  9143 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  9144 | `			return FALSE;` |
|       - |  9145 | `		}` |
|   19464 |  9146 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    7718 |  9147 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    7703 |  9148 | `			return TRUE;` |
|       - |  9149 | `		}` |
|    7907 |  9150 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    7907 |  9151 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  9152 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  9153 | `				return TRUE;` |
|       - |  9154 | `			}` |
|     ! 0 |  9155 | `		}` |
|    7907 |  9156 | `		pInterface = pInterface->pBase;` |
|    7907 |  9157 | `		iDepth++;` |
|       5 |  9158 | `	}` |
|    4019 |  9159 | `	return FALSE;` |
|    5861 |  9160 | `}` |
|   11712 |  9161 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  9162 | `{` |
|   11717 |  9163 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  9164 | `}` |
|       - |  9165 | `/*` |
|       - |  9166 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  9167 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  9168 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  9169 | ` */` |
|    7698 |  9170 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  9171 | `{` |
|    7707 |  9172 | `	while( pBase ){` |
|      10 |  9173 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  9174 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  9175 | `			return TRUE;` |
|       - |  9176 | `		}` |
|      10 |  9177 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  9178 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  9179 | `			return TRUE;` |
|       - |  9180 | `		}` |
|       5 |  9181 | `		pBase = pBase->pBase;` |
|       1 |  9182 | `	}` |
|    7699 |  9183 | `	return FALSE;` |
|    3854 |  9184 | `}` |
|       - |  9185 | `/*` |
|       - |  9186 | ` * Compile a class declaration, named or anonymous.` |
|       - |  9187 | ` *` |
|       - |  9188 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - |  9189 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - |  9190 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - |  9191 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - |  9192 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - |  9193 | ` * implements, body, install) is shared by both paths.` |
|       - |  9194 | ` */` |
|  112816 |  9195 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - |  9196 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 |  9197 | `{` |
|  112821 |  9198 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9199 | `	ph7_class *pClass,*pBase;` |
|       - |  9200 | `	SyToken *pEnd,*pTmp;` |
|       - |  9201 | `	sxi32 iProtection;` |
|       - |  9202 | `	SySet aInterfaces;` |
|       - |  9203 | `	SySet aUseEntries;` |
|       - |  9204 | `	sxi32 iAttrflags;` |
|       - |  9205 | `	SyString *pName;` |
|       - |  9206 | `	sxi32 nKwrd;` |
|       - |  9207 | `	sxi32 rc;` |
|       - |  9208 | `	/* Jump the 'class' keyword */` |
|  112821 |  9209 | `	pGen->pIn++;` |
|  112821 |  9210 | `	if( pAnonName ){` |
|       - |  9211 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - |  9212 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - |  9213 | `		 * then use the synthesized name. */` |
|      30 |  9214 | `		*ppArgStart = *ppArgEnd = 0;` |
|      30 |  9215 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       7 |  9216 | `			pGen->pIn++; /* Jump '(' */` |
|       7 |  9217 | `			*ppArgStart = pGen->pIn;` |
|      10 |  9218 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       3 |  9219 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       7 |  9220 | `			pGen->pIn = *ppArgEnd;` |
|       7 |  9221 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       3 |  9222 | `		}` |
|      30 |  9223 | `		pName = pAnonName;` |
|      30 |  9224 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      17 |  9225 | `	}else{` |
|  112795 |  9226 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  9227 | `			/* Syntax error */` |
|     ! 0 |  9228 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  9229 | `			if( rc == SXERR_ABORT ){` |
|       - |  9230 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  9231 | `				return SXERR_ABORT;` |
|       - |  9232 | `			}` |
|       - |  9233 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  9234 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  9235 | `				pGen->pIn++;` |
|     ! 0 |  9236 | `			}` |
|     ! 0 |  9237 | `			return SXRET_OK;` |
|       - |  9238 | `		}` |
|       - |  9239 | `		/* Extract class name */` |
|  112795 |  9240 | `		pName = &pGen->pIn->sData;` |
|       - |  9241 | `		/* Advance the stream cursor */` |
|  112795 |  9242 | `		pGen->pIn++;` |
|       - |  9243 | `		/* Build FQN and obtain a raw class */ {` |
|       - |  9244 | `			SyBlob sFQN;` |
|       - |  9245 | `			SyString sFQNStr;` |
|  112795 |  9246 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|  112795 |  9247 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|  112795 |  9248 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|  112795 |  9249 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|  112795 |  9250 | `			SyBlobRelease(&sFQN);` |
|       - |  9251 | `		}` |
|       - |  9252 | `	}` |
|  112821 |  9253 | `	if( pClass == 0 ){` |
|     ! 0 |  9254 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9255 | `		return SXERR_ABORT;` |
|       - |  9256 | `	}` |
|       - |  9257 | `	/* implemented interfaces and per-use-statement trait containers */` |
|  112821 |  9258 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|  112821 |  9259 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  9260 | `	/* Assume a standalone class */` |
|  112821 |  9261 | `	pBase = 0;` |
|  112821 |  9262 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   96455 |  9263 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   96455 |  9264 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  9265 | `			SyBlob sResolved;` |
|       - |  9266 | `			SyString sBaseName;` |
|       - |  9267 | `			sxu32 nRefLine;` |
|   84763 |  9268 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   84763 |  9269 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   84763 |  9270 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   84763 |  9271 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  9272 | `				SyBlobRelease(&sResolved);` |
|       4 |  9273 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  9274 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  9275 | `					pName);` |
|       3 |  9276 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  9277 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9278 | `					return SXERR_ABORT;` |
|       - |  9279 | `				}` |
|       3 |  9280 | `				return SXRET_OK;` |
|       - |  9281 | `			}` |
|  127139 |  9282 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   84756 |  9283 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   84761 |  9284 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  9285 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  9286 | `			/* Interfaces are not allowed */` |
|   84761 |  9287 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  9288 | `				pBase = pBase->pNextName;` |
|     ! 0 |  9289 | `			}` |
|   84761 |  9290 | `			if( pBase == 0 ){` |
|     ! 0 |  9291 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  9292 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  9293 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9294 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  9295 | `					return SXERR_ABORT;` |
|       - |  9296 | `				}` |
|     ! 0 |  9297 | `			}else{` |
|   84761 |  9298 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  9299 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  9300 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  9301 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9302 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  9303 | `						return SXERR_ABORT;` |
|       - |  9304 | `					}` |
|     ! 0 |  9305 | `				}` |
|       - |  9306 | `			}` |
|   84761 |  9307 | `			SyBlobRelease(&sResolved);` |
|   42378 |  9308 | `		}` |
|   96453 |  9309 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  9310 | `			ph7_class *pInterface;` |
|       - |  9311 | `			/* Interface implementation */` |
|   11705 |  9312 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5862 |  9313 | `			for(;;){` |
|       - |  9314 | `				SyBlob sResolved;` |
|       - |  9315 | `				SyString sIntName;` |
|       - |  9316 | `				sxu32 nRefLine;` |
|   11717 |  9317 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   11717 |  9318 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   11717 |  9319 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  9320 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  9321 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  9322 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  9323 | `						pName);` |
|     ! 0 |  9324 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9325 | `						return SXERR_ABORT;` |
|       - |  9326 | `					}` |
|     ! 0 |  9327 | `					break;` |
|       - |  9328 | `				}` |
|   23429 |  9329 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   11712 |  9330 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   11717 |  9331 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  9332 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  9333 | `				/* Only interfaces are allowed */` |
|   11717 |  9334 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  9335 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  9336 | `				}` |
|   11717 |  9337 | `				if( pInterface == 0 ){` |
|     ! 0 |  9338 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  9339 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  9340 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9341 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  9342 | `						return SXERR_ABORT;` |
|       - |  9343 | `					}` |
|     ! 0 |  9344 | `				}else{` |
|       - |  9345 | `					/* Reject user classes that try to implement Throwable` |
|       - |  9346 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  9347 | `					 * unless they already extend Exception or Error.` |
|       - |  9348 | `					 * Exception and Error themselves are compiled from the` |
|       - |  9349 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  9350 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   11717 |  9351 | `					SyString *pFqn = &pClass->sName;` |
|   11717 |  9352 | `					int bIsExceptionOrError =` |
|    9704 |  9353 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   19494 |  9354 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    9797 |  9355 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3858 |  9356 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   19408 |  9357 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|   11550 |  9358 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3847 |  9359 | `						!bIsExceptionOrError ){` |
|      12 |  9360 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9361 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  9362 | `							&pClass->sName);` |
|       9 |  9363 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9364 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  9365 | `							return SXERR_ABORT;` |
|       - |  9366 | `						}` |
|       - |  9367 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  9368 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  9369 | `					}else{` |
|   11711 |  9370 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  9371 | `					}` |
|       - |  9372 | `				}` |
|   11717 |  9373 | `				SyBlobRelease(&sResolved);` |
|   11717 |  9374 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5855 |  9375 | `					break;` |
|       - |  9376 | `				}` |
|      16 |  9377 | `				pGen->pIn++;/* Jump the comma */` |
|       4 |  9378 | `			}` |
|    5850 |  9379 | `		}` |
|   48224 |  9380 | `	}` |
|  112819 |  9381 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  9382 | `		/* Syntax error */` |
|     ! 0 |  9383 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  9384 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9385 | `		if( rc == SXERR_ABORT ){` |
|       - |  9386 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9387 | `			return SXERR_ABORT;` |
|       - |  9388 | `		}` |
|     ! 0 |  9389 | `		return SXRET_OK;` |
|       - |  9390 | `	}` |
|  112819 |  9391 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|  112819 |  9392 | `	pEnd = 0; /* cc warning */` |
|       - |  9393 | `	/* Delimit the class body */` |
|  112819 |  9394 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|  112819 |  9395 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  9396 | `		/* Syntax error */` |
|     ! 0 |  9397 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  9398 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9399 | `		if( rc == SXERR_ABORT ){` |
|       - |  9400 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9401 | `			return SXERR_ABORT;` |
|       - |  9402 | `		}` |
|     ! 0 |  9403 | `		return SXRET_OK;` |
|       - |  9404 | `	}` |
|       - |  9405 | `	/* Swap token stream */` |
|  112819 |  9406 | `	pTmp = pGen->pEnd;` |
|  112819 |  9407 | `	pGen->pEnd = pEnd;` |
|       - |  9408 | `	/* Set the inherited flags */` |
|  112819 |  9409 | `	pClass->iFlags = iFlags;` |
|       - |  9410 | `	/* Start the parse process */` |
|  151172 |  9411 | `	for(;;){` |
|       - |  9412 | `		/* Jump leading/trailing semi-colons */` |
|  465343 |  9413 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   81543 |  9414 | `			pGen->pIn++;` |
|       5 |  9415 | `		}` |
|  383805 |  9416 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9417 | `			/* End of class body */` |
|  112781 |  9418 | `			break;` |
|       - |  9419 | `		}` |
|  271024 |  9420 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  135517 |  9421 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  9422 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9423 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  9424 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9425 | `			if( rc == SXERR_ABORT ){` |
|       - |  9426 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  9427 | `				return SXERR_ABORT;` |
|       - |  9428 | `			}` |
|     ! 0 |  9429 | `			goto done;` |
|       - |  9430 | `		}` |
|       - |  9431 | `		/* Assume public visibility */` |
|  271029 |  9432 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  271029 |  9433 | `		iAttrflags = 0;` |
|       - |  9434 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  9435 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  9436 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  9437 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  271029 |  9438 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9439 | `			int bMod = 0;` |
|     ! 0 |  9440 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9441 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  9442 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  9443 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  9444 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  9445 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  9446 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  9447 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  9448 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  9449 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  9450 | `			}` |
|     ! 0 |  9451 | `			if( !bMod ){` |
|     ! 0 |  9452 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9453 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9454 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9455 | `						return SXERR_ABORT;` |
|       - |  9456 | `					}` |
|     ! 0 |  9457 | `					goto done;` |
|       - |  9458 | `				}` |
|     ! 0 |  9459 | `				continue;` |
|       - |  9460 | `			}` |
|     ! 0 |  9461 | `		}` |
|  271029 |  9462 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9463 | `			/* Extract the current keyword */` |
|  271029 |  9464 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  271029 |  9465 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9466 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  9467 | `				TraitUseEntry sUse;` |
|      57 |  9468 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      57 |  9469 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      57 |  9470 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      34 |  9471 | `				for(;;){` |
|       - |  9472 | `					ph7_class *pTrait;` |
|       - |  9473 | `					SyString *pTraitName;` |
|      65 |  9474 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9475 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9476 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  9477 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9478 | `							return SXERR_ABORT;` |
|       - |  9479 | `						}` |
|     ! 0 |  9480 | `						break;` |
|       - |  9481 | `					}` |
|      65 |  9482 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  9483 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  9484 | `						SyBlob sResolved;` |
|      65 |  9485 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      65 |  9486 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     125 |  9487 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      60 |  9488 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      65 |  9489 | `						SyBlobRelease(&sResolved);` |
|       - |  9490 | `					}` |
|       - |  9491 | `					/* Only traits are allowed */` |
|      65 |  9492 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9493 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  9494 | `					}` |
|      65 |  9495 | `					if( pTrait == 0 ){` |
|     ! 0 |  9496 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9497 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  9498 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9499 | `							return SXERR_ABORT;` |
|       - |  9500 | `						}` |
|     ! 0 |  9501 | `					}else{` |
|      65 |  9502 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  9503 | `					}` |
|      65 |  9504 | `					pGen->pIn++; /* Advance past trait name */` |
|      65 |  9505 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      31 |  9506 | `						break;` |
|       - |  9507 | `					}` |
|      10 |  9508 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  9509 | `				}` |
|       - |  9510 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      57 |  9511 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  9512 | `					SyToken *pBlock;` |
|      13 |  9513 | `					pGen->pIn++; /* Jump '{' */` |
|      13 |  9514 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      13 |  9515 | `					sUse.pResolvStart = pGen->pIn;` |
|      13 |  9516 | `					sUse.pResolvEnd = pBlock;` |
|      13 |  9517 | `					if( pBlock < pGen->pEnd ){` |
|      13 |  9518 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       8 |  9519 | `					}else{` |
|     ! 0 |  9520 | `						pGen->pIn = pGen->pEnd;` |
|       - |  9521 | `					}` |
|       5 |  9522 | `				}` |
|      57 |  9523 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  9524 | `				/* The semicolon will be consumed by the outer loop */` |
|      57 |  9525 | `				continue;` |
|       - |  9526 | `			}` |
|  270977 |  9527 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  270631 |  9528 | `				iProtection = nKwrd;` |
|  270631 |  9529 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  9530 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  270631 |  9531 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  9532 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  9533 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  9534 | `				}` |
|  270626 |  9535 | `				if( pGen->pIn >= pGen->pEnd` |
|  270631 |  9536 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9537 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9538 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  9539 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9540 | `					if( rc == SXERR_ABORT ){` |
|       - |  9541 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  9542 | `						return SXERR_ABORT;` |
|       - |  9543 | `					}` |
|     ! 0 |  9544 | `					goto done;` |
|       - |  9545 | `				}` |
|  270631 |  9546 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9547 | `					/* Attribute declaration (untyped) */` |
|   81225 |  9548 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   81225 |  9549 | `					if( rc != SXRET_OK ){` |
|      11 |  9550 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9551 | `							return SXERR_ABORT;` |
|       - |  9552 | `						}` |
|      11 |  9553 | `						goto done;` |
|       - |  9554 | `					}` |
|   81217 |  9555 | `					continue;` |
|       - |  9556 | `				}` |
|  189411 |  9557 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9558 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     175 |  9559 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     175 |  9560 | `					if( rc != SXRET_OK ){` |
|       8 |  9561 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9562 | `							return SXERR_ABORT;` |
|       - |  9563 | `						}` |
|       8 |  9564 | `						goto done;` |
|       - |  9565 | `					}` |
|     169 |  9566 | `					continue;` |
|       - |  9567 | `				}` |
|       - |  9568 | `				/* Extract the keyword */` |
|  189241 |  9569 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   94618 |  9570 | `			}` |
|  189587 |  9571 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  9572 | `				/* Process constant declaration */` |
|      87 |  9573 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      87 |  9574 | `				if( rc != SXRET_OK ){` |
|      11 |  9575 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9576 | `						return SXERR_ABORT;` |
|       - |  9577 | `					}` |
|      11 |  9578 | `					goto done;` |
|       - |  9579 | `				}` |
|      42 |  9580 | `			}else{` |
|  189505 |  9581 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  9582 | `					/* Static method or attribute,record that */` |
|   11611 |  9583 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|   11611 |  9584 | `					pGen->pIn++; /* Jump the static keyword */` |
|   11611 |  9585 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9586 | `						/* Extract the keyword */` |
|   11599 |  9587 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   11599 |  9588 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9589 | `							iProtection = nKwrd;` |
|     ! 0 |  9590 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  9591 | `						}` |
|    5797 |  9592 | `					}` |
|       - |  9593 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  9594 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  9595 | `					 * than a generic "expecting method" parse error. */` |
|   11611 |  9596 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9597 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9598 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  9599 | `					}` |
|   11606 |  9600 | `					if( pGen->pIn >= pGen->pEnd` |
|   11611 |  9601 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9602 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9603 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  9604 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9605 | `						if( rc == SXERR_ABORT ){` |
|       - |  9606 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9607 | `							return SXERR_ABORT;` |
|       - |  9608 | `						}` |
|     ! 0 |  9609 | `						goto done;` |
|       - |  9610 | `					}` |
|   11611 |  9611 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9612 | `						/* Attribute declaration */` |
|      13 |  9613 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      13 |  9614 | `						if( rc != SXRET_OK ){` |
|       3 |  9615 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9616 | `								return SXERR_ABORT;` |
|       - |  9617 | `							}` |
|       3 |  9618 | `							goto done;` |
|       - |  9619 | `						}` |
|      10 |  9620 | `						continue;` |
|       - |  9621 | `					}` |
|   11601 |  9622 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9623 | `						/* Typed static attribute declaration */` |
|      15 |  9624 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  9625 | `						if( rc != SXRET_OK ){` |
|       3 |  9626 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9627 | `								return SXERR_ABORT;` |
|       - |  9628 | `							}` |
|       3 |  9629 | `							goto done;` |
|       - |  9630 | `						}` |
|      13 |  9631 | `						continue;` |
|       - |  9632 | `					}` |
|       - |  9633 | `					/* Extract the keyword */` |
|   11589 |  9634 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  183691 |  9635 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  9636 | `					/* Abstract method,record that */` |
|      15 |  9637 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  9638 | `					/* Mark the whole class as abstract */` |
|      15 |  9639 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  9640 | `					/* Advance the stream cursor */` |
|      15 |  9641 | `					pGen->pIn++;` |
|      15 |  9642 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      15 |  9643 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      15 |  9644 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      13 |  9645 | `							iProtection = nKwrd;` |
|      13 |  9646 | `							pGen->pIn++; /* Jump the visibility token */` |
|       5 |  9647 | `						}` |
|       6 |  9648 | `					}` |
|      15 |  9649 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      12 |  9650 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9651 | `							/* Static method */` |
|     ! 0 |  9652 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9653 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9654 | `					}` |
|      15 |  9655 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      12 |  9656 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9657 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9658 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  9659 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9660 | `							if( rc == SXERR_ABORT ){` |
|       - |  9661 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9662 | `								return SXERR_ABORT;` |
|       - |  9663 | `							}` |
|     ! 0 |  9664 | `							goto done;` |
|       - |  9665 | `					}` |
|      15 |  9666 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  177893 |  9667 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  9668 | `					/* final method ,record that */` |
|      17 |  9669 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      17 |  9670 | `					pGen->pIn++; /* Jump the final keyword */` |
|      17 |  9671 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9672 | `						/* Extract the keyword */` |
|      17 |  9673 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      17 |  9674 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       9 |  9675 | `							iProtection = nKwrd;` |
|       9 |  9676 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  9677 | `						}` |
|       7 |  9678 | `					}` |
|      17 |  9679 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  9680 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  9681 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  9682 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  9683 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  9684 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9685 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  9686 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  9687 | `									return SXERR_ABORT;` |
|       - |  9688 | `								}` |
|     ! 0 |  9689 | `								goto done;` |
|       - |  9690 | `							}` |
|      12 |  9691 | `							continue;` |
|       - |  9692 | `					}` |
|       6 |  9693 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  9694 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9695 | `							/* Static method */` |
|     ! 0 |  9696 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9697 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9698 | `					}` |
|       6 |  9699 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9700 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9701 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9702 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  9703 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9704 | `							if( rc == SXERR_ABORT ){` |
|       - |  9705 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9706 | `								return SXERR_ABORT;` |
|       - |  9707 | `							}` |
|     ! 0 |  9708 | `							goto done;` |
|       - |  9709 | `					}` |
|       6 |  9710 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9711 | `				}` |
|  189473 |  9712 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9713 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9714 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  9715 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9716 | `						if( rc == SXERR_ABORT ){` |
|       - |  9717 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9718 | `							return SXERR_ABORT;` |
|       - |  9719 | `						}` |
|     ! 0 |  9720 | `						goto done;` |
|       - |  9721 | `				}` |
|  189473 |  9722 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  9723 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  9724 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  9725 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9726 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9727 | `						if( rc == SXERR_ABORT ){` |
|       - |  9728 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9729 | `							return SXERR_ABORT;` |
|       - |  9730 | `						}` |
|     ! 0 |  9731 | `						goto done;` |
|       - |  9732 | `					}` |
|       - |  9733 | `					/* Attribute declaration */` |
|       7 |  9734 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  9735 | `				}else{` |
|       - |  9736 | `					/* Process method declaration */` |
|  189467 |  9737 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9738 | `				}` |
|  189473 |  9739 | `				if( rc != SXRET_OK ){` |
|      16 |  9740 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9741 | `						return SXERR_ABORT;` |
|       - |  9742 | `					}` |
|      16 |  9743 | `					goto done;` |
|       - |  9744 | `				}` |
|       - |  9745 | `			}` |
|   94770 |  9746 | `		}else{` |
|       - |  9747 | `			/* Attribute declaration */` |
|     ! 0 |  9748 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9749 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9750 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9751 | `					return SXERR_ABORT;` |
|       - |  9752 | `				}` |
|     ! 0 |  9753 | `				goto done;` |
|       - |  9754 | `			}` |
|       - |  9755 | `		}` |
|       5 |  9756 | `	}` |
|       - |  9757 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  9758 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  9759 | `	 */` |
|       - |  9760 | `	{` |
|       - |  9761 | `		TraitUseEntry *apUse;` |
|       - |  9762 | `		sxu32 nU;` |
|  112781 |  9763 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|  112833 |  9764 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      57 |  9765 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      57 |  9766 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      57 |  9767 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      57 |  9768 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  9769 | `			sxu32 nT;` |
|      57 |  9770 | `			if( !hasResolution ){` |
|       - |  9771 | `				/* No conflict resolution block: use standard trait application */` |
|      95 |  9772 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      53 |  9773 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      53 |  9774 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9775 | `						break;` |
|       - |  9776 | `					}` |
|      29 |  9777 | `				}` |
|      26 |  9778 | `			}else{` |
|       - |  9779 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  9780 | `				 * then use the block to resolve method conflicts.` |
|       - |  9781 | `				 */` |
|       - |  9782 | `				SyToken *pR;` |
|      25 |  9783 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      15 |  9784 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  9785 | `					ph7_class_attr *pAR;` |
|       - |  9786 | `					SyHashEntry *pER;` |
|       - |  9787 | `					SyString *pNR;` |
|      15 |  9788 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      21 |  9789 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  9790 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  9791 | `						pNR = &pAR->sName;` |
|     ! 0 |  9792 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  9793 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  9794 | `						}` |
|     ! 0 |  9795 | `					}` |
|      15 |  9796 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       9 |  9797 | `				}` |
|       - |  9798 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      13 |  9799 | `				pR = pUse->pResolvStart;` |
|      27 |  9800 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9801 | `					SyString sTrait,sMethod;` |
|       - |  9802 | `					ph7_class *pSrcTrait;` |
|       - |  9803 | `					ph7_class_method *pMeth;` |
|       - |  9804 | `					sxi32 nRKwrd;` |
|      41 |  9805 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9806 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9807 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9808 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9809 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9810 | `					sMethod = pR->sData;` |
|      17 |  9811 | `					pR++;` |
|      17 |  9812 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9813 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9814 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9815 | `							sTrait = sMethod;` |
|       7 |  9816 | `							pR++;` |
|       7 |  9817 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9818 | `							sMethod = pR->sData;` |
|       7 |  9819 | `							pR++;` |
|       3 |  9820 | `						}` |
|       3 |  9821 | `					}` |
|      17 |  9822 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9823 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9824 | `						continue;` |
|       - |  9825 | `					}` |
|      17 |  9826 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9827 | `					pR++;` |
|      17 |  9828 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  9829 | `						pSrcTrait = 0;` |
|       7 |  9830 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  9831 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  9832 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  9833 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  9834 | `								pSrcTrait = apTrait[nT];` |
|       5 |  9835 | `								break;` |
|       - |  9836 | `							}` |
|       2 |  9837 | `						}` |
|       5 |  9838 | `						if( pSrcTrait ){` |
|       5 |  9839 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  9840 | `							if( pMeth ){` |
|       5 |  9841 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  9842 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  9843 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  9844 | `								}` |
|       2 |  9845 | `							}` |
|       2 |  9846 | `						}` |
|       2 |  9847 | `					}` |
|      35 |  9848 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9849 | `				}` |
|       - |  9850 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      25 |  9851 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  9852 | `					ph7_class_method *pMR;` |
|       - |  9853 | `					SyHashEntry *pER;` |
|       - |  9854 | `					SyString *pNR;` |
|      15 |  9855 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      41 |  9856 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      23 |  9857 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      23 |  9858 | `						pNR = &pMR->sFunc.sName;` |
|      23 |  9859 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      14 |  9860 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       6 |  9861 | `						}` |
|       3 |  9862 | `					}` |
|       9 |  9863 | `				}` |
|       - |  9864 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      13 |  9865 | `				pR = pUse->pResolvStart;` |
|      27 |  9866 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9867 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  9868 | `					ph7_class *pSrcTrait;` |
|       - |  9869 | `					ph7_class_method *pMeth;` |
|      27 |  9870 | `					int hasQual = 0;` |
|       - |  9871 | `					sxi32 nRKwrd;` |
|      41 |  9872 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9873 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9874 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9875 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9876 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      17 |  9877 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9878 | `					sMethod = pR->sData;` |
|      17 |  9879 | `					pR++;` |
|      17 |  9880 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9881 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9882 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9883 | `							sTrait = sMethod;` |
|       7 |  9884 | `							hasQual = 1;` |
|       7 |  9885 | `							pR++;` |
|       7 |  9886 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9887 | `							sMethod = pR->sData;` |
|       7 |  9888 | `							pR++;` |
|       3 |  9889 | `						}` |
|       3 |  9890 | `					}` |
|      17 |  9891 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9892 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9893 | `						continue;` |
|       - |  9894 | `					}` |
|      17 |  9895 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9896 | `					pR++;` |
|      17 |  9897 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      13 |  9898 | `						sxi32 iNewVis = -1;` |
|      13 |  9899 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  9900 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  9901 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  9902 | `								iNewVis = nAK;` |
|       7 |  9903 | `								pR++;` |
|       3 |  9904 | `							}` |
|       3 |  9905 | `						}` |
|      13 |  9906 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      11 |  9907 | `							sAlias = pR->sData;` |
|      11 |  9908 | `							pR++;` |
|       4 |  9909 | `						}` |
|      13 |  9910 | `						pMeth = 0;` |
|      13 |  9911 | `						if( hasQual ){` |
|       3 |  9912 | `							pSrcTrait = 0;` |
|       5 |  9913 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  9914 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  9915 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  9916 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  9917 | `									pSrcTrait = apTrait[nT];` |
|       3 |  9918 | `									break;` |
|       - |  9919 | `								}` |
|       2 |  9920 | `							}` |
|       3 |  9921 | `							if( pSrcTrait ){` |
|       3 |  9922 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  9923 | `							}` |
|       2 |  9924 | `						}else{` |
|      10 |  9925 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  9926 | `						}` |
|      13 |  9927 | `						if( pMeth ){` |
|      13 |  9928 | `							if( sAlias.nByte > 0 ){` |
|       - |  9929 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  9930 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  9931 | `								 */` |
|       - |  9932 | `								ph7_class_method *pAlias;` |
|       - |  9933 | `								char *zAliasDup;` |
|      11 |  9934 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      11 |  9935 | `								if( pAlias ){` |
|      11 |  9936 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      11 |  9937 | `									if( iNewVis >= 0 ){` |
|       5 |  9938 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9939 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9940 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  9941 | `									}` |
|      11 |  9942 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      11 |  9943 | `									if( zAliasDup ){` |
|      11 |  9944 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       4 |  9945 | `									}` |
|       7 |  9946 | `								}` |
|       7 |  9947 | `							}else if( iNewVis >= 0 ){` |
|       - |  9948 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  9949 | `								ph7_class_method *pCopy;` |
|       3 |  9950 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  9951 | `								if( pCopy ){` |
|       3 |  9952 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  9953 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  9954 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9955 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9956 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  9957 | `									/* Replace the method in the class hash */` |
|       3 |  9958 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  9959 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  9960 | `								}` |
|       1 |  9961 | `							}` |
|       5 |  9962 | `						}` |
|       5 |  9963 | `						SXUNUSED(hasQual);` |
|       5 |  9964 | `					}` |
|      21 |  9965 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9966 | `				}` |
|       - |  9967 | `			}` |
|      57 |  9968 | `			SySetRelease(&pUse->aTraits);` |
|      31 |  9969 | `		}` |
|       - |  9970 | `	}` |
|       - |  9971 | `	/* Install the class */` |
|  112781 |  9972 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|  112781 |  9973 | `	if( rc == SXRET_OK ){` |
|       - |  9974 | `		ph7_class **apInterface;` |
|       - |  9975 | `		sxu32 n;` |
|  112781 |  9976 | `		if( pBase ){` |
|       - |  9977 | `			/* Inherit from base class and mark as a subclass */` |
|   84761 |  9978 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   42378 |  9979 | `		}` |
|  112781 |  9980 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  124487 |  9981 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  9982 | `			/* Implements one or more interface */` |
|   11711 |  9983 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   11711 |  9984 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9985 | `				break;` |
|       - |  9986 | `			}` |
|    5858 |  9987 | `		}` |
|       - |  9988 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  9989 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  169164 |  9990 | `		if( rc == SXRET_OK` |
|  112776 |  9991 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|  112781 |  9992 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   92351 |  9993 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  9994 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   92351 |  9995 | `			if( pStringable ){` |
|   92351 |  9996 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   92351 |  9997 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  9998 | `				sxu32 i;` |
|   92351 |  9999 | `				int bAlready = 0;` |
|  100043 | 10000 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    7699 | 10001 | `					if( apImpl[i] == pStringable ){` |
|       3 | 10002 | `						bAlready = 1;` |
|       3 | 10003 | `						break;` |
|       - | 10004 | `					}` |
|    3851 | 10005 | `				}` |
|   92351 | 10006 | `				if( !bAlready ){` |
|   92349 | 10007 | `					PH7_ClassImplement(pClass,pStringable);` |
|   46172 | 10008 | `				}` |
|   46173 | 10009 | `			}` |
|   46173 | 10010 | `		}` |
|       - | 10011 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|  112781 | 10012 | `		if( rc == SXRET_OK ){` |
|  112781 | 10013 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|  112781 | 10014 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 10015 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 10016 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 10017 | `				return SXERR_ABORT;` |
|       - | 10018 | `			}` |
|   56388 | 10019 | `		}` |
|       - | 10020 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|  112781 | 10021 | `		if( rc == SXRET_OK ){` |
|  112781 | 10022 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|  112781 | 10023 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 10024 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 10025 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 10026 | `				return SXERR_ABORT;` |
|       - | 10027 | `			}` |
|   56388 | 10028 | `		}` |
|   56388 | 10029 | `	}` |
|  112781 | 10030 | `	SySetRelease(&aUseEntries);` |
|  112781 | 10031 | `	SySetRelease(&aInterfaces);` |
|  112781 | 10032 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10033 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10034 | `		return SXERR_ABORT;` |
|       - | 10035 | `	}` |
|   56388 | 10036 | `done:` |
|       - | 10037 | `	/* Point beyond the class body */` |
|  112819 | 10038 | `	pGen->pIn = &pEnd[1];` |
|  112819 | 10039 | `	pGen->pEnd = pTmp;` |
|  112819 | 10040 | `	return PH7_OK;` |
|   56413 | 10041 | `}` |
|       - | 10042 | `/* Compile a named class declaration (the common case). */` |
|  112790 | 10043 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 | 10044 | `{` |
|  112795 | 10045 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 | 10046 | `}` |
|       - | 10047 | `/*` |
|       - | 10048 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - | 10049 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - | 10050 | ` * compile + install the class body once (at compile time, like every other` |
|       - | 10051 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - | 10052 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - | 10053 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - | 10054 | ` */` |
|      26 | 10055 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 | 10056 | `{` |
|       - | 10057 | `	char zName[128];         /* Synthesized class name */` |
|       - | 10058 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - | 10059 | `	SyString sName;` |
|       - | 10060 | `	SyToken *pArgStart,*pArgEnd;` |
|       - | 10061 | `	ph7_value *pObj;` |
|      30 | 10062 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10063 | `	sxu32 nIdx,nLen;` |
|       - | 10064 | `	sxi32 nArg,rc;` |
|      13 | 10065 | `	SXUNUSED(iCompileFlag);` |
|       - | 10066 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|      30 | 10067 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      30 | 10068 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 10069 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 | 10070 | `	}` |
|      30 | 10071 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - | 10072 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - | 10073 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - | 10074 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|      30 | 10075 | `	pArgStart = pArgEnd = 0;` |
|      30 | 10076 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      30 | 10077 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10078 | `		return rc;` |
|       - | 10079 | `	}` |
|       - | 10080 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - | 10081 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      30 | 10082 | `	nArg = 0;` |
|      30 | 10083 | `	if( pArgStart < pArgEnd ){` |
|       7 | 10084 | `		SyToken *pSavedIn = pGen->pIn;` |
|       7 | 10085 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 10086 | `		SyToken *pArgNext;` |
|       7 | 10087 | `		pGen->pIn = pArgStart;` |
|       7 | 10088 | `		pGen->pEnd = pArgEnd;` |
|      13 | 10089 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       7 | 10090 | `			if( pGen->pIn < pArgNext ){` |
|       7 | 10091 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       7 | 10092 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10093 | `					pGen->pIn = pSavedIn;` |
|     ! 0 | 10094 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 | 10095 | `					return SXERR_ABORT;` |
|       - | 10096 | `				}` |
|       7 | 10097 | `				nArg++;` |
|       3 | 10098 | `			}` |
|       7 | 10099 | `			pGen->pIn = &pArgNext[1];` |
|       1 | 10100 | `		}` |
|       7 | 10101 | `		pGen->pIn = pSavedIn;` |
|       7 | 10102 | `		pGen->pEnd = pSavedEnd;` |
|       3 | 10103 | `	}` |
|       - | 10104 | `	/* Load the synthesized class name */` |
|      30 | 10105 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      30 | 10106 | `	if( pObj == 0 ){` |
|     ! 0 | 10107 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10108 | `		return SXERR_ABORT;` |
|       - | 10109 | `	}` |
|      30 | 10110 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      30 | 10111 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 10112 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      30 | 10113 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      30 | 10114 | `	return SXRET_OK;` |
|      17 | 10115 | `}` |
|       - | 10116 | `/*` |
|       - | 10117 | ` * Compile a user-defined abstract class.` |
|       - | 10118 | ` *  According to the PHP language reference manual` |
|       - | 10119 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 10120 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 10121 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 10122 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 10123 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 10124 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 10125 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 10126 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 10127 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 10128 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 10129 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 10130 | ` *   could differ.` |
|       - | 10131 | ` */` |
|       - | 10132 | `/*` |
|       - | 10133 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - | 10134 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - | 10135 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - | 10136 | ` */` |
| 1061914 | 10137 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 | 10138 | `{` |
| 1061919 | 10139 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  712663 | 10140 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  712663 | 10141 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  704953 | 10142 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  352443 | 10143 | `	}` |
| 1054147 | 10144 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
| 1054087 | 10145 | `	return FALSE;` |
|  530962 | 10146 | `}` |
|       - | 10147 | `/*` |
|       - | 10148 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - | 10149 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - | 10150 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - | 10151 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - | 10152 | ` */` |
| 1054082 | 10153 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 | 10154 | `{` |
| 1054087 | 10155 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
| 1054087 | 10156 | `	sxi32 iFlags = 0,iFlag;` |
| 1061919 | 10157 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    7837 | 10158 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 | 10159 | `			pDup = pIn;` |
|       2 | 10160 | `		}` |
|    7837 | 10161 | `		iFlags \|= iFlag;` |
|    7837 | 10162 | `		pIn++;` |
|       5 | 10163 | `	}` |
| 1054087 | 10164 | `	*ppIn = pIn;` |
| 1054087 | 10165 | `	if( ppDup ){ *ppDup = pDup; }` |
| 1054087 | 10166 | `	return iFlags;` |
|       5 | 10167 | `}` |
|       - | 10168 | `/*` |
|       - | 10169 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - | 10170 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - | 10171 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - | 10172 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - | 10173 | `` * `readonly`) to their existing handlers.`` |
|       - | 10174 | ` */` |
| 1050176 | 10175 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 | 10176 | `{` |
| 1050181 | 10177 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  529001 | 10178 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
| 1052131 | 10179 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 | 10180 | `}` |
|       - | 10181 | `/*` |
|       - | 10182 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - | 10183 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - | 10184 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - | 10185 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - | 10186 | `` * `abstract`+`final` pair, like PHP.`` |
|       - | 10187 | ` */` |
|    3906 | 10188 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 | 10189 | `{` |
|       - | 10190 | `	SyToken *pDup;` |
|    3911 | 10191 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - | 10192 | `	sxi32 rc;` |
|    3911 | 10193 | `	if( pDup ){` |
|       4 | 10194 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 | 10195 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 | 10196 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10197 | `			return SXERR_ABORT;` |
|       - | 10198 | `		}` |
|       1 | 10199 | `	}` |
|    5859 | 10200 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    1958 | 10201 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 | 10202 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10203 | `			"Cannot use the final modifier on an abstract class");` |
|       3 | 10204 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10205 | `			return SXERR_ABORT;` |
|       - | 10206 | `		}` |
|       1 | 10207 | `	}` |
|    3911 | 10208 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    1958 | 10209 | `}` |
|       - | 10210 | `/*` |
|       - | 10211 | ` * Compile a user-defined trait.` |
|       - | 10212 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 10213 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 10214 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 10215 | ` */` |
|      64 | 10216 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 | 10217 | `{` |
|      69 | 10218 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10219 | `	ph7_class *pClass;` |
|       - | 10220 | `	SyToken *pEnd,*pTmp;` |
|       - | 10221 | `	sxi32 iProtection;` |
|       - | 10222 | `	sxi32 iAttrflags;` |
|       - | 10223 | `	SyString *pName;` |
|       - | 10224 | `	sxi32 nKwrd;` |
|       - | 10225 | `	sxi32 rc;` |
|       - | 10226 | `	/* Jump the 'trait' keyword */` |
|      69 | 10227 | `	pGen->pIn++;` |
|      69 | 10228 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 10229 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 10230 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10231 | `			return SXERR_ABORT;` |
|       - | 10232 | `		}` |
|     ! 0 | 10233 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 10234 | `			pGen->pIn++;` |
|     ! 0 | 10235 | `		}` |
|     ! 0 | 10236 | `		return SXRET_OK;` |
|       - | 10237 | `	}` |
|       - | 10238 | `	/* Extract trait name */` |
|      69 | 10239 | `	pName = &pGen->pIn->sData;` |
|      69 | 10240 | `	pGen->pIn++;` |
|       - | 10241 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 10242 | `		SyBlob sFQN;` |
|       - | 10243 | `		SyString sFQNStr;` |
|      69 | 10244 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      69 | 10245 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      69 | 10246 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      69 | 10247 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      69 | 10248 | `		SyBlobRelease(&sFQN);` |
|       - | 10249 | `	}` |
|      69 | 10250 | `	if( pClass == 0 ){` |
|     ! 0 | 10251 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10252 | `		return SXERR_ABORT;` |
|       - | 10253 | `	}` |
|       - | 10254 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      69 | 10255 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 10256 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 10257 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 10258 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10259 | `			return SXERR_ABORT;` |
|       - | 10260 | `		}` |
|     ! 0 | 10261 | `		return SXRET_OK;` |
|       - | 10262 | `	}` |
|      69 | 10263 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      69 | 10264 | `	pEnd = 0;` |
|      69 | 10265 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      69 | 10266 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 10267 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 10268 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 10269 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10270 | `			return SXERR_ABORT;` |
|       - | 10271 | `		}` |
|     ! 0 | 10272 | `		return SXRET_OK;` |
|       - | 10273 | `	}` |
|       - | 10274 | `	/* Swap token stream */` |
|      69 | 10275 | `	pTmp = pGen->pEnd;` |
|      69 | 10276 | `	pGen->pEnd = pEnd;` |
|       - | 10277 | `	/* Mark as trait */` |
|      69 | 10278 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 10279 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      64 | 10280 | `	for(;;){` |
|     177 | 10281 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 | 10282 | `			pGen->pIn++;` |
|       4 | 10283 | `		}` |
|     153 | 10284 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      69 | 10285 | `			break;` |
|       - | 10286 | `		}` |
|      89 | 10287 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 10288 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10289 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 10290 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 10291 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10292 | `				return SXERR_ABORT;` |
|       - | 10293 | `			}` |
|     ! 0 | 10294 | `			goto done;` |
|       - | 10295 | `		}` |
|      89 | 10296 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      89 | 10297 | `		iAttrflags = 0;` |
|      89 | 10298 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      89 | 10299 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      89 | 10300 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 10301 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 10302 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 10303 | `				for(;;){` |
|       - | 10304 | `					ph7_class *pUsedTrait;` |
|       - | 10305 | `					SyString *pUsedName;` |
|       5 | 10306 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 10307 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 10308 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 10309 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10310 | `							return SXERR_ABORT;` |
|       - | 10311 | `						}` |
|     ! 0 | 10312 | `						break;` |
|       - | 10313 | `					}` |
|       5 | 10314 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 10315 | `					{` |
|       - | 10316 | `						SyBlob sResolved;` |
|       5 | 10317 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 10318 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 10319 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 10320 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 10321 | `						SyBlobRelease(&sResolved);` |
|       - | 10322 | `					}` |
|       5 | 10323 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 10324 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 10325 | `					}` |
|       5 | 10326 | `					if( pUsedTrait == 0 ){` |
|       4 | 10327 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 10328 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 10329 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10330 | `							return SXERR_ABORT;` |
|       - | 10331 | `						}` |
|       2 | 10332 | `					}else{` |
|       3 | 10333 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 10334 | `					}` |
|       5 | 10335 | `					pGen->pIn++;` |
|       5 | 10336 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 10337 | `						break;` |
|       - | 10338 | `					}` |
|     ! 0 | 10339 | `					pGen->pIn++;` |
|     ! 0 | 10340 | `				}` |
|       5 | 10341 | `				continue;` |
|       - | 10342 | `			}` |
|      85 | 10343 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 | 10344 | `				iProtection = nKwrd;` |
|      73 | 10345 | `				pGen->pIn++;` |
|      68 | 10346 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 | 10347 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 10348 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10349 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 10350 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 10351 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10352 | `						return SXERR_ABORT;` |
|       - | 10353 | `					}` |
|     ! 0 | 10354 | `					goto done;` |
|       - | 10355 | `				}` |
|      73 | 10356 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 | 10357 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 | 10358 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 10359 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10360 | `							return SXERR_ABORT;` |
|       - | 10361 | `						}` |
|     ! 0 | 10362 | `						goto done;` |
|       - | 10363 | `					}` |
|      12 | 10364 | `					continue;` |
|       - | 10365 | `				}` |
|      63 | 10366 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 | 10367 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 10368 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 10369 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10370 | `							return SXERR_ABORT;` |
|       - | 10371 | `						}` |
|     ! 0 | 10372 | `						goto done;` |
|       - | 10373 | `					}` |
|       5 | 10374 | `					continue;` |
|       - | 10375 | `				}` |
|      58 | 10376 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 | 10377 | `			}` |
|      71 | 10378 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 10379 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10380 | `					"Traits cannot have constants");` |
|     ! 0 | 10381 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10382 | `					return SXERR_ABORT;` |
|       - | 10383 | `				}` |
|     ! 0 | 10384 | `				goto done;` |
|     ! 0 | 10385 | `			}else{` |
|      71 | 10386 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 10387 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 10388 | `					pGen->pIn++;` |
|       5 | 10389 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 10390 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 10391 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 10392 | `							iProtection = nKwrd;` |
|     ! 0 | 10393 | `							pGen->pIn++;` |
|     ! 0 | 10394 | `						}` |
|       1 | 10395 | `					}` |
|       4 | 10396 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 | 10397 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 10398 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10399 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 10400 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 10401 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10402 | `							return SXERR_ABORT;` |
|       - | 10403 | `						}` |
|     ! 0 | 10404 | `						goto done;` |
|       - | 10405 | `					}` |
|       5 | 10406 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 10407 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 10408 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 10409 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 10410 | `								return SXERR_ABORT;` |
|       - | 10411 | `							}` |
|     ! 0 | 10412 | `							goto done;` |
|       - | 10413 | `						}` |
|       3 | 10414 | `						continue;` |
|       - | 10415 | `					}` |
|       3 | 10416 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 | 10417 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10418 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 10419 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 10420 | `								return SXERR_ABORT;` |
|       - | 10421 | `							}` |
|     ! 0 | 10422 | `							goto done;` |
|       - | 10423 | `						}` |
|     ! 0 | 10424 | `						continue;` |
|       - | 10425 | `					}` |
|       3 | 10426 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      68 | 10427 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 | 10428 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 | 10429 | `					pGen->pIn++;` |
|       6 | 10430 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 | 10431 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 | 10432 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 | 10433 | `							iProtection = nKwrd;` |
|       6 | 10434 | `							pGen->pIn++;` |
|       2 | 10435 | `						}` |
|       2 | 10436 | `					}` |
|       6 | 10437 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 10438 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 10439 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10440 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 10441 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 10442 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10443 | `							return SXERR_ABORT;` |
|       - | 10444 | `						}` |
|     ! 0 | 10445 | `						goto done;` |
|       - | 10446 | `					}` |
|       6 | 10447 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 10448 | `				}` |
|      69 | 10449 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 10450 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10451 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 10452 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 10453 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10454 | `						return SXERR_ABORT;` |
|       - | 10455 | `					}` |
|     ! 0 | 10456 | `					goto done;` |
|       - | 10457 | `				}` |
|      69 | 10458 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 10459 | `					pGen->pIn++;` |
|     ! 0 | 10460 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 10461 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10462 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 10463 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10464 | `							return SXERR_ABORT;` |
|       - | 10465 | `						}` |
|     ! 0 | 10466 | `						goto done;` |
|       - | 10467 | `					}` |
|     ! 0 | 10468 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10469 | `				}else{` |
|      69 | 10470 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 10471 | `				}` |
|      69 | 10472 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10473 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10474 | `						return SXERR_ABORT;` |
|       - | 10475 | `					}` |
|     ! 0 | 10476 | `					goto done;` |
|       - | 10477 | `				}` |
|       - | 10478 | `			}` |
|      37 | 10479 | `		}else{` |
|     ! 0 | 10480 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10481 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10482 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10483 | `					return SXERR_ABORT;` |
|       - | 10484 | `				}` |
|     ! 0 | 10485 | `				goto done;` |
|       - | 10486 | `			}` |
|       - | 10487 | `		}` |
|       5 | 10488 | `	}` |
|       - | 10489 | `	/* Install the trait */` |
|      69 | 10490 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      69 | 10491 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10492 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10493 | `		return SXERR_ABORT;` |
|       - | 10494 | `	}` |
|      32 | 10495 | `done:` |
|       - | 10496 | `	/* Point beyond the trait body */` |
|      69 | 10497 | `	pGen->pIn = &pEnd[1];` |
|      69 | 10498 | `	pGen->pEnd = pTmp;` |
|      69 | 10499 | `	return PH7_OK;` |
|      37 | 10500 | `}` |
|       - | 10501 | `/*` |
|       - | 10502 | ` * Compile a user-defined class.` |
|       - | 10503 | ` *  According to the PHP language reference manual` |
|       - | 10504 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 10505 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 10506 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 10507 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 10508 | ` *   and functions (called "methods").` |
|       - | 10509 | ` */` |
|  108884 | 10510 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 | 10511 | `{` |
|       - | 10512 | `	sxi32 rc;` |
|  108889 | 10513 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|  108889 | 10514 | `	return rc;` |
|       5 | 10515 | `}` |
|       - | 10516 | `/*` |
|       - | 10517 | ` * Exception handling.` |
|       - | 10518 | ` *  According to the PHP language reference manual` |
|       - | 10519 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 10520 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 10521 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 10522 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 10523 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 10524 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 10525 | ` *    (or re-thrown) within a catch block.` |
|       - | 10526 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 10527 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 10528 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 10529 | ` *    been defined with set_exception_handler().` |
|       - | 10530 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 10531 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 10532 | ` */` |
|       - | 10533 | `/*` |
|       - | 10534 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 10535 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 10536 | ` * indicates failure.` |
|       - | 10537 | ` */` |
|   15750 | 10538 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 10539 | `{` |
|   15755 | 10540 | `	sxi32 rc = SXRET_OK;` |
|   15755 | 10541 | `	if( pRoot->pOp ){` |
|   15743 | 10542 | `		switch( pRoot->pOp->iOp ){` |
|    7869 | 10543 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - | 10544 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - | 10545 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - | 10546 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - | 10547 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - | 10548 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   15743 | 10549 | `			break;` |
|     ! 0 | 10550 | `		default:` |
|       - | 10551 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - | 10552 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - | 10553 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 | 10554 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 10555 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 10556 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 10557 | `				rc = SXERR_INVALID;` |
|     ! 0 | 10558 | `			}` |
|     ! 0 | 10559 | `			break;` |
|       - | 10560 | `		}` |
|    7886 | 10561 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 10562 | `		/* Unexpected expression */` |
|     ! 0 | 10563 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 10564 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10565 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 10566 | `			rc = SXERR_INVALID;` |
|     ! 0 | 10567 | `		}` |
|     ! 0 | 10568 | `	}` |
|   15755 | 10569 | `	return rc;` |
|       5 | 10570 | `}` |
|       - | 10571 | `/*` |
|       - | 10572 | ` * Compile a 'throw' statement.` |
|       - | 10573 | ` * throw: This is how you trigger an exception.` |
|       - | 10574 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 10575 | ` */` |
|   15714 | 10576 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 | 10577 | `{` |
|   15719 | 10578 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10579 | `	GenBlock *pBlock;` |
|       - | 10580 | `	sxu32 nIdx;` |
|       - | 10581 | `	sxi32 rc;` |
|   15719 | 10582 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 10583 | `	/* Compile the expression */` |
|   15719 | 10584 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   15719 | 10585 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10586 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 10587 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10588 | `			return SXERR_ABORT;` |
|       - | 10589 | `		}` |
|     ! 0 | 10590 | `		return SXRET_OK;` |
|       - | 10591 | `	}` |
|   15719 | 10592 | `	pBlock = pGen->pCurrent;` |
|       - | 10593 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   62201 | 10594 | `	while(pBlock->pParent){` |
|   62197 | 10595 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   15715 | 10596 | `			break;` |
|       - | 10597 | `		}` |
|       - | 10598 | `		/* Point to the parent block */` |
|   46487 | 10599 | `		pBlock = pBlock->pParent;` |
|       5 | 10600 | `	}` |
|       - | 10601 | `	/* Emit the throw instruction */` |
|   15719 | 10602 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 10603 | `	/* Emit the jump */` |
|   15719 | 10604 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   15719 | 10605 | `	return SXRET_OK;` |
|    7862 | 10606 | `}` |
|       - | 10607 | `/*` |
|       - | 10608 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - | 10609 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - | 10610 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - | 10611 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - | 10612 | ` * the validator guarantees the operand is a valid exception target.` |
|       - | 10613 | ` */` |
|      36 | 10614 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 10615 | `{` |
|      38 | 10616 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10617 | `	GenBlock *pBlock;` |
|       - | 10618 | `	sxu32 nIdx;` |
|       - | 10619 | `	sxi32 rc;` |
|      18 | 10620 | `	(void)iCompileFlag;` |
|      38 | 10621 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 | 10622 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 10623 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10624 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10625 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10626 | `			return SXERR_ABORT;` |
|       - | 10627 | `		}` |
|     ! 0 | 10628 | `		return SXRET_OK;` |
|       - | 10629 | `	}` |
|      38 | 10630 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 | 10631 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10632 | `		return SXERR_ABORT;` |
|       - | 10633 | `	}` |
|      38 | 10634 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10635 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10636 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10637 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10638 | `			return SXERR_ABORT;` |
|       - | 10639 | `		}` |
|     ! 0 | 10640 | `		return SXRET_OK;` |
|       - | 10641 | `	}` |
|       - | 10642 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 | 10643 | `	pBlock = pGen->pCurrent;` |
|      60 | 10644 | `	while( pBlock->pParent ){` |
|      49 | 10645 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 | 10646 | `			break;` |
|       - | 10647 | `		}` |
|      23 | 10648 | `		pBlock = pBlock->pParent;` |
|       1 | 10649 | `	}` |
|      38 | 10650 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 | 10651 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 | 10652 | `	return SXRET_OK;` |
|      20 | 10653 | `}` |
|       - | 10654 | `/*` |
|       - | 10655 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|       - | 10656 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|       - | 10657 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|       - | 10658 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|       - | 10659 | ` * compile error propagated from the parser.` |
|       - | 10660 | ` */` |
|      46 | 10661 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|       4 | 10662 | `{` |
|       - | 10663 | `	SyString sClassName;` |
|       - | 10664 | `	SyToken *pToken;` |
|       - | 10665 | `	SyString *pName;` |
|       - | 10666 | `	char *zDup;` |
|       - | 10667 | `	sxi32 rc;` |
|      50 | 10668 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|      50 | 10669 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|      50 | 10670 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|      50 | 10671 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      50 | 10672 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 10673 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10674 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10675 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10676 | `		return SXERR_INVALID;` |
|       - | 10677 | `	}` |
|      50 | 10678 | `	pGen->pIn++; /* '(' */` |
|      23 | 10679 | `	for(;;){` |
|       - | 10680 | `		SyBlob sResolved;` |
|      50 | 10681 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      50 | 10682 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 10683 | `			SyBlobRelease(&sResolved);` |
|     ! 0 | 10684 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10685 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10686 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10687 | `			return SXERR_INVALID;` |
|       - | 10688 | `		}` |
|      73 | 10689 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      46 | 10690 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|      50 | 10691 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|      50 | 10692 | `		SyBlobRelease(&sResolved);` |
|      50 | 10693 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|      50 | 10694 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|      50 | 10695 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      46 | 10696 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|       4 | 10697 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     ! 0 | 10698 | `			pGen->pIn++; continue;` |
|       - | 10699 | `		}` |
|      50 | 10700 | `		break;` |
|     ! 0 | 10701 | `	}` |
|      69 | 10702 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|      50 | 10703 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 10704 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10705 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10706 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10707 | `		return SXERR_INVALID;` |
|       - | 10708 | `	}` |
|      50 | 10709 | `	pGen->pIn++; /* '$' */` |
|      50 | 10710 | `	pName = &pGen->pIn->sData;` |
|      50 | 10711 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      50 | 10712 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|      50 | 10713 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|      50 | 10714 | `	pGen->pIn++;` |
|      50 | 10715 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 10716 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10717 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10718 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10719 | `		return SXERR_INVALID;` |
|       - | 10720 | `	}` |
|      50 | 10721 | `	pGen->pIn++; /* ')' */` |
|      50 | 10722 | `	return SXRET_OK;` |
|      27 | 10723 | `}` |
|       - | 10724 | `/*` |
|       - | 10725 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|       - | 10726 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|       - | 10727 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|       - | 10728 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|       - | 10729 | ` * VmThrowException):` |
|       - | 10730 | ` *` |
|       - | 10731 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|       - | 10732 | ` *    <try body>` |
|       - | 10733 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|       - | 10734 | ` *    JMP  -> finally\|end` |
|       - | 10735 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|       - | 10736 | ` *    <catch body>` |
|       - | 10737 | ` *    JMP  -> finally\|end` |
|       - | 10738 | ` *    ... more catches ...` |
|       - | 10739 | ` *  Lfin: <finally body>` |
|       - | 10740 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|       - | 10741 | ` *  Lend:` |
|       - | 10742 | ` */` |
|      90 | 10743 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|       4 | 10744 | `{` |
|      94 | 10745 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10746 | `	GenBlock *pTry;` |
|       - | 10747 | `	VmInstr *pInstr;` |
|      94 | 10748 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|       - | 10749 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|       - | 10750 | `	sxi32 rc;` |
|      94 | 10751 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|       - | 10752 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      94 | 10753 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      94 | 10754 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      94 | 10755 | `	pTry->pUserData = pException;` |
|      94 | 10756 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      94 | 10757 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      94 | 10758 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      94 | 10759 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      94 | 10760 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      94 | 10761 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10762 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      94 | 10763 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      94 | 10764 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      94 | 10765 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      94 | 10766 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10767 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      94 | 10768 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|       - | 10769 | `	/* Catch clauses (inline) */` |
|      94 | 10770 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      90 | 10771 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|      50 | 10772 | `		sxu32 k = 0;` |
|      69 | 10773 | `		for(;;){` |
|       - | 10774 | `			ph7_exception_block sCatch;` |
|       - | 10775 | `			GenBlock *pCatchBlk;` |
|      96 | 10776 | `			sxu32 idxJmp = 0;` |
|      92 | 10777 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      88 | 10778 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      27 | 10779 | `				break;` |
|       - | 10780 | `			}` |
|      50 | 10781 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|      50 | 10782 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      50 | 10783 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|      50 | 10784 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|      50 | 10785 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|      50 | 10786 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|      50 | 10787 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       - | 10788 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|       - | 10789 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|       - | 10790 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|      50 | 10791 | `			pCatchBlk->pUserData = pException;` |
|      50 | 10792 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      50 | 10793 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      50 | 10794 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      50 | 10795 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10796 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|       - | 10797 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|      50 | 10798 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      50 | 10799 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|      50 | 10800 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|      50 | 10801 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      50 | 10802 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      50 | 10803 | `			k++;` |
|       4 | 10804 | `		}` |
|      23 | 10805 | `	}` |
|       - | 10806 | `	/* Finally (inline) */` |
|      94 | 10807 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      74 | 10808 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10809 | `		GenBlock *pFinBlk;` |
|      52 | 10810 | `		pGen->pIn++; /* Jump 'finally' */` |
|      52 | 10811 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|      52 | 10812 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|      52 | 10813 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      52 | 10814 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      52 | 10815 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      52 | 10816 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      52 | 10817 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      52 | 10818 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|      52 | 10819 | `		pException->iHasFinally = 1;` |
|      24 | 10820 | `	}` |
|      94 | 10821 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      94 | 10822 | `	pException->iInlined = 1;` |
|       - | 10823 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|       - | 10824 | `	{` |
|      94 | 10825 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|       - | 10826 | `		sxu32 *aJ; sxu32 n;` |
|      94 | 10827 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      94 | 10828 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      94 | 10829 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|     140 | 10830 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|      50 | 10831 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|      50 | 10832 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      27 | 10833 | `		}` |
|       - | 10834 | `	}` |
|      94 | 10835 | `	SySetRelease(&aCatchJmp);` |
|      94 | 10836 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|     ! 0 | 10837 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|     ! 0 | 10838 | `	}` |
|      94 | 10839 | `	return SXRET_OK;` |
|      49 | 10840 | `}` |
|       - | 10841 | `/*` |
|       - | 10842 | ` * Compile a 'catch' block.` |
|       - | 10843 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 10844 | ` * an object containing the exception information.` |
|       - | 10845 | ` */` |
|    1004 | 10846 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 10847 | `{` |
|    1009 | 10848 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10849 | `	ph7_exception_block sCatch;` |
|       - | 10850 | `	SySet *pInstrContainer;` |
|       - | 10851 | `	SyString sClassName;` |
|       - | 10852 | `	GenBlock *pCatch;` |
|       - | 10853 | `	SyToken *pToken;` |
|       - | 10854 | `	SyString *pName;` |
|       - | 10855 | `	char *zDup;` |
|       - | 10856 | `	sxi32 rc;` |
|    1009 | 10857 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 10858 | `	/* Zero the structure */` |
|    1009 | 10859 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 10860 | `	/* Initialize fields */` |
|    1009 | 10861 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|    1009 | 10862 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|    1009 | 10863 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 10864 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10865 | `			pToken = pGen->pIn;` |
|     ! 0 | 10866 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10867 | `				pToken--;` |
|     ! 0 | 10868 | `			}` |
|     ! 0 | 10869 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10870 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10871 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10872 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10873 | `				return SXERR_ABORT;` |
|       - | 10874 | `			}` |
|     ! 0 | 10875 | `			return SXERR_INVALID;` |
|       - | 10876 | `	}` |
|       - | 10877 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|    1009 | 10878 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     516 | 10879 | `	for(;;){` |
|       - | 10880 | `		SyBlob sResolved;` |
|    1037 | 10881 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    1037 | 10882 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 10883 | `			SyBlobRelease(&sResolved);` |
|       6 | 10884 | `			pToken = pGen->pIn;` |
|       6 | 10885 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10886 | `				pToken--;` |
|     ! 0 | 10887 | `			}` |
|       8 | 10888 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10889 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 10890 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 10891 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10892 | `				return SXERR_ABORT;` |
|       - | 10893 | `			}` |
|       6 | 10894 | `			return SXERR_INVALID;` |
|       - | 10895 | `		}` |
|       - | 10896 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 10897 | `		 * transient SyBlob allocation. */` |
|    1547 | 10898 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    1028 | 10899 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    1033 | 10900 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    1033 | 10901 | `		SyBlobRelease(&sResolved);` |
|    1033 | 10902 | `		if( zDup == 0 ){` |
|     ! 0 | 10903 | `			goto Mem;` |
|       - | 10904 | `		}` |
|    1033 | 10905 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    1033 | 10906 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10907 | `			goto Mem;` |
|       - | 10908 | `		}` |
|       - | 10909 | `		/* Check for '\|' (multi-catch separator) */` |
|    1042 | 10910 | `		if( pGen->pIn < pGen->pEnd &&` |
|    1028 | 10911 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      33 | 10912 | `			pGen->pIn->sData.nByte == 1 &&` |
|      28 | 10913 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      30 | 10914 | `			pGen->pIn++; /* Consume the '\|' */` |
|      30 | 10915 | `			continue;` |
|       - | 10916 | `		}` |
|    1005 | 10917 | `		break;` |
|     ! 0 | 10918 | `	}` |
|    1500 | 10919 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|    1005 | 10920 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 10921 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10922 | `			pToken = pGen->pIn;` |
|     ! 0 | 10923 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10924 | `				pToken--;` |
|     ! 0 | 10925 | `			}` |
|     ! 0 | 10926 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10927 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10928 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10929 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10930 | `				return SXERR_ABORT;` |
|       - | 10931 | `			}` |
|     ! 0 | 10932 | `			return SXERR_INVALID;` |
|       - | 10933 | `	}` |
|    1005 | 10934 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 10935 | `	/* Duplicate instance name */` |
|    1005 | 10936 | `	pName = &pGen->pIn->sData;` |
|    1005 | 10937 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    1005 | 10938 | `	if( zDup == 0 ){` |
|     ! 0 | 10939 | `		goto Mem;` |
|       - | 10940 | `	}` |
|    1005 | 10941 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|    1005 | 10942 | `	pGen->pIn++;` |
|    1005 | 10943 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 10944 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 10945 | `		pToken = pGen->pIn;` |
|     ! 0 | 10946 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10947 | `			pToken--;` |
|     ! 0 | 10948 | `		}` |
|     ! 0 | 10949 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10950 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10951 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10952 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10953 | `			return SXERR_ABORT;` |
|       - | 10954 | `		}` |
|     ! 0 | 10955 | `		return SXERR_INVALID;` |
|       - | 10956 | `	}` |
|       - | 10957 | `	/* Compile the block */` |
|    1005 | 10958 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 10959 | `	/* Create the catch block */` |
|    1005 | 10960 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|    1005 | 10961 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10962 | `		return SXERR_ABORT;` |
|       - | 10963 | `	}` |
|       - | 10964 | `	/* Swap bytecode container */` |
|    1005 | 10965 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    1005 | 10966 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 10967 | `	/* Compile the block */` |
|    1005 | 10968 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 10969 | `	/* Fix forward jumps now the destination is resolved  */` |
|    1005 | 10970 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10971 | `	/* Emit the DONE instruction */` |
|    1005 | 10972 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10973 | `	/* Leave the block */` |
|    1005 | 10974 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10975 | `	/* Restore the default container */` |
|    1005 | 10976 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10977 | `	/* Install the catch block */` |
|    1005 | 10978 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|    1005 | 10979 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10980 | `		goto Mem;` |
|       - | 10981 | `	}` |
|    1005 | 10982 | `	return SXRET_OK;` |
|     ! 0 | 10983 | `Mem:` |
|     ! 0 | 10984 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10985 | `	return SXERR_ABORT;` |
|     507 | 10986 | `}` |
|       - | 10987 | `/*` |
|       - | 10988 | ` * Compile a 'try' block.` |
|       - | 10989 | ` * A function using an exception should be in a "try" block.` |
|       - | 10990 | ` * If the exception does not trigger, the code will continue` |
|       - | 10991 | ` * as normal. However if the exception triggers, an exception` |
|       - | 10992 | ` * is "thrown".` |
|       - | 10993 | ` */` |
|    1152 | 10994 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 10995 | `{` |
|       - | 10996 | `	ph7_exception *pException;` |
|    1157 | 10997 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10998 | `	GenBlock *pTry;` |
|       - | 10999 | `	sxu32 nJmpIdx;` |
|       - | 11000 | `	sxi32 rc;` |
|       - | 11001 | `	/* Create the exception container */` |
|    1157 | 11002 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    1157 | 11003 | `	if( pException == 0 ){` |
|     ! 0 | 11004 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 11005 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 11006 | `		return SXERR_ABORT;` |
|       - | 11007 | `	}` |
|       - | 11008 | `	/* Zero the structure */` |
|    1157 | 11009 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 11010 | `	/* Initialize fields */` |
|    1157 | 11011 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    1157 | 11012 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    1157 | 11013 | `	pException->iHasFinally = 0;` |
|    1157 | 11014 | `	pException->iFinallyDone = 0;` |
|    1157 | 11015 | `	pException->pVm = pGen->pVm;` |
|       - | 11016 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|       - | 11017 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|       - | 11018 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|       - | 11019 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|       - | 11020 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|       - | 11021 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|    1157 | 11022 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      94 | 11023 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|       - | 11024 | `	}` |
|       - | 11025 | `	/* Create the try block */` |
|    1067 | 11026 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|    1067 | 11027 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 11028 | `		return SXERR_ABORT;` |
|       - | 11029 | `	}` |
|       - | 11030 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|    1067 | 11031 | `	pTry->pUserData = pException;` |
|       - | 11032 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    1067 | 11033 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 11034 | `	/* Fix the jump later when the destination is resolved */` |
|    1067 | 11035 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    1067 | 11036 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 11037 | `	/* Compile the block */` |
|    1067 | 11038 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    1067 | 11039 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 11040 | `		return SXERR_ABORT;` |
|       - | 11041 | `	}` |
|       - | 11042 | `	/* Fix forward jumps now the destination is resolved */` |
|    1067 | 11043 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11044 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    1067 | 11045 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 11046 | `	/* Leave the block */` |
|    1067 | 11047 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 11048 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    1067 | 11049 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    1060 | 11050 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 11051 | `		/* Compile one or more catch blocks */` |
|    1000 | 11052 | `		for(;;){` |
|    2000 | 11053 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    1442 | 11054 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     503 | 11055 | `					break;` |
|       - | 11056 | `			}` |
|    1009 | 11057 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|    1009 | 11058 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11059 | `				return SXERR_ABORT;` |
|       - | 11060 | `			}` |
|       5 | 11061 | `		}` |
|     498 | 11062 | `	}` |
|       - | 11063 | `	/* Compile optional finally block */` |
|    1067 | 11064 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     430 | 11065 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 11066 | `		SySet *pInstrContainer;` |
|       - | 11067 | `		GenBlock *pFinBlock;` |
|     129 | 11068 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 11069 | `		/* Create the finally block for jump fixup bookkeeping */` |
|     129 | 11070 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     129 | 11071 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11072 | `			return SXERR_ABORT;` |
|       - | 11073 | `		}` |
|       - | 11074 | `		/* Swap bytecode container */` |
|     129 | 11075 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     129 | 11076 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 11077 | `		/* Compile the finally body */` |
|     129 | 11078 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     129 | 11079 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11080 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 11081 | `			return SXERR_ABORT;` |
|       - | 11082 | `		}` |
|       - | 11083 | `		/* Fix forward jumps now the destination is resolved */` |
|     129 | 11084 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11085 | `		/* Emit DONE to terminate the finally block */` |
|     129 | 11086 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 11087 | `		/* Leave the block */` |
|     129 | 11088 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 11089 | `		/* Restore the default container */` |
|     129 | 11090 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     129 | 11091 | `		pException->iHasFinally = 1;` |
|      62 | 11092 | `	}` |
|       - | 11093 | `	/* Must have at least one catch or finally */` |
|    1067 | 11094 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       9 | 11095 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 11096 | `			"Cannot use try without catch or finally");` |
|       9 | 11097 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11098 | `			return SXERR_ABORT;` |
|       - | 11099 | `		}` |
|       3 | 11100 | `	}` |
|    1067 | 11101 | `	return SXRET_OK;` |
|     581 | 11102 | `}` |
|       - | 11103 | `/*` |
|       - | 11104 | ` * Compile a switch block.` |
|       - | 11105 | ` *  (See block-comment below for more information)` |
|       - | 11106 | ` */` |
|     112 | 11107 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 11108 | `{` |
|     117 | 11109 | `	sxi32 rc = SXRET_OK;` |
|     117 | 11110 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 11111 | `		/* Unexpected token */` |
|     ! 0 | 11112 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 11113 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11114 | `			return SXERR_ABORT;` |
|       - | 11115 | `		}` |
|     ! 0 | 11116 | `		pGen->pIn++;` |
|     ! 0 | 11117 | `	}` |
|     117 | 11118 | `	pGen->pIn++;` |
|       - | 11119 | `	/* First instruction to execute in this block. */` |
|     117 | 11120 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 11121 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 11122 | `	 * or the '}' token */` |
|     206 | 11123 | `	for(;;){` |
|     417 | 11124 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11125 | `			/* No more input to process */` |
|     ! 0 | 11126 | `			break;` |
|       - | 11127 | `		}` |
|     417 | 11128 | `		rc = SXRET_OK;` |
|     417 | 11129 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 | 11130 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 | 11131 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 11132 | `					/* Unexpected token */` |
|     ! 0 | 11133 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 11134 | `						&pGen->pIn->sData);` |
|     ! 0 | 11135 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11136 | `						return SXERR_ABORT;` |
|       - | 11137 | `					}` |
|       - | 11138 | `					/* FALL THROUGH */` |
|     ! 0 | 11139 | `				}` |
|      31 | 11140 | `				rc = SXERR_EOF;` |
|      31 | 11141 | `				break;` |
|       - | 11142 | `			}` |
|      32 | 11143 | `		}else{` |
|       - | 11144 | `			sxi32 nKwrd;` |
|       - | 11145 | `			/* Extract the keyword */` |
|     337 | 11146 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 | 11147 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 | 11148 | `				break;` |
|       - | 11149 | `			}` |
|     253 | 11150 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 11151 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 11152 | `					/* Unexpected token */` |
|     ! 0 | 11153 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 11154 | `						&pGen->pIn->sData);` |
|     ! 0 | 11155 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11156 | `						return SXERR_ABORT;` |
|       - | 11157 | `					}` |
|       - | 11158 | `					/* FALL THROUGH */` |
|     ! 0 | 11159 | `				}` |
|       - | 11160 | `				/* Block compiled */` |
|       3 | 11161 | `				break;` |
|       - | 11162 | `			}` |
|       - | 11163 | `		}` |
|       - | 11164 | `		/* Compile block */` |
|     305 | 11165 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 | 11166 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11167 | `			return SXERR_ABORT;` |
|       - | 11168 | `		}` |
|       5 | 11169 | `	}` |
|     117 | 11170 | `	return rc;` |
|      61 | 11171 | `}` |
|       - | 11172 | `/*` |
|       - | 11173 | ` * Compile a case eXpression.` |
|       - | 11174 | ` *  (See block-comment below for more information)` |
|       - | 11175 | ` */` |
|      92 | 11176 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 11177 | `{` |
|       - | 11178 | `	SySet *pInstrContainer;` |
|       - | 11179 | `	SyToken *pEnd,*pTmp;` |
|      97 | 11180 | `	sxi32 iNest = 0;` |
|       - | 11181 | `	sxi32 rc;` |
|       - | 11182 | `	/* Delimit the expression */` |
|      97 | 11183 | `	pEnd = pGen->pIn;` |
|     197 | 11184 | `	while( pEnd < pGen->pEnd ){` |
|     197 | 11185 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 11186 | `			/* Increment nesting level */` |
|       3 | 11187 | `			iNest++;` |
|     196 | 11188 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 11189 | `			/* Decrement nesting level */` |
|       3 | 11190 | `			iNest--;` |
|     194 | 11191 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 | 11192 | `			break;` |
|       - | 11193 | `		}` |
|     105 | 11194 | `		pEnd++;` |
|       5 | 11195 | `	}` |
|      97 | 11196 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 11197 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 11198 | `		if( rc == SXERR_ABORT ){` |
|       - | 11199 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 11200 | `			return SXERR_ABORT;` |
|       - | 11201 | `		}` |
|     ! 0 | 11202 | `	}` |
|       - | 11203 | `	/* Swap token stream */` |
|      97 | 11204 | `	pTmp = pGen->pEnd;` |
|      97 | 11205 | `	pGen->pEnd = pEnd;` |
|      97 | 11206 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 | 11207 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 | 11208 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 11209 | `	/* Emit the done instruction */` |
|      97 | 11210 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 | 11211 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 11212 | `	/* Update token stream */` |
|      97 | 11213 | `	pGen->pIn  = pEnd;` |
|      97 | 11214 | `	pGen->pEnd = pTmp;` |
|      97 | 11215 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 11216 | `		return SXERR_ABORT;` |
|       - | 11217 | `	}` |
|      97 | 11218 | `	return SXRET_OK;` |
|      51 | 11219 | `}` |
|       - | 11220 | `/*` |
|       - | 11221 | ` * Compile the smart switch statement.` |
|       - | 11222 | ` * According to the PHP language reference manual` |
|       - | 11223 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 11224 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 11225 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 11226 | ` *  This is exactly what the switch statement is for.` |
|       - | 11227 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 11228 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 11229 | ` *  of the outer loop, use continue 2.` |
|       - | 11230 | ` *  Note that switch/case does loose comparision.` |
|       - | 11231 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 11232 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 11233 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 11234 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 11235 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 11236 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 11237 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 11238 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 11239 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 11240 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 11241 | ` *  list for the next case.` |
|       - | 11242 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 11243 | ` *  or floating-point numbers and strings.` |
|       - | 11244 | ` */` |
|      28 | 11245 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 11246 | `{` |
|       - | 11247 | `	GenBlock *pSwitchBlock;` |
|       - | 11248 | `	SyToken *pTmp,*pEnd;` |
|       - | 11249 | `	ph7_switch *pSwitch;` |
|       - | 11250 | `	sxu32 nToken;` |
|       - | 11251 | `	sxu32 nLine;` |
|       - | 11252 | `	sxi32 rc;` |
|      33 | 11253 | `	nLine = pGen->pIn->nLine;` |
|       - | 11254 | `	/* Jump the 'switch' keyword */` |
|      33 | 11255 | `	pGen->pIn++;` |
|      33 | 11256 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 11257 | `		/* Syntax error */` |
|     ! 0 | 11258 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 11259 | `		if( rc == SXERR_ABORT ){` |
|       - | 11260 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 11261 | `			return SXERR_ABORT;` |
|       - | 11262 | `		}` |
|     ! 0 | 11263 | `		goto Synchronize;` |
|       - | 11264 | `	}` |
|       - | 11265 | `	/* Jump the left parenthesis '(' */` |
|      33 | 11266 | `	pGen->pIn++;` |
|      33 | 11267 | `	pEnd = 0; /* cc warning */` |
|       - | 11268 | `	/* Create the loop block */` |
|      47 | 11269 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 11270 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 | 11271 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 11272 | `		return SXERR_ABORT;` |
|       - | 11273 | `	}` |
|       - | 11274 | `	/* Delimit the condition */` |
|      33 | 11275 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 | 11276 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 11277 | `		/* Empty expression */` |
|     ! 0 | 11278 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 11279 | `		if( rc == SXERR_ABORT ){` |
|       - | 11280 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 11281 | `			return SXERR_ABORT;` |
|       - | 11282 | `		}` |
|     ! 0 | 11283 | `	}` |
|       - | 11284 | `	/* Swap token streams */` |
|      33 | 11285 | `	pTmp = pGen->pEnd;` |
|      33 | 11286 | `	pGen->pEnd = pEnd;` |
|       - | 11287 | `	/* Compile the expression */` |
|      33 | 11288 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 | 11289 | `	if( rc == SXERR_ABORT ){` |
|       - | 11290 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 11291 | `		return SXERR_ABORT;` |
|       - | 11292 | `	}` |
|       - | 11293 | `	/* Update token stream */` |
|      33 | 11294 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 11295 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 11296 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 11297 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11298 | `			return SXERR_ABORT;` |
|       - | 11299 | `		}` |
|     ! 0 | 11300 | `		pGen->pIn++;` |
|     ! 0 | 11301 | `	}` |
|      33 | 11302 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 11303 | `	pGen->pEnd = pTmp;` |
|      33 | 11304 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 11305 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 11306 | `			pTmp = pGen->pIn;` |
|     ! 0 | 11307 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 11308 | `				pTmp--;` |
|     ! 0 | 11309 | `			}` |
|       - | 11310 | `			/* Unexpected token */` |
|     ! 0 | 11311 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 11312 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11313 | `				return SXERR_ABORT;` |
|       - | 11314 | `			}` |
|     ! 0 | 11315 | `			goto Synchronize;` |
|       - | 11316 | `	}` |
|       - | 11317 | `	/* Set the delimiter token */` |
|      33 | 11318 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 11319 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 11320 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 11321 | `	}else{` |
|      31 | 11322 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 11323 | `	}` |
|      33 | 11324 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 11325 | `	/* Create the switch blocks container */` |
|      33 | 11326 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 11327 | `	if( pSwitch == 0 ){` |
|       - | 11328 | `		/* Abort compilation */` |
|     ! 0 | 11329 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 11330 | `		return SXERR_ABORT;` |
|       - | 11331 | `	}` |
|       - | 11332 | `	/* Zero the structure */` |
|      33 | 11333 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 11334 | `	/* Initialize fields */` |
|      33 | 11335 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 11336 | `	/* Emit the switch instruction */` |
|      33 | 11337 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 11338 | `	/* Compile case blocks */` |
|     100 | 11339 | `	for(;;){` |
|       - | 11340 | `		sxu32 nKwrd;` |
|     119 | 11341 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11342 | `			/* No more input to process */` |
|     ! 0 | 11343 | `			break;` |
|       - | 11344 | `		}` |
|     119 | 11345 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 11346 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 11347 | `				/* Unexpected token */` |
|     ! 0 | 11348 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11349 | `					&pGen->pIn->sData);` |
|     ! 0 | 11350 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11351 | `					return SXERR_ABORT;` |
|       - | 11352 | `				}` |
|       - | 11353 | `				/* FALL THROUGH */` |
|     ! 0 | 11354 | `			}` |
|       - | 11355 | `			/* Block compiled */` |
|     ! 0 | 11356 | `			break;` |
|       - | 11357 | `		}` |
|       - | 11358 | `		/* Extract the keyword */` |
|     119 | 11359 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 11360 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 11361 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 11362 | `				/* Unexpected token */` |
|     ! 0 | 11363 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11364 | `					&pGen->pIn->sData);` |
|     ! 0 | 11365 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11366 | `					return SXERR_ABORT;` |
|       - | 11367 | `				}` |
|       - | 11368 | `				/* FALL THROUGH */` |
|     ! 0 | 11369 | `			}` |
|       - | 11370 | `			/* Block compiled */` |
|       3 | 11371 | `			break;` |
|       - | 11372 | `		}` |
|     117 | 11373 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 11374 | `			/*` |
|       - | 11375 | `			 * Accroding to the PHP language reference manual` |
|       - | 11376 | `			 *  A special case is the default case. This case matches anything` |
|       - | 11377 | `			 *  that wasn't matched by the other cases.` |
|       - | 11378 | `			 */` |
|      25 | 11379 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 11380 | `				/* Default case already compiled */` |
|     ! 0 | 11381 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 11382 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11383 | `					return SXERR_ABORT;` |
|       - | 11384 | `				}` |
|     ! 0 | 11385 | `			}` |
|      25 | 11386 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 11387 | `			/* Compile the default block */` |
|      25 | 11388 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 11389 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 11390 | `				return SXERR_ABORT;` |
|      25 | 11391 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 11392 | `				break;` |
|       1 | 11393 | `			}` |
|      98 | 11394 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 11395 | `			ph7_case_expr sCase;` |
|       - | 11396 | `			/* Standard case block */` |
|      97 | 11397 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 11398 | `			/* initialize the structure */` |
|      97 | 11399 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 11400 | `			/* Compile the case expression */` |
|      97 | 11401 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 11402 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11403 | `				return SXERR_ABORT;` |
|       - | 11404 | `			}` |
|       - | 11405 | `			/* Compile the case block */` |
|      97 | 11406 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 11407 | `			/* Insert in the switch container */` |
|      97 | 11408 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 11409 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 11410 | `				return SXERR_ABORT;` |
|      97 | 11411 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 11412 | `				break;` |
|       - | 11413 | `			}` |
|      47 | 11414 | `		}else{` |
|       - | 11415 | `			/* Unexpected token */` |
|     ! 0 | 11416 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11417 | `				&pGen->pIn->sData);` |
|     ! 0 | 11418 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11419 | `				return SXERR_ABORT;` |
|       - | 11420 | `			}` |
|     ! 0 | 11421 | `			break;` |
|       - | 11422 | `		}` |
|       5 | 11423 | `	}` |
|       - | 11424 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 11425 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 11426 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11427 | `	/* Release the loop block */` |
|      33 | 11428 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 11429 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 11430 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 11431 | `		pGen->pIn++;` |
|      14 | 11432 | `	}` |
|       - | 11433 | `	/* Statement successfully compiled */` |
|      33 | 11434 | `	return SXRET_OK;` |
|     ! 0 | 11435 | `Synchronize:` |
|       - | 11436 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 11437 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 11438 | `		pGen->pIn++;` |
|     ! 0 | 11439 | `	}` |
|     ! 0 | 11440 | `	return SXRET_OK;` |
|      19 | 11441 | `}` |
|       - | 11442 | `/*` |
|       - | 11443 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 11444 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 11445 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 11446 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 11447 | ` */` |
|       - | 11448 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 11449 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 11450 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 11451 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 11452 |  |
|       - | 11453 | `/*` |
|       - | 11454 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 11455 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 11456 | ` * patched entries from the pending set.` |
|       - | 11457 | ` */` |
| 2853604 | 11458 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 11459 | `{` |
| 2853609 | 11460 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 11461 | `	sxu32 nTarget;` |
|       - | 11462 | `	sxu32 *aIdx;` |
|       - | 11463 | `	sxu32 i;` |
| 2853609 | 11464 | `	if( nCur <= nBaseline ){` |
| 2853513 | 11465 | `		return;` |
|       - | 11466 | `	}` |
|     100 | 11467 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|     100 | 11468 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     204 | 11469 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     108 | 11470 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     108 | 11471 | `		if( pInstr ){` |
|     108 | 11472 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      52 | 11473 | `		}` |
|      56 | 11474 | `	}` |
|     100 | 11475 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1426807 | 11476 | `}` |
|       - | 11477 |  |
|       - | 11478 | `/*` |
|       - | 11479 | ` * By-reference out-parameters of builtin functions.` |
|       - | 11480 | ` *` |
|       - | 11481 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 11482 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 11483 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 11484 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 11485 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 11486 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 11487 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 11488 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 11489 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 11490 | ` * creates it" behaviour).` |
|       - | 11491 | ` *` |
|       - | 11492 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 11493 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 11494 | ` */` |
|  480544 | 11495 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 11496 | `{` |
|       - | 11497 | `	static const struct {` |
|       - | 11498 | `		const char *zName;` |
|       - | 11499 | `		sxu32 nByte;` |
|       - | 11500 | `		sxu32 mask;` |
|       - | 11501 | `	} aByRef[] = {` |
|       - | 11502 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 11503 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 11504 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 11505 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 11506 | `	};` |
|       - | 11507 | `	sxu32 i;` |
|  480549 | 11508 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    2349 | 11509 | `		return 0;` |
|       - | 11510 | `	}` |
| 2390733 | 11511 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1912622 | 11512 | `		if( pName->nByte == aByRef[i].nByte` |
|  980520 | 11513 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      99 | 11514 | `			return aByRef[i].mask;` |
|       - | 11515 | `		}` |
|  956269 | 11516 | `	}` |
|  478111 | 11517 | `	return 0;` |
|  240277 | 11518 | `}` |
|       - | 11519 | `/*` |
|       - | 11520 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 11521 | ` *` |
|       - | 11522 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 11523 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 11524 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 11525 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 11526 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 11527 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 11528 | ` */` |
|  480544 | 11529 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 11530 | `{` |
|       - | 11531 | `	SyToken *p, *pEnd;` |
|  480549 | 11532 | `	pOut->zString = 0;` |
|  480549 | 11533 | `	pOut->nByte = 0;` |
|  480549 | 11534 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 11535 | `		return;` |
|       - | 11536 | `	}` |
|  480549 | 11537 | `	p = pLeft->pStart;` |
|  480549 | 11538 | `	pEnd = pLeft->pEnd;` |
|       - | 11539 | `	/* Optional single leading namespace separator (absolute path). */` |
|  480549 | 11540 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|    3877 | 11541 | `		p++;` |
|    1936 | 11542 | `	}` |
|  480549 | 11543 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    2313 | 11544 | `		return;` |
|       - | 11545 | `	}` |
|       - | 11546 | `	/* Must be a single component: nothing follows the name token. */` |
|  478241 | 11547 | `	if( p + 1 != pEnd ){` |
|      41 | 11548 | `		return;` |
|       - | 11549 | `	}` |
|  478205 | 11550 | `	*pOut = p->sData;` |
|  240277 | 11551 | `}` |
|       - | 11552 | `/*` |
|       - | 11553 | ` * Generate bytecode for a given expression tree.` |
|       - | 11554 | ` * If something goes wrong while generating bytecode` |
|       - | 11555 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 11556 | ` * this function takes care of generating the appropriate` |
|       - | 11557 | ` * error message.` |
|       - | 11558 | ` */` |
| 3821368 | 11559 | `static sxi32 GenStateEmitExprCode(` |
|       - | 11560 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11561 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 11562 | `	sxi32 iFlags /* Control flags */` |
|       - | 11563 | `	)` |
|       5 | 11564 | `{` |
|       - | 11565 | `	VmInstr *pInstr;` |
|       - | 11566 | `	sxu32 nJmpIdx;` |
| 3821373 | 11567 | `	sxi32 iP1 = 0;` |
| 3821373 | 11568 | `	sxu32 iP2 = 0;` |
| 3821373 | 11569 | `	void *p3  = 0;` |
|       - | 11570 | `	sxi32 iVmOp;` |
|       - | 11571 | `	sxi32 rc;` |
| 3821373 | 11572 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3821373 | 11573 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 3821373 | 11574 | `	sxu32 nRhsNsBase = 0;` |
| 3821373 | 11575 | `	if( pNode->xCode ){` |
|       - | 11576 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 11577 | `		/* Compile node */` |
| 2385407 | 11578 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2385407 | 11579 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2385407 | 11580 | `		RE_SWAP_DELIMITER(pGen);` |
| 2385407 | 11581 | `		return rc;` |
|       - | 11582 | `	}` |
| 1435971 | 11583 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 11584 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 11585 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 11586 | `		return SXERR_ABORT;` |
|       - | 11587 | `	}` |
| 1435971 | 11588 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1435971 | 11589 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|       - | 11590 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|       - | 11591 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|       - | 11592 | `		 * and later errors are still reported. */` |
|       3 | 11593 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 11594 | `			"The (unset) cast is no longer supported");` |
|       3 | 11595 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11596 | `			return SXERR_ABORT;` |
|       - | 11597 | `		}` |
|       1 | 11598 | `	}` |
| 1435971 | 11599 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      65 | 11600 | `		sxu32 nJmp = 0;` |
|       - | 11601 | `		sxu32 nNcNsBase;` |
|       - | 11602 | `		VmInstr *pInstrFix;` |
|       - | 11603 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 11604 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 11605 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 11606 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 11607 | `		 * stack slot carries a writable nIdx. */` |
|      65 | 11608 | `		if( pNode->pRight ){` |
|      65 | 11609 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 11610 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|      65 | 11611 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11612 | `				return rc;` |
|       - | 11613 | `			}` |
|      65 | 11614 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 11615 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 11616 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 11617 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 11618 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 11619 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 11620 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 11621 | `			 * cascade for the actual write path stays correct. */` |
|      65 | 11622 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 11623 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      31 | 11624 | `				pInstrFix->iP2 = 3;` |
|      14 | 11625 | `			}` |
|      31 | 11626 | `		}` |
|       - | 11627 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      65 | 11628 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 11629 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      65 | 11630 | `		if( pNode->pLeft ){` |
|      65 | 11631 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 11632 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      65 | 11633 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11634 | `				return rc;` |
|       - | 11635 | `			}` |
|      65 | 11636 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      31 | 11637 | `		}` |
|       - | 11638 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      65 | 11639 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 11640 | `		/* Patch the short-circuit jump to land after the store. */` |
|      65 | 11641 | `		if( nJmp > 0 ){` |
|      65 | 11642 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      65 | 11643 | `			if( pInstrFix ){` |
|      65 | 11644 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      31 | 11645 | `			}` |
|      31 | 11646 | `		}` |
|      65 | 11647 | `		return SXRET_OK;` |
|       - | 11648 | `	}` |
| 1435909 | 11649 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 11650 | `		sxu32 nJz,nJmp;` |
|       - | 11651 | `		sxu32 nTernaryNsBase;` |
|       - | 11652 | `		/* Ternary operator require special handling */` |
|       - | 11653 | `		/* Phase#1: Compile the condition */` |
|    2675 | 11654 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2675 | 11655 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2675 | 11656 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11657 | `			return rc;` |
|       - | 11658 | `		}` |
|       - | 11659 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 11660 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 11661 | `		 * condition expression, not leak past the ternary. */` |
|    2675 | 11662 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2675 | 11663 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2675 | 11664 | `		if( pNode->pLeft ){` |
|       - | 11665 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 11666 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2607 | 11667 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 11668 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2607 | 11669 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2607 | 11670 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2607 | 11671 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11672 | `				return rc;` |
|       - | 11673 | `			}` |
|    2607 | 11674 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1306 | 11675 | `		}else{` |
|       - | 11676 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 11677 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 11678 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 11679 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 11680 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 11681 | `		}` |
|       - | 11682 | `		/* Phase#4: Emit the unconditional jump */` |
|    2675 | 11683 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 11684 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2675 | 11685 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2675 | 11686 | `		if( pInstr ){` |
|    2675 | 11687 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1335 | 11688 | `		}` |
|    2675 | 11689 | `		if( !pNode->pLeft ){` |
|       - | 11690 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 11691 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 11692 | `		}` |
|       - | 11693 | `		/* Phase#6: Compile the 'else' expression */` |
|    2675 | 11694 | `		if( pNode->pRight ){` |
|    2675 | 11695 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2675 | 11696 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2675 | 11697 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11698 | `				return rc;` |
|       - | 11699 | `			}` |
|    2675 | 11700 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1335 | 11701 | `		}` |
|    2675 | 11702 | `		if( nJmp > 0 ){` |
|       - | 11703 | `			/* Phase#7: Fix the unconditional jump */` |
|    2675 | 11704 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2675 | 11705 | `			if( pInstr ){` |
|    2675 | 11706 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1335 | 11707 | `			}` |
|    1335 | 11708 | `		}` |
|       - | 11709 | `		/* All done */` |
|    2675 | 11710 | `		return SXRET_OK;` |
|       - | 11711 | `	}` |
| 1433239 | 11712 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|       - | 11713 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|       - | 11714 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|       - | 11715 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|       - | 11716 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|       - | 11717 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|       - | 11718 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|       - | 11719 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|       - | 11720 | `		sxu32 nPipeNsBase;` |
|      27 | 11721 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|      27 | 11722 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|     ! 0 | 11723 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 11724 | `				"'\|>': Missing operand");` |
|     ! 0 | 11725 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - | 11726 | `		}` |
|       - | 11727 | `		/* Argument: the LHS value. */` |
|      27 | 11728 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      27 | 11729 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|      27 | 11730 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11731 | `			return rc;` |
|       - | 11732 | `		}` |
|      27 | 11733 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|       - | 11734 | `		/* Callable: the RHS. */` |
|      27 | 11735 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      27 | 11736 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|      27 | 11737 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11738 | `			return rc;` |
|       - | 11739 | `		}` |
|      27 | 11740 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|       - | 11741 | `		/* Invoke the callable with the single piped argument. */` |
|      27 | 11742 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      27 | 11743 | `		return SXRET_OK;` |
|       - | 11744 | `	}` |
| 1433213 | 11745 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 11746 | `	/* Generate code for the left tree */` |
| 1433213 | 11747 | `	if( pNode->pLeft ){` |
| 1433181 | 11748 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1433181 | 11749 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 11750 | `			ph7_expr_node **apNode;` |
|  484561 | 11751 | `			int hasSpread = 0;` |
|  484561 | 11752 | `			int hasNamed = 0;` |
|  484561 | 11753 | `			int bAnySpread = 0;` |
|  484561 | 11754 | `			sxu32 byRefMask = 0;` |
|       - | 11755 | `			sxi32 nArgs;` |
|       - | 11756 | `			sxi32 n;` |
|       - | 11757 | `			/* Recurse and generate bytecodes for function arguments */` |
|  484561 | 11758 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  484561 | 11759 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 11760 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|       - | 11761 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|       - | 11762 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  484561 | 11763 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|      77 | 11764 | `				bFcc = 1;` |
|      77 | 11765 | `				nArgs = 0;` |
|      38 | 11766 | `			}` |
|       - | 11767 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 11768 | `			{` |
|  484561 | 11769 | `				int seenNamed = 0;` |
|  983119 | 11770 | `				for( n = 0; n < nArgs; ++n ){` |
|  498565 | 11771 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     253 | 11772 | `						seenNamed = 1;` |
|     253 | 11773 | `						hasNamed = 1;` |
|  498441 | 11774 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|    3877 | 11775 | `						bAnySpread = 1;` |
|  496381 | 11776 | `					}else if( seenNamed ){` |
|       3 | 11777 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 11778 | `							"Cannot use positional argument after named argument");` |
|       3 | 11779 | `						return SXERR_SYNTAX;` |
|       - | 11780 | `					}` |
|  249284 | 11781 | `				}` |
|       - | 11782 | `			}` |
|       - | 11783 | `			/* Read-only load */` |
|  484559 | 11784 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 11785 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 11786 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 11787 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 11788 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  484559 | 11789 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  484559 | 11790 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  484554 | 11791 | `				if( pCallName->nByte == 5` |
|  264546 | 11792 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   23403 | 11793 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  472860 | 11794 | `				}else if( pCallName->nByte == 5` |
|  241148 | 11795 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      99 | 11796 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      47 | 11797 | `				}` |
|       - | 11798 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 11799 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 11800 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 11801 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 11802 | `				 * the compile-time positional index no longer maps to the` |
|       - | 11803 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  484559 | 11804 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 11805 | `					SyString sBuiltin;` |
|  480549 | 11806 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  480549 | 11807 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  240272 | 11808 | `				}` |
|  242277 | 11809 | `			}` |
|  983115 | 11810 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  498561 | 11811 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  498561 | 11812 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11813 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 11814 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|       - | 11815 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|       - | 11816 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|       - | 11817 | `				 * builtin to write back through. A plain $var target is unaffected` |
|       - | 11818 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  498561 | 11819 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      55 | 11820 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      55 | 11821 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      25 | 11822 | `				}` |
|  498561 | 11823 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  498561 | 11824 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11825 | `					return rc;` |
|       - | 11826 | `				}` |
|       - | 11827 | `				/* Each argument is an independent nullsafe scope. */` |
|  498561 | 11828 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  498561 | 11829 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 11830 | `					/* Emit spread opcode to unpack this array argument */` |
|    3877 | 11831 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|    3877 | 11832 | `					hasSpread = 1;` |
|    1936 | 11833 | `				}` |
|  249283 | 11834 | `			}` |
|       - | 11835 | `			/* Total number of given arguments */` |
|  484559 | 11836 | `			iP1 = nArgs;` |
|  484559 | 11837 | `			iP2 = hasSpread;` |
|       - | 11838 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 11839 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  484559 | 11840 | `			if( hasNamed ){` |
|     142 | 11841 | `				sxu32 nStrBytes = 0;` |
|       - | 11842 | `				char *zBuf;` |
|     424 | 11843 | `				for( n = 0; n < nArgs; ++n ){` |
|     286 | 11844 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     250 | 11845 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|     123 | 11846 | `					}` |
|     145 | 11847 | `				}` |
|       - | 11848 | `				{` |
|     142 | 11849 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     142 | 11850 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|     138 | 11851 | `					&pGen->pVm->sAllocator, mapSize);` |
|     142 | 11852 | `				if( pMap ){` |
|     142 | 11853 | `					SyZero(pMap, mapSize);` |
|     142 | 11854 | `					pMap->bHasNamed = 1;` |
|     142 | 11855 | `					pMap->nTotal = (sxu32)nArgs;` |
|     142 | 11856 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     142 | 11857 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     424 | 11858 | `					for( n = 0; n < nArgs; ++n ){` |
|     286 | 11859 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     250 | 11860 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     250 | 11861 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     250 | 11862 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     250 | 11863 | `							zBuf += nb;` |
|     123 | 11864 | `						}` |
|       - | 11865 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     145 | 11866 | `					}` |
|     142 | 11867 | `					p3 = (void *)pMap;` |
|      69 | 11868 | `				}` |
|       - | 11869 | `				}` |
|      69 | 11870 | `			}` |
|       - | 11871 | `			/* Remove stale flags now */` |
|  484559 | 11872 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  242277 | 11873 | `		}` |
|       - | 11874 | `		{` |
|       - | 11875 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|       - | 11876 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|       - | 11877 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|       - | 11878 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|       - | 11879 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|       - | 11880 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|       - | 11881 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|       - | 11882 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 1433179 | 11883 | `			sxi32 iLeftFlags = iFlags;` |
| 1617565 | 11884 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  909756 | 11885 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  377586 | 11886 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  368813 | 11887 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   17769 | 11888 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|    8882 | 11889 | `			}` |
|       - | 11890 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|       - | 11891 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|       - | 11892 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|       - | 11893 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|       - | 11894 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|       - | 11895 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|       - | 11896 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 2053859 | 11897 | `			if( pNode->pOp` |
| 1433179 | 11898 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 1337324 | 11899 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 1241422 | 11900 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  192187 | 11901 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   96091 | 11902 | `			}` |
| 1433179 | 11903 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|       - | 11904 | `		}` |
| 1433179 | 11905 | `		if( rc != SXRET_OK ){` |
|      34 | 11906 | `			return rc;` |
|       - | 11907 | `		}` |
| 1433149 | 11908 | `		if( !bIsChainOp ){` |
|       - | 11909 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 11910 | `			 * target the end of that LHS chain, which is right here. */` |
|  657585 | 11911 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  328790 | 11912 | `		}` |
| 1433149 | 11913 | `		if( iVmOp == PH7_OP_CALL ){` |
|  484559 | 11914 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  484559 | 11915 | `			if( pInstr ){` |
|  484559 | 11916 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  478359 | 11917 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 11918 | `					sxu32 nQual;` |
|  478359 | 11919 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11920 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 11921 | `					 * so the later NEW handler (if any) can see it. */` |
|  478359 | 11922 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 11923 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 11924 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 11925 | `					 * imports — class imports must NOT affect function` |
|       - | 11926 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 11927 | `					 * before NEW; we store the original literal index in the` |
|       - | 11928 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 11929 | `					 * the unqualified name and re-qualify with class imports. */` |
|  478359 | 11930 | `					if( bAbsolute ){` |
|    3877 | 11931 | `						pInstr->iP2 = (sxi32)nOrig;` |
|    1941 | 11932 | `					}else{` |
|  474487 | 11933 | `						int fromImport = 0;` |
|  474487 | 11934 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  474487 | 11935 | `						pInstr->iP2 = (sxi32)nQual;` |
|  474487 | 11936 | `						if( nQual != nOrig ){` |
|       - | 11937 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 11938 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 11939 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 11940 | `							if( !fromImport ){` |
|       - | 11941 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 11942 | `								if( p3 == 0 ){` |
|      67 | 11943 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 11944 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 11945 | `									if( pMap ){` |
|      67 | 11946 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 11947 | `										p3 = (void *)pMap;` |
|      31 | 11948 | `									}` |
|      31 | 11949 | `								}` |
|      67 | 11950 | `								if( p3 ){` |
|      67 | 11951 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 11952 | `								}` |
|      31 | 11953 | `							}` |
|      36 | 11954 | `						}` |
|       5 | 11955 | `					}` |
|  245382 | 11956 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 11957 | `					/* Method call,flag that */` |
|    1899 | 11958 | `					pInstr->iP2 = 1;` |
|     947 | 11959 | `				}` |
|  242282 | 11960 | `			}` |
| 1190872 | 11961 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 11962 | `			ph7_expr_node **apNode;` |
|       - | 11963 | `			sxi32 n;` |
|   98833 | 11964 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 11965 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 11966 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11967 | `			/* Recurse and generate bytecodes for array index */` |
|   98833 | 11968 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  178329 | 11969 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   79501 | 11970 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   79501 | 11971 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   79501 | 11972 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11973 | `					return rc;` |
|       - | 11974 | `				}` |
|       - | 11975 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   79501 | 11976 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   39753 | 11977 | `			}` |
|   98833 | 11978 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   79501 | 11979 | `				iP1 = 1; /* Node have an index associated with it */` |
|   39748 | 11980 | `			}` |
|   98833 | 11981 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 11982 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     245 | 11983 | `				iP2 = 4;` |
|   98713 | 11984 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 11985 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 11986 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      64 | 11987 | `				iP2 = 5;` |
|   98563 | 11988 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 11989 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 11990 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 11991 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 11992 | `				iP2 = 6;` |
|   98521 | 11993 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 11994 | `				/* Create an empty entry when the desired index is not found */` |
|   38991 | 11995 | `				iP2 = 1;` |
|   19498 | 11996 | `			}` |
|  899181 | 11997 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 11998 | `			/* POP the left node */` |
|      32 | 11999 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 12000 | `		}` |
|  716572 | 12001 | `	}` |
| 1433181 | 12002 | `	rc = SXRET_OK;` |
| 1433181 | 12003 | `	nJmpIdx = 0;` |
|       - | 12004 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 12005 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 12006 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1433181 | 12007 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     415 | 12008 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     415 | 12009 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     415 | 12010 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     415 | 12011 | `			int isSpecial = 0;` |
|     415 | 12012 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     323 | 12013 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     323 | 12014 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     335 | 12015 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     287 | 12016 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     146 | 12017 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|     111 | 12018 | `					isSpecial = 1;` |
|      53 | 12019 | `				}` |
|     182 | 12020 | `			}` |
|     461 | 12021 | `			pInstr->iP1 = 0;` |
|     461 | 12022 | `			if( !isSpecial ){` |
|     263 | 12023 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     129 | 12024 | `			}` |
|       - | 12025 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 12026 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     369 | 12027 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     263 | 12028 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     263 | 12029 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      46 | 12030 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      48 | 12031 | `					return SXRET_OK;` |
|       - | 12032 | `				}` |
|     107 | 12033 | `			}` |
|     160 | 12034 | `		}` |
|     231 | 12035 | `	}` |
|       - | 12036 | `	/* Generate code for the right tree */` |
| 1433105 | 12037 | `	if( pNode->pRight ){` |
|  772795 | 12038 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 12039 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   12047 | 12040 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  766774 | 12041 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 12042 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    4029 | 12043 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  758741 | 12044 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 12045 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     135 | 12046 | `			iVmOp = 0; /* No binary operator to emit */` |
|     135 | 12047 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  756716 | 12048 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 12049 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 12050 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 12051 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 12052 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 12053 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 12054 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     108 | 12055 | `			sxu32 nNsJmp = 0;` |
|     108 | 12056 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     108 | 12057 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  756547 | 12058 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|       - | 12059 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|       - | 12060 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|       - | 12061 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  321491 | 12062 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  160743 | 12063 | `		}` |
|  772795 | 12064 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  772795 | 12065 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  772795 | 12066 | `		if( !bIsChainOp ){` |
|       - | 12067 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 12068 | `			 * operator instruction is emitted. */` |
|  580657 | 12069 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  290326 | 12070 | `		}` |
|  772795 | 12071 | `		if( iVmOp == PH7_OP_STORE ){` |
|  317365 | 12072 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  317330 | 12073 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 12074 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 12075 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 12076 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 12077 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 12078 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 12079 | `				 */` |
|      85 | 12080 | `				iVmOp = 0;` |
|  317325 | 12081 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  317285 | 12082 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 12083 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   85083 | 12084 | `					iP2 = 1;` |
|   42544 | 12085 | `				}else{` |
|  232207 | 12086 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 12087 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   38909 | 12088 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   38909 | 12089 | `						iP1 = pInstr->iP1;` |
|   19457 | 12090 | `					}else{` |
|  193303 | 12091 | `						p3 = pInstr->p3;` |
|       - | 12092 | `					}` |
|       - | 12093 | `					/* POP the last dynamic load instruction */` |
|  232207 | 12094 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 12095 | `				}` |
|  158645 | 12096 | `			}` |
|  614115 | 12097 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      61 | 12098 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      61 | 12099 | `			if( pInstr ){` |
|      61 | 12100 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 12101 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 12102 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 12103 | `					 */` |
|      19 | 12104 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      19 | 12105 | `					iP1 = pInstr->iP1;` |
|      19 | 12106 | `					iP2 = pInstr->iP2;` |
|      19 | 12107 | `					p3  = pInstr->p3;` |
|      10 | 12108 | `				}else{` |
|      43 | 12109 | `					p3 = pInstr->p3;` |
|       - | 12110 | `				}` |
|      29 | 12111 | `			}` |
|      29 | 12112 | `		}` |
|  386395 | 12113 | `	}` |
| 1433100 | 12114 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   12524 | 12115 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|       - | 12116 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|       - | 12117 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|      30 | 12118 | `		iVmOp = 0;` |
|      13 | 12119 | `	}` |
| 1433105 | 12120 | `	if( iVmOp > 0 ){` |
| 1432839 | 12121 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   15793 | 12122 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 12123 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|   11557 | 12124 | `				iP1 = 1;` |
|    5781 | 12125 | `			}` |
| 1424945 | 12126 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 12127 | `			/* Namespace-qualify the class name for NEW */ {` |
|   24797 | 12128 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   24797 | 12129 | `				VmInstr *pCallInstr = 0;` |
|   24797 | 12130 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   24603 | 12131 | `					pCallInstr = pPeek;` |
|   24603 | 12132 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   12299 | 12133 | `				}` |
|   24797 | 12134 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   24793 | 12135 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 12136 | `					sxu32 nLitForClass;` |
|       - | 12137 | `					/* If the CALL handler already qualified the name using` |
|       - | 12138 | `					 * function imports, recover the original unqualified` |
|       - | 12139 | `					 * literal so we can re-qualify with class imports. */` |
|   24793 | 12140 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 12141 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 12142 | `					}else{` |
|   24761 | 12143 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 12144 | `					}` |
|   24793 | 12145 | `					pPeek->iP1 = 0;` |
|   24793 | 12146 | `					if( !bAbsolute ){` |
|   20925 | 12147 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|   10465 | 12148 | `					}else{` |
|    3873 | 12149 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 12150 | `					}` |
|   12394 | 12151 | `				}` |
|       - | 12152 | `			}` |
|   24797 | 12153 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   24797 | 12154 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 12155 | `				VmInstr *pPrev;` |
|   24603 | 12156 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   24603 | 12157 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 12158 | `					/* Pop the call instruction, preserve named-arg map */` |
|   24603 | 12159 | `					iP1 = pInstr->iP1;` |
|   24603 | 12160 | `					if( pInstr->p3 ){` |
|      43 | 12161 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 12162 | `					}` |
|   24603 | 12163 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   12299 | 12164 | `				}` |
|   12304 | 12165 | `			}` |
| 1404655 | 12166 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 12167 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 12168 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     203 | 12169 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     203 | 12170 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     203 | 12171 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     203 | 12172 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     203 | 12173 | `				int isSpecialIs = 0;` |
|     203 | 12174 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     203 | 12175 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     203 | 12176 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     203 | 12177 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     196 | 12178 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      99 | 12179 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 12180 | `						isSpecialIs = 1;` |
|       5 | 12181 | `					}` |
|      99 | 12182 | `				}` |
|     203 | 12183 | `				pInstr->iP1 = 0;` |
|     203 | 12184 | `				if( !isSpecialIs && !bAbsolute ){` |
|     183 | 12185 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      89 | 12186 | `				}` |
|     104 | 12187 | `			}` |
| 1392160 | 12188 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 12189 | `			/* Prevent constant expansion for member/property names.` |
|       - | 12190 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 12191 | `			 * should not trigger constant lookup. */` |
|  192143 | 12192 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  192143 | 12193 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  192093 | 12194 | `				pInstr->iP1 = 0;` |
|   96044 | 12195 | `			}` |
|  192143 | 12196 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 12197 | `				/* Static member access,remember that */` |
|     339 | 12198 | `				iP1 = 1;` |
|     339 | 12199 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     339 | 12200 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      44 | 12201 | `					p3 = pInstr->p3;` |
|      44 | 12202 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      20 | 12203 | `				}` |
|     167 | 12204 | `			}` |
|       - | 12205 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|       - | 12206 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|       - | 12207 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|       - | 12208 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  192143 | 12209 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  192143 | 12210 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|      30 | 12211 | `					iP2 = PH7_MEMBER_UNSET;` |
|  192129 | 12212 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|      77 | 12213 | `					iP2 = PH7_MEMBER_ISSET;` |
|  192079 | 12214 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|      13 | 12215 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  192037 | 12216 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|       - | 12217 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   85163 | 12218 | `					iP2 = PH7_MEMBER_WRITE;` |
|   42579 | 12219 | `				}` |
|   96069 | 12220 | `			}` |
|   96069 | 12221 | `		}` |
|       - | 12222 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|       - | 12223 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|       - | 12224 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|       - | 12225 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|       - | 12226 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 1432839 | 12227 | `		if( bFcc ){` |
|      77 | 12228 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|      77 | 12229 | `			iP2 = 0;` |
|      77 | 12230 | `			p3 = 0;` |
|      77 | 12231 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      77 | 12232 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 12233 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|       - | 12234 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|       - | 12235 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|       - | 12236 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|      35 | 12237 | `				void *pMemberName = pInstr->p3;` |
|      35 | 12238 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|      35 | 12239 | `				if( pMemberName ){` |
|       3 | 12240 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       1 | 12241 | `				}` |
|      35 | 12242 | `				iP1 = 2;` |
|      18 | 12243 | `			}else{` |
|      43 | 12244 | `				iP1 = 1;` |
|       - | 12245 | `			}` |
|      38 | 12246 | `		}` |
|       - | 12247 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 12248 | `		 * This is the primary emit path for user-visible calls. */` |
| 1432839 | 12249 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  509275 | 12250 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  254635 | 12251 | `		}` |
|       - | 12252 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1432839 | 12253 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  716417 | 12254 | `	}` |
| 1433105 | 12255 | `	if( nJmpIdx > 0 ){` |
|       - | 12256 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   16201 | 12257 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   16201 | 12258 | `		if( pInstr ){` |
|   16201 | 12259 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    8098 | 12260 | `		}` |
|    8098 | 12261 | `	}` |
| 1433105 | 12262 | `	return rc;` |
| 1910673 | 12263 | `}` |
|       - | 12264 | `/*` |
|       - | 12265 | ` * Compile a PHP expression.` |
|       - | 12266 | ` * According to the PHP language reference manual:` |
|       - | 12267 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 12268 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 12269 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 12270 | ` *  is "anything that has a value".` |
|       - | 12271 | ` * If something goes wrong while compiling the expression,this` |
|       - | 12272 | ` * function takes care of generating the appropriate error` |
|       - | 12273 | ` * message.` |
|       - | 12274 | ` */` |
| 1029394 | 12275 | `static sxi32 PH7_CompileExpr(` |
|       - | 12276 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 12277 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 12278 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 12279 | `	)` |
|       5 | 12280 | `{` |
|       - | 12281 | `	ph7_expr_node *pRoot;` |
|       - | 12282 | `	SySet sExprNode;` |
|       - | 12283 | `	SyToken *pEnd;` |
|       - | 12284 | `	sxi32 nExpr;` |
|       - | 12285 | `	sxi32 iNest;` |
|       - | 12286 | `	sxi32 rc;` |
|       - | 12287 | `	sxu32 nNullsafeBase;` |
|       - | 12288 | `	/* Initialize worker variables */` |
| 1029399 | 12289 | `	nExpr = 0;` |
| 1029399 | 12290 | `	pRoot = 0;` |
|       - | 12291 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 12292 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
| 1029399 | 12293 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1029399 | 12294 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
| 1029399 | 12295 | `	SySetAlloc(&sExprNode,0x10);` |
| 1029399 | 12296 | `	rc = SXRET_OK;` |
|       - | 12297 | `	/* Delimit the expression */` |
| 1029399 | 12298 | `	pEnd = pGen->pIn;` |
| 1029399 | 12299 | `	iNest = 0;` |
| 6945693 | 12300 | `	while( pEnd < pGen->pEnd ){` |
| 6591625 | 12301 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 12302 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     563 | 12303 | `			iNest++;` |
| 6591346 | 12304 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     571 | 12305 | `			iNest--;` |
| 6590784 | 12306 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  675767 | 12307 | `			if( iNest <= 0 ){` |
|  675331 | 12308 | `				break;` |
|       - | 12309 | `			}` |
|     218 | 12310 | `		}` |
| 5916299 | 12311 | `		pEnd++;` |
|       5 | 12312 | `	}` |
| 1029399 | 12313 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   23669 | 12314 | `		SyToken *pEnd2 = pGen->pIn;` |
|   23669 | 12315 | `		iNest = 0;` |
|       - | 12316 | `		/* Stop at the first comma */` |
|   47651 | 12317 | `		while( pEnd2 < pEnd ){` |
|   23993 | 12318 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      71 | 12319 | `				iNest++;` |
|   23960 | 12320 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      71 | 12321 | `				iNest--;` |
|   23894 | 12322 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      57 | 12323 | `				if( iNest <= 0 ){` |
|       7 | 12324 | `					break;` |
|       - | 12325 | `				}` |
|      23 | 12326 | `			}` |
|   23987 | 12327 | `			pEnd2++;` |
|       5 | 12328 | `		}` |
|   23669 | 12329 | `		if( pEnd2 <pEnd ){` |
|       7 | 12330 | `			pEnd = pEnd2;` |
|       3 | 12331 | `		}` |
|   11832 | 12332 | `	}` |
| 1029399 | 12333 | `	if( pEnd > pGen->pIn ){` |
| 1029389 | 12334 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 12335 | `		/* Swap delimiter */` |
| 1029389 | 12336 | `		pGen->pEnd = pEnd;` |
|       - | 12337 | `		/* Try to get an expression tree */` |
| 1029389 | 12338 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
| 1029389 | 12339 | `		if( rc == SXRET_OK && pRoot ){` |
| 1029207 | 12340 | `			rc = SXRET_OK;` |
| 1029207 | 12341 | `			if( xTreeValidator ){` |
|       - | 12342 | `				/* Call the upper layer validator callback */` |
|   31051 | 12343 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   15523 | 12344 | `			}` |
| 1029207 | 12345 | `			if( rc != SXERR_ABORT ){` |
|       - | 12346 | `				/* Generate code for the given tree */` |
| 1029207 | 12347 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 12348 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 12349 | `				 * expression so they short-circuit to its end. */` |
| 1029207 | 12350 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  514601 | 12351 | `			}` |
| 1029207 | 12352 | `			nExpr = 1;` |
|  514601 | 12353 | `		}` |
|       - | 12354 | `		/* Release the whole tree */` |
| 1029389 | 12355 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 12356 | `		/* Synchronize token stream */` |
| 1029389 | 12357 | `		pGen->pEnd = pTmp;` |
| 1029389 | 12358 | `		pGen->pIn  = pEnd;` |
| 1029389 | 12359 | `		if( rc == SXERR_ABORT ){` |
|      13 | 12360 | `			SySetRelease(&sExprNode);` |
|      13 | 12361 | `			return SXERR_ABORT;` |
|       - | 12362 | `		}` |
|  514687 | 12363 | `	}` |
| 1029389 | 12364 | `	SySetRelease(&sExprNode);` |
| 1029389 | 12365 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  514702 | 12366 | `}` |
|       - | 12367 | `/*` |
|       - | 12368 | ` * Return a pointer to the node construct handler associated` |
|       - | 12369 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 12370 | ` */` |
|  268670 | 12371 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 12372 | `{` |
|  268675 | 12373 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 12374 | `		/* Numeric literal: Either real or integer */` |
|  136043 | 12375 | `		return PH7_CompileNumLiteral;` |
|  132637 | 12376 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 12377 | `		/* Double quoted string */` |
|   25417 | 12378 | `		return PH7_CompileString;` |
|  107225 | 12379 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 12380 | `		/* Single quoted string */` |
|  107105 | 12381 | `		return PH7_CompileSimpleString;` |
|     125 | 12382 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 12383 | `		/* Heredoc */` |
|      71 | 12384 | `		return PH7_CompileHereDoc;` |
|      58 | 12385 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 12386 | `		/* Nowdoc */` |
|      51 | 12387 | `		return PH7_CompileNowDoc;` |
|       9 | 12388 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 12389 | `		/* Backtick quoted string */` |
|       6 | 12390 | `		return PH7_CompileBacktic;` |
|       - | 12391 | `	}` |
|       3 | 12392 | `	return 0;` |
|  134340 | 12393 | `}` |
|       - | 12394 | `/*` |
|       - | 12395 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 12396 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 12397 | ` * in write context" parse error.` |
|       - | 12398 | ` */` |
|    6742 | 12399 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 12400 | `{` |
|       - | 12401 | `	sxi32 rc;` |
|    6747 | 12402 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6745 | 12403 | `		return SXRET_OK;` |
|       - | 12404 | `	}` |
|       5 | 12405 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 12406 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 12407 | `		"Can't use nullsafe operator in write context");` |
|       3 | 12408 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3376 | 12409 | `}` |
|       - | 12410 | `/*` |
|       - | 12411 | ` * Compile an unset() statement.` |
|       - | 12412 | ` * unset($var, $arr[$key], ...);` |
|       - | 12413 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 12414 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 12415 | ` * parent array before extracting the element to unset.` |
|       - | 12416 | ` */` |
|    2892 | 12417 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 12418 | `{` |
|    2897 | 12419 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2897 | 12420 | `	sxu32 nIdx = 0;` |
|       - | 12421 | `	SyString sName;` |
|       - | 12422 | `	sxi32 rc;` |
|       - | 12423 | `	/* Jump the 'unset' keyword */` |
|    2897 | 12424 | `	pGen->pIn++;` |
|       - | 12425 | `	/* Save delimiter */` |
|    2897 | 12426 | `	pTmp = pGen->pEnd;` |
|       - | 12427 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2897 | 12428 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2897 | 12429 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 12430 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 12431 | `		SyToken *pClose;` |
|    2897 | 12432 | `		pGen->pIn++;   /* Skip '(' */` |
|    2897 | 12433 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2897 | 12434 | `		pEnd = pClose; /* Stop at ')' */` |
|    1446 | 12435 | `	}` |
|    2897 | 12436 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 12437 | `	/* Resolve the 'unset' builtin name once */` |
|    2897 | 12438 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     369 | 12439 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     369 | 12440 | `		if( pObj == 0 ){` |
|     ! 0 | 12441 | `			return SXERR_ABORT;` |
|       - | 12442 | `		}` |
|     369 | 12443 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     369 | 12444 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     182 | 12445 | `	}` |
|       - | 12446 | `	/* Compile each comma-separated argument */` |
|    9641 | 12447 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6749 | 12448 | `		if( pGen->pIn < pNext ){` |
|    6749 | 12449 | `			pGen->pEnd = pNext;` |
|    6749 | 12450 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 12451 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 12452 | `				GenStateUnsetValidator);` |
|    6749 | 12453 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 12454 | `				return SXERR_ABORT;` |
|       - | 12455 | `			}` |
|    6749 | 12456 | `			if( rc != SXERR_EMPTY ){` |
|       - | 12457 | `				/* Emit call for this single argument */` |
|    6747 | 12458 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6747 | 12459 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6747 | 12460 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3371 | 12461 | `			}` |
|    3372 | 12462 | `		}` |
|       - | 12463 | `		/* Jump trailing commas */` |
|   10603 | 12464 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3859 | 12465 | `			pNext++;` |
|       5 | 12466 | `		}` |
|    6749 | 12467 | `		pGen->pIn = pNext;` |
|       5 | 12468 | `	}` |
|       - | 12469 | `	/* Skip past the closing ')' if present */` |
|    2897 | 12470 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2897 | 12471 | `		pGen->pIn++;` |
|    1446 | 12472 | `	}` |
|       - | 12473 | `	/* Restore token stream */` |
|    2897 | 12474 | `	pGen->pEnd = pTmp;` |
|    2897 | 12475 | `	return SXRET_OK;` |
|    1451 | 12476 | `}` |
|       - | 12477 | `/*` |
|       - | 12478 | ` * PHP Language construct table.` |
|       - | 12479 | ` */` |
|       - | 12480 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 12481 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 12482 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 12483 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 12484 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 12485 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 12486 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 12487 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 12488 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 12489 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 12490 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 12491 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 12492 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 12493 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 12494 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 12495 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 12496 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 12497 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 12498 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 12499 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 12500 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 12501 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 12502 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 12503 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 12504 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 12505 | `};` |
|       - | 12506 | `/*` |
|       - | 12507 | ` * Return a pointer to the statement handler routine associated` |
|       - | 12508 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 12509 | ` */` |
|  697078 | 12510 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 12511 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 12512 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 12513 | `	)` |
|       5 | 12514 | `{` |
|  697083 | 12515 | `	sxu32 n = 0;` |
| 3669752 | 12516 | `	for(;;){` |
| 7339509 | 12517 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  151747 | 12518 | `			break;` |
|       - | 12519 | `		}` |
| 7187767 | 12520 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  545341 | 12521 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 12522 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 12523 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 12524 | `					/* 'static' (class context),return null */` |
|     ! 0 | 12525 | `					return 0;` |
|       - | 12526 | `				}` |
|     ! 0 | 12527 | `			}` |
|  545336 | 12528 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       8 | 12529 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       9 | 12530 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 12531 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 12532 | `				return 0;` |
|       - | 12533 | `			}` |
|       - | 12534 | `			/* Return a pointer to the handler.` |
|       - | 12535 | `			*/` |
|  545341 | 12536 | `			return aLangConstruct[n].xConstruct;` |
|       - | 12537 | `		}` |
| 6642431 | 12538 | `		n++;` |
|       5 | 12539 | `	}` |
|  151747 | 12540 | `	if( pLookahed ){` |
|  151747 | 12541 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   42373 | 12542 | `			return PH7_CompileClassInterface;` |
|  109379 | 12543 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|  108889 | 12544 | `			return PH7_CompileClass;` |
|     495 | 12545 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      69 | 12546 | `			return PH7_CompileTrait;` |
|       - | 12547 | `		}` |
|       - | 12548 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 12549 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 12550 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 12551 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     213 | 12552 | `	}` |
|       - | 12553 | `	/* Not a language construct */` |
|     431 | 12554 | `	return 0;` |
|  348544 | 12555 | `}` |
|       - | 12556 | `/*` |
|       - | 12557 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 12558 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 12559 | ` */` |
|     426 | 12560 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 12561 | `{` |
|       - | 12562 | `	int rc;` |
|     431 | 12563 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     431 | 12564 | `	if( rc == FALSE ){` |
|     312 | 12565 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     311 | 12566 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 12567 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 12568 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 12569 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 12570 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 12571 | `			*/` |
|       - | 12572 | `			){` |
|     309 | 12573 | `				rc = TRUE;` |
|     152 | 12574 | `		}` |
|     156 | 12575 | `	}` |
|     431 | 12576 | `	return rc;` |
|       5 | 12577 | `}` |
|       - | 12578 | `/*` |
|       - | 12579 | ` * Compile a PHP chunk.` |
|       - | 12580 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 12581 | ` * takes care of generating the appropriate error message.` |
|       - | 12582 | ` */` |
|  832736 | 12583 | `static sxi32 GenStateCompileChunk(` |
|       - | 12584 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 12585 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 12586 | `	)` |
|       5 | 12587 | `{` |
|       - | 12588 | `	ProcLangConstruct xCons;` |
|       - | 12589 | `	sxi32 rc;` |
|  832741 | 12590 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  655788 | 12591 | `	for(;;){` |
| 1072161 | 12592 | `		int bStmtIsDeclare = 0;` |
| 1072161 | 12593 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 12594 | `			/* No more input to process */` |
|   18123 | 12595 | `			break;` |
|       - | 12596 | `		}` |
|       - | 12597 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 12598 | `		 * below doesn't fire before the directive has a chance to run. */` |
| 1054043 | 12599 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  700963 | 12600 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  700963 | 12601 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      47 | 12602 | `				bStmtIsDeclare = 1;` |
|      21 | 12603 | `			}` |
|  350479 | 12604 | `		}` |
| 1054043 | 12605 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 12606 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 12607 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  239393 | 12608 | `			pGen->bStrictTypesLocked = 1;` |
|  119694 | 12609 | `		}` |
| 1054043 | 12610 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 12611 | `			/* Compile block */` |
|    3867 | 12612 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|    3867 | 12613 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 12614 | `				break;` |
|       - | 12615 | `			}` |
|    1936 | 12616 | `		}else{` |
| 1050181 | 12617 | `			xCons = 0;` |
| 1050181 | 12618 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 12619 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 12620 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 12621 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    3911 | 12622 | `				xCons = PH7_CompileClassModifiers;` |
| 1048228 | 12623 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  697083 | 12624 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 12625 | `				/* Try to extract a language construct handler */` |
|  697083 | 12626 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  697083 | 12627 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 12628 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 12629 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 12630 | `						&pGen->pIn->sData);` |
|       9 | 12631 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 12632 | `						break;` |
|       - | 12633 | `					}` |
|       - | 12634 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 12635 | `					 * this erroneous statement.` |
|       - | 12636 | `					 */` |
|       9 | 12637 | `					xCons = PH7_ErrorRecover;` |
|       4 | 12638 | `				}` |
|  697736 | 12639 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   57693 | 12640 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 12641 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 12642 | `				xCons = PH7_CompileLabel;` |
|      56 | 12643 | `			}` |
| 1050181 | 12644 | `			if( xCons == 0 ){` |
|       - | 12645 | `				/* Assume an expression an try to compile it */` |
|  349503 | 12646 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  349503 | 12647 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 12648 | `					/* Pop l-value */` |
|  349353 | 12649 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  174674 | 12650 | `				}` |
|  174754 | 12651 | `			}else{` |
|       - | 12652 | `				/* Go compile the sucker */` |
|  700683 | 12653 | `				rc = xCons(&(*pGen));` |
|       - | 12654 | `			}` |
| 1050181 | 12655 | `			if( rc == SXERR_ABORT ){` |
|       - | 12656 | `				/* Request to abort compilation */` |
|      13 | 12657 | `				break;` |
|       - | 12658 | `			}` |
|       - | 12659 | `		}` |
|       - | 12660 | `		/* Ignore trailing semi-colons ';' */` |
| 1696995 | 12661 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  642967 | 12662 | `			pGen->pIn++;` |
|       5 | 12663 | `		}` |
| 1054033 | 12664 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 12665 | `			/* Compile a single statement and return */` |
|  814613 | 12666 | `			break;` |
|       - | 12667 | `		}` |
|       - | 12668 | `		/* LOOP ONE */` |
|       - | 12669 | `		/* LOOP TWO */` |
|       - | 12670 | `		/* LOOP THREE */` |
|       - | 12671 | `		/* LOOP FOUR */` |
|       5 | 12672 | `	}` |
|       - | 12673 | `	/* Return compilation status */` |
|  832741 | 12674 | `	return rc;` |
|       5 | 12675 | `}` |
|       - | 12676 | `/*` |
|       - | 12677 | ` * Compile a Raw PHP chunk.` |
|       - | 12678 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 12679 | ` * takes care of generating the appropriate error message.` |
|       - | 12680 | ` */` |
|   18130 | 12681 | `static sxi32 PH7_CompilePHP(` |
|       - | 12682 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 12683 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 12684 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 12685 | `	)` |
|       5 | 12686 | `{` |
|   18135 | 12687 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 12688 | `	sxi32 rc;` |
|       - | 12689 | `	/* Reset the token set */` |
|   18135 | 12690 | `	SySetReset(&(*pTokenSet));` |
|       - | 12691 | `	/* Mark as the default token set */` |
|   18135 | 12692 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 12693 | `	/* Advance the stream cursor */` |
|   18135 | 12694 | `	pGen->pRawIn++;` |
|       - | 12695 | `	/* Tokenize the PHP chunk first */` |
|   18135 | 12696 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 12697 | `	/* Point to the head and tail of the token stream. */` |
|   18135 | 12698 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   18135 | 12699 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   18135 | 12700 | `	if( is_expr ){` |
|     ! 0 | 12701 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 12702 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 12703 | `			/* A simple expression,compile it */` |
|     ! 0 | 12704 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 12705 | `		}` |
|       - | 12706 | `		/* Emit the DONE instruction */` |
|     ! 0 | 12707 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 12708 | `		return SXRET_OK;` |
|       - | 12709 | `	}` |
|   18135 | 12710 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 12711 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 12712 | `		/*` |
|       - | 12713 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 12714 | `		 * According to the PHP reference manual:` |
|       - | 12715 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 12716 | `		 *  immediately follow` |
|       - | 12717 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 12718 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 12719 | `		 * Symisc extension:` |
|       - | 12720 | `		 *   This short syntax works with all PHP opening` |
|       - | 12721 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 12722 | `		 *   only short tag.` |
|       - | 12723 | `		 */` |
|       - | 12724 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 12725 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 12726 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 12727 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 12728 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 12729 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 12730 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 12731 | `		}` |
|       3 | 12732 | `		return SXRET_OK;` |
|       - | 12733 | `	}` |
|       - | 12734 | `	/* Compile the PHP chunk */` |
|   18133 | 12735 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 12736 | `	/* Fix exceptions jumps */` |
|   18133 | 12737 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 12738 | `	/* Fix gotos now, the jump destination is resolved */` |
|   18133 | 12739 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 12740 | `		rc = SXERR_ABORT;` |
|       1 | 12741 | `	}` |
|       - | 12742 | `	/* Reset container */` |
|   18133 | 12743 | `	SySetReset(&pGen->aGoto);` |
|   18133 | 12744 | `	SySetReset(&pGen->aLabel);` |
|   18133 | 12745 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 12746 | `	/* Compilation result */` |
|   18133 | 12747 | `	return rc;` |
|    9070 | 12748 | `}` |
|       - | 12749 | `/*` |
|       - | 12750 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 12751 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 12752 | ` * This is the only compile interface exported from this file.` |
|       - | 12753 | ` */` |
|   21080 | 12754 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 12755 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 12756 | `	SyString *pScript,  /* Script to compile */` |
|       - | 12757 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 12758 | `	)` |
|       5 | 12759 | `{` |
|       - | 12760 | `	SySet aPhpToken,aRawToken;` |
|       - | 12761 | `	ph7_gen_state *pCodeGen;` |
|       - | 12762 | `	ph7_value *pRawObj;` |
|       - | 12763 | `	sxu32 nObjIdx;` |
|       - | 12764 | `	sxi32 nRawObj;` |
|       - | 12765 | `	int is_expr;` |
|       - | 12766 | `	sxi8 bSavedStrict;` |
|       - | 12767 | `	sxi8 bSavedStrictLocked;` |
|       - | 12768 | `	sxi32 rc;` |
|   21085 | 12769 | `	if( pScript->nByte < 1 ){` |
|       - | 12770 | `		/* Nothing to compile */` |
|     ! 0 | 12771 | `		return PH7_OK;` |
|       - | 12772 | `	}` |
|       - | 12773 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 12774 | `	 * file's flags so include/require restore them on return. */` |
|   21085 | 12775 | `	pCodeGen = &pVm->sCodeGen;` |
|   21085 | 12776 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   21085 | 12777 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   21085 | 12778 | `	pCodeGen->bStrictTypes = 0;` |
|   21085 | 12779 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 12780 | `	/* Initialize the tokens containers */` |
|   21085 | 12781 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   21085 | 12782 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   21085 | 12783 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   21085 | 12784 | `	is_expr = 0;` |
|   21085 | 12785 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 12786 | `		SyToken sTmp;` |
|       - | 12787 | `		/* PHP only: -*/` |
|    7791 | 12788 | `		sTmp.nLine = 1;` |
|    7791 | 12789 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    7791 | 12790 | `		sTmp.pUserData = 0;` |
|    7791 | 12791 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    7791 | 12792 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    7791 | 12793 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 12794 | `			/* A simple PHP expression */` |
|     ! 0 | 12795 | `			is_expr = 1;` |
|     ! 0 | 12796 | `		}` |
|    3898 | 12797 | `	}else{` |
|       - | 12798 | `		/* Tokenize raw text */` |
|   13299 | 12799 | `		SySetAlloc(&aRawToken,32);` |
|   13299 | 12800 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 12801 | `	}` |
|       - | 12802 | `	/* Process high-level tokens */` |
|   21085 | 12803 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   21085 | 12804 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   21085 | 12805 | `	rc = PH7_OK;` |
|   21085 | 12806 | `	if( is_expr ){` |
|       - | 12807 | `		/* Compile the expression */` |
|     ! 0 | 12808 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 12809 | `		goto cleanup;` |
|       - | 12810 | `	}` |
|   21085 | 12811 | `	nObjIdx = 0;` |
|       - | 12812 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 12813 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 12814 | `	 * preventing namespace bleeding across include()d files. */` |
|   21085 | 12815 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 12816 | `	/* Start the compilation process */` |
|   17193 | 12817 | `	for(;;){` |
|   52509 | 12818 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   21073 | 12819 | `			break; /* No more tokens to process */` |
|       - | 12820 | `		}` |
|   31441 | 12821 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 12822 | `			/* Compile the PHP chunk */` |
|   18135 | 12823 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   18135 | 12824 | `			if( rc == SXERR_ABORT ){` |
|      15 | 12825 | `				break;` |
|       - | 12826 | `			}` |
|   18123 | 12827 | `			continue;` |
|       - | 12828 | `		}` |
|       - | 12829 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13311 | 12830 | `		nRawObj = 0;` |
|   26659 | 12831 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 12832 | `			/* Consume the raw chunk without any processing */` |
|   13353 | 12833 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13353 | 12834 | `			if( pRawObj == 0 ){` |
|     ! 0 | 12835 | `				rc = SXERR_MEM;` |
|     ! 0 | 12836 | `				break;` |
|       - | 12837 | `			}` |
|       - | 12838 | `			/* Mark as constant and emit the load constant instruction */` |
|   13353 | 12839 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13353 | 12840 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13353 | 12841 | `			++nRawObj;` |
|   13353 | 12842 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 12843 | `		}` |
|   13311 | 12844 | `		if( nRawObj > 0 ){` |
|       - | 12845 | `			/* Emit the consume instruction */` |
|   13311 | 12846 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6653 | 12847 | `		}` |
|   10545 | 12848 | `	}` |
|   10540 | 12849 | `cleanup:` |
|   21085 | 12850 | `	SySetRelease(&aRawToken);` |
|   21085 | 12851 | `	SySetRelease(&aPhpToken);` |
|       - | 12852 | `	/* Restore outer file's strict_types scope */` |
|   21085 | 12853 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   21085 | 12854 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   21085 | 12855 | `	return rc;` |
|   10545 | 12856 | `}` |
|       - | 12857 | `/*` |
|       - | 12858 | ` * Utility routines.Initialize the code generator.` |
|       - | 12859 | ` */` |
|    3844 | 12860 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 12861 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12862 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12863 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12864 | `	)` |
|       5 | 12865 | `{` |
|    3849 | 12866 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12867 | `	/* Zero the structure */` |
|    3849 | 12868 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 12869 | `	/* Initial state */` |
|    3849 | 12870 | `	pGen->pVm  = &(*pVm);` |
|    3849 | 12871 | `	pGen->xErr = xErr;` |
|    3849 | 12872 | `	pGen->pErrData = pErrData;` |
|    3849 | 12873 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3849 | 12874 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3849 | 12875 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3849 | 12876 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3849 | 12877 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 12878 | `	/* Error log buffer */` |
|    3849 | 12879 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 12880 | `	/* General purpose working buffer */` |
|    3849 | 12881 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 12882 | `	/* Namespace state */` |
|    3849 | 12883 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3849 | 12884 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3849 | 12885 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3849 | 12886 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12887 | `	/* Create the global scope */` |
|    3849 | 12888 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 12889 | `	/* Point to the global scope */` |
|    3849 | 12890 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3849 | 12891 | `	return SXRET_OK;` |
|       5 | 12892 | `}` |
|       - | 12893 | `/*` |
|       - | 12894 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 12895 | ` */` |
|   24552 | 12896 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 12897 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12898 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12899 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12900 | `	)` |
|       5 | 12901 | `{` |
|   24557 | 12902 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12903 | `	GenBlock *pBlock,*pParent;` |
|       - | 12904 | `	/* Reset state */` |
|   24557 | 12905 | `	SySetReset(&pGen->aLabel);` |
|   24557 | 12906 | `	SySetReset(&pGen->aGoto);` |
|   24557 | 12907 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   24557 | 12908 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   24557 | 12909 | `	SyBlobRelease(&pGen->sWorker);` |
|   24557 | 12910 | `	SyBlobRelease(&pGen->sNamespace);` |
|   24557 | 12911 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   24557 | 12912 | `	SyHashRelease(&pGen->hUseImports);` |
|   24557 | 12913 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   24557 | 12914 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   24557 | 12915 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   24557 | 12916 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   24557 | 12917 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12918 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 12919 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 12920 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 12921 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 12922 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 12923 | `	 * number of unique names, which is acceptable. */` |
|       - | 12924 | `	/* Point to the global scope */` |
|   24557 | 12925 | `	pBlock = pGen->pCurrent;` |
|   24557 | 12926 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 12927 | `		pParent = pBlock->pParent;` |
|     ! 0 | 12928 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 12929 | `		pBlock = pParent;` |
|     ! 0 | 12930 | `	}` |
|   24557 | 12931 | `	pGen->xErr = xErr;` |
|   24557 | 12932 | `	pGen->pErrData = pErrData;` |
|   24557 | 12933 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   24557 | 12934 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   24557 | 12935 | `	pGen->pIn = pGen->pEnd = 0;` |
|   24557 | 12936 | `	pGen->nErr = 0;` |
|   24557 | 12937 | `	return SXRET_OK;` |
|       5 | 12938 | `}` |
|       - | 12939 | `/*` |
|       - | 12940 | ` * Generate a compile-time error message.` |
|       - | 12941 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 12942 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 12943 | ` * abort compilation immediately.` |
|       - | 12944 | ` */` |
|     642 | 12945 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 12946 | `{` |
|     647 | 12947 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     647 | 12948 | `	const char *zErr = "Error";` |
|       - | 12949 | `	SyString *pFile;` |
|       - | 12950 | `	va_list ap;` |
|       - | 12951 | `	sxi32 rc;` |
|       - | 12952 | `	/* Reset the working buffer */` |
|     647 | 12953 | `	SyBlobReset(pWorker);` |
|       - | 12954 | `	/* Peek the processed file path if available */` |
|     647 | 12955 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     647 | 12956 | `	if( nErrType == E_ERROR ){` |
|       - | 12957 | `		/* Increment the error counter */` |
|     533 | 12958 | `		pGen->nErr++;` |
|     533 | 12959 | `		if( pGen->nErr > 15 ){` |
|       - | 12960 | `			/* Error count limit reached */` |
|       6 | 12961 | `			if( pGen->xErr ){` |
|       6 | 12962 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       6 | 12963 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       6 | 12964 | `				if( pFile ){` |
|       6 | 12965 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 12966 | `				}` |
|       6 | 12967 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       6 | 12968 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       6 | 12969 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 12970 | `				}` |
|       2 | 12971 | `			}` |
|       - | 12972 | `			/* Abort immediately */` |
|       6 | 12973 | `			return SXERR_ABORT;` |
|       - | 12974 | `		}` |
|     262 | 12975 | `	}` |
|     643 | 12976 | `	if( pGen->xErr == 0 ){` |
|       - | 12977 | `		/* No available error consumer,return immediately */` |
|       3 | 12978 | `		return SXRET_OK;` |
|       - | 12979 | `	}` |
|     640 | 12980 | `	switch(nErrType){` |
|     526 | 12981 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      32 | 12982 | `	case E_WARNING: zErr = "Warning";     break;` |
|      82 | 12983 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      12 | 12984 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 12985 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 12986 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 12987 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 12988 | `	default:` |
|     ! 0 | 12989 | `		break;` |
|       - | 12990 | `	}` |
|     640 | 12991 | `	rc = SXRET_OK;` |
|       - | 12992 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     640 | 12993 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     640 | 12994 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     640 | 12995 | `	va_start(ap,zFormat);` |
|     640 | 12996 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     640 | 12997 | `	va_end(ap);` |
|     640 | 12998 | `	if( pFile ){` |
|     640 | 12999 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     318 | 13000 | `	}` |
|       - | 13001 | `	/* Append a new line */` |
|     640 | 13002 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     640 | 13003 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 13004 | `		/* Consume the generated error message */` |
|     640 | 13005 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     318 | 13006 | `	}` |
|     640 | 13007 | `	return rc;` |
|     326 | 13008 | `}` |
|       - | 13009 |  |
