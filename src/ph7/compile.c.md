# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6055/7501 lines (80.72%)

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
|    4114 |   137 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   138 | `{` |
|    4119 |   139 | `	GenBlock *pBlock = pCurrent;` |
|   11754 |   140 | `	for(;;){` |
|   23513 |   141 | `		if( pBlock->iFlags & iBlockType ){` |
|    4011 |   142 | `			iCount--; /* Decrement nesting level */` |
|    4011 |   143 | `			if( iCount < 1 ){` |
|       - |   144 | `				/* Block meet with the desired criteria */` |
|    3985 |   145 | `				return pBlock;` |
|       - |   146 | `			}` |
|      13 |   147 | `		}` |
|       - |   148 | `		/* Point to the upper block */` |
|   19533 |   149 | `		pBlock = pBlock->pParent;` |
|   19533 |   150 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   151 | `			/* Forbidden */` |
|      72 |   152 | `			break;` |
|       - |   153 | `		}` |
|       5 |   154 | `	}` |
|       - |   155 | `	/* No such block */` |
|     139 |   156 | `	return 0;` |
|    2062 |   157 | `}` |
|       - |   158 | `/*` |
|       - |   159 | ` * Initialize a freshly allocated block instance.` |
|       - |   160 | ` */` |
|  909098 |   161 | `static void GenStateInitBlock(` |
|       - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   166 | `	void *pUserData      /* Upper layer private data */` |
|       - |   167 | `	)` |
|       5 |   168 | `{` |
|       - |   169 | `	/* Initialize block fields */` |
|  909103 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  909103 |   171 | `	pBlock->pUserData   = pUserData;` |
|  909103 |   172 | `	pBlock->pGen        = pGen;` |
|  909103 |   173 | `	pBlock->iFlags      = iType;` |
|  909103 |   174 | `	pBlock->pParent     = 0;` |
|  909103 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  909103 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  909103 |   177 | `}` |
|       - |   178 | `/*` |
|       - |   179 | ` * Allocate a new block instance.` |
|       - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   182 | ` * processing on failure.` |
|       - |   183 | ` */` |
|  905268 |   184 | `static sxi32 GenStateEnterBlock(` |
|       - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   190 | `	)` |
|       5 |   191 | `{` |
|       - |   192 | `	GenBlock *pBlock;` |
|       - |   193 | `	/* Allocate a new block instance */` |
|  905273 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  905273 |   195 | `	if( pBlock == 0 ){` |
|       - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   198 | `		 */` |
|     ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   200 | `		/* Abort processing immediately */` |
|     ! 0 |   201 | `		return SXERR_ABORT;` |
|       - |   202 | `	}` |
|       - |   203 | `	/* Zero the structure */` |
|  905273 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  905273 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   206 | `	/* Link to the parent block */` |
|  905273 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   208 | `	/* Mark as the current block */` |
|  905273 |   209 | `	pGen->pCurrent = pBlock;` |
|  905273 |   210 | `	if( ppBlock ){` |
|       - |   211 | `		/* Write a pointer to the new instance */` |
|  437871 |   212 | `		*ppBlock = pBlock;` |
|  218933 |   213 | `	}` |
|  905273 |   214 | `	return SXRET_OK;` |
|  452639 |   215 | `}` |
|       - |   216 | `/*` |
|       - |   217 | ` * Release block fields without freeing the whole instance.` |
|       - |   218 | ` */` |
|  905260 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   220 | `{` |
|  905265 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  905265 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  905265 |   223 | `}` |
|       - |   224 | `/*` |
|       - |   225 | ` * Release a block.` |
|       - |   226 | ` */` |
|  905260 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   228 | `{` |
|  905265 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  905265 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   231 | `	/* Free the instance */` |
|  905265 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  905265 |   233 | `}` |
|       - |   234 | `/*` |
|       - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   236 | ` */` |
|  905260 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   238 | `{` |
|  905265 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  905265 |   240 | `	if( pBlock == 0 ){` |
|       - |   241 | `		/* No more block to pop */` |
|     ! 0 |   242 | `		return SXERR_EMPTY;` |
|       - |   243 | `	}` |
|       - |   244 | `	/* Point to the upper block */` |
|  905265 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  905265 |   246 | `	if( ppBlock ){` |
|       - |   247 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   248 | `		*ppBlock = pBlock;` |
|     ! 0 |   249 | `	}else{` |
|       - |   250 | `		/* Safely release the block */` |
|  905265 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   252 | `	}` |
|  905265 |   253 | `	return SXRET_OK;` |
|  452635 |   254 | `}` |
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
|  258794 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   266 | `{` |
|       - |   267 | `	JumpFixup sJumpFix;` |
|       - |   268 | `	sxi32 rc;` |
|       - |   269 | `	/* Init the JumpFixup structure */` |
|  258799 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  258799 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   272 | `	/* Insert in the jump fixup table */` |
|  258799 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  258799 |   274 | `	return rc;` |
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
|  631060 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   288 | `{` |
|       - |   289 | `	JumpFixup *aFix;` |
|       - |   290 | `	VmInstr *pInstr;` |
|       - |   291 | `	sxu32 nFixed;` |
|       - |   292 | `	sxu32 n;` |
|       - |   293 | `	/* Point to the jump fixup table */` |
|  631065 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   295 | `	/* Fix the desired jumps */` |
| 1136107 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  505047 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   298 | `			/* Already fixed */` |
|  199425 |   299 | `			continue;` |
|       - |   300 | `		}` |
|  305627 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   302 | `			/* Not of our interest */` |
|   46835 |   303 | `			continue;` |
|       - |   304 | `		}` |
|       - |   305 | `		/* Point to the instruction to fix */` |
|  258797 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  258797 |   307 | `		if( pInstr ){` |
|  258797 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  258797 |   309 | `			nFixed++;` |
|       - |   310 | `			/* Mark as fixed */` |
|  258797 |   311 | `			aFix[n].nJumpType = -1;` |
|  129396 |   312 | `		}` |
|  129401 |   313 | `	}` |
|       - |   314 | `	/* Total number of fixed jumps */` |
|  631065 |   315 | `	return nFixed;` |
|       5 |   316 | `}` |
|       - |   317 | `/*` |
|       - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   319 | ` * The goto statement can be used to jump to another section` |
|       - |   320 | ` * in the program.` |
|       - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   322 | ` * statement for more information.` |
|       - |   323 | ` */` |
|  258262 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   325 | `{` |
|       - |   326 | `	JumpFixup *pJump,*aJumps;` |
|       - |   327 | `	Label *pLabel,*aLabel;` |
|       - |   328 | `	VmInstr *pInstr;` |
|       - |   329 | `	sxi32 rc;` |
|       - |   330 | `	sxu32 n;` |
|       - |   331 | `	/* Point to the goto table */` |
|  258267 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   333 | `	/* Fix */` |
|  258413 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|      11 |   348 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|      11 |   349 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |   350 | `				return SXERR_ABORT;` |
|       - |   351 | `			}` |
|       4 |   352 | `		}` |
|       - |   353 | `		/* Fix the jump now the destination is resolved */` |
|      96 |   354 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|      96 |   355 | `		if( pInstr ){` |
|      96 |   356 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      46 |   357 | `		}` |
|      50 |   358 | `	}` |
|  258265 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  258397 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   362 | `			/* Emit a warning */` |
|      40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   365 | `		}` |
|      71 |   366 | `	}` |
|  258265 |   367 | `	return SXRET_OK;` |
|  129136 |   368 | `}` |
|       - |   369 | `/*` |
|       - |   370 | ` * Check if a given token value is installed in the literal table.` |
|       - |   371 | ` */` |
|  822238 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   373 | `{` |
|       - |   374 | `	SyHashEntry *pEntry;` |
|  822243 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  822243 |   376 | `	if( pEntry == 0 ){` |
|  371009 |   377 | `		return SXERR_NOTFOUND;` |
|       - |   378 | `	}` |
|  451239 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  451239 |   380 | `	return SXRET_OK;` |
|  411124 |   381 | `}` |
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
|  371004 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   393 | `{` |
|  371009 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  371009 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  185502 |   396 | `	}` |
|  371009 |   397 | `	return SXRET_OK;` |
|       5 |   398 | `}` |
|       - |   399 | `/*` |
|       - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   401 | ` * in the constant table.` |
|       - |   402 | ` */` |
|  134144 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   404 | `{` |
|       - |   405 | `	ph7_value *pObj;` |
|  134149 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   407 | `	/* Reserve a new constant */` |
|  134149 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  134149 |   409 | `	if( pObj == 0 ){` |
|     ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   411 | `		return 0;` |
|       - |   412 | `	}` |
|  134149 |   413 | `	*pIdx = nIdx;` |
|       - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   416 | `	 */` |
|  134149 |   417 | `	return pObj;` |
|   67077 |   418 | `}` |
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
|  513750 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   434 | `{` |
|       - |   435 | `	VmCallArgMap *pMap;` |
|  513755 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|      39 |   437 | `	if( p3 == 0 ){` |
|      35 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      35 |   439 | `		if( pMap == 0 ) return 0;` |
|      35 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      35 |   441 | `		p3 = (void *)pMap;` |
|      16 |   442 | `	}` |
|      39 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      39 |   444 | `	return p3;` |
|  256880 |   445 | `}` |
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
|  134894 |   504 | `static int GenStateFindBadNumericSeparator(` |
|       - |   505 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   506 | `{` |
|  134899 |   507 | `	const char *z = pRaw->zString;` |
|  134899 |   508 | `	sxu32 n = pRaw->nByte;` |
|  134899 |   509 | `	int base = 10;` |
|       - |   510 | `	sxu32 i, start;` |
|  134899 |   511 | `	if( n < 2 ) return 0;` |
|   11147 |   512 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   513 | `		base = 16;` |
|   11112 |   514 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   515 | `		base = 2;` |
|     139 |   516 | `	}` |
|   40067 |   517 | `	for( i = 0; i < n; ++i ){` |
|   28939 |   518 | `		if( z[i] != '_' ) continue;` |
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
|   11133 |   535 | `	return 0;` |
|   67452 |   536 | `}` |
|       - |   537 | `/*` |
|       - |   538 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   539 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   540 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   541 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   542 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   543 | ` * so callers can bail from the current construct).` |
|       - |   544 | ` */` |
|  134894 |   545 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   546 | `{` |
|  134899 |   547 | `	const char *zBad = 0;` |
|  134899 |   548 | `	sxu32 nBad = 0;` |
|       - |   549 | `	SyString sBad;` |
|       - |   550 | `	sxi32 rc;` |
|  134899 |   551 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  134885 |   552 | `		return SXRET_OK;` |
|       - |   553 | `	}` |
|      18 |   554 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   555 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   556 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   557 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   558 | `		return SXERR_ABORT;` |
|       - |   559 | `	}` |
|      18 |   560 | `	return SXERR_SYNTAX;` |
|   67452 |   561 | `}` |
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
|  134880 |   578 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   579 | `	SyMemBackend *pAlloc,` |
|       - |   580 | `	const SyString *pToken,` |
|       - |   581 | `	char *zScratch, sxu32 nScratch,` |
|       - |   582 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   583 | `{` |
|       - |   584 | `	sxu32 i, j;` |
|  134885 |   585 | `	int hasUnderscore = 0;` |
|       - |   586 | `	char *zBuf;` |
|  134885 |   587 | `	*pzAlloc = 0;` |
|  285491 |   588 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  150863 |   589 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   75308 |   590 | `	}` |
|  134885 |   591 | `	if( !hasUnderscore ){` |
|  134633 |   592 | `		SyStringDupPtr(pOut, pToken);` |
|  134633 |   593 | `		return SXRET_OK;` |
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
|   67445 |   610 | `}` |
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
|  134866 |   627 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   628 | `{` |
|  134871 |   629 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  134871 |   630 | `	sxu32 nIdx = 0;` |
|       - |   631 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  134871 |   632 | `	char *zAlloc = 0;` |
|       - |   633 | `	SyString sNum;` |
|       - |   634 | `	sxi32 rc;` |
|   67433 |   635 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  134871 |   636 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  134871 |   637 | `	if( rc != SXRET_OK ){` |
|      14 |   638 | `		return rc;` |
|       - |   639 | `	}` |
|  202289 |   640 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   67428 |   641 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  134861 |   642 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   643 | `		return SXERR_ABORT;` |
|       - |   644 | `	}` |
|  134861 |   645 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   646 | `		ph7_value *pObj;` |
|       - |   647 | `		sxi64 iValue;` |
|  134149 |   648 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  134149 |   649 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  134149 |   650 | `		if( pObj == 0 ){` |
|     ! 0 |   651 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   652 | `			return SXERR_ABORT;` |
|       - |   653 | `		}` |
|  134149 |   654 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   67077 |   655 | `	}else{` |
|       - |   656 | `		/* Real number */` |
|       - |   657 | `		ph7_value *pObj;` |
|       - |   658 | `		/* Reserve a new constant */` |
|     716 |   659 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     716 |   660 | `		if( pObj == 0 ){` |
|     ! 0 |   661 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   662 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   663 | `			return SXERR_ABORT;` |
|       - |   664 | `		}` |
|     716 |   665 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     716 |   666 | `		PH7_MemObjToReal(pObj);` |
|       - |   667 | `	}` |
|  134861 |   668 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   669 | `	/* Emit the load constant instruction */` |
|  134861 |   670 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   671 | `	/* Node successfully compiled */` |
|  134861 |   672 | `	return SXRET_OK;` |
|   67438 |   673 | `}` |
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
|  106684 |   685 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   686 | `{` |
|  106689 |   687 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   688 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   689 | `	ph7_value *pObj;` |
|       - |   690 | `	sxu32 nIdx;` |
|  106689 |   691 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   692 | `	/* Delimit the string */` |
|  106689 |   693 | `	zIn  = pStr->zString;` |
|  106689 |   694 | `	zEnd = &zIn[pStr->nByte];` |
|  106689 |   695 | `	if( zIn >= zEnd ){` |
|       - |   696 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   697 | `		 * rather than reserving a new object each time. */` |
|    7839 |   698 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    7839 |   699 | `		return SXRET_OK;` |
|       - |   700 | `	}` |
|   98855 |   701 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   702 | `		/* Already processed,emit the load constant instruction` |
|       - |   703 | `		 * and return.` |
|       - |   704 | `		 */` |
|   37551 |   705 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   37551 |   706 | `		return SXRET_OK;` |
|       - |   707 | `	}` |
|       - |   708 | `	/* Reserve a new constant */` |
|   61309 |   709 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   61309 |   710 | `	if( pObj == 0 ){` |
|     ! 0 |   711 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   712 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   713 | `		return SXERR_ABORT;` |
|       - |   714 | `	}` |
|   61309 |   715 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   716 | `	/* Compile the node */` |
|   61363 |   717 | `	for(;;){` |
|  122731 |   718 | `		if( zIn >= zEnd ){` |
|       - |   719 | `			/* End of input */` |
|   61309 |   720 | `			break;` |
|       - |   721 | `		}` |
|   61427 |   722 | `		zCur = zIn;` |
| 1049803 |   723 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  988381 |   724 | `			zIn++;` |
|       5 |   725 | `		}` |
|   61427 |   726 | `		if( zIn > zCur ){` |
|       - |   727 | `			/* Append raw contents*/` |
|   61403 |   728 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   30699 |   729 | `		}` |
|   61427 |   730 | `		zIn++;` |
|   61427 |   731 | `		if( zIn < zEnd ){` |
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
|   61427 |   746 | `		zIn++;` |
|       5 |   747 | `	}` |
|       - |   748 | `	/* Emit the load constant instruction */` |
|   61309 |   749 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   61309 |   750 | `	if( pStr->nByte < 1024 ){` |
|       - |   751 | `		/* Install in the literal table */` |
|   61309 |   752 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   30652 |   753 | `	}` |
|       - |   754 | `	/* Node successfully compiled */` |
|   61309 |   755 | `	return SXRET_OK;` |
|   53347 |   756 | `}` |
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
|    2336 |   922 | `static sxi32 GenStateProcessStringExpression(` |
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
|    2341 |   933 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   934 | `	/* Preallocate some slots */` |
|    2341 |   935 | `	SySetAlloc(&sToken,0x08);` |
|       - |   936 | `	/* Tokenize the text */` |
|    2341 |   937 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   938 | `	/* Swap delimiter */` |
|    2341 |   939 | `	pTmpIn  = pGen->pIn;` |
|    2341 |   940 | `	pTmpEnd = pGen->pEnd;` |
|    2341 |   941 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2341 |   942 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   943 | `	/* Compile the expression */` |
|    2341 |   944 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   945 | `	/* Restore token stream */` |
|    2341 |   946 | `	pGen->pIn  = pTmpIn;` |
|    2341 |   947 | `	pGen->pEnd = pTmpEnd;` |
|       - |   948 | `	/* Release the token set */` |
|    2341 |   949 | `	SySetRelease(&sToken);` |
|       - |   950 | `	/* Compilation result */` |
|    2341 |   951 | `	return rc;` |
|       5 |   952 | `}` |
|       - |   953 | `/*` |
|       - |   954 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   955 | ` */` |
|   26372 |   956 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   957 | `{` |
|       - |   958 | `	ph7_value *pConstObj;` |
|   26377 |   959 | `	sxu32 nIdx = 0;` |
|       - |   960 | `	/* Reserve a new constant */` |
|   26377 |   961 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   26377 |   962 | `	if( pConstObj == 0 ){` |
|     ! 0 |   963 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   964 | `		return 0;` |
|       - |   965 | `	}` |
|   26377 |   966 | `	(*pCount)++;` |
|   26377 |   967 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   968 | `	/* Emit the load constant instruction */` |
|   26377 |   969 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   26377 |   970 | `	return pConstObj;` |
|   13191 |   971 | `}` |
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
|   24822 |  1034 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|       5 |  1035 | `{` |
|   24827 |  1036 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1037 | `	const char *zIn,*zCur,*zEnd;` |
|   24827 |  1038 | `	ph7_value *pObj = 0;` |
|       - |  1039 | `	sxi32 iCons;` |
|       - |  1040 | `	sxi32 rc;` |
|       - |  1041 | `	/* Delimit the string */` |
|   24827 |  1042 | `	zIn  = pStr->zString;` |
|   24827 |  1043 | `	zEnd = &zIn[pStr->nByte];` |
|   24827 |  1044 | `	if( zIn >= zEnd ){` |
|       - |  1045 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1046 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1047 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1048 | `		 */` |
|     299 |  1049 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     299 |  1050 | `		return SXRET_OK;` |
|       - |  1051 | `	}` |
|   24533 |  1052 | `	zCur = 0;` |
|       - |  1053 | `	/* Compile the node */` |
|   24533 |  1054 | `	iCons = 0;` |
|   13432 |  1055 | `	for(;;){` |
|   40531 |  1056 | `		zCur = zIn;` |
|  183809 |  1057 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  145619 |  1058 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      68 |  1059 | `				break;` |
|  145493 |  1060 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2214 |  1061 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1108 |  1062 | `					break;` |
|       - |  1063 | `			}` |
|  143283 |  1064 | `			zIn++;` |
|       5 |  1065 | `		}` |
|   40531 |  1066 | `		if( zIn > zCur ){` |
|   18277 |  1067 | `			if( pObj == 0 ){` |
|   17759 |  1068 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   17759 |  1069 | `				if( pObj == 0 ){` |
|     ! 0 |  1070 | `					return SXERR_ABORT;` |
|       - |  1071 | `				}` |
|    8877 |  1072 | `			}` |
|   18277 |  1073 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    9136 |  1074 | `		}` |
|   40531 |  1075 | `		if( zIn >= zEnd ){` |
|   24531 |  1076 | `			break;` |
|       - |  1077 | `		}` |
|   16005 |  1078 | `		if( zIn[0] == '\\' ){` |
|   13669 |  1079 | `			const char *zPtr = 0;` |
|       - |  1080 | `			sxu32 n;` |
|   13669 |  1081 | `			zIn++;` |
|   13669 |  1082 | `			if( pObj == 0 ){` |
|    8623 |  1083 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    8623 |  1084 | `				if( pObj == 0 ){` |
|     ! 0 |  1085 | `					return SXERR_ABORT;` |
|       - |  1086 | `				}` |
|    4309 |  1087 | `			}` |
|   13669 |  1088 | `			if( zIn >= zEnd ){` |
|       - |  1089 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|       3 |  1090 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       3 |  1091 | `				break;` |
|       - |  1092 | `			}` |
|   13667 |  1093 | `			n = sizeof(char); /* size of conversion */` |
|   13667 |  1094 | `			switch( zIn[0] ){` |
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
|    6285 |  1111 | `			case 'n':` |
|       - |  1112 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   12575 |  1113 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   12575 |  1114 | `				break;` |
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
|      18 |  1178 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
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
|   13667 |  1237 | `			zIn += n;` |
|   13667 |  1238 | `			continue;` |
|       - |  1239 | `		}` |
|    2341 |  1240 | `		if( zIn[0] == '{' ){` |
|       - |  1241 | `			/* Curly syntax */` |
|       - |  1242 | `			const char *zExpr;` |
|     133 |  1243 | `			sxi32 iNest = 1;` |
|     133 |  1244 | `			zIn++;` |
|     133 |  1245 | `			zExpr = zIn;` |
|       - |  1246 | `			/* Synchronize with the next closing curly braces */` |
|    1365 |  1247 | `			while( zIn < zEnd ){` |
|    1365 |  1248 | `				if( zIn[0] == '{' ){` |
|       - |  1249 | `					/* Increment nesting level */` |
|       9 |  1250 | `					iNest++;` |
|    1361 |  1251 | `				}else if(zIn[0] == '}' ){` |
|       - |  1252 | `					/* Decrement nesting level */` |
|     141 |  1253 | `					iNest--;` |
|     141 |  1254 | `					if( iNest <= 0 ){` |
|     133 |  1255 | `						break;` |
|       - |  1256 | `					}` |
|       4 |  1257 | `				}` |
|    1235 |  1258 | `				zIn++;` |
|       3 |  1259 | `			}` |
|       - |  1260 | `			/* Process the expression */` |
|     133 |  1261 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     133 |  1262 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1263 | `				return SXERR_ABORT;` |
|       - |  1264 | `			}` |
|     133 |  1265 | `			if( rc != SXERR_EMPTY ){` |
|     133 |  1266 | `				++iCons;` |
|      65 |  1267 | `			}` |
|     133 |  1268 | `			if( zIn < zEnd ){` |
|       - |  1269 | `				/* Jump the trailing curly */` |
|     133 |  1270 | `				zIn++;` |
|      65 |  1271 | `			}` |
|      68 |  1272 | `		}else{` |
|       - |  1273 | `			/* Simple syntax */` |
|    2211 |  1274 | `			const char *zExpr = zIn;` |
|       - |  1275 | `			/* Assemble variable name */` |
|    1113 |  1276 | `			for(;;){` |
|       - |  1277 | `				/* Jump leading dollars */` |
|    4437 |  1278 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2211 |  1279 | `					zIn++;` |
|       5 |  1280 | `				}` |
|    1113 |  1281 | `				for(;;){` |
|   12048 |  1282 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8709 |  1283 | `						zIn++;` |
|       5 |  1284 | `					}` |
|    2231 |  1285 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1286 | `						/* UTF-8 stream */` |
|     ! 0 |  1287 | `						zIn++;` |
|     ! 0 |  1288 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1289 | `							zIn++;` |
|     ! 0 |  1290 | `						}` |
|     ! 0 |  1291 | `						continue;` |
|       - |  1292 | `					}` |
|    2231 |  1293 | `					break;` |
|     ! 0 |  1294 | `				}` |
|    2231 |  1295 | `				if( zIn >= zEnd ){` |
|     223 |  1296 | `					break;` |
|       - |  1297 | `				}` |
|    2013 |  1298 | `				if( zIn[0] == '[' ){` |
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
|    2003 |  1316 | `				}else if(zIn[0] == '{' ){` |
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
|    1999 |  1334 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1335 | `					/* Member access operator '->' */` |
|      23 |  1336 | `					zIn += 2;` |
|    1989 |  1337 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1338 | `					/* Static member access operator '::' */` |
|     ! 0 |  1339 | `					zIn += 2;` |
|     ! 0 |  1340 | `				}else{` |
|     992 |  1341 | `					break;` |
|       - |  1342 | `				}` |
|       3 |  1343 | `			}` |
|       - |  1344 | `			/* Process the expression */` |
|    2211 |  1345 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2211 |  1346 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1347 | `				return SXERR_ABORT;` |
|       - |  1348 | `			}` |
|    2211 |  1349 | `			if( rc != SXERR_EMPTY ){` |
|    2209 |  1350 | `				++iCons;` |
|    1102 |  1351 | `			}` |
|       - |  1352 | `		}` |
|       - |  1353 | `		/* Invalidate the previously used constant */` |
|    2341 |  1354 | `		pObj = 0;` |
|       5 |  1355 | `	}/*for(;;)*/` |
|   24533 |  1356 | `	if( iCons > 1 ){` |
|       - |  1357 | `		/* Concatenate all compiled constants */` |
|    1733 |  1358 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     864 |  1359 | `	}` |
|       - |  1360 | `	/* Node successfully compiled */` |
|   24533 |  1361 | `	return SXRET_OK;` |
|   12416 |  1362 | `}` |
|       - |  1363 | `/*` |
|       - |  1364 | ` * Compile a double quoted string.` |
|       - |  1365 | ` *  See the block-comment above for more information.` |
|       - |  1366 | ` */` |
|   24760 |  1367 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1368 | `{` |
|       - |  1369 | `	sxi32 rc;` |
|   24765 |  1370 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|   12380 |  1371 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1372 | `	/* Compilation result */` |
|   24765 |  1373 | `	return rc;` |
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
|   23424 |  1417 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   23429 |  1428 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1429 | `	/* Compile the expression*/` |
|   23429 |  1430 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1431 | `	/* Restore token stream */` |
|   23429 |  1432 | `	RE_SWAP_DELIMITER(pGen);` |
|   23429 |  1433 | `	return rc;` |
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
|       4 |  1444 | `{` |
|      40 |  1445 | `	sxi32 rc = SXRET_OK;` |
|      40 |  1446 | `	if( pRoot->pOp ){` |
|      14 |  1447 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 |  1448 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      16 |  1449 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  1450 | `			/* Unexpected expression */` |
|      13 |  1451 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1452 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      13 |  1453 | `			if( rc != SXERR_ABORT ){` |
|      13 |  1454 | `				rc = SXERR_INVALID;` |
|       5 |  1455 | `			}` |
|       9 |  1456 | `		}` |
|      31 |  1457 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1458 | `		/* Unexpected expression */` |
|       3 |  1459 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1460 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 |  1461 | `		if( rc != SXERR_ABORT ){` |
|       3 |  1462 | `			rc = SXERR_INVALID;` |
|       1 |  1463 | `		}` |
|       1 |  1464 | `	}` |
|      40 |  1465 | `	return rc;` |
|       4 |  1466 | `}` |
|       - |  1467 | `/*` |
|       - |  1468 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|       - |  1469 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|       - |  1470 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|       - |  1471 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|       - |  1472 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|       - |  1473 | ` */` |
|   25996 |  1474 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 |  1475 | `{` |
|   26001 |  1476 | `	SyToken *pCur = pStart;` |
|   26001 |  1477 | `	sxi32 iNest = 0;` |
|   73899 |  1478 | `	while( pCur < pEnd ){` |
|   53755 |  1479 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    5853 |  1480 | `			return pCur;` |
|       - |  1481 | `		}` |
|       - |  1482 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1483 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - |  1484 | `		 * not an entry separator. Skip past the signature.` |
|       - |  1485 | `		 */` |
|   47907 |  1486 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|   47901 |  1547 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     413 |  1548 | `			iNest++;` |
|   47696 |  1549 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1550 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - |  1551 | `			 * parser will shortly detect any syntax error. */` |
|     413 |  1552 | `			iNest--;` |
|     205 |  1553 | `		}` |
|   47901 |  1554 | `		pCur++;` |
|       5 |  1555 | `	}` |
|   20149 |  1556 | `	return pEnd;` |
|   13003 |  1557 | `}` |
|       - |  1558 | `/*` |
|       - |  1559 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1560 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1561 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1562 | ` */` |
|   33668 |  1563 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1564 | `{` |
|       - |  1565 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1566 | `	SyToken *pKey,*pCur;` |
|   33673 |  1567 | `	sxi32 iEmitRef = 0;` |
|   33673 |  1568 | `	sxi32 iSpread = 0;` |
|   33673 |  1569 | `	sxi32 nPair = 0;` |
|       - |  1570 | `	sxi32 rc;` |
|   33673 |  1571 | `	xValidator = 0;` |
|   27568 |  1572 | `	for(;;){` |
|       - |  1573 | `		/* Jump leading commas */` |
|   62561 |  1574 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    7425 |  1575 | `			pGen->pIn++;` |
|       5 |  1576 | `		}` |
|   55141 |  1577 | `		pCur = pGen->pIn;` |
|   55141 |  1578 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1579 | `			/* No more entry to process */` |
|   33657 |  1580 | `			break;` |
|       - |  1581 | `		}` |
|   21489 |  1582 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1583 | `			continue;` |
|       - |  1584 | `		}` |
|       - |  1585 | `		/* Compile the key if available */` |
|   21489 |  1586 | `		pKey = pCur;` |
|   21489 |  1587 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   21489 |  1588 | `		rc = SXERR_EMPTY;` |
|   21489 |  1589 | `		if( pCur < pGen->pIn ){` |
|    1739 |  1590 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1591 | `				/* Missing value */` |
|      13 |  1592 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      13 |  1593 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1594 | `					return SXERR_ABORT;` |
|       - |  1595 | `				}` |
|      13 |  1596 | `				return SXRET_OK;` |
|       - |  1597 | `			}` |
|       - |  1598 | `			/* Compile the expression holding the key */` |
|    1729 |  1599 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1600 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1729 |  1601 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1602 | `				return SXERR_ABORT;` |
|       - |  1603 | `			}` |
|    1729 |  1604 | `			pCur++; /* Jump the '=>' operator */` |
|   20617 |  1605 | `		}else if( pKey == pCur ){` |
|       - |  1606 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1607 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1608 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1609 | `		}else{` |
|       - |  1610 | `			/* Reset back the cursor and point to the entry value */` |
|   19755 |  1611 | `			pCur = pKey;` |
|       - |  1612 | `		}` |
|   21479 |  1613 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1614 | `			/* No available key,load NULL */` |
|   19757 |  1615 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    9876 |  1616 | `		}` |
|   21479 |  1617 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   21477 |  1636 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   21477 |  1637 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   21473 |  1650 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   21473 |  1651 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1652 | `			return SXERR_ABORT;` |
|       - |  1653 | `		}` |
|   21473 |  1654 | `		if( iSpread ){` |
|       - |  1655 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      65 |  1656 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   21442 |  1657 | `		}else if( iEmitRef ){` |
|       - |  1658 | `			/* Emit the load reference instruction */` |
|      40 |  1659 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1660 | `		}` |
|   21473 |  1661 | `		xValidator = 0;` |
|   21473 |  1662 | `		iEmitRef = 0;` |
|   21473 |  1663 | `		iSpread = 0;` |
|   21473 |  1664 | `		nPair++;` |
|       5 |  1665 | `	}` |
|       - |  1666 | `	/* Emit the load map instruction */` |
|   33657 |  1667 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1668 | `	/* Node successfully compiled */` |
|   33657 |  1669 | `	return SXRET_OK;` |
|   16839 |  1670 | `}` |
|       - |  1671 | `/*` |
|       - |  1672 | ` * Compile the 'array' language construct.` |
|       - |  1673 | ` *	 According to the PHP language reference manual` |
|       - |  1674 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1675 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1676 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1677 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1678 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1679 | ` */` |
|   32466 |  1680 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1681 | `{` |
|       - |  1682 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   32471 |  1683 | `	pGen->pIn += 2;` |
|   32471 |  1684 | `	pGen->pEnd--;` |
|   16233 |  1685 | `	SXUNUSED(iCompileFlag);` |
|   32471 |  1686 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1687 | `}` |
|       - |  1688 | `/*` |
|       - |  1689 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1690 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1691 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1692 | ` */` |
|    1202 |  1693 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1694 | `{` |
|       - |  1695 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    1207 |  1696 | `	pGen->pIn++;` |
|    1207 |  1697 | `	pGen->pEnd--;` |
|     601 |  1698 | `	SXUNUSED(iCompileFlag);` |
|    1207 |  1699 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1700 | `}` |
|       - |  1701 | `/*` |
|       - |  1702 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1703 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1704 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1705 | ` * error message.` |
|       - |  1706 | ` * See the routine responible of compiling the list language construct` |
|       - |  1707 | ` * for more inforation.` |
|       - |  1708 | ` */` |
|     190 |  1709 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  1710 | `{` |
|     195 |  1711 | `	sxi32 rc = SXRET_OK;` |
|     195 |  1712 | `	if( pRoot->pOp ){` |
|       4 |  1713 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|       2 |  1714 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1715 | `				/* Unexpected expression */` |
|     ! 0 |  1716 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1717 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1718 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1719 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1720 | `				}` |
|       1 |  1721 | `		}` |
|     193 |  1722 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1723 | `		/* Unexpected expression */` |
|       6 |  1724 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1725 | `			"list(): Expecting a variable not an expression");` |
|       6 |  1726 | `		if( rc != SXERR_ABORT ){` |
|       6 |  1727 | `			rc = SXERR_INVALID;` |
|       2 |  1728 | `		}` |
|       2 |  1729 | `	}` |
|     195 |  1730 | `	return rc;` |
|       5 |  1731 | `}` |
|       - |  1732 | `/*` |
|       - |  1733 | ` * Compile the 'list' language construct.` |
|       - |  1734 | ` *  According to the PHP language reference` |
|       - |  1735 | ` *  list(): Assign variables as if they were an array.` |
|       - |  1736 | ` *  list() is used to assign a list of variables in one operation.` |
|       - |  1737 | ` *  Description` |
|       - |  1738 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - |  1739 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - |  1740 | ` *   list() is used to assign a list of variables in one operation.` |
|       - |  1741 | ` *  Parameters` |
|       - |  1742 | ` *   $varname: A variable.` |
|       - |  1743 | ` *  Return Values` |
|       - |  1744 | ` *   The assigned array.` |
|       - |  1745 | ` */` |
|       - |  1746 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - |  1747 | `struct NestedListEntry {` |
|       - |  1748 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - |  1749 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - |  1750 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - |  1751 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - |  1752 | `};` |
|       - |  1753 | `/*` |
|       - |  1754 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|       - |  1755 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|       - |  1756 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|       - |  1757 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|       - |  1758 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|       - |  1759 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|       - |  1760 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|       - |  1761 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|       - |  1762 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|       - |  1763 | ` */` |
|      28 |  1764 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|       2 |  1765 | `{` |
|       - |  1766 | `	SyToken *pNext;` |
|       - |  1767 | `	sxi32 rc;` |
|      66 |  1768 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       - |  1769 | `		SyToken *pArrow,*pTarget;` |
|       - |  1770 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|      38 |  1771 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|      38 |  1772 | `		pTarget = &pArrow[1];` |
|      38 |  1773 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|       - |  1774 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|       - |  1775 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|     ! 0 |  1776 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1777 | `				"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1778 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1779 | `		}` |
|       - |  1780 | `		/* DUP the source array (it is on the stack top) */` |
|      38 |  1781 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1782 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|      38 |  1783 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|      38 |  1784 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1785 | `			return SXERR_ABORT;` |
|       - |  1786 | `		}` |
|       - |  1787 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|       - |  1788 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|       - |  1789 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|       - |  1790 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|       - |  1791 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|       - |  1792 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|      38 |  1793 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|      38 |  1794 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|      34 |  1795 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|      18 |  1796 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|       - |  1797 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|       - |  1798 | `			 * Treat source[key] as the inner body's source, then drop the` |
|       - |  1799 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|       5 |  1800 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|       5 |  1801 | `			SyToken *pSavedIn = pGen->pIn;` |
|       5 |  1802 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       5 |  1803 | `			pGen->pIn = pTarget;` |
|       5 |  1804 | `			pGen->pEnd = pNext;` |
|       5 |  1805 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|       2 |  1806 | `			             : PH7_CompileList(&(*pGen),0);` |
|       5 |  1807 | `			pGen->pIn = pSavedIn;` |
|       5 |  1808 | `			pGen->pEnd = pSavedEnd;` |
|       5 |  1809 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1810 | `				return SXERR_ABORT;` |
|       - |  1811 | `			}` |
|       5 |  1812 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       3 |  1813 | `		}else{` |
|       - |  1814 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|       - |  1815 | `			 * is already on the stack as the value; compiling the target appends` |
|       - |  1816 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|       - |  1817 | `			 * assignment does. */` |
|       - |  1818 | `			VmInstr *pInstr;` |
|      34 |  1819 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|      34 |  1820 | `			sxi32 iP1 = 0, iP2 = 0;` |
|      34 |  1821 | `			void *p3 = 0;` |
|      34 |  1822 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|       - |  1823 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      34 |  1824 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  1825 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1826 | `			}` |
|      34 |  1827 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|      34 |  1828 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|       3 |  1829 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|      33 |  1830 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       3 |  1831 | `					iVmOp = PH7_OP_STORE_IDX;` |
|       3 |  1832 | `					iP1 = pInstr->iP1;` |
|       3 |  1833 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       2 |  1834 | `				}else{` |
|      30 |  1835 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|      30 |  1836 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - |  1837 | `				}` |
|      16 |  1838 | `			}` |
|      34 |  1839 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - |  1840 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|       - |  1841 | `			 * source array is back on top for the next entry. */` |
|      34 |  1842 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       - |  1843 | `		}` |
|      38 |  1844 | `		pGen->pIn = &pNext[1];` |
|       2 |  1845 | `	}` |
|      30 |  1846 | `	return SXRET_OK;` |
|      16 |  1847 | `}` |
|       - |  1848 | `/*` |
|       - |  1849 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1850 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1851 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1852 | ` */` |
|     116 |  1853 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       5 |  1854 | `{` |
|       - |  1855 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1856 | `	SyToken *pNext;` |
|       - |  1857 | `	SyToken *pClassifyIn;` |
|     121 |  1858 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|       - |  1859 | `	sxi32 nExpr;` |
|       - |  1860 | `	sxi32 rc;` |
|       - |  1861 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|       - |  1862 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|       - |  1863 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|       - |  1864 | `	 * list. */` |
|     121 |  1865 | `	pClassifyIn = pGen->pIn;` |
|     341 |  1866 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     225 |  1867 | `		if( pGen->pIn >= pNext ){` |
|      13 |  1868 | `			nEmpty++;` |
|     219 |  1869 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|      38 |  1870 | `			nKeyed++;` |
|      20 |  1871 | `		}else{` |
|     177 |  1872 | `			nPositional++;` |
|       - |  1873 | `		}` |
|     225 |  1874 | `		pGen->pIn = &pNext[1];` |
|       5 |  1875 | `	}` |
|     121 |  1876 | `	pGen->pIn = pClassifyIn;` |
|     121 |  1877 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|     ! 0 |  1878 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1879 | `			"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1880 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1881 | `	}` |
|     121 |  1882 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|     ! 0 |  1883 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1884 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|     ! 0 |  1885 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1886 | `	}` |
|     121 |  1887 | `	if( nKeyed > 0 ){` |
|      30 |  1888 | `		return GenStateCompileKeyedListBody(pGen);` |
|       - |  1889 | `	}` |
|      93 |  1890 | `	nExpr = 0;` |
|      93 |  1891 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     277 |  1892 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     189 |  1893 | `		if( pGen->pIn < pNext ){` |
|       - |  1894 | `			/* Check for nested list() */` |
|     177 |  1895 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  1896 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  1897 | `				/* Record this nested list for post-processing */` |
|       3 |  1898 | `				SyToken *pListEnd = 0;` |
|       3 |  1899 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  1900 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  1901 | `				}` |
|       3 |  1902 | `				if( pListEnd ){` |
|       - |  1903 | `					struct NestedListEntry sEntry;` |
|       3 |  1904 | `					sEntry.nIndex = nExpr;` |
|       3 |  1905 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  1906 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  1907 | `					sEntry.isShort = 0;` |
|       3 |  1908 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  1909 | `				}` |
|       - |  1910 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  1911 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     176 |  1912 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  1913 | `				/* Nested short destructuring [...] */` |
|      13 |  1914 | `				SyToken *pBracketEnd = 0;` |
|      13 |  1915 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  1916 | `				if( pBracketEnd ){` |
|       - |  1917 | `					struct NestedListEntry sEntry;` |
|      13 |  1918 | `					sEntry.nIndex = nExpr;` |
|      13 |  1919 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  1920 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  1921 | `					sEntry.isShort = 1;` |
|      13 |  1922 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  1923 | `				}` |
|       - |  1924 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  1925 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  1926 | `			}else{` |
|       - |  1927 | `				/* Compile the expression holding the variable */` |
|     163 |  1928 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     163 |  1929 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  1930 | `					SySetRelease(&sNested);` |
|     ! 0 |  1931 | `					return SXRET_OK;` |
|       - |  1932 | `				}` |
|       - |  1933 | `			}` |
|      91 |  1934 | `		}else{` |
|       - |  1935 | `			/* Empty entry,load NULL */` |
|      13 |  1936 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  1937 | `		}` |
|     189 |  1938 | `		nExpr++;` |
|       - |  1939 | `		/* Advance the stream cursor */` |
|     189 |  1940 | `		pGen->pIn = &pNext[1];` |
|       5 |  1941 | `	}` |
|       - |  1942 | `	/* Emit the LOAD_LIST instruction */` |
|      93 |  1943 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  1944 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  1945 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  1946 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  1947 | `	 */` |
|      93 |  1948 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  1949 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  1950 | `		sxu32 i;` |
|      27 |  1951 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  1952 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  1953 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  1954 | `			ph7_value *pIdx;` |
|       - |  1955 | `			sxu32 nConstIdx;` |
|       - |  1956 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  1957 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1958 | `			/* Push the integer index for this nested entry */` |
|      15 |  1959 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  1960 | `			if( pIdx == 0 ){` |
|     ! 0 |  1961 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1962 | `				SySetRelease(&sNested);` |
|     ! 0 |  1963 | `				return SXERR_ABORT;` |
|       - |  1964 | `			}` |
|      15 |  1965 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  1966 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  1967 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  1968 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  1969 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  1970 | `			 */` |
|      15 |  1971 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  1972 | `			/* Recursively compile the inner list */` |
|      15 |  1973 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  1974 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  1975 | `			if( apNested[i].isShort ){` |
|      13 |  1976 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  1977 | `			}else{` |
|       3 |  1978 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  1979 | `			}` |
|      15 |  1980 | `			pGen->pIn = pSavedIn;` |
|      15 |  1981 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  1982 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1983 | `				SySetRelease(&sNested);` |
|     ! 0 |  1984 | `				return SXERR_ABORT;` |
|       - |  1985 | `			}` |
|       - |  1986 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  1987 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  1988 | `		}` |
|       6 |  1989 | `	}` |
|      93 |  1990 | `	SySetRelease(&sNested);` |
|       - |  1991 | `	/* Node successfully compiled */` |
|      93 |  1992 | `	return SXRET_OK;` |
|      63 |  1993 | `}` |
|      38 |  1994 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1995 | `{` |
|       - |  1996 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      43 |  1997 | `	pGen->pIn += 2;` |
|      43 |  1998 | `	pGen->pEnd--;` |
|      19 |  1999 | `	SXUNUSED(iCompileFlag);` |
|      43 |  2000 | `	return GenStateCompileListBody(pGen);` |
|       5 |  2001 | `}` |
|      78 |  2002 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2003 | `{` |
|       - |  2004 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      82 |  2005 | `	pGen->pIn++;` |
|      82 |  2006 | `	pGen->pEnd--;` |
|      39 |  2007 | `	SXUNUSED(iCompileFlag);` |
|      82 |  2008 | `	return GenStateCompileListBody(pGen);` |
|       4 |  2009 | `}` |
|       - |  2010 | `/* Forward declarations */` |
|       - |  2011 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  2012 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  2013 | `static int GenStateIsReadonly(SyToken *pTok);` |
|       - |  2014 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|       - |  2015 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|       - |  2016 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  2017 | `/*` |
|       - |  2018 | ` * Compile an annoynmous function or a closure.` |
|       - |  2019 | ` * According to the PHP language reference` |
|       - |  2020 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  2021 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  2022 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  2023 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  2024 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  2025 | ` *  Example Anonymous function variable assignment example` |
|       - |  2026 | ` * <?php` |
|       - |  2027 | ` * $greet = function($name)` |
|       - |  2028 | ` * {` |
|       - |  2029 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  2030 | ` * };` |
|       - |  2031 | ` * $greet('World');` |
|       - |  2032 | ` * $greet('PHP');` |
|       - |  2033 | ` * ?>` |
|       - |  2034 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  2035 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  2036 | ` */` |
|     320 |  2037 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2038 | `{` |
|       - |  2039 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  2040 | `	char zName[512];         /* Unique lambda name */` |
|       - |  2041 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  2042 | `							  * one thread is allowed to compile the script.` |
|       - |  2043 | `						      */` |
|       - |  2044 | `	SyString sName;` |
|       - |  2045 | `	sxu32 nLen;` |
|       - |  2046 | `	sxi32 rc;` |
|     160 |  2047 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2048 |  |
|     325 |  2049 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     325 |  2050 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  2051 | `		pGen->pIn++;` |
|     ! 0 |  2052 | `	}` |
|       - |  2053 | `	/* Generate a unique name */` |
|     325 |  2054 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  2055 | `	/* Make sure the generated name is unique */` |
|     325 |  2056 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  2057 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  2058 | `	}` |
|     325 |  2059 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  2060 | `	/* Compile the lambda body */` |
|     325 |  2061 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     325 |  2062 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2063 | `		return SXERR_ABORT;` |
|       - |  2064 | `	}` |
|       - |  2065 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|       - |  2066 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|       - |  2067 | `	 * the handler wraps either in a Closure instance. */` |
|     325 |  2068 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       - |  2069 | `	/* Node successfully compiled */` |
|     325 |  2070 | `	return SXRET_OK;` |
|     165 |  2071 | `}` |
|       - |  2072 | `/*` |
|       - |  2073 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  2074 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  2075 | ` * enclosing arrow level, or has already been captured.` |
|       - |  2076 | ` */` |
|     184 |  2077 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  2078 | `	ph7_gen_state *pGen,` |
|       - |  2079 | `	ph7_vm_func *pFunc,` |
|       - |  2080 | `	const char *zName,` |
|       - |  2081 | `	sxu32 nByte,` |
|       - |  2082 | `	SyString *aShadow,` |
|       - |  2083 | `	sxu32 nShadow)` |
|       2 |  2084 | `{` |
|       - |  2085 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2086 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  2087 | `	sxu32 n, nEnv;` |
|       - |  2088 | `	char *zDup;` |
|     186 |  2089 | `	if( nByte == 0 ){` |
|     ! 0 |  2090 | `		return SXRET_OK;` |
|       - |  2091 | `	}` |
|     184 |  2092 | `	if( nByte == sizeof("this")-1` |
|     100 |  2093 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  2094 | `		return SXRET_OK;` |
|       - |  2095 | `	}` |
|     232 |  2096 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     172 |  2097 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     165 |  2098 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|     126 |  2099 | `			return SXRET_OK;` |
|       - |  2100 | `		}` |
|      26 |  2101 | `	}` |
|      59 |  2102 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      59 |  2103 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      87 |  2104 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  2105 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  2106 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  2107 | `			return SXRET_OK;` |
|       - |  2108 | `		}` |
|      15 |  2109 | `	}` |
|      59 |  2110 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      59 |  2111 | `	if( zDup == 0 ){` |
|     ! 0 |  2112 | `		return SXERR_ABORT;` |
|       - |  2113 | `	}` |
|      59 |  2114 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      59 |  2115 | `	sEnv.iFlags = 0;` |
|      59 |  2116 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      59 |  2117 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      59 |  2118 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      59 |  2119 | `	return SXRET_OK;` |
|      94 |  2120 | `}` |
|       - |  2121 | `/*` |
|       - |  2122 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  2123 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  2124 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  2125 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  2126 | ` */` |
|      36 |  2127 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  2128 | `	ph7_gen_state *pGen,` |
|       - |  2129 | `	ph7_vm_func *pFunc,` |
|       - |  2130 | `	const char *zIn,` |
|       - |  2131 | `	const char *zEnd,` |
|       - |  2132 | `	SyString *aShadow,` |
|       - |  2133 | `	sxu32 nShadow)` |
|       2 |  2134 | `{` |
|       - |  2135 | `	sxi32 rc;` |
|     302 |  2136 | `	while( zIn < zEnd ){` |
|     266 |  2137 | `		if( zIn[0] == '\\' ){` |
|       5 |  2138 | `			zIn++;` |
|       5 |  2139 | `			if( zIn < zEnd ){` |
|       5 |  2140 | `				zIn++;` |
|       2 |  2141 | `			}` |
|       5 |  2142 | `			continue;` |
|       - |  2143 | `		}` |
|     260 |  2144 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      22 |  2145 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      20 |  2146 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  2147 | `			const char *zName;` |
|      22 |  2148 | `			zIn++; /* skip '$' */` |
|      22 |  2149 | `			zName = zIn;` |
|      74 |  2150 | `			while( zIn < zEnd ){` |
|      70 |  2151 | `				unsigned char c = (unsigned char)zIn[0];` |
|      70 |  2152 | `				if( c >= 0xc0 ){` |
|     ! 0 |  2153 | `					zIn++;` |
|     ! 0 |  2154 | `					while( zIn < zEnd` |
|     ! 0 |  2155 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  2156 | `						zIn++;` |
|     ! 0 |  2157 | `					}` |
|     ! 0 |  2158 | `					continue;` |
|       - |  2159 | `				}` |
|      70 |  2160 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      18 |  2161 | `					break;` |
|       - |  2162 | `				}` |
|      54 |  2163 | `				zIn++;` |
|       2 |  2164 | `			}` |
|      22 |  2165 | `			if( zIn > zName ){` |
|      32 |  2166 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      20 |  2167 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      22 |  2168 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2169 | `					return SXERR_ABORT;` |
|       - |  2170 | `				}` |
|      10 |  2171 | `			}` |
|      22 |  2172 | `			continue;` |
|       - |  2173 | `		}` |
|     242 |  2174 | `		zIn++;` |
|       2 |  2175 | `	}` |
|      38 |  2176 | `	return SXRET_OK;` |
|      20 |  2177 | `}` |
|       - |  2178 | `/*` |
|       - |  2179 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  2180 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  2181 | ` *   - plain $<id> pairs` |
|       - |  2182 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  2183 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  2184 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  2185 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  2186 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  2187 | ` *     are never mistakenly captured.` |
|       - |  2188 | ` */` |
|     198 |  2189 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  2190 | `	ph7_gen_state *pGen,` |
|       - |  2191 | `	ph7_vm_func *pFunc,` |
|       - |  2192 | `	SyToken *pStart,` |
|       - |  2193 | `	SyToken *pEnd,` |
|       - |  2194 | `	SyString *aShadow,` |
|       - |  2195 | `	sxu32 nShadow)` |
|       3 |  2196 | `{` |
|     201 |  2197 | `	SyToken *pScan = pStart;` |
|       - |  2198 | `	sxi32 rc;` |
|     839 |  2199 | `	while( pScan < pEnd ){` |
|     641 |  2200 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      56 |  2201 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|      18 |  2202 | `				pScan->sData.zString,` |
|      36 |  2203 | `				pScan->sData.zString + pScan->sData.nByte,` |
|      18 |  2204 | `				aShadow,nShadow);` |
|      38 |  2205 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2206 | `				return SXERR_ABORT;` |
|       - |  2207 | `			}` |
|      38 |  2208 | `			pScan++;` |
|      38 |  2209 | `			continue;` |
|       - |  2210 | `		}` |
|     605 |  2211 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      24 |  2212 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      24 |  2213 | `			SyToken *pFnKw = pScan;` |
|      22 |  2214 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2215 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       2 |  2216 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2217 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2218 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2219 | `			}` |
|      24 |  2220 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2221 | `				SyToken *pInnerSigStart;` |
|       - |  2222 | `				SyToken *pInnerSigEnd;` |
|       - |  2223 | `				SyToken *pInnerBodyEnd;` |
|       - |  2224 | `				SyString *aInnerShadow;` |
|       - |  2225 | `				sxu32 nInnerShadow;` |
|       - |  2226 | `				sxu32 nInnerParamMax;` |
|       - |  2227 | `				SyToken *p;` |
|       - |  2228 | `				int iNestInner;` |
|      19 |  2229 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2230 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2231 | `					pScan++;` |
|     ! 0 |  2232 | `				}` |
|      19 |  2233 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2234 | `					pScan++;` |
|     ! 0 |  2235 | `					continue;` |
|       - |  2236 | `				}` |
|      19 |  2237 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2238 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2239 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2240 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2241 | `					pScan = pEnd;` |
|     ! 0 |  2242 | `					continue;` |
|       - |  2243 | `				}` |
|       - |  2244 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2245 | `				nInnerParamMax = 0;` |
|      57 |  2246 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2247 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2248 | `						nInnerParamMax++;` |
|       6 |  2249 | `					}` |
|      20 |  2250 | `				}` |
|      19 |  2251 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2252 | `					&pGen->pVm->sAllocator,` |
|      18 |  2253 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2254 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2255 | `					return SXERR_ABORT;` |
|       - |  2256 | `				}` |
|      19 |  2257 | `				nInnerShadow = 0;` |
|      25 |  2258 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2259 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2260 | `				}` |
|      57 |  2261 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2262 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2263 | `						continue;` |
|       - |  2264 | `					}` |
|      13 |  2265 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2266 | `						break;` |
|       - |  2267 | `					}` |
|      13 |  2268 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2269 | `						continue;` |
|       - |  2270 | `					}` |
|      13 |  2271 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2272 | `				}` |
|      19 |  2273 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2274 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2275 | `					pScan++;` |
|     ! 0 |  2276 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2277 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2278 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2279 | `						pScan++;` |
|     ! 0 |  2280 | `					}` |
|     ! 0 |  2281 | `					if( pScan < pEnd` |
|     ! 0 |  2282 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2283 | `						pScan++;` |
|     ! 0 |  2284 | `					}` |
|     ! 0 |  2285 | `				}` |
|      19 |  2286 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2287 | `					pScan++; /* past '=>' */` |
|       9 |  2288 | `				}` |
|      19 |  2289 | `				pInnerBodyEnd = pScan;` |
|      19 |  2290 | `				iNestInner = 0;` |
|     131 |  2291 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2292 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2293 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2294 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2295 | `						break;` |
|       - |  2296 | `					}` |
|     113 |  2297 | `					if( pInnerBodyEnd->nType &` |
|       - |  2298 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2299 | `						iNestInner++;` |
|     112 |  2300 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2301 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2302 | `						iNestInner--;` |
|       1 |  2303 | `					}` |
|     113 |  2304 | `					pInnerBodyEnd++;` |
|       1 |  2305 | `				}` |
|       - |  2306 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2307 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2308 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2309 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2310 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2311 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2312 | `				 *` |
|       - |  2313 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2314 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2315 | `				 * range after the '=' sign. */` |
|       - |  2316 | `				{` |
|      19 |  2317 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2318 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2319 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2320 | `						SyToken *pEq = 0;` |
|      13 |  2321 | `						int iNestArg = 0;` |
|      49 |  2322 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2323 | `							if( iNestArg == 0` |
|      39 |  2324 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2325 | `								break;` |
|       - |  2326 | `							}` |
|      37 |  2327 | `							if( pArgEnd->nType &` |
|       - |  2328 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2329 | `								iNestArg++;` |
|      37 |  2330 | `							}else if( pArgEnd->nType &` |
|       - |  2331 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2332 | `								iNestArg--;` |
|     ! 0 |  2333 | `							}` |
|      36 |  2334 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2335 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2336 | `								pEq = pArgEnd;` |
|       3 |  2337 | `							}` |
|      37 |  2338 | `							pArgEnd++;` |
|       1 |  2339 | `						}` |
|      13 |  2340 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2341 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2342 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2343 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2344 | `								return SXERR_ABORT;` |
|       - |  2345 | `							}` |
|       3 |  2346 | `						}` |
|      13 |  2347 | `						pArgStart = pArgEnd;` |
|      12 |  2348 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2349 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2350 | `							pArgStart++;` |
|       1 |  2351 | `						}` |
|       1 |  2352 | `					}` |
|       - |  2353 | `				}` |
|      28 |  2354 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2355 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2356 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2357 | `					return SXERR_ABORT;` |
|       - |  2358 | `				}` |
|      19 |  2359 | `				pScan = pInnerBodyEnd;` |
|      19 |  2360 | `				continue;` |
|       - |  2361 | `			}` |
|       2 |  2362 | `		}` |
|     587 |  2363 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     423 |  2364 | `			pScan++;` |
|     423 |  2365 | `			continue;` |
|       - |  2366 | `		}` |
|       - |  2367 | `		{` |
|       - |  2368 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     166 |  2369 | `			SyToken *pDollar = pScan;` |
|     246 |  2370 | `			while( &pDollar[1] < pEnd` |
|     166 |  2371 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2372 | `				pDollar++;` |
|     ! 0 |  2373 | `			}` |
|     166 |  2374 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2375 | `				break;` |
|       - |  2376 | `			}` |
|     166 |  2377 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2378 | `				pScan = pDollar + 1;` |
|     ! 0 |  2379 | `				continue;` |
|       - |  2380 | `			}` |
|     248 |  2381 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     164 |  2382 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      82 |  2383 | `				aShadow,nShadow);` |
|     166 |  2384 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2385 | `				return SXERR_ABORT;` |
|       - |  2386 | `			}` |
|     166 |  2387 | `			pScan = pDollar + 2;` |
|       - |  2388 | `		}` |
|       2 |  2389 | `	}` |
|     201 |  2390 | `	return SXRET_OK;` |
|     102 |  2391 | `}` |
|       - |  2392 | `/*` |
|       - |  2393 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2394 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2395 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2396 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2397 | ` * $this is also made available.` |
|       - |  2398 | ` */` |
|     180 |  2399 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2400 | `{` |
|       - |  2401 | `	ph7_vm_func *pFunc;` |
|       - |  2402 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2403 | `	GenBlock *pBlock;` |
|       - |  2404 | `	SySet *pInstrContainer;` |
|       - |  2405 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2406 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2407 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2408 | `	SyToken *pSavedEnd;` |
|       - |  2409 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2410 | `	char zName[512];` |
|       - |  2411 | `	static int iCnt = 1;` |
|       - |  2412 | `	char *zDup;` |
|       - |  2413 | `	sxu32 nLen;` |
|       - |  2414 | `	sxu32 nLine;` |
|     184 |  2415 | `	sxi32 iFlags = 0;` |
|     184 |  2416 | `	int bStatic = 0;` |
|       - |  2417 | `	sxi32 rc;` |
|       - |  2418 | `	sxu32 n;` |
|      90 |  2419 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2420 |  |
|     184 |  2421 | `	nLine = pGen->pIn->nLine;` |
|       - |  2422 | `	/* Optional 'static' prefix */` |
|     180 |  2423 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     184 |  2424 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2425 | `		bStatic = 1;` |
|       3 |  2426 | `		pGen->pIn++;` |
|       1 |  2427 | `	}` |
|       - |  2428 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     180 |  2429 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     184 |  2430 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2431 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2432 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2433 | `		return SXERR_SYNTAX;` |
|       - |  2434 | `	}` |
|     184 |  2435 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2436 | `	/* Optional '&' — return by reference */` |
|     184 |  2437 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2438 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2439 | `		pGen->pIn++;` |
|     ! 0 |  2440 | `	}` |
|       - |  2441 | `	/* Expect '(' */` |
|     184 |  2442 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2443 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2444 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2445 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2446 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2447 | `		}else{` |
|     ! 0 |  2448 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2449 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2450 | `		}` |
|       3 |  2451 | `		return SXERR_SYNTAX;` |
|       - |  2452 | `	}` |
|     181 |  2453 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2454 | `	/* Delimit the parameter list */` |
|     181 |  2455 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     181 |  2456 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2457 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2458 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2459 | `		return SXERR_SYNTAX;` |
|       - |  2460 | `	}` |
|       - |  2461 | `	/* Allocate the function state */` |
|     179 |  2462 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     179 |  2463 | `	if( pFunc == 0 ){` |
|     ! 0 |  2464 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2465 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2466 | `		return SXERR_ABORT;` |
|       - |  2467 | `	}` |
|       - |  2468 | `	/* Generate a unique lambda name */` |
|     179 |  2469 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     277 |  2470 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     100 |  2471 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2472 | `	}` |
|     179 |  2473 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     179 |  2474 | `	if( zDup == 0 ){` |
|     ! 0 |  2475 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2476 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2477 | `		return SXERR_ABORT;` |
|       - |  2478 | `	}` |
|     179 |  2479 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2480 | `	/* Collect function arguments */` |
|     179 |  2481 | `	if( pGen->pIn < pSigEnd ){` |
|     103 |  2482 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|     103 |  2483 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2484 | `			return SXERR_ABORT;` |
|       - |  2485 | `		}` |
|      50 |  2486 | `	}` |
|       - |  2487 | `	/* Point past ')' and parse optional return type */` |
|     179 |  2488 | `	pGen->pIn = &pSigEnd[1];` |
|     179 |  2489 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     179 |  2490 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2491 | `		return SXERR_ABORT;` |
|     179 |  2492 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2493 | `		return SXERR_SYNTAX;` |
|       - |  2494 | `	}` |
|       - |  2495 | `	/* Expect '=>' */` |
|     179 |  2496 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2497 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2498 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2499 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2500 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2501 | `		}else{` |
|     ! 0 |  2502 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2503 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2504 | `		}` |
|       3 |  2505 | `		return SXERR_SYNTAX;` |
|       - |  2506 | `	}` |
|     177 |  2507 | `	pGen->pIn++; /* Jump '=>' */` |
|     177 |  2508 | `	pBodyStart = pGen->pIn;` |
|     177 |  2509 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2510 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2511 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2512 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2513 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     177 |  2514 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2515 | `	{` |
|     177 |  2516 | `		SyString *aShadow = 0;` |
|     177 |  2517 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     177 |  2518 | `		if( nShadow > 0 ){` |
|     100 |  2519 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      98 |  2520 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|     100 |  2521 | `			if( aShadow == 0 ){` |
|     ! 0 |  2522 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2523 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2524 | `				return SXERR_ABORT;` |
|       - |  2525 | `			}` |
|     224 |  2526 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     126 |  2527 | `				aShadow[n] = aArgs[n].sName;` |
|      64 |  2528 | `			}` |
|      49 |  2529 | `		}` |
|     264 |  2530 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      87 |  2531 | `			aShadow,nShadow);` |
|     177 |  2532 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2533 | `			return SXERR_ABORT;` |
|       - |  2534 | `		}` |
|       - |  2535 | `	}` |
|       - |  2536 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2537 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2538 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2539 | `	 * $this. */` |
|     177 |  2540 | `	if( !bStatic ){` |
|       - |  2541 | `		char *zThisDup;` |
|     175 |  2542 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     175 |  2543 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2544 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2545 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2546 | `			return SXERR_ABORT;` |
|       - |  2547 | `		}` |
|     175 |  2548 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     175 |  2549 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     175 |  2550 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     175 |  2551 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     175 |  2552 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      86 |  2553 | `	}` |
|       - |  2554 | `	/* Arrow functions are always closures */` |
|     177 |  2555 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2556 | `	/* Compile the body expression as an implicit return */` |
|     264 |  2557 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      87 |  2558 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     177 |  2559 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2560 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2561 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2562 | `		return SXERR_ABORT;` |
|       - |  2563 | `	}` |
|     177 |  2564 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     177 |  2565 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     177 |  2566 | `	pSavedEnd = pGen->pEnd;` |
|     177 |  2567 | `	pGen->pIn = pBodyStart;` |
|     177 |  2568 | `	pGen->pEnd = pBodyEnd;` |
|     177 |  2569 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     177 |  2570 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2571 | `		return SXERR_ABORT;` |
|       - |  2572 | `	}` |
|       - |  2573 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2574 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2575 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2576 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     177 |  2577 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     177 |  2578 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     177 |  2579 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     177 |  2580 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     177 |  2581 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2582 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     177 |  2583 | `	pGen->pIn = pBodyEnd;` |
|     177 |  2584 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2585 | `	/* Emit the load-closure instruction */` |
|     177 |  2586 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     177 |  2587 | `	return SXRET_OK;` |
|      94 |  2588 | `}` |
|       - |  2589 | `/*` |
|       - |  2590 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2591 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2592 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2593 | ` * expression's value.` |
|       - |  2594 | ` */` |
|     346 |  2595 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2596 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  2597 | `{` |
|       - |  2598 | `	SySet *pInstrContainer;` |
|       - |  2599 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2600 | `	GenBlock *pArmBlock;` |
|       - |  2601 | `	sxi32 rc;` |
|     349 |  2602 | `	pTmpIn  = pGen->pIn;` |
|     349 |  2603 | `	pTmpEnd = pGen->pEnd;` |
|     349 |  2604 | `	pGen->pIn  = pStart;` |
|     349 |  2605 | `	pGen->pEnd = pStop;` |
|     349 |  2606 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     349 |  2607 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2608 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2609 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2610 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2611 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2612 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     522 |  2613 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2614 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     349 |  2615 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2616 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2617 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2618 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2619 | `		return SXERR_ABORT;` |
|       - |  2620 | `	}` |
|     349 |  2621 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     349 |  2622 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     349 |  2623 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     349 |  2624 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     349 |  2625 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     349 |  2626 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     349 |  2627 | `	pGen->pIn  = pTmpIn;` |
|     349 |  2628 | `	pGen->pEnd = pTmpEnd;` |
|     349 |  2629 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2630 | `		return SXERR_ABORT;` |
|       - |  2631 | `	}` |
|     349 |  2632 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2633 | `		return SXERR_EMPTY;` |
|       - |  2634 | `	}` |
|     349 |  2635 | `	return SXRET_OK;` |
|     176 |  2636 | `}` |
|       - |  2637 | `/*` |
|       - |  2638 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2639 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2640 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2641 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2642 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2643 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2644 | ` */` |
|       - |  2645 | `/*` |
|       - |  2646 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2647 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2648 | ` * caller can bail out of the current expression.` |
|       - |  2649 | ` */` |
|       2 |  2650 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2651 | `{` |
|       - |  2652 | `	va_list ap;` |
|       - |  2653 | `	sxi32 rc;` |
|       - |  2654 | `	SyBlob sMsg;` |
|       3 |  2655 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2656 | `	va_start(ap,zFmt);` |
|       3 |  2657 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2658 | `	va_end(ap);` |
|       3 |  2659 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2660 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2661 | `	SyBlobRelease(&sMsg);` |
|       3 |  2662 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2663 | `		return SXERR_ABORT;` |
|       - |  2664 | `	}` |
|       3 |  2665 | `	return SXERR_SYNTAX;` |
|       2 |  2666 | `}` |
|       - |  2667 | `/*` |
|       - |  2668 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2669 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2670 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2671 | ` */` |
|     348 |  2672 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  2673 | `{` |
|     352 |  2674 | `	SyToken *pCur = pStart;` |
|     352 |  2675 | `	int iNest = 0;` |
|     814 |  2676 | `	while( pCur < pEnd ){` |
|     780 |  2677 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2678 | `			iNest++;` |
|     774 |  2679 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2680 | `			iNest--;` |
|     762 |  2681 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     317 |  2682 | `			return pCur;` |
|       - |  2683 | `		}` |
|     466 |  2684 | `		pCur++;` |
|       4 |  2685 | `	}` |
|      37 |  2686 | `	return pEnd;` |
|     178 |  2687 | `}` |
|      70 |  2688 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2689 | `{` |
|       - |  2690 | `	ph7_match *pMatch;` |
|       - |  2691 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      75 |  2692 | `	int bHasDefault = 0;` |
|       - |  2693 | `	sxu32 nLine;` |
|       - |  2694 | `	sxi32 rc;` |
|      35 |  2695 | `	SXUNUSED(iCompileFlag);` |
|      75 |  2696 | `	nLine = pGen->pIn->nLine;` |
|      75 |  2697 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2698 | `	/* Expect '(' */` |
|      75 |  2699 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2700 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2701 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2702 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2703 | `	}` |
|      75 |  2704 | `	pGen->pIn++; /* Jump '(' */` |
|      75 |  2705 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      75 |  2706 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2707 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2708 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2709 | `	}` |
|      75 |  2710 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2711 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2712 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2713 | `	}` |
|       - |  2714 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      75 |  2715 | `	pSavedEnd = pGen->pEnd;` |
|      75 |  2716 | `	pGen->pEnd = pSubjEnd;` |
|      75 |  2717 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      75 |  2718 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2719 | `		return SXERR_ABORT;` |
|       - |  2720 | `	}` |
|      75 |  2721 | `	pGen->pEnd = pSavedEnd;` |
|      75 |  2722 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2723 | `	/* Expect '{' */` |
|      75 |  2724 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2725 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2726 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2727 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2728 | `	}` |
|      75 |  2729 | `	pGen->pIn++; /* Jump '{' */` |
|      75 |  2730 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      75 |  2731 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2732 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2733 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2734 | `	}` |
|       - |  2735 | `	/* Allocate ph7_match container */` |
|      75 |  2736 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      75 |  2737 | `	if( pMatch == 0 ){` |
|     ! 0 |  2738 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2739 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2740 | `		return SXERR_ABORT;` |
|       - |  2741 | `	}` |
|      75 |  2742 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      75 |  2743 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2744 | `	/* Iterate arms */` |
|     253 |  2745 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2746 | `		ph7_match_arm sArm;` |
|       - |  2747 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     186 |  2748 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     186 |  2749 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     186 |  2750 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     186 |  2751 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2752 | `		/* 'default' arm? */` |
|     182 |  2753 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 |  2754 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2755 | `			if( bHasDefault ){` |
|       3 |  2756 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2757 | `					"Match expressions may only contain one default arm");` |
|       4 |  2758 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2759 | `			}` |
|      20 |  2760 | `			sArm.bDefault = 1;` |
|      20 |  2761 | `			bHasDefault = 1;` |
|      20 |  2762 | `			pGen->pIn++;` |
|      20 |  2763 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2764 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2765 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2766 | `			}` |
|      20 |  2767 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2768 | `		}else{` |
|       - |  2769 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     166 |  2770 | `			pCondStart = pGen->pIn;` |
|     166 |  2771 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2772 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     174 |  2773 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2774 | `				SySet sCondBc;` |
|       9 |  2775 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2776 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2777 | `						"syntax error, empty match condition expression");` |
|       - |  2778 | `				}` |
|       9 |  2779 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2780 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2781 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2782 | `					return SXERR_ABORT;` |
|       - |  2783 | `				}` |
|       9 |  2784 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2785 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2786 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2787 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2788 | `			}` |
|     166 |  2789 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2790 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2791 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2792 | `			}` |
|     163 |  2793 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2794 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2795 | `					"syntax error, empty match condition expression");` |
|       - |  2796 | `			}` |
|       - |  2797 | `			{` |
|       - |  2798 | `				SySet sCondBc;` |
|     163 |  2799 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     163 |  2800 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     163 |  2801 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2802 | `					return SXERR_ABORT;` |
|       - |  2803 | `				}` |
|     163 |  2804 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2805 | `			}` |
|     163 |  2806 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2807 | `		}` |
|       - |  2808 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     181 |  2809 | `		pResStart = pGen->pIn;` |
|     181 |  2810 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     181 |  2811 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2812 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2813 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2814 | `		}` |
|     181 |  2815 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     181 |  2816 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2817 | `			return SXERR_ABORT;` |
|       - |  2818 | `		}` |
|     181 |  2819 | `		pGen->pIn = pResEnd;` |
|     181 |  2820 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     149 |  2821 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2822 | `		}` |
|     181 |  2823 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       3 |  2824 | `	}` |
|      69 |  2825 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      69 |  2826 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      69 |  2827 | `	return SXRET_OK;` |
|      40 |  2828 | `}` |
|       - |  2829 | `/*` |
|       - |  2830 | ` * Compile a backtick quoted string.` |
|       - |  2831 | ` */` |
|       4 |  2832 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2833 | `{` |
|       - |  2834 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2835 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2836 | `	 */` |
|       8 |  2837 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2838 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2839 | `		ph7_lib_version()` |
|       - |  2840 | `		);` |
|       - |  2841 | `	/* Load NULL */` |
|       6 |  2842 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2843 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2844 | `	/* Node successfully compiled */` |
|       6 |  2845 | `	return SXRET_OK;` |
|       2 |  2846 | `}` |
|       - |  2847 | `/*` |
|       - |  2848 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2849 | ` * construct.` |
|       - |  2850 | ` */` |
|      82 |  2851 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2852 | `{` |
|       - |  2853 | `	SyString *pName;` |
|       - |  2854 | `	sxu32 nKeyID;` |
|       - |  2855 | `	sxi32 rc;` |
|       - |  2856 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      87 |  2857 | `	pName = &pGen->pIn->sData;` |
|      87 |  2858 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      87 |  2859 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      87 |  2860 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2861 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2862 | `		/* Compile arguments one after one */` |
|       9 |  2863 | `		pTmp = pGen->pEnd;` |
|       - |  2864 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2865 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2866 | `		 *  mean that the following expression is valid:` |
|       - |  2867 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2868 | `		 */` |
|       9 |  2869 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2870 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2871 | `			if( pGen->pIn < pNext ){` |
|       9 |  2872 | `				pGen->pEnd = pNext;` |
|       9 |  2873 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2874 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2875 | `					return SXERR_ABORT;` |
|       - |  2876 | `				}` |
|       9 |  2877 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2878 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2879 | `					 * without the overhead of a function call.` |
|       - |  2880 | `					 * This is a very powerful optimization that improve` |
|       - |  2881 | `					 * performance greatly.` |
|       - |  2882 | `					 */` |
|       9 |  2883 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2884 | `				}` |
|       4 |  2885 | `			}` |
|       - |  2886 | `			/* Jump trailing commas */` |
|       9 |  2887 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2888 | `				pNext++;` |
|     ! 0 |  2889 | `			}` |
|       9 |  2890 | `			pGen->pIn = pNext;` |
|       1 |  2891 | `		}` |
|       - |  2892 | `		/* Restore token stream */` |
|       9 |  2893 | `		pGen->pEnd = pTmp;` |
|       5 |  2894 | `	}else{` |
|      79 |  2895 | `		sxi32 nArg = 0;` |
|      79 |  2896 | `		sxu32 nIdx = 0;` |
|      79 |  2897 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      79 |  2898 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2899 | `			return SXERR_ABORT;` |
|      79 |  2900 | `		}else if(rc != SXERR_EMPTY ){` |
|      79 |  2901 | `			nArg = 1;` |
|      37 |  2902 | `		}` |
|      79 |  2903 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2904 | `			ph7_value *pObj;` |
|       - |  2905 | `			/* Emit the call instruction */` |
|      31 |  2906 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      31 |  2907 | `			if( pObj == 0 ){` |
|     ! 0 |  2908 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2909 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2910 | `				return SXERR_ABORT;` |
|       - |  2911 | `			}` |
|      31 |  2912 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2913 | `			/* Install in the literal table */` |
|      31 |  2914 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      13 |  2915 | `		}` |
|       - |  2916 | `		/* Emit the call instruction */` |
|      79 |  2917 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      79 |  2918 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  2919 | `	}` |
|       - |  2920 | `	/* Node successfully compiled */` |
|      87 |  2921 | `	return SXRET_OK;` |
|      46 |  2922 | `}` |
|       - |  2923 | `/*` |
|       - |  2924 | ` * Compile a node holding a variable declaration.` |
|       - |  2925 | ` * According to the PHP language reference` |
|       - |  2926 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2927 | ` *  The variable name is case-sensitive.` |
|       - |  2928 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2929 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2930 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2931 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2932 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2933 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2934 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2935 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2936 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2937 | ` *  the chapter on Expressions.` |
|       - |  2938 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2939 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2940 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2941 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2942 | ` *  is being assigned (the source variable).` |
|       - |  2943 | ` */` |
| 1224366 |  2944 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2945 | `{` |
| 1224371 |  2946 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2947 | `	sxi32 iVv;` |
|       - |  2948 | `	sxi32 iP1;` |
|       - |  2949 | `	void *p3;` |
|       - |  2950 | `	sxi32 rc;` |
| 1224371 |  2951 | `	iVv = -1; /* Variable variable counter */` |
| 2448749 |  2952 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1224383 |  2953 | `		pGen->pIn++;` |
| 1224383 |  2954 | `		iVv++;` |
|       5 |  2955 | `	}` |
| 1224371 |  2956 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2957 | `		/* Invalid variable name */` |
|     ! 0 |  2958 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2959 | `		if( rc == SXERR_ABORT ){` |
|       - |  2960 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2961 | `			return SXERR_ABORT;` |
|       - |  2962 | `		}` |
|     ! 0 |  2963 | `		return SXRET_OK;` |
|       - |  2964 | `	}` |
| 1224371 |  2965 | `	p3  = 0;` |
| 1224371 |  2966 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2967 | `		/* Dynamic variable creation */` |
|      19 |  2968 | `		pGen->pIn++;  /* Jump the open curly */` |
|      19 |  2969 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      19 |  2970 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2971 | `			/* Empty expression */` |
|       3 |  2972 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2973 | `			return SXRET_OK;` |
|       - |  2974 | `		}` |
|       - |  2975 | `		/* Compile the expression holding the variable name */` |
|      16 |  2976 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2977 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2978 | `			return SXERR_ABORT;` |
|      16 |  2979 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2980 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2981 | `			return SXRET_OK;` |
|       - |  2982 | `		}` |
|       7 |  2983 | `	}else{` |
|       - |  2984 | `		SyHashEntry *pEntry;` |
|       - |  2985 | `		SyString *pName;` |
| 1224355 |  2986 | `		char *zName = 0;` |
|       - |  2987 | `		/* Extract variable name */` |
| 1224355 |  2988 | `		pName = &pGen->pIn->sData;` |
|       - |  2989 | `		/* Advance the stream cursor */` |
| 1224355 |  2990 | `		pGen->pIn++;` |
| 1224355 |  2991 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1224355 |  2992 | `		if( pEntry == 0 ){` |
|       - |  2993 | `			/* Duplicate name */` |
|  176423 |  2994 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  176423 |  2995 | `			if( zName == 0 ){` |
|     ! 0 |  2996 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2997 | `				return SXERR_ABORT;` |
|       - |  2998 | `			}` |
|       - |  2999 | `			/* Install in the hashtable */` |
|  176423 |  3000 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   88214 |  3001 | `		}else{` |
|       - |  3002 | `			/* Name already available */` |
| 1047937 |  3003 | `			zName = (char *)pEntry->pUserData;` |
|       - |  3004 | `		}` |
| 1224355 |  3005 | `		p3 = (void *)zName;` |
|       - |  3006 | `	}` |
| 1224367 |  3007 | `	iP1 = 0;` |
| 1224367 |  3008 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  477205 |  3009 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  3010 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  477187 |  3011 | `			iP1 = 1;` |
|  238591 |  3012 | `		}` |
|  238600 |  3013 | `	}` |
|       - |  3014 | `	/* Emit the load instruction */` |
| 1224367 |  3015 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1224379 |  3016 | `	while( iVv > 0 ){` |
|      13 |  3017 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  3018 | `		iVv--;` |
|       1 |  3019 | `	}` |
|       - |  3020 | `	/* Node successfully compiled */` |
| 1224367 |  3021 | `	return SXRET_OK;` |
|  612188 |  3022 | `}` |
|       - |  3023 | `/*` |
|       - |  3024 | ` * Load a literal.` |
|       - |  3025 | ` */` |
|  844976 |  3026 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  3027 | `{` |
|  844981 |  3028 | `	SyToken *pToken = pGen->pIn;` |
|       - |  3029 | `	ph7_value *pObj;` |
|       - |  3030 | `	SyString *pStr;` |
|       - |  3031 | `	sxu32 nIdx;` |
|       - |  3032 | `	/* Extract token value */` |
|  844981 |  3033 | `	pStr = &pToken->sData;` |
|       - |  3034 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  844981 |  3035 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  179103 |  3036 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  3037 | `			/* NULL constant are always indexed at 0 */` |
|   65847 |  3038 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   65847 |  3039 | `			return SXRET_OK;` |
|  113261 |  3040 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  3041 | `			/* TRUE constant are always indexed at 1 */` |
|     829 |  3042 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     829 |  3043 | `			return SXRET_OK;` |
|       5 |  3044 | `		}` |
|  779275 |  3045 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  114352 |  3046 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  3047 | `			/* FALSE constant are always indexed at 2 */` |
|   50381 |  3048 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   50381 |  3049 | `			return SXRET_OK;` |
|  675468 |  3050 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  119922 |  3051 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  3052 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|   11501 |  3053 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   11501 |  3054 | `			if( pObj == 0 ){` |
|     ! 0 |  3055 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3056 | `				return SXERR_ABORT;` |
|       - |  3057 | `			}` |
|   11501 |  3058 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  3059 | `			/* Emit the load constant instruction */` |
|   11501 |  3060 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   11501 |  3061 | `			return SXRET_OK;` |
|  623381 |  3062 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   38740 |  3063 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  3064 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 |  3065 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 |  3066 | `			if( pObj == 0 ){` |
|     ! 0 |  3067 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3068 | `				return SXERR_ABORT;` |
|       - |  3069 | `			}` |
|       7 |  3070 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  3071 | `				SyString sNs;` |
|       7 |  3072 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 |  3073 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 |  3074 | `			}else{` |
|     ! 0 |  3075 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  3076 | `			}` |
|       7 |  3077 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 |  3078 | `			return SXRET_OK;` |
|  612112 |  3079 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   26846 |  3080 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  614610 |  3081 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   21234 |  3082 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  3083 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  3084 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  3085 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  3086 | `				/* Point to the upper block */` |
|      11 |  3087 | `				pBlock = pBlock->pParent;` |
|       1 |  3088 | `			}` |
|      11 |  3089 | `			if( pBlock == 0 ){` |
|       - |  3090 | `				/* Called in the global scope,load NULL */` |
|       5 |  3091 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  3092 | `			}else{` |
|       - |  3093 | `				/* Extract the target function/method */` |
|       7 |  3094 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  3095 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  3096 | `					/* Not a class method,Load null */` |
|       3 |  3097 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  3098 | `				}else{` |
|       5 |  3099 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  3100 | `					if( pObj == 0 ){` |
|     ! 0 |  3101 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3102 | `						return SXERR_ABORT;` |
|       - |  3103 | `					}` |
|       5 |  3104 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  3105 | `					/* Emit the load constant instruction */` |
|       5 |  3106 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  3107 | `				}` |
|       - |  3108 | `			}` |
|      11 |  3109 | `			return SXRET_OK;` |
|       - |  3110 | `	}` |
|       - |  3111 | `	/* Query literal table */` |
|  716427 |  3112 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  3113 | `		ph7_value *pLitObj;` |
|       - |  3114 | `		/* Unknown literal,install it in the literal table */` |
|  305387 |  3115 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  305387 |  3116 | `		if( pLitObj == 0 ){` |
|     ! 0 |  3117 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3118 | `			return SXERR_ABORT;` |
|       - |  3119 | `		}` |
|  305387 |  3120 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  305387 |  3121 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  152691 |  3122 | `	}` |
|       - |  3123 | `	/* Emit the load constant instruction */` |
|  716427 |  3124 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  716427 |  3125 | `	return SXRET_OK;` |
|  422493 |  3126 | `}` |
|       - |  3127 | `/*` |
|       - |  3128 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  3129 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  3130 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  3131 | ` * Otherwise, load the simple literal directly.` |
|       - |  3132 | ` */` |
|  848854 |  3133 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  3134 | `{` |
|       - |  3135 | `	sxi32 rc;` |
|  848859 |  3136 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3137 | `		return SXRET_OK;` |
|       - |  3138 | `	}` |
|       - |  3139 | `	/* Check if this is a multi-token namespace path */` |
|  848859 |  3140 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  3141 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|    3883 |  3142 | `		SyBlob *pWorker = &pGen->sWorker;` |
|    3883 |  3143 | `		int isAbsolute = 0;` |
|    3883 |  3144 | `		SyBlobReset(pWorker);` |
|       - |  3145 | `		/* Check for leading backslash (absolute path) */` |
|    3883 |  3146 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|    3881 |  3147 | `			isAbsolute = 1;` |
|    3881 |  3148 | `			pGen->pIn++; /* Skip leading backslash */` |
|    1938 |  3149 | `		}` |
|       - |  3150 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|    3883 |  3151 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  3152 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  3153 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  3154 | `		}` |
|       - |  3155 | `		/* Collect all path components */` |
|    3991 |  3156 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    3991 |  3157 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      59 |  3158 | `				SyBlobAppend(pWorker,"\\",1);` |
|      32 |  3159 | `			}else{` |
|    3937 |  3160 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  3161 | `			}` |
|    3991 |  3162 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|    3883 |  3163 | `				pGen->pIn++;` |
|    3883 |  3164 | `				break;` |
|       - |  3165 | `			}` |
|     113 |  3166 | `			pGen->pIn++;` |
|       5 |  3167 | `		}` |
|    3883 |  3168 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  3169 | `			ph7_value *pObj;` |
|       - |  3170 | `			SyString sPath;` |
|       - |  3171 | `			sxu32 nIdx;` |
|    3883 |  3172 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  3173 | `			/* Install in the literal table */` |
|    3883 |  3174 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|    3855 |  3175 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    3855 |  3176 | `				if( pObj == 0 ){` |
|     ! 0 |  3177 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3178 | `					return SXERR_ABORT;` |
|       - |  3179 | `				}` |
|    3855 |  3180 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|    3855 |  3181 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|    1925 |  3182 | `			}` |
|       - |  3183 | `			/* Emit the load constant instruction.` |
|       - |  3184 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  3185 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|    5822 |  3186 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|    1939 |  3187 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|    1939 |  3188 | `				nIdx,0,0);` |
|    3883 |  3189 | `			return SXRET_OK;` |
|       - |  3190 | `		}` |
|     ! 0 |  3191 | `	}` |
|       - |  3192 | `	/* Single-token literal: load directly */` |
|  844981 |  3193 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  844981 |  3194 | `	return rc;` |
|  424432 |  3195 | `}` |
|       - |  3196 | `/*` |
|       - |  3197 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  3198 | ` */` |
|       - |  3199 | `/*` |
|       - |  3200 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|       - |  3201 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|       - |  3202 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|       - |  3203 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|       - |  3204 | ` */` |
|     ! 0 |  3205 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|     ! 0 |  3206 | `{` |
|     ! 0 |  3207 | `	SXUNUSED(iCompileFlag);` |
|     ! 0 |  3208 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|       - |  3209 | `		"Cannot use the first-class callable syntax '...' here");` |
|     ! 0 |  3210 | `	return SXERR_SYNTAX;` |
|     ! 0 |  3211 | `}` |
|  848854 |  3212 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3213 | `{` |
|       - |  3214 | `	sxi32 rc;` |
|  848859 |  3215 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  848859 |  3216 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3217 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3218 | `		return rc;` |
|       - |  3219 | `	}` |
|       - |  3220 | `	/* Node successfully compiled */` |
|  848859 |  3221 | `	return SXRET_OK;` |
|  424432 |  3222 | `}` |
|       - |  3223 | `/*` |
|       - |  3224 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3225 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3226 | ` */` |
|       8 |  3227 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3228 | `{` |
|       - |  3229 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3230 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3231 | `		pGen->pIn++;` |
|       1 |  3232 | `	}` |
|       9 |  3233 | `	return SXRET_OK;` |
|       1 |  3234 | `}` |
|       - |  3235 | `/*` |
|       - |  3236 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3237 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3238 | ` */` |
|     132 |  3239 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3240 | `{` |
|     137 |  3241 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      34 |  3242 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3243 | `			return TRUE;` |
|      32 |  3244 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       6 |  3245 | `			return TRUE;` |
|       3 |  3246 | `		}` |
|     119 |  3247 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3248 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3249 | `			return TRUE;` |
|       - |  3250 | `		}` |
|     ! 0 |  3251 | `	}` |
|       - |  3252 | `	/* Not a reserved constant */` |
|     129 |  3253 | `	return FALSE;` |
|      71 |  3254 | `}` |
|       - |  3255 | `/*` |
|       - |  3256 | ` * Compile the 'const' statement.` |
|       - |  3257 | ` * According to the PHP language reference` |
|       - |  3258 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3259 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3260 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3261 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3262 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3263 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3264 | ` *  Syntax` |
|       - |  3265 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3266 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3267 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3268 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3269 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3270 | ` *  to get a list of all defined constants.` |
|       - |  3271 | ` *` |
|       - |  3272 | ` * Symisc eXtension.` |
|       - |  3273 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3274 | ` *  would allow only simple scalar value.` |
|       - |  3275 | ` *  Example` |
|       - |  3276 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3277 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3278 | ` */` |
|      38 |  3279 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3280 | `{` |
|       - |  3281 | `	SySet *pConsCode,*pInstrContainer;` |
|      43 |  3282 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3283 | `	SyString *pName;` |
|       - |  3284 | `	sxi32 rc;` |
|      43 |  3285 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      43 |  3286 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3287 | `		/* Invalid constant name */` |
|       9 |  3288 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       9 |  3289 | `		if( rc == SXERR_ABORT ){` |
|       - |  3290 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3291 | `			return SXERR_ABORT;` |
|       - |  3292 | `		}` |
|       9 |  3293 | `		goto Synchronize;` |
|       - |  3294 | `	}` |
|       - |  3295 | `	/* Peek constant name */` |
|      37 |  3296 | `	pName = &pGen->pIn->sData;` |
|       - |  3297 | `	/* Make sure the constant name isn't reserved */` |
|      37 |  3298 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3299 | `		/* Reserved constant */` |
|      10 |  3300 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|      10 |  3301 | `		if( rc == SXERR_ABORT ){` |
|       - |  3302 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3303 | `			return SXERR_ABORT;` |
|       - |  3304 | `		}` |
|      10 |  3305 | `		goto Synchronize;` |
|       - |  3306 | `	}` |
|      28 |  3307 | `	pGen->pIn++;` |
|      28 |  3308 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3309 | `		/* Invalid statement*/` |
|       6 |  3310 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3311 | `		if( rc == SXERR_ABORT ){` |
|       - |  3312 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3313 | `			return SXERR_ABORT;` |
|       - |  3314 | `		}` |
|       6 |  3315 | `		goto Synchronize;` |
|       - |  3316 | `	}` |
|      22 |  3317 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3318 | `	/* Allocate a new constant value container */` |
|      22 |  3319 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      22 |  3320 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3321 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3322 | `		return SXERR_ABORT;` |
|       - |  3323 | `	}` |
|      22 |  3324 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3325 | `	/* Swap bytecode container */` |
|      22 |  3326 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      22 |  3327 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3328 | `	/* Compile constant value */` |
|      22 |  3329 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3330 | `	/* Emit the done instruction */` |
|      22 |  3331 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      22 |  3332 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      22 |  3333 | `	if( rc == SXERR_ABORT ){` |
|       - |  3334 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3335 | `		return SXERR_ABORT;` |
|       - |  3336 | `	}` |
|      22 |  3337 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3338 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3339 | `	{` |
|       - |  3340 | `		SyBlob sFQN;` |
|       - |  3341 | `		SyString sFQNStr;` |
|      22 |  3342 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      22 |  3343 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      22 |  3344 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      22 |  3345 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      22 |  3346 | `		SyBlobRelease(&sFQN);` |
|       - |  3347 | `	}` |
|      22 |  3348 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3349 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3350 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3351 | `	}` |
|      22 |  3352 | `	return SXRET_OK;` |
|       9 |  3353 | `Synchronize:` |
|       - |  3354 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3355 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      41 |  3356 | `		pGen->pIn++;` |
|       3 |  3357 | `	}` |
|      22 |  3358 | `	return SXRET_OK;` |
|      24 |  3359 | `}` |
|       - |  3360 | `/*` |
|       - |  3361 | ` * Compile the 'continue' statement.` |
|       - |  3362 | ` * According to the PHP language reference` |
|       - |  3363 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3364 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3365 | ` *  iteration.` |
|       - |  3366 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3367 | ` *  the purposes of continue.` |
|       - |  3368 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3369 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3370 | ` *  Note:` |
|       - |  3371 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3372 | ` */` |
|       - |  3373 | `/*` |
|       - |  3374 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3375 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3376 | ` * break/continue crosses a try boundary.` |
|       - |  3377 | ` *` |
|       - |  3378 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3379 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3380 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3381 | ` */` |
|    3976 |  3382 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3383 | `{` |
|    3981 |  3384 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    3981 |  3385 | `	int nInlineTry = 0;` |
|   23359 |  3386 | `	while( pBlock && pBlock != pTarget ){` |
|   19383 |  3387 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       6 |  3388 | `			if( pBlock->pUserData ){` |
|       - |  3389 | `				/* A try block with an exception context. In a generator its catch/finally` |
|       - |  3390 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|       - |  3391 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|       - |  3392 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|       6 |  3393 | `				if( pGen->bInGenerator ){` |
|       3 |  3394 | `					nInlineTry++;` |
|       2 |  3395 | `				}else{` |
|       3 |  3396 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       - |  3397 | `				}` |
|       4 |  3398 | `			}else{` |
|       - |  3399 | `				/* A catch/finally block compiled into a separate bytecode container` |
|       - |  3400 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|     ! 0 |  3401 | `				break;` |
|       - |  3402 | `			}` |
|       2 |  3403 | `		}` |
|   19383 |  3404 | `		pBlock = pBlock->pParent;` |
|       5 |  3405 | `	}` |
|    3981 |  3406 | `	return nInlineTry;` |
|       5 |  3407 | `}` |
|    3878 |  3408 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3409 | `{` |
|       - |  3410 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3411 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3412 | `	sxu32 nLineLocal;` |
|       - |  3413 | `	sxi32 rc;` |
|    3883 |  3414 | `	nLineLocal = pGen->pIn->nLine;` |
|    3883 |  3415 | `	iLevel = 0;` |
|       - |  3416 | `	/* Jump the 'continue' keyword */` |
|    3883 |  3417 | `	pGen->pIn++;` |
|    3883 |  3418 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3419 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3420 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3421 | `		 */` |
|       - |  3422 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3423 | `		char *zAlloc = 0;` |
|       - |  3424 | `		SyString sNum;` |
|      17 |  3425 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3426 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3427 | `			return SXERR_ABORT;` |
|       - |  3428 | `		}` |
|      17 |  3429 | `		if( rc == SXRET_OK ){` |
|      20 |  3430 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3431 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3432 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3433 | `				return SXERR_ABORT;` |
|       - |  3434 | `			}` |
|      14 |  3435 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3436 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3437 | `		}` |
|      17 |  3438 | `		if( iLevel < 2 ){` |
|       3 |  3439 | `			iLevel = 0;` |
|       1 |  3440 | `		}` |
|      17 |  3441 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3442 | `	}` |
|       - |  3443 | `	/* Point to the target loop */` |
|    3883 |  3444 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3883 |  3445 | `	if( pLoop == 0 ){` |
|       - |  3446 | `		/* Illegal continue */` |
|      13 |  3447 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      13 |  3448 | `		if( rc == SXERR_ABORT ){` |
|       - |  3449 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3450 | `			return SXERR_ABORT;` |
|       - |  3451 | `		}` |
|       8 |  3452 | `	}else{` |
|    3873 |  3453 | `		sxu32 nInstrIdx = 0;` |
|       - |  3454 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    3873 |  3455 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|       - |  3456 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|       - |  3457 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    3873 |  3458 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    3873 |  3459 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3460 | `			/* According to the PHP language reference manual` |
|       - |  3461 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3462 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3463 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3464 | `			 */` |
|       5 |  3465 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|       5 |  3466 | `			if( rc == SXRET_OK ){` |
|       5 |  3467 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3468 | `			}` |
|       3 |  3469 | `		}else{` |
|       - |  3470 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3869 |  3471 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3869 |  3472 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3473 | `				JumpFixup sJumpFix;` |
|       - |  3474 | `				/* Post-continue */` |
|      14 |  3475 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3476 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3477 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3478 | `			}` |
|       - |  3479 | `		}` |
|       - |  3480 | `	}` |
|    3883 |  3481 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3482 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3483 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3484 | `	}` |
|       - |  3485 | `	/* Statement successfully compiled */` |
|    3883 |  3486 | `	return SXRET_OK;` |
|    1944 |  3487 | `}` |
|       - |  3488 | `/*` |
|       - |  3489 | ` * Compile the 'break' statement.` |
|       - |  3490 | ` * According to the PHP language reference` |
|       - |  3491 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3492 | ` *  structure.` |
|       - |  3493 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3494 | ` *  enclosing structures are to be broken out of.` |
|       - |  3495 | ` */` |
|     124 |  3496 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3497 | `{` |
|       - |  3498 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3499 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3500 | `	sxi32 rc;` |
|     129 |  3501 | `	iLevel = 0;` |
|       - |  3502 | `	/* Jump the 'break' keyword */` |
|     129 |  3503 | `	pGen->pIn++;` |
|     129 |  3504 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3505 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3506 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3507 | `		 */` |
|       - |  3508 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3509 | `		char *zAlloc = 0;` |
|       - |  3510 | `		SyString sNum;` |
|      17 |  3511 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3512 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3513 | `			return SXERR_ABORT;` |
|       - |  3514 | `		}` |
|      17 |  3515 | `		if( rc == SXRET_OK ){` |
|      21 |  3516 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3517 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      15 |  3518 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3519 | `				return SXERR_ABORT;` |
|       - |  3520 | `			}` |
|      15 |  3521 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      15 |  3522 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3523 | `		}` |
|      17 |  3524 | `		if( iLevel < 2 ){` |
|       3 |  3525 | `			iLevel = 0;` |
|       1 |  3526 | `		}` |
|      17 |  3527 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3528 | `	}` |
|       - |  3529 | `	/* Extract the target loop */` |
|     129 |  3530 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     129 |  3531 | `	if( pLoop == 0 ){` |
|       - |  3532 | `		/* Illegal break */` |
|      19 |  3533 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      19 |  3534 | `		if( rc == SXERR_ABORT ){` |
|       - |  3535 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3536 | `			return SXERR_ABORT;` |
|       - |  3537 | `		}` |
|      11 |  3538 | `	}else{` |
|       - |  3539 | `		sxu32 nInstrIdx;` |
|       - |  3540 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     113 |  3541 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|       - |  3542 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     113 |  3543 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     113 |  3544 | `		if( rc == SXRET_OK ){` |
|       - |  3545 | `			/* Fix the jump later when the jump destination is resolved */` |
|     113 |  3546 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      54 |  3547 | `		}` |
|       - |  3548 | `	}` |
|     129 |  3549 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3550 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3551 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3552 | `	}` |
|       - |  3553 | `	/* Statement successfully compiled */` |
|     129 |  3554 | `	return SXRET_OK;` |
|      67 |  3555 | `}` |
|       - |  3556 | `/*` |
|       - |  3557 | ` * Compile or record a label.` |
|       - |  3558 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3559 | ` * Example` |
|       - |  3560 | ` *  goto LABEL;` |
|       - |  3561 | ` *   echo 'Foo';` |
|       - |  3562 | ` *  LABEL:` |
|       - |  3563 | ` *   echo 'Bar';` |
|       - |  3564 | ` */` |
|     112 |  3565 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  3566 | `{` |
|       - |  3567 | `	GenBlock *pBlock;` |
|       - |  3568 | `	Label sLabel;` |
|       - |  3569 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     117 |  3570 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     117 |  3571 | `	if( pBlock ){` |
|       - |  3572 | `		sxi32 rc;` |
|       8 |  3573 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3574 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       6 |  3575 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3576 | `			return SXERR_ABORT;` |
|       - |  3577 | `		}` |
|       4 |  3578 | `	}else{` |
|     113 |  3579 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3580 | `		char *zDup;` |
|       - |  3581 | `		/* Initialize label fields */` |
|     113 |  3582 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3583 | `		/* Duplicate label name */` |
|     113 |  3584 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     113 |  3585 | `		if( zDup == 0 ){` |
|     ! 0 |  3586 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3587 | `			return SXERR_ABORT;` |
|       - |  3588 | `		}` |
|     113 |  3589 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     113 |  3590 | `		sLabel.bRef  = FALSE;` |
|     113 |  3591 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     113 |  3592 | `		pBlock = pGen->pCurrent;` |
|     221 |  3593 | `		while( pBlock ){` |
|     133 |  3594 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      24 |  3595 | `				break;` |
|       - |  3596 | `			}` |
|       - |  3597 | `			/* Point to the upper block */` |
|     113 |  3598 | `			pBlock = pBlock->pParent;` |
|       5 |  3599 | `		}` |
|     113 |  3600 | `		if( pBlock ){` |
|      24 |  3601 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      14 |  3602 | `		}else{` |
|      93 |  3603 | `			sLabel.pFunc = 0;` |
|       - |  3604 | `		}` |
|       - |  3605 | `		/* Insert in label set */` |
|     113 |  3606 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3607 | `	}` |
|     117 |  3608 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     117 |  3609 | `	return SXRET_OK;` |
|      61 |  3610 | `}` |
|       - |  3611 | `/*` |
|       - |  3612 | ` * Compile the so hated 'goto' statement.` |
|       - |  3613 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3614 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3615 | ` * a compiler it has to do this.` |
|       - |  3616 | ` * According to the PHP language reference manual` |
|       - |  3617 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3618 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3619 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3620 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3621 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3622 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3623 | ` *   of a multi-level break` |
|       - |  3624 | ` */` |
|     152 |  3625 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  3626 | `{` |
|       - |  3627 | `	JumpFixup sJump;` |
|       - |  3628 | `	sxi32 rc;` |
|     157 |  3629 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     157 |  3630 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3631 | `		/* Missing label */` |
|     ! 0 |  3632 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3633 | `		if( rc == SXERR_ABORT ){` |
|       - |  3634 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3635 | `			return SXERR_ABORT;` |
|       - |  3636 | `		}` |
|     ! 0 |  3637 | `		return SXRET_OK;` |
|       - |  3638 | `	}` |
|     157 |  3639 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       6 |  3640 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       6 |  3641 | `		if( rc == SXERR_ABORT ){` |
|       - |  3642 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3643 | `			return SXERR_ABORT;` |
|       - |  3644 | `		}` |
|       4 |  3645 | `	}else{` |
|     153 |  3646 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3647 | `		GenBlock *pBlock;` |
|       - |  3648 | `		char *zDup;` |
|       - |  3649 | `		/* Prepare the jump destination */` |
|     153 |  3650 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     153 |  3651 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3652 | `		/* Duplicate label name */` |
|     153 |  3653 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     153 |  3654 | `		if( zDup == 0 ){` |
|     ! 0 |  3655 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3656 | `			return SXERR_ABORT;` |
|       - |  3657 | `		}` |
|     153 |  3658 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     153 |  3659 | `		pBlock = pGen->pCurrent;` |
|     315 |  3660 | `		while( pBlock ){` |
|     199 |  3661 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      37 |  3662 | `				break;` |
|       - |  3663 | `			}` |
|       - |  3664 | `			/* Point to the upper block */` |
|     167 |  3665 | `			pBlock = pBlock->pParent;` |
|       5 |  3666 | `		}` |
|     153 |  3667 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       9 |  3668 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       9 |  3669 | `			if( rc == SXERR_ABORT ){` |
|       - |  3670 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3671 | `				return SXERR_ABORT;` |
|       - |  3672 | `			}` |
|       3 |  3673 | `		}` |
|     153 |  3674 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      30 |  3675 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      17 |  3676 | `		}else{` |
|     127 |  3677 | `			sJump.pFunc = 0;` |
|       - |  3678 | `		}` |
|       - |  3679 | `		/* Emit the unconditional jump */` |
|     153 |  3680 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     153 |  3681 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3682 | `		}` |
|       - |  3683 | `	}` |
|     157 |  3684 | `	pGen->pIn++; /* Jump the label name */` |
|     157 |  3685 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3686 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3687 | `	}` |
|       - |  3688 | `	/* Statement successfully compiled */` |
|     157 |  3689 | `	return SXRET_OK;` |
|      81 |  3690 | `}` |
|       - |  3691 | `/*` |
|       - |  3692 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3693 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3694 | ` * failure.` |
|       - |  3695 | ` */` |
|      20 |  3696 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       2 |  3697 | `{` |
|       - |  3698 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3699 | `	sxu32 nRawObj;` |
|      10 |  3700 | `	sxu32 nObjIdx;` |
|       - |  3701 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3702 | `	 * a PHP block.` |
|       - |  3703 | `	 */` |
|      10 |  3704 | `Consume:` |
|      22 |  3705 | `	nRawObj = nObjIdx = 0;` |
|      22 |  3706 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3707 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3708 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3709 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3710 | `			return SXERR_ABORT;` |
|       - |  3711 | `		}` |
|       - |  3712 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3713 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3714 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3715 | `		++nRawObj;` |
|     ! 0 |  3716 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3717 | `	}` |
|      22 |  3718 | `	if( nRawObj > 0 ){` |
|       - |  3719 | `		/* Emit the consume instruction */` |
|     ! 0 |  3720 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3721 | `	}` |
|      22 |  3722 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3723 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3724 | `		/* Reset the token set */` |
|     ! 0 |  3725 | `		SySetReset(pTokenSet);` |
|       - |  3726 | `		/* Tokenize input */` |
|     ! 0 |  3727 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3728 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3729 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3730 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3731 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3732 | `		/* Advance the stream cursor */` |
|     ! 0 |  3733 | `		pGen->pRawIn++;` |
|       - |  3734 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3735 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3736 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3737 | `			sxi32 rc;` |
|       - |  3738 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3739 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3740 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3741 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3742 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3743 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3744 | `				return SXERR_ABORT;` |
|     ! 0 |  3745 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3746 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3747 | `			}` |
|     ! 0 |  3748 | `			goto Consume;` |
|       - |  3749 | `		}` |
|     ! 0 |  3750 | `	}else{` |
|       - |  3751 | `		/* No more chunks to process */` |
|      22 |  3752 | `		pGen->pIn = pGen->pEnd;` |
|      22 |  3753 | `		return SXERR_EOF;` |
|       - |  3754 | `	}` |
|     ! 0 |  3755 | `	return SXRET_OK;` |
|      12 |  3756 | `}` |
|       - |  3757 | `/*` |
|       - |  3758 | ` * Compile a PHP block.` |
|       - |  3759 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3760 | ` * optionally delimited by braces {}.` |
|       - |  3761 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3762 | ` * and this function takes care of generating the appropriate error` |
|       - |  3763 | ` * message.` |
|       - |  3764 | ` */` |
|  468876 |  3765 | `static sxi32 PH7_CompileBlock(` |
|       - |  3766 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3767 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3768 | `	)` |
|       5 |  3769 | `{` |
|       - |  3770 | `	sxi32 rc;` |
|       - |  3771 | `	sxu32 nLine;` |
|  468881 |  3772 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  467407 |  3773 | `		nLine = pGen->pIn->nLine;` |
|  467407 |  3774 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  467407 |  3775 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3776 | `			return SXERR_ABORT;` |
|       - |  3777 | `		}` |
|  467407 |  3778 | `		pGen->pIn++;` |
|       - |  3779 | `		/* Compile until we hit the closing braces '}' */` |
|  638757 |  3780 | `		for(;;){` |
| 1277519 |  3781 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      22 |  3782 | `				rc = GenStateNextChunk(&(*pGen));` |
|      22 |  3783 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3784 | `			 	   return SXERR_ABORT;` |
|       - |  3785 | `				}` |
|      22 |  3786 | `				if( rc == SXERR_EOF ){` |
|       - |  3787 | `					/* No more token to process. Missing closing braces */` |
|      22 |  3788 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      22 |  3789 | `					break;` |
|       - |  3790 | `				}` |
|     ! 0 |  3791 | `			}` |
| 1277499 |  3792 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3793 | `				/* Closing braces found,break immediately*/` |
|  467387 |  3794 | `				pGen->pIn++;` |
|  467387 |  3795 | `				break;` |
|       - |  3796 | `			}` |
|       - |  3797 | `			/* Compile a single statement */` |
|  810117 |  3798 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  810117 |  3799 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3800 | `				return SXERR_ABORT;` |
|       - |  3801 | `			}` |
|       5 |  3802 | `		}` |
|  467407 |  3803 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  235180 |  3804 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3805 | `		pGen->pIn++;` |
|     ! 0 |  3806 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3807 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3808 | `			return SXERR_ABORT;` |
|       - |  3809 | `		}` |
|       - |  3810 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3811 | `		for(;;){` |
|     ! 0 |  3812 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3813 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3814 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3815 | `			 	   return SXERR_ABORT;` |
|       - |  3816 | `				}` |
|     ! 0 |  3817 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3818 | `					/* No more token to process */` |
|     ! 0 |  3819 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3820 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3821 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3822 | `					}` |
|     ! 0 |  3823 | `					break;` |
|       - |  3824 | `				}` |
|     ! 0 |  3825 | `			}` |
|     ! 0 |  3826 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3827 | `				sxi32 nKwrd;` |
|       - |  3828 | `				/* Keyword found */` |
|     ! 0 |  3829 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3830 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3831 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3832 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3833 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3834 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3835 | `						}` |
|     ! 0 |  3836 | `						break;` |
|       - |  3837 | `				}` |
|     ! 0 |  3838 | `			}` |
|       - |  3839 | `			/* Compile a single statement */` |
|     ! 0 |  3840 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3841 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3842 | `				return SXERR_ABORT;` |
|       - |  3843 | `			}` |
|     ! 0 |  3844 | `		}` |
|     ! 0 |  3845 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3846 | `	}else{` |
|       - |  3847 | `		/* Compile a single statement */` |
|    1479 |  3848 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1479 |  3849 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3850 | `			return SXERR_ABORT;` |
|       - |  3851 | `		}` |
|       - |  3852 | `	}` |
|       - |  3853 | `	/* Jump trailing semi-colons ';' */` |
|  468881 |  3854 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3855 | `		pGen->pIn++;` |
|     ! 0 |  3856 | `	}` |
|  468881 |  3857 | `	return SXRET_OK;` |
|  234443 |  3858 | `}` |
|       - |  3859 | `/*` |
|       - |  3860 | ` * Compile the gentle 'while' statement.` |
|       - |  3861 | ` * According to the PHP language reference` |
|       - |  3862 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3863 | ` *  The basic form of a while statement is:` |
|       - |  3864 | ` *  while (expr)` |
|       - |  3865 | ` *   statement` |
|       - |  3866 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3867 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3868 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3869 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3870 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3871 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3872 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3873 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3874 | ` *  while (expr):` |
|       - |  3875 | ` *    statement` |
|       - |  3876 | ` *   endwhile;` |
|       - |  3877 | ` */` |
|   15452 |  3878 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3879 | `{` |
|   15457 |  3880 | `	GenBlock *pWhileBlock = 0;` |
|   15457 |  3881 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3882 | `	sxu32 nFalseJump;` |
|       - |  3883 | `	sxu32 nLine;` |
|       - |  3884 | `	sxi32 rc;` |
|   15457 |  3885 | `	nLine = pGen->pIn->nLine;` |
|       - |  3886 | `	/* Jump the 'while' keyword */` |
|   15457 |  3887 | `	pGen->pIn++;` |
|   15457 |  3888 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3889 | `		/* Syntax error */` |
|     ! 0 |  3890 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3891 | `		if( rc == SXERR_ABORT ){` |
|       - |  3892 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3893 | `			return SXERR_ABORT;` |
|       - |  3894 | `		}` |
|     ! 0 |  3895 | `		goto Synchronize;` |
|       - |  3896 | `	}` |
|       - |  3897 | `	/* Jump the left parenthesis '(' */` |
|   15457 |  3898 | `	pGen->pIn++;` |
|       - |  3899 | `	/* Create the loop block */` |
|   15457 |  3900 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   15457 |  3901 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3902 | `		return SXERR_ABORT;` |
|       - |  3903 | `	}` |
|       - |  3904 | `	/* Delimit the condition */` |
|   15457 |  3905 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   15457 |  3906 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3907 | `		/* Empty expression */` |
|       3 |  3908 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3909 | `		if( rc == SXERR_ABORT ){` |
|       - |  3910 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3911 | `			return SXERR_ABORT;` |
|       - |  3912 | `		}` |
|       1 |  3913 | `	}` |
|       - |  3914 | `	/* Swap token streams */` |
|   15457 |  3915 | `	pTmp = pGen->pEnd;` |
|   15457 |  3916 | `	pGen->pEnd = pEnd;` |
|       - |  3917 | `	/* Compile the expression */` |
|   15457 |  3918 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   15457 |  3919 | `	if( rc == SXERR_ABORT ){` |
|       - |  3920 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3921 | `		return SXERR_ABORT;` |
|       - |  3922 | `	}` |
|       - |  3923 | `	/* Update token stream */` |
|   15457 |  3924 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3925 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3926 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3927 | `			return SXERR_ABORT;` |
|       - |  3928 | `		}` |
|     ! 0 |  3929 | `		pGen->pIn++;` |
|     ! 0 |  3930 | `	}` |
|       - |  3931 | `	/* Synchronize pointers */` |
|   15457 |  3932 | `	pGen->pIn  = &pEnd[1];` |
|   15457 |  3933 | `	pGen->pEnd = pTmp;` |
|       - |  3934 | `	/* Emit the false jump */` |
|   15457 |  3935 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3936 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   15457 |  3937 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3938 | `	/* Compile the loop body */` |
|   15457 |  3939 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   15457 |  3940 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3941 | `		return SXERR_ABORT;` |
|       - |  3942 | `	}` |
|       - |  3943 | `	/* Emit the unconditional jump to the start of the loop */` |
|   15457 |  3944 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3945 | `	/* Fix all jumps now the destination is resolved */` |
|   15457 |  3946 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3947 | `	/* Release the loop block */` |
|   15457 |  3948 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3949 | `	/* Statement successfully compiled */` |
|   15457 |  3950 | `	return SXRET_OK;` |
|     ! 0 |  3951 | `Synchronize:` |
|       - |  3952 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3953 | `	 * compiling this erroneous block.` |
|       - |  3954 | `	 */` |
|     ! 0 |  3955 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3956 | `		pGen->pIn++;` |
|     ! 0 |  3957 | `	}` |
|     ! 0 |  3958 | `	return SXRET_OK;` |
|    7731 |  3959 | `}` |
|       - |  3960 | `/*` |
|       - |  3961 | ` * Compile the ugly do..while() statement.` |
|       - |  3962 | ` * According to the PHP language reference` |
|       - |  3963 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3964 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3965 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3966 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3967 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3968 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3969 | ` *  would end immediately).` |
|       - |  3970 | ` *  There is just one syntax for do-while loops:` |
|       - |  3971 | ` *  <?php` |
|       - |  3972 | ` *  $i = 0;` |
|       - |  3973 | ` *  do {` |
|       - |  3974 | ` *   echo $i;` |
|       - |  3975 | ` *  } while ($i > 0);` |
|       - |  3976 | ` * ?>` |
|       - |  3977 | ` */` |
|       2 |  3978 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3979 | `{` |
|       3 |  3980 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3981 | `	GenBlock *pDoBlock = 0;` |
|       - |  3982 | `	sxu32 nLine;` |
|       - |  3983 | `	sxi32 rc;` |
|       3 |  3984 | `	nLine = pGen->pIn->nLine;` |
|       - |  3985 | `	/* Jump the 'do' keyword */` |
|       3 |  3986 | `	pGen->pIn++;` |
|       - |  3987 | `	/* Create the loop block */` |
|       3 |  3988 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3989 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3990 | `		return SXERR_ABORT;` |
|       - |  3991 | `	}` |
|       - |  3992 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3993 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3994 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3995 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3996 | `		return SXERR_ABORT;` |
|       - |  3997 | `	}` |
|       3 |  3998 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3999 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  4000 | `	}` |
|       3 |  4001 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  4002 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  4003 | `			/* Missing 'while' statement */` |
|       3 |  4004 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  4005 | `			if( rc == SXERR_ABORT ){` |
|       - |  4006 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4007 | `				return SXERR_ABORT;` |
|       - |  4008 | `			}` |
|       3 |  4009 | `			goto Synchronize;` |
|       - |  4010 | `	}` |
|       - |  4011 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  4012 | `	pGen->pIn++;` |
|     ! 0 |  4013 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4014 | `		/* Syntax error */` |
|     ! 0 |  4015 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  4016 | `		if( rc == SXERR_ABORT ){` |
|       - |  4017 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4018 | `			return SXERR_ABORT;` |
|       - |  4019 | `		}` |
|     ! 0 |  4020 | `		goto Synchronize;` |
|       - |  4021 | `	}` |
|       - |  4022 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  4023 | `	pGen->pIn++;` |
|       - |  4024 | `	/* Delimit the condition */` |
|     ! 0 |  4025 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  4026 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4027 | `		/* Empty expression */` |
|     ! 0 |  4028 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  4029 | `		if( rc == SXERR_ABORT ){` |
|       - |  4030 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4031 | `			return SXERR_ABORT;` |
|       - |  4032 | `		}` |
|     ! 0 |  4033 | `		goto Synchronize;` |
|       - |  4034 | `	}` |
|       - |  4035 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  4036 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  4037 | `		JumpFixup *aPost;` |
|       - |  4038 | `		VmInstr *pInstr;` |
|       - |  4039 | `		sxu32 nJumpDest;` |
|       - |  4040 | `		sxu32 n;` |
|     ! 0 |  4041 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  4042 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  4043 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  4044 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  4045 | `			if( pInstr ){` |
|       - |  4046 | `				/* Fix */` |
|     ! 0 |  4047 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  4048 | `			}` |
|     ! 0 |  4049 | `		}` |
|     ! 0 |  4050 | `	}` |
|       - |  4051 | `	/* Swap token streams */` |
|     ! 0 |  4052 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  4053 | `	pGen->pEnd = pEnd;` |
|       - |  4054 | `	/* Compile the expression */` |
|     ! 0 |  4055 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4056 | `	if( rc == SXERR_ABORT ){` |
|       - |  4057 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4058 | `		return SXERR_ABORT;` |
|       - |  4059 | `	}` |
|       - |  4060 | `	/* Update token stream */` |
|     ! 0 |  4061 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  4062 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4063 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4064 | `			return SXERR_ABORT;` |
|       - |  4065 | `		}` |
|     ! 0 |  4066 | `		pGen->pIn++;` |
|     ! 0 |  4067 | `	}` |
|     ! 0 |  4068 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  4069 | `	pGen->pEnd = pTmp;` |
|       - |  4070 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  4071 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  4072 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  4073 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4074 | `	/* Release the loop block */` |
|     ! 0 |  4075 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4076 | `	/* Statement successfully compiled */` |
|     ! 0 |  4077 | `	return SXRET_OK;` |
|       1 |  4078 | `Synchronize:` |
|       - |  4079 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4080 | `	 * compiling this erroneous block.` |
|       - |  4081 | `	 */` |
|       3 |  4082 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4083 | `		pGen->pIn++;` |
|     ! 0 |  4084 | `	}` |
|       3 |  4085 | `	return SXRET_OK;` |
|       2 |  4086 | `}` |
|       - |  4087 | `/*` |
|       - |  4088 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  4089 | ` * According to the PHP language reference` |
|       - |  4090 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  4091 | ` *  The syntax of a for loop is:` |
|       - |  4092 | ` *  for (expr1; expr2; expr3)` |
|       - |  4093 | ` *   statement` |
|       - |  4094 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  4095 | ` *  the beginning of the loop.` |
|       - |  4096 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  4097 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  4098 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  4099 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  4100 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  4101 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  4102 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  4103 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  4104 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  4105 | ` *  of using the for truth expression.` |
|       - |  4106 | ` */` |
|   15452 |  4107 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  4108 | `{` |
|   15457 |  4109 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   15457 |  4110 | `	GenBlock *pForBlock = 0;` |
|       - |  4111 | `	sxu32 nFalseJump;` |
|       - |  4112 | `	sxu32 nLine;` |
|       - |  4113 | `	sxi32 rc;` |
|   15457 |  4114 | `	nLine = pGen->pIn->nLine;` |
|       - |  4115 | `	/* Jump the 'for' keyword */` |
|   15457 |  4116 | `	pGen->pIn++;` |
|   15457 |  4117 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4118 | `		/* Syntax error */` |
|     ! 0 |  4119 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  4120 | `		if( rc == SXERR_ABORT ){` |
|       - |  4121 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4122 | `			return SXERR_ABORT;` |
|       - |  4123 | `		}` |
|     ! 0 |  4124 | `		return SXRET_OK;` |
|       - |  4125 | `	}` |
|       - |  4126 | `	/* Jump the left parenthesis '(' */` |
|   15457 |  4127 | `	pGen->pIn++;` |
|       - |  4128 | `	/* Delimit the init-expr;condition;post-expr */` |
|   15457 |  4129 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   15457 |  4130 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4131 | `		/* Empty expression */` |
|     ! 0 |  4132 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  4133 | `		if( rc == SXERR_ABORT ){` |
|       - |  4134 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4135 | `			return SXERR_ABORT;` |
|       - |  4136 | `		}` |
|       - |  4137 | `		/* Synchronize */` |
|     ! 0 |  4138 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4139 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4140 | `			pGen->pIn++;` |
|     ! 0 |  4141 | `		}` |
|     ! 0 |  4142 | `		return SXRET_OK;` |
|       - |  4143 | `	}` |
|       - |  4144 | `	/* Swap token streams */` |
|   15457 |  4145 | `	pTmp = pGen->pEnd;` |
|   15457 |  4146 | `	pGen->pEnd = pEnd;` |
|       - |  4147 | `	/* Compile initialization expressions if available */` |
|   15457 |  4148 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4149 | `	/* Pop operand lvalues */` |
|   15457 |  4150 | `	if( rc == SXERR_ABORT ){` |
|       - |  4151 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4152 | `		return SXERR_ABORT;` |
|   15457 |  4153 | `	}else if( rc != SXERR_EMPTY ){` |
|   15455 |  4154 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7725 |  4155 | `	}` |
|   15457 |  4156 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4157 | `		/* Syntax error */` |
|     ! 0 |  4158 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4159 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  4160 | `		if( rc == SXERR_ABORT ){` |
|       - |  4161 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4162 | `			return SXERR_ABORT;` |
|       - |  4163 | `		}` |
|     ! 0 |  4164 | `		return SXRET_OK;` |
|       - |  4165 | `	}` |
|       - |  4166 | `	/* Jump the trailing ';' */` |
|   15457 |  4167 | `	pGen->pIn++;` |
|       - |  4168 | `	/* Create the loop block */` |
|   15457 |  4169 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   15457 |  4170 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4171 | `		return SXERR_ABORT;` |
|       - |  4172 | `	}` |
|       - |  4173 | `	/* Deffer continue jumps */` |
|   15457 |  4174 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  4175 | `	/* Compile the condition */` |
|   15457 |  4176 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   15457 |  4177 | `	if( rc == SXERR_ABORT ){` |
|       - |  4178 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4179 | `		return SXERR_ABORT;` |
|   15457 |  4180 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  4181 | `		/* Emit the false jump */` |
|   15455 |  4182 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4183 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   15455 |  4184 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    7725 |  4185 | `	}` |
|   15457 |  4186 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4187 | `		/* Syntax error */` |
|       6 |  4188 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4189 | `			"for: Expected ';' after conditionals expressions");` |
|       6 |  4190 | `		if( rc == SXERR_ABORT ){` |
|       - |  4191 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4192 | `			return SXERR_ABORT;` |
|       - |  4193 | `		}` |
|       6 |  4194 | `		return SXRET_OK;` |
|       - |  4195 | `	}` |
|       - |  4196 | `	/* Jump the trailing ';' */` |
|   15453 |  4197 | `	pGen->pIn++;` |
|       - |  4198 | `	/* Save the post condition stream */` |
|   15453 |  4199 | `	pPostStart = pGen->pIn;` |
|       - |  4200 | `	/* Compile the loop body */` |
|   15453 |  4201 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   15453 |  4202 | `	pGen->pEnd = pTmp;` |
|   15453 |  4203 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   15453 |  4204 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4205 | `		return SXERR_ABORT;` |
|       - |  4206 | `	}` |
|       - |  4207 | `	/* Fix post-continue jumps */` |
|   15453 |  4208 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  4209 | `		JumpFixup *aPost;` |
|       - |  4210 | `		VmInstr *pInstr;` |
|       - |  4211 | `		sxu32 nJumpDest;` |
|       - |  4212 | `		sxu32 n;` |
|      14 |  4213 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  4214 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  4215 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  4216 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  4217 | `			if( pInstr ){` |
|       - |  4218 | `				/* Fix jump */` |
|      14 |  4219 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  4220 | `			}` |
|       8 |  4221 | `		}` |
|       6 |  4222 | `	}` |
|       - |  4223 | `	/* compile the post-expressions if available */` |
|   15453 |  4224 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4225 | `		pPostStart++;` |
|     ! 0 |  4226 | `	}` |
|   15453 |  4227 | `	if( pPostStart < pEnd ){` |
|       - |  4228 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   15453 |  4229 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   15453 |  4230 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   15453 |  4231 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4232 | `			/* Syntax error */` |
|     ! 0 |  4233 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4234 | `			if( rc == SXERR_ABORT ){` |
|       - |  4235 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4236 | `				return SXERR_ABORT;` |
|       - |  4237 | `			}` |
|     ! 0 |  4238 | `			return SXRET_OK;` |
|       - |  4239 | `		}` |
|   15453 |  4240 | `		RE_SWAP_DELIMITER(pGen);` |
|   15453 |  4241 | `		if( rc == SXERR_ABORT ){` |
|       - |  4242 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4243 | `			return SXERR_ABORT;` |
|   15453 |  4244 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4245 | `			/* Pop operand lvalue */` |
|   15453 |  4246 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7724 |  4247 | `		}` |
|    7724 |  4248 | `	}` |
|       - |  4249 | `	/* Emit the unconditional jump to the start of the loop */` |
|   15453 |  4250 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4251 | `	/* Fix all jumps now the destination is resolved */` |
|   15453 |  4252 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4253 | `	/* Release the loop block */` |
|   15453 |  4254 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4255 | `	/* Statement successfully compiled */` |
|   15453 |  4256 | `	return SXRET_OK;` |
|    7731 |  4257 | `}` |
|       - |  4258 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4259 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4260 | ` * are allowed.` |
|       - |  4261 | ` */` |
|    8290 |  4262 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4263 | `{` |
|    8295 |  4264 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    8295 |  4265 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4266 | `		/* Unexpected expression */` |
|     ! 0 |  4267 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4268 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4269 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4270 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4271 | `		}` |
|     ! 0 |  4272 | `	}` |
|    8295 |  4273 | `	return rc;` |
|       5 |  4274 | `}` |
|       - |  4275 | `/*` |
|       - |  4276 | ` * Compile the 'foreach' statement.` |
|       - |  4277 | ` * According to the PHP language reference` |
|       - |  4278 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4279 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4280 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4281 | ` *  is a minor but useful extension of the first:` |
|       - |  4282 | ` *  foreach (array_expression as $value)` |
|       - |  4283 | ` *    statement` |
|       - |  4284 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4285 | ` *   statement` |
|       - |  4286 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4287 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4288 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4289 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4290 | ` *  to the variable $key on each loop.` |
|       - |  4291 | ` *  Note:` |
|       - |  4292 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4293 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4294 | ` *  Note:` |
|       - |  4295 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4296 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4297 | ` *  or after the foreach without resetting it.` |
|       - |  4298 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4299 | ` *  of copying the value.` |
|       - |  4300 | ` */` |
|    4268 |  4301 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4302 | `{` |
|    4273 |  4303 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    4273 |  4304 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    4273 |  4305 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4306 | `	ph7_foreach_info *pInfo;` |
|       - |  4307 | `	sxu32 nFalseJump;` |
|       - |  4308 | `	VmInstr *pInstr;` |
|       - |  4309 | `	sxu32 nLine;` |
|       - |  4310 | `	sxi32 rc;` |
|    4273 |  4311 | `	nLine = pGen->pIn->nLine;` |
|       - |  4312 | `	/* Jump the 'foreach' keyword */` |
|    4273 |  4313 | `	pGen->pIn++;` |
|    4273 |  4314 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4315 | `		/* Syntax error */` |
|     ! 0 |  4316 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4317 | `		if( rc == SXERR_ABORT ){` |
|       - |  4318 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4319 | `			return SXERR_ABORT;` |
|       - |  4320 | `		}` |
|     ! 0 |  4321 | `		goto Synchronize;` |
|       - |  4322 | `	}` |
|       - |  4323 | `	/* Jump the left parenthesis '(' */` |
|    4273 |  4324 | `	pGen->pIn++;` |
|       - |  4325 | `	/* Create the loop block */` |
|    4273 |  4326 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    4273 |  4327 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4328 | `		return SXERR_ABORT;` |
|       - |  4329 | `	}` |
|       - |  4330 | `	/* Delimit the expression */` |
|    4273 |  4331 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    4273 |  4332 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4333 | `		/* Empty expression */` |
|     ! 0 |  4334 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4335 | `		if( rc == SXERR_ABORT ){` |
|       - |  4336 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4337 | `			return SXERR_ABORT;` |
|       - |  4338 | `		}` |
|       - |  4339 | `		/* Synchronize */` |
|     ! 0 |  4340 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4341 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4342 | `			pGen->pIn++;` |
|     ! 0 |  4343 | `		}` |
|     ! 0 |  4344 | `		return SXRET_OK;` |
|       - |  4345 | `	}` |
|       - |  4346 | `	/* Compile the array expression */` |
|    4273 |  4347 | `	pCur = pGen->pIn;` |
|   29305 |  4348 | `	while( pCur < pEnd ){` |
|   29305 |  4349 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    4287 |  4350 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    4287 |  4351 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4352 | `				/* Break with the first 'as' found */` |
|    4273 |  4353 | `				break;` |
|       - |  4354 | `			}` |
|       7 |  4355 | `		}` |
|       - |  4356 | `		/* Advance the stream cursor */` |
|   25037 |  4357 | `		pCur++;` |
|       5 |  4358 | `	}` |
|    4273 |  4359 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4360 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4361 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4362 | `		if( rc == SXERR_ABORT ){` |
|       - |  4363 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4364 | `			return SXERR_ABORT;` |
|       - |  4365 | `		}` |
|     ! 0 |  4366 | `		goto Synchronize;` |
|       - |  4367 | `	}` |
|       - |  4368 | `	/* Swap token streams */` |
|    4273 |  4369 | `	pTmp = pGen->pEnd;` |
|    4273 |  4370 | `	pGen->pEnd = pCur;` |
|    4273 |  4371 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    4273 |  4372 | `	if( rc == SXERR_ABORT ){` |
|       - |  4373 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4374 | `		return SXERR_ABORT;` |
|       - |  4375 | `	}` |
|       - |  4376 | `	/* Update token stream */` |
|    4273 |  4377 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4378 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4379 | `		if( rc == SXERR_ABORT ){` |
|       - |  4380 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4381 | `			return SXERR_ABORT;` |
|       - |  4382 | `		}` |
|     ! 0 |  4383 | `		pGen->pIn++;` |
|     ! 0 |  4384 | `	}` |
|    4273 |  4385 | `	pCur++; /* Jump the 'as' keyword */` |
|    4273 |  4386 | `	pGen->pIn = pCur;` |
|    4273 |  4387 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4388 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4389 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4390 | `			return SXERR_ABORT;` |
|       - |  4391 | `		}` |
|     ! 0 |  4392 | `	}` |
|       - |  4393 | `	/* Create the foreach context */` |
|    4273 |  4394 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    4273 |  4395 | `	if( pInfo == 0 ){` |
|     ! 0 |  4396 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4397 | `		return SXERR_ABORT;` |
|       - |  4398 | `	}` |
|       - |  4399 | `	/* Zero the structure */` |
|    4273 |  4400 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4401 | `	/* Initialize structure fields */` |
|    4273 |  4402 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4403 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - |  4404 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - |  4405 | `	 * '=>'. */` |
|    4273 |  4406 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    4273 |  4407 | `	if( pCur < pEnd ){` |
|       - |  4408 | `		/* Compile the expression holding the key name */` |
|    4047 |  4409 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4410 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4411 | `			if( rc == SXERR_ABORT ){` |
|       - |  4412 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4413 | `				return SXERR_ABORT;` |
|       - |  4414 | `			}` |
|     ! 0 |  4415 | `		}else{` |
|    4047 |  4416 | `			pGen->pEnd = pCur;` |
|    4047 |  4417 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    4047 |  4418 | `			if( rc == SXERR_ABORT ){` |
|       - |  4419 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4420 | `				return SXERR_ABORT;` |
|       - |  4421 | `			}` |
|    4047 |  4422 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    4047 |  4423 | `			if( pInstr->p3 ){` |
|       - |  4424 | `				/* Record key name */` |
|    4047 |  4425 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    2021 |  4426 | `			}` |
|    4047 |  4427 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4428 | `		}` |
|    4047 |  4429 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    2021 |  4430 | `	}` |
|    4273 |  4431 | `	pGen->pEnd = pEnd;` |
|    4273 |  4432 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4433 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4434 | `		if( rc == SXERR_ABORT ){` |
|       - |  4435 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4436 | `			return SXERR_ABORT;` |
|       - |  4437 | `		}` |
|     ! 0 |  4438 | `		goto Synchronize;` |
|       - |  4439 | `	}` |
|    4273 |  4440 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4441 | `		pGen->pIn++;` |
|       - |  4442 | `		/* Pass by reference  */` |
|      11 |  4443 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4444 | `	}` |
|       - |  4445 | `	/* Check if the value target is list() */` |
|    4273 |  4446 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4447 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4448 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4449 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4450 | `		 */` |
|       - |  4451 | `		static int iForeachListCnt = 0;` |
|       - |  4452 | `		char zTmp[128];` |
|       - |  4453 | `		sxu32 nLen;` |
|       - |  4454 | `		char *zDup;` |
|      10 |  4455 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4456 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4457 | `		if( zDup == 0 ){` |
|     ! 0 |  4458 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4459 | `			return SXERR_ABORT;` |
|       - |  4460 | `		}` |
|      10 |  4461 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4462 | `		/* Save list() token boundaries */` |
|      10 |  4463 | `		pListStart = pGen->pIn;` |
|       - |  4464 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4465 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4466 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4467 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4468 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4469 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4470 | `				return SXERR_ABORT;` |
|       - |  4471 | `			}` |
|       3 |  4472 | `			goto Synchronize;` |
|       - |  4473 | `		}` |
|       7 |  4474 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4475 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4476 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4477 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4478 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4479 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4480 | `				return SXERR_ABORT;` |
|       - |  4481 | `			}` |
|     ! 0 |  4482 | `			goto Synchronize;` |
|       - |  4483 | `		}` |
|       7 |  4484 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4485 | `		pListEnd = pGen->pIn;` |
|       7 |  4486 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    4268 |  4487 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4488 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4489 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4490 | `		 */` |
|       - |  4491 | `		static int iForeachShortListCnt = 0;` |
|       - |  4492 | `		char zTmp[128];` |
|       - |  4493 | `		sxu32 nLen;` |
|       - |  4494 | `		char *zDup;` |
|      13 |  4495 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|      13 |  4496 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      13 |  4497 | `		if( zDup == 0 ){` |
|     ! 0 |  4498 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4499 | `			return SXERR_ABORT;` |
|       - |  4500 | `		}` |
|      13 |  4501 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4502 | `		/* Save [...] token boundaries */` |
|      13 |  4503 | `		pListStart = pGen->pIn;` |
|       - |  4504 | `		/* Advance past [...] */` |
|      13 |  4505 | `		pGen->pIn++; /* Jump '[' */` |
|      13 |  4506 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|      13 |  4507 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4508 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4509 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4510 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4511 | `				return SXERR_ABORT;` |
|       - |  4512 | `			}` |
|     ! 0 |  4513 | `			goto Synchronize;` |
|       - |  4514 | `		}` |
|      13 |  4515 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|      13 |  4516 | `		pListEnd = pGen->pIn;` |
|      13 |  4517 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       7 |  4518 | `	}else{` |
|       - |  4519 | `		/* Compile the expression holding the value name */` |
|    4253 |  4520 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    4253 |  4521 | `		if( rc == SXERR_ABORT ){` |
|       - |  4522 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4523 | `			return SXERR_ABORT;` |
|       - |  4524 | `		}` |
|    4253 |  4525 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    4253 |  4526 | `		if( pInstr->p3 ){` |
|       - |  4527 | `			/* Record value name */` |
|    4253 |  4528 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    2124 |  4529 | `		}` |
|       - |  4530 | `	}` |
|       - |  4531 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    4271 |  4532 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4533 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    4271 |  4534 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4535 | `	/* Record the first instruction to execute */` |
|    4271 |  4536 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4537 | `	/* Emit the FOREACH_STEP instruction */` |
|    4271 |  4538 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4539 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    4271 |  4540 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4541 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    4271 |  4542 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4543 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4544 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4545 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4546 | `		 */` |
|      19 |  4547 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4548 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4549 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4550 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4551 | `		 */` |
|      19 |  4552 | `		pSavedIn = pGen->pIn;` |
|      19 |  4553 | `		pSavedEnd = pGen->pEnd;` |
|      19 |  4554 | `		pGen->pIn = pListStart;` |
|      19 |  4555 | `		pGen->pEnd = pListEnd;` |
|      19 |  4556 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|      13 |  4557 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  4558 | `		}else{` |
|       7 |  4559 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4560 | `		}` |
|      19 |  4561 | `		pGen->pIn = pSavedIn;` |
|      19 |  4562 | `		pGen->pEnd = pSavedEnd;` |
|      19 |  4563 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4564 | `			return SXERR_ABORT;` |
|       - |  4565 | `		}` |
|       - |  4566 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      19 |  4567 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       9 |  4568 | `	}` |
|       - |  4569 | `	/* Compile the loop body */` |
|    4271 |  4570 | `	pGen->pIn = &pEnd[1];` |
|    4271 |  4571 | `	pGen->pEnd = pTmp;` |
|    4271 |  4572 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    4271 |  4573 | `	if( rc == SXERR_ABORT ){` |
|       - |  4574 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4575 | `		return SXERR_ABORT;` |
|       - |  4576 | `	}` |
|       - |  4577 | `	/* Emit the unconditional jump to the start of the loop */` |
|    4271 |  4578 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4579 | `	/* Fix all jumps now the destination is resolved */` |
|    4271 |  4580 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4581 | `	/* Release the loop block */` |
|    4271 |  4582 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4583 | `	/* Statement successfully compiled */` |
|    4271 |  4584 | `	return SXRET_OK;` |
|       1 |  4585 | `Synchronize:` |
|       - |  4586 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4587 | `	 * compiling this erroneous block.` |
|       - |  4588 | `	 */` |
|       3 |  4589 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4590 | `		pGen->pIn++;` |
|     ! 0 |  4591 | `	}` |
|       3 |  4592 | `	return SXRET_OK;` |
|    2139 |  4593 | `}` |
|       - |  4594 | `/*` |
|       - |  4595 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4596 | ` * According to the PHP language reference` |
|       - |  4597 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4598 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4599 | ` *  that is similar to that of C:` |
|       - |  4600 | ` *  if (expr)` |
|       - |  4601 | ` *   statement` |
|       - |  4602 | ` *  else construct:` |
|       - |  4603 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4604 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4605 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4606 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4607 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4608 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4609 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4610 | ` *  elseif` |
|       - |  4611 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4612 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4613 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4614 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4615 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4616 | ` *   <?php` |
|       - |  4617 | ` *    if ($a > $b) {` |
|       - |  4618 | ` *     echo "a is bigger than b";` |
|       - |  4619 | ` *    } elseif ($a == $b) {` |
|       - |  4620 | ` *     echo "a is equal to b";` |
|       - |  4621 | ` *    } else {` |
|       - |  4622 | ` *     echo "a is smaller than b";` |
|       - |  4623 | ` *    }` |
|       - |  4624 | ` *    ?>` |
|       - |  4625 | ` */` |
|  159672 |  4626 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4627 | `{` |
|  159677 |  4628 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  159677 |  4629 | `	GenBlock *pCondBlock = 0;` |
|       - |  4630 | `	sxu32 nJumpIdx;` |
|       - |  4631 | `	sxu32 nKeyID;` |
|       - |  4632 | `	sxi32 rc;` |
|       - |  4633 | `	/* Jump the 'if' keyword */` |
|  159677 |  4634 | `	pGen->pIn++;` |
|  159677 |  4635 | `	pToken = pGen->pIn;` |
|       - |  4636 | `	/* Create the conditional block */` |
|  159677 |  4637 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  159677 |  4638 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4639 | `		return SXERR_ABORT;` |
|       - |  4640 | `	}` |
|       - |  4641 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   87555 |  4642 | `	for(;;){` |
|  175115 |  4643 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4644 | `			/* Syntax error */` |
|     ! 0 |  4645 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4646 | `				pToken--;` |
|     ! 0 |  4647 | `			}` |
|     ! 0 |  4648 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4649 | `			if( rc == SXERR_ABORT ){` |
|       - |  4650 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4651 | `				return SXERR_ABORT;` |
|       - |  4652 | `			}` |
|     ! 0 |  4653 | `			goto Synchronize;` |
|       - |  4654 | `		}` |
|       - |  4655 | `		/* Jump the left parenthesis '(' */` |
|  175115 |  4656 | `		pToken++;` |
|       - |  4657 | `		/* Delimit the condition */` |
|  175115 |  4658 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  175115 |  4659 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4660 | `			/* Syntax error */` |
|     ! 0 |  4661 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4662 | `				pToken--;` |
|     ! 0 |  4663 | `			}` |
|     ! 0 |  4664 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4665 | `			if( rc == SXERR_ABORT ){` |
|       - |  4666 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4667 | `				return SXERR_ABORT;` |
|       - |  4668 | `			}` |
|     ! 0 |  4669 | `			goto Synchronize;` |
|       - |  4670 | `		}` |
|       - |  4671 | `		/* Swap token streams */` |
|  175115 |  4672 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4673 | `		/* Compile the condition */` |
|  175115 |  4674 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4675 | `		/* Update token stream */` |
|  175115 |  4676 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4677 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4678 | `			pGen->pIn++;` |
|     ! 0 |  4679 | `		}` |
|  175115 |  4680 | `		pGen->pIn  = &pEnd[1];` |
|  175115 |  4681 | `		pGen->pEnd = pTmp;` |
|  175115 |  4682 | `		if( rc == SXERR_ABORT ){` |
|       - |  4683 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4684 | `			return SXERR_ABORT;` |
|       - |  4685 | `		}` |
|       - |  4686 | `		/* Emit the false jump */` |
|  175115 |  4687 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4688 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  175115 |  4689 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4690 | `		/* Compile the body */` |
|  175115 |  4691 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  175115 |  4692 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4693 | `			return SXERR_ABORT;` |
|       - |  4694 | `		}` |
|  175115 |  4695 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   48786 |  4696 | `			break;` |
|       - |  4697 | `		}` |
|       - |  4698 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   77553 |  4699 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   77553 |  4700 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   50137 |  4701 | `			break;` |
|       - |  4702 | `		}` |
|       - |  4703 | `		/* Emit the unconditional jump */` |
|   27421 |  4704 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4705 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   27421 |  4706 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   27421 |  4707 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   19643 |  4708 | `			pToken = &pGen->pIn[1];` |
|   19643 |  4709 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    7702 |  4710 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5994 |  4711 | `					break;` |
|       - |  4712 | `			}` |
|    7665 |  4713 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3830 |  4714 | `		}` |
|   15443 |  4715 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4716 | `		/* Synchronize cursors */` |
|   15443 |  4717 | `		pToken = pGen->pIn;` |
|       - |  4718 | `		/* Fix the false jump */` |
|   15443 |  4719 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4720 | `	} /* For(;;) */` |
|       - |  4721 | `	/* Fix the false jump */` |
|  159677 |  4722 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  159677 |  4723 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   62110 |  4724 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4725 | `			/* Compile the else block */` |
|   11983 |  4726 | `			pGen->pIn++;` |
|   11983 |  4727 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   11983 |  4728 | `			if( rc == SXERR_ABORT ){` |
|       - |  4729 |  |
|     ! 0 |  4730 | `				return SXERR_ABORT;` |
|       - |  4731 | `			}` |
|    5989 |  4732 | `	}` |
|  159677 |  4733 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4734 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  159677 |  4735 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4736 | `	/* Release the conditional block */` |
|  159677 |  4737 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4738 | `	/* Statement successfully compiled */` |
|  159677 |  4739 | `	return SXRET_OK;` |
|     ! 0 |  4740 | `Synchronize:` |
|       - |  4741 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4742 | `	 */` |
|     ! 0 |  4743 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4744 | `		pGen->pIn++;` |
|     ! 0 |  4745 | `	}` |
|     ! 0 |  4746 | `	return SXRET_OK;` |
|   79841 |  4747 | `}` |
|       - |  4748 | `/*` |
|       - |  4749 | ` * Compile the global construct.` |
|       - |  4750 | ` * According to the PHP language reference` |
|       - |  4751 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4752 | ` *  to be used in that function.` |
|       - |  4753 | ` *  Example #1 Using global` |
|       - |  4754 | ` *  <?php` |
|       - |  4755 | ` *   $a = 1;` |
|       - |  4756 | ` *   $b = 2;` |
|       - |  4757 | ` *   function Sum()` |
|       - |  4758 | ` *   {` |
|       - |  4759 | ` *    global $a, $b;` |
|       - |  4760 | ` *    $b = $a + $b;` |
|       - |  4761 | ` *   }` |
|       - |  4762 | ` *   Sum();` |
|       - |  4763 | ` *   echo $b;` |
|       - |  4764 | ` *  ?>` |
|       - |  4765 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4766 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4767 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4768 | ` */` |
|      36 |  4769 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 |  4770 | `{` |
|      41 |  4771 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4772 | `	sxi32 nExpr;` |
|       - |  4773 | `	sxi32 rc;` |
|       - |  4774 | `	/* Jump the 'global' keyword */` |
|      41 |  4775 | `	pGen->pIn++;` |
|      41 |  4776 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4777 | `		/* Nothing to process */` |
|     ! 0 |  4778 | `		return SXRET_OK;` |
|       - |  4779 | `	}` |
|      41 |  4780 | `	pTmp = pGen->pEnd;` |
|      41 |  4781 | `	nExpr = 0;` |
|      87 |  4782 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      51 |  4783 | `		if( pGen->pIn < pNext ){` |
|      51 |  4784 | `			pGen->pEnd = pNext;` |
|      51 |  4785 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4786 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4787 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4788 | `					return SXERR_ABORT;` |
|       - |  4789 | `				}` |
|     ! 0 |  4790 | `			}else{` |
|      51 |  4791 | `				pGen->pIn++;` |
|      51 |  4792 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4793 | `					/* Emit a warning */` |
|     ! 0 |  4794 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4795 | `				}else{` |
|      51 |  4796 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 |  4797 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4798 | `						return SXERR_ABORT;` |
|      51 |  4799 | `					}else if(rc != SXERR_EMPTY ){` |
|      51 |  4800 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      51 |  4801 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4802 | `							/* Variable name, not a constant */` |
|      51 |  4803 | `							pLast->iP1 = 0;` |
|      23 |  4804 | `						}` |
|      51 |  4805 | `						nExpr++;` |
|      23 |  4806 | `					}` |
|       - |  4807 | `				}` |
|       - |  4808 | `			}` |
|      23 |  4809 | `		}` |
|       - |  4810 | `		/* Next expression in the stream */` |
|      51 |  4811 | `		pGen->pIn = pNext;` |
|       - |  4812 | `		/* Jump trailing commas */` |
|      61 |  4813 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  4814 | `			pGen->pIn++;` |
|       5 |  4815 | `		}` |
|       5 |  4816 | `	}` |
|       - |  4817 | `	/* Restore token stream */` |
|      41 |  4818 | `	pGen->pEnd = pTmp;` |
|      41 |  4819 | `	if( nExpr > 0 ){` |
|       - |  4820 | `		/* Emit the uplink instruction */` |
|      41 |  4821 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  4822 | `	}` |
|      41 |  4823 | `	return SXRET_OK;` |
|      23 |  4824 | `}` |
|       - |  4825 | `/*` |
|       - |  4826 | ` * Compile the return statement.` |
|       - |  4827 | ` * According to the PHP language reference` |
|       - |  4828 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4829 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4830 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4831 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4832 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4833 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4834 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4835 | ` *  from within the main script file, then script execution end.` |
|       - |  4836 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4837 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4838 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4839 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4840 | ` */` |
|  254370 |  4841 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4842 | `{` |
|  254375 |  4843 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4844 | `	sxi32 rc;` |
|  254375 |  4845 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  254375 |  4846 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|       - |  4847 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|       - |  4848 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|       - |  4849 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|       - |  4850 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|       - |  4851 | `	 * normally below so token processing stays consistent. */` |
|  655059 |  4852 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  400689 |  4853 | `		pFuncBlock = pFuncBlock->pParent;` |
|       5 |  4854 | `	}` |
|  254370 |  4855 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  254343 |  4856 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|       3 |  4857 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  4858 | `			"A never-returning function must not return");` |
|       3 |  4859 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4860 | `			return SXERR_ABORT;` |
|       - |  4861 | `		}` |
|       1 |  4862 | `	}` |
|       - |  4863 | `	/* Jump the 'return' keyword */` |
|  254375 |  4864 | `	pGen->pIn++;` |
|  254375 |  4865 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4866 | `		/* Compile the expression */` |
|  254345 |  4867 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  254345 |  4868 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4869 | `			return SXERR_ABORT;` |
|  254345 |  4870 | `		}else if(rc != SXERR_EMPTY ){` |
|  254345 |  4871 | `			nRet = 1;` |
|  127170 |  4872 | `		}` |
|  127170 |  4873 | `	}` |
|       - |  4874 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|       - |  4875 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|       - |  4876 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|       - |  4877 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  254375 |  4878 | `	if( pGen->bInGenerator ){` |
|      30 |  4879 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      30 |  4880 | `		return SXRET_OK;` |
|       - |  4881 | `	}` |
|       - |  4882 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4883 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4884 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4885 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4886 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  254349 |  4887 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  254349 |  4888 | `	return SXRET_OK;` |
|  127190 |  4889 | `}` |
|       - |  4890 | `/*` |
|       - |  4891 | ` * Compile a yield expression.` |
|       - |  4892 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4893 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4894 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4895 | ` */` |
|     328 |  4896 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4897 | `{` |
|       - |  4898 | `	SyToken *pTmp, *pSplit;` |
|     333 |  4899 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     333 |  4900 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4901 | `	sxi32 rc;` |
|     164 |  4902 | `	(void)iCompileFlag;` |
|       - |  4903 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     333 |  4904 | `	pGen->pIn++;` |
|       - |  4905 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4906 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - |  4907 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - |  4908 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - |  4909 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     328 |  4910 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     194 |  4911 | `		&& pGen->pIn->sData.nByte == 4` |
|      66 |  4912 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      64 |  4913 | `		pGen->pIn++; /* Skip 'from' */` |
|      64 |  4914 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      64 |  4915 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4916 | `			return SXERR_ABORT;` |
|       - |  4917 | `		}` |
|      64 |  4918 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  4919 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 |  4920 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - |  4921 | `				"Missing expression after 'yield from'");` |
|     ! 0 |  4922 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4923 | `				return SXERR_ABORT;` |
|       - |  4924 | `			}` |
|     ! 0 |  4925 | `		}` |
|      64 |  4926 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      64 |  4927 | `		return SXRET_OK;` |
|       - |  4928 | `	}` |
|     273 |  4929 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4930 | `		/* Bare yield — no value */` |
|       3 |  4931 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       3 |  4932 | `		return SXRET_OK;` |
|       - |  4933 | `	}` |
|       - |  4934 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     271 |  4935 | `	pSplit = 0;` |
|       - |  4936 | `	{` |
|     271 |  4937 | `		SyToken *pCur = pGen->pIn;` |
|     271 |  4938 | `		sxi32 nNest = 0;` |
|     569 |  4939 | `		while( pCur < pGen->pEnd ){` |
|     317 |  4940 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  4941 | `				nNest++;` |
|     316 |  4942 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  4943 | `				nNest--;` |
|     314 |  4944 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4945 | `				pSplit = pCur;` |
|      16 |  4946 | `				break;` |
|       - |  4947 | `			}` |
|     303 |  4948 | `			pCur++;` |
|       5 |  4949 | `		}` |
|       - |  4950 | `	}` |
|     271 |  4951 | `	pTmp = pGen->pEnd;` |
|     271 |  4952 | `	if( pSplit ){` |
|       - |  4953 | `		/* yield $key => $value */` |
|      16 |  4954 | `		pGen->pEnd = pSplit;` |
|      16 |  4955 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4956 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4957 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  4958 | `		pGen->pEnd = pTmp;` |
|      16 |  4959 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4960 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4961 | `		iP1 = 1;` |
|      16 |  4962 | `		iP2 = 1;` |
|       9 |  4963 | `	}else{` |
|       - |  4964 | `		/* yield $value */` |
|     257 |  4965 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     257 |  4966 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     257 |  4967 | `		if( rc != SXERR_EMPTY ){` |
|     257 |  4968 | `			iP1 = 1;` |
|     126 |  4969 | `		}` |
|       - |  4970 | `	}` |
|     271 |  4971 | `	pGen->pEnd = pTmp;` |
|     271 |  4972 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     271 |  4973 | `	return SXRET_OK;` |
|     169 |  4974 | `}` |
|       - |  4975 | `/*` |
|       - |  4976 | ` * Compile the die/exit language construct.` |
|       - |  4977 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4978 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4979 | ` */` |
|     122 |  4980 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  4981 | `{` |
|     127 |  4982 | `	sxi32 nExpr = 0;` |
|       - |  4983 | `	sxi32 rc;` |
|       - |  4984 | `	/* Jump the die/exit keyword */` |
|     127 |  4985 | `	pGen->pIn++;` |
|     127 |  4986 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4987 | `		/* Compile the expression */` |
|     127 |  4988 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     127 |  4989 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4990 | `			return SXERR_ABORT;` |
|     127 |  4991 | `		}else if(rc != SXERR_EMPTY ){` |
|     127 |  4992 | `			nExpr = 1;` |
|      61 |  4993 | `		}` |
|      61 |  4994 | `	}` |
|       - |  4995 | `	/* Emit the HALT instruction */` |
|     127 |  4996 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     127 |  4997 | `	return SXRET_OK;` |
|      66 |  4998 | `}` |
|       - |  4999 | `/*` |
|       - |  5000 | ` * Compile the 'echo' language construct.` |
|       - |  5001 | ` */` |
|   14692 |  5002 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  5003 | `{` |
|   14697 |  5004 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  5005 | `	sxi32 rc;` |
|       - |  5006 | `	/* Jump the 'echo' keyword */` |
|   14697 |  5007 | `	pGen->pIn++;` |
|       - |  5008 | `	/* Compile arguments one after one */` |
|   14697 |  5009 | `	pTmp = pGen->pEnd;` |
|   33757 |  5010 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   19065 |  5011 | `		if( pGen->pIn < pNext ){` |
|   19065 |  5012 | `			pGen->pEnd = pNext;` |
|   19065 |  5013 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   19065 |  5014 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5015 | `				return SXERR_ABORT;` |
|   19065 |  5016 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  5017 | `				/* Emit the consume instruction */` |
|   19041 |  5018 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    9518 |  5019 | `			}` |
|    9530 |  5020 | `		}` |
|       - |  5021 | `		/* Jump trailing commas */` |
|   23433 |  5022 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    4373 |  5023 | `			pNext++;` |
|       5 |  5024 | `		}` |
|   19065 |  5025 | `		pGen->pIn = pNext;` |
|       5 |  5026 | `	}` |
|       - |  5027 | `	/* Restore token stream */` |
|   14697 |  5028 | `	pGen->pEnd = pTmp;` |
|   14697 |  5029 | `	return SXRET_OK;` |
|    7351 |  5030 | `}` |
|       - |  5031 | `/*` |
|       - |  5032 | ` * Compile the static statement.` |
|       - |  5033 | ` * According to the PHP language reference` |
|       - |  5034 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  5035 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  5036 | ` *  when program execution leaves this scope.` |
|       - |  5037 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  5038 | ` * Symisc eXtension.` |
|       - |  5039 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  5040 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  5041 | ` *  Example` |
|       - |  5042 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  5043 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  5044 | ` */` |
|       8 |  5045 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       3 |  5046 | `{` |
|       - |  5047 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  5048 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  5049 | `	GenBlock *pBlock;` |
|       - |  5050 | `	SyString *pName;` |
|       - |  5051 | `	char *zDup;` |
|       - |  5052 | `	sxu32 nLine;` |
|       - |  5053 | `	sxi32 rc;` |
|       - |  5054 | `	/* Jump the static keyword */` |
|      11 |  5055 | `	nLine = pGen->pIn->nLine;` |
|      11 |  5056 | `	pGen->pIn++;` |
|       - |  5057 | `	/* Extract the enclosing function if any */` |
|      11 |  5058 | `	pBlock = pGen->pCurrent;` |
|      19 |  5059 | `	while( pBlock ){` |
|      19 |  5060 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|      11 |  5061 | `			break;` |
|       - |  5062 | `		}` |
|       - |  5063 | `		/* Point to the upper block */` |
|      11 |  5064 | `		pBlock = pBlock->pParent;` |
|       3 |  5065 | `	}` |
|      11 |  5066 | `	if( pBlock == 0 ){` |
|       - |  5067 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  5068 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  5069 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  5070 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5071 | `				return SXERR_ABORT;` |
|       - |  5072 | `			}` |
|     ! 0 |  5073 | `			goto Synchronize;` |
|       - |  5074 | `		}` |
|       - |  5075 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  5076 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  5077 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5078 | `			return SXERR_ABORT;` |
|     ! 0 |  5079 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  5080 | `			/* Emit the POP instruction */` |
|     ! 0 |  5081 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  5082 | `		}` |
|     ! 0 |  5083 | `		return SXRET_OK;` |
|       - |  5084 | `	}` |
|      11 |  5085 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  5086 | `	/* Make sure we are dealing with a valid statement */` |
|      11 |  5087 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       6 |  5088 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  5089 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  5090 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5091 | `				return SXERR_ABORT;` |
|       - |  5092 | `			}` |
|       3 |  5093 | `			goto Synchronize;` |
|       - |  5094 | `	}` |
|       8 |  5095 | `	pGen->pIn++;` |
|       - |  5096 | `	/* Extract variable name */` |
|       8 |  5097 | `	pName = &pGen->pIn->sData;` |
|       8 |  5098 | `	pGen->pIn++; /* Jump the var name */` |
|       8 |  5099 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  5100 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  5101 | `		goto Synchronize;` |
|       - |  5102 | `	}` |
|       - |  5103 | `	/* Initialize the structure describing the static variable */` |
|       8 |  5104 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       8 |  5105 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  5106 | `	/* Duplicate variable name */` |
|       8 |  5107 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       8 |  5108 | `	if( zDup == 0 ){` |
|     ! 0 |  5109 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  5110 | `		return SXERR_ABORT;` |
|       - |  5111 | `	}` |
|       8 |  5112 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  5113 | `	/* Check if we have an expression to compile */` |
|       8 |  5114 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  5115 | `		SySet *pInstrContainer;` |
|       - |  5116 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  5117 | `		 * Static variable can take any complex expression including function` |
|       - |  5118 | `		 * call as their initialization value.` |
|       - |  5119 | `		 * Example:` |
|       - |  5120 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  5121 | `		 */` |
|       8 |  5122 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  5123 | `		/* Swap bytecode container */` |
|       8 |  5124 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       8 |  5125 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  5126 | `		/* Compile the expression */` |
|       8 |  5127 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5128 | `		/* Emit the done instruction */` |
|       8 |  5129 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  5130 | `		/* Restore default bytecode container */` |
|       8 |  5131 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       3 |  5132 | `	}` |
|       - |  5133 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       8 |  5134 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       8 |  5135 | `	return SXRET_OK;` |
|       1 |  5136 | `Synchronize:` |
|       - |  5137 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  5138 | `	 * statement.` |
|       - |  5139 | `	 */` |
|       5 |  5140 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  5141 | `		pGen->pIn++;` |
|       1 |  5142 | `	}` |
|       3 |  5143 | `	return SXRET_OK;` |
|       7 |  5144 | `}` |
|       - |  5145 | `/*` |
|       - |  5146 | ` * Compile the var statement.` |
|       - |  5147 | ` * Symisc Extension:` |
|       - |  5148 | ` *      var statement can be used outside of a class definition.` |
|       - |  5149 | ` */` |
|       4 |  5150 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  5151 | `{` |
|       - |  5152 | `	sxu32 nLine;` |
|       - |  5153 | `	sxi32 rc;` |
|       5 |  5154 | `	nLine = pGen->pIn->nLine;` |
|       - |  5155 | `	/* Jump the 'var' keyword */` |
|       5 |  5156 | `	pGen->pIn++;` |
|       5 |  5157 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  5158 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  5159 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  5160 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  5161 | `			pGen->pIn++;` |
|     ! 0 |  5162 | `		}` |
|     ! 0 |  5163 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5164 | `			return SXERR_ABORT;` |
|       - |  5165 | `		}` |
|     ! 0 |  5166 | `	}else{` |
|       - |  5167 | `		/* Compile the expression */` |
|       5 |  5168 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  5169 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5170 | `			return SXERR_ABORT;` |
|       5 |  5171 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  5172 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  5173 | `		}` |
|       - |  5174 | `	}` |
|       5 |  5175 | `	return SXRET_OK;` |
|       3 |  5176 | `}` |
|       - |  5177 | `/*` |
|       - |  5178 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  5179 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  5180 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  5181 | ` */` |
|       - |  5182 | `/*` |
|       - |  5183 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  5184 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  5185 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  5186 | ` * qualified name and updates the instruction's operand index.` |
|       - |  5187 | ` *` |
|       - |  5188 | ` * Resolution order:` |
|       - |  5189 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  5190 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  5191 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  5192 | ` *` |
|       - |  5193 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  5194 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  5195 | ` * Returns the (possibly new) literal index.` |
|       - |  5196 | ` */` |
|  493598 |  5197 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  5198 | `{` |
|       - |  5199 | `	ph7_value *pLit;` |
|       - |  5200 | `	const char *zLit;` |
|       - |  5201 | `	SyString sQualified;` |
|       - |  5202 | `	sxu32 nLit;` |
|       - |  5203 | `	sxu32 k;` |
|       - |  5204 | `	sxu32 nNewIdx;` |
|       - |  5205 | `	int hasNsSep;` |
|       - |  5206 | `	SyHashEntry *pImport;` |
|       - |  5207 | `	ph7_value *pNew;` |
|  493603 |  5208 | `	if( pFromImport ){` |
|  472349 |  5209 | `		*pFromImport = 0;` |
|  236172 |  5210 | `	}` |
|  493603 |  5211 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  493603 |  5212 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  5213 | `		return nOrigIdx;` |
|       - |  5214 | `	}` |
|  493603 |  5215 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  493603 |  5216 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  5217 | `	/* Skip if already qualified (contains backslash) */` |
|  493603 |  5218 | `	hasNsSep = 0;` |
| 5447679 |  5219 | `	for( k = 0; k < nLit; k++ ){` |
| 4954089 |  5220 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2477043 |  5221 | `	}` |
|  493603 |  5222 | `	if( hasNsSep ){` |
|      10 |  5223 | `		return nOrigIdx;` |
|       - |  5224 | `	}` |
|       - |  5225 | `	/* Check use imports first (works even outside namespaces) */` |
|  493595 |  5226 | `	SyBlobReset(&pGen->sWorker);` |
|  493595 |  5227 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  493595 |  5228 | `	if( pImport ){` |
|      41 |  5229 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  5230 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  5231 | `		if( pFromImport ){` |
|      18 |  5232 | `			*pFromImport = 1;` |
|       8 |  5233 | `		}` |
|      23 |  5234 | `	}else{` |
|  493559 |  5235 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  493469 |  5236 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  5237 | `		}` |
|       - |  5238 | `		/* Prepend current namespace */` |
|      95 |  5239 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  5240 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  5241 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  5242 | `	}` |
|       - |  5243 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  5244 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  5245 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  5246 | `		return nNewIdx; /* Already interned */` |
|       - |  5247 | `	}` |
|      79 |  5248 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  5249 | `	if( pNew == 0 ){` |
|     ! 0 |  5250 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  5251 | `	}` |
|      79 |  5252 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  5253 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  5254 | `	return nNewIdx;` |
|  246804 |  5255 | `}` |
|       - |  5256 | `/*` |
|       - |  5257 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  5258 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  5259 | ` */` |
|  104474 |  5260 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5261 | `{` |
|       - |  5262 | `	SyHashEntry *pImport;` |
|       - |  5263 | `	/* Check use imports first */` |
|  104479 |  5264 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|  104479 |  5265 | `	if( pImport ){` |
|      19 |  5266 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      19 |  5267 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      19 |  5268 | `		return;` |
|       - |  5269 | `	}` |
|       - |  5270 | `	/* Prepend current namespace if active */` |
|  104463 |  5271 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  5272 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  5273 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  5274 | `	}` |
|  104463 |  5275 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   52242 |  5276 | `}` |
|       - |  5277 | `/*` |
|       - |  5278 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5279 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5280 | ` * The caller must release pOut when done.` |
|       - |  5281 | ` */` |
|  154668 |  5282 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5283 | `{` |
|  154673 |  5284 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|    3893 |  5285 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|    3893 |  5286 | `		SyBlobAppend(pOut,"\\",1);` |
|    1944 |  5287 | `	}` |
|  154673 |  5288 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  154673 |  5289 | `}` |
|       - |  5290 | `/*` |
|       - |  5291 | ` * Compile a namespace statement` |
|       - |  5292 | ` * According to the PHP language reference manual` |
|       - |  5293 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5294 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5295 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5296 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5297 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5298 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5299 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5300 | ` *  programming world.` |
|       - |  5301 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5302 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5303 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5304 | ` *  classes/functions/constants.` |
|       - |  5305 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5306 | ` *  readability of source code.` |
|       - |  5307 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5308 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5309 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5310 | ` *       class MyClass {}` |
|       - |  5311 | ` *       function myfunction() {}` |
|       - |  5312 | ` *       const MYCONST = 1;` |
|       - |  5313 | ` *       $a = new MyClass;` |
|       - |  5314 | ` *       $c = new \my\name\MyClass;` |
|       - |  5315 | ` *       $a = strlen('hi');` |
|       - |  5316 | ` *       $d = namespace\MYCONST;` |
|       - |  5317 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5318 | ` *       echo constant($d);` |
|       - |  5319 | ` * NOTE` |
|       - |  5320 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5321 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5322 | ` */` |
|       - |  5323 | `/*` |
|       - |  5324 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5325 | ` */` |
|      14 |  5326 | `static const char * TokenTypeName(sxu32 nType)` |
|       3 |  5327 | `{` |
|      17 |  5328 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      10 |  5329 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      10 |  5330 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      10 |  5331 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      10 |  5332 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      10 |  5333 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5334 | `	return "token";` |
|      10 |  5335 | `}` |
|    3936 |  5336 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5337 | `{` |
|       - |  5338 | `	sxu32 nLine;` |
|       - |  5339 | `	sxi32 rc;` |
|    3941 |  5340 | `	nLine = pGen->pIn->nLine;` |
|    3941 |  5341 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5342 | `	/* Reset namespace and clear previous use imports */` |
|    3941 |  5343 | `	SyBlobReset(&pGen->sNamespace);` |
|    3941 |  5344 | `	SyHashRelease(&pGen->hUseImports);` |
|    3941 |  5345 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|    3941 |  5346 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|    3941 |  5347 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|    3941 |  5348 | `	SyHashRelease(&pGen->hUseConstImports);` |
|    3941 |  5349 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|    3941 |  5350 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5351 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5352 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5353 | `		return SXRET_OK;` |
|       - |  5354 | `	}` |
|    3941 |  5355 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5356 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5357 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5358 | `		return SXRET_OK;` |
|       - |  5359 | `	}` |
|    3941 |  5360 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5361 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5362 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5363 | `		return SXRET_OK;` |
|       - |  5364 | `	}` |
|       - |  5365 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|    7919 |  5366 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|    3983 |  5367 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5368 | `			/* Append backslash separator */` |
|      27 |  5369 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      27 |  5370 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5371 | `			}` |
|      16 |  5372 | `		}else{` |
|       - |  5373 | `			/* Append identifier */` |
|    3961 |  5374 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5375 | `		}` |
|    3983 |  5376 | `		pGen->pIn++;` |
|       5 |  5377 | `	}` |
|       - |  5378 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5379 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5380 | `	{` |
|    3941 |  5381 | `		char *zNsDup = 0;` |
|    3941 |  5382 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|    5906 |  5383 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    3934 |  5384 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|    1967 |  5385 | `		}` |
|    3941 |  5386 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5387 | `	}` |
|    3941 |  5388 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5389 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5390 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5391 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5392 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5393 | `			return SXERR_ABORT;` |
|       - |  5394 | `		}` |
|       2 |  5395 | `	}` |
|    3941 |  5396 | `	return SXRET_OK;` |
|    1973 |  5397 | `}` |
|       - |  5398 | `/*` |
|       - |  5399 | ` * Compile the 'use' statement` |
|       - |  5400 | ` * According to the PHP language reference manual` |
|       - |  5401 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5402 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5403 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5404 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5405 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5406 | ` *  a function or constant is not supported.` |
|       - |  5407 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5408 | ` * NOTE` |
|       - |  5409 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5410 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5411 | ` */` |
|      72 |  5412 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5413 | `{` |
|       - |  5414 | `	sxu32 nLine;` |
|       - |  5415 | `	sxi32 rc;` |
|       - |  5416 | `	SyBlob sPath;` |
|       - |  5417 | `	SyString sAlias;` |
|       - |  5418 | `	SyToken *pLast;` |
|       - |  5419 | `	char *zDup;` |
|       - |  5420 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5421 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5422 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      77 |  5423 | `	nLine = pGen->pIn->nLine;` |
|      77 |  5424 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5425 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      77 |  5426 | `	iUseType = 0;` |
|      77 |  5427 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5428 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5429 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5430 | `			iUseType = 1;` |
|      16 |  5431 | `			pGen->pIn++;` |
|      23 |  5432 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5433 | `			iUseType = 2;` |
|      16 |  5434 | `			pGen->pIn++;` |
|       7 |  5435 | `		}` |
|      14 |  5436 | `	}` |
|       - |  5437 | `	/* Select target hash tables based on import type */` |
|      77 |  5438 | `	switch( iUseType ){` |
|       7 |  5439 | `		case 1:` |
|      16 |  5440 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5441 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5442 | `			break;` |
|       7 |  5443 | `		case 2:` |
|      16 |  5444 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5445 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5446 | `			break;` |
|      22 |  5447 | `		default:` |
|      49 |  5448 | `			pGenHash = &pGen->hUseImports;` |
|      49 |  5449 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      44 |  5450 | `			break;` |
|       - |  5451 | `	}` |
|      77 |  5452 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5453 | `	/* Process one or more use declarations separated by commas */` |
|      37 |  5454 | `	for(;;){` |
|      79 |  5455 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5456 | `			break;` |
|       - |  5457 | `		}` |
|      79 |  5458 | `		SyBlobReset(&sPath);` |
|      79 |  5459 | `		pLast = 0;` |
|       - |  5460 | `		/* Collect the full namespace path */` |
|     269 |  5461 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     195 |  5462 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     135 |  5463 | `				pLast = pGen->pIn;` |
|     135 |  5464 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5465 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5466 | `				}` |
|     135 |  5467 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      65 |  5468 | `			}` |
|     195 |  5469 | `			pGen->pIn++;` |
|       5 |  5470 | `		}` |
|      79 |  5471 | `		if( pLast == 0 ){` |
|       - |  5472 | `			/* Empty path */` |
|       6 |  5473 | `			break;` |
|       - |  5474 | `		}` |
|       - |  5475 | `		/* Default alias is the last component of the path */` |
|      75 |  5476 | `		sAlias = pLast->sData;` |
|       - |  5477 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      70 |  5478 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      50 |  5479 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      24 |  5480 | `			pGen->pIn++; /* Jump 'as' */` |
|      24 |  5481 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      24 |  5482 | `				sAlias = pGen->pIn->sData;` |
|      24 |  5483 | `				pGen->pIn++;` |
|      10 |  5484 | `			}` |
|      10 |  5485 | `		}` |
|       - |  5486 | `		/* Check for duplicate import alias (per-type) */` |
|      75 |  5487 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5488 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5489 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5490 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5491 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5492 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5493 | `				return SXERR_ABORT;` |
|       - |  5494 | `			}` |
|       2 |  5495 | `		}` |
|       - |  5496 | `		/* Register the import: alias -> FQN.` |
|       - |  5497 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5498 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5499 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     110 |  5500 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      70 |  5501 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      75 |  5502 | `		if( zDup ){` |
|      75 |  5503 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      75 |  5504 | `			if( pVmHash ){` |
|       - |  5505 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5506 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      47 |  5507 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      47 |  5508 | `				if( zAliasDup ){` |
|      47 |  5509 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      21 |  5510 | `				}` |
|      21 |  5511 | `			}` |
|      75 |  5512 | `			if( iUseType == 2 ){` |
|       - |  5513 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5514 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5515 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5516 | `				if( zAliasDup ){` |
|       - |  5517 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5518 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5519 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5520 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5521 | `					if( azPair ){` |
|      16 |  5522 | `						azPair[0] = zAliasDup;` |
|      16 |  5523 | `						azPair[1] = zDup;` |
|      16 |  5524 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5525 | `					}` |
|       7 |  5526 | `				}` |
|       7 |  5527 | `			}` |
|      35 |  5528 | `		}` |
|       - |  5529 | `		/* Check for comma (multiple use declarations) */` |
|      75 |  5530 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5531 | `			pGen->pIn++;` |
|       2 |  5532 | `		}else{` |
|      39 |  5533 | `			break;` |
|       - |  5534 | `		}` |
|       1 |  5535 | `	}` |
|      77 |  5536 | `	SyBlobRelease(&sPath);` |
|      77 |  5537 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5538 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5539 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5540 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5541 | `			return SXERR_ABORT;` |
|       - |  5542 | `		}` |
|       1 |  5543 | `	}` |
|      77 |  5544 | `	return SXRET_OK;` |
|      41 |  5545 | `}` |
|       - |  5546 | `/*` |
|       - |  5547 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5548 | ` *` |
|       - |  5549 | ` * According to the PHP language reference manual.` |
|       - |  5550 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5551 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5552 | ` *  declare (directive)` |
|       - |  5553 | ` *   statement` |
|       - |  5554 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5555 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5556 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5557 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5558 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5559 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5560 | ` * <?php` |
|       - |  5561 | ` * // these are the same:` |
|       - |  5562 | ` * // you can use this:` |
|       - |  5563 | ` * declare(ticks=1) {` |
|       - |  5564 | ` *   // entire script here` |
|       - |  5565 | ` * }` |
|       - |  5566 | ` * // or you can use this:` |
|       - |  5567 | ` * declare(ticks=1);` |
|       - |  5568 | ` * // entire script here` |
|       - |  5569 | ` * ?>` |
|       - |  5570 | ` *` |
|       - |  5571 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5572 | ` */` |
|       - |  5573 | `/*` |
|       - |  5574 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5575 | ` */` |
|      72 |  5576 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5577 | `{` |
|     109 |  5578 | `	return SyStringLength(pName) == nWant` |
|      72 |  5579 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5580 | `}` |
|       - |  5581 |  |
|      42 |  5582 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5583 | `{` |
|      47 |  5584 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      47 |  5585 | `	SyToken *pBodyEnd = 0;` |
|       - |  5586 | `	SyToken *pBodyStart;` |
|       - |  5587 | `	SyToken *pCursor;` |
|       - |  5588 | `	int bHasStrictTypes;` |
|       - |  5589 | `	int bBlockForm;` |
|       - |  5590 | `	int bPlacementOk;` |
|       - |  5591 | `	sxi32 rc;` |
|      47 |  5592 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      47 |  5593 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 |  5594 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       6 |  5595 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5596 | `			return SXERR_ABORT;` |
|       - |  5597 | `		}` |
|       6 |  5598 | `		goto Synchro;` |
|       - |  5599 | `	}` |
|      43 |  5600 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      43 |  5601 | `	pBodyStart = pGen->pIn;` |
|       - |  5602 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      43 |  5603 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      43 |  5604 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5605 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5606 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5607 | `			return SXERR_ABORT;` |
|       - |  5608 | `		}` |
|     ! 0 |  5609 | `		return SXRET_OK;` |
|       - |  5610 | `	}` |
|       - |  5611 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5612 | `	 * now delimits the comma-separated directive list. */` |
|      43 |  5613 | `	pGen->pIn = &pBodyEnd[1];` |
|      43 |  5614 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5615 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5616 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5617 | `			return SXERR_ABORT;` |
|       - |  5618 | `		}` |
|     ! 0 |  5619 | `	}` |
|      43 |  5620 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      43 |  5621 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      43 |  5622 | `	bHasStrictTypes = 0;` |
|       - |  5623 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5624 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5625 | `	 * directive appears anywhere in the list, before validating values. */` |
|      43 |  5626 | `	pCursor = pBodyStart;` |
|      55 |  5627 | `	while( pCursor < pBodyEnd ){` |
|      51 |  5628 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      43 |  5629 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      39 |  5630 | `				bHasStrictTypes = 1;` |
|      39 |  5631 | `				break;` |
|       - |  5632 | `			}` |
|       2 |  5633 | `		}` |
|      14 |  5634 | `		pCursor++;` |
|       2 |  5635 | `	}` |
|      43 |  5636 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5637 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5638 | `			"strict_types declaration must not use block mode");` |
|       3 |  5639 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5640 | `		return SXRET_OK;` |
|       - |  5641 | `	}` |
|      41 |  5642 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5643 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5644 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5645 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5646 | `		return SXRET_OK;` |
|       - |  5647 | `	}` |
|       - |  5648 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      37 |  5649 | `	pCursor = pBodyStart;` |
|      69 |  5650 | `	while( pCursor < pBodyEnd ){` |
|       - |  5651 | `		SyToken *pNameTok;` |
|       - |  5652 | `		SyToken *pEqTok;` |
|       - |  5653 | `		SyToken *pValTok;` |
|       - |  5654 | `		SyString *pDirName;` |
|       - |  5655 | `		int bIsStrict;` |
|       - |  5656 | `		int iStrictValue;` |
|      39 |  5657 | `		pNameTok = pCursor;` |
|      39 |  5658 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5659 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5660 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5661 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5662 | `			return SXRET_OK;` |
|       - |  5663 | `		}` |
|      39 |  5664 | `		pEqTok = pNameTok + 1;` |
|      39 |  5665 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5666 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5667 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5668 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5669 | `			return SXRET_OK;` |
|       - |  5670 | `		}` |
|      39 |  5671 | `		pValTok = pEqTok + 1;` |
|      39 |  5672 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5673 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5674 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5675 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5676 | `			return SXRET_OK;` |
|       - |  5677 | `		}` |
|      39 |  5678 | `		pDirName = &pNameTok->sData;` |
|      39 |  5679 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      39 |  5680 | `		if( bIsStrict ){` |
|       - |  5681 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5682 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      35 |  5683 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5684 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5685 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5686 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5687 | `				return SXRET_OK;` |
|       - |  5688 | `			}` |
|      35 |  5689 | `			iStrictValue = -1;` |
|      35 |  5690 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      35 |  5691 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      35 |  5692 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      35 |  5693 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      33 |  5694 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      15 |  5695 | `			}` |
|      35 |  5696 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5697 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5698 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5699 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5700 | `				return SXRET_OK;` |
|       - |  5701 | `			}` |
|      32 |  5702 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      18 |  5703 | `		}else{` |
|       - |  5704 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5705 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5706 | `			 * behavior don't regress. */` |
|       8 |  5707 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5708 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5709 | `				ph7_lib_version()` |
|       - |  5710 | `				);` |
|       - |  5711 | `		}` |
|      37 |  5712 | `		pCursor = pValTok + 1;` |
|       - |  5713 | `		/* Consume separating comma (or end). */` |
|      37 |  5714 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5715 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5716 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5717 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5718 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5719 | `				return SXRET_OK;` |
|       - |  5720 | `			}` |
|       3 |  5721 | `			pCursor++;` |
|       1 |  5722 | `		}` |
|       5 |  5723 | `	}` |
|       - |  5724 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5725 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5726 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      35 |  5727 | `	return SXRET_OK;` |
|       2 |  5728 | `Synchro:` |
|       - |  5729 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 |  5730 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 |  5731 | `		pGen->pIn++;` |
|       2 |  5732 | `	}` |
|       6 |  5733 | `	return SXRET_OK;` |
|      26 |  5734 | `}` |
|       - |  5735 | `/*` |
|       - |  5736 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5737 | ` * as follows:` |
|       - |  5738 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5739 | ` * {` |
|       - |  5740 | ` *   return "Making a cup of $type.\n";` |
|       - |  5741 | ` * }` |
|       - |  5742 | ` * Symisc eXtension.` |
|       - |  5743 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5744 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5745 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5746 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5747 | ` *      {` |
|       - |  5748 | ` *       var_dump($a);` |
|       - |  5749 | ` *      }` |
|       - |  5750 | ` *     //call test without args` |
|       - |  5751 | ` *      test();` |
|       - |  5752 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5753 | ` *      Example:` |
|       - |  5754 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5755 | ` * 3 -) Function overloading!!` |
|       - |  5756 | ` *      Example:` |
|       - |  5757 | ` *      function foo($a) {` |
|       - |  5758 | ` *   	  return $a.PHP_EOL;` |
|       - |  5759 | ` *	    }` |
|       - |  5760 | ` *	    function foo($a, $b) {` |
|       - |  5761 | ` *   	  return $a + $b;` |
|       - |  5762 | ` *	    }` |
|       - |  5763 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5764 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5765 | ` *      // Same arg` |
|       - |  5766 | ` *	   function foo(string $a)` |
|       - |  5767 | ` *	   {` |
|       - |  5768 | ` *	     echo "a is a string\n";` |
|       - |  5769 | ` *	     var_dump($a);` |
|       - |  5770 | ` *	   }` |
|       - |  5771 | ` *	  function foo(int $a)` |
|       - |  5772 | ` *	  {` |
|       - |  5773 | ` *	    echo "a is integer\n";` |
|       - |  5774 | ` *	    var_dump($a);` |
|       - |  5775 | ` *	  }` |
|       - |  5776 | ` *	  function foo(array $a)` |
|       - |  5777 | ` *	  {` |
|       - |  5778 | ` * 	    echo "a is an array\n";` |
|       - |  5779 | ` * 	    var_dump($a);` |
|       - |  5780 | ` *	  }` |
|       - |  5781 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5782 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5783 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5784 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5785 | ` * introduced by the PH7 engine.` |
|       - |  5786 | ` */` |
|   80500 |  5787 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5788 | `{` |
|       - |  5789 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5790 | `	SySet *pInstrContainer;` |
|       - |  5791 | `	sxi32 rc;` |
|       - |  5792 | `	/* Swap token stream */` |
|   80505 |  5793 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   80505 |  5794 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   80505 |  5795 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5796 | `	/* Compile the expression holding the argument value */` |
|   80505 |  5797 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5798 | `	/* Emit the done instruction */` |
|   80505 |  5799 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   80505 |  5800 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   80505 |  5801 | `	RE_SWAP_DELIMITER(pGen);` |
|   80505 |  5802 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5803 | `		return SXERR_ABORT;` |
|       - |  5804 | `	}` |
|   80505 |  5805 | `	return SXRET_OK;` |
|   40255 |  5806 | `}` |
|       - |  5807 | `/*` |
|       - |  5808 | ` * Collect function arguments one after one.` |
|       - |  5809 | ` * According to the PHP language reference manual.` |
|       - |  5810 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5811 | ` * list of expressions.` |
|       - |  5812 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5813 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5814 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5815 | ` * for more information.` |
|       - |  5816 | ` * Example #1 Passing arrays to functions` |
|       - |  5817 | ` * <?php` |
|       - |  5818 | ` * function takes_array($input)` |
|       - |  5819 | ` * {` |
|       - |  5820 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5821 | ` * }` |
|       - |  5822 | ` * ?>` |
|       - |  5823 | ` * Making arguments be passed by reference` |
|       - |  5824 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5825 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5826 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5827 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5828 | ` * to the argument name in the function definition:` |
|       - |  5829 | ` * Example #2 Passing function parameters by reference` |
|       - |  5830 | ` * <?php` |
|       - |  5831 | ` * function add_some_extra(&$string)` |
|       - |  5832 | ` * {` |
|       - |  5833 | ` *   $string .= 'and something extra.';` |
|       - |  5834 | ` * }` |
|       - |  5835 | ` * $str = 'This is a string, ';` |
|       - |  5836 | ` * add_some_extra($str);` |
|       - |  5837 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5838 | ` * ?>` |
|       - |  5839 | ` *` |
|       - |  5840 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5841 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5842 | ` * on these extension.` |
|       - |  5843 | ` */` |
|  112582 |  5844 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5845 | `{` |
|       - |  5846 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5847 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5848 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5849 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5850 | `	sxi32 rc;` |
|       - |  5851 |  |
|  112587 |  5852 | `	pIn = pGen->pIn;` |
|  112587 |  5853 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5854 | `	/* Process arguments one after one */` |
|  145531 |  5855 | `	for(;;){` |
|  291067 |  5856 | `		if( pIn >= pEnd ){` |
|       - |  5857 | `			/* No more arguments to process */` |
|  112571 |  5858 | `			break;` |
|       - |  5859 | `		}` |
|  178501 |  5860 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  178501 |  5861 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  178501 |  5862 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  178501 |  5863 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5864 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  5865 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  5866 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  5867 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  5868 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  5869 | `		{` |
|  178501 |  5870 | `			int bReadonly = 0, bVisSeen = 0;` |
|  178501 |  5871 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  178501 |  5872 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  5873 | `				bReadonly = 1;` |
|       3 |  5874 | `				pIn++;` |
|       1 |  5875 | `			}` |
|  178501 |  5876 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   69241 |  5877 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   69241 |  5878 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      73 |  5879 | `					bVisSeen = 1;` |
|      73 |  5880 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      96 |  5881 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      31 |  5882 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      73 |  5883 | `					pIn++;` |
|      73 |  5884 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      16 |  5885 | `						bReadonly = 1;` |
|      16 |  5886 | `						pIn++;` |
|       6 |  5887 | `					}` |
|      34 |  5888 | `				}` |
|   34618 |  5889 | `			}` |
|  178501 |  5890 | `			if( bVisSeen \|\| bReadonly ){` |
|      75 |  5891 | `				if( !bCtorCtx ){` |
|       6 |  5892 | `					if( bAbstractCtx ){` |
|       3 |  5893 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5894 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5895 | `					}else{` |
|       3 |  5896 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5897 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5898 | `					}` |
|       6 |  5899 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5900 | `						return SXERR_ABORT;` |
|       - |  5901 | `					}` |
|       6 |  5902 | `					return SXERR_SYNTAX;` |
|       - |  5903 | `				}` |
|      71 |  5904 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      71 |  5905 | `				sArg.iPromoteVis = iVis;` |
|      71 |  5906 | `				if( bReadonly ){` |
|      18 |  5907 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       7 |  5908 | `				}` |
|      33 |  5909 | `			}` |
|       - |  5910 | `		}` |
|       - |  5911 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  178492 |  5912 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  135428 |  5913 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   90441 |  5914 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   84653 |  5915 | `			sxu32 nLineLocal = pIn->nLine;` |
|   84653 |  5916 | `			sxi32 iTFlags = 0;` |
|   84653 |  5917 | `			pGen->pIn = pIn;` |
|   84653 |  5918 | `			rc = GenStateParseUnionTypeDecl(` |
|   42324 |  5919 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   42324 |  5920 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5921 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5922 | `				/* bAllowVoid */ 0,` |
|   42324 |  5923 | `						nLineLocal);` |
|   84653 |  5924 | `			pIn = pGen->pIn;` |
|   84653 |  5925 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5926 | `				return SXERR_ABORT;` |
|   84653 |  5927 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5928 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5929 | `				return SXERR_SYNTAX;` |
|   84651 |  5930 | `			}else if( rc == SXERR_SYNTAX ){` |
|      12 |  5931 | `				if( pIn < pEnd ){` |
|      16 |  5932 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5933 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       4 |  5934 | `						&pIn->sData);` |
|       8 |  5935 | `				}else{` |
|     ! 0 |  5936 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5937 | `						"syntax error, unexpected end of file");` |
|       - |  5938 | `				}` |
|      12 |  5939 | `				return SXERR_SYNTAX;` |
|       - |  5940 | `			}` |
|   84643 |  5941 | `			sArg.iFlags \|= iTFlags;` |
|   42319 |  5942 | `		}` |
|  178487 |  5943 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5944 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5945 | `			return rc;` |
|       - |  5946 | `		}` |
|  178487 |  5947 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5948 | `			/* Pass by reference,record that */` |
|    3865 |  5949 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3865 |  5950 | `			pIn++;` |
|    1930 |  5951 | `		}` |
|  178487 |  5952 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5953 | `			/* Variadic parameter: ...$args */` |
|    3885 |  5954 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    3885 |  5955 | `			pIn++;` |
|    1940 |  5956 | `		}` |
|  178487 |  5957 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5958 | `			/* Invalid argument */` |
|     ! 0 |  5959 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5960 | `			return rc;` |
|       - |  5961 | `		}` |
|  178487 |  5962 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5963 | `		/* Copy argument name */` |
|  178487 |  5964 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  178487 |  5965 | `		if( zDup == 0 ){` |
|     ! 0 |  5966 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5967 | `			return SXERR_ABORT;` |
|       - |  5968 | `		}` |
|  178487 |  5969 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  178487 |  5970 | `		pIn++;` |
|  178487 |  5971 | `		if( pIn < pEnd ){` |
|  108113 |  5972 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5973 | `				SyToken *pDefend;` |
|   80507 |  5974 | `				sxi32 iNest = 0;` |
|   80507 |  5975 | `				pIn++; /* Jump the equal sign */` |
|   80507 |  5976 | `				pDefend = pIn;` |
|       - |  5977 | `				/* Process the default value associated with this argument */` |
|  168681 |  5978 | `				while( pDefend < pEnd ){` |
|  126493 |  5979 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   38319 |  5980 | `						break;` |
|       - |  5981 | `					}` |
|   88179 |  5982 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5983 | `						/* Increment nesting level */` |
|    3841 |  5984 | `						iNest++;` |
|   86261 |  5985 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5986 | `						/* Decrement nesting level */` |
|    3841 |  5987 | `						iNest--;` |
|    1918 |  5988 | `					}` |
|   88179 |  5989 | `					pDefend++;` |
|       5 |  5990 | `				}` |
|   80507 |  5991 | `				if( pIn >= pDefend ){` |
|       3 |  5992 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5993 | `					return rc;` |
|       - |  5994 | `				}` |
|       - |  5995 | `				/* Process default value */` |
|   80505 |  5996 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   80505 |  5997 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5998 | `					return rc;` |
|       - |  5999 | `				}` |
|       - |  6000 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|       - |  6001 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|       - |  6002 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|       - |  6003 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|       - |  6004 | `				 * arg-type check lets null through. */` |
|   80500 |  6005 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|   63241 |  6006 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|   63240 |  6007 | `					&& &pIn[1] == pDefend` |
|   44065 |  6008 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|   34483 |  6009 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|   21076 |  6010 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|   15331 |  6011 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|    7663 |  6012 | `				}` |
|       - |  6013 | `				/* Point beyond the default value */` |
|   80505 |  6014 | `				pIn = pDefend;` |
|   40250 |  6015 | `			}` |
|  108111 |  6016 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  6017 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  6018 | `				return rc;` |
|       - |  6019 | `			}` |
|  108111 |  6020 | `			pIn++; /* Jump the trailing comma */` |
|   54053 |  6021 | `		}` |
|       - |  6022 | `		/* Append argument signature */` |
|  178485 |  6023 | `		if( sArg.nType > 0 ){` |
|   84587 |  6024 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  6025 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|   15391 |  6026 | `				int marker = 'o';` |
|   15391 |  6027 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|   15391 |  6028 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    7698 |  6029 | `			}else{` |
|       - |  6030 | `				int c;` |
|   69201 |  6031 | `				c = 'n'; /* cc warning */` |
|       - |  6032 | `				/* Type leading character */` |
|   69201 |  6033 | `				switch(sArg.nType){` |
|       4 |  6034 | `				case MEMOBJ_HASHMAP:` |
|       - |  6035 | `					/* Hashmap aka 'array' */` |
|       9 |  6036 | `					c = 'h';` |
|       9 |  6037 | `					break;` |
|    9660 |  6038 | `				case MEMOBJ_INT:` |
|       - |  6039 | `					/* Integer */` |
|   19325 |  6040 | `					c = 'i';` |
|   19325 |  6041 | `					break;` |
|       2 |  6042 | `				case MEMOBJ_BOOL:` |
|       - |  6043 | `					/* Bool */` |
|       5 |  6044 | `					c = 'b';` |
|       5 |  6045 | `					break;` |
|       3 |  6046 | `				case MEMOBJ_REAL:` |
|       - |  6047 | `					/* Float */` |
|       8 |  6048 | `					c = 'f';` |
|       8 |  6049 | `					break;` |
|   24921 |  6050 | `				case MEMOBJ_STRING:` |
|       - |  6051 | `					/* String */` |
|   49847 |  6052 | `					c = 's';` |
|   49847 |  6053 | `					break;` |
|       7 |  6054 | `				case MEMOBJ_OBJ:` |
|       - |  6055 | `					/* Object */` |
|      16 |  6056 | `					c = 'o';` |
|      14 |  6057 | `					break;` |
|       1 |  6058 | `				default:` |
|       2 |  6059 | `					break;` |
|       - |  6060 | `				}` |
|   69201 |  6061 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  6062 | `			}` |
|   42296 |  6063 | `		}else{` |
|       - |  6064 | `			/* No type is associated with this parameter which mean` |
|       - |  6065 | `			 * that this function is not condidate for overloading.` |
|       - |  6066 | `			 */` |
|   93903 |  6067 | `			SyBlobRelease(&sSig);` |
|       - |  6068 | `		}` |
|       - |  6069 | `		/* Save in the argument set */` |
|  178485 |  6070 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  6071 | `	}` |
|  112571 |  6072 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  6073 | `		/* Save function signature */` |
|   53899 |  6074 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   26947 |  6075 | `	}` |
|  112571 |  6076 | `	return SXRET_OK;` |
|   56296 |  6077 | `}` |
|       - |  6078 | `/*` |
|       - |  6079 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|       - |  6080 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|       - |  6081 | ` * the enclosing function. Returns the token just past the nested construct.` |
|       - |  6082 | ` */` |
|      20 |  6083 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|       2 |  6084 | `{` |
|      22 |  6085 | `	sxi32 iParen = 0;` |
|      22 |  6086 | `	pIn++; /* past 'function'/'fn' */` |
|       - |  6087 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|       - |  6088 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|       - |  6089 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|      82 |  6090 | `	while( pIn < pEnd ){` |
|      82 |  6091 | `		sxu32 t = pIn->nType;` |
|      82 |  6092 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|      62 |  6093 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|      42 |  6094 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|      22 |  6095 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|      62 |  6096 | `		pIn++;` |
|       2 |  6097 | `	}` |
|      22 |  6098 | `	if( pIn >= pEnd ){ return pIn; }` |
|       - |  6099 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|       - |  6100 | `	{` |
|      22 |  6101 | `		sxi32 d = 0;` |
|     210 |  6102 | `		while( pIn < pEnd ){` |
|     210 |  6103 | `			sxu32 t = pIn->nType;` |
|     210 |  6104 | `			if( t & PH7_TK_OCB ){ d++; }` |
|     186 |  6105 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|     190 |  6106 | `			pIn++;` |
|       2 |  6107 | `		}` |
|       - |  6108 | `	}` |
|      22 |  6109 | `	return pIn;` |
|      12 |  6110 | `}` |
|       - |  6111 | `/*` |
|       - |  6112 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|       - |  6113 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|       - |  6114 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|       - |  6115 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|       - |  6116 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|       - |  6117 | ` * detached-mini-program path untouched.` |
|       - |  6118 | ` */` |
|       - |  6119 | `/*` |
|       - |  6120 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|       - |  6121 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|       - |  6122 | ` * mixed, object.` |
|       - |  6123 | ` */` |
|      28 |  6124 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|       3 |  6125 | `{` |
|       - |  6126 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|       - |  6127 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|       - |  6128 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|       - |  6129 | `	};` |
|       - |  6130 | `	sxu32 i;` |
|      31 |  6131 | `	if( nName > 0 && zName[0] == '\\' ){` |
|     ! 0 |  6132 | `		zName++;` |
|     ! 0 |  6133 | `		nName--;` |
|     ! 0 |  6134 | `	}` |
|      63 |  6135 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|      59 |  6136 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|      27 |  6137 | `			return 1;` |
|       - |  6138 | `		}` |
|      17 |  6139 | `	}` |
|       5 |  6140 | `	return 0;` |
|      17 |  6141 | `}` |
|       - |  6142 | `/*` |
|       - |  6143 | ` * One atom of a generator's declared return type: is it a supertype of` |
|       - |  6144 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|       - |  6145 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|       - |  6146 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|       - |  6147 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|       - |  6148 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|       - |  6149 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|       - |  6150 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|       - |  6151 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|       - |  6152 | ` * both rather than fatal on valid code (divergence recorded in PLAN.md).` |
|       - |  6153 | ` */` |
|      26 |  6154 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|       4 |  6155 | `{` |
|      30 |  6156 | `	if( nType == MEMOBJ_OBJ ){` |
|     ! 0 |  6157 | ``		return 1; /* bare `object` */`` |
|       - |  6158 | `	}` |
|      30 |  6159 | `	if( nType != SXU32_HIGH ){` |
|       3 |  6160 | `		return 0; /* scalar/array/void/never/null/... */` |
|       - |  6161 | `	}` |
|      27 |  6162 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|      23 |  6163 | `		return 1;` |
|       - |  6164 | `	}` |
|       - |  6165 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|       - |  6166 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|       - |  6167 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|       - |  6168 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|       - |  6169 | `	{` |
|       - |  6170 | `		SyBlob sFQN;` |
|       - |  6171 | `		int bOk;` |
|       5 |  6172 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       5 |  6173 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|       5 |  6174 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|       5 |  6175 | `		SyBlobRelease(&sFQN);` |
|       5 |  6176 | `		return bOk;` |
|       - |  6177 | `	}` |
|      17 |  6178 | `}` |
|       - |  6179 | `/*` |
|       - |  6180 | ` * php 8: a generator function may only declare a return type that is a` |
|       - |  6181 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|       - |  6182 | ` * group qualifies only if every member does. Anything else is php's exact` |
|       - |  6183 | ` * compile-time fatal "Generator return type must be a supertype of` |
|       - |  6184 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|       - |  6185 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|       - |  6186 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|       - |  6187 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|       - |  6188 | ` */` |
|     212 |  6189 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|       5 |  6190 | `{` |
|     217 |  6191 | `	int bOk = 0;` |
|       - |  6192 | `	sxu32 nLine;` |
|       - |  6193 | `	sxi32 rc;` |
|     217 |  6194 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|     191 |  6195 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|       - |  6196 | `	}` |
|      30 |  6197 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|     ! 0 |  6198 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|     ! 0 |  6199 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|       - |  6200 | `		sxu32 i,j;` |
|     ! 0 |  6201 | `		for( i = 0; i < n && !bOk; i++ ){` |
|       - |  6202 | `			int bGroupOk;` |
|     ! 0 |  6203 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|     ! 0 |  6204 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|       - |  6205 | `			}` |
|     ! 0 |  6206 | `			bGroupOk = 1;` |
|     ! 0 |  6207 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|     ! 0 |  6208 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|     ! 0 |  6209 | `					bGroupOk = 0;` |
|     ! 0 |  6210 | `					break;` |
|       - |  6211 | `				}` |
|     ! 0 |  6212 | `			}` |
|     ! 0 |  6213 | `			bOk = bGroupOk;` |
|     ! 0 |  6214 | `		}` |
|     ! 0 |  6215 | `	}else{` |
|      30 |  6216 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|       - |  6217 | `	}` |
|      30 |  6218 | `	if( bOk ){` |
|      27 |  6219 | `		return SXRET_OK;` |
|       - |  6220 | `	}` |
|       - |  6221 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|       - |  6222 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|       - |  6223 | `	 * token of this stream — its line is the function's closing brace. php` |
|       - |  6224 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|       - |  6225 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|       3 |  6226 | `	nLine = pGen->pIn[-1].nLine;` |
|       - |  6227 | `	{` |
|       3 |  6228 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|       3 |  6229 | `		if( sGiven.nByte < 1 ){` |
|     ! 0 |  6230 | `			sGiven = pFunc->sReturnClass;` |
|     ! 0 |  6231 | `		}` |
|       3 |  6232 | `		if( sGiven.nByte < 1 ){` |
|       - |  6233 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|       - |  6234 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|       - |  6235 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|     ! 0 |  6236 | `			const char *zScalar =` |
|     ! 0 |  6237 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|     ! 0 |  6238 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|     ! 0 |  6239 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|     ! 0 |  6240 | `		}` |
|       3 |  6241 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  6242 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|       - |  6243 | `	}` |
|       3 |  6244 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|     111 |  6245 | `}` |
|  240170 |  6246 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|       5 |  6247 | `{` |
|  240175 |  6248 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|  240175 |  6249 | `	SyToken *pEnd = pGen->pEnd;` |
|  240175 |  6250 | `	sxi32 iDepth = 0;` |
|  240175 |  6251 | `	int bStarted = 0;` |
| 7975787 |  6252 | `	while( pIn < pEnd ){` |
| 7975787 |  6253 | `		sxu32 t = pIn->nType;` |
| 7975787 |  6254 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 7516399 |  6255 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 7057327 |  6256 | `		if( t & PH7_TK_KEYWORD ){` |
|  559811 |  6257 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  559811 |  6258 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  559599 |  6259 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|       - |  6260 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|  279787 |  6261 | `		}` |
| 7057095 |  6262 | `		pIn++;` |
|       5 |  6263 | `	}` |
|  239963 |  6264 | `	return FALSE;` |
|  120090 |  6265 | `}` |
|       - |  6266 | `/*` |
|       - |  6267 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  6268 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  6269 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  6270 | ` */` |
|  240170 |  6271 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  6272 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  6273 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  6274 | `	)` |
|       5 |  6275 | `{` |
|       - |  6276 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  6277 | `	GenBlock *pBlock;` |
|       - |  6278 | `	sxu32 nGotoOfft;` |
|       - |  6279 | `	sxi32 rc;` |
|       - |  6280 | `	/* Attach the new function */` |
|  240175 |  6281 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  240175 |  6282 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6283 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  6284 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6285 | `		return SXERR_ABORT;` |
|       - |  6286 | `	}` |
|  240175 |  6287 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  6288 | `	/* Swap bytecode containers */` |
|  240175 |  6289 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  240175 |  6290 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  6291 | `	/* Emit constructor property promotion prologue:` |
|       - |  6292 | `	 *   $this->NAME = $NAME;` |
|       - |  6293 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  6294 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  6295 | `	{` |
|  240175 |  6296 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  6297 | `		sxu32 i;` |
|  387867 |  6298 | `		for( i = 0; i < nArg; i++ ){` |
|  147697 |  6299 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  6300 | `			char *zSrc;` |
|       - |  6301 | `			sxu32 nSrc,nName;` |
|       - |  6302 | `			SySet sToken;` |
|       - |  6303 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  6304 | `			sxi32 rcPromote;` |
|  147697 |  6305 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  147641 |  6306 | `				continue;` |
|       - |  6307 | `			}` |
|       - |  6308 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  6309 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  6310 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  6311 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  6312 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      61 |  6313 | `			nName = SyStringLength(&pArg->sName);` |
|      61 |  6314 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      61 |  6315 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      61 |  6316 | `			if( zSrc == 0 ){` |
|     ! 0 |  6317 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6318 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6319 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  6320 | `				return SXERR_ABORT;` |
|       - |  6321 | `			}` |
|       - |  6322 | `			{` |
|      61 |  6323 | `				char *z = zSrc;` |
|      61 |  6324 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      61 |  6325 | `				z += sizeof("$this->")-1;` |
|      61 |  6326 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      61 |  6327 | `				z += nName;` |
|      61 |  6328 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      61 |  6329 | `				z += sizeof(" = $")-1;` |
|      61 |  6330 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      61 |  6331 | `				z += nName;` |
|      61 |  6332 | `				*z = 0;` |
|       - |  6333 | `			}` |
|      61 |  6334 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      61 |  6335 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      61 |  6336 | `			pTmpIn = pGen->pIn;` |
|      61 |  6337 | `			pTmpEnd = pGen->pEnd;` |
|      61 |  6338 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      61 |  6339 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      61 |  6340 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      61 |  6341 | `			pGen->pIn = pTmpIn;` |
|      61 |  6342 | `			pGen->pEnd = pTmpEnd;` |
|      61 |  6343 | `			SySetRelease(&sToken);` |
|      61 |  6344 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  6345 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6346 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6347 | `				return SXERR_ABORT;` |
|       - |  6348 | `			}` |
|       - |  6349 | `			/* Discard the assignment result — this is a statement expression. */` |
|      61 |  6350 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      33 |  6351 | `		}` |
|       - |  6352 | `	}` |
|       - |  6353 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|       - |  6354 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|       - |  6355 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|       - |  6356 | `	 * generator — and vice versa — is classified independently. */` |
|       - |  6357 | `	{` |
|  240175 |  6358 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|  240175 |  6359 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|       - |  6360 | `		/* Compile the body */` |
|  240175 |  6361 | `		PH7_CompileBlock(&(*pGen),0);` |
|  240175 |  6362 | `		pGen->bInGenerator = bSavedGen;` |
|       - |  6363 | `	}` |
|       - |  6364 | `	/* Fix exception jumps now the destination is resolved */` |
|  240175 |  6365 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  6366 | `	/* Emit the final return if not yet done */` |
|  240175 |  6367 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  6368 | `	/* Fix gotos jumps now the destination is resolved */` |
|  240175 |  6369 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  6370 | `		rc = SXERR_ABORT;` |
|     ! 0 |  6371 | `	}` |
|  240175 |  6372 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  6373 | `	/* Restore the default container */` |
|  240175 |  6374 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  6375 | `	/* Leave function block */` |
|  240175 |  6376 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  240175 |  6377 | `	if( rc == SXERR_ABORT ){` |
|       - |  6378 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6379 | `		return SXERR_ABORT;` |
|       - |  6380 | `	}` |
|       - |  6381 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  6382 | `	{` |
|  240175 |  6383 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  6384 | `		sxu32 i;` |
| 4716377 |  6385 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 4476419 |  6386 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     217 |  6387 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     217 |  6388 | `				break;` |
|       - |  6389 | `			}` |
| 2238106 |  6390 | `		}` |
|       - |  6391 | `	}` |
|  240175 |  6392 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|       - |  6393 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     217 |  6394 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|     ! 0 |  6395 | `			return SXERR_ABORT;` |
|       - |  6396 | `		}` |
|     106 |  6397 | `	}` |
|       - |  6398 | `	/* All done, function body compiled */` |
|  240175 |  6399 | `	return SXRET_OK;` |
|  120090 |  6400 | `}` |
|       - |  6401 | `/*` |
|       - |  6402 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  6403 | ` * According to the PHP language reference manual.` |
|       - |  6404 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  6405 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  6406 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  6407 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  6408 | ` *  Functions need not be defined before they are referenced.` |
|       - |  6409 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  6410 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  6411 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  6412 | ` *  calls with over 32-64 recursion levels.` |
|       - |  6413 | ` *` |
|       - |  6414 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  6415 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  6416 | ` * on these extension.` |
|       - |  6417 | ` */` |
|       - |  6418 | `/*` |
|       - |  6419 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  6420 | ` */` |
|     522 |  6421 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  6422 | `{` |
|       - |  6423 | `	sxu32 i;` |
|    1481 |  6424 | `	for( i = 0; i < n; i++ ){` |
|    1271 |  6425 | `		int a = zA[i], b = zB[i];` |
|    1271 |  6426 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|    1271 |  6427 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|    1271 |  6428 | `		if( a != b ) return a - b;` |
|     482 |  6429 | `	}` |
|     215 |  6430 | `	return 0;` |
|     266 |  6431 | `}` |
|       - |  6432 | `/*` |
|       - |  6433 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  6434 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  6435 | ` * (which are positive bit values stored in sxu32).` |
|       - |  6436 | ` */` |
|       - |  6437 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  6438 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  6439 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  6440 |  |
|       - |  6441 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|       - |  6442 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|       - |  6443 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|       - |  6444 |  |
|       - |  6445 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  6446 | `struct PhlTypeAtom {` |
|       - |  6447 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  6448 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  6449 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  6450 | `	sxu32 nCanon;` |
|       - |  6451 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|       - |  6452 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|       - |  6453 | `};` |
|       - |  6454 |  |
|       - |  6455 | `/*` |
|       - |  6456 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  6457 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  6458 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  6459 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  6460 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  6461 | ` * already be consumed by the caller.` |
|       - |  6462 | ` */` |
|   85598 |  6463 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  6464 | `{` |
|   85603 |  6465 | `	SyToken *pIn = pGen->pIn;` |
|   85603 |  6466 | `	SyZero(pOut, sizeof(*pOut));` |
|   85603 |  6467 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   85603 |  6468 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6469 | `		return SXERR_SYNTAX;` |
|       - |  6470 | `	}` |
|       - |  6471 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   85603 |  6472 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  6473 | `		pIn++;` |
|       8 |  6474 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6475 | `			return SXERR_SYNTAX;` |
|       - |  6476 | `		}` |
|       3 |  6477 | `	}` |
|   85603 |  6478 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6479 | `		return SXERR_SYNTAX;` |
|       - |  6480 | `	}` |
|   85603 |  6481 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   69797 |  6482 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   69797 |  6483 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      34 |  6484 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   69782 |  6485 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      73 |  6486 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   69733 |  6487 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   19611 |  6488 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   59896 |  6489 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   50021 |  6490 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   25085 |  6491 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      35 |  6492 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      61 |  6493 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  6494 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      33 |  6495 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|      10 |  6496 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      19 |  6497 | `			pOut->nType = SXU32_HIGH;` |
|      19 |  6498 | `			pOut->sClass = pIn->sData;` |
|      11 |  6499 | `		}else{` |
|       3 |  6500 | `			return SXERR_SYNTAX;` |
|       - |  6501 | `		}` |
|   69795 |  6502 | `		pIn++;` |
|   34900 |  6503 | `	}else{` |
|       - |  6504 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6505 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   15811 |  6506 | `		SyString *pT = &pIn->sData;` |
|   15811 |  6507 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      32 |  6508 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      32 |  6509 | `			pIn++;` |
|   15797 |  6510 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     161 |  6511 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     161 |  6512 | `			pIn++;` |
|   15705 |  6513 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|      24 |  6514 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|      24 |  6515 | `			pIn++;` |
|      14 |  6516 | `		}else{` |
|       - |  6517 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   15607 |  6518 | `			SyToken *pFirst = pIn;` |
|   15607 |  6519 | `			SyToken *pLast = pIn;` |
|   15607 |  6520 | `			pOut->nType = SXU32_HIGH;` |
|   15607 |  6521 | `			pOut->sClass = pIn->sData;` |
|   15607 |  6522 | `			pIn++;` |
|   23406 |  6523 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   15610 |  6524 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6525 | `				pLast = &pIn[1];` |
|       3 |  6526 | `				pIn += 2;` |
|       1 |  6527 | `			}` |
|   15607 |  6528 | `			if( pLast != pFirst ){` |
|       3 |  6529 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6530 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6531 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6532 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6533 | `			}` |
|       - |  6534 | `		}` |
|       - |  6535 | `	}` |
|   85601 |  6536 | `	pGen->pIn = pIn;` |
|   85601 |  6537 | `	return SXRET_OK;` |
|   42804 |  6538 | `}` |
|       - |  6539 |  |
|       - |  6540 | `/*` |
|       - |  6541 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6542 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6543 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6544 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6545 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6546 | ` */` |
|   85432 |  6547 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6548 | `{` |
|       - |  6549 | `	int i;` |
|   85437 |  6550 | `	int nNonNull = 0;` |
|   85437 |  6551 | `	int bAnyIntersection = 0;` |
|       - |  6552 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   85437 |  6553 | `	sxu32 nMaxGroup = 0;` |
| 2819261 |  6554 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  171009 |  6555 | `	for( i = 0; i < nAtoms; i++ ){` |
|   85577 |  6556 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   85549 |  6557 | `			nNonNull++;` |
|   85549 |  6558 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   85549 |  6559 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   85549 |  6560 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   42772 |  6561 | `			}` |
|   42772 |  6562 | `		}` |
|   42791 |  6563 | `	}` |
|  170967 |  6564 | `	for( i = 0; i < nAtoms; i++ ){` |
|   85555 |  6565 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      24 |  6566 | `			bAnyIntersection = 1;` |
|      24 |  6567 | `			break;` |
|       - |  6568 | `		}` |
|   42770 |  6569 | `	}` |
|   85437 |  6570 | `	if( bAnyIntersection ){` |
|       - |  6571 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|       - |  6572 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|       - |  6573 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|      24 |  6574 | `		sxu32 g, nGroups = 0;` |
|      24 |  6575 | `		int bFirstGroup = 1;` |
|      48 |  6576 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|      48 |  6577 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|      28 |  6578 | `			int bFirstMember = 1;` |
|       - |  6579 | `			int bWrap;` |
|      28 |  6580 | `			if( aGroupCount[g] == 0 ) continue;` |
|       - |  6581 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|       - |  6582 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|       - |  6583 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|       - |  6584 | `			 * parens, matching PHP's canonical text. */` |
|      38 |  6585 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|      28 |  6586 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|      28 |  6587 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      84 |  6588 | `			for( i = 0; i < nAtoms; i++ ){` |
|      60 |  6589 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|      48 |  6590 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|      48 |  6591 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      46 |  6592 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      25 |  6593 | `				}else{` |
|       3 |  6594 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6595 | `				}` |
|      48 |  6596 | `				bFirstMember = 0;` |
|      26 |  6597 | `			}` |
|      28 |  6598 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|      28 |  6599 | `			bFirstGroup = 0;` |
|      16 |  6600 | `		}` |
|      24 |  6601 | `		if( bNullable ){` |
|     ! 0 |  6602 | `			SyBlobAppend(pBlob, "\|", 1);` |
|     ! 0 |  6603 | `			SyBlobAppend(pBlob, "null", 4);` |
|     ! 0 |  6604 | `		}` |
|      64 |  6605 | `		return;` |
|       - |  6606 | `	}` |
|   85417 |  6607 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6608 | `		/* Shorthand: ?T */` |
|      85 |  6609 | `		for( i = 0; i < nAtoms; i++ ){` |
|      85 |  6610 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      85 |  6611 | `			SyBlobAppend(pBlob, "?", 1);` |
|      85 |  6612 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      21 |  6613 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      12 |  6614 | `			}else{` |
|      67 |  6615 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6616 | `			}` |
|      85 |  6617 | `			return;` |
|     ! 0 |  6618 | `		}` |
|     ! 0 |  6619 | `	}` |
|       - |  6620 | `	{` |
|   85337 |  6621 | `		int bFirst = 1;` |
|       - |  6622 | `		/* 1) Classes in declaration order */` |
|  170773 |  6623 | `		for( i = 0; i < nAtoms; i++ ){` |
|   85441 |  6624 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   15563 |  6625 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   15563 |  6626 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   15563 |  6627 | `				bFirst = 0;` |
|    7779 |  6628 | `			}` |
|   42723 |  6629 | `		}` |
|       - |  6630 | `		/* 2) Built-ins in canonical order */` |
|       - |  6631 | `		{` |
|       - |  6632 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6633 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6634 | `			int k;` |
|  597329 |  6635 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  954811 |  6636 | `				for( i = 0; i < nAtoms; i++ ){` |
|  512513 |  6637 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   69699 |  6638 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   69699 |  6639 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   69699 |  6640 | `						bFirst = 0;` |
|   69699 |  6641 | `						break;` |
|       - |  6642 | `					}` |
|  221412 |  6643 | `				}` |
|  256001 |  6644 | `			}` |
|       - |  6645 | `		}` |
|       - |  6646 | `		/* 3) null suffix */` |
|   85337 |  6647 | `		if( bNullable ){` |
|      19 |  6648 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      19 |  6649 | `			SyBlobAppend(pBlob, "null", 4);` |
|       8 |  6650 | `		}` |
|       - |  6651 | `	}` |
|   42721 |  6652 | `}` |
|       - |  6653 |  |
|       - |  6654 | `/*` |
|       - |  6655 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|       - |  6656 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|       - |  6657 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|       - |  6658 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|       - |  6659 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|       - |  6660 | ` * whether it was parenthesized.` |
|       - |  6661 | ` *` |
|       - |  6662 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|       - |  6663 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|       - |  6664 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|       - |  6665 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|       - |  6666 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|       - |  6667 | ` */` |
|   85576 |  6668 | `static sxi32 GenStateParsePart(` |
|       - |  6669 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|       - |  6670 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|       5 |  6671 | `{` |
|       - |  6672 | `	sxi32 rc;` |
|   85581 |  6673 | `	int nMembers = 0;` |
|   85581 |  6674 | `	int bParen = 0;` |
|   85581 |  6675 | `	*pnMembers = 0;` |
|   85581 |  6676 | `	*pbParen = 0;` |
|   85581 |  6677 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       6 |  6678 | `		bParen = 1;` |
|       6 |  6679 | `		pGen->pIn++; /* skip '(' */` |
|       2 |  6680 | `	}` |
|   42788 |  6681 | `	for(;;){` |
|   85603 |  6682 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6683 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6684 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6685 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6686 | `		}` |
|   85603 |  6687 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   85603 |  6688 | `		if( rc != SXRET_OK ){` |
|       3 |  6689 | `			return rc;` |
|       - |  6690 | `		}` |
|   85601 |  6691 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   85601 |  6692 | `		(*pnAtoms)++;` |
|   85601 |  6693 | `		nMembers++;` |
|       - |  6694 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   85601 |  6695 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      30 |  6696 | `			SyToken *pNext = &pGen->pIn[1];` |
|      26 |  6697 | `			if( pNext < pGen->pEnd` |
|      30 |  6698 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      26 |  6699 | `				pGen->pIn++; /* skip '&' */` |
|      26 |  6700 | `				continue;` |
|       - |  6701 | `			}` |
|       2 |  6702 | `		}` |
|   85579 |  6703 | `		break;` |
|     ! 0 |  6704 | `	}` |
|   85579 |  6705 | `	if( bParen ){` |
|       6 |  6706 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6707 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6708 | `				"Malformed DNF type: expecting ')'");` |
|     ! 0 |  6709 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6710 | `		}` |
|       6 |  6711 | `		pGen->pIn++; /* skip ')' */` |
|       6 |  6712 | `		if( nMembers < 2 ){` |
|     ! 0 |  6713 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6714 | `				"Parenthesized type must be an intersection of at least two types");` |
|     ! 0 |  6715 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6716 | `		}` |
|       2 |  6717 | `	}` |
|   85579 |  6718 | `	*pnMembers = nMembers;` |
|   85579 |  6719 | `	*pbParen = bParen;` |
|   85579 |  6720 | `	return SXRET_OK;` |
|   42793 |  6721 | `}` |
|       - |  6722 |  |
|       - |  6723 | `/*` |
|       - |  6724 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6725 | ` *` |
|       - |  6726 | ` * Outputs:` |
|       - |  6727 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6728 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6729 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6730 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6731 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6732 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6733 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6734 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6735 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6736 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6737 | ` *` |
|       - |  6738 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6739 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6740 | ` */` |
|   85448 |  6741 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6742 | `	ph7_gen_state *pGen,` |
|       - |  6743 | `	sxu32 *pnType,` |
|       - |  6744 | `	SyString *pClass,` |
|       - |  6745 | `	SySet *pAlts,` |
|       - |  6746 | `	sxi32 *piTypeFlags,` |
|       - |  6747 | `	SyString *pTypeText,` |
|       - |  6748 | `	int iNullableFlag,` |
|       - |  6749 | `	int iUnionFlag,` |
|       - |  6750 | `	int bAllowVoid,` |
|       - |  6751 | `	sxu32 nLine` |
|       5 |  6752 | `){` |
|       - |  6753 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   85453 |  6754 | `	int nAtoms = 0;` |
|   85453 |  6755 | `	int bShortNullable = 0;` |
|   85453 |  6756 | `	int bExplicitNull = 0;` |
|       - |  6757 | `	sxi32 rc;` |
|   85453 |  6758 | `	*pnType = 0;` |
|   85453 |  6759 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   85453 |  6760 | `	*piTypeFlags = 0;` |
|   85453 |  6761 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6762 |  |
|   85453 |  6763 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6764 | `		return SXRET_OK;` |
|       - |  6765 | `	}` |
|       - |  6766 | ``	/* Optional `?` shorthand prefix */`` |
|   85448 |  6767 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      75 |  6768 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      75 |  6769 | `		bShortNullable = 1;` |
|      75 |  6770 | `		pGen->pIn++;` |
|      75 |  6771 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6772 | `			return SXERR_SYNTAX;` |
|       - |  6773 | `		}` |
|      35 |  6774 | `	}` |
|       - |  6775 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|       - |  6776 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|       - |  6777 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|       - |  6778 | `	{` |
|       - |  6779 | `		int nMembers, bParen;` |
|   85453 |  6780 | `		sxu32 iGroup = 0;` |
|   85453 |  6781 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   85453 |  6782 | `		if( rc != SXRET_OK ){` |
|       4 |  6783 | `			return rc;` |
|       - |  6784 | `		}` |
|       - |  6785 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|       - |  6786 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|       - |  6787 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|       - |  6788 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|       - |  6789 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|  128364 |  6790 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   85646 |  6791 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     135 |  6792 | `			if( bShortNullable ){` |
|       - |  6793 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6794 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6795 | `				 * already reported" so callers skip their own error emission. */` |
|       3 |  6796 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6797 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6798 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6799 | `			}` |
|     133 |  6800 | `			if( nMembers >= 2 && !bParen ){` |
|     ! 0 |  6801 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|       - |  6802 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6803 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6804 | `			}` |
|     133 |  6805 | ``			pGen->pIn++; /* skip `\|` */`` |
|     133 |  6806 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|     133 |  6807 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  6808 | `				return rc;` |
|       - |  6809 | `			}` |
|       5 |  6810 | `		}` |
|   85449 |  6811 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|     ! 0 |  6812 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6813 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6814 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6815 | `		}` |
|       - |  6816 | `	}` |
|       - |  6817 | `	/* Validation pass.` |
|       - |  6818 | `	 *` |
|       - |  6819 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6820 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6821 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6822 | `	 */` |
|       - |  6823 | `	{` |
|       - |  6824 | `		int i, j;` |
|   85449 |  6825 | `		int bHasNonNull = 0;` |
|   85449 |  6826 | `		int bAnyIntersection = 0;` |
|       - |  6827 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|       - |  6828 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|       - |  6829 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 2819657 |  6830 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  171043 |  6831 | `		for( i = 0; i < nAtoms; i++ ){` |
|   85599 |  6832 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   42802 |  6833 | `		}` |
|  170997 |  6834 | `		for( i = 0; i < nAtoms; i++ ){` |
|   85575 |  6835 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   42779 |  6836 | `		}` |
|       - |  6837 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|       - |  6838 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   85449 |  6839 | `		if( bShortNullable && bAnyIntersection ){` |
|     ! 0 |  6840 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6841 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|     ! 0 |  6842 | `			return SXERR_SYNTAX;` |
|       - |  6843 | `		}` |
|  171029 |  6844 | `		for( i = 0; i < nAtoms; i++ ){` |
|       - |  6845 | `			/* Intersection members must be class/interface types (PHP rejects` |
|       - |  6846 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|       - |  6847 | ``			 * `true`/`false` in an intersection). */`` |
|   85597 |  6848 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      46 |  6849 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|      46 |  6850 | `				if( bClassLike ){` |
|      44 |  6851 | `					SyString *pC = &aAtoms[i].sClass;` |
|      40 |  6852 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|      40 |  6853 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|      40 |  6854 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|      44 |  6855 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|     ! 0 |  6856 | `						bClassLike = 0;` |
|     ! 0 |  6857 | `					}` |
|      20 |  6858 | `				}` |
|      46 |  6859 | `				if( !bClassLike ){` |
|       - |  6860 | `					const char *zName; sxu32 nName;` |
|       3 |  6861 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6862 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6863 | `					}else{` |
|       3 |  6864 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|       - |  6865 | `					}` |
|       4 |  6866 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6867 | `						"Type %.*s cannot be part of an intersection type",` |
|       1 |  6868 | `						(int)nName, zName);` |
|       3 |  6869 | `					return SXERR_SYNTAX;` |
|       - |  6870 | `				}` |
|      20 |  6871 | `			}` |
|   85595 |  6872 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     161 |  6873 | `				if( nAtoms > 1 ){` |
|       3 |  6874 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6875 | `						"Void can only be used as a standalone type");` |
|       3 |  6876 | `					return SXERR_SYNTAX;` |
|       - |  6877 | `				}` |
|     159 |  6878 | `				if( !bAllowVoid ){` |
|     ! 0 |  6879 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6880 | `						"void cannot be used here");` |
|     ! 0 |  6881 | `					return SXERR_SYNTAX;` |
|       - |  6882 | `				}` |
|     159 |  6883 | `				if( bShortNullable ){` |
|     ! 0 |  6884 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6885 | `						"Void type cannot be nullable");` |
|     ! 0 |  6886 | `					return SXERR_SYNTAX;` |
|       - |  6887 | `				}` |
|      77 |  6888 | `			}` |
|   85593 |  6889 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6890 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|       - |  6891 | `				 * type (never = the function does not return). Mirrors the void` |
|       - |  6892 | `				 * validation above; accepted here and enforced at compile time` |
|       - |  6893 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|      24 |  6894 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|       - |  6895 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|       - |  6896 | `					 * same as any other non-standalone use. */` |
|       5 |  6897 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6898 | `						"never can only be used as a standalone type");` |
|       5 |  6899 | `					return SXERR_SYNTAX;` |
|       - |  6900 | `				}` |
|      19 |  6901 | `				if( !bAllowVoid ){` |
|       - |  6902 | `					/* Return-only: params call with bAllowVoid=0. */` |
|       3 |  6903 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6904 | `						"never cannot be used as a parameter type");` |
|       3 |  6905 | `					return SXERR_SYNTAX;` |
|       - |  6906 | `				}` |
|       7 |  6907 | `			}` |
|   85587 |  6908 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      32 |  6909 | `				bExplicitNull = 1;` |
|      18 |  6910 | `			}else{` |
|   85559 |  6911 | `				bHasNonNull = 1;` |
|       - |  6912 | `			}` |
|       - |  6913 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|       - |  6914 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|       - |  6915 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|       - |  6916 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|       - |  6917 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   85773 |  6918 | `			for( j = 0; j < i; j++ ){` |
|     193 |  6919 | `				int bDup = 0;` |
|     193 |  6920 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|     369 |  6921 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|     188 |  6922 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|     193 |  6923 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|     185 |  6924 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      47 |  6925 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      40 |  6926 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      42 |  6927 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|      17 |  6928 | `								aAtoms[j].sClass.zString,` |
|      34 |  6929 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6930 | `							bDup = 1;` |
|     ! 0 |  6931 | `						}` |
|      25 |  6932 | `					}else{` |
|       3 |  6933 | `						bDup = 1;` |
|       - |  6934 | `					}` |
|      21 |  6935 | `				}` |
|     185 |  6936 | `				if( bDup ){` |
|       - |  6937 | `					const char *zName;` |
|       - |  6938 | `					sxu32 nName;` |
|       3 |  6939 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6940 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6941 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6942 | `					}else{` |
|       3 |  6943 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6944 | `						nName = aAtoms[i].nCanon;` |
|       - |  6945 | `					}` |
|       4 |  6946 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6947 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6948 | `					return SXERR_SYNTAX;` |
|       - |  6949 | `				}` |
|      94 |  6950 | `			}` |
|   42795 |  6951 | `		}` |
|   85437 |  6952 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6953 | `			if( bShortNullable ){` |
|       - |  6954 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6955 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6956 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6957 | `				return SXERR_SYNTAX;` |
|       - |  6958 | `			}` |
|       - |  6959 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6960 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6961 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6962 | `			 * atom, so set it here. */` |
|       7 |  6963 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6964 | `		}` |
|       - |  6965 | `	}` |
|       - |  6966 | `	/* Compute nullability flag */` |
|   85437 |  6967 | `	if( bShortNullable \|\| bExplicitNull ){` |
|     101 |  6968 | `		*piTypeFlags \|= iNullableFlag;` |
|      48 |  6969 | `	}` |
|       - |  6970 | `	/* Build canonical type text */` |
|   85437 |  6971 | `	if( pTypeText ){` |
|       - |  6972 | `		SyBlob sBlob;` |
|   85437 |  6973 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  128119 |  6974 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   42716 |  6975 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   85437 |  6976 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  127901 |  6977 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   85264 |  6978 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   85269 |  6979 | `			if( zDup ){` |
|   85269 |  6980 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   42632 |  6981 | `			}` |
|   42632 |  6982 | `		}` |
|   85437 |  6983 | `		SyBlobRelease(&sBlob);` |
|   42716 |  6984 | `	}` |
|       - |  6985 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6986 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6987 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6988 | `	{` |
|   85437 |  6989 | `		int nNonNull = 0;` |
|   85437 |  6990 | `		int iNonNullIdx = -1;` |
|       - |  6991 | `		int i;` |
|  171009 |  6992 | `		for( i = 0; i < nAtoms; i++ ){` |
|   85577 |  6993 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   85549 |  6994 | `				nNonNull++;` |
|   85549 |  6995 | `				iNonNullIdx = i;` |
|   42772 |  6996 | `			}` |
|   42791 |  6997 | `		}` |
|   85437 |  6998 | `		if( nNonNull <= 1 ){` |
|       - |  6999 | `			/* Fast path: store as single type. */` |
|   85339 |  7000 | `			if( iNonNullIdx >= 0 ){` |
|   85333 |  7001 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   85333 |  7002 | `				if( pA->nType == SXU32_HIGH ){` |
|   23303 |  7003 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    7766 |  7004 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   15537 |  7005 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   15537 |  7006 | `					*pnType = SXU32_HIGH;` |
|   15537 |  7007 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   77567 |  7008 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     159 |  7009 | `					*pnType = MEMOBJ_VOID;` |
|   69724 |  7010 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|      16 |  7011 | `					*pnType = MEMOBJ_NEVER;` |
|       9 |  7012 | `				}else{` |
|   69633 |  7013 | `					*pnType = pA->nType;` |
|       - |  7014 | `				}` |
|   42664 |  7015 | `			}` |
|   42672 |  7016 | `		}else{` |
|       - |  7017 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|     103 |  7018 | `			*piTypeFlags \|= iUnionFlag;` |
|     329 |  7019 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  7020 | `				ph7_type_alt sAlt;` |
|     231 |  7021 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     221 |  7022 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     221 |  7023 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|     221 |  7024 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|     134 |  7025 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      43 |  7026 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      91 |  7027 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      91 |  7028 | `					sAlt.nType = SXU32_HIGH;` |
|      91 |  7029 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      48 |  7030 | `				}else{` |
|     135 |  7031 | `					sAlt.nType = aAtoms[i].nType;` |
|     135 |  7032 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  7033 | `				}` |
|     221 |  7034 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|     113 |  7035 | `			}` |
|       - |  7036 | `		}` |
|       - |  7037 | `	}` |
|   85437 |  7038 | `	return SXRET_OK;` |
|   42729 |  7039 | `}` |
|       - |  7040 |  |
|       - |  7041 | `/*` |
|       - |  7042 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  7043 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  7044 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  7045 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  7046 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  7047 | `` *          and union types `: T\|U`.`` |
|       - |  7048 | ` */` |
|  340006 |  7049 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  7050 | `{` |
|  340011 |  7051 | `	sxi32 iFlags = 0;` |
|       - |  7052 | `	sxi32 rc;` |
|       - |  7053 | `	sxu32 nLine;` |
|  340011 |  7054 | `	pFunc->nReturnType = 0;` |
|  340011 |  7055 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  340011 |  7056 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|       - |  7057 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|       - |  7058 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|       - |  7059 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|       - |  7060 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|       - |  7061 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|  340011 |  7062 | `	SySetReset(&pFunc->aReturnUnion);` |
|  340011 |  7063 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|  340011 |  7064 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  339435 |  7065 | `		return SXRET_OK;` |
|       - |  7066 | `	}` |
|     581 |  7067 | `	pGen->pIn++; /* Skip ':' */` |
|     581 |  7068 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7069 | `		return SXRET_OK;` |
|       - |  7070 | `	}` |
|     581 |  7071 | `	nLine = pGen->pIn->nLine;` |
|     581 |  7072 | `	rc = GenStateParseUnionTypeDecl(` |
|     288 |  7073 | `		pGen,` |
|     288 |  7074 | `		&pFunc->nReturnType,` |
|     288 |  7075 | `		&pFunc->sReturnClass,` |
|     288 |  7076 | `		&pFunc->aReturnUnion,` |
|       - |  7077 | `		&iFlags,` |
|     288 |  7078 | `		&pFunc->sReturnTypeName,` |
|       - |  7079 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|       - |  7080 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|       - |  7081 | `		/* iUnionFlag */ 0,` |
|       - |  7082 | `		/* bAllowVoid */ 1,` |
|     288 |  7083 | `		nLine);` |
|     581 |  7084 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7085 | `		return SXERR_ABORT;` |
|       - |  7086 | `	}` |
|     581 |  7087 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  7088 | `		/* Error already reported */` |
|     ! 0 |  7089 | `		return SXERR_SYNTAX;` |
|       - |  7090 | `	}` |
|     581 |  7091 | `	if( rc == SXERR_SYNTAX ){` |
|       8 |  7092 | `		if( pGen->pIn < pGen->pEnd ){` |
|      11 |  7093 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  7094 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       6 |  7095 | `				&pGen->pIn->sData);` |
|       5 |  7096 | `		}else{` |
|     ! 0 |  7097 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  7098 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  7099 | `		}` |
|       8 |  7100 | `		return SXERR_SYNTAX;` |
|       - |  7101 | `	}` |
|     575 |  7102 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     575 |  7103 | `	return SXRET_OK;` |
|  170008 |  7104 | `}` |
|       - |  7105 |  |
|   51406 |  7106 | `static sxi32 GenStateCompileFunc(` |
|       - |  7107 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7108 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  7109 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  7110 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  7111 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  7112 | `	)` |
|       5 |  7113 | `{` |
|       - |  7114 | `	ph7_vm_func *pFunc;` |
|       - |  7115 | `	SyToken *pEnd;` |
|       - |  7116 | `	sxu32 nLine;` |
|       - |  7117 | `	char *zName;` |
|       - |  7118 | `	sxi32 rc;` |
|       - |  7119 | `	/* Extract line number */` |
|   51411 |  7120 | `	nLine = pGen->pIn->nLine;` |
|       - |  7121 | `	/* Jump the left parenthesis '(' */` |
|   51411 |  7122 | `	pGen->pIn++;` |
|       - |  7123 | `	/* Delimit the function signature */` |
|   51411 |  7124 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   51411 |  7125 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7126 | `		/* Syntax error */` |
|       8 |  7127 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       8 |  7128 | `		if( rc == SXERR_ABORT ){` |
|       - |  7129 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7130 | `			return SXERR_ABORT;` |
|       - |  7131 | `		}` |
|       8 |  7132 | `		pGen->pIn = pGen->pEnd;` |
|       8 |  7133 | `		return SXRET_OK;` |
|       - |  7134 | `	}` |
|       - |  7135 | `	/* Create the function state */` |
|   51405 |  7136 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   51405 |  7137 | `	if( pFunc == 0 ){` |
|     ! 0 |  7138 | `		goto OutOfMem;` |
|       - |  7139 | `	}` |
|       - |  7140 | `	/* Build the function name, prepending namespace if active */` |
|   51412 |  7141 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  7142 | `		SyBlob sFQN;` |
|       - |  7143 | `		sxu32 nLen;` |
|      16 |  7144 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  7145 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  7146 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  7147 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  7148 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  7149 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  7150 | `		SyBlobRelease(&sFQN);` |
|      16 |  7151 | `		if( zName == 0 ){` |
|     ! 0 |  7152 | `			goto OutOfMem;` |
|       - |  7153 | `		}` |
|      16 |  7154 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  7155 | `	}else{` |
|   51391 |  7156 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   51391 |  7157 | `		if( zName == 0 ){` |
|     ! 0 |  7158 | `			goto OutOfMem;` |
|       - |  7159 | `		}` |
|   51391 |  7160 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  7161 | `	}` |
|   51405 |  7162 | `	if( pGen->pIn < pEnd ){` |
|       - |  7163 | `		/* Collect function arguments */` |
|   35399 |  7164 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   35399 |  7165 | `		if( rc == SXERR_ABORT ){` |
|       - |  7166 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7167 | `			return SXERR_ABORT;` |
|       - |  7168 | `		}` |
|   17697 |  7169 | `	}` |
|       - |  7170 | `	/* Point past ')' and parse optional return type ': type' */` |
|   51405 |  7171 | `	pGen->pIn = &pEnd[1];` |
|       - |  7172 | `	{` |
|   51405 |  7173 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   51405 |  7174 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7175 | `			return SXERR_ABORT;` |
|   51405 |  7176 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       8 |  7177 | `			return SXERR_SYNTAX;` |
|       - |  7178 | `		}` |
|       - |  7179 | `	}` |
|   51399 |  7180 | `	if( bHandleClosure ){` |
|       - |  7181 | `		ph7_vm_func_closure_env sEnv;` |
|     325 |  7182 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     320 |  7183 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     177 |  7184 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      29 |  7185 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  7186 | `				/* Closure,record environment variable */` |
|      29 |  7187 | `				pGen->pIn++;` |
|      29 |  7188 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  7189 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  7190 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7191 | `						return SXERR_ABORT;` |
|       - |  7192 | `					}` |
|     ! 0 |  7193 | `				}` |
|      29 |  7194 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  7195 | `				/* Compile until we hit the first closing parenthesis */` |
|      57 |  7196 | `				while( pGen->pIn < pGen->pEnd ){` |
|      57 |  7197 | `					int iFlagsLocal = 0;` |
|      57 |  7198 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      29 |  7199 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      29 |  7200 | `						break;` |
|       - |  7201 | `					}` |
|      33 |  7202 | `					nLineLocal = pGen->pIn->nLine;` |
|      33 |  7203 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  7204 | `						/* Pass by reference,record that */` |
|     ! 0 |  7205 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  7206 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  7207 | `							);` |
|     ! 0 |  7208 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  7209 | `						pGen->pIn++;` |
|     ! 0 |  7210 | `					}` |
|      28 |  7211 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      33 |  7212 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7213 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  7214 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  7215 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  7216 | `								return SXERR_ABORT;` |
|       - |  7217 | `							}` |
|       - |  7218 | `							/* Find the closing parenthesis */` |
|     ! 0 |  7219 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  7220 | `								pGen->pIn++;` |
|     ! 0 |  7221 | `							}` |
|     ! 0 |  7222 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  7223 | `								pGen->pIn++;` |
|     ! 0 |  7224 | `							}` |
|     ! 0 |  7225 | `							break;` |
|       - |  7226 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  7227 | `					}else{` |
|       - |  7228 | `						SyString *pNameLocal;` |
|       - |  7229 | `						char *zDup;` |
|       - |  7230 | `						/* Duplicate variable name */` |
|      33 |  7231 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      33 |  7232 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      33 |  7233 | `						if( zDup ){` |
|       - |  7234 | `							/* Zero the structure */` |
|      33 |  7235 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      33 |  7236 | `							sEnv.iFlags = iFlagsLocal;` |
|      33 |  7237 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      33 |  7238 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      33 |  7239 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  7240 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  7241 | `									got_this = 1;` |
|     ! 0 |  7242 | `							}` |
|       - |  7243 | `							/* Save imported variable */` |
|      33 |  7244 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      19 |  7245 | `						}else{` |
|     ! 0 |  7246 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7247 | `							 return SXERR_ABORT;` |
|       - |  7248 | `						}` |
|       - |  7249 | `					}` |
|      33 |  7250 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      39 |  7251 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7252 | `						/* Ignore trailing commas */` |
|       7 |  7253 | `						pGen->pIn++;` |
|       1 |  7254 | `					}` |
|       5 |  7255 | `				}` |
|      29 |  7256 | `				if( !got_this ){` |
|       - |  7257 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  7258 | `					 * available to the closure environment.` |
|       - |  7259 | `					 */` |
|      29 |  7260 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      29 |  7261 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      29 |  7262 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      29 |  7263 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      29 |  7264 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      12 |  7265 | `				}` |
|      29 |  7266 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  7267 | `					/* Mark as closure */` |
|      29 |  7268 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|      12 |  7269 | `				}` |
|       - |  7270 | `				/* php 7.1+: the return type follows the use clause —` |
|       - |  7271 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|       - |  7272 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|       - |  7273 | `				 * so an unconditional call would wipe a type parsed at the` |
|       - |  7274 | `				 * legacy pre-use position. */` |
|      29 |  7275 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|       7 |  7276 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|       7 |  7277 | `					if( rcRt2 == SXERR_ABORT ){` |
|     ! 0 |  7278 | `						return SXERR_ABORT;` |
|       7 |  7279 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|     ! 0 |  7280 | `						return SXERR_SYNTAX;` |
|       - |  7281 | `					}` |
|       3 |  7282 | `				}` |
|      12 |  7283 | `		}` |
|     160 |  7284 | `	}` |
|       - |  7285 | `	/* Compile the body */` |
|   51399 |  7286 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   51399 |  7287 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7288 | `		return SXERR_ABORT;` |
|       - |  7289 | `	}` |
|   51399 |  7290 | `	if( ppFunc ){` |
|     325 |  7291 | `		*ppFunc = pFunc;` |
|     160 |  7292 | `	}` |
|   51399 |  7293 | `	rc = SXRET_OK;` |
|   51399 |  7294 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  7295 | `		/* Finally register the function */` |
|   51375 |  7296 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   25685 |  7297 | `	}` |
|   51399 |  7298 | `	if( rc == SXRET_OK ){` |
|   51399 |  7299 | `		return SXRET_OK;` |
|       - |  7300 | `	}` |
|       - |  7301 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  7302 | `OutOfMem:` |
|       - |  7303 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  7304 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  7305 | `	 */` |
|     ! 0 |  7306 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  7307 | `	return SXERR_ABORT;` |
|   25708 |  7308 | `}` |
|       - |  7309 | `/*` |
|       - |  7310 | ` * Compile a standard PHP function.` |
|       - |  7311 | ` *  Refer to the block-comment above for more information.` |
|       - |  7312 | ` */` |
|   51094 |  7313 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  7314 | `{` |
|       - |  7315 | `	SyString *pName;` |
|       - |  7316 | `	sxi32 iFlags;` |
|       - |  7317 | `	sxu32 nLine;` |
|       - |  7318 | `	sxi32 rc;` |
|       - |  7319 |  |
|   51099 |  7320 | `	nLine = pGen->pIn->nLine;` |
|   51099 |  7321 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   51099 |  7322 | `	iFlags = 0;` |
|   51099 |  7323 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7324 | `		/* Return by reference,remember that */` |
|      10 |  7325 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7326 | `		/* Jump the '&' token */` |
|      10 |  7327 | `		pGen->pIn++;` |
|       4 |  7328 | `	}` |
|   51099 |  7329 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7330 | `		/* Invalid function name */` |
|       8 |  7331 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       8 |  7332 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7333 | `			return SXERR_ABORT;` |
|       - |  7334 | `		}` |
|       - |  7335 | `		/* Sychronize with the next semi-colon or braces*/` |
|      22 |  7336 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      16 |  7337 | `			pGen->pIn++;` |
|       2 |  7338 | `		}` |
|       8 |  7339 | `		return SXRET_OK;` |
|       - |  7340 | `	}` |
|   51093 |  7341 | `	pName = &pGen->pIn->sData;` |
|   51093 |  7342 | `	nLine = pGen->pIn->nLine;` |
|       - |  7343 | `	/* Jump the function name */` |
|   51093 |  7344 | `	pGen->pIn++;` |
|   51093 |  7345 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7346 | `		/* Syntax error */` |
|       3 |  7347 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  7348 | `		if( rc == SXERR_ABORT ){` |
|       - |  7349 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7350 | `			return SXERR_ABORT;` |
|       - |  7351 | `		}` |
|       - |  7352 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  7353 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  7354 | `			pGen->pIn++;` |
|     ! 0 |  7355 | `		}` |
|       3 |  7356 | `		return SXRET_OK;` |
|       - |  7357 | `	}` |
|       - |  7358 | `	/* Compile function body */` |
|   51091 |  7359 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   51091 |  7360 | `	return rc;` |
|   25552 |  7361 | `}` |
|       - |  7362 | `/*` |
|       - |  7363 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  7364 | ` * According to the PHP language reference manual` |
|       - |  7365 | ` *  Visibility:` |
|       - |  7366 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  7367 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  7368 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  7369 | ` *  Members declared protected can be accessed only within the class` |
|       - |  7370 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  7371 | ` *  may only be accessed by the class that defines the member.` |
|       - |  7372 | ` */` |
|  369656 |  7373 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  7374 | `{` |
|  369661 |  7375 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   23097 |  7376 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  346569 |  7377 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   49849 |  7378 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  7379 | `	}` |
|       - |  7380 | `	/* Assume public by default */` |
|  296725 |  7381 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  184833 |  7382 | `}` |
|       - |  7383 | `/*` |
|       - |  7384 | ` * Compile a class constant.` |
|       - |  7385 | ` * According to the PHP language reference manual` |
|       - |  7386 | ` *  Class Constants` |
|       - |  7387 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  7388 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  7389 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  7390 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  7391 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  7392 | ` *   It's also possible for interfaces to have constants.` |
|       - |  7393 | ` * Symisc eXtension.` |
|       - |  7394 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  7395 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7396 | ` *  Example:` |
|       - |  7397 | ` *   class Test{` |
|       - |  7398 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7399 | ` *   };` |
|       - |  7400 | ` *   var_dump(TEST::MyConst);` |
|       - |  7401 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7402 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7403 | ` */` |
|       - |  7404 | `/*` |
|       - |  7405 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  7406 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  7407 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  7408 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  7409 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  7410 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  7411 | ` */` |
|      98 |  7412 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  7413 | `{` |
|       - |  7414 | `	SyToken *p0, *p1;` |
|     103 |  7415 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7416 | `		return 0;` |
|       - |  7417 | `	}` |
|     103 |  7418 | `	p0 = pGen->pIn;` |
|       - |  7419 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|     103 |  7420 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  7421 | `		return 1;` |
|       - |  7422 | `	}` |
|     103 |  7423 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  7424 | `		return 1;` |
|       - |  7425 | `	}` |
|       - |  7426 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  7427 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  7428 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|      99 |  7429 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      99 |  7430 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      99 |  7431 | `		if( p1 ){` |
|      99 |  7432 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      34 |  7433 | `				return 1;` |
|       - |  7434 | `			}` |
|      69 |  7435 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  7436 | `				return 1;` |
|       - |  7437 | `			}` |
|      30 |  7438 | `		}` |
|      30 |  7439 | `	}` |
|      65 |  7440 | `	return 0;` |
|      54 |  7441 | `}` |
|       - |  7442 | `/*` |
|       - |  7443 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|       - |  7444 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|       - |  7445 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|       - |  7446 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|       - |  7447 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|       - |  7448 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|       - |  7449 | ` * Peek only; never consumes tokens.` |
|       - |  7450 | ` */` |
|      24 |  7451 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|       4 |  7452 | `{` |
|      28 |  7453 | `	SyToken *p = pGen->pIn;` |
|      39 |  7454 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      20 |  7455 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|       3 |  7456 | `		p++; /* skip leading unary sign(s) */` |
|       1 |  7457 | `	}` |
|      28 |  7458 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|      23 |  7459 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|       - |  7460 | `	}` |
|       6 |  7461 | `	p++;` |
|       - |  7462 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|       6 |  7463 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|      16 |  7464 | `}` |
|       - |  7465 | `/*` |
|       - |  7466 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|       - |  7467 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|       - |  7468 | `` * `$o->new`), not a `new` expression.`` |
|       - |  7469 | ` */` |
|       6 |  7470 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|       3 |  7471 | `{` |
|       - |  7472 | `	sxi32 iOp;` |
|       9 |  7473 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|     ! 0 |  7474 | `		return 0;` |
|       - |  7475 | `	}` |
|       9 |  7476 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       9 |  7477 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|       6 |  7478 | `}` |
|       - |  7479 | `/*` |
|       - |  7480 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|       - |  7481 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|       - |  7482 | ` * interface-constant and (instance/static) property-default initializers` |
|       - |  7483 | ` * ("New expressions are not supported in this context") while still allowing it` |
|       - |  7484 | ` * in global constants, parameter defaults and static-local initializers (which` |
|       - |  7485 | ` * are compiled by different functions and left untouched). The scan is` |
|       - |  7486 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|       - |  7487 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|       - |  7488 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|       - |  7489 | ` *` |
|       - |  7490 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|       - |  7491 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|       - |  7492 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|       - |  7493 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|       - |  7494 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|       - |  7495 | ` */` |
|   23578 |  7496 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|       5 |  7497 | `{` |
|   23583 |  7498 | `	SyToken *p = pGen->pIn;` |
|   23583 |  7499 | `	int iDepth = 0;` |
|   70943 |  7500 | `	while( p < pGen->pEnd ){` |
|   70943 |  7501 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   23575 |  7502 | `			break; /* end of this initializer */` |
|       - |  7503 | `		}` |
|   47368 |  7504 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   23694 |  7505 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      10 |  7506 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|       - |  7507 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|       - |  7508 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|       - |  7509 | `			 * expression. */` |
|       3 |  7510 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|       3 |  7511 | `			p++;` |
|       3 |  7512 | `			if( bArrow ){` |
|       - |  7513 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|       - |  7514 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|       3 |  7515 | `				int iBase = iDepth;` |
|      17 |  7516 | `				while( p < pGen->pEnd ){` |
|      17 |  7517 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       5 |  7518 | `						iDepth++;` |
|      15 |  7519 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       5 |  7520 | `						if( iDepth <= iBase ){` |
|     ! 0 |  7521 | `							break; /* closes an enclosing group, not the fn's own */` |
|       - |  7522 | `						}` |
|       5 |  7523 | `						iDepth--;` |
|      11 |  7524 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|       3 |  7525 | `						break;` |
|       - |  7526 | `					}` |
|      15 |  7527 | `					p++;` |
|       1 |  7528 | `				}` |
|       2 |  7529 | `			}else{` |
|       - |  7530 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|       - |  7531 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|       - |  7532 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|       - |  7533 | `				 * then skip the balanced brace block. */` |
|     ! 0 |  7534 | `				int iLocal = 0;` |
|     ! 0 |  7535 | `				while( p < pGen->pEnd ){` |
|     ! 0 |  7536 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|     ! 0 |  7537 | `						break; /* body brace */` |
|       - |  7538 | `					}` |
|     ! 0 |  7539 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  7540 | `						iLocal++;` |
|     ! 0 |  7541 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  7542 | `						if( iLocal > 0 ){` |
|     ! 0 |  7543 | `							iLocal--;` |
|     ! 0 |  7544 | `						}` |
|     ! 0 |  7545 | `					}` |
|     ! 0 |  7546 | `					p++;` |
|     ! 0 |  7547 | `				}` |
|     ! 0 |  7548 | `				if( p < pGen->pEnd ){` |
|     ! 0 |  7549 | `					int iBrace = 0; /* p is on the body '{' */` |
|     ! 0 |  7550 | `					while( p < pGen->pEnd ){` |
|     ! 0 |  7551 | `						if( p->nType & PH7_TK_OCB ){` |
|     ! 0 |  7552 | `							iBrace++;` |
|     ! 0 |  7553 | `						}else if( p->nType & PH7_TK_CCB ){` |
|     ! 0 |  7554 | `							iBrace--;` |
|     ! 0 |  7555 | `							if( iBrace == 0 ){` |
|     ! 0 |  7556 | `								p++;` |
|     ! 0 |  7557 | `								break;` |
|       - |  7558 | `							}` |
|     ! 0 |  7559 | `						}` |
|     ! 0 |  7560 | `						p++;` |
|     ! 0 |  7561 | `					}` |
|     ! 0 |  7562 | `				}` |
|       - |  7563 | `			}` |
|       3 |  7564 | `			continue;` |
|       - |  7565 | `		}` |
|   47371 |  7566 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      69 |  7567 | `			iDepth++;` |
|   47339 |  7568 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      67 |  7569 | `			if( iDepth > 0 ){` |
|      67 |  7570 | `				iDepth--;` |
|      31 |  7571 | `			}` |
|   47276 |  7572 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|   23555 |  7573 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|       - |  7574 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|       - |  7575 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|       - |  7576 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|      11 |  7577 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|      11 |  7578 | `				return 1;` |
|       - |  7579 | `			}` |
|     ! 0 |  7580 | `		}` |
|   47363 |  7581 | `		p++;` |
|       5 |  7582 | `	}` |
|   23575 |  7583 | `	return 0;` |
|   11794 |  7584 | `}` |
|       - |  7585 | `/*` |
|       - |  7586 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  7587 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  7588 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  7589 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  7590 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  7591 | ` * share the same backing.` |
|       - |  7592 | ` */` |
|     214 |  7593 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  7594 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  7595 | `{` |
|     219 |  7596 | `	pAttr->nType = nType;` |
|     219 |  7597 | `	pAttr->sClass = *pClass;` |
|     219 |  7598 | `	pAttr->sTypeName = *pTypeName;` |
|     219 |  7599 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7600 | `		sxu32 i;` |
|      67 |  7601 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      47 |  7602 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      47 |  7603 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      26 |  7604 | `		}` |
|      10 |  7605 | `	}` |
|     219 |  7606 | `}` |
|      98 |  7607 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7608 | `{` |
|     103 |  7609 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7610 | `	SySet *pInstrContainer;` |
|       - |  7611 | `	ph7_class_attr *pCons;` |
|       - |  7612 | `	SyString *pName;` |
|       - |  7613 | `	sxi32 rc;` |
|     103 |  7614 | `	sxu32 nType = 0;` |
|       - |  7615 | `	SyString sTypeClass;` |
|       - |  7616 | `	SyString sTypeText;` |
|       - |  7617 | `	SySet aUnionAlts;` |
|     103 |  7618 | `	sxi32 iTypeFlags = 0;` |
|     103 |  7619 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|     103 |  7620 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|     103 |  7621 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7622 | `	/* Extract visibility level */` |
|     103 |  7623 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7624 | `	/* Mark as constant */` |
|     103 |  7625 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|     103 |  7626 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  7627 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  7628 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|     122 |  7629 | `	if( GenStateClassConstHasType(pGen) ){` |
|      61 |  7630 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      38 |  7631 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  7632 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  7633 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  7634 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  7635 | `		 * and success paths release. */` |
|      42 |  7636 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7637 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7638 | `			goto Synchronize;` |
|      42 |  7639 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7640 | `			return SXERR_ABORT;` |
|      42 |  7641 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7642 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7643 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  7644 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7645 | `				return SXERR_ABORT;` |
|       - |  7646 | `			}` |
|     ! 0 |  7647 | `			goto Synchronize;` |
|       - |  7648 | `		}` |
|      42 |  7649 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      19 |  7650 | `	}` |
|      49 |  7651 | `loop:` |
|     105 |  7652 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7653 | `		/* Invalid constant name */` |
|     ! 0 |  7654 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  7655 | `		if( rc == SXERR_ABORT ){` |
|       - |  7656 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7657 | `			return SXERR_ABORT;` |
|       - |  7658 | `		}` |
|     ! 0 |  7659 | `		goto Synchronize;` |
|       - |  7660 | `	}` |
|       - |  7661 | `	/* Peek constant name */` |
|     105 |  7662 | `	pName = &pGen->pIn->sData;` |
|       - |  7663 | `	/* Make sure the constant name isn't reserved */` |
|     105 |  7664 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  7665 | `		/* Reserved constant name */` |
|     ! 0 |  7666 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  7667 | `		if( rc == SXERR_ABORT ){` |
|       - |  7668 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7669 | `			return SXERR_ABORT;` |
|       - |  7670 | `		}` |
|     ! 0 |  7671 | `		goto Synchronize;` |
|       - |  7672 | `	}` |
|       - |  7673 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|     105 |  7674 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      61 |  7675 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      38 |  7676 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      19 |  7677 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      42 |  7678 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7679 | `			return SXERR_ABORT;` |
|      42 |  7680 | `		}else if( rc != SXRET_OK ){` |
|       3 |  7681 | `			goto Synchronize;` |
|       - |  7682 | `		}` |
|      18 |  7683 | `	}` |
|       - |  7684 | `	/* Advance the stream cursor */` |
|     103 |  7685 | `	pGen->pIn++;` |
|     103 |  7686 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  7687 | `		/* Invalid declaration */` |
|     ! 0 |  7688 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  7689 | `		if( rc == SXERR_ABORT ){` |
|       - |  7690 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7691 | `			return SXERR_ABORT;` |
|       - |  7692 | `		}` |
|     ! 0 |  7693 | `		goto Synchronize;` |
|       - |  7694 | `	}` |
|     103 |  7695 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  7696 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|       - |  7697 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|       - |  7698 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|       - |  7699 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|      98 |  7700 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|      39 |  7701 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|       8 |  7702 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7703 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|       2 |  7704 | `			&pClass->sName,pName,&sTypeText);` |
|       6 |  7705 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7706 | `			return SXERR_ABORT;` |
|       - |  7707 | `		}` |
|       6 |  7708 | `		goto Synchronize;` |
|       - |  7709 | `	}` |
|       - |  7710 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|       - |  7711 | `	 * constant initializer ("New expressions are not supported in this context").` |
|       - |  7712 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|      99 |  7713 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|       5 |  7714 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7715 | `			"New expressions are not supported in this context");` |
|       5 |  7716 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7717 | `			return SXERR_ABORT;` |
|       - |  7718 | `		}` |
|       5 |  7719 | `		goto Synchronize;` |
|       - |  7720 | `	}` |
|       - |  7721 | `	/* Allocate a new class attribute */` |
|      95 |  7722 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      95 |  7723 | `	if( pCons == 0 ){` |
|     ! 0 |  7724 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7725 | `		return SXERR_ABORT;` |
|       - |  7726 | `	}` |
|      95 |  7727 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      35 |  7728 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      16 |  7729 | `	}` |
|       - |  7730 | `	/* Swap bytecode container */` |
|      95 |  7731 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      95 |  7732 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  7733 | `	/* Compile constant value.` |
|       - |  7734 | `	 */` |
|      95 |  7735 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      95 |  7736 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  7737 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  7738 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7739 | `			return SXERR_ABORT;` |
|       - |  7740 | `		}` |
|       1 |  7741 | `	}` |
|       - |  7742 | `	/* Emit the done instruction */` |
|      95 |  7743 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      95 |  7744 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      95 |  7745 | `	if( rc == SXERR_ABORT ){` |
|       - |  7746 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7747 | `		return SXERR_ABORT;` |
|       - |  7748 | `	}` |
|       - |  7749 | `	/* All done,install the constant */` |
|      95 |  7750 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      95 |  7751 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7752 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7753 | `		return SXERR_ABORT;` |
|       - |  7754 | `	}` |
|      95 |  7755 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7756 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  7757 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  7758 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7759 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7760 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7761 | `				pTok--;` |
|     ! 0 |  7762 | `			}` |
|     ! 0 |  7763 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7764 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  7765 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7766 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7767 | `				return SXERR_ABORT;` |
|       - |  7768 | `			}` |
|     ! 0 |  7769 | `		}else{` |
|       3 |  7770 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  7771 | `				goto loop;` |
|       - |  7772 | `			}` |
|       - |  7773 | `		}` |
|     ! 0 |  7774 | `	}` |
|      93 |  7775 | `	SySetRelease(&aUnionAlts);` |
|      93 |  7776 | `	return SXRET_OK;` |
|       5 |  7777 | `Synchronize:` |
|      13 |  7778 | `	SySetRelease(&aUnionAlts);` |
|       - |  7779 | `	/* Synchronize with the first semi-colon */` |
|      45 |  7780 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      35 |  7781 | `		pGen->pIn++;` |
|       3 |  7782 | `	}` |
|      13 |  7783 | `	return SXERR_CORRUPT;` |
|      54 |  7784 | `}` |
|       - |  7785 | `/*` |
|       - |  7786 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  7787 | ` * According to the PHP language reference manual` |
|       - |  7788 | ` *  Properties` |
|       - |  7789 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  7790 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  7791 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  7792 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  7793 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  7794 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  7795 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  7796 | ` * Symisc eXtension.` |
|       - |  7797 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  7798 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7799 | ` *  Example:` |
|       - |  7800 | ` *   class Test{` |
|       - |  7801 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7802 | ` *   };` |
|       - |  7803 | ` *   var_dump(TEST::myVar);` |
|       - |  7804 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7805 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7806 | ` */` |
|       - |  7807 | `/*` |
|       - |  7808 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  7809 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  7810 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  7811 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  7812 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  7813 | ` */` |
|  200320 |  7814 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  7815 | `{` |
|  200325 |  7816 | `	SyToken *p = pStart;` |
|  200325 |  7817 | `	int bFirst = 1;` |
|  200325 |  7818 | `	if( p >= pEnd ) return 0;` |
|       - |  7819 | ``	/* Optional nullable `?` shorthand. */`` |
|  200325 |  7820 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      19 |  7821 | `		p++;` |
|      19 |  7822 | `		if( p >= pEnd ) return 0;` |
|       8 |  7823 | `	}` |
|       - |  7824 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  7825 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  7826 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  7827 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|  100160 |  7828 | `	for(;;){` |
|  200343 |  7829 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  7830 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  7831 | `			p++;` |
|       9 |  7832 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  7833 | `			if( p >= pEnd ) return 0;` |
|       3 |  7834 | `			p++; /* skip ')' */` |
|       2 |  7835 | `		}else{` |
|       - |  7836 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - |  7837 | ``			 * then any `&`-joined intersection members. */`` |
|  200341 |  7838 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  200341 |  7839 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7840 | `				return 0;` |
|       - |  7841 | `			}` |
|       - |  7842 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - |  7843 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - |  7844 | `			 * may still appear at the initial dispatch site). */` |
|  200341 |  7845 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  200295 |  7846 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  200290 |  7847 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|   11718 |  7848 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  200139 |  7849 | `					return 0;` |
|       - |  7850 | `				}` |
|      78 |  7851 | `			}` |
|     207 |  7852 | `			p++;` |
|     209 |  7853 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7854 | `				p += 2;` |
|       1 |  7855 | `			}` |
|     306 |  7856 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     210 |  7857 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7858 | `				p++; /* skip '&' */` |
|       3 |  7859 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       3 |  7860 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       3 |  7861 | `				p++;` |
|       3 |  7862 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  7863 | `					p += 2;` |
|     ! 0 |  7864 | `				}` |
|       1 |  7865 | `			}` |
|       - |  7866 | `		}` |
|     209 |  7867 | `		bFirst = 0;` |
|     204 |  7868 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      23 |  7869 | `			&& p->sData.zString[0] == '\|' ){` |
|      23 |  7870 | ``			p++; /* next `\|`-separated part */`` |
|      23 |  7871 | `			continue;` |
|       - |  7872 | `		}` |
|     191 |  7873 | `		break;` |
|     ! 0 |  7874 | `	}` |
|     191 |  7875 | `	if( p >= pEnd ) return 0;` |
|     191 |  7876 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|  100165 |  7877 | `}` |
|       - |  7878 |  |
|       - |  7879 | `/*` |
|       - |  7880 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  7881 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  7882 | ` * if not). Recognized forms:` |
|       - |  7883 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  7884 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  7885 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  7886 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  7887 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  7888 | ` * on unrecoverable error.` |
|       - |  7889 | ` *` |
|       - |  7890 | ` * When a type is parsed:` |
|       - |  7891 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  7892 | ` *   *pClass is set to the class name (for class types)` |
|       - |  7893 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  7894 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  7895 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  7896 | ` */` |
|     186 |  7897 | `static sxi32 GenStateParsePropertyType(` |
|       - |  7898 | `	ph7_gen_state *pGen,` |
|       - |  7899 | `	sxu32 *pnType,` |
|       - |  7900 | `	SyString *pClass,` |
|       - |  7901 | `	sxi32 *piTypeFlags,` |
|       - |  7902 | `	SyString *pTypeText,` |
|       - |  7903 | `	SySet *pAlts` |
|       5 |  7904 | `){` |
|     191 |  7905 | `	sxi32 iFlags = 0;` |
|       - |  7906 | `	sxi32 rc;` |
|     191 |  7907 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7908 | `		return SXRET_OK;` |
|       - |  7909 | `	}` |
|       - |  7910 | `	/* If the first token is '$', there's no type */` |
|     191 |  7911 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  7912 | `		return SXRET_OK;` |
|       - |  7913 | `	}` |
|     191 |  7914 | `	rc = GenStateParseUnionTypeDecl(` |
|      93 |  7915 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  7916 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  7917 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  7918 | `		/* bAllowVoid */ 0,` |
|     186 |  7919 | `		pGen->pIn->nLine);` |
|     191 |  7920 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7921 | `		return rc;` |
|       - |  7922 | `	}` |
|       - |  7923 | `	/* Verify next token is '$' (start of property name) */` |
|     191 |  7924 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7925 | `		return SXERR_SYNTAX;` |
|       - |  7926 | `	}` |
|     191 |  7927 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     191 |  7928 | `	return SXRET_OK;` |
|      98 |  7929 | `}` |
|       - |  7930 |  |
|       - |  7931 | `/*` |
|       - |  7932 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  7933 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  7934 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  7935 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  7936 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  7937 | ` * by the type parser itself before reaching here.` |
|       - |  7938 | ` *` |
|       - |  7939 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  7940 | ` * use in the error message.` |
|       - |  7941 | ` */` |
|     340 |  7942 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  7943 | `	sxu32 nType,` |
|       - |  7944 | `	const SyString *pClass,` |
|       - |  7945 | `	const char **pzName,` |
|       - |  7946 | `	sxu32 *pnName)` |
|       5 |  7947 | `{` |
|       - |  7948 | `	const char *z;` |
|       - |  7949 | `	sxu32 n;` |
|     345 |  7950 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     291 |  7951 | `		return 0;` |
|       - |  7952 | `	}` |
|      59 |  7953 | `	z = pClass->zString;` |
|      59 |  7954 | `	n = pClass->nByte;` |
|      59 |  7955 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  7956 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  7957 | `	}` |
|       - |  7958 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  7959 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  7960 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      52 |  7961 | `	return 0;` |
|     175 |  7962 | `}` |
|       - |  7963 |  |
|       - |  7964 | `/*` |
|       - |  7965 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  7966 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  7967 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  7968 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  7969 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  7970 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  7971 | ` *` |
|       - |  7972 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  7973 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  7974 | ` */` |
|     282 |  7975 | `static sxi32 GenStateValidateMemberType(` |
|       - |  7976 | `	ph7_gen_state *pGen,` |
|       - |  7977 | `	ph7_class *pClass,` |
|       - |  7978 | `	const SyString *pMemberName,` |
|       - |  7979 | `	sxu32 nType,` |
|       - |  7980 | `	const SyString *pTypeClass,` |
|       - |  7981 | `	const SyString *pTypeText,` |
|       - |  7982 | `	SySet *pUnionAlts,` |
|       - |  7983 | `	const char *zErrFmt,` |
|       - |  7984 | `	sxu32 nLine)` |
|       5 |  7985 | `{` |
|     287 |  7986 | `	const char *zBad = 0;` |
|     287 |  7987 | `	sxu32 nBad = 0;` |
|       - |  7988 | `	SyString sFallback;` |
|       - |  7989 | `	const SyString *pBad;` |
|       - |  7990 | `	sxi32 rc;` |
|     287 |  7991 | `	int bDisallowed = 0;` |
|     287 |  7992 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  7993 | `		bDisallowed = 1;` |
|     285 |  7994 | `	}else if( pUnionAlts ){` |
|       - |  7995 | `		sxu32 i;` |
|      89 |  7996 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      63 |  7997 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      63 |  7998 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  7999 | `				bDisallowed = 1;` |
|       3 |  8000 | `				break;` |
|       - |  8001 | `			}` |
|      33 |  8002 | `		}` |
|      14 |  8003 | `	}` |
|     287 |  8004 | `	if( !bDisallowed ){` |
|     281 |  8005 | `		return SXRET_OK;` |
|       - |  8006 | `	}` |
|       - |  8007 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  8008 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  8009 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  8010 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  8011 | `		pBad = pTypeText;` |
|       5 |  8012 | `	}else{` |
|     ! 0 |  8013 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  8014 | `		pBad = &sFallback;` |
|       - |  8015 | `	}` |
|      11 |  8016 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  8017 | `		zErrFmt,` |
|       3 |  8018 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  8019 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8020 | `		return SXERR_ABORT;` |
|       - |  8021 | `	}` |
|       8 |  8022 | `	return SXERR_SYNTAX;` |
|     146 |  8023 | `}` |
|       - |  8024 | `/*` |
|       - |  8025 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  8026 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  8027 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  8028 | ` * than promoted to a lexer keyword.` |
|       - |  8029 | ` */` |
| 1779586 |  8030 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  8031 | `{` |
| 1816020 |  8032 | `	return (pTok->nType & PH7_TK_ID)` |
|  926222 |  8033 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1816015 |  8034 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  8035 | `}` |
|   81132 |  8036 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  8037 | `{` |
|   81137 |  8038 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8039 | `	ph7_class_attr *pAttr;` |
|       - |  8040 | `	SyString *pName;` |
|       - |  8041 | `	sxi32 rc;` |
|   81137 |  8042 | `	sxu32 nType = 0;` |
|       - |  8043 | `	SyString sTypeClass;` |
|       - |  8044 | `	SyString sTypeText;` |
|       - |  8045 | `	SySet aUnionAlts;` |
|   81137 |  8046 | `	sxi32 iTypeFlags = 0;` |
|   81137 |  8047 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   81137 |  8048 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   81137 |  8049 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  8050 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  8051 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  8052 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   81137 |  8053 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  8054 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  8055 | `	}` |
|       - |  8056 | `	/* Extract visibility level */` |
|   81137 |  8057 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  8058 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   81230 |  8059 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     191 |  8060 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     191 |  8061 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  8062 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  8063 | `			goto Synchronize;` |
|     191 |  8064 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  8065 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8066 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  8067 | `				&pGen->pIn->sData);` |
|     ! 0 |  8068 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8069 | `				return SXERR_ABORT;` |
|       - |  8070 | `			}` |
|     ! 0 |  8071 | `			goto Synchronize;` |
|     191 |  8072 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  8073 | `			return SXERR_ABORT;` |
|       - |  8074 | `		}` |
|      93 |  8075 | `	}` |
|     ! 0 |  8076 | `loop:` |
|   81141 |  8077 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  8078 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  8079 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8080 | `			return SXERR_ABORT;` |
|       - |  8081 | `		}` |
|     ! 0 |  8082 | `		goto Synchronize;` |
|       - |  8083 | `	}` |
|   81141 |  8084 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   81141 |  8085 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  8086 | `		/* Invalid attribute name */` |
|     ! 0 |  8087 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  8088 | `		if( rc == SXERR_ABORT ){` |
|       - |  8089 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8090 | `			return SXERR_ABORT;` |
|       - |  8091 | `		}` |
|     ! 0 |  8092 | `		goto Synchronize;` |
|       - |  8093 | `	}` |
|       - |  8094 | `	/* Peek attribute name */` |
|   81141 |  8095 | `	pName = &pGen->pIn->sData;` |
|       - |  8096 | `	/* Advance the stream cursor */` |
|   81141 |  8097 | `	pGen->pIn++;` |
|   81141 |  8098 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  8099 | `		/* Invalid declaration */` |
|       3 |  8100 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  8101 | `		if( rc == SXERR_ABORT ){` |
|       - |  8102 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8103 | `			return SXERR_ABORT;` |
|       - |  8104 | `		}` |
|       3 |  8105 | `		goto Synchronize;` |
|       - |  8106 | `	}` |
|       - |  8107 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  8108 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   81139 |  8109 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  8110 | `		const char *zRoErr = 0;` |
|      39 |  8111 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  8112 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  8113 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  8114 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  8115 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  8116 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  8117 | `		}` |
|      39 |  8118 | `		if( zRoErr ){` |
|      13 |  8119 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  8120 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8121 | `				return SXERR_ABORT;` |
|       - |  8122 | `			}` |
|      13 |  8123 | `			goto Synchronize;` |
|       - |  8124 | `		}` |
|      12 |  8125 | `	}` |
|       - |  8126 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  8127 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  8128 | `	 * by the type parser. */` |
|   81129 |  8129 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     281 |  8130 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  8131 | `			&sTypeText,` |
|     184 |  8132 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      92 |  8133 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     189 |  8134 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8135 | `			return SXERR_ABORT;` |
|     189 |  8136 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  8137 | `			goto Synchronize;` |
|       - |  8138 | `		}` |
|      92 |  8139 | `	}` |
|       - |  8140 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   81129 |  8141 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  8142 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8143 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  8144 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8145 | `			return SXERR_ABORT;` |
|       - |  8146 | `		}` |
|       3 |  8147 | `		goto Synchronize;` |
|       - |  8148 | `	}` |
|       - |  8149 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|       - |  8150 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|       - |  8151 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|       - |  8152 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|       - |  8153 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|       - |  8154 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   81127 |  8155 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|       6 |  8156 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8157 | `			"New expressions are not supported in this context");` |
|       6 |  8158 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8159 | `			return SXERR_ABORT;` |
|       - |  8160 | `		}` |
|       6 |  8161 | `		goto Synchronize;` |
|       - |  8162 | `	}` |
|       - |  8163 | `	/* Allocate a new class attribute */` |
|   81123 |  8164 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   81123 |  8165 | `	if( pAttr == 0 ){` |
|     ! 0 |  8166 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  8167 | `		return SXERR_ABORT;` |
|       - |  8168 | `	}` |
|   81123 |  8169 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     187 |  8170 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      91 |  8171 | `	}` |
|   81123 |  8172 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  8173 | `		SySet *pInstrContainer;` |
|   23485 |  8174 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  8175 | `		/* Swap bytecode container */` |
|   23485 |  8176 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   23485 |  8177 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  8178 | `		/* Compile attribute value.` |
|       - |  8179 | `		 */` |
|   23485 |  8180 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   23485 |  8181 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  8182 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  8183 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8184 | `				return SXERR_ABORT;` |
|       - |  8185 | `			}` |
|     ! 0 |  8186 | `		}` |
|       - |  8187 | `		/* Emit the done instruction */` |
|   23485 |  8188 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   23485 |  8189 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   11740 |  8190 | `	}` |
|       - |  8191 | `	/* All done,install the attribute */` |
|   81123 |  8192 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   81123 |  8193 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8194 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8195 | `		return SXERR_ABORT;` |
|       - |  8196 | `	}` |
|   81123 |  8197 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  8198 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  8199 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  8200 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  8201 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  8202 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  8203 | `				pTok--;` |
|     ! 0 |  8204 | `			}` |
|     ! 0 |  8205 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8206 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8207 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  8208 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8209 | `				return SXERR_ABORT;` |
|       - |  8210 | `			}` |
|     ! 0 |  8211 | `		}else{` |
|       5 |  8212 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  8213 | `				goto loop;` |
|       - |  8214 | `			}` |
|       - |  8215 | `		}` |
|     ! 0 |  8216 | `	}` |
|   81119 |  8217 | `	SySetRelease(&aUnionAlts);` |
|   81119 |  8218 | `	return SXRET_OK;` |
|       9 |  8219 | `Synchronize:` |
|       - |  8220 | `	/* Synchronize with the first semi-colon */` |
|      56 |  8221 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      37 |  8222 | `		pGen->pIn++;` |
|       3 |  8223 | `	}` |
|      22 |  8224 | `	SySetRelease(&aUnionAlts);` |
|      22 |  8225 | `	return SXERR_CORRUPT;` |
|   40571 |  8226 | `}` |
|       - |  8227 | `/*` |
|       - |  8228 | ` * Compile a class method.` |
|       - |  8229 | ` *` |
|       - |  8230 | ` * Refer to the official documentation for more information` |
|       - |  8231 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  8232 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  8233 | ` * overloading and many more.` |
|       - |  8234 | ` */` |
|  288426 |  8235 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  8236 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  8237 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  8238 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  8239 | `	int doBody,          /* TRUE to process method body */` |
|       - |  8240 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  8241 | `	)` |
|       5 |  8242 | `{` |
|  288431 |  8243 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8244 | `	ph7_class_method *pMeth;` |
|       - |  8245 | `	sxi32 iFuncFlags;` |
|       - |  8246 | `	SyString *pName;` |
|       - |  8247 | `	SyToken *pEnd;` |
|       - |  8248 | `	sxi32 rc;` |
|       - |  8249 | `	/* Extract visibility level */` |
|  288431 |  8250 | `	iProtection = GetProtectionLevel(iProtection);` |
|  288431 |  8251 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  288431 |  8252 | `	iFuncFlags = 0;` |
|  288431 |  8253 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8254 | `		/* Invalid method name */` |
|     ! 0 |  8255 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  8256 | `		if( rc == SXERR_ABORT ){` |
|       - |  8257 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8258 | `			return SXERR_ABORT;` |
|       - |  8259 | `		}` |
|     ! 0 |  8260 | `		goto Synchronize;` |
|       - |  8261 | `	}` |
|  288431 |  8262 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  8263 | `		/* Return by reference,remember that */` |
|     ! 0 |  8264 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  8265 | `		/* Jump the '&' token */` |
|     ! 0 |  8266 | `		pGen->pIn++;` |
|     ! 0 |  8267 | `	}` |
|  288431 |  8268 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  8269 | `		/* Invalid method name */` |
|     ! 0 |  8270 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  8271 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8272 | `			return SXERR_ABORT;` |
|       - |  8273 | `		}` |
|     ! 0 |  8274 | `		goto Synchronize;` |
|       - |  8275 | `	}` |
|       - |  8276 | `	/* Peek method name */` |
|  288431 |  8277 | `	pName = &pGen->pIn->sData;` |
|  288431 |  8278 | `	nLine = pGen->pIn->nLine;` |
|       - |  8279 | `	/* Jump the method name */` |
|  288431 |  8280 | `	pGen->pIn++;` |
|  288431 |  8281 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  8282 | `		/* Abstract method */` |
|   99643 |  8283 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  8284 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8285 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  8286 | `				&pClass->sName,pName);` |
|     ! 0 |  8287 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8288 | `				return SXERR_ABORT;` |
|       - |  8289 | `			}` |
|     ! 0 |  8290 | `		}` |
|       - |  8291 | `		/* Assemble method signature only */` |
|   99643 |  8292 | `		doBody = FALSE;` |
|   49819 |  8293 | `	}` |
|  288431 |  8294 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  8295 | `		/* Syntax error */` |
|     ! 0 |  8296 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  8297 | `		if( rc == SXERR_ABORT ){` |
|       - |  8298 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8299 | `			return SXERR_ABORT;` |
|       - |  8300 | `		}` |
|     ! 0 |  8301 | `		goto Synchronize;` |
|       - |  8302 | `	}` |
|       - |  8303 | `	/* Allocate a new class_method instance */` |
|  288431 |  8304 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  288431 |  8305 | `	if( pMeth == 0 ){` |
|     ! 0 |  8306 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8307 | `		return SXERR_ABORT;` |
|       - |  8308 | `	}` |
|       - |  8309 | `	/* Jump the left parenthesis '(' */` |
|  288431 |  8310 | `	pGen->pIn++;` |
|  288431 |  8311 | `	pEnd = 0; /* cc warning */` |
|       - |  8312 | `	/* Delimit the method signature */` |
|  288431 |  8313 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  288431 |  8314 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8315 | `		/* Syntax error */` |
|       3 |  8316 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  8317 | `		if( rc == SXERR_ABORT ){` |
|       - |  8318 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8319 | `			return SXERR_ABORT;` |
|       - |  8320 | `		}` |
|       3 |  8321 | `		goto Synchronize;` |
|       - |  8322 | `	}` |
|       - |  8323 | `	{` |
|  288429 |  8324 | `		int bIsCtor = 0;` |
|  288429 |  8325 | `		int bAbstractCtor = 0;` |
|  288424 |  8326 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  171151 |  8327 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  276856 |  8328 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   23151 |  8329 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  8330 | `				bAbstractCtor = 1;` |
|       2 |  8331 | `			}else{` |
|   23149 |  8332 | `				bIsCtor = 1;` |
|       - |  8333 | `			}` |
|   11573 |  8334 | `		}` |
|  288429 |  8335 | `		if( pGen->pIn < pEnd ){` |
|       - |  8336 | `			/* Collect method arguments */` |
|   77093 |  8337 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   77093 |  8338 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8339 | `				return SXERR_ABORT;` |
|       - |  8340 | `			}` |
|   38544 |  8341 | `		}` |
|       - |  8342 | `	}` |
|       - |  8343 | `	/* Point past ')' and parse optional return type ': type' */` |
|  288429 |  8344 | `	pGen->pIn = &pEnd[1];` |
|       - |  8345 | `	{` |
|  288429 |  8346 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  288429 |  8347 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  8348 | `			return SXERR_ABORT;` |
|  288429 |  8349 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  8350 | `			goto Synchronize;` |
|       - |  8351 | `		}` |
|       - |  8352 | `	}` |
|       - |  8353 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  8354 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  8355 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  8356 | `	{` |
|  288429 |  8357 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  8358 | `		sxu32 i;` |
|  419253 |  8359 | `		for( i = 0; i < nArg; i++ ){` |
|  130839 |  8360 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  8361 | `			ph7_class_attr *pAttr;` |
|  130839 |  8362 | `			sxi32 iAttrFlags = 0;` |
|       - |  8363 | `			int bArgTyped;` |
|  130839 |  8364 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  130773 |  8365 | `				continue;` |
|       - |  8366 | `			}` |
|       - |  8367 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - |  8368 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - |  8369 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      50 |  8370 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      72 |  8371 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|      71 |  8372 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  8373 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8374 | `					"Cannot declare variadic promoted property");` |
|       3 |  8375 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8376 | `					return SXERR_ABORT;` |
|       - |  8377 | `				}` |
|       3 |  8378 | `				goto Synchronize;` |
|       - |  8379 | `			}` |
|       - |  8380 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  8381 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  8382 | `			 * appear as an alternative of a union type. */` |
|      69 |  8383 | `			if( bArgTyped ){` |
|      95 |  8384 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      60 |  8385 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      60 |  8386 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      30 |  8387 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      65 |  8388 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8389 | `					return SXERR_ABORT;` |
|      65 |  8390 | `				}else if( rc != SXRET_OK ){` |
|       6 |  8391 | `					goto Synchronize;` |
|       - |  8392 | `				}` |
|      28 |  8393 | `			}` |
|       - |  8394 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      65 |  8395 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  8396 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8397 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  8398 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8399 | `					return SXERR_ABORT;` |
|       - |  8400 | `				}` |
|       3 |  8401 | `				goto Synchronize;` |
|       - |  8402 | `			}` |
|      63 |  8403 | `			if( bArgTyped ){` |
|      59 |  8404 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      27 |  8405 | `			}` |
|      63 |  8406 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  8407 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  8408 | `			}` |
|      63 |  8409 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 |  8410 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 |  8411 | `			}` |
|      63 |  8412 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  8413 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  8414 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      24 |  8415 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  8416 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8417 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  8418 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8419 | `						return SXERR_ABORT;` |
|       - |  8420 | `					}` |
|       3 |  8421 | `					goto Synchronize;` |
|       - |  8422 | `				}` |
|      22 |  8423 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  8424 | `			}` |
|      61 |  8425 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      61 |  8426 | `			if( pAttr == 0 ){` |
|     ! 0 |  8427 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8428 | `				return SXERR_ABORT;` |
|       - |  8429 | `			}` |
|      61 |  8430 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      59 |  8431 | `				pAttr->nType = pArg->nType;` |
|      59 |  8432 | `				pAttr->sClass = pArg->sClass;` |
|      59 |  8433 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      59 |  8434 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  8435 | `					sxu32 k;` |
|      20 |  8436 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 |  8437 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 |  8438 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 |  8439 | `					}` |
|       3 |  8440 | `				}` |
|      27 |  8441 | `			}` |
|      61 |  8442 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      61 |  8443 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8444 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8445 | `				return SXERR_ABORT;` |
|       - |  8446 | `			}` |
|      33 |  8447 | `		}` |
|       - |  8448 | `	}` |
|  288419 |  8449 | `	if( doBody ){` |
|       - |  8450 | `		/* Compile method body */` |
|  188781 |  8451 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  188781 |  8452 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8453 | `			return SXERR_ABORT;` |
|       - |  8454 | `		}` |
|   94393 |  8455 | `	}else{` |
|       - |  8456 | `		/* Only method signature is allowed */` |
|   99643 |  8457 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  8458 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8459 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  8460 | `				if( rc == SXERR_ABORT ){` |
|       - |  8461 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8462 | `					return SXERR_ABORT;` |
|       - |  8463 | `				}` |
|     ! 0 |  8464 | `				return SXERR_CORRUPT;` |
|       - |  8465 | `			}` |
|       - |  8466 | `	}` |
|       - |  8467 | `	/* All done,install the method */` |
|  288419 |  8468 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  288419 |  8469 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8470 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8471 | `		return SXERR_ABORT;` |
|       - |  8472 | `	}` |
|  288419 |  8473 | `	return SXRET_OK;` |
|       6 |  8474 | `Synchronize:` |
|       - |  8475 | `	/* Synchronize with the first semi-colon */` |
|      40 |  8476 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  8477 | `		pGen->pIn++;` |
|       4 |  8478 | `	}` |
|      16 |  8479 | `	return SXERR_CORRUPT;` |
|  144218 |  8480 | `}` |
|       - |  8481 | `/*` |
|       - |  8482 | ` * Compile an object interface.` |
|       - |  8483 | ` *  According to the PHP language reference manual` |
|       - |  8484 | ` *   Object Interfaces:` |
|       - |  8485 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  8486 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  8487 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  8488 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  8489 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  8490 | ` */` |
|   42214 |  8491 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  8492 | `{` |
|   42219 |  8493 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8494 | `	ph7_class *pClass,*pBase;` |
|       - |  8495 | `	SyToken *pEnd,*pTmp;` |
|       - |  8496 | `	SyString *pName;` |
|       - |  8497 | `	sxi32 nKwrd;` |
|       - |  8498 | `	sxi32 rc;` |
|       - |  8499 | `	/* Jump the 'interface' keyword */` |
|   42219 |  8500 | `	pGen->pIn++;` |
|       - |  8501 | `	/* Extract interface name */` |
|   42219 |  8502 | `	pName = &pGen->pIn->sData;` |
|       - |  8503 | `	/* Advance the stream cursor */` |
|   42219 |  8504 | `	pGen->pIn++;` |
|       - |  8505 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8506 | `		SyBlob sFQN;` |
|       - |  8507 | `		SyString sFQNStr;` |
|   42219 |  8508 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   42219 |  8509 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   42219 |  8510 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   42219 |  8511 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   42219 |  8512 | `		SyBlobRelease(&sFQN);` |
|       - |  8513 | `	}` |
|   42219 |  8514 | `	if( pClass == 0 ){` |
|     ! 0 |  8515 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8516 | `		return SXERR_ABORT;` |
|       - |  8517 | `	}` |
|       - |  8518 | `	/* Mark as an interface */` |
|   42219 |  8519 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  8520 | `	/* Assume no base class is given */` |
|   42219 |  8521 | `	pBase = 0;` |
|   42219 |  8522 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   11503 |  8523 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   11503 |  8524 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  8525 | `			SyBlob sResolved;` |
|       - |  8526 | `			SyString sBaseName;` |
|       - |  8527 | `			sxu32 nRefLine;` |
|       - |  8528 | `			/* Extract base interface */` |
|   11503 |  8529 | `			pGen->pIn++;` |
|   11503 |  8530 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   11503 |  8531 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   11503 |  8532 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8533 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  8534 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8535 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  8536 | `					pName);` |
|     ! 0 |  8537 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8538 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8539 | `					return SXERR_ABORT;` |
|       - |  8540 | `				}` |
|     ! 0 |  8541 | `				return SXRET_OK;` |
|       - |  8542 | `			}` |
|   17252 |  8543 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   11498 |  8544 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   11503 |  8545 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8546 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8547 | `			/* Only interfaces is allowed */` |
|   11503 |  8548 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8549 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8550 | `			}` |
|   11503 |  8551 | `			if( pBase == 0 ){` |
|     ! 0 |  8552 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8553 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  8554 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8555 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8556 | `					return SXERR_ABORT;` |
|       - |  8557 | `				}` |
|     ! 0 |  8558 | `			}` |
|   11503 |  8559 | `			SyBlobRelease(&sResolved);` |
|    5749 |  8560 | `		}` |
|    5749 |  8561 | `	}` |
|   42219 |  8562 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8563 | `		/* Syntax error */` |
|     ! 0 |  8564 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  8565 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8566 | `		if( rc == SXERR_ABORT ){` |
|       - |  8567 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8568 | `			return SXERR_ABORT;` |
|       - |  8569 | `		}` |
|     ! 0 |  8570 | `		return SXRET_OK;` |
|       - |  8571 | `	}` |
|   42219 |  8572 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   42219 |  8573 | `	pEnd = 0; /* cc warning */` |
|       - |  8574 | `	/* Delimit the interface body */` |
|   42219 |  8575 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   42219 |  8576 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8577 | `		/* Syntax error */` |
|     ! 0 |  8578 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  8579 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8580 | `		if( rc == SXERR_ABORT ){` |
|       - |  8581 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8582 | `			return SXERR_ABORT;` |
|       - |  8583 | `		}` |
|     ! 0 |  8584 | `		return SXRET_OK;` |
|       - |  8585 | `	}` |
|       - |  8586 | `	/* Swap token stream */` |
|   42219 |  8587 | `	pTmp = pGen->pEnd;` |
|   42219 |  8588 | `	pGen->pEnd = pEnd;` |
|       - |  8589 | `	/* Start the parse process` |
|       - |  8590 | `	 * Note (According to the PHP reference manual):` |
|       - |  8591 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  8592 | `	 *  Only 'public' visibility is allowed.` |
|       - |  8593 | `	 */` |
|   70921 |  8594 | `	for(;;){` |
|       - |  8595 | `		/* Jump leading/trailing semi-colons */` |
|  241475 |  8596 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   99633 |  8597 | `			pGen->pIn++;` |
|       5 |  8598 | `		}` |
|  141847 |  8599 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8600 | `			/* End of interface body */` |
|   42215 |  8601 | `			break;` |
|       - |  8602 | `		}` |
|   99637 |  8603 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8604 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8605 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  8606 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8607 | `			if( rc == SXERR_ABORT ){` |
|       - |  8608 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8609 | `				return SXERR_ABORT;` |
|       - |  8610 | `			}` |
|     ! 0 |  8611 | `			goto done;` |
|       - |  8612 | `		}` |
|       - |  8613 | `		/* Extract the current keyword */` |
|   99637 |  8614 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   99637 |  8615 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  8616 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  8617 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  8618 | `			const char *zKind = "member";` |
|       3 |  8619 | `			SyString *pMemberName = 0;` |
|       3 |  8620 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  8621 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  8622 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  8623 | `					zKind = "constant";` |
|       3 |  8624 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  8625 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  8626 | `					}` |
|       1 |  8627 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8628 | `					zKind = "method";` |
|     ! 0 |  8629 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  8630 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  8631 | `					}` |
|     ! 0 |  8632 | `				}` |
|       1 |  8633 | `			}` |
|       3 |  8634 | `			if( pMemberName ){` |
|       4 |  8635 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  8636 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  8637 | `			}else{` |
|     ! 0 |  8638 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8639 | `					"Access type for interface %s must be public",zKind);` |
|       - |  8640 | `			}` |
|       3 |  8641 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8642 | `				return SXERR_ABORT;` |
|       - |  8643 | `			}` |
|       3 |  8644 | `			goto done;` |
|       - |  8645 | `		}` |
|   99635 |  8646 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8647 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8648 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8649 | `			if( rc == SXERR_ABORT ){` |
|       - |  8650 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8651 | `				return SXERR_ABORT;` |
|       - |  8652 | `			}` |
|     ! 0 |  8653 | `			goto done;` |
|       - |  8654 | `		}` |
|   99635 |  8655 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  8656 | `			/* Advance the stream cursor */` |
|   99623 |  8657 | `			pGen->pIn++;` |
|   99623 |  8658 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8659 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8660 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8661 | `				if( rc == SXERR_ABORT ){` |
|       - |  8662 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8663 | `					return SXERR_ABORT;` |
|       - |  8664 | `				}` |
|     ! 0 |  8665 | `				goto done;` |
|       - |  8666 | `			}` |
|   99623 |  8667 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   99623 |  8668 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8669 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8670 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8671 | `				if( rc == SXERR_ABORT ){` |
|       - |  8672 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8673 | `					return SXERR_ABORT;` |
|       - |  8674 | `				}` |
|     ! 0 |  8675 | `				goto done;` |
|       - |  8676 | `			}` |
|   49809 |  8677 | `		}` |
|   99635 |  8678 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8679 | `			/* Parse constant */` |
|      10 |  8680 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|      10 |  8681 | `			if( rc != SXRET_OK ){` |
|       3 |  8682 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8683 | `					return SXERR_ABORT;` |
|       - |  8684 | `				}` |
|       3 |  8685 | `				goto done;` |
|       - |  8686 | `			}` |
|       4 |  8687 | `		}else{` |
|   99627 |  8688 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   99627 |  8689 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8690 | `				/* Static method,record that */` |
|   11495 |  8691 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  8692 | `				/* Advance the stream cursor */` |
|   11495 |  8693 | `				pGen->pIn++;` |
|   11490 |  8694 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|   11495 |  8695 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8696 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8697 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8698 | `						if( rc == SXERR_ABORT ){` |
|       - |  8699 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8700 | `							return SXERR_ABORT;` |
|       - |  8701 | `						}` |
|     ! 0 |  8702 | `						goto done;` |
|       - |  8703 | `				}` |
|    5745 |  8704 | `			}` |
|       - |  8705 | `			/* Process method signature (no body for interface methods) */` |
|   99627 |  8706 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   99627 |  8707 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8708 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8709 | `					return SXERR_ABORT;` |
|       - |  8710 | `				}` |
|     ! 0 |  8711 | `				goto done;` |
|       - |  8712 | `			}` |
|       - |  8713 | `		}` |
|       5 |  8714 | `	}` |
|       - |  8715 | `	/* Install the interface */` |
|   42215 |  8716 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   42215 |  8717 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  8718 | `		/* Inherit from the base interface */` |
|   11503 |  8719 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    5749 |  8720 | `	}` |
|   42215 |  8721 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8722 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8723 | `		return SXERR_ABORT;` |
|       - |  8724 | `	}` |
|   21105 |  8725 | `done:` |
|       - |  8726 | `	/* Point beyond the interface body */` |
|   42219 |  8727 | `	pGen->pIn  = &pEnd[1];` |
|   42219 |  8728 | `	pGen->pEnd = pTmp;` |
|   42219 |  8729 | `	return PH7_OK;` |
|   21112 |  8730 | `}` |
|       - |  8731 | `/*` |
|       - |  8732 | ` * Compile a user-defined class.` |
|       - |  8733 | ` * According to the PHP language reference manual` |
|       - |  8734 | ` *  class` |
|       - |  8735 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  8736 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  8737 | ` *  of the properties and methods belonging to the class.` |
|       - |  8738 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  8739 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  8740 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  8741 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  8742 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  8743 | ` *  (called "methods").` |
|       - |  8744 | ` */` |
|       - |  8745 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  8746 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  8747 | `struct TraitUseEntry {` |
|       - |  8748 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  8749 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  8750 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  8751 | `};` |
|       - |  8752 | `/*` |
|       - |  8753 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  8754 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  8755 | ` */` |
|  112356 |  8756 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8757 | `{` |
|       - |  8758 | `	ph7_class **apIface;` |
|       - |  8759 | `	sxu32 nIface,i;` |
|       - |  8760 | `	sxi32 rc;` |
|  112361 |  8761 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  8762 | `		return SXRET_OK;` |
|       - |  8763 | `	}` |
|  112361 |  8764 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  112361 |  8765 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  216031 |  8766 | `	for(i = 0; i < nIface; i++){` |
|  103675 |  8767 | `		ph7_class *pIface = apIface[i];` |
|       - |  8768 | `		SyHashEntry *pEntry;` |
|  103675 |  8769 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  276491 |  8770 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  172821 |  8771 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  8772 | `			ph7_class_method *pImplMeth;` |
|  172821 |  8773 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  8774 | `			/* Find the implementing method in the class */` |
|  172821 |  8775 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  172821 |  8776 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  8777 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  8778 | `			}` |
|       - |  8779 | `			/* Check visibility: interface methods must be implemented as public */` |
|  172807 |  8780 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  8781 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8782 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  8783 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  8784 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8785 | `					return SXERR_ABORT;` |
|       - |  8786 | `				}` |
|       1 |  8787 | `			}` |
|       - |  8788 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  8789 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  8790 | `			 */` |
|       - |  8791 | `			{` |
|  172807 |  8792 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  172807 |  8793 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  172807 |  8794 | `				int sigError = 0;` |
|  172807 |  8795 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  8796 | `					sigError = 1;` |
|  172806 |  8797 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  8798 | `					/* Extra parameters must all have default values */` |
|       6 |  8799 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  8800 | `					sxu32 k;` |
|       8 |  8801 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  8802 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  8803 | `							sigError = 1;` |
|       3 |  8804 | `							break;` |
|       - |  8805 | `						}` |
|       2 |  8806 | `					}` |
|       2 |  8807 | `				}` |
|  172807 |  8808 | `				if( sigError ){` |
|       - |  8809 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  8810 | `					ph7_vm_func_arg *aArgs;` |
|       - |  8811 | `					sxu32 j;` |
|       6 |  8812 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  8813 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  8814 | `					/* Build implementing method signature */` |
|       6 |  8815 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  8816 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  8817 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  8818 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  8819 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8820 | `					}` |
|       - |  8821 | `					/* Build interface method signature */` |
|       6 |  8822 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  8823 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  8824 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  8825 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  8826 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8827 | `					}` |
|       8 |  8828 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8829 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  8830 | `						&pClass->sName,pMName,` |
|       4 |  8831 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  8832 | `						&pIface->sName,pMName,` |
|       4 |  8833 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  8834 | `					SyBlobRelease(&sImplSig);` |
|       6 |  8835 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  8836 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8837 | `						return SXERR_ABORT;` |
|       - |  8838 | `					}` |
|       2 |  8839 | `				}` |
|       - |  8840 | `			}` |
|       5 |  8841 | `		}` |
|   51840 |  8842 | `	}` |
|  112361 |  8843 | `	return SXRET_OK;` |
|   56183 |  8844 | `}` |
|       - |  8845 | `/*` |
|       - |  8846 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  8847 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  8848 | ` */` |
|  112356 |  8849 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8850 | `{` |
|       - |  8851 | `	ph7_class_method *pMeth;` |
|       - |  8852 | `	SyHashEntry *pEntry;` |
|       - |  8853 | `	sxu32 nAbstract;` |
|       - |  8854 | `	SyBlob sMsg;` |
|       - |  8855 | `	sxi32 rc;` |
|       - |  8856 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|  112361 |  8857 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      37 |  8858 | `		return SXRET_OK;` |
|       - |  8859 | `	}` |
|       - |  8860 | `	/* Count abstract methods */` |
|  112329 |  8861 | `	nAbstract = 0;` |
|  112329 |  8862 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
| 1056275 |  8863 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  943951 |  8864 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  943951 |  8865 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  8866 | `			nAbstract++;` |
|       8 |  8867 | `		}` |
|       5 |  8868 | `	}` |
|  112329 |  8869 | `	if( nAbstract == 0 ){` |
|  112315 |  8870 | `		return SXRET_OK;` |
|       - |  8871 | `	}` |
|       - |  8872 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  8873 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  8874 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  8875 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  8876 | `		&pClass->sName,nAbstract,` |
|       7 |  8877 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  8878 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  8879 | `	/* Second pass: list methods with origins */` |
|       - |  8880 | `	{` |
|      18 |  8881 | `		sxu32 nListed = 0;` |
|      18 |  8882 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  8883 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  8884 | `			ph7_class *pOrigin = 0;` |
|       - |  8885 | `			SyString *pMName;` |
|      22 |  8886 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  8887 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  8888 | `				continue;` |
|       - |  8889 | `			}` |
|      20 |  8890 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  8891 | `			if( nListed > 0 ){` |
|       3 |  8892 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  8893 | `			}` |
|       - |  8894 | `			/* Find the origin of this abstract method.` |
|       - |  8895 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  8896 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  8897 | `			 * methods. Abstract class methods only win when the class` |
|       - |  8898 | `			 * itself declared the abstract method (not inherited from` |
|       - |  8899 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  8900 | `			 * class's namespace.` |
|       - |  8901 | `			 */` |
|       - |  8902 | `			{` |
|       - |  8903 | `				ph7_class **apIface;` |
|       - |  8904 | `				ph7_class **apTrait;` |
|       - |  8905 | `				ph7_class *pWalk;` |
|       - |  8906 | `				sxu32 i;` |
|       - |  8907 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  8908 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  8909 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  8910 | `				 */` |
|      20 |  8911 | `				if( pClass->pBase ){` |
|      11 |  8912 | `					pWalk = pClass->pBase;` |
|      19 |  8913 | `					while( pWalk ){` |
|       - |  8914 | `						ph7_class_method *pParentMeth;` |
|      13 |  8915 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  8916 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  8917 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  8918 | `							 * in this class's ancestor chain.` |
|       - |  8919 | `							 */` |
|      13 |  8920 | `							int fromIface = 0;` |
|      13 |  8921 | `							ph7_class *pAnc = pWalk;` |
|      17 |  8922 | `							while( pAnc ){` |
|       - |  8923 | `								ph7_class **apPI;` |
|       - |  8924 | `								sxu32 j;` |
|      15 |  8925 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  8926 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  8927 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  8928 | `										fromIface = 1;` |
|      10 |  8929 | `										break;` |
|       - |  8930 | `									}` |
|     ! 0 |  8931 | `								}` |
|      15 |  8932 | `								if( fromIface ) break;` |
|       6 |  8933 | `								pAnc = pAnc->pBase;` |
|       2 |  8934 | `							}` |
|      13 |  8935 | `							if( !fromIface ){` |
|       3 |  8936 | `								pOrigin = pWalk;` |
|       3 |  8937 | `								break;` |
|       - |  8938 | `							}` |
|       4 |  8939 | `						}` |
|      10 |  8940 | `						pWalk = pWalk->pBase;` |
|       2 |  8941 | `					}` |
|       4 |  8942 | `				}` |
|       - |  8943 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  8944 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  8945 | `				 */` |
|      20 |  8946 | `				if( !pOrigin ){` |
|      18 |  8947 | `					pWalk = pClass;` |
|      40 |  8948 | `					while( pWalk && !pOrigin ){` |
|      26 |  8949 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  8950 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  8951 | `							ph7_class *pIface = apIface[i];` |
|      16 |  8952 | `							ph7_class *pDeepest = 0;` |
|      28 |  8953 | `							while( pIface ){` |
|      16 |  8954 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  8955 | `									pDeepest = pIface;` |
|       6 |  8956 | `								}` |
|      16 |  8957 | `								pIface = pIface->pBase;` |
|       4 |  8958 | `							}` |
|      16 |  8959 | `							if( pDeepest ){` |
|      16 |  8960 | `								pOrigin = pDeepest;` |
|      16 |  8961 | `								break;` |
|       - |  8962 | `							}` |
|     ! 0 |  8963 | `						}` |
|      26 |  8964 | `						pWalk = pWalk->pBase;` |
|       4 |  8965 | `					}` |
|       7 |  8966 | `				}` |
|       - |  8967 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  8968 | `				if( !pOrigin ){` |
|       3 |  8969 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  8970 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  8971 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  8972 | `							pOrigin = pClass;` |
|       3 |  8973 | `							break;` |
|       - |  8974 | `						}` |
|     ! 0 |  8975 | `					}` |
|       1 |  8976 | `				}` |
|       - |  8977 | `			}` |
|      20 |  8978 | `			if( pOrigin ){` |
|      20 |  8979 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  8980 | `			}else{` |
|       - |  8981 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  8982 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  8983 | `			}` |
|      20 |  8984 | `			nListed++;` |
|       4 |  8985 | `		}` |
|       - |  8986 | `	}` |
|      18 |  8987 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  8988 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  8989 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  8990 | `	SyBlobRelease(&sMsg);` |
|      18 |  8991 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8992 | `		return SXERR_ABORT;` |
|       - |  8993 | `	}` |
|      18 |  8994 | `	return SXRET_OK;` |
|   56183 |  8995 | `}` |
|       - |  8996 | `/*` |
|       - |  8997 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  8998 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  8999 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  9000 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  9001 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  9002 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  9003 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  9004 | ` */` |
|  108660 |  9005 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  9006 | `{` |
|  108665 |  9007 | `	int isAbsolute = 0;` |
|  108665 |  9008 | `	SyToken *pStart = pGen->pIn;` |
|       - |  9009 | `	SyBlob sName;` |
|  108665 |  9010 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|    4255 |  9011 | `		isAbsolute = 1;` |
|    4255 |  9012 | `		pGen->pIn++;` |
|    2125 |  9013 | `	}` |
|  108665 |  9014 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       8 |  9015 | `		pGen->pIn = pStart;` |
|       8 |  9016 | `		return SXERR_INVALID;` |
|       - |  9017 | `	}` |
|  108659 |  9018 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|  108659 |  9019 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|  108659 |  9020 | `	pGen->pIn++;` |
|  163002 |  9021 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   54353 |  9022 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      16 |  9023 | `		SyBlobAppend(&sName,"\\",1);` |
|      16 |  9024 | `		pGen->pIn++;` |
|      16 |  9025 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      16 |  9026 | `		pGen->pIn++;` |
|       2 |  9027 | `	}` |
|  108659 |  9028 | `	if( isAbsolute ){` |
|    4253 |  9029 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    2129 |  9030 | `	}else{` |
|       - |  9031 | `		SyString sRaw;` |
|  104411 |  9032 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|  104411 |  9033 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  9034 | `	}` |
|  108659 |  9035 | `	SyBlobRelease(&sName);` |
|  108659 |  9036 | `	return SXRET_OK;` |
|   54335 |  9037 | `}` |
|       - |  9038 | `/*` |
|       - |  9039 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  9040 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  9041 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  9042 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  9043 | ` * either direction cannot run unbounded.` |
|       - |  9044 | ` */` |
|       - |  9045 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   11668 |  9046 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  9047 | `{` |
|       - |  9048 | `	ph7_class **apParent;` |
|       - |  9049 | `	sxu32 n;` |
|   19545 |  9050 | `	while( pInterface ){` |
|   15547 |  9051 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  9052 | `			return FALSE;` |
|       - |  9053 | `		}` |
|   19392 |  9054 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    7690 |  9055 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    7675 |  9056 | `			return TRUE;` |
|       - |  9057 | `		}` |
|    7877 |  9058 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    7877 |  9059 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  9060 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  9061 | `				return TRUE;` |
|       - |  9062 | `			}` |
|     ! 0 |  9063 | `		}` |
|    7877 |  9064 | `		pInterface = pInterface->pBase;` |
|    7877 |  9065 | `		iDepth++;` |
|       5 |  9066 | `	}` |
|    4003 |  9067 | `	return FALSE;` |
|    5839 |  9068 | `}` |
|   11668 |  9069 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  9070 | `{` |
|   11673 |  9071 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  9072 | `}` |
|       - |  9073 | `/*` |
|       - |  9074 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  9075 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  9076 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  9077 | ` */` |
|    7670 |  9078 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  9079 | `{` |
|    7679 |  9080 | `	while( pBase ){` |
|      10 |  9081 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  9082 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  9083 | `			return TRUE;` |
|       - |  9084 | `		}` |
|      10 |  9085 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  9086 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  9087 | `			return TRUE;` |
|       - |  9088 | `		}` |
|       5 |  9089 | `		pBase = pBase->pBase;` |
|       1 |  9090 | `	}` |
|    7671 |  9091 | `	return FALSE;` |
|    3840 |  9092 | `}` |
|       - |  9093 | `/*` |
|       - |  9094 | ` * Compile a class declaration, named or anonymous.` |
|       - |  9095 | ` *` |
|       - |  9096 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - |  9097 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - |  9098 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - |  9099 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - |  9100 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - |  9101 | ` * implements, body, install) is shared by both paths.` |
|       - |  9102 | ` */` |
|  112396 |  9103 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - |  9104 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 |  9105 | `{` |
|  112401 |  9106 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9107 | `	ph7_class *pClass,*pBase;` |
|       - |  9108 | `	SyToken *pEnd,*pTmp;` |
|       - |  9109 | `	sxi32 iProtection;` |
|       - |  9110 | `	SySet aInterfaces;` |
|       - |  9111 | `	SySet aUseEntries;` |
|       - |  9112 | `	sxi32 iAttrflags;` |
|       - |  9113 | `	SyString *pName;` |
|       - |  9114 | `	sxi32 nKwrd;` |
|       - |  9115 | `	sxi32 rc;` |
|       - |  9116 | `	/* Jump the 'class' keyword */` |
|  112401 |  9117 | `	pGen->pIn++;` |
|  112401 |  9118 | `	if( pAnonName ){` |
|       - |  9119 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - |  9120 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - |  9121 | `		 * then use the synthesized name. */` |
|      30 |  9122 | `		*ppArgStart = *ppArgEnd = 0;` |
|      30 |  9123 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       7 |  9124 | `			pGen->pIn++; /* Jump '(' */` |
|       7 |  9125 | `			*ppArgStart = pGen->pIn;` |
|      10 |  9126 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       3 |  9127 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       7 |  9128 | `			pGen->pIn = *ppArgEnd;` |
|       7 |  9129 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       3 |  9130 | `		}` |
|      30 |  9131 | `		pName = pAnonName;` |
|      30 |  9132 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      17 |  9133 | `	}else{` |
|  112375 |  9134 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  9135 | `			/* Syntax error */` |
|     ! 0 |  9136 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  9137 | `			if( rc == SXERR_ABORT ){` |
|       - |  9138 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  9139 | `				return SXERR_ABORT;` |
|       - |  9140 | `			}` |
|       - |  9141 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  9142 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  9143 | `				pGen->pIn++;` |
|     ! 0 |  9144 | `			}` |
|     ! 0 |  9145 | `			return SXRET_OK;` |
|       - |  9146 | `		}` |
|       - |  9147 | `		/* Extract class name */` |
|  112375 |  9148 | `		pName = &pGen->pIn->sData;` |
|       - |  9149 | `		/* Advance the stream cursor */` |
|  112375 |  9150 | `		pGen->pIn++;` |
|       - |  9151 | `		/* Build FQN and obtain a raw class */ {` |
|       - |  9152 | `			SyBlob sFQN;` |
|       - |  9153 | `			SyString sFQNStr;` |
|  112375 |  9154 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|  112375 |  9155 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|  112375 |  9156 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|  112375 |  9157 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|  112375 |  9158 | `			SyBlobRelease(&sFQN);` |
|       - |  9159 | `		}` |
|       - |  9160 | `	}` |
|  112401 |  9161 | `	if( pClass == 0 ){` |
|     ! 0 |  9162 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9163 | `		return SXERR_ABORT;` |
|       - |  9164 | `	}` |
|       - |  9165 | `	/* implemented interfaces and per-use-statement trait containers */` |
|  112401 |  9166 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|  112401 |  9167 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  9168 | `	/* Assume a standalone class */` |
|  112401 |  9169 | `	pBase = 0;` |
|  112401 |  9170 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   96103 |  9171 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   96103 |  9172 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  9173 | `			SyBlob sResolved;` |
|       - |  9174 | `			SyString sBaseName;` |
|       - |  9175 | `			sxu32 nRefLine;` |
|   84455 |  9176 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   84455 |  9177 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   84455 |  9178 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   84455 |  9179 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  9180 | `				SyBlobRelease(&sResolved);` |
|       4 |  9181 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  9182 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  9183 | `					pName);` |
|       3 |  9184 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  9185 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9186 | `					return SXERR_ABORT;` |
|       - |  9187 | `				}` |
|       3 |  9188 | `				return SXRET_OK;` |
|       - |  9189 | `			}` |
|  126677 |  9190 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   84448 |  9191 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   84453 |  9192 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  9193 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  9194 | `			/* Interfaces are not allowed */` |
|   84453 |  9195 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  9196 | `				pBase = pBase->pNextName;` |
|     ! 0 |  9197 | `			}` |
|   84453 |  9198 | `			if( pBase == 0 ){` |
|     ! 0 |  9199 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  9200 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  9201 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9202 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  9203 | `					return SXERR_ABORT;` |
|       - |  9204 | `				}` |
|     ! 0 |  9205 | `			}else{` |
|   84453 |  9206 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  9207 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  9208 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  9209 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9210 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  9211 | `						return SXERR_ABORT;` |
|       - |  9212 | `					}` |
|     ! 0 |  9213 | `				}` |
|       - |  9214 | `			}` |
|   84453 |  9215 | `			SyBlobRelease(&sResolved);` |
|   42224 |  9216 | `		}` |
|   96101 |  9217 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  9218 | `			ph7_class *pInterface;` |
|       - |  9219 | `			/* Interface implementation */` |
|   11661 |  9220 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5840 |  9221 | `			for(;;){` |
|       - |  9222 | `				SyBlob sResolved;` |
|       - |  9223 | `				SyString sIntName;` |
|       - |  9224 | `				sxu32 nRefLine;` |
|   11673 |  9225 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   11673 |  9226 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   11673 |  9227 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  9228 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  9229 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  9230 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  9231 | `						pName);` |
|     ! 0 |  9232 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9233 | `						return SXERR_ABORT;` |
|       - |  9234 | `					}` |
|     ! 0 |  9235 | `					break;` |
|       - |  9236 | `				}` |
|   23341 |  9237 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   11668 |  9238 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   11673 |  9239 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  9240 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  9241 | `				/* Only interfaces are allowed */` |
|   11673 |  9242 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  9243 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  9244 | `				}` |
|   11673 |  9245 | `				if( pInterface == 0 ){` |
|     ! 0 |  9246 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  9247 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  9248 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9249 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  9250 | `						return SXERR_ABORT;` |
|       - |  9251 | `					}` |
|     ! 0 |  9252 | `				}else{` |
|       - |  9253 | `					/* Reject user classes that try to implement Throwable` |
|       - |  9254 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  9255 | `					 * unless they already extend Exception or Error.` |
|       - |  9256 | `					 * Exception and Error themselves are compiled from the` |
|       - |  9257 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  9258 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   11673 |  9259 | `					SyString *pFqn = &pClass->sName;` |
|   11673 |  9260 | `					int bIsExceptionOrError =` |
|    9668 |  9261 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   19421 |  9262 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    9760 |  9263 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3844 |  9264 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   15503 |  9265 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|   11508 |  9266 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3833 |  9267 | `						!bIsExceptionOrError ){` |
|      12 |  9268 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9269 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  9270 | `							&pClass->sName);` |
|       9 |  9271 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9272 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  9273 | `							return SXERR_ABORT;` |
|       - |  9274 | `						}` |
|       - |  9275 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  9276 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  9277 | `					}else{` |
|   11667 |  9278 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  9279 | `					}` |
|       - |  9280 | `				}` |
|   11673 |  9281 | `				SyBlobRelease(&sResolved);` |
|   11673 |  9282 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5833 |  9283 | `					break;` |
|       - |  9284 | `				}` |
|      16 |  9285 | `				pGen->pIn++;/* Jump the comma */` |
|       4 |  9286 | `			}` |
|    5828 |  9287 | `		}` |
|   48048 |  9288 | `	}` |
|  112399 |  9289 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  9290 | `		/* Syntax error */` |
|     ! 0 |  9291 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  9292 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9293 | `		if( rc == SXERR_ABORT ){` |
|       - |  9294 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9295 | `			return SXERR_ABORT;` |
|       - |  9296 | `		}` |
|     ! 0 |  9297 | `		return SXRET_OK;` |
|       - |  9298 | `	}` |
|  112399 |  9299 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|  112399 |  9300 | `	pEnd = 0; /* cc warning */` |
|       - |  9301 | `	/* Delimit the class body */` |
|  112399 |  9302 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|  112399 |  9303 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  9304 | `		/* Syntax error */` |
|     ! 0 |  9305 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  9306 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9307 | `		if( rc == SXERR_ABORT ){` |
|       - |  9308 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9309 | `			return SXERR_ABORT;` |
|       - |  9310 | `		}` |
|     ! 0 |  9311 | `		return SXRET_OK;` |
|       - |  9312 | `	}` |
|       - |  9313 | `	/* Swap token stream */` |
|  112399 |  9314 | `	pTmp = pGen->pEnd;` |
|  112399 |  9315 | `	pGen->pEnd = pEnd;` |
|       - |  9316 | `	/* Set the inherited flags */` |
|  112399 |  9317 | `	pClass->iFlags = iFlags;` |
|       - |  9318 | `	/* Start the parse process */` |
|  150600 |  9319 | `	for(;;){` |
|       - |  9320 | `		/* Jump leading/trailing semi-colons */` |
|  463593 |  9321 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   81239 |  9322 | `			pGen->pIn++;` |
|       5 |  9323 | `		}` |
|  382359 |  9324 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9325 | `			/* End of class body */` |
|  112361 |  9326 | `			break;` |
|       - |  9327 | `		}` |
|  269998 |  9328 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  135004 |  9329 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  9330 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9331 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  9332 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9333 | `			if( rc == SXERR_ABORT ){` |
|       - |  9334 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  9335 | `				return SXERR_ABORT;` |
|       - |  9336 | `			}` |
|     ! 0 |  9337 | `			goto done;` |
|       - |  9338 | `		}` |
|       - |  9339 | `		/* Assume public visibility */` |
|  270003 |  9340 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  270003 |  9341 | `		iAttrflags = 0;` |
|       - |  9342 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  9343 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  9344 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  9345 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  270003 |  9346 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9347 | `			int bMod = 0;` |
|     ! 0 |  9348 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9349 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  9350 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  9351 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  9352 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  9353 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  9354 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  9355 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  9356 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  9357 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  9358 | `			}` |
|     ! 0 |  9359 | `			if( !bMod ){` |
|     ! 0 |  9360 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9361 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9362 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9363 | `						return SXERR_ABORT;` |
|       - |  9364 | `					}` |
|     ! 0 |  9365 | `					goto done;` |
|       - |  9366 | `				}` |
|     ! 0 |  9367 | `				continue;` |
|       - |  9368 | `			}` |
|     ! 0 |  9369 | `		}` |
|  270003 |  9370 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9371 | `			/* Extract the current keyword */` |
|  270003 |  9372 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  270003 |  9373 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9374 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  9375 | `				TraitUseEntry sUse;` |
|      57 |  9376 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      57 |  9377 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      57 |  9378 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      34 |  9379 | `				for(;;){` |
|       - |  9380 | `					ph7_class *pTrait;` |
|       - |  9381 | `					SyString *pTraitName;` |
|      65 |  9382 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9383 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9384 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  9385 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9386 | `							return SXERR_ABORT;` |
|       - |  9387 | `						}` |
|     ! 0 |  9388 | `						break;` |
|       - |  9389 | `					}` |
|      65 |  9390 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  9391 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  9392 | `						SyBlob sResolved;` |
|      65 |  9393 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      65 |  9394 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     125 |  9395 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      60 |  9396 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      65 |  9397 | `						SyBlobRelease(&sResolved);` |
|       - |  9398 | `					}` |
|       - |  9399 | `					/* Only traits are allowed */` |
|      65 |  9400 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9401 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  9402 | `					}` |
|      65 |  9403 | `					if( pTrait == 0 ){` |
|     ! 0 |  9404 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9405 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  9406 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9407 | `							return SXERR_ABORT;` |
|       - |  9408 | `						}` |
|     ! 0 |  9409 | `					}else{` |
|      65 |  9410 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  9411 | `					}` |
|      65 |  9412 | `					pGen->pIn++; /* Advance past trait name */` |
|      65 |  9413 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      31 |  9414 | `						break;` |
|       - |  9415 | `					}` |
|      10 |  9416 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  9417 | `				}` |
|       - |  9418 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      57 |  9419 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  9420 | `					SyToken *pBlock;` |
|      13 |  9421 | `					pGen->pIn++; /* Jump '{' */` |
|      13 |  9422 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      13 |  9423 | `					sUse.pResolvStart = pGen->pIn;` |
|      13 |  9424 | `					sUse.pResolvEnd = pBlock;` |
|      13 |  9425 | `					if( pBlock < pGen->pEnd ){` |
|      13 |  9426 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       8 |  9427 | `					}else{` |
|     ! 0 |  9428 | `						pGen->pIn = pGen->pEnd;` |
|       - |  9429 | `					}` |
|       5 |  9430 | `				}` |
|      57 |  9431 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  9432 | `				/* The semicolon will be consumed by the outer loop */` |
|      57 |  9433 | `				continue;` |
|       - |  9434 | `			}` |
|  269951 |  9435 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  269635 |  9436 | `				iProtection = nKwrd;` |
|  269635 |  9437 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  9438 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  269635 |  9439 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  9440 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  9441 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  9442 | `				}` |
|  269630 |  9443 | `				if( pGen->pIn >= pGen->pEnd` |
|  269635 |  9444 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9445 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9446 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  9447 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9448 | `					if( rc == SXERR_ABORT ){` |
|       - |  9449 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  9450 | `						return SXERR_ABORT;` |
|       - |  9451 | `					}` |
|     ! 0 |  9452 | `					goto done;` |
|       - |  9453 | `				}` |
|  269635 |  9454 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9455 | `					/* Attribute declaration (untyped) */` |
|   80925 |  9456 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   80925 |  9457 | `					if( rc != SXRET_OK ){` |
|      11 |  9458 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9459 | `							return SXERR_ABORT;` |
|       - |  9460 | `						}` |
|      11 |  9461 | `						goto done;` |
|       - |  9462 | `					}` |
|   80917 |  9463 | `					continue;` |
|       - |  9464 | `				}` |
|  188715 |  9465 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9466 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     175 |  9467 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     175 |  9468 | `					if( rc != SXRET_OK ){` |
|       8 |  9469 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9470 | `							return SXERR_ABORT;` |
|       - |  9471 | `						}` |
|       8 |  9472 | `						goto done;` |
|       - |  9473 | `					}` |
|     169 |  9474 | `					continue;` |
|       - |  9475 | `				}` |
|       - |  9476 | `				/* Extract the keyword */` |
|  188545 |  9477 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   94270 |  9478 | `			}` |
|  188861 |  9479 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  9480 | `				/* Process constant declaration */` |
|      85 |  9481 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      85 |  9482 | `				if( rc != SXRET_OK ){` |
|      11 |  9483 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9484 | `						return SXERR_ABORT;` |
|       - |  9485 | `					}` |
|      11 |  9486 | `					goto done;` |
|       - |  9487 | `				}` |
|      41 |  9488 | `			}else{` |
|  188781 |  9489 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  9490 | `					/* Static method or attribute,record that */` |
|   11563 |  9491 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|   11563 |  9492 | `					pGen->pIn++; /* Jump the static keyword */` |
|   11563 |  9493 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9494 | `						/* Extract the keyword */` |
|   11553 |  9495 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   11553 |  9496 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9497 | `							iProtection = nKwrd;` |
|     ! 0 |  9498 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  9499 | `						}` |
|    5774 |  9500 | `					}` |
|       - |  9501 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  9502 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  9503 | `					 * than a generic "expecting method" parse error. */` |
|   11563 |  9504 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9505 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9506 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  9507 | `					}` |
|   11558 |  9508 | `					if( pGen->pIn >= pGen->pEnd` |
|   11563 |  9509 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9510 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9511 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  9512 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9513 | `						if( rc == SXERR_ABORT ){` |
|       - |  9514 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9515 | `							return SXERR_ABORT;` |
|       - |  9516 | `						}` |
|     ! 0 |  9517 | `						goto done;` |
|       - |  9518 | `					}` |
|   11563 |  9519 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9520 | `						/* Attribute declaration */` |
|      11 |  9521 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 |  9522 | `						if( rc != SXRET_OK ){` |
|       3 |  9523 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9524 | `								return SXERR_ABORT;` |
|       - |  9525 | `							}` |
|       3 |  9526 | `							goto done;` |
|       - |  9527 | `						}` |
|       8 |  9528 | `						continue;` |
|       - |  9529 | `					}` |
|   11555 |  9530 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9531 | `						/* Typed static attribute declaration */` |
|      15 |  9532 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  9533 | `						if( rc != SXRET_OK ){` |
|       3 |  9534 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9535 | `								return SXERR_ABORT;` |
|       - |  9536 | `							}` |
|       3 |  9537 | `							goto done;` |
|       - |  9538 | `						}` |
|      13 |  9539 | `						continue;` |
|       - |  9540 | `					}` |
|       - |  9541 | `					/* Extract the keyword */` |
|   11543 |  9542 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  182992 |  9543 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  9544 | `					/* Abstract method,record that */` |
|      15 |  9545 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  9546 | `					/* Mark the whole class as abstract */` |
|      15 |  9547 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  9548 | `					/* Advance the stream cursor */` |
|      15 |  9549 | `					pGen->pIn++;` |
|      15 |  9550 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      15 |  9551 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      15 |  9552 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      13 |  9553 | `							iProtection = nKwrd;` |
|      13 |  9554 | `							pGen->pIn++; /* Jump the visibility token */` |
|       5 |  9555 | `						}` |
|       6 |  9556 | `					}` |
|      15 |  9557 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      12 |  9558 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9559 | `							/* Static method */` |
|     ! 0 |  9560 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9561 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9562 | `					}` |
|      15 |  9563 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      12 |  9564 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9565 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9566 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  9567 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9568 | `							if( rc == SXERR_ABORT ){` |
|       - |  9569 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9570 | `								return SXERR_ABORT;` |
|       - |  9571 | `							}` |
|     ! 0 |  9572 | `							goto done;` |
|       - |  9573 | `					}` |
|      15 |  9574 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  177217 |  9575 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  9576 | `					/* final method ,record that */` |
|      17 |  9577 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      17 |  9578 | `					pGen->pIn++; /* Jump the final keyword */` |
|      17 |  9579 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9580 | `						/* Extract the keyword */` |
|      17 |  9581 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      17 |  9582 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       9 |  9583 | `							iProtection = nKwrd;` |
|       9 |  9584 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  9585 | `						}` |
|       7 |  9586 | `					}` |
|      17 |  9587 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  9588 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  9589 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  9590 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  9591 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  9592 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9593 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  9594 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  9595 | `									return SXERR_ABORT;` |
|       - |  9596 | `								}` |
|     ! 0 |  9597 | `								goto done;` |
|       - |  9598 | `							}` |
|      12 |  9599 | `							continue;` |
|       - |  9600 | `					}` |
|       6 |  9601 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  9602 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9603 | `							/* Static method */` |
|     ! 0 |  9604 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9605 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9606 | `					}` |
|       6 |  9607 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9608 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9609 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9610 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  9611 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9612 | `							if( rc == SXERR_ABORT ){` |
|       - |  9613 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9614 | `								return SXERR_ABORT;` |
|       - |  9615 | `							}` |
|     ! 0 |  9616 | `							goto done;` |
|       - |  9617 | `					}` |
|       6 |  9618 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9619 | `				}` |
|  188751 |  9620 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9621 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9622 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  9623 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9624 | `						if( rc == SXERR_ABORT ){` |
|       - |  9625 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9626 | `							return SXERR_ABORT;` |
|       - |  9627 | `						}` |
|     ! 0 |  9628 | `						goto done;` |
|       - |  9629 | `				}` |
|  188751 |  9630 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  9631 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  9632 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  9633 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9634 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9635 | `						if( rc == SXERR_ABORT ){` |
|       - |  9636 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9637 | `							return SXERR_ABORT;` |
|       - |  9638 | `						}` |
|     ! 0 |  9639 | `						goto done;` |
|       - |  9640 | `					}` |
|       - |  9641 | `					/* Attribute declaration */` |
|       7 |  9642 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  9643 | `				}else{` |
|       - |  9644 | `					/* Process method declaration */` |
|  188745 |  9645 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9646 | `				}` |
|  188751 |  9647 | `				if( rc != SXRET_OK ){` |
|      16 |  9648 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9649 | `						return SXERR_ABORT;` |
|       - |  9650 | `					}` |
|      16 |  9651 | `					goto done;` |
|       - |  9652 | `				}` |
|       - |  9653 | `			}` |
|   94408 |  9654 | `		}else{` |
|       - |  9655 | `			/* Attribute declaration */` |
|     ! 0 |  9656 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9657 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9658 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9659 | `					return SXERR_ABORT;` |
|       - |  9660 | `				}` |
|     ! 0 |  9661 | `				goto done;` |
|       - |  9662 | `			}` |
|       - |  9663 | `		}` |
|       5 |  9664 | `	}` |
|       - |  9665 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  9666 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  9667 | `	 */` |
|       - |  9668 | `	{` |
|       - |  9669 | `		TraitUseEntry *apUse;` |
|       - |  9670 | `		sxu32 nU;` |
|  112361 |  9671 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|  112413 |  9672 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      57 |  9673 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      57 |  9674 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      57 |  9675 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      57 |  9676 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  9677 | `			sxu32 nT;` |
|      57 |  9678 | `			if( !hasResolution ){` |
|       - |  9679 | `				/* No conflict resolution block: use standard trait application */` |
|      95 |  9680 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      53 |  9681 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      53 |  9682 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9683 | `						break;` |
|       - |  9684 | `					}` |
|      29 |  9685 | `				}` |
|      26 |  9686 | `			}else{` |
|       - |  9687 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  9688 | `				 * then use the block to resolve method conflicts.` |
|       - |  9689 | `				 */` |
|       - |  9690 | `				SyToken *pR;` |
|      25 |  9691 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      15 |  9692 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  9693 | `					ph7_class_attr *pAR;` |
|       - |  9694 | `					SyHashEntry *pER;` |
|       - |  9695 | `					SyString *pNR;` |
|      15 |  9696 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      21 |  9697 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  9698 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  9699 | `						pNR = &pAR->sName;` |
|     ! 0 |  9700 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  9701 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  9702 | `						}` |
|     ! 0 |  9703 | `					}` |
|      15 |  9704 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       9 |  9705 | `				}` |
|       - |  9706 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      13 |  9707 | `				pR = pUse->pResolvStart;` |
|      27 |  9708 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9709 | `					SyString sTrait,sMethod;` |
|       - |  9710 | `					ph7_class *pSrcTrait;` |
|       - |  9711 | `					ph7_class_method *pMeth;` |
|       - |  9712 | `					sxi32 nRKwrd;` |
|      41 |  9713 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9714 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9715 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9716 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9717 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9718 | `					sMethod = pR->sData;` |
|      17 |  9719 | `					pR++;` |
|      17 |  9720 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9721 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9722 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9723 | `							sTrait = sMethod;` |
|       7 |  9724 | `							pR++;` |
|       7 |  9725 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9726 | `							sMethod = pR->sData;` |
|       7 |  9727 | `							pR++;` |
|       3 |  9728 | `						}` |
|       3 |  9729 | `					}` |
|      17 |  9730 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9731 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9732 | `						continue;` |
|       - |  9733 | `					}` |
|      17 |  9734 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9735 | `					pR++;` |
|      17 |  9736 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  9737 | `						pSrcTrait = 0;` |
|       7 |  9738 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  9739 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  9740 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  9741 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  9742 | `								pSrcTrait = apTrait[nT];` |
|       5 |  9743 | `								break;` |
|       - |  9744 | `							}` |
|       2 |  9745 | `						}` |
|       5 |  9746 | `						if( pSrcTrait ){` |
|       5 |  9747 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  9748 | `							if( pMeth ){` |
|       5 |  9749 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  9750 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  9751 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  9752 | `								}` |
|       2 |  9753 | `							}` |
|       2 |  9754 | `						}` |
|       2 |  9755 | `					}` |
|      35 |  9756 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9757 | `				}` |
|       - |  9758 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      25 |  9759 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  9760 | `					ph7_class_method *pMR;` |
|       - |  9761 | `					SyHashEntry *pER;` |
|       - |  9762 | `					SyString *pNR;` |
|      15 |  9763 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      41 |  9764 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      23 |  9765 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      23 |  9766 | `						pNR = &pMR->sFunc.sName;` |
|      23 |  9767 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      14 |  9768 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       6 |  9769 | `						}` |
|       3 |  9770 | `					}` |
|       9 |  9771 | `				}` |
|       - |  9772 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      13 |  9773 | `				pR = pUse->pResolvStart;` |
|      27 |  9774 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9775 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  9776 | `					ph7_class *pSrcTrait;` |
|       - |  9777 | `					ph7_class_method *pMeth;` |
|      27 |  9778 | `					int hasQual = 0;` |
|       - |  9779 | `					sxi32 nRKwrd;` |
|      41 |  9780 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9781 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9782 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9783 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9784 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      17 |  9785 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9786 | `					sMethod = pR->sData;` |
|      17 |  9787 | `					pR++;` |
|      17 |  9788 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9789 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9790 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9791 | `							sTrait = sMethod;` |
|       7 |  9792 | `							hasQual = 1;` |
|       7 |  9793 | `							pR++;` |
|       7 |  9794 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9795 | `							sMethod = pR->sData;` |
|       7 |  9796 | `							pR++;` |
|       3 |  9797 | `						}` |
|       3 |  9798 | `					}` |
|      17 |  9799 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9800 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9801 | `						continue;` |
|       - |  9802 | `					}` |
|      17 |  9803 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9804 | `					pR++;` |
|      17 |  9805 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      13 |  9806 | `						sxi32 iNewVis = -1;` |
|      13 |  9807 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  9808 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  9809 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  9810 | `								iNewVis = nAK;` |
|       7 |  9811 | `								pR++;` |
|       3 |  9812 | `							}` |
|       3 |  9813 | `						}` |
|      13 |  9814 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      11 |  9815 | `							sAlias = pR->sData;` |
|      11 |  9816 | `							pR++;` |
|       4 |  9817 | `						}` |
|      13 |  9818 | `						pMeth = 0;` |
|      13 |  9819 | `						if( hasQual ){` |
|       3 |  9820 | `							pSrcTrait = 0;` |
|       5 |  9821 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  9822 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  9823 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  9824 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  9825 | `									pSrcTrait = apTrait[nT];` |
|       3 |  9826 | `									break;` |
|       - |  9827 | `								}` |
|       2 |  9828 | `							}` |
|       3 |  9829 | `							if( pSrcTrait ){` |
|       3 |  9830 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  9831 | `							}` |
|       2 |  9832 | `						}else{` |
|      10 |  9833 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  9834 | `						}` |
|      13 |  9835 | `						if( pMeth ){` |
|      13 |  9836 | `							if( sAlias.nByte > 0 ){` |
|       - |  9837 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  9838 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  9839 | `								 */` |
|       - |  9840 | `								ph7_class_method *pAlias;` |
|       - |  9841 | `								char *zAliasDup;` |
|      11 |  9842 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      11 |  9843 | `								if( pAlias ){` |
|      11 |  9844 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      11 |  9845 | `									if( iNewVis >= 0 ){` |
|       5 |  9846 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9847 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9848 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  9849 | `									}` |
|      11 |  9850 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      11 |  9851 | `									if( zAliasDup ){` |
|      11 |  9852 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       4 |  9853 | `									}` |
|       7 |  9854 | `								}` |
|       7 |  9855 | `							}else if( iNewVis >= 0 ){` |
|       - |  9856 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  9857 | `								ph7_class_method *pCopy;` |
|       3 |  9858 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  9859 | `								if( pCopy ){` |
|       3 |  9860 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  9861 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  9862 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9863 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9864 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  9865 | `									/* Replace the method in the class hash */` |
|       3 |  9866 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  9867 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  9868 | `								}` |
|       1 |  9869 | `							}` |
|       5 |  9870 | `						}` |
|       5 |  9871 | `						SXUNUSED(hasQual);` |
|       5 |  9872 | `					}` |
|      21 |  9873 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9874 | `				}` |
|       - |  9875 | `			}` |
|      57 |  9876 | `			SySetRelease(&pUse->aTraits);` |
|      31 |  9877 | `		}` |
|       - |  9878 | `	}` |
|       - |  9879 | `	/* Install the class */` |
|  112361 |  9880 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|  112361 |  9881 | `	if( rc == SXRET_OK ){` |
|       - |  9882 | `		ph7_class **apInterface;` |
|       - |  9883 | `		sxu32 n;` |
|  112361 |  9884 | `		if( pBase ){` |
|       - |  9885 | `			/* Inherit from base class and mark as a subclass */` |
|   84453 |  9886 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   42224 |  9887 | `		}` |
|  112361 |  9888 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  124023 |  9889 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  9890 | `			/* Implements one or more interface */` |
|   11667 |  9891 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   11667 |  9892 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9893 | `				break;` |
|       - |  9894 | `			}` |
|    5836 |  9895 | `		}` |
|       - |  9896 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  9897 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  112356 |  9898 | `		if( rc == SXRET_OK` |
|  112356 |  9899 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|  112361 |  9900 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   92015 |  9901 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  9902 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   92015 |  9903 | `			if( pStringable ){` |
|   92015 |  9904 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   92015 |  9905 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  9906 | `				sxu32 i;` |
|   92015 |  9907 | `				int bAlready = 0;` |
|   99679 |  9908 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    7671 |  9909 | `					if( apImpl[i] == pStringable ){` |
|       3 |  9910 | `						bAlready = 1;` |
|       3 |  9911 | `						break;` |
|       - |  9912 | `					}` |
|    3837 |  9913 | `				}` |
|   92015 |  9914 | `				if( !bAlready ){` |
|   92013 |  9915 | `					PH7_ClassImplement(pClass,pStringable);` |
|   46004 |  9916 | `				}` |
|   46005 |  9917 | `			}` |
|   46005 |  9918 | `		}` |
|       - |  9919 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|  112361 |  9920 | `		if( rc == SXRET_OK ){` |
|  112361 |  9921 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|  112361 |  9922 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9923 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9924 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9925 | `				return SXERR_ABORT;` |
|       - |  9926 | `			}` |
|   56178 |  9927 | `		}` |
|       - |  9928 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|  112361 |  9929 | `		if( rc == SXRET_OK ){` |
|  112361 |  9930 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|  112361 |  9931 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9932 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9933 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9934 | `				return SXERR_ABORT;` |
|       - |  9935 | `			}` |
|   56178 |  9936 | `		}` |
|   56178 |  9937 | `	}` |
|  112361 |  9938 | `	SySetRelease(&aUseEntries);` |
|  112361 |  9939 | `	SySetRelease(&aInterfaces);` |
|  112361 |  9940 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9941 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9942 | `		return SXERR_ABORT;` |
|       - |  9943 | `	}` |
|   56178 |  9944 | `done:` |
|       - |  9945 | `	/* Point beyond the class body */` |
|  112399 |  9946 | `	pGen->pIn = &pEnd[1];` |
|  112399 |  9947 | `	pGen->pEnd = pTmp;` |
|  112399 |  9948 | `	return PH7_OK;` |
|   56203 |  9949 | `}` |
|       - |  9950 | `/* Compile a named class declaration (the common case). */` |
|  112370 |  9951 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  9952 | `{` |
|  112375 |  9953 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 |  9954 | `}` |
|       - |  9955 | `/*` |
|       - |  9956 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - |  9957 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - |  9958 | ` * compile + install the class body once (at compile time, like every other` |
|       - |  9959 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - |  9960 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - |  9961 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - |  9962 | ` */` |
|      26 |  9963 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  9964 | `{` |
|       - |  9965 | `	char zName[128];         /* Synthesized class name */` |
|       - |  9966 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - |  9967 | `	SyString sName;` |
|       - |  9968 | `	SyToken *pArgStart,*pArgEnd;` |
|       - |  9969 | `	ph7_value *pObj;` |
|      30 |  9970 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9971 | `	sxu32 nIdx,nLen;` |
|       - |  9972 | `	sxi32 nArg,rc;` |
|      13 |  9973 | `	SXUNUSED(iCompileFlag);` |
|       - |  9974 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|      30 |  9975 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      30 |  9976 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  9977 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 |  9978 | `	}` |
|      30 |  9979 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  9980 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - |  9981 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - |  9982 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|      30 |  9983 | `	pArgStart = pArgEnd = 0;` |
|      30 |  9984 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      30 |  9985 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9986 | `		return rc;` |
|       - |  9987 | `	}` |
|       - |  9988 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - |  9989 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      30 |  9990 | `	nArg = 0;` |
|      30 |  9991 | `	if( pArgStart < pArgEnd ){` |
|       7 |  9992 | `		SyToken *pSavedIn = pGen->pIn;` |
|       7 |  9993 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  9994 | `		SyToken *pArgNext;` |
|       7 |  9995 | `		pGen->pIn = pArgStart;` |
|       7 |  9996 | `		pGen->pEnd = pArgEnd;` |
|      13 |  9997 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       7 |  9998 | `			if( pGen->pIn < pArgNext ){` |
|       7 |  9999 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       7 | 10000 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10001 | `					pGen->pIn = pSavedIn;` |
|     ! 0 | 10002 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 | 10003 | `					return SXERR_ABORT;` |
|       - | 10004 | `				}` |
|       7 | 10005 | `				nArg++;` |
|       3 | 10006 | `			}` |
|       7 | 10007 | `			pGen->pIn = &pArgNext[1];` |
|       1 | 10008 | `		}` |
|       7 | 10009 | `		pGen->pIn = pSavedIn;` |
|       7 | 10010 | `		pGen->pEnd = pSavedEnd;` |
|       3 | 10011 | `	}` |
|       - | 10012 | `	/* Load the synthesized class name */` |
|      30 | 10013 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      30 | 10014 | `	if( pObj == 0 ){` |
|     ! 0 | 10015 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10016 | `		return SXERR_ABORT;` |
|       - | 10017 | `	}` |
|      30 | 10018 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      30 | 10019 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 10020 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      30 | 10021 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      30 | 10022 | `	return SXRET_OK;` |
|      17 | 10023 | `}` |
|       - | 10024 | `/*` |
|       - | 10025 | ` * Compile a user-defined abstract class.` |
|       - | 10026 | ` *  According to the PHP language reference manual` |
|       - | 10027 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 10028 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 10029 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 10030 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 10031 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 10032 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 10033 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 10034 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 10035 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 10036 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 10037 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 10038 | ` *   could differ.` |
|       - | 10039 | ` */` |
|       - | 10040 | `/*` |
|       - | 10041 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - | 10042 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - | 10043 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - | 10044 | ` */` |
| 1057576 | 10045 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 | 10046 | `{` |
| 1057581 | 10047 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  709811 | 10048 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  709811 | 10049 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  702133 | 10050 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  351033 | 10051 | `	}` |
| 1049841 | 10052 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
| 1049781 | 10053 | `	return FALSE;` |
|  528793 | 10054 | `}` |
|       - | 10055 | `/*` |
|       - | 10056 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - | 10057 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - | 10058 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - | 10059 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - | 10060 | ` */` |
| 1049776 | 10061 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 | 10062 | `{` |
| 1049781 | 10063 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
| 1049781 | 10064 | `	sxi32 iFlags = 0,iFlag;` |
| 1057581 | 10065 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    7805 | 10066 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 | 10067 | `			pDup = pIn;` |
|       2 | 10068 | `		}` |
|    7805 | 10069 | `		iFlags \|= iFlag;` |
|    7805 | 10070 | `		pIn++;` |
|       5 | 10071 | `	}` |
| 1049781 | 10072 | `	*ppIn = pIn;` |
| 1049781 | 10073 | `	if( ppDup ){ *ppDup = pDup; }` |
| 1049781 | 10074 | `	return iFlags;` |
|       5 | 10075 | `}` |
|       - | 10076 | `/*` |
|       - | 10077 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - | 10078 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - | 10079 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - | 10080 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - | 10081 | `` * `readonly`) to their existing handlers.`` |
|       - | 10082 | ` */` |
| 1045886 | 10083 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 | 10084 | `{` |
| 1045891 | 10085 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  526840 | 10086 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
| 1047833 | 10087 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 | 10088 | `}` |
|       - | 10089 | `/*` |
|       - | 10090 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - | 10091 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - | 10092 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - | 10093 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - | 10094 | `` * `abstract`+`final` pair, like PHP.`` |
|       - | 10095 | ` */` |
|    3890 | 10096 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 | 10097 | `{` |
|       - | 10098 | `	SyToken *pDup;` |
|    3895 | 10099 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - | 10100 | `	sxi32 rc;` |
|    3895 | 10101 | `	if( pDup ){` |
|       4 | 10102 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 | 10103 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 | 10104 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10105 | `			return SXERR_ABORT;` |
|       - | 10106 | `		}` |
|       1 | 10107 | `	}` |
|    3890 | 10108 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    1950 | 10109 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 | 10110 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10111 | `			"Cannot use the final modifier on an abstract class");` |
|       3 | 10112 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10113 | `			return SXERR_ABORT;` |
|       - | 10114 | `		}` |
|       1 | 10115 | `	}` |
|    3895 | 10116 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    1950 | 10117 | `}` |
|       - | 10118 | `/*` |
|       - | 10119 | ` * Compile a user-defined trait.` |
|       - | 10120 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 10121 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 10122 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 10123 | ` */` |
|      64 | 10124 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 | 10125 | `{` |
|      69 | 10126 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10127 | `	ph7_class *pClass;` |
|       - | 10128 | `	SyToken *pEnd,*pTmp;` |
|       - | 10129 | `	sxi32 iProtection;` |
|       - | 10130 | `	sxi32 iAttrflags;` |
|       - | 10131 | `	SyString *pName;` |
|       - | 10132 | `	sxi32 nKwrd;` |
|       - | 10133 | `	sxi32 rc;` |
|       - | 10134 | `	/* Jump the 'trait' keyword */` |
|      69 | 10135 | `	pGen->pIn++;` |
|      69 | 10136 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 10137 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 10138 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10139 | `			return SXERR_ABORT;` |
|       - | 10140 | `		}` |
|     ! 0 | 10141 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 10142 | `			pGen->pIn++;` |
|     ! 0 | 10143 | `		}` |
|     ! 0 | 10144 | `		return SXRET_OK;` |
|       - | 10145 | `	}` |
|       - | 10146 | `	/* Extract trait name */` |
|      69 | 10147 | `	pName = &pGen->pIn->sData;` |
|      69 | 10148 | `	pGen->pIn++;` |
|       - | 10149 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 10150 | `		SyBlob sFQN;` |
|       - | 10151 | `		SyString sFQNStr;` |
|      69 | 10152 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      69 | 10153 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      69 | 10154 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      69 | 10155 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      69 | 10156 | `		SyBlobRelease(&sFQN);` |
|       - | 10157 | `	}` |
|      69 | 10158 | `	if( pClass == 0 ){` |
|     ! 0 | 10159 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10160 | `		return SXERR_ABORT;` |
|       - | 10161 | `	}` |
|       - | 10162 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      69 | 10163 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 10164 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 10165 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 10166 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10167 | `			return SXERR_ABORT;` |
|       - | 10168 | `		}` |
|     ! 0 | 10169 | `		return SXRET_OK;` |
|       - | 10170 | `	}` |
|      69 | 10171 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      69 | 10172 | `	pEnd = 0;` |
|      69 | 10173 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      69 | 10174 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 10175 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 10176 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 10177 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10178 | `			return SXERR_ABORT;` |
|       - | 10179 | `		}` |
|     ! 0 | 10180 | `		return SXRET_OK;` |
|       - | 10181 | `	}` |
|       - | 10182 | `	/* Swap token stream */` |
|      69 | 10183 | `	pTmp = pGen->pEnd;` |
|      69 | 10184 | `	pGen->pEnd = pEnd;` |
|       - | 10185 | `	/* Mark as trait */` |
|      69 | 10186 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 10187 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      64 | 10188 | `	for(;;){` |
|     177 | 10189 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 | 10190 | `			pGen->pIn++;` |
|       4 | 10191 | `		}` |
|     153 | 10192 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      69 | 10193 | `			break;` |
|       - | 10194 | `		}` |
|      89 | 10195 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 10196 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10197 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 10198 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 10199 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10200 | `				return SXERR_ABORT;` |
|       - | 10201 | `			}` |
|     ! 0 | 10202 | `			goto done;` |
|       - | 10203 | `		}` |
|      89 | 10204 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      89 | 10205 | `		iAttrflags = 0;` |
|      89 | 10206 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      89 | 10207 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      89 | 10208 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 10209 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 10210 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 10211 | `				for(;;){` |
|       - | 10212 | `					ph7_class *pUsedTrait;` |
|       - | 10213 | `					SyString *pUsedName;` |
|       5 | 10214 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 10215 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 10216 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 10217 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10218 | `							return SXERR_ABORT;` |
|       - | 10219 | `						}` |
|     ! 0 | 10220 | `						break;` |
|       - | 10221 | `					}` |
|       5 | 10222 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 10223 | `					{` |
|       - | 10224 | `						SyBlob sResolved;` |
|       5 | 10225 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 10226 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 10227 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 10228 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 10229 | `						SyBlobRelease(&sResolved);` |
|       - | 10230 | `					}` |
|       5 | 10231 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 10232 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 10233 | `					}` |
|       5 | 10234 | `					if( pUsedTrait == 0 ){` |
|       4 | 10235 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 10236 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 10237 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10238 | `							return SXERR_ABORT;` |
|       - | 10239 | `						}` |
|       2 | 10240 | `					}else{` |
|       3 | 10241 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 10242 | `					}` |
|       5 | 10243 | `					pGen->pIn++;` |
|       5 | 10244 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 10245 | `						break;` |
|       - | 10246 | `					}` |
|     ! 0 | 10247 | `					pGen->pIn++;` |
|     ! 0 | 10248 | `				}` |
|       5 | 10249 | `				continue;` |
|       - | 10250 | `			}` |
|      85 | 10251 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 | 10252 | `				iProtection = nKwrd;` |
|      73 | 10253 | `				pGen->pIn++;` |
|      68 | 10254 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 | 10255 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 10256 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10257 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 10258 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 10259 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10260 | `						return SXERR_ABORT;` |
|       - | 10261 | `					}` |
|     ! 0 | 10262 | `					goto done;` |
|       - | 10263 | `				}` |
|      73 | 10264 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 | 10265 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 | 10266 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 10267 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10268 | `							return SXERR_ABORT;` |
|       - | 10269 | `						}` |
|     ! 0 | 10270 | `						goto done;` |
|       - | 10271 | `					}` |
|      12 | 10272 | `					continue;` |
|       - | 10273 | `				}` |
|      63 | 10274 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 | 10275 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 10276 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 10277 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10278 | `							return SXERR_ABORT;` |
|       - | 10279 | `						}` |
|     ! 0 | 10280 | `						goto done;` |
|       - | 10281 | `					}` |
|       5 | 10282 | `					continue;` |
|       - | 10283 | `				}` |
|      58 | 10284 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 | 10285 | `			}` |
|      71 | 10286 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 10287 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10288 | `					"Traits cannot have constants");` |
|     ! 0 | 10289 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10290 | `					return SXERR_ABORT;` |
|       - | 10291 | `				}` |
|     ! 0 | 10292 | `				goto done;` |
|     ! 0 | 10293 | `			}else{` |
|      71 | 10294 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 10295 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 10296 | `					pGen->pIn++;` |
|       5 | 10297 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 10298 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 10299 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 10300 | `							iProtection = nKwrd;` |
|     ! 0 | 10301 | `							pGen->pIn++;` |
|     ! 0 | 10302 | `						}` |
|       1 | 10303 | `					}` |
|       4 | 10304 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 | 10305 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 10306 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10307 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 10308 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 10309 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10310 | `							return SXERR_ABORT;` |
|       - | 10311 | `						}` |
|     ! 0 | 10312 | `						goto done;` |
|       - | 10313 | `					}` |
|       5 | 10314 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 10315 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 10316 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 10317 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 10318 | `								return SXERR_ABORT;` |
|       - | 10319 | `							}` |
|     ! 0 | 10320 | `							goto done;` |
|       - | 10321 | `						}` |
|       3 | 10322 | `						continue;` |
|       - | 10323 | `					}` |
|       3 | 10324 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 | 10325 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10326 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 10327 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 10328 | `								return SXERR_ABORT;` |
|       - | 10329 | `							}` |
|     ! 0 | 10330 | `							goto done;` |
|       - | 10331 | `						}` |
|     ! 0 | 10332 | `						continue;` |
|       - | 10333 | `					}` |
|       3 | 10334 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      68 | 10335 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 | 10336 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 | 10337 | `					pGen->pIn++;` |
|       6 | 10338 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 | 10339 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 | 10340 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 | 10341 | `							iProtection = nKwrd;` |
|       6 | 10342 | `							pGen->pIn++;` |
|       2 | 10343 | `						}` |
|       2 | 10344 | `					}` |
|       6 | 10345 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 10346 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 10347 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10348 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 10349 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 10350 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10351 | `							return SXERR_ABORT;` |
|       - | 10352 | `						}` |
|     ! 0 | 10353 | `						goto done;` |
|       - | 10354 | `					}` |
|       6 | 10355 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 10356 | `				}` |
|      69 | 10357 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 10358 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10359 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 10360 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 10361 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10362 | `						return SXERR_ABORT;` |
|       - | 10363 | `					}` |
|     ! 0 | 10364 | `					goto done;` |
|       - | 10365 | `				}` |
|      69 | 10366 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 10367 | `					pGen->pIn++;` |
|     ! 0 | 10368 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 10369 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10370 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 10371 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10372 | `							return SXERR_ABORT;` |
|       - | 10373 | `						}` |
|     ! 0 | 10374 | `						goto done;` |
|       - | 10375 | `					}` |
|     ! 0 | 10376 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10377 | `				}else{` |
|      69 | 10378 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 10379 | `				}` |
|      69 | 10380 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10381 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10382 | `						return SXERR_ABORT;` |
|       - | 10383 | `					}` |
|     ! 0 | 10384 | `					goto done;` |
|       - | 10385 | `				}` |
|       - | 10386 | `			}` |
|      37 | 10387 | `		}else{` |
|     ! 0 | 10388 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10389 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10390 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10391 | `					return SXERR_ABORT;` |
|       - | 10392 | `				}` |
|     ! 0 | 10393 | `				goto done;` |
|       - | 10394 | `			}` |
|       - | 10395 | `		}` |
|       5 | 10396 | `	}` |
|       - | 10397 | `	/* Install the trait */` |
|      69 | 10398 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      69 | 10399 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10400 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10401 | `		return SXERR_ABORT;` |
|       - | 10402 | `	}` |
|      32 | 10403 | `done:` |
|       - | 10404 | `	/* Point beyond the trait body */` |
|      69 | 10405 | `	pGen->pIn = &pEnd[1];` |
|      69 | 10406 | `	pGen->pEnd = pTmp;` |
|      69 | 10407 | `	return PH7_OK;` |
|      37 | 10408 | `}` |
|       - | 10409 | `/*` |
|       - | 10410 | ` * Compile a user-defined class.` |
|       - | 10411 | ` *  According to the PHP language reference manual` |
|       - | 10412 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 10413 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 10414 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 10415 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 10416 | ` *   and functions (called "methods").` |
|       - | 10417 | ` */` |
|  108480 | 10418 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 | 10419 | `{` |
|       - | 10420 | `	sxi32 rc;` |
|  108485 | 10421 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|  108485 | 10422 | `	return rc;` |
|       5 | 10423 | `}` |
|       - | 10424 | `/*` |
|       - | 10425 | ` * Exception handling.` |
|       - | 10426 | ` *  According to the PHP language reference manual` |
|       - | 10427 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 10428 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 10429 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 10430 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 10431 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 10432 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 10433 | ` *    (or re-thrown) within a catch block.` |
|       - | 10434 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 10435 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 10436 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 10437 | ` *    been defined with set_exception_handler().` |
|       - | 10438 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 10439 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 10440 | ` */` |
|       - | 10441 | `/*` |
|       - | 10442 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 10443 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 10444 | ` * indicates failure.` |
|       - | 10445 | ` */` |
|   15694 | 10446 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 10447 | `{` |
|   15699 | 10448 | `	sxi32 rc = SXRET_OK;` |
|   15699 | 10449 | `	if( pRoot->pOp ){` |
|   15687 | 10450 | `		switch( pRoot->pOp->iOp ){` |
|    7841 | 10451 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - | 10452 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - | 10453 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - | 10454 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - | 10455 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - | 10456 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   15687 | 10457 | `			break;` |
|     ! 0 | 10458 | `		default:` |
|       - | 10459 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - | 10460 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - | 10461 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 | 10462 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 10463 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 10464 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 10465 | `				rc = SXERR_INVALID;` |
|     ! 0 | 10466 | `			}` |
|     ! 0 | 10467 | `			break;` |
|       - | 10468 | `		}` |
|    7858 | 10469 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 10470 | `		/* Unexpected expression */` |
|     ! 0 | 10471 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 10472 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10473 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 10474 | `			rc = SXERR_INVALID;` |
|     ! 0 | 10475 | `		}` |
|     ! 0 | 10476 | `	}` |
|   15699 | 10477 | `	return rc;` |
|       5 | 10478 | `}` |
|       - | 10479 | `/*` |
|       - | 10480 | ` * Compile a 'throw' statement.` |
|       - | 10481 | ` * throw: This is how you trigger an exception.` |
|       - | 10482 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 10483 | ` */` |
|   15658 | 10484 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 | 10485 | `{` |
|   15663 | 10486 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10487 | `	GenBlock *pBlock;` |
|       - | 10488 | `	sxu32 nIdx;` |
|       - | 10489 | `	sxi32 rc;` |
|   15663 | 10490 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 10491 | `	/* Compile the expression */` |
|   15663 | 10492 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   15663 | 10493 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10494 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 10495 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10496 | `			return SXERR_ABORT;` |
|       - | 10497 | `		}` |
|     ! 0 | 10498 | `		return SXRET_OK;` |
|       - | 10499 | `	}` |
|   15663 | 10500 | `	pBlock = pGen->pCurrent;` |
|       - | 10501 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   61977 | 10502 | `	while(pBlock->pParent){` |
|   61973 | 10503 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   15659 | 10504 | `			break;` |
|       - | 10505 | `		}` |
|       - | 10506 | `		/* Point to the parent block */` |
|   46319 | 10507 | `		pBlock = pBlock->pParent;` |
|       5 | 10508 | `	}` |
|       - | 10509 | `	/* Emit the throw instruction */` |
|   15663 | 10510 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 10511 | `	/* Emit the jump */` |
|   15663 | 10512 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   15663 | 10513 | `	return SXRET_OK;` |
|    7834 | 10514 | `}` |
|       - | 10515 | `/*` |
|       - | 10516 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - | 10517 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - | 10518 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - | 10519 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - | 10520 | ` * the validator guarantees the operand is a valid exception target.` |
|       - | 10521 | ` */` |
|      36 | 10522 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 10523 | `{` |
|      38 | 10524 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10525 | `	GenBlock *pBlock;` |
|       - | 10526 | `	sxu32 nIdx;` |
|       - | 10527 | `	sxi32 rc;` |
|      18 | 10528 | `	(void)iCompileFlag;` |
|      38 | 10529 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 | 10530 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 10531 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10532 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10533 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10534 | `			return SXERR_ABORT;` |
|       - | 10535 | `		}` |
|     ! 0 | 10536 | `		return SXRET_OK;` |
|       - | 10537 | `	}` |
|      38 | 10538 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 | 10539 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10540 | `		return SXERR_ABORT;` |
|       - | 10541 | `	}` |
|      38 | 10542 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10543 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10544 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10545 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10546 | `			return SXERR_ABORT;` |
|       - | 10547 | `		}` |
|     ! 0 | 10548 | `		return SXRET_OK;` |
|       - | 10549 | `	}` |
|       - | 10550 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 | 10551 | `	pBlock = pGen->pCurrent;` |
|      60 | 10552 | `	while( pBlock->pParent ){` |
|      49 | 10553 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 | 10554 | `			break;` |
|       - | 10555 | `		}` |
|      23 | 10556 | `		pBlock = pBlock->pParent;` |
|       1 | 10557 | `	}` |
|      38 | 10558 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 | 10559 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 | 10560 | `	return SXRET_OK;` |
|      20 | 10561 | `}` |
|       - | 10562 | `/*` |
|       - | 10563 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|       - | 10564 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|       - | 10565 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|       - | 10566 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|       - | 10567 | ` * compile error propagated from the parser.` |
|       - | 10568 | ` */` |
|      46 | 10569 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|       4 | 10570 | `{` |
|       - | 10571 | `	SyString sClassName;` |
|       - | 10572 | `	SyToken *pToken;` |
|       - | 10573 | `	SyString *pName;` |
|       - | 10574 | `	char *zDup;` |
|       - | 10575 | `	sxi32 rc;` |
|      50 | 10576 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|      50 | 10577 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|      50 | 10578 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|      50 | 10579 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      50 | 10580 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 10581 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10582 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10583 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10584 | `		return SXERR_INVALID;` |
|       - | 10585 | `	}` |
|      50 | 10586 | `	pGen->pIn++; /* '(' */` |
|      23 | 10587 | `	for(;;){` |
|       - | 10588 | `		SyBlob sResolved;` |
|      50 | 10589 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      50 | 10590 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 10591 | `			SyBlobRelease(&sResolved);` |
|     ! 0 | 10592 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10593 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10594 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10595 | `			return SXERR_INVALID;` |
|       - | 10596 | `		}` |
|      73 | 10597 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      46 | 10598 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|      50 | 10599 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|      50 | 10600 | `		SyBlobRelease(&sResolved);` |
|      50 | 10601 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|      50 | 10602 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|      50 | 10603 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      46 | 10604 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|       4 | 10605 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     ! 0 | 10606 | `			pGen->pIn++; continue;` |
|       - | 10607 | `		}` |
|      50 | 10608 | `		break;` |
|     ! 0 | 10609 | `	}` |
|      46 | 10610 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|      50 | 10611 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 10612 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10613 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10614 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10615 | `		return SXERR_INVALID;` |
|       - | 10616 | `	}` |
|      50 | 10617 | `	pGen->pIn++; /* '$' */` |
|      50 | 10618 | `	pName = &pGen->pIn->sData;` |
|      50 | 10619 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      50 | 10620 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|      50 | 10621 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|      50 | 10622 | `	pGen->pIn++;` |
|      50 | 10623 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 10624 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10625 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10626 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10627 | `		return SXERR_INVALID;` |
|       - | 10628 | `	}` |
|      50 | 10629 | `	pGen->pIn++; /* ')' */` |
|      50 | 10630 | `	return SXRET_OK;` |
|      27 | 10631 | `}` |
|       - | 10632 | `/*` |
|       - | 10633 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|       - | 10634 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|       - | 10635 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|       - | 10636 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|       - | 10637 | ` * VmThrowException):` |
|       - | 10638 | ` *` |
|       - | 10639 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|       - | 10640 | ` *    <try body>` |
|       - | 10641 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|       - | 10642 | ` *    JMP  -> finally\|end` |
|       - | 10643 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|       - | 10644 | ` *    <catch body>` |
|       - | 10645 | ` *    JMP  -> finally\|end` |
|       - | 10646 | ` *    ... more catches ...` |
|       - | 10647 | ` *  Lfin: <finally body>` |
|       - | 10648 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|       - | 10649 | ` *  Lend:` |
|       - | 10650 | ` */` |
|      90 | 10651 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|       4 | 10652 | `{` |
|      94 | 10653 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10654 | `	GenBlock *pTry;` |
|       - | 10655 | `	VmInstr *pInstr;` |
|      94 | 10656 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|       - | 10657 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|       - | 10658 | `	sxi32 rc;` |
|      94 | 10659 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|       - | 10660 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      94 | 10661 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      94 | 10662 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      94 | 10663 | `	pTry->pUserData = pException;` |
|      94 | 10664 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      94 | 10665 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      94 | 10666 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      94 | 10667 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      94 | 10668 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      94 | 10669 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10670 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      94 | 10671 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      94 | 10672 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      94 | 10673 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      94 | 10674 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10675 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      94 | 10676 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|       - | 10677 | `	/* Catch clauses (inline) */` |
|      94 | 10678 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      90 | 10679 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|      50 | 10680 | `		sxu32 k = 0;` |
|      69 | 10681 | `		for(;;){` |
|       - | 10682 | `			ph7_exception_block sCatch;` |
|       - | 10683 | `			GenBlock *pCatchBlk;` |
|      96 | 10684 | `			sxu32 idxJmp = 0;` |
|      92 | 10685 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      88 | 10686 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      27 | 10687 | `				break;` |
|       - | 10688 | `			}` |
|      50 | 10689 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|      50 | 10690 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      50 | 10691 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|      50 | 10692 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|      50 | 10693 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|      50 | 10694 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|      50 | 10695 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       - | 10696 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|       - | 10697 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|       - | 10698 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|      50 | 10699 | `			pCatchBlk->pUserData = pException;` |
|      50 | 10700 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      50 | 10701 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      50 | 10702 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      50 | 10703 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10704 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|       - | 10705 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|      50 | 10706 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      50 | 10707 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|      50 | 10708 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|      50 | 10709 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      50 | 10710 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      50 | 10711 | `			k++;` |
|       4 | 10712 | `		}` |
|      23 | 10713 | `	}` |
|       - | 10714 | `	/* Finally (inline) */` |
|      94 | 10715 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      74 | 10716 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10717 | `		GenBlock *pFinBlk;` |
|      52 | 10718 | `		pGen->pIn++; /* Jump 'finally' */` |
|      52 | 10719 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|      52 | 10720 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|      52 | 10721 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      52 | 10722 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      52 | 10723 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      52 | 10724 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      52 | 10725 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      52 | 10726 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|      52 | 10727 | `		pException->iHasFinally = 1;` |
|      24 | 10728 | `	}` |
|      94 | 10729 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      94 | 10730 | `	pException->iInlined = 1;` |
|       - | 10731 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|       - | 10732 | `	{` |
|      94 | 10733 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|       - | 10734 | `		sxu32 *aJ; sxu32 n;` |
|      94 | 10735 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      94 | 10736 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      94 | 10737 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|     140 | 10738 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|      50 | 10739 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|      50 | 10740 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      27 | 10741 | `		}` |
|       - | 10742 | `	}` |
|      94 | 10743 | `	SySetRelease(&aCatchJmp);` |
|      94 | 10744 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|     ! 0 | 10745 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|     ! 0 | 10746 | `	}` |
|      94 | 10747 | `	return SXRET_OK;` |
|      49 | 10748 | `}` |
|       - | 10749 | `/*` |
|       - | 10750 | ` * Compile a 'catch' block.` |
|       - | 10751 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 10752 | ` * an object containing the exception information.` |
|       - | 10753 | ` */` |
|     970 | 10754 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 10755 | `{` |
|     975 | 10756 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10757 | `	ph7_exception_block sCatch;` |
|       - | 10758 | `	SySet *pInstrContainer;` |
|       - | 10759 | `	SyString sClassName;` |
|       - | 10760 | `	GenBlock *pCatch;` |
|       - | 10761 | `	SyToken *pToken;` |
|       - | 10762 | `	SyString *pName;` |
|       - | 10763 | `	char *zDup;` |
|       - | 10764 | `	sxi32 rc;` |
|     975 | 10765 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 10766 | `	/* Zero the structure */` |
|     975 | 10767 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 10768 | `	/* Initialize fields */` |
|     975 | 10769 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     975 | 10770 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     975 | 10771 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 10772 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10773 | `			pToken = pGen->pIn;` |
|     ! 0 | 10774 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10775 | `				pToken--;` |
|     ! 0 | 10776 | `			}` |
|     ! 0 | 10777 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10778 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10779 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10780 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10781 | `				return SXERR_ABORT;` |
|       - | 10782 | `			}` |
|     ! 0 | 10783 | `			return SXERR_INVALID;` |
|       - | 10784 | `	}` |
|       - | 10785 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     975 | 10786 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     499 | 10787 | `	for(;;){` |
|       - | 10788 | `		SyBlob sResolved;` |
|    1003 | 10789 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    1003 | 10790 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 10791 | `			SyBlobRelease(&sResolved);` |
|       6 | 10792 | `			pToken = pGen->pIn;` |
|       6 | 10793 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10794 | `				pToken--;` |
|     ! 0 | 10795 | `			}` |
|       8 | 10796 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10797 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 10798 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 10799 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10800 | `				return SXERR_ABORT;` |
|       - | 10801 | `			}` |
|       6 | 10802 | `			return SXERR_INVALID;` |
|       - | 10803 | `		}` |
|       - | 10804 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 10805 | `		 * transient SyBlob allocation. */` |
|    1496 | 10806 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     994 | 10807 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     999 | 10808 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     999 | 10809 | `		SyBlobRelease(&sResolved);` |
|     999 | 10810 | `		if( zDup == 0 ){` |
|     ! 0 | 10811 | `			goto Mem;` |
|       - | 10812 | `		}` |
|     999 | 10813 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     999 | 10814 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10815 | `			goto Mem;` |
|       - | 10816 | `		}` |
|       - | 10817 | `		/* Check for '\|' (multi-catch separator) */` |
|     994 | 10818 | `		if( pGen->pIn < pGen->pEnd &&` |
|     994 | 10819 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      33 | 10820 | `			pGen->pIn->sData.nByte == 1 &&` |
|      28 | 10821 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      30 | 10822 | `			pGen->pIn++; /* Consume the '\|' */` |
|      30 | 10823 | `			continue;` |
|       - | 10824 | `		}` |
|     971 | 10825 | `		break;` |
|     ! 0 | 10826 | `	}` |
|     966 | 10827 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     971 | 10828 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 10829 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10830 | `			pToken = pGen->pIn;` |
|     ! 0 | 10831 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10832 | `				pToken--;` |
|     ! 0 | 10833 | `			}` |
|     ! 0 | 10834 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10835 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10836 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10837 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10838 | `				return SXERR_ABORT;` |
|       - | 10839 | `			}` |
|     ! 0 | 10840 | `			return SXERR_INVALID;` |
|       - | 10841 | `	}` |
|     971 | 10842 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 10843 | `	/* Duplicate instance name */` |
|     971 | 10844 | `	pName = &pGen->pIn->sData;` |
|     971 | 10845 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     971 | 10846 | `	if( zDup == 0 ){` |
|     ! 0 | 10847 | `		goto Mem;` |
|       - | 10848 | `	}` |
|     971 | 10849 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     971 | 10850 | `	pGen->pIn++;` |
|     971 | 10851 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 10852 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 10853 | `		pToken = pGen->pIn;` |
|     ! 0 | 10854 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10855 | `			pToken--;` |
|     ! 0 | 10856 | `		}` |
|     ! 0 | 10857 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10858 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10859 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10860 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10861 | `			return SXERR_ABORT;` |
|       - | 10862 | `		}` |
|     ! 0 | 10863 | `		return SXERR_INVALID;` |
|       - | 10864 | `	}` |
|       - | 10865 | `	/* Compile the block */` |
|     971 | 10866 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 10867 | `	/* Create the catch block */` |
|     971 | 10868 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     971 | 10869 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10870 | `		return SXERR_ABORT;` |
|       - | 10871 | `	}` |
|       - | 10872 | `	/* Swap bytecode container */` |
|     971 | 10873 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     971 | 10874 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 10875 | `	/* Compile the block */` |
|     971 | 10876 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 10877 | `	/* Fix forward jumps now the destination is resolved  */` |
|     971 | 10878 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10879 | `	/* Emit the DONE instruction */` |
|     971 | 10880 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10881 | `	/* Leave the block */` |
|     971 | 10882 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10883 | `	/* Restore the default container */` |
|     971 | 10884 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10885 | `	/* Install the catch block */` |
|     971 | 10886 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     971 | 10887 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10888 | `		goto Mem;` |
|       - | 10889 | `	}` |
|     971 | 10890 | `	return SXRET_OK;` |
|     ! 0 | 10891 | `Mem:` |
|     ! 0 | 10892 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10893 | `	return SXERR_ABORT;` |
|     490 | 10894 | `}` |
|       - | 10895 | `/*` |
|       - | 10896 | ` * Compile a 'try' block.` |
|       - | 10897 | ` * A function using an exception should be in a "try" block.` |
|       - | 10898 | ` * If the exception does not trigger, the code will continue` |
|       - | 10899 | ` * as normal. However if the exception triggers, an exception` |
|       - | 10900 | ` * is "thrown".` |
|       - | 10901 | ` */` |
|    1118 | 10902 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 10903 | `{` |
|       - | 10904 | `	ph7_exception *pException;` |
|    1123 | 10905 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10906 | `	GenBlock *pTry;` |
|       - | 10907 | `	sxu32 nJmpIdx;` |
|       - | 10908 | `	sxi32 rc;` |
|       - | 10909 | `	/* Create the exception container */` |
|    1123 | 10910 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    1123 | 10911 | `	if( pException == 0 ){` |
|     ! 0 | 10912 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 10913 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10914 | `		return SXERR_ABORT;` |
|       - | 10915 | `	}` |
|       - | 10916 | `	/* Zero the structure */` |
|    1123 | 10917 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 10918 | `	/* Initialize fields */` |
|    1123 | 10919 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    1123 | 10920 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    1123 | 10921 | `	pException->iHasFinally = 0;` |
|    1123 | 10922 | `	pException->iFinallyDone = 0;` |
|    1123 | 10923 | `	pException->pVm = pGen->pVm;` |
|       - | 10924 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|       - | 10925 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|       - | 10926 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|       - | 10927 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|       - | 10928 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|       - | 10929 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|    1123 | 10930 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      94 | 10931 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|       - | 10932 | `	}` |
|       - | 10933 | `	/* Create the try block */` |
|    1033 | 10934 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|    1033 | 10935 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10936 | `		return SXERR_ABORT;` |
|       - | 10937 | `	}` |
|       - | 10938 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|    1033 | 10939 | `	pTry->pUserData = pException;` |
|       - | 10940 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    1033 | 10941 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 10942 | `	/* Fix the jump later when the destination is resolved */` |
|    1033 | 10943 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    1033 | 10944 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 10945 | `	/* Compile the block */` |
|    1033 | 10946 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    1033 | 10947 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10948 | `		return SXERR_ABORT;` |
|       - | 10949 | `	}` |
|       - | 10950 | `	/* Fix forward jumps now the destination is resolved */` |
|    1033 | 10951 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10952 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    1033 | 10953 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 10954 | `	/* Leave the block */` |
|    1033 | 10955 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10956 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    1033 | 10957 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    1026 | 10958 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 10959 | `		/* Compile one or more catch blocks */` |
|     966 | 10960 | `		for(;;){` |
|    1932 | 10961 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    1386 | 10962 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     486 | 10963 | `					break;` |
|       - | 10964 | `			}` |
|     975 | 10965 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     975 | 10966 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10967 | `				return SXERR_ABORT;` |
|       - | 10968 | `			}` |
|       5 | 10969 | `		}` |
|     481 | 10970 | `	}` |
|       - | 10971 | `	/* Compile optional finally block */` |
|    1033 | 10972 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     414 | 10973 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10974 | `		SySet *pInstrContainer;` |
|       - | 10975 | `		GenBlock *pFinBlock;` |
|     129 | 10976 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 10977 | `		/* Create the finally block for jump fixup bookkeeping */` |
|     129 | 10978 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     129 | 10979 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10980 | `			return SXERR_ABORT;` |
|       - | 10981 | `		}` |
|       - | 10982 | `		/* Swap bytecode container */` |
|     129 | 10983 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     129 | 10984 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 10985 | `		/* Compile the finally body */` |
|     129 | 10986 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     129 | 10987 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10988 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 10989 | `			return SXERR_ABORT;` |
|       - | 10990 | `		}` |
|       - | 10991 | `		/* Fix forward jumps now the destination is resolved */` |
|     129 | 10992 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10993 | `		/* Emit DONE to terminate the finally block */` |
|     129 | 10994 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10995 | `		/* Leave the block */` |
|     129 | 10996 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10997 | `		/* Restore the default container */` |
|     129 | 10998 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     129 | 10999 | `		pException->iHasFinally = 1;` |
|      62 | 11000 | `	}` |
|       - | 11001 | `	/* Must have at least one catch or finally */` |
|    1033 | 11002 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       8 | 11003 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 11004 | `			"Cannot use try without catch or finally");` |
|       8 | 11005 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11006 | `			return SXERR_ABORT;` |
|       - | 11007 | `		}` |
|       3 | 11008 | `	}` |
|    1033 | 11009 | `	return SXRET_OK;` |
|     564 | 11010 | `}` |
|       - | 11011 | `/*` |
|       - | 11012 | ` * Compile a switch block.` |
|       - | 11013 | ` *  (See block-comment below for more information)` |
|       - | 11014 | ` */` |
|     112 | 11015 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 11016 | `{` |
|     117 | 11017 | `	sxi32 rc = SXRET_OK;` |
|     117 | 11018 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 11019 | `		/* Unexpected token */` |
|     ! 0 | 11020 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 11021 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11022 | `			return SXERR_ABORT;` |
|       - | 11023 | `		}` |
|     ! 0 | 11024 | `		pGen->pIn++;` |
|     ! 0 | 11025 | `	}` |
|     117 | 11026 | `	pGen->pIn++;` |
|       - | 11027 | `	/* First instruction to execute in this block. */` |
|     117 | 11028 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 11029 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 11030 | `	 * or the '}' token */` |
|     206 | 11031 | `	for(;;){` |
|     417 | 11032 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11033 | `			/* No more input to process */` |
|     ! 0 | 11034 | `			break;` |
|       - | 11035 | `		}` |
|     417 | 11036 | `		rc = SXRET_OK;` |
|     417 | 11037 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 | 11038 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 | 11039 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 11040 | `					/* Unexpected token */` |
|     ! 0 | 11041 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 11042 | `						&pGen->pIn->sData);` |
|     ! 0 | 11043 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11044 | `						return SXERR_ABORT;` |
|       - | 11045 | `					}` |
|       - | 11046 | `					/* FALL THROUGH */` |
|     ! 0 | 11047 | `				}` |
|      31 | 11048 | `				rc = SXERR_EOF;` |
|      31 | 11049 | `				break;` |
|       - | 11050 | `			}` |
|      32 | 11051 | `		}else{` |
|       - | 11052 | `			sxi32 nKwrd;` |
|       - | 11053 | `			/* Extract the keyword */` |
|     337 | 11054 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 | 11055 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 | 11056 | `				break;` |
|       - | 11057 | `			}` |
|     253 | 11058 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 11059 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 11060 | `					/* Unexpected token */` |
|     ! 0 | 11061 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 11062 | `						&pGen->pIn->sData);` |
|     ! 0 | 11063 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11064 | `						return SXERR_ABORT;` |
|       - | 11065 | `					}` |
|       - | 11066 | `					/* FALL THROUGH */` |
|     ! 0 | 11067 | `				}` |
|       - | 11068 | `				/* Block compiled */` |
|       3 | 11069 | `				break;` |
|       - | 11070 | `			}` |
|       - | 11071 | `		}` |
|       - | 11072 | `		/* Compile block */` |
|     305 | 11073 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 | 11074 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11075 | `			return SXERR_ABORT;` |
|       - | 11076 | `		}` |
|       5 | 11077 | `	}` |
|     117 | 11078 | `	return rc;` |
|      61 | 11079 | `}` |
|       - | 11080 | `/*` |
|       - | 11081 | ` * Compile a case eXpression.` |
|       - | 11082 | ` *  (See block-comment below for more information)` |
|       - | 11083 | ` */` |
|      92 | 11084 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 11085 | `{` |
|       - | 11086 | `	SySet *pInstrContainer;` |
|       - | 11087 | `	SyToken *pEnd,*pTmp;` |
|      97 | 11088 | `	sxi32 iNest = 0;` |
|       - | 11089 | `	sxi32 rc;` |
|       - | 11090 | `	/* Delimit the expression */` |
|      97 | 11091 | `	pEnd = pGen->pIn;` |
|     197 | 11092 | `	while( pEnd < pGen->pEnd ){` |
|     197 | 11093 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 11094 | `			/* Increment nesting level */` |
|       3 | 11095 | `			iNest++;` |
|     196 | 11096 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 11097 | `			/* Decrement nesting level */` |
|       3 | 11098 | `			iNest--;` |
|     194 | 11099 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 | 11100 | `			break;` |
|       - | 11101 | `		}` |
|     105 | 11102 | `		pEnd++;` |
|       5 | 11103 | `	}` |
|      97 | 11104 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 11105 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 11106 | `		if( rc == SXERR_ABORT ){` |
|       - | 11107 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 11108 | `			return SXERR_ABORT;` |
|       - | 11109 | `		}` |
|     ! 0 | 11110 | `	}` |
|       - | 11111 | `	/* Swap token stream */` |
|      97 | 11112 | `	pTmp = pGen->pEnd;` |
|      97 | 11113 | `	pGen->pEnd = pEnd;` |
|      97 | 11114 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 | 11115 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 | 11116 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 11117 | `	/* Emit the done instruction */` |
|      97 | 11118 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 | 11119 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 11120 | `	/* Update token stream */` |
|      97 | 11121 | `	pGen->pIn  = pEnd;` |
|      97 | 11122 | `	pGen->pEnd = pTmp;` |
|      97 | 11123 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 11124 | `		return SXERR_ABORT;` |
|       - | 11125 | `	}` |
|      97 | 11126 | `	return SXRET_OK;` |
|      51 | 11127 | `}` |
|       - | 11128 | `/*` |
|       - | 11129 | ` * Compile the smart switch statement.` |
|       - | 11130 | ` * According to the PHP language reference manual` |
|       - | 11131 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 11132 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 11133 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 11134 | ` *  This is exactly what the switch statement is for.` |
|       - | 11135 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 11136 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 11137 | ` *  of the outer loop, use continue 2.` |
|       - | 11138 | ` *  Note that switch/case does loose comparision.` |
|       - | 11139 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 11140 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 11141 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 11142 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 11143 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 11144 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 11145 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 11146 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 11147 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 11148 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 11149 | ` *  list for the next case.` |
|       - | 11150 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 11151 | ` *  or floating-point numbers and strings.` |
|       - | 11152 | ` */` |
|      28 | 11153 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 11154 | `{` |
|       - | 11155 | `	GenBlock *pSwitchBlock;` |
|       - | 11156 | `	SyToken *pTmp,*pEnd;` |
|       - | 11157 | `	ph7_switch *pSwitch;` |
|       - | 11158 | `	sxu32 nToken;` |
|       - | 11159 | `	sxu32 nLine;` |
|       - | 11160 | `	sxi32 rc;` |
|      33 | 11161 | `	nLine = pGen->pIn->nLine;` |
|       - | 11162 | `	/* Jump the 'switch' keyword */` |
|      33 | 11163 | `	pGen->pIn++;` |
|      33 | 11164 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 11165 | `		/* Syntax error */` |
|     ! 0 | 11166 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 11167 | `		if( rc == SXERR_ABORT ){` |
|       - | 11168 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 11169 | `			return SXERR_ABORT;` |
|       - | 11170 | `		}` |
|     ! 0 | 11171 | `		goto Synchronize;` |
|       - | 11172 | `	}` |
|       - | 11173 | `	/* Jump the left parenthesis '(' */` |
|      33 | 11174 | `	pGen->pIn++;` |
|      33 | 11175 | `	pEnd = 0; /* cc warning */` |
|       - | 11176 | `	/* Create the loop block */` |
|      47 | 11177 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 11178 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 | 11179 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 11180 | `		return SXERR_ABORT;` |
|       - | 11181 | `	}` |
|       - | 11182 | `	/* Delimit the condition */` |
|      33 | 11183 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 | 11184 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 11185 | `		/* Empty expression */` |
|     ! 0 | 11186 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 11187 | `		if( rc == SXERR_ABORT ){` |
|       - | 11188 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 11189 | `			return SXERR_ABORT;` |
|       - | 11190 | `		}` |
|     ! 0 | 11191 | `	}` |
|       - | 11192 | `	/* Swap token streams */` |
|      33 | 11193 | `	pTmp = pGen->pEnd;` |
|      33 | 11194 | `	pGen->pEnd = pEnd;` |
|       - | 11195 | `	/* Compile the expression */` |
|      33 | 11196 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 | 11197 | `	if( rc == SXERR_ABORT ){` |
|       - | 11198 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 11199 | `		return SXERR_ABORT;` |
|       - | 11200 | `	}` |
|       - | 11201 | `	/* Update token stream */` |
|      33 | 11202 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 11203 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 11204 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 11205 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11206 | `			return SXERR_ABORT;` |
|       - | 11207 | `		}` |
|     ! 0 | 11208 | `		pGen->pIn++;` |
|     ! 0 | 11209 | `	}` |
|      33 | 11210 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 11211 | `	pGen->pEnd = pTmp;` |
|      33 | 11212 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 11213 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 11214 | `			pTmp = pGen->pIn;` |
|     ! 0 | 11215 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 11216 | `				pTmp--;` |
|     ! 0 | 11217 | `			}` |
|       - | 11218 | `			/* Unexpected token */` |
|     ! 0 | 11219 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 11220 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11221 | `				return SXERR_ABORT;` |
|       - | 11222 | `			}` |
|     ! 0 | 11223 | `			goto Synchronize;` |
|       - | 11224 | `	}` |
|       - | 11225 | `	/* Set the delimiter token */` |
|      33 | 11226 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 11227 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 11228 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 11229 | `	}else{` |
|      31 | 11230 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 11231 | `	}` |
|      33 | 11232 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 11233 | `	/* Create the switch blocks container */` |
|      33 | 11234 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 11235 | `	if( pSwitch == 0 ){` |
|       - | 11236 | `		/* Abort compilation */` |
|     ! 0 | 11237 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 11238 | `		return SXERR_ABORT;` |
|       - | 11239 | `	}` |
|       - | 11240 | `	/* Zero the structure */` |
|      33 | 11241 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 11242 | `	/* Initialize fields */` |
|      33 | 11243 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 11244 | `	/* Emit the switch instruction */` |
|      33 | 11245 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 11246 | `	/* Compile case blocks */` |
|     100 | 11247 | `	for(;;){` |
|       - | 11248 | `		sxu32 nKwrd;` |
|     119 | 11249 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11250 | `			/* No more input to process */` |
|     ! 0 | 11251 | `			break;` |
|       - | 11252 | `		}` |
|     119 | 11253 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 11254 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 11255 | `				/* Unexpected token */` |
|     ! 0 | 11256 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11257 | `					&pGen->pIn->sData);` |
|     ! 0 | 11258 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11259 | `					return SXERR_ABORT;` |
|       - | 11260 | `				}` |
|       - | 11261 | `				/* FALL THROUGH */` |
|     ! 0 | 11262 | `			}` |
|       - | 11263 | `			/* Block compiled */` |
|     ! 0 | 11264 | `			break;` |
|       - | 11265 | `		}` |
|       - | 11266 | `		/* Extract the keyword */` |
|     119 | 11267 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 11268 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 11269 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 11270 | `				/* Unexpected token */` |
|     ! 0 | 11271 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11272 | `					&pGen->pIn->sData);` |
|     ! 0 | 11273 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11274 | `					return SXERR_ABORT;` |
|       - | 11275 | `				}` |
|       - | 11276 | `				/* FALL THROUGH */` |
|     ! 0 | 11277 | `			}` |
|       - | 11278 | `			/* Block compiled */` |
|       3 | 11279 | `			break;` |
|       - | 11280 | `		}` |
|     117 | 11281 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 11282 | `			/*` |
|       - | 11283 | `			 * Accroding to the PHP language reference manual` |
|       - | 11284 | `			 *  A special case is the default case. This case matches anything` |
|       - | 11285 | `			 *  that wasn't matched by the other cases.` |
|       - | 11286 | `			 */` |
|      25 | 11287 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 11288 | `				/* Default case already compiled */` |
|     ! 0 | 11289 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 11290 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11291 | `					return SXERR_ABORT;` |
|       - | 11292 | `				}` |
|     ! 0 | 11293 | `			}` |
|      25 | 11294 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 11295 | `			/* Compile the default block */` |
|      25 | 11296 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 11297 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 11298 | `				return SXERR_ABORT;` |
|      25 | 11299 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 11300 | `				break;` |
|       1 | 11301 | `			}` |
|      98 | 11302 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 11303 | `			ph7_case_expr sCase;` |
|       - | 11304 | `			/* Standard case block */` |
|      97 | 11305 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 11306 | `			/* initialize the structure */` |
|      97 | 11307 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 11308 | `			/* Compile the case expression */` |
|      97 | 11309 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 11310 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11311 | `				return SXERR_ABORT;` |
|       - | 11312 | `			}` |
|       - | 11313 | `			/* Compile the case block */` |
|      97 | 11314 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 11315 | `			/* Insert in the switch container */` |
|      97 | 11316 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 11317 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 11318 | `				return SXERR_ABORT;` |
|      97 | 11319 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 11320 | `				break;` |
|       - | 11321 | `			}` |
|      47 | 11322 | `		}else{` |
|       - | 11323 | `			/* Unexpected token */` |
|     ! 0 | 11324 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11325 | `				&pGen->pIn->sData);` |
|     ! 0 | 11326 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11327 | `				return SXERR_ABORT;` |
|       - | 11328 | `			}` |
|     ! 0 | 11329 | `			break;` |
|       - | 11330 | `		}` |
|       5 | 11331 | `	}` |
|       - | 11332 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 11333 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 11334 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11335 | `	/* Release the loop block */` |
|      33 | 11336 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 11337 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 11338 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 11339 | `		pGen->pIn++;` |
|      14 | 11340 | `	}` |
|       - | 11341 | `	/* Statement successfully compiled */` |
|      33 | 11342 | `	return SXRET_OK;` |
|     ! 0 | 11343 | `Synchronize:` |
|       - | 11344 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 11345 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 11346 | `		pGen->pIn++;` |
|     ! 0 | 11347 | `	}` |
|     ! 0 | 11348 | `	return SXRET_OK;` |
|      19 | 11349 | `}` |
|       - | 11350 | `/*` |
|       - | 11351 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 11352 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 11353 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 11354 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 11355 | ` */` |
|       - | 11356 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 11357 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 11358 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 11359 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 11360 |  |
|       - | 11361 | `/*` |
|       - | 11362 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 11363 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 11364 | ` * patched entries from the pending set.` |
|       - | 11365 | ` */` |
| 2840810 | 11366 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 11367 | `{` |
| 2840815 | 11368 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 11369 | `	sxu32 nTarget;` |
|       - | 11370 | `	sxu32 *aIdx;` |
|       - | 11371 | `	sxu32 i;` |
| 2840815 | 11372 | `	if( nCur <= nBaseline ){` |
| 2840721 | 11373 | `		return;` |
|       - | 11374 | `	}` |
|      98 | 11375 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      98 | 11376 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     200 | 11377 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     106 | 11378 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     106 | 11379 | `		if( pInstr ){` |
|     106 | 11380 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      51 | 11381 | `		}` |
|      55 | 11382 | `	}` |
|      98 | 11383 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1420410 | 11384 | `}` |
|       - | 11385 |  |
|       - | 11386 | `/*` |
|       - | 11387 | ` * By-reference out-parameters of builtin functions.` |
|       - | 11388 | ` *` |
|       - | 11389 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 11390 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 11391 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 11392 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 11393 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 11394 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 11395 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 11396 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 11397 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 11398 | ` * creates it" behaviour).` |
|       - | 11399 | ` *` |
|       - | 11400 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 11401 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 11402 | ` */` |
|  478328 | 11403 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 11404 | `{` |
|       - | 11405 | `	static const struct {` |
|       - | 11406 | `		const char *zName;` |
|       - | 11407 | `		sxu32 nByte;` |
|       - | 11408 | `		sxu32 mask;` |
|       - | 11409 | `	} aByRef[] = {` |
|       - | 11410 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 11411 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 11412 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 11413 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 11414 | `	};` |
|       - | 11415 | `	sxu32 i;` |
|  478333 | 11416 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    2283 | 11417 | `		return 0;` |
|       - | 11418 | `	}` |
| 2379983 | 11419 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1904022 | 11420 | `		if( pName->nByte == aByRef[i].nByte` |
|  976074 | 11421 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      99 | 11422 | `			return aByRef[i].mask;` |
|       - | 11423 | `		}` |
|  951969 | 11424 | `	}` |
|  475961 | 11425 | `	return 0;` |
|  239169 | 11426 | `}` |
|       - | 11427 | `/*` |
|       - | 11428 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 11429 | ` *` |
|       - | 11430 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 11431 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 11432 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 11433 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 11434 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 11435 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 11436 | ` */` |
|  478328 | 11437 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 11438 | `{` |
|       - | 11439 | `	SyToken *p, *pEnd;` |
|  478333 | 11440 | `	pOut->zString = 0;` |
|  478333 | 11441 | `	pOut->nByte = 0;` |
|  478333 | 11442 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 11443 | `		return;` |
|       - | 11444 | `	}` |
|  478333 | 11445 | `	p = pLeft->pStart;` |
|  478333 | 11446 | `	pEnd = pLeft->pEnd;` |
|       - | 11447 | `	/* Optional single leading namespace separator (absolute path). */` |
|  478333 | 11448 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|    3863 | 11449 | `		p++;` |
|    1929 | 11450 | `	}` |
|  478333 | 11451 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    2247 | 11452 | `		return;` |
|       - | 11453 | `	}` |
|       - | 11454 | `	/* Must be a single component: nothing follows the name token. */` |
|  476091 | 11455 | `	if( p + 1 != pEnd ){` |
|      41 | 11456 | `		return;` |
|       - | 11457 | `	}` |
|  476055 | 11458 | `	*pOut = p->sData;` |
|  239169 | 11459 | `}` |
|       - | 11460 | `/*` |
|       - | 11461 | ` * Generate bytecode for a given expression tree.` |
|       - | 11462 | ` * If something goes wrong while generating bytecode` |
|       - | 11463 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 11464 | ` * this function takes care of generating the appropriate` |
|       - | 11465 | ` * error message.` |
|       - | 11466 | ` */` |
| 3804340 | 11467 | `static sxi32 GenStateEmitExprCode(` |
|       - | 11468 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11469 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 11470 | `	sxi32 iFlags /* Control flags */` |
|       - | 11471 | `	)` |
|       5 | 11472 | `{` |
|       - | 11473 | `	VmInstr *pInstr;` |
|       - | 11474 | `	sxu32 nJmpIdx;` |
| 3804345 | 11475 | `	sxi32 iP1 = 0;` |
| 3804345 | 11476 | `	sxu32 iP2 = 0;` |
| 3804345 | 11477 | `	void *p3  = 0;` |
|       - | 11478 | `	sxi32 iVmOp;` |
|       - | 11479 | `	sxi32 rc;` |
| 3804345 | 11480 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3804345 | 11481 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 3804345 | 11482 | `	sxu32 nRhsNsBase = 0;` |
| 3804345 | 11483 | `	if( pNode->xCode ){` |
|       - | 11484 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 11485 | `		/* Compile node */` |
| 2374443 | 11486 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2374443 | 11487 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2374443 | 11488 | `		RE_SWAP_DELIMITER(pGen);` |
| 2374443 | 11489 | `		return rc;` |
|       - | 11490 | `	}` |
| 1429907 | 11491 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 11492 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 11493 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 11494 | `		return SXERR_ABORT;` |
|       - | 11495 | `	}` |
| 1429907 | 11496 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1429907 | 11497 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|       - | 11498 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|       - | 11499 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|       - | 11500 | `		 * and later errors are still reported. */` |
|       3 | 11501 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 11502 | `			"The (unset) cast is no longer supported");` |
|       3 | 11503 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11504 | `			return SXERR_ABORT;` |
|       - | 11505 | `		}` |
|       1 | 11506 | `	}` |
| 1429907 | 11507 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      65 | 11508 | `		sxu32 nJmp = 0;` |
|       - | 11509 | `		sxu32 nNcNsBase;` |
|       - | 11510 | `		VmInstr *pInstrFix;` |
|       - | 11511 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 11512 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 11513 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 11514 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 11515 | `		 * stack slot carries a writable nIdx. */` |
|      65 | 11516 | `		if( pNode->pRight ){` |
|      65 | 11517 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 11518 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|      65 | 11519 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11520 | `				return rc;` |
|       - | 11521 | `			}` |
|      65 | 11522 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 11523 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 11524 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 11525 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 11526 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 11527 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 11528 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 11529 | `			 * cascade for the actual write path stays correct. */` |
|      65 | 11530 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 11531 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      31 | 11532 | `				pInstrFix->iP2 = 3;` |
|      14 | 11533 | `			}` |
|      31 | 11534 | `		}` |
|       - | 11535 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      65 | 11536 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 11537 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      65 | 11538 | `		if( pNode->pLeft ){` |
|      65 | 11539 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 11540 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      65 | 11541 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11542 | `				return rc;` |
|       - | 11543 | `			}` |
|      65 | 11544 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      31 | 11545 | `		}` |
|       - | 11546 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      65 | 11547 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 11548 | `		/* Patch the short-circuit jump to land after the store. */` |
|      65 | 11549 | `		if( nJmp > 0 ){` |
|      65 | 11550 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      65 | 11551 | `			if( pInstrFix ){` |
|      65 | 11552 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      31 | 11553 | `			}` |
|      31 | 11554 | `		}` |
|      65 | 11555 | `		return SXRET_OK;` |
|       - | 11556 | `	}` |
| 1429845 | 11557 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 11558 | `		sxu32 nJz,nJmp;` |
|       - | 11559 | `		sxu32 nTernaryNsBase;` |
|       - | 11560 | `		/* Ternary operator require special handling */` |
|       - | 11561 | `		/* Phase#1: Compile the condition */` |
|    2651 | 11562 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2651 | 11563 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2651 | 11564 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11565 | `			return rc;` |
|       - | 11566 | `		}` |
|       - | 11567 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 11568 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 11569 | `		 * condition expression, not leak past the ternary. */` |
|    2651 | 11570 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2651 | 11571 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2651 | 11572 | `		if( pNode->pLeft ){` |
|       - | 11573 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 11574 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2583 | 11575 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 11576 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2583 | 11577 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2583 | 11578 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2583 | 11579 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11580 | `				return rc;` |
|       - | 11581 | `			}` |
|    2583 | 11582 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1294 | 11583 | `		}else{` |
|       - | 11584 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 11585 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 11586 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 11587 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 11588 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 11589 | `		}` |
|       - | 11590 | `		/* Phase#4: Emit the unconditional jump */` |
|    2651 | 11591 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 11592 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2651 | 11593 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2651 | 11594 | `		if( pInstr ){` |
|    2651 | 11595 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1323 | 11596 | `		}` |
|    2651 | 11597 | `		if( !pNode->pLeft ){` |
|       - | 11598 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 11599 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 11600 | `		}` |
|       - | 11601 | `		/* Phase#6: Compile the 'else' expression */` |
|    2651 | 11602 | `		if( pNode->pRight ){` |
|    2651 | 11603 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2651 | 11604 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2651 | 11605 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11606 | `				return rc;` |
|       - | 11607 | `			}` |
|    2651 | 11608 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1323 | 11609 | `		}` |
|    2651 | 11610 | `		if( nJmp > 0 ){` |
|       - | 11611 | `			/* Phase#7: Fix the unconditional jump */` |
|    2651 | 11612 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2651 | 11613 | `			if( pInstr ){` |
|    2651 | 11614 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1323 | 11615 | `			}` |
|    1323 | 11616 | `		}` |
|       - | 11617 | `		/* All done */` |
|    2651 | 11618 | `		return SXRET_OK;` |
|       - | 11619 | `	}` |
| 1427199 | 11620 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 11621 | `	/* Generate code for the left tree */` |
| 1427199 | 11622 | `	if( pNode->pLeft ){` |
| 1427159 | 11623 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1427159 | 11624 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 11625 | `			ph7_expr_node **apNode;` |
|  482329 | 11626 | `			int hasSpread = 0;` |
|  482329 | 11627 | `			int hasNamed = 0;` |
|  482329 | 11628 | `			int bAnySpread = 0;` |
|  482329 | 11629 | `			sxu32 byRefMask = 0;` |
|       - | 11630 | `			sxi32 nArgs;` |
|       - | 11631 | `			sxi32 n;` |
|       - | 11632 | `			/* Recurse and generate bytecodes for function arguments */` |
|  482329 | 11633 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  482329 | 11634 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 11635 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|       - | 11636 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|       - | 11637 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  482329 | 11638 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|      65 | 11639 | `				bFcc = 1;` |
|      65 | 11640 | `				nArgs = 0;` |
|      32 | 11641 | `			}` |
|       - | 11642 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 11643 | `			{` |
|  482329 | 11644 | `				int seenNamed = 0;` |
|  978235 | 11645 | `				for( n = 0; n < nArgs; ++n ){` |
|  495913 | 11646 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     253 | 11647 | `						seenNamed = 1;` |
|     253 | 11648 | `						hasNamed = 1;` |
|  495789 | 11649 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|    3861 | 11650 | `						bAnySpread = 1;` |
|  493737 | 11651 | `					}else if( seenNamed ){` |
|       3 | 11652 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 11653 | `							"Cannot use positional argument after named argument");` |
|       3 | 11654 | `						return SXERR_SYNTAX;` |
|       - | 11655 | `					}` |
|  247958 | 11656 | `				}` |
|       - | 11657 | `			}` |
|       - | 11658 | `			/* Read-only load */` |
|  482327 | 11659 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 11660 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 11661 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 11662 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 11663 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  482327 | 11664 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  482327 | 11665 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  482322 | 11666 | `				if( pCallName->nByte == 5` |
|  263307 | 11667 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   23317 | 11668 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  470671 | 11669 | `				}else if( pCallName->nByte == 5` |
|  239995 | 11670 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      99 | 11671 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      47 | 11672 | `				}` |
|       - | 11673 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 11674 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 11675 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 11676 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 11677 | `				 * the compile-time positional index no longer maps to the` |
|       - | 11678 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  482327 | 11679 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 11680 | `					SyString sBuiltin;` |
|  478333 | 11681 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  478333 | 11682 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  239164 | 11683 | `				}` |
|  241161 | 11684 | `			}` |
|  978231 | 11685 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  495909 | 11686 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  495909 | 11687 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11688 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 11689 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|       - | 11690 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|       - | 11691 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|       - | 11692 | `				 * builtin to write back through. A plain $var target is unaffected` |
|       - | 11693 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  495909 | 11694 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      55 | 11695 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      55 | 11696 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      25 | 11697 | `				}` |
|  495909 | 11698 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  495909 | 11699 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11700 | `					return rc;` |
|       - | 11701 | `				}` |
|       - | 11702 | `				/* Each argument is an independent nullsafe scope. */` |
|  495909 | 11703 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  495909 | 11704 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 11705 | `					/* Emit spread opcode to unpack this array argument */` |
|    3861 | 11706 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|    3861 | 11707 | `					hasSpread = 1;` |
|    1928 | 11708 | `				}` |
|  247957 | 11709 | `			}` |
|       - | 11710 | `			/* Total number of given arguments */` |
|  482327 | 11711 | `			iP1 = nArgs;` |
|  482327 | 11712 | `			iP2 = hasSpread;` |
|       - | 11713 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 11714 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  482327 | 11715 | `			if( hasNamed ){` |
|     142 | 11716 | `				sxu32 nStrBytes = 0;` |
|       - | 11717 | `				char *zBuf;` |
|     424 | 11718 | `				for( n = 0; n < nArgs; ++n ){` |
|     286 | 11719 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     250 | 11720 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|     123 | 11721 | `					}` |
|     145 | 11722 | `				}` |
|       - | 11723 | `				{` |
|     142 | 11724 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     142 | 11725 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|     138 | 11726 | `					&pGen->pVm->sAllocator, mapSize);` |
|     142 | 11727 | `				if( pMap ){` |
|     142 | 11728 | `					SyZero(pMap, mapSize);` |
|     142 | 11729 | `					pMap->bHasNamed = 1;` |
|     142 | 11730 | `					pMap->nTotal = (sxu32)nArgs;` |
|     142 | 11731 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     142 | 11732 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     424 | 11733 | `					for( n = 0; n < nArgs; ++n ){` |
|     286 | 11734 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     250 | 11735 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     250 | 11736 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     250 | 11737 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     250 | 11738 | `							zBuf += nb;` |
|     123 | 11739 | `						}` |
|       - | 11740 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     145 | 11741 | `					}` |
|     142 | 11742 | `					p3 = (void *)pMap;` |
|      69 | 11743 | `				}` |
|       - | 11744 | `				}` |
|      69 | 11745 | `			}` |
|       - | 11746 | `			/* Remove stale flags now */` |
|  482327 | 11747 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  241161 | 11748 | `		}` |
|       - | 11749 | `		{` |
|       - | 11750 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|       - | 11751 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|       - | 11752 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|       - | 11753 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|       - | 11754 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|       - | 11755 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|       - | 11756 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|       - | 11757 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 1427157 | 11758 | `			sxi32 iLeftFlags = iFlags;` |
| 1427152 | 11759 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
| 1089682 | 11760 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  376131 | 11761 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  367414 | 11762 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   17653 | 11763 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|    8824 | 11764 | `			}` |
|       - | 11765 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|       - | 11766 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|       - | 11767 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|       - | 11768 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|       - | 11769 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|       - | 11770 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|       - | 11771 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 1427152 | 11772 | `			if( pNode->pOp` |
| 2045226 | 11773 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 1331696 | 11774 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 1236189 | 11775 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  191389 | 11776 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   95692 | 11777 | `			}` |
| 1427157 | 11778 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|       - | 11779 | `		}` |
| 1427157 | 11780 | `		if( rc != SXRET_OK ){` |
|      34 | 11781 | `			return rc;` |
|       - | 11782 | `		}` |
| 1427127 | 11783 | `		if( !bIsChainOp ){` |
|       - | 11784 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 11785 | `			 * target the end of that LHS chain, which is right here. */` |
|  655007 | 11786 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  327501 | 11787 | `		}` |
| 1427127 | 11788 | `		if( iVmOp == PH7_OP_CALL ){` |
|  482327 | 11789 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  482327 | 11790 | `			if( pInstr ){` |
|  482327 | 11791 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  476207 | 11792 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 11793 | `					sxu32 nQual;` |
|  476207 | 11794 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11795 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 11796 | `					 * so the later NEW handler (if any) can see it. */` |
|  476207 | 11797 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 11798 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 11799 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 11800 | `					 * imports — class imports must NOT affect function` |
|       - | 11801 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 11802 | `					 * before NEW; we store the original literal index in the` |
|       - | 11803 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 11804 | `					 * the unqualified name and re-qualify with class imports. */` |
|  476207 | 11805 | `					if( bAbsolute ){` |
|    3863 | 11806 | `						pInstr->iP2 = (sxi32)nOrig;` |
|    1934 | 11807 | `					}else{` |
|  472349 | 11808 | `						int fromImport = 0;` |
|  472349 | 11809 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  472349 | 11810 | `						pInstr->iP2 = (sxi32)nQual;` |
|  472349 | 11811 | `						if( nQual != nOrig ){` |
|       - | 11812 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 11813 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 11814 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 11815 | `							if( !fromImport ){` |
|       - | 11816 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 11817 | `								if( p3 == 0 ){` |
|      67 | 11818 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 11819 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 11820 | `									if( pMap ){` |
|      67 | 11821 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 11822 | `										p3 = (void *)pMap;` |
|      31 | 11823 | `									}` |
|      31 | 11824 | `								}` |
|      67 | 11825 | `								if( p3 ){` |
|      67 | 11826 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 11827 | `								}` |
|      31 | 11828 | `							}` |
|      36 | 11829 | `						}` |
|       5 | 11830 | `					}` |
|  244226 | 11831 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 11832 | `					/* Method call,flag that */` |
|    1839 | 11833 | `					pInstr->iP2 = 1;` |
|     917 | 11834 | `				}` |
|  241166 | 11835 | `			}` |
| 1185966 | 11836 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 11837 | `			ph7_expr_node **apNode;` |
|       - | 11838 | `			sxi32 n;` |
|   98419 | 11839 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 11840 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 11841 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11842 | `			/* Recurse and generate bytecodes for array index */` |
|   98419 | 11843 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  177573 | 11844 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   79159 | 11845 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   79159 | 11846 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   79159 | 11847 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11848 | `					return rc;` |
|       - | 11849 | `				}` |
|       - | 11850 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   79159 | 11851 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   39582 | 11852 | `			}` |
|   98419 | 11853 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   79159 | 11854 | `				iP1 = 1; /* Node have an index associated with it */` |
|   39577 | 11855 | `			}` |
|   98419 | 11856 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 11857 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     245 | 11858 | `				iP2 = 4;` |
|   98299 | 11859 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 11860 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 11861 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      56 | 11862 | `				iP2 = 5;` |
|   98153 | 11863 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 11864 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 11865 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 11866 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 11867 | `				iP2 = 6;` |
|   98115 | 11868 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 11869 | `				/* Create an empty entry when the desired index is not found */` |
|   38825 | 11870 | `				iP2 = 1;` |
|   19415 | 11871 | `			}` |
|  895598 | 11872 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 11873 | `			/* POP the left node */` |
|      32 | 11874 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 11875 | `		}` |
|  713561 | 11876 | `	}` |
| 1427167 | 11877 | `	rc = SXRET_OK;` |
| 1427167 | 11878 | `	nJmpIdx = 0;` |
|       - | 11879 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 11880 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 11881 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1427167 | 11882 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     413 | 11883 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     413 | 11884 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     413 | 11885 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     413 | 11886 | `			int isSpecial = 0;` |
|     413 | 11887 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     317 | 11888 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     317 | 11889 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     312 | 11890 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     300 | 11891 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     144 | 11892 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|     111 | 11893 | `					isSpecial = 1;` |
|      53 | 11894 | `				}` |
|     180 | 11895 | `			}` |
|     461 | 11896 | `			pInstr->iP1 = 0;` |
|     461 | 11897 | `			if( !isSpecial ){` |
|     259 | 11898 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     127 | 11899 | `			}` |
|       - | 11900 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 11901 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     365 | 11902 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     259 | 11903 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     259 | 11904 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      46 | 11905 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      48 | 11906 | `					return SXRET_OK;` |
|       - | 11907 | `				}` |
|     105 | 11908 | `			}` |
|     158 | 11909 | `		}` |
|     239 | 11910 | `	}` |
|       - | 11911 | `	/* Generate code for the right tree */` |
| 1427085 | 11912 | `	if( pNode->pRight ){` |
|  769769 | 11913 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 11914 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   12003 | 11915 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  763770 | 11916 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 11917 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    4015 | 11918 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  755766 | 11919 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 11920 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 11921 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 11922 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  753750 | 11923 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 11924 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 11925 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 11926 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 11927 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 11928 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 11929 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     106 | 11930 | `			sxu32 nNsJmp = 0;` |
|     106 | 11931 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     106 | 11932 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  753586 | 11933 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|       - | 11934 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|       - | 11935 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|       - | 11936 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  320233 | 11937 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  160114 | 11938 | `		}` |
|  769769 | 11939 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  769769 | 11940 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  769769 | 11941 | `		if( !bIsChainOp ){` |
|       - | 11942 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 11943 | `			 * operator instruction is emitted. */` |
|  578429 | 11944 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  289212 | 11945 | `		}` |
|  769769 | 11946 | `		if( iVmOp == PH7_OP_STORE ){` |
|  316129 | 11947 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  316094 | 11948 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 11949 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 11950 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 11951 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 11952 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 11953 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 11954 | `				 */` |
|      85 | 11955 | `				iVmOp = 0;` |
|  316089 | 11956 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  316049 | 11957 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11958 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   84769 | 11959 | `					iP2 = 1;` |
|   42387 | 11960 | `				}else{` |
|  231285 | 11961 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11962 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   38747 | 11963 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   38747 | 11964 | `						iP1 = pInstr->iP1;` |
|   19376 | 11965 | `					}else{` |
|  192543 | 11966 | `						p3 = pInstr->p3;` |
|       - | 11967 | `					}` |
|       - | 11968 | `					/* POP the last dynamic load instruction */` |
|  231285 | 11969 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 11970 | `				}` |
|  158027 | 11971 | `			}` |
|  611707 | 11972 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      57 | 11973 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      57 | 11974 | `			if( pInstr ){` |
|      57 | 11975 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11976 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 11977 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 11978 | `					 */` |
|      17 | 11979 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      17 | 11980 | `					iP1 = pInstr->iP1;` |
|      17 | 11981 | `					iP2 = pInstr->iP2;` |
|      17 | 11982 | `					p3  = pInstr->p3;` |
|       9 | 11983 | `				}else{` |
|      41 | 11984 | `					p3 = pInstr->p3;` |
|       - | 11985 | `				}` |
|      27 | 11986 | `			}` |
|      27 | 11987 | `		}` |
|  384882 | 11988 | `	}` |
| 1427080 | 11989 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   12467 | 11990 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|       - | 11991 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|       - | 11992 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|      30 | 11993 | `		iVmOp = 0;` |
|      13 | 11994 | `	}` |
| 1427085 | 11995 | `	if( iVmOp > 0 ){` |
| 1426825 | 11996 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   15733 | 11997 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 11998 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|   11515 | 11999 | `				iP1 = 1;` |
|    5760 | 12000 | `			}` |
| 1418961 | 12001 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 12002 | `			/* Namespace-qualify the class name for NEW */ {` |
|   24685 | 12003 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   24685 | 12004 | `				VmInstr *pCallInstr = 0;` |
|   24685 | 12005 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   24493 | 12006 | `					pCallInstr = pPeek;` |
|   24493 | 12007 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   12244 | 12008 | `				}` |
|   24685 | 12009 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   24683 | 12010 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 12011 | `					sxu32 nLitForClass;` |
|       - | 12012 | `					/* If the CALL handler already qualified the name using` |
|       - | 12013 | `					 * function imports, recover the original unqualified` |
|       - | 12014 | `					 * literal so we can re-qualify with class imports. */` |
|   24683 | 12015 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 12016 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 12017 | `					}else{` |
|   24651 | 12018 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 12019 | `					}` |
|   24683 | 12020 | `					pPeek->iP1 = 0;` |
|   24683 | 12021 | `					if( !bAbsolute ){` |
|   20829 | 12022 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|   10417 | 12023 | `					}else{` |
|    3859 | 12024 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 12025 | `					}` |
|   12339 | 12026 | `				}` |
|       - | 12027 | `			}` |
|   24685 | 12028 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   24685 | 12029 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 12030 | `				VmInstr *pPrev;` |
|   24493 | 12031 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   24493 | 12032 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 12033 | `					/* Pop the call instruction, preserve named-arg map */` |
|   24493 | 12034 | `					iP1 = pInstr->iP1;` |
|   24493 | 12035 | `					if( pInstr->p3 ){` |
|      43 | 12036 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 12037 | `					}` |
|   24493 | 12038 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   12244 | 12039 | `				}` |
|   12249 | 12040 | `			}` |
| 1398757 | 12041 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 12042 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 12043 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     203 | 12044 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     203 | 12045 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     203 | 12046 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     203 | 12047 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     203 | 12048 | `				int isSpecialIs = 0;` |
|     203 | 12049 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     199 | 12050 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     199 | 12051 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     194 | 12052 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     199 | 12053 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      98 | 12054 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 12055 | `						isSpecialIs = 1;` |
|       5 | 12056 | `					}` |
|      98 | 12057 | `				}` |
|     205 | 12058 | `				pInstr->iP1 = 0;` |
|     205 | 12059 | `				if( !isSpecialIs && !bAbsolute ){` |
|     181 | 12060 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      88 | 12061 | `				}` |
|     103 | 12062 | `			}` |
| 1386321 | 12063 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 12064 | `			/* Prevent constant expansion for member/property names.` |
|       - | 12065 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 12066 | `			 * should not trigger constant lookup. */` |
|  191345 | 12067 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  191345 | 12068 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  191297 | 12069 | `				pInstr->iP1 = 0;` |
|   95646 | 12070 | `			}` |
|  191345 | 12071 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 12072 | `				/* Static member access,remember that */` |
|     331 | 12073 | `				iP1 = 1;` |
|     331 | 12074 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     331 | 12075 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      42 | 12076 | `					p3 = pInstr->p3;` |
|      42 | 12077 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      19 | 12078 | `				}` |
|     163 | 12079 | `			}` |
|       - | 12080 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|       - | 12081 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|       - | 12082 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|       - | 12083 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  191345 | 12084 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  191345 | 12085 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|      30 | 12086 | `					iP2 = PH7_MEMBER_UNSET;` |
|  191331 | 12087 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|      77 | 12088 | `					iP2 = PH7_MEMBER_ISSET;` |
|  191281 | 12089 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|      13 | 12090 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  191239 | 12091 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|       - | 12092 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   84849 | 12093 | `					iP2 = PH7_MEMBER_WRITE;` |
|   42422 | 12094 | `				}` |
|   95670 | 12095 | `			}` |
|   95670 | 12096 | `		}` |
|       - | 12097 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|       - | 12098 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|       - | 12099 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|       - | 12100 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|       - | 12101 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 1426823 | 12102 | `		if( bFcc ){` |
|      65 | 12103 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|      65 | 12104 | `			iP2 = 0;` |
|      65 | 12105 | `			p3 = 0;` |
|      65 | 12106 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 12107 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 12108 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|       - | 12109 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|       - | 12110 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|       - | 12111 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|      31 | 12112 | `				void *pMemberName = pInstr->p3;` |
|      31 | 12113 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|      31 | 12114 | `				if( pMemberName ){` |
|       3 | 12115 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       1 | 12116 | `				}` |
|      31 | 12117 | `				iP1 = 2;` |
|      16 | 12118 | `			}else{` |
|      35 | 12119 | `				iP1 = 1;` |
|       - | 12120 | `			}` |
|      32 | 12121 | `		}` |
|       - | 12122 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 12123 | `		 * This is the primary emit path for user-visible calls. */` |
| 1426823 | 12124 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  506943 | 12125 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  253469 | 12126 | `		}` |
|       - | 12127 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1426823 | 12128 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  713409 | 12129 | `	}` |
| 1427083 | 12130 | `	if( nJmpIdx > 0 ){` |
|       - | 12131 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   16137 | 12132 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   16137 | 12133 | `		if( pInstr ){` |
|   16137 | 12134 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    8066 | 12135 | `		}` |
|    8066 | 12136 | `	}` |
| 1427083 | 12137 | `	return rc;` |
| 1902155 | 12138 | `}` |
|       - | 12139 | `/*` |
|       - | 12140 | ` * Compile a PHP expression.` |
|       - | 12141 | ` * According to the PHP language reference manual:` |
|       - | 12142 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 12143 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 12144 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 12145 | ` *  is "anything that has a value".` |
|       - | 12146 | ` * If something goes wrong while compiling the expression,this` |
|       - | 12147 | ` * function takes care of generating the appropriate error` |
|       - | 12148 | ` * message.` |
|       - | 12149 | ` */` |
| 1024524 | 12150 | `static sxi32 PH7_CompileExpr(` |
|       - | 12151 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 12152 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 12153 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 12154 | `	)` |
|       5 | 12155 | `{` |
|       - | 12156 | `	ph7_expr_node *pRoot;` |
|       - | 12157 | `	SySet sExprNode;` |
|       - | 12158 | `	SyToken *pEnd;` |
|       - | 12159 | `	sxi32 nExpr;` |
|       - | 12160 | `	sxi32 iNest;` |
|       - | 12161 | `	sxi32 rc;` |
|       - | 12162 | `	sxu32 nNullsafeBase;` |
|       - | 12163 | `	/* Initialize worker variables */` |
| 1024529 | 12164 | `	nExpr = 0;` |
| 1024529 | 12165 | `	pRoot = 0;` |
|       - | 12166 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 12167 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
| 1024529 | 12168 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1024529 | 12169 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
| 1024529 | 12170 | `	SySetAlloc(&sExprNode,0x10);` |
| 1024529 | 12171 | `	rc = SXRET_OK;` |
|       - | 12172 | `	/* Delimit the expression */` |
| 1024529 | 12173 | `	pEnd = pGen->pIn;` |
| 1024529 | 12174 | `	iNest = 0;` |
| 6913939 | 12175 | `	while( pEnd < pGen->pEnd ){` |
| 6562035 | 12176 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 12177 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     557 | 12178 | `			iNest++;` |
| 6561759 | 12179 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     565 | 12180 | `			iNest--;` |
| 6561203 | 12181 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  673053 | 12182 | `			if( iNest <= 0 ){` |
|  672625 | 12183 | `				break;` |
|       - | 12184 | `			}` |
|     214 | 12185 | `		}` |
| 5889415 | 12186 | `		pEnd++;` |
|       5 | 12187 | `	}` |
| 1024529 | 12188 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   23575 | 12189 | `		SyToken *pEnd2 = pGen->pIn;` |
|   23575 | 12190 | `		iNest = 0;` |
|       - | 12191 | `		/* Stop at the first comma */` |
|   47463 | 12192 | `		while( pEnd2 < pEnd ){` |
|   23899 | 12193 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      71 | 12194 | `				iNest++;` |
|   23866 | 12195 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      71 | 12196 | `				iNest--;` |
|   23800 | 12197 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      57 | 12198 | `				if( iNest <= 0 ){` |
|       7 | 12199 | `					break;` |
|       - | 12200 | `				}` |
|      23 | 12201 | `			}` |
|   23893 | 12202 | `			pEnd2++;` |
|       5 | 12203 | `		}` |
|   23575 | 12204 | `		if( pEnd2 <pEnd ){` |
|       7 | 12205 | `			pEnd = pEnd2;` |
|       3 | 12206 | `		}` |
|   11785 | 12207 | `	}` |
| 1024529 | 12208 | `	if( pEnd > pGen->pIn ){` |
| 1024519 | 12209 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 12210 | `		/* Swap delimiter */` |
| 1024519 | 12211 | `		pGen->pEnd = pEnd;` |
|       - | 12212 | `		/* Try to get an expression tree */` |
| 1024519 | 12213 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
| 1024519 | 12214 | `		if( rc == SXRET_OK && pRoot ){` |
| 1024337 | 12215 | `			rc = SXRET_OK;` |
| 1024337 | 12216 | `			if( xTreeValidator ){` |
|       - | 12217 | `				/* Call the upper layer validator callback */` |
|   30927 | 12218 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   15461 | 12219 | `			}` |
| 1024337 | 12220 | `			if( rc != SXERR_ABORT ){` |
|       - | 12221 | `				/* Generate code for the given tree */` |
| 1024337 | 12222 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 12223 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 12224 | `				 * expression so they short-circuit to its end. */` |
| 1024337 | 12225 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  512166 | 12226 | `			}` |
| 1024337 | 12227 | `			nExpr = 1;` |
|  512166 | 12228 | `		}` |
|       - | 12229 | `		/* Release the whole tree */` |
| 1024519 | 12230 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 12231 | `		/* Synchronize token stream */` |
| 1024519 | 12232 | `		pGen->pEnd = pTmp;` |
| 1024519 | 12233 | `		pGen->pIn  = pEnd;` |
| 1024519 | 12234 | `		if( rc == SXERR_ABORT ){` |
|      13 | 12235 | `			SySetRelease(&sExprNode);` |
|      13 | 12236 | `			return SXERR_ABORT;` |
|       - | 12237 | `		}` |
|  512252 | 12238 | `	}` |
| 1024519 | 12239 | `	SySetRelease(&sExprNode);` |
| 1024519 | 12240 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  512267 | 12241 | `}` |
|       - | 12242 | `/*` |
|       - | 12243 | ` * Return a pointer to the node construct handler associated` |
|       - | 12244 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 12245 | ` */` |
|  266526 | 12246 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 12247 | `{` |
|  266531 | 12248 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 12249 | `		/* Numeric literal: Either real or integer */` |
|  134961 | 12250 | `		return PH7_CompileNumLiteral;` |
|  131575 | 12251 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 12252 | `		/* Double quoted string */` |
|   24771 | 12253 | `		return PH7_CompileString;` |
|  106809 | 12254 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 12255 | `		/* Single quoted string */` |
|  106689 | 12256 | `		return PH7_CompileSimpleString;` |
|     125 | 12257 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 12258 | `		/* Heredoc */` |
|      71 | 12259 | `		return PH7_CompileHereDoc;` |
|      59 | 12260 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 12261 | `		/* Nowdoc */` |
|      51 | 12262 | `		return PH7_CompileNowDoc;` |
|       9 | 12263 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 12264 | `		/* Backtick quoted string */` |
|       6 | 12265 | `		return PH7_CompileBacktic;` |
|       - | 12266 | `	}` |
|       3 | 12267 | `	return 0;` |
|  133268 | 12268 | `}` |
|       - | 12269 | `/*` |
|       - | 12270 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 12271 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 12272 | ` * in write context" parse error.` |
|       - | 12273 | ` */` |
|    6712 | 12274 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 12275 | `{` |
|       - | 12276 | `	sxi32 rc;` |
|    6717 | 12277 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6715 | 12278 | `		return SXRET_OK;` |
|       - | 12279 | `	}` |
|       5 | 12280 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 12281 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 12282 | `		"Can't use nullsafe operator in write context");` |
|       3 | 12283 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3361 | 12284 | `}` |
|       - | 12285 | `/*` |
|       - | 12286 | ` * Compile an unset() statement.` |
|       - | 12287 | ` * unset($var, $arr[$key], ...);` |
|       - | 12288 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 12289 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 12290 | ` * parent array before extracting the element to unset.` |
|       - | 12291 | ` */` |
|    2888 | 12292 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 12293 | `{` |
|    2893 | 12294 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2893 | 12295 | `	sxu32 nIdx = 0;` |
|       - | 12296 | `	SyString sName;` |
|       - | 12297 | `	sxi32 rc;` |
|       - | 12298 | `	/* Jump the 'unset' keyword */` |
|    2893 | 12299 | `	pGen->pIn++;` |
|       - | 12300 | `	/* Save delimiter */` |
|    2893 | 12301 | `	pTmp = pGen->pEnd;` |
|       - | 12302 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2893 | 12303 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2893 | 12304 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 12305 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 12306 | `		SyToken *pClose;` |
|    2893 | 12307 | `		pGen->pIn++;   /* Skip '(' */` |
|    2893 | 12308 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2893 | 12309 | `		pEnd = pClose; /* Stop at ')' */` |
|    1444 | 12310 | `	}` |
|    2893 | 12311 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 12312 | `	/* Resolve the 'unset' builtin name once */` |
|    2893 | 12313 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     373 | 12314 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     373 | 12315 | `		if( pObj == 0 ){` |
|     ! 0 | 12316 | `			return SXERR_ABORT;` |
|       - | 12317 | `		}` |
|     373 | 12318 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     373 | 12319 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     184 | 12320 | `	}` |
|       - | 12321 | `	/* Compile each comma-separated argument */` |
|    9607 | 12322 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6719 | 12323 | `		if( pGen->pIn < pNext ){` |
|    6719 | 12324 | `			pGen->pEnd = pNext;` |
|    6719 | 12325 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 12326 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 12327 | `				GenStateUnsetValidator);` |
|    6719 | 12328 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 12329 | `				return SXERR_ABORT;` |
|       - | 12330 | `			}` |
|    6719 | 12331 | `			if( rc != SXERR_EMPTY ){` |
|       - | 12332 | `				/* Emit call for this single argument */` |
|    6717 | 12333 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6717 | 12334 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6717 | 12335 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3356 | 12336 | `			}` |
|    3357 | 12337 | `		}` |
|       - | 12338 | `		/* Jump trailing commas */` |
|   10547 | 12339 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3833 | 12340 | `			pNext++;` |
|       5 | 12341 | `		}` |
|    6719 | 12342 | `		pGen->pIn = pNext;` |
|       5 | 12343 | `	}` |
|       - | 12344 | `	/* Skip past the closing ')' if present */` |
|    2893 | 12345 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2893 | 12346 | `		pGen->pIn++;` |
|    1444 | 12347 | `	}` |
|       - | 12348 | `	/* Restore token stream */` |
|    2893 | 12349 | `	pGen->pEnd = pTmp;` |
|    2893 | 12350 | `	return SXRET_OK;` |
|    1449 | 12351 | `}` |
|       - | 12352 | `/*` |
|       - | 12353 | ` * PHP Language construct table.` |
|       - | 12354 | ` */` |
|       - | 12355 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 12356 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 12357 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 12358 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 12359 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 12360 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 12361 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 12362 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 12363 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 12364 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 12365 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 12366 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 12367 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 12368 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 12369 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 12370 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 12371 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 12372 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 12373 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 12374 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 12375 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 12376 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 12377 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 12378 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 12379 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 12380 | `};` |
|       - | 12381 | `/*` |
|       - | 12382 | ` * Return a pointer to the statement handler routine associated` |
|       - | 12383 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 12384 | ` */` |
|  694290 | 12385 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 12386 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 12387 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 12388 | `	)` |
|       5 | 12389 | `{` |
|  694295 | 12390 | `	sxu32 n = 0;` |
| 3655964 | 12391 | `	for(;;){` |
| 7311933 | 12392 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  151189 | 12393 | `			break;` |
|       - | 12394 | `		}` |
| 7160749 | 12395 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  543111 | 12396 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 12397 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 12398 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 12399 | `					/* 'static' (class context),return null */` |
|     ! 0 | 12400 | `					return 0;` |
|       - | 12401 | `				}` |
|     ! 0 | 12402 | `			}` |
|  543106 | 12403 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       8 | 12404 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       9 | 12405 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 12406 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 12407 | `				return 0;` |
|       - | 12408 | `			}` |
|       - | 12409 | `			/* Return a pointer to the handler.` |
|       - | 12410 | `			*/` |
|  543111 | 12411 | `			return aLangConstruct[n].xConstruct;` |
|       - | 12412 | `		}` |
| 6617643 | 12413 | `		n++;` |
|       5 | 12414 | `	}` |
|  151189 | 12415 | `	if( pLookahed ){` |
|  151189 | 12416 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   42219 | 12417 | `			return PH7_CompileClassInterface;` |
|  108975 | 12418 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|  108485 | 12419 | `			return PH7_CompileClass;` |
|     495 | 12420 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      69 | 12421 | `			return PH7_CompileTrait;` |
|       - | 12422 | `		}` |
|       - | 12423 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 12424 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 12425 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 12426 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     213 | 12427 | `	}` |
|       - | 12428 | `	/* Not a language construct */` |
|     431 | 12429 | `	return 0;` |
|  347150 | 12430 | `}` |
|       - | 12431 | `/*` |
|       - | 12432 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 12433 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 12434 | ` */` |
|     426 | 12435 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 12436 | `{` |
|       - | 12437 | `	int rc;` |
|     431 | 12438 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     431 | 12439 | `	if( rc == FALSE ){` |
|     312 | 12440 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     311 | 12441 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 12442 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 12443 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 12444 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 12445 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 12446 | `			*/` |
|       - | 12447 | `			){` |
|     309 | 12448 | `				rc = TRUE;` |
|     152 | 12449 | `		}` |
|     156 | 12450 | `	}` |
|     431 | 12451 | `	return rc;` |
|       5 | 12452 | `}` |
|       - | 12453 | `/*` |
|       - | 12454 | ` * Compile a PHP chunk.` |
|       - | 12455 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 12456 | ` * takes care of generating the appropriate error message.` |
|       - | 12457 | ` */` |
|  829678 | 12458 | `static sxi32 GenStateCompileChunk(` |
|       - | 12459 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 12460 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 12461 | `	)` |
|       5 | 12462 | `{` |
|       - | 12463 | `	ProcLangConstruct xCons;` |
|       - | 12464 | `	sxi32 rc;` |
|  829683 | 12465 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  652977 | 12466 | `	for(;;){` |
| 1067821 | 12467 | `		int bStmtIsDeclare = 0;` |
| 1067821 | 12468 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 12469 | `			/* No more input to process */` |
|   18087 | 12470 | `			break;` |
|       - | 12471 | `		}` |
|       - | 12472 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 12473 | `		 * below doesn't fire before the directive has a chance to run. */` |
| 1049739 | 12474 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  698159 | 12475 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  698159 | 12476 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      47 | 12477 | `				bStmtIsDeclare = 1;` |
|      21 | 12478 | `			}` |
|  349077 | 12479 | `		}` |
| 1049739 | 12480 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 12481 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 12482 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  238111 | 12483 | `			pGen->bStrictTypesLocked = 1;` |
|  119053 | 12484 | `		}` |
| 1049739 | 12485 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 12486 | `			/* Compile block */` |
|    3853 | 12487 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|    3853 | 12488 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 12489 | `				break;` |
|       - | 12490 | `			}` |
|    1929 | 12491 | `		}else{` |
| 1045891 | 12492 | `			xCons = 0;` |
| 1045891 | 12493 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 12494 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 12495 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 12496 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    3895 | 12497 | `				xCons = PH7_CompileClassModifiers;` |
| 1043946 | 12498 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  694295 | 12499 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 12500 | `				/* Try to extract a language construct handler */` |
|  694295 | 12501 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  694295 | 12502 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 12503 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 12504 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 12505 | `						&pGen->pIn->sData);` |
|       9 | 12506 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 12507 | `						break;` |
|       - | 12508 | `					}` |
|       - | 12509 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 12510 | `					 * this erroneous statement.` |
|       - | 12511 | `					 */` |
|       9 | 12512 | `					xCons = PH7_ErrorRecover;` |
|       4 | 12513 | `				}` |
|  694856 | 12514 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   57359 | 12515 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 12516 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 12517 | `				xCons = PH7_CompileLabel;` |
|      56 | 12518 | `			}` |
| 1045891 | 12519 | `			if( xCons == 0 ){` |
|       - | 12520 | `				/* Assume an expression an try to compile it */` |
|  348017 | 12521 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  348017 | 12522 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 12523 | `					/* Pop l-value */` |
|  347867 | 12524 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  173931 | 12525 | `				}` |
|  174011 | 12526 | `			}else{` |
|       - | 12527 | `				/* Go compile the sucker */` |
|  697879 | 12528 | `				rc = xCons(&(*pGen));` |
|       - | 12529 | `			}` |
| 1045891 | 12530 | `			if( rc == SXERR_ABORT ){` |
|       - | 12531 | `				/* Request to abort compilation */` |
|      13 | 12532 | `				break;` |
|       - | 12533 | `			}` |
|       - | 12534 | `		}` |
|       - | 12535 | `		/* Ignore trailing semi-colons ';' */` |
| 1689889 | 12536 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  640165 | 12537 | `			pGen->pIn++;` |
|       5 | 12538 | `		}` |
| 1049729 | 12539 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 12540 | `			/* Compile a single statement and return */` |
|  811591 | 12541 | `			break;` |
|       - | 12542 | `		}` |
|       - | 12543 | `		/* LOOP ONE */` |
|       - | 12544 | `		/* LOOP TWO */` |
|       - | 12545 | `		/* LOOP THREE */` |
|       - | 12546 | `		/* LOOP FOUR */` |
|       5 | 12547 | `	}` |
|       - | 12548 | `	/* Return compilation status */` |
|  829683 | 12549 | `	return rc;` |
|       5 | 12550 | `}` |
|       - | 12551 | `/*` |
|       - | 12552 | ` * Compile a Raw PHP chunk.` |
|       - | 12553 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 12554 | ` * takes care of generating the appropriate error message.` |
|       - | 12555 | ` */` |
|   18094 | 12556 | `static sxi32 PH7_CompilePHP(` |
|       - | 12557 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 12558 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 12559 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 12560 | `	)` |
|       5 | 12561 | `{` |
|   18099 | 12562 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 12563 | `	sxi32 rc;` |
|       - | 12564 | `	/* Reset the token set */` |
|   18099 | 12565 | `	SySetReset(&(*pTokenSet));` |
|       - | 12566 | `	/* Mark as the default token set */` |
|   18099 | 12567 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 12568 | `	/* Advance the stream cursor */` |
|   18099 | 12569 | `	pGen->pRawIn++;` |
|       - | 12570 | `	/* Tokenize the PHP chunk first */` |
|   18099 | 12571 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 12572 | `	/* Point to the head and tail of the token stream. */` |
|   18099 | 12573 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   18099 | 12574 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   18099 | 12575 | `	if( is_expr ){` |
|     ! 0 | 12576 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 12577 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 12578 | `			/* A simple expression,compile it */` |
|     ! 0 | 12579 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 12580 | `		}` |
|       - | 12581 | `		/* Emit the DONE instruction */` |
|     ! 0 | 12582 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 12583 | `		return SXRET_OK;` |
|       - | 12584 | `	}` |
|   18099 | 12585 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 12586 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 12587 | `		/*` |
|       - | 12588 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 12589 | `		 * According to the PHP reference manual:` |
|       - | 12590 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 12591 | `		 *  immediately follow` |
|       - | 12592 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 12593 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 12594 | `		 * Symisc extension:` |
|       - | 12595 | `		 *   This short syntax works with all PHP opening` |
|       - | 12596 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 12597 | `		 *   only short tag.` |
|       - | 12598 | `		 */` |
|       - | 12599 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 12600 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 12601 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 12602 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 12603 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 12604 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 12605 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 12606 | `		}` |
|       3 | 12607 | `		return SXRET_OK;` |
|       - | 12608 | `	}` |
|       - | 12609 | `	/* Compile the PHP chunk */` |
|   18097 | 12610 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 12611 | `	/* Fix exceptions jumps */` |
|   18097 | 12612 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 12613 | `	/* Fix gotos now, the jump destination is resolved */` |
|   18097 | 12614 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 12615 | `		rc = SXERR_ABORT;` |
|       1 | 12616 | `	}` |
|       - | 12617 | `	/* Reset container */` |
|   18097 | 12618 | `	SySetReset(&pGen->aGoto);` |
|   18097 | 12619 | `	SySetReset(&pGen->aLabel);` |
|   18097 | 12620 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 12621 | `	/* Compilation result */` |
|   18097 | 12622 | `	return rc;` |
|    9052 | 12623 | `}` |
|       - | 12624 | `/*` |
|       - | 12625 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 12626 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 12627 | ` * This is the only compile interface exported from this file.` |
|       - | 12628 | ` */` |
|   20998 | 12629 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 12630 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 12631 | `	SyString *pScript,  /* Script to compile */` |
|       - | 12632 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 12633 | `	)` |
|       5 | 12634 | `{` |
|       - | 12635 | `	SySet aPhpToken,aRawToken;` |
|       - | 12636 | `	ph7_gen_state *pCodeGen;` |
|       - | 12637 | `	ph7_value *pRawObj;` |
|       - | 12638 | `	sxu32 nObjIdx;` |
|       - | 12639 | `	sxi32 nRawObj;` |
|       - | 12640 | `	int is_expr;` |
|       - | 12641 | `	sxi8 bSavedStrict;` |
|       - | 12642 | `	sxi8 bSavedStrictLocked;` |
|       - | 12643 | `	sxi32 rc;` |
|   21003 | 12644 | `	if( pScript->nByte < 1 ){` |
|       - | 12645 | `		/* Nothing to compile */` |
|     ! 0 | 12646 | `		return PH7_OK;` |
|       - | 12647 | `	}` |
|       - | 12648 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 12649 | `	 * file's flags so include/require restore them on return. */` |
|   21003 | 12650 | `	pCodeGen = &pVm->sCodeGen;` |
|   21003 | 12651 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   21003 | 12652 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   21003 | 12653 | `	pCodeGen->bStrictTypes = 0;` |
|   21003 | 12654 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 12655 | `	/* Initialize the tokens containers */` |
|   21003 | 12656 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   21003 | 12657 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   21003 | 12658 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   21003 | 12659 | `	is_expr = 0;` |
|   21003 | 12660 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 12661 | `		SyToken sTmp;` |
|       - | 12662 | `		/* PHP only: -*/` |
|    7763 | 12663 | `		sTmp.nLine = 1;` |
|    7763 | 12664 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    7763 | 12665 | `		sTmp.pUserData = 0;` |
|    7763 | 12666 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    7763 | 12667 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    7763 | 12668 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 12669 | `			/* A simple PHP expression */` |
|     ! 0 | 12670 | `			is_expr = 1;` |
|     ! 0 | 12671 | `		}` |
|    3884 | 12672 | `	}else{` |
|       - | 12673 | `		/* Tokenize raw text */` |
|   13245 | 12674 | `		SySetAlloc(&aRawToken,32);` |
|   13245 | 12675 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 12676 | `	}` |
|       - | 12677 | `	/* Process high-level tokens */` |
|   21003 | 12678 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   21003 | 12679 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   21003 | 12680 | `	rc = PH7_OK;` |
|   21003 | 12681 | `	if( is_expr ){` |
|       - | 12682 | `		/* Compile the expression */` |
|     ! 0 | 12683 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 12684 | `		goto cleanup;` |
|       - | 12685 | `	}` |
|   21003 | 12686 | `	nObjIdx = 0;` |
|       - | 12687 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 12688 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 12689 | `	 * preventing namespace bleeding across include()d files. */` |
|   21003 | 12690 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 12691 | `	/* Start the compilation process */` |
|   17125 | 12692 | `	for(;;){` |
|   52337 | 12693 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   20991 | 12694 | `			break; /* No more tokens to process */` |
|       - | 12695 | `		}` |
|   31351 | 12696 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 12697 | `			/* Compile the PHP chunk */` |
|   18099 | 12698 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   18099 | 12699 | `			if( rc == SXERR_ABORT ){` |
|      15 | 12700 | `				break;` |
|       - | 12701 | `			}` |
|   18087 | 12702 | `			continue;` |
|       - | 12703 | `		}` |
|       - | 12704 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13257 | 12705 | `		nRawObj = 0;` |
|   26551 | 12706 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 12707 | `			/* Consume the raw chunk without any processing */` |
|   13299 | 12708 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13299 | 12709 | `			if( pRawObj == 0 ){` |
|     ! 0 | 12710 | `				rc = SXERR_MEM;` |
|     ! 0 | 12711 | `				break;` |
|       - | 12712 | `			}` |
|       - | 12713 | `			/* Mark as constant and emit the load constant instruction */` |
|   13299 | 12714 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13299 | 12715 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13299 | 12716 | `			++nRawObj;` |
|   13299 | 12717 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 12718 | `		}` |
|   13257 | 12719 | `		if( nRawObj > 0 ){` |
|       - | 12720 | `			/* Emit the consume instruction */` |
|   13257 | 12721 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6626 | 12722 | `		}` |
|   10504 | 12723 | `	}` |
|   10499 | 12724 | `cleanup:` |
|   21003 | 12725 | `	SySetRelease(&aRawToken);` |
|   21003 | 12726 | `	SySetRelease(&aPhpToken);` |
|       - | 12727 | `	/* Restore outer file's strict_types scope */` |
|   21003 | 12728 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   21003 | 12729 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   21003 | 12730 | `	return rc;` |
|   10504 | 12731 | `}` |
|       - | 12732 | `/*` |
|       - | 12733 | ` * Utility routines.Initialize the code generator.` |
|       - | 12734 | ` */` |
|    3830 | 12735 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 12736 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12737 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12738 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12739 | `	)` |
|       5 | 12740 | `{` |
|    3835 | 12741 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12742 | `	/* Zero the structure */` |
|    3835 | 12743 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 12744 | `	/* Initial state */` |
|    3835 | 12745 | `	pGen->pVm  = &(*pVm);` |
|    3835 | 12746 | `	pGen->xErr = xErr;` |
|    3835 | 12747 | `	pGen->pErrData = pErrData;` |
|    3835 | 12748 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3835 | 12749 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3835 | 12750 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3835 | 12751 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3835 | 12752 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 12753 | `	/* Error log buffer */` |
|    3835 | 12754 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 12755 | `	/* General purpose working buffer */` |
|    3835 | 12756 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 12757 | `	/* Namespace state */` |
|    3835 | 12758 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3835 | 12759 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3835 | 12760 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3835 | 12761 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12762 | `	/* Create the global scope */` |
|    3835 | 12763 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 12764 | `	/* Point to the global scope */` |
|    3835 | 12765 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3835 | 12766 | `	return SXRET_OK;` |
|       5 | 12767 | `}` |
|       - | 12768 | `/*` |
|       - | 12769 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 12770 | ` */` |
|   24456 | 12771 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 12772 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12773 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12774 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12775 | `	)` |
|       5 | 12776 | `{` |
|   24461 | 12777 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12778 | `	GenBlock *pBlock,*pParent;` |
|       - | 12779 | `	/* Reset state */` |
|   24461 | 12780 | `	SySetReset(&pGen->aLabel);` |
|   24461 | 12781 | `	SySetReset(&pGen->aGoto);` |
|   24461 | 12782 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   24461 | 12783 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   24461 | 12784 | `	SyBlobRelease(&pGen->sWorker);` |
|   24461 | 12785 | `	SyBlobRelease(&pGen->sNamespace);` |
|   24461 | 12786 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   24461 | 12787 | `	SyHashRelease(&pGen->hUseImports);` |
|   24461 | 12788 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   24461 | 12789 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   24461 | 12790 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   24461 | 12791 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   24461 | 12792 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12793 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 12794 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 12795 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 12796 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 12797 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 12798 | `	 * number of unique names, which is acceptable. */` |
|       - | 12799 | `	/* Point to the global scope */` |
|   24461 | 12800 | `	pBlock = pGen->pCurrent;` |
|   24461 | 12801 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 12802 | `		pParent = pBlock->pParent;` |
|     ! 0 | 12803 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 12804 | `		pBlock = pParent;` |
|     ! 0 | 12805 | `	}` |
|   24461 | 12806 | `	pGen->xErr = xErr;` |
|   24461 | 12807 | `	pGen->pErrData = pErrData;` |
|   24461 | 12808 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   24461 | 12809 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   24461 | 12810 | `	pGen->pIn = pGen->pEnd = 0;` |
|   24461 | 12811 | `	pGen->nErr = 0;` |
|   24461 | 12812 | `	return SXRET_OK;` |
|       5 | 12813 | `}` |
|       - | 12814 | `/*` |
|       - | 12815 | ` * Generate a compile-time error message.` |
|       - | 12816 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 12817 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 12818 | ` * abort compilation immediately.` |
|       - | 12819 | ` */` |
|     642 | 12820 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 12821 | `{` |
|     647 | 12822 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     647 | 12823 | `	const char *zErr = "Error";` |
|       - | 12824 | `	SyString *pFile;` |
|       - | 12825 | `	va_list ap;` |
|       - | 12826 | `	sxi32 rc;` |
|       - | 12827 | `	/* Reset the working buffer */` |
|     647 | 12828 | `	SyBlobReset(pWorker);` |
|       - | 12829 | `	/* Peek the processed file path if available */` |
|     647 | 12830 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     647 | 12831 | `	if( nErrType == E_ERROR ){` |
|       - | 12832 | `		/* Increment the error counter */` |
|     533 | 12833 | `		pGen->nErr++;` |
|     533 | 12834 | `		if( pGen->nErr > 15 ){` |
|       - | 12835 | `			/* Error count limit reached */` |
|       6 | 12836 | `			if( pGen->xErr ){` |
|       6 | 12837 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       6 | 12838 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       6 | 12839 | `				if( pFile ){` |
|       6 | 12840 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 12841 | `				}` |
|       6 | 12842 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       6 | 12843 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       6 | 12844 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 12845 | `				}` |
|       2 | 12846 | `			}` |
|       - | 12847 | `			/* Abort immediately */` |
|       6 | 12848 | `			return SXERR_ABORT;` |
|       - | 12849 | `		}` |
|     262 | 12850 | `	}` |
|     643 | 12851 | `	if( pGen->xErr == 0 ){` |
|       - | 12852 | `		/* No available error consumer,return immediately */` |
|       3 | 12853 | `		return SXRET_OK;` |
|       - | 12854 | `	}` |
|     640 | 12855 | `	switch(nErrType){` |
|     526 | 12856 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      32 | 12857 | `	case E_WARNING: zErr = "Warning";     break;` |
|      82 | 12858 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      12 | 12859 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 12860 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 12861 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 12862 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 12863 | `	default:` |
|     ! 0 | 12864 | `		break;` |
|       - | 12865 | `	}` |
|     640 | 12866 | `	rc = SXRET_OK;` |
|       - | 12867 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     640 | 12868 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     640 | 12869 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     640 | 12870 | `	va_start(ap,zFormat);` |
|     640 | 12871 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     640 | 12872 | `	va_end(ap);` |
|     640 | 12873 | `	if( pFile ){` |
|     640 | 12874 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     318 | 12875 | `	}` |
|       - | 12876 | `	/* Append a new line */` |
|     640 | 12877 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     640 | 12878 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 12879 | `		/* Consume the generated error message */` |
|     640 | 12880 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     318 | 12881 | `	}` |
|     640 | 12882 | `	return rc;` |
|     326 | 12883 | `}` |
|       - | 12884 |  |
