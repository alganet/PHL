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
|    4112 |   137 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   138 | `{` |
|    4117 |   139 | `	GenBlock *pBlock = pCurrent;` |
|   11748 |   140 | `	for(;;){` |
|   23501 |   141 | `		if( pBlock->iFlags & iBlockType ){` |
|    4009 |   142 | `			iCount--; /* Decrement nesting level */` |
|    4009 |   143 | `			if( iCount < 1 ){` |
|       - |   144 | `				/* Block meet with the desired criteria */` |
|    3983 |   145 | `				return pBlock;` |
|       - |   146 | `			}` |
|      13 |   147 | `		}` |
|       - |   148 | `		/* Point to the upper block */` |
|   19523 |   149 | `		pBlock = pBlock->pParent;` |
|   19523 |   150 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   151 | `			/* Forbidden */` |
|      72 |   152 | `			break;` |
|       - |   153 | `		}` |
|       5 |   154 | `	}` |
|       - |   155 | `	/* No such block */` |
|     139 |   156 | `	return 0;` |
|    2061 |   157 | `}` |
|       - |   158 | `/*` |
|       - |   159 | ` * Initialize a freshly allocated block instance.` |
|       - |   160 | ` */` |
|  908734 |   161 | `static void GenStateInitBlock(` |
|       - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   166 | `	void *pUserData      /* Upper layer private data */` |
|       - |   167 | `	)` |
|       5 |   168 | `{` |
|       - |   169 | `	/* Initialize block fields */` |
|  908739 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  908739 |   171 | `	pBlock->pUserData   = pUserData;` |
|  908739 |   172 | `	pBlock->pGen        = pGen;` |
|  908739 |   173 | `	pBlock->iFlags      = iType;` |
|  908739 |   174 | `	pBlock->pParent     = 0;` |
|  908739 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  908739 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  908739 |   177 | `}` |
|       - |   178 | `/*` |
|       - |   179 | ` * Allocate a new block instance.` |
|       - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   182 | ` * processing on failure.` |
|       - |   183 | ` */` |
|  904906 |   184 | `static sxi32 GenStateEnterBlock(` |
|       - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   190 | `	)` |
|       5 |   191 | `{` |
|       - |   192 | `	GenBlock *pBlock;` |
|       - |   193 | `	/* Allocate a new block instance */` |
|  904911 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  904911 |   195 | `	if( pBlock == 0 ){` |
|       - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   198 | `		 */` |
|     ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   200 | `		/* Abort processing immediately */` |
|     ! 0 |   201 | `		return SXERR_ABORT;` |
|       - |   202 | `	}` |
|       - |   203 | `	/* Zero the structure */` |
|  904911 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  904911 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   206 | `	/* Link to the parent block */` |
|  904911 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   208 | `	/* Mark as the current block */` |
|  904911 |   209 | `	pGen->pCurrent = pBlock;` |
|  904911 |   210 | `	if( ppBlock ){` |
|       - |   211 | `		/* Write a pointer to the new instance */` |
|  437699 |   212 | `		*ppBlock = pBlock;` |
|  218847 |   213 | `	}` |
|  904911 |   214 | `	return SXRET_OK;` |
|  452458 |   215 | `}` |
|       - |   216 | `/*` |
|       - |   217 | ` * Release block fields without freeing the whole instance.` |
|       - |   218 | ` */` |
|  904898 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   220 | `{` |
|  904903 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  904903 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  904903 |   223 | `}` |
|       - |   224 | `/*` |
|       - |   225 | ` * Release a block.` |
|       - |   226 | ` */` |
|  904898 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   228 | `{` |
|  904903 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  904903 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   231 | `	/* Free the instance */` |
|  904903 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  904903 |   233 | `}` |
|       - |   234 | `/*` |
|       - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   236 | ` */` |
|  904898 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   238 | `{` |
|  904903 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  904903 |   240 | `	if( pBlock == 0 ){` |
|       - |   241 | `		/* No more block to pop */` |
|     ! 0 |   242 | `		return SXERR_EMPTY;` |
|       - |   243 | `	}` |
|       - |   244 | `	/* Point to the upper block */` |
|  904903 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  904903 |   246 | `	if( ppBlock ){` |
|       - |   247 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   248 | `		*ppBlock = pBlock;` |
|     ! 0 |   249 | `	}else{` |
|       - |   250 | `		/* Safely release the block */` |
|  904903 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   252 | `	}` |
|  904903 |   253 | `	return SXRET_OK;` |
|  452454 |   254 | `}` |
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
|  258652 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   266 | `{` |
|       - |   267 | `	JumpFixup sJumpFix;` |
|       - |   268 | `	sxi32 rc;` |
|       - |   269 | `	/* Init the JumpFixup structure */` |
|  258657 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  258657 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   272 | `	/* Insert in the jump fixup table */` |
|  258657 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  258657 |   274 | `	return rc;` |
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
|  630762 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   288 | `{` |
|       - |   289 | `	JumpFixup *aFix;` |
|       - |   290 | `	VmInstr *pInstr;` |
|       - |   291 | `	sxu32 nFixed;` |
|       - |   292 | `	sxu32 n;` |
|       - |   293 | `	/* Point to the jump fixup table */` |
|  630767 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   295 | `	/* Fix the desired jumps */` |
| 1135539 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  504777 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   298 | `			/* Already fixed */` |
|  199327 |   299 | `			continue;` |
|       - |   300 | `		}` |
|  305455 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   302 | `			/* Not of our interest */` |
|   46805 |   303 | `			continue;` |
|       - |   304 | `		}` |
|       - |   305 | `		/* Point to the instruction to fix */` |
|  258655 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  258655 |   307 | `		if( pInstr ){` |
|  258655 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  258655 |   309 | `			nFixed++;` |
|       - |   310 | `			/* Mark as fixed */` |
|  258655 |   311 | `			aFix[n].nJumpType = -1;` |
|  129325 |   312 | `		}` |
|  129330 |   313 | `	}` |
|       - |   314 | `	/* Total number of fixed jumps */` |
|  630767 |   315 | `	return nFixed;` |
|       5 |   316 | `}` |
|       - |   317 | `/*` |
|       - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   319 | ` * The goto statement can be used to jump to another section` |
|       - |   320 | ` * in the program.` |
|       - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   322 | ` * statement for more information.` |
|       - |   323 | ` */` |
|  258160 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   325 | `{` |
|       - |   326 | `	JumpFixup *pJump,*aJumps;` |
|       - |   327 | `	Label *pLabel,*aLabel;` |
|       - |   328 | `	VmInstr *pInstr;` |
|       - |   329 | `	sxi32 rc;` |
|       - |   330 | `	sxu32 n;` |
|       - |   331 | `	/* Point to the goto table */` |
|  258165 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   333 | `	/* Fix */` |
|  258311 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  258163 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  258295 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   362 | `			/* Emit a warning */` |
|      40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   365 | `		}` |
|      71 |   366 | `	}` |
|  258163 |   367 | `	return SXRET_OK;` |
|  129085 |   368 | `}` |
|       - |   369 | `/*` |
|       - |   370 | ` * Check if a given token value is installed in the literal table.` |
|       - |   371 | ` */` |
|  822000 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   373 | `{` |
|       - |   374 | `	SyHashEntry *pEntry;` |
|  822005 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  822005 |   376 | `	if( pEntry == 0 ){` |
|  370841 |   377 | `		return SXERR_NOTFOUND;` |
|       - |   378 | `	}` |
|  451169 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  451169 |   380 | `	return SXRET_OK;` |
|  411005 |   381 | `}` |
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
|  370836 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   393 | `{` |
|  370841 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  370841 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  185418 |   396 | `	}` |
|  370841 |   397 | `	return SXRET_OK;` |
|       5 |   398 | `}` |
|       - |   399 | `/*` |
|       - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   401 | ` * in the constant table.` |
|       - |   402 | ` */` |
|  134186 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   404 | `{` |
|       - |   405 | `	ph7_value *pObj;` |
|  134191 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   407 | `	/* Reserve a new constant */` |
|  134191 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  134191 |   409 | `	if( pObj == 0 ){` |
|     ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   411 | `		return 0;` |
|       - |   412 | `	}` |
|  134191 |   413 | `	*pIdx = nIdx;` |
|       - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   416 | `	 */` |
|  134191 |   417 | `	return pObj;` |
|   67098 |   418 | `}` |
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
|  513656 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   434 | `{` |
|       - |   435 | `	VmCallArgMap *pMap;` |
|  513661 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|      39 |   437 | `	if( p3 == 0 ){` |
|      35 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      35 |   439 | `		if( pMap == 0 ) return 0;` |
|      35 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      35 |   441 | `		p3 = (void *)pMap;` |
|      16 |   442 | `	}` |
|      39 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      39 |   444 | `	return p3;` |
|  256833 |   445 | `}` |
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
|  134942 |   504 | `static int GenStateFindBadNumericSeparator(` |
|       - |   505 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   506 | `{` |
|  134947 |   507 | `	const char *z = pRaw->zString;` |
|  134947 |   508 | `	sxu32 n = pRaw->nByte;` |
|  134947 |   509 | `	int base = 10;` |
|       - |   510 | `	sxu32 i, start;` |
|  134947 |   511 | `	if( n < 2 ) return 0;` |
|   11181 |   512 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   513 | `		base = 16;` |
|   11146 |   514 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   515 | `		base = 2;` |
|     139 |   516 | `	}` |
|   40181 |   517 | `	for( i = 0; i < n; ++i ){` |
|   29019 |   518 | `		if( z[i] != '_' ) continue;` |
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
|   11167 |   535 | `	return 0;` |
|   67476 |   536 | `}` |
|       - |   537 | `/*` |
|       - |   538 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   539 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   540 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   541 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   542 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   543 | ` * so callers can bail from the current construct).` |
|       - |   544 | ` */` |
|  134942 |   545 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   546 | `{` |
|  134947 |   547 | `	const char *zBad = 0;` |
|  134947 |   548 | `	sxu32 nBad = 0;` |
|       - |   549 | `	SyString sBad;` |
|       - |   550 | `	sxi32 rc;` |
|  134947 |   551 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  134933 |   552 | `		return SXRET_OK;` |
|       - |   553 | `	}` |
|      18 |   554 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   555 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   556 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   557 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   558 | `		return SXERR_ABORT;` |
|       - |   559 | `	}` |
|      18 |   560 | `	return SXERR_SYNTAX;` |
|   67476 |   561 | `}` |
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
|  134928 |   578 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   579 | `	SyMemBackend *pAlloc,` |
|       - |   580 | `	const SyString *pToken,` |
|       - |   581 | `	char *zScratch, sxu32 nScratch,` |
|       - |   582 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   583 | `{` |
|       - |   584 | `	sxu32 i, j;` |
|  134933 |   585 | `	int hasUnderscore = 0;` |
|       - |   586 | `	char *zBuf;` |
|  134933 |   587 | `	*pzAlloc = 0;` |
|  285633 |   588 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  150957 |   589 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   75355 |   590 | `	}` |
|  134933 |   591 | `	if( !hasUnderscore ){` |
|  134681 |   592 | `		SyStringDupPtr(pOut, pToken);` |
|  134681 |   593 | `		return SXRET_OK;` |
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
|   67469 |   610 | `}` |
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
|  134914 |   627 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   628 | `{` |
|  134919 |   629 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  134919 |   630 | `	sxu32 nIdx = 0;` |
|       - |   631 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  134919 |   632 | `	char *zAlloc = 0;` |
|       - |   633 | `	SyString sNum;` |
|       - |   634 | `	sxi32 rc;` |
|   67457 |   635 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  134919 |   636 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  134919 |   637 | `	if( rc != SXRET_OK ){` |
|      14 |   638 | `		return rc;` |
|       - |   639 | `	}` |
|  202361 |   640 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   67452 |   641 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  134909 |   642 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   643 | `		return SXERR_ABORT;` |
|       - |   644 | `	}` |
|  134909 |   645 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   646 | `		ph7_value *pObj;` |
|       - |   647 | `		sxi64 iValue;` |
|  134191 |   648 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  134191 |   649 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  134191 |   650 | `		if( pObj == 0 ){` |
|     ! 0 |   651 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   652 | `			return SXERR_ABORT;` |
|       - |   653 | `		}` |
|  134191 |   654 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   67098 |   655 | `	}else{` |
|       - |   656 | `		/* Real number */` |
|       - |   657 | `		ph7_value *pObj;` |
|       - |   658 | `		/* Reserve a new constant */` |
|     722 |   659 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     722 |   660 | `		if( pObj == 0 ){` |
|     ! 0 |   661 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   662 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   663 | `			return SXERR_ABORT;` |
|       - |   664 | `		}` |
|     722 |   665 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     722 |   666 | `		PH7_MemObjToReal(pObj);` |
|       - |   667 | `	}` |
|  134909 |   668 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   669 | `	/* Emit the load constant instruction */` |
|  134909 |   670 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   671 | `	/* Node successfully compiled */` |
|  134909 |   672 | `	return SXRET_OK;` |
|   67462 |   673 | `}` |
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
|  106626 |   685 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   686 | `{` |
|  106631 |   687 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   688 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   689 | `	ph7_value *pObj;` |
|       - |   690 | `	sxu32 nIdx;` |
|  106631 |   691 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   692 | `	/* Delimit the string */` |
|  106631 |   693 | `	zIn  = pStr->zString;` |
|  106631 |   694 | `	zEnd = &zIn[pStr->nByte];` |
|  106631 |   695 | `	if( zIn >= zEnd ){` |
|       - |   696 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   697 | `		 * rather than reserving a new object each time. */` |
|    7835 |   698 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    7835 |   699 | `		return SXRET_OK;` |
|       - |   700 | `	}` |
|   98801 |   701 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   702 | `		/* Already processed,emit the load constant instruction` |
|       - |   703 | `		 * and return.` |
|       - |   704 | `		 */` |
|   37533 |   705 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   37533 |   706 | `		return SXRET_OK;` |
|       - |   707 | `	}` |
|       - |   708 | `	/* Reserve a new constant */` |
|   61273 |   709 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   61273 |   710 | `	if( pObj == 0 ){` |
|     ! 0 |   711 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   712 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   713 | `		return SXERR_ABORT;` |
|       - |   714 | `	}` |
|   61273 |   715 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   716 | `	/* Compile the node */` |
|   61327 |   717 | `	for(;;){` |
|  122659 |   718 | `		if( zIn >= zEnd ){` |
|       - |   719 | `			/* End of input */` |
|   61273 |   720 | `			break;` |
|       - |   721 | `		}` |
|   61391 |   722 | `		zCur = zIn;` |
| 1049229 |   723 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  987843 |   724 | `			zIn++;` |
|       5 |   725 | `		}` |
|   61391 |   726 | `		if( zIn > zCur ){` |
|       - |   727 | `			/* Append raw contents*/` |
|   61367 |   728 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   30681 |   729 | `		}` |
|   61391 |   730 | `		zIn++;` |
|   61391 |   731 | `		if( zIn < zEnd ){` |
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
|   61391 |   746 | `		zIn++;` |
|       5 |   747 | `	}` |
|       - |   748 | `	/* Emit the load constant instruction */` |
|   61273 |   749 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   61273 |   750 | `	if( pStr->nByte < 1024 ){` |
|       - |   751 | `		/* Install in the literal table */` |
|   61273 |   752 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   30634 |   753 | `	}` |
|       - |   754 | `	/* Node successfully compiled */` |
|   61273 |   755 | `	return SXRET_OK;` |
|   53318 |   756 | `}` |
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
|    2370 |   922 | `static sxi32 GenStateProcessStringExpression(` |
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
|    2375 |   933 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   934 | `	/* Preallocate some slots */` |
|    2375 |   935 | `	SySetAlloc(&sToken,0x08);` |
|       - |   936 | `	/* Tokenize the text */` |
|    2375 |   937 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   938 | `	/* Swap delimiter */` |
|    2375 |   939 | `	pTmpIn  = pGen->pIn;` |
|    2375 |   940 | `	pTmpEnd = pGen->pEnd;` |
|    2375 |   941 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2375 |   942 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   943 | `	/* Compile the expression */` |
|    2375 |   944 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   945 | `	/* Restore token stream */` |
|    2375 |   946 | `	pGen->pIn  = pTmpIn;` |
|    2375 |   947 | `	pGen->pEnd = pTmpEnd;` |
|       - |   948 | `	/* Release the token set */` |
|    2375 |   949 | `	SySetRelease(&sToken);` |
|       - |   950 | `	/* Compilation result */` |
|    2375 |   951 | `	return rc;` |
|       5 |   952 | `}` |
|       - |   953 | `/*` |
|       - |   954 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   955 | ` */` |
|   26542 |   956 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   957 | `{` |
|       - |   958 | `	ph7_value *pConstObj;` |
|   26547 |   959 | `	sxu32 nIdx = 0;` |
|       - |   960 | `	/* Reserve a new constant */` |
|   26547 |   961 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   26547 |   962 | `	if( pConstObj == 0 ){` |
|     ! 0 |   963 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   964 | `		return 0;` |
|       - |   965 | `	}` |
|   26547 |   966 | `	(*pCount)++;` |
|   26547 |   967 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   968 | `	/* Emit the load constant instruction */` |
|   26547 |   969 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   26547 |   970 | `	return pConstObj;` |
|   13276 |   971 | `}` |
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
|   24976 |  1034 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|       5 |  1035 | `{` |
|   24981 |  1036 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1037 | `	const char *zIn,*zCur,*zEnd;` |
|   24981 |  1038 | `	ph7_value *pObj = 0;` |
|       - |  1039 | `	sxi32 iCons;` |
|       - |  1040 | `	sxi32 rc;` |
|       - |  1041 | `	/* Delimit the string */` |
|   24981 |  1042 | `	zIn  = pStr->zString;` |
|   24981 |  1043 | `	zEnd = &zIn[pStr->nByte];` |
|   24981 |  1044 | `	if( zIn >= zEnd ){` |
|       - |  1045 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1046 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1047 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1048 | `		 */` |
|     301 |  1049 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     301 |  1050 | `		return SXRET_OK;` |
|       - |  1051 | `	}` |
|   24685 |  1052 | `	zCur = 0;` |
|       - |  1053 | `	/* Compile the node */` |
|   24685 |  1054 | `	iCons = 0;` |
|   13525 |  1055 | `	for(;;){` |
|   40805 |  1056 | `		zCur = zIn;` |
|  184203 |  1057 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  145773 |  1058 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      69 |  1059 | `				break;` |
|  145645 |  1060 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2246 |  1061 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1124 |  1062 | `					break;` |
|       - |  1063 | `			}` |
|  143403 |  1064 | `			zIn++;` |
|       5 |  1065 | `		}` |
|   40805 |  1066 | `		if( zIn > zCur ){` |
|   18343 |  1067 | `			if( pObj == 0 ){` |
|   17825 |  1068 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   17825 |  1069 | `				if( pObj == 0 ){` |
|     ! 0 |  1070 | `					return SXERR_ABORT;` |
|       - |  1071 | `				}` |
|    8910 |  1072 | `			}` |
|   18343 |  1073 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    9169 |  1074 | `		}` |
|   40805 |  1075 | `		if( zIn >= zEnd ){` |
|   24683 |  1076 | `			break;` |
|       - |  1077 | `		}` |
|   16127 |  1078 | `		if( zIn[0] == '\\' ){` |
|   13757 |  1079 | `			const char *zPtr = 0;` |
|       - |  1080 | `			sxu32 n;` |
|   13757 |  1081 | `			zIn++;` |
|   13757 |  1082 | `			if( pObj == 0 ){` |
|    8727 |  1083 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    8727 |  1084 | `				if( pObj == 0 ){` |
|     ! 0 |  1085 | `					return SXERR_ABORT;` |
|       - |  1086 | `				}` |
|    4361 |  1087 | `			}` |
|   13757 |  1088 | `			if( zIn >= zEnd ){` |
|       - |  1089 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|       3 |  1090 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       3 |  1091 | `				break;` |
|       - |  1092 | `			}` |
|   13755 |  1093 | `			n = sizeof(char); /* size of conversion */` |
|   13755 |  1094 | `			switch( zIn[0] ){` |
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
|    6329 |  1111 | `			case 'n':` |
|       - |  1112 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   12663 |  1113 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   12663 |  1114 | `				break;` |
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
|   13755 |  1237 | `			zIn += n;` |
|   13755 |  1238 | `			continue;` |
|       - |  1239 | `		}` |
|    2375 |  1240 | `		if( zIn[0] == '{' ){` |
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
|    2243 |  1274 | `			const char *zExpr = zIn;` |
|       - |  1275 | `			/* Assemble variable name */` |
|    1144 |  1276 | `			for(;;){` |
|       - |  1277 | `				/* Jump leading dollars */` |
|    4531 |  1278 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2243 |  1279 | `					zIn++;` |
|       5 |  1280 | `				}` |
|    1144 |  1281 | `				for(;;){` |
|   12203 |  1282 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8771 |  1283 | `						zIn++;` |
|       5 |  1284 | `					}` |
|    2293 |  1285 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1286 | `						/* UTF-8 stream */` |
|     ! 0 |  1287 | `						zIn++;` |
|     ! 0 |  1288 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1289 | `							zIn++;` |
|     ! 0 |  1290 | `						}` |
|     ! 0 |  1291 | `						continue;` |
|       - |  1292 | `					}` |
|    2293 |  1293 | `					break;` |
|     ! 0 |  1294 | `				}` |
|    2293 |  1295 | `				if( zIn >= zEnd ){` |
|     225 |  1296 | `					break;` |
|       - |  1297 | `				}` |
|    2073 |  1298 | `				if( zIn[0] == '[' ){` |
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
|    2063 |  1316 | `				}else if(zIn[0] == '{' ){` |
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
|    2059 |  1334 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1335 | `					/* Member access operator '->' */` |
|      53 |  1336 | `					zIn += 2;` |
|    2034 |  1337 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1338 | `					/* Static member access operator '::' */` |
|     ! 0 |  1339 | `					zIn += 2;` |
|     ! 0 |  1340 | `				}else{` |
|    1007 |  1341 | `					break;` |
|       - |  1342 | `				}` |
|       3 |  1343 | `			}` |
|       - |  1344 | `			/* Process the expression */` |
|    2243 |  1345 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2243 |  1346 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1347 | `				return SXERR_ABORT;` |
|       - |  1348 | `			}` |
|    2243 |  1349 | `			if( rc != SXERR_EMPTY ){` |
|    2241 |  1350 | `				++iCons;` |
|    1118 |  1351 | `			}` |
|       - |  1352 | `		}` |
|       - |  1353 | `		/* Invalidate the previously used constant */` |
|    2375 |  1354 | `		pObj = 0;` |
|       5 |  1355 | `	}/*for(;;)*/` |
|   24685 |  1356 | `	if( iCons > 1 ){` |
|       - |  1357 | `		/* Concatenate all compiled constants */` |
|    1751 |  1358 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     873 |  1359 | `	}` |
|       - |  1360 | `	/* Node successfully compiled */` |
|   24685 |  1361 | `	return SXRET_OK;` |
|   12493 |  1362 | `}` |
|       - |  1363 | `/*` |
|       - |  1364 | ` * Compile a double quoted string.` |
|       - |  1365 | ` *  See the block-comment above for more information.` |
|       - |  1366 | ` */` |
|   24914 |  1367 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1368 | `{` |
|       - |  1369 | `	sxi32 rc;` |
|   24919 |  1370 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|   12457 |  1371 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1372 | `	/* Compilation result */` |
|   24919 |  1373 | `	return rc;` |
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
|   23528 |  1417 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   23533 |  1428 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1429 | `	/* Compile the expression*/` |
|   23533 |  1430 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1431 | `	/* Restore token stream */` |
|   23533 |  1432 | `	RE_SWAP_DELIMITER(pGen);` |
|   23533 |  1433 | `	return rc;` |
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
|      19 |  1447 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
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
|   26042 |  1474 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 |  1475 | `{` |
|   26047 |  1476 | `	SyToken *pCur = pStart;` |
|   26047 |  1477 | `	sxi32 iNest = 0;` |
|   73983 |  1478 | `	while( pCur < pEnd ){` |
|   53815 |  1479 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    5875 |  1480 | `			return pCur;` |
|       - |  1481 | `		}` |
|       - |  1482 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1483 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - |  1484 | `		 * not an entry separator. Skip past the signature.` |
|       - |  1485 | `		 */` |
|   47945 |  1486 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|   47939 |  1547 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     413 |  1548 | `			iNest++;` |
|   47734 |  1549 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1550 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - |  1551 | `			 * parser will shortly detect any syntax error. */` |
|     413 |  1552 | `			iNest--;` |
|     205 |  1553 | `		}` |
|   47939 |  1554 | `		pCur++;` |
|       5 |  1555 | `	}` |
|   20173 |  1556 | `	return pEnd;` |
|   13026 |  1557 | `}` |
|       - |  1558 | `/*` |
|       - |  1559 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1560 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1561 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1562 | ` */` |
|   33686 |  1563 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1564 | `{` |
|       - |  1565 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1566 | `	SyToken *pKey,*pCur;` |
|   33691 |  1567 | `	sxi32 iEmitRef = 0;` |
|   33691 |  1568 | `	sxi32 iSpread = 0;` |
|   33691 |  1569 | `	sxi32 nPair = 0;` |
|       - |  1570 | `	sxi32 rc;` |
|   33691 |  1571 | `	xValidator = 0;` |
|   27601 |  1572 | `	for(;;){` |
|       - |  1573 | `		/* Jump leading commas */` |
|   62651 |  1574 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    7449 |  1575 | `			pGen->pIn++;` |
|       5 |  1576 | `		}` |
|   55207 |  1577 | `		pCur = pGen->pIn;` |
|   55207 |  1578 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1579 | `			/* No more entry to process */` |
|   33675 |  1580 | `			break;` |
|       - |  1581 | `		}` |
|   21537 |  1582 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1583 | `			continue;` |
|       - |  1584 | `		}` |
|       - |  1585 | `		/* Compile the key if available */` |
|   21537 |  1586 | `		pKey = pCur;` |
|   21537 |  1587 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   21537 |  1588 | `		rc = SXERR_EMPTY;` |
|   21537 |  1589 | `		if( pCur < pGen->pIn ){` |
|    1763 |  1590 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1591 | `				/* Missing value */` |
|      13 |  1592 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      13 |  1593 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1594 | `					return SXERR_ABORT;` |
|       - |  1595 | `				}` |
|      13 |  1596 | `				return SXRET_OK;` |
|       - |  1597 | `			}` |
|       - |  1598 | `			/* Compile the expression holding the key */` |
|    1753 |  1599 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1600 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1753 |  1601 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1602 | `				return SXERR_ABORT;` |
|       - |  1603 | `			}` |
|    1753 |  1604 | `			pCur++; /* Jump the '=>' operator */` |
|   20653 |  1605 | `		}else if( pKey == pCur ){` |
|       - |  1606 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1607 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1608 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1609 | `		}else{` |
|       - |  1610 | `			/* Reset back the cursor and point to the entry value */` |
|   19779 |  1611 | `			pCur = pKey;` |
|       - |  1612 | `		}` |
|   21527 |  1613 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1614 | `			/* No available key,load NULL */` |
|   19781 |  1615 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    9888 |  1616 | `		}` |
|   21527 |  1617 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   21525 |  1636 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   21525 |  1637 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   21521 |  1650 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   21521 |  1651 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1652 | `			return SXERR_ABORT;` |
|       - |  1653 | `		}` |
|   21521 |  1654 | `		if( iSpread ){` |
|       - |  1655 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      65 |  1656 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   21490 |  1657 | `		}else if( iEmitRef ){` |
|       - |  1658 | `			/* Emit the load reference instruction */` |
|      40 |  1659 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1660 | `		}` |
|   21521 |  1661 | `		xValidator = 0;` |
|   21521 |  1662 | `		iEmitRef = 0;` |
|   21521 |  1663 | `		iSpread = 0;` |
|   21521 |  1664 | `		nPair++;` |
|       5 |  1665 | `	}` |
|       - |  1666 | `	/* Emit the load map instruction */` |
|   33675 |  1667 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1668 | `	/* Node successfully compiled */` |
|   33675 |  1669 | `	return SXRET_OK;` |
|   16848 |  1670 | `}` |
|       - |  1671 | `/*` |
|       - |  1672 | ` * Compile the 'array' language construct.` |
|       - |  1673 | ` *	 According to the PHP language reference manual` |
|       - |  1674 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1675 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1676 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1677 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1678 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1679 | ` */` |
|   32450 |  1680 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1681 | `{` |
|       - |  1682 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   32455 |  1683 | `	pGen->pIn += 2;` |
|   32455 |  1684 | `	pGen->pEnd--;` |
|   16225 |  1685 | `	SXUNUSED(iCompileFlag);` |
|   32455 |  1686 | `	return GenStateCompileArrayBody(pGen);` |
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
|      18 |  1701 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1702 | `{` |
|       - |  1703 | `	SyToken *pIn,*pEnd,*pNext;` |
|      20 |  1704 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|      20 |  1705 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|      20 |  1706 | `	int nArg = 0;` |
|       - |  1707 | `	sxi32 rc;` |
|       9 |  1708 | `	SXUNUSED(iCompileFlag);` |
|       - |  1709 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|      20 |  1710 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|      20 |  1711 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|       - |  1712 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|      20 |  1713 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|     ! 0 |  1714 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  1715 | `			"clone(...) first-class callable form is not yet supported");` |
|       - |  1716 | `	}` |
|       - |  1717 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|      52 |  1718 | `	while( pIn < pEnd ){` |
|      34 |  1719 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|      34 |  1720 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|     ! 0 |  1721 | `			break;` |
|       - |  1722 | `		}` |
|      34 |  1723 | `		pArgStart = pIn;` |
|      34 |  1724 | `		pArgEnd   = pNext;` |
|       - |  1725 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|       - |  1726 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|      35 |  1727 | `		if( (pArgEnd - pArgStart) >= 2` |
|      31 |  1728 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      20 |  1729 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|       5 |  1730 | `			pName = pArgStart;` |
|       5 |  1731 | `			pArgStart += 2;` |
|       2 |  1732 | `		}` |
|      34 |  1733 | `		if( pName ){` |
|       4 |  1734 | `			if( pName->sData.nByte == sizeof("object")-1` |
|       4 |  1735 | `				&& SyStrnicmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|       3 |  1736 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|       4 |  1737 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|       3 |  1738 | `				&& SyStrnicmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|       3 |  1739 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|       2 |  1740 | `			}else{` |
|     ! 0 |  1741 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|     ! 0 |  1742 | `					"Unknown named parameter $%z for clone()",&pName->sData);` |
|       1 |  1743 | `			}` |
|      32 |  1744 | `		}else if( nArg == 0 ){` |
|      18 |  1745 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|      21 |  1746 | `		}else if( nArg == 1 ){` |
|      13 |  1747 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|       7 |  1748 | `		}else{` |
|     ! 0 |  1749 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|       - |  1750 | `				"clone() expects at most 2 arguments");` |
|       - |  1751 | `		}` |
|      34 |  1752 | `		nArg++;` |
|      34 |  1753 | `		pIn = pNext;` |
|      34 |  1754 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  1755 | `			pIn++; /* step over the argument separator */` |
|       7 |  1756 | `		}` |
|       2 |  1757 | `	}` |
|      20 |  1758 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|     ! 0 |  1759 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  1760 | `			"clone() expects at least 1 argument, 0 given");` |
|       - |  1761 | `	}` |
|       - |  1762 | `	/* Object argument -> clone (+ __clone()). */` |
|      20 |  1763 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|      20 |  1764 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1765 | `		return SXERR_ABORT;` |
|       - |  1766 | `	}` |
|      20 |  1767 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|       - |  1768 | `	/* Property updates (evaluated after __clone runs). */` |
|      20 |  1769 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|      15 |  1770 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|      15 |  1771 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1772 | `			return SXERR_ABORT;` |
|       - |  1773 | `		}` |
|      15 |  1774 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|       7 |  1775 | `	}` |
|      20 |  1776 | `	return SXRET_OK;` |
|      11 |  1777 | `}` |
|       - |  1778 | `/*` |
|       - |  1779 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1780 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1781 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1782 | ` */` |
|    1236 |  1783 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1784 | `{` |
|       - |  1785 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    1241 |  1786 | `	pGen->pIn++;` |
|    1241 |  1787 | `	pGen->pEnd--;` |
|     618 |  1788 | `	SXUNUSED(iCompileFlag);` |
|    1241 |  1789 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1790 | `}` |
|       - |  1791 | `/*` |
|       - |  1792 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1793 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1794 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1795 | ` * error message.` |
|       - |  1796 | ` * See the routine responible of compiling the list language construct` |
|       - |  1797 | ` * for more inforation.` |
|       - |  1798 | ` */` |
|     190 |  1799 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  1800 | `{` |
|     195 |  1801 | `	sxi32 rc = SXRET_OK;` |
|     195 |  1802 | `	if( pRoot->pOp ){` |
|       4 |  1803 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|       2 |  1804 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1805 | `				/* Unexpected expression */` |
|     ! 0 |  1806 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1807 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1808 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1809 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1810 | `				}` |
|       1 |  1811 | `		}` |
|     193 |  1812 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1813 | `		/* Unexpected expression */` |
|       6 |  1814 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1815 | `			"list(): Expecting a variable not an expression");` |
|       6 |  1816 | `		if( rc != SXERR_ABORT ){` |
|       6 |  1817 | `			rc = SXERR_INVALID;` |
|       2 |  1818 | `		}` |
|       2 |  1819 | `	}` |
|     195 |  1820 | `	return rc;` |
|       5 |  1821 | `}` |
|       - |  1822 | `/*` |
|       - |  1823 | ` * Compile the 'list' language construct.` |
|       - |  1824 | ` *  According to the PHP language reference` |
|       - |  1825 | ` *  list(): Assign variables as if they were an array.` |
|       - |  1826 | ` *  list() is used to assign a list of variables in one operation.` |
|       - |  1827 | ` *  Description` |
|       - |  1828 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - |  1829 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - |  1830 | ` *   list() is used to assign a list of variables in one operation.` |
|       - |  1831 | ` *  Parameters` |
|       - |  1832 | ` *   $varname: A variable.` |
|       - |  1833 | ` *  Return Values` |
|       - |  1834 | ` *   The assigned array.` |
|       - |  1835 | ` */` |
|       - |  1836 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - |  1837 | `struct NestedListEntry {` |
|       - |  1838 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - |  1839 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - |  1840 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - |  1841 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - |  1842 | `};` |
|       - |  1843 | `/*` |
|       - |  1844 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|       - |  1845 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|       - |  1846 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|       - |  1847 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|       - |  1848 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|       - |  1849 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|       - |  1850 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|       - |  1851 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|       - |  1852 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|       - |  1853 | ` */` |
|      28 |  1854 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|       2 |  1855 | `{` |
|       - |  1856 | `	SyToken *pNext;` |
|       - |  1857 | `	sxi32 rc;` |
|      66 |  1858 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       - |  1859 | `		SyToken *pArrow,*pTarget;` |
|       - |  1860 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|      38 |  1861 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|      38 |  1862 | `		pTarget = &pArrow[1];` |
|      38 |  1863 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|       - |  1864 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|       - |  1865 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|     ! 0 |  1866 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1867 | `				"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1868 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1869 | `		}` |
|       - |  1870 | `		/* DUP the source array (it is on the stack top) */` |
|      38 |  1871 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1872 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|      38 |  1873 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|      38 |  1874 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1875 | `			return SXERR_ABORT;` |
|       - |  1876 | `		}` |
|       - |  1877 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|       - |  1878 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|       - |  1879 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|       - |  1880 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|       - |  1881 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|       - |  1882 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|      38 |  1883 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|      38 |  1884 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|      34 |  1885 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|      18 |  1886 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|       - |  1887 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|       - |  1888 | `			 * Treat source[key] as the inner body's source, then drop the` |
|       - |  1889 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|       5 |  1890 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|       5 |  1891 | `			SyToken *pSavedIn = pGen->pIn;` |
|       5 |  1892 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       5 |  1893 | `			pGen->pIn = pTarget;` |
|       5 |  1894 | `			pGen->pEnd = pNext;` |
|       5 |  1895 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|       2 |  1896 | `			             : PH7_CompileList(&(*pGen),0);` |
|       5 |  1897 | `			pGen->pIn = pSavedIn;` |
|       5 |  1898 | `			pGen->pEnd = pSavedEnd;` |
|       5 |  1899 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1900 | `				return SXERR_ABORT;` |
|       - |  1901 | `			}` |
|       5 |  1902 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       3 |  1903 | `		}else{` |
|       - |  1904 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|       - |  1905 | `			 * is already on the stack as the value; compiling the target appends` |
|       - |  1906 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|       - |  1907 | `			 * assignment does. */` |
|       - |  1908 | `			VmInstr *pInstr;` |
|      34 |  1909 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|      34 |  1910 | `			sxi32 iP1 = 0, iP2 = 0;` |
|      34 |  1911 | `			void *p3 = 0;` |
|      34 |  1912 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|       - |  1913 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      34 |  1914 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  1915 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1916 | `			}` |
|      34 |  1917 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|      34 |  1918 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|       3 |  1919 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|      33 |  1920 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       3 |  1921 | `					iVmOp = PH7_OP_STORE_IDX;` |
|       3 |  1922 | `					iP1 = pInstr->iP1;` |
|       3 |  1923 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       2 |  1924 | `				}else{` |
|      30 |  1925 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|      30 |  1926 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - |  1927 | `				}` |
|      16 |  1928 | `			}` |
|      34 |  1929 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - |  1930 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|       - |  1931 | `			 * source array is back on top for the next entry. */` |
|      34 |  1932 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       - |  1933 | `		}` |
|      38 |  1934 | `		pGen->pIn = &pNext[1];` |
|       2 |  1935 | `	}` |
|      30 |  1936 | `	return SXRET_OK;` |
|      16 |  1937 | `}` |
|       - |  1938 | `/*` |
|       - |  1939 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1940 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1941 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1942 | ` */` |
|     116 |  1943 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       5 |  1944 | `{` |
|       - |  1945 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1946 | `	SyToken *pNext;` |
|       - |  1947 | `	SyToken *pClassifyIn;` |
|     121 |  1948 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|       - |  1949 | `	sxi32 nExpr;` |
|       - |  1950 | `	sxi32 rc;` |
|       - |  1951 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|       - |  1952 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|       - |  1953 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|       - |  1954 | `	 * list. */` |
|     121 |  1955 | `	pClassifyIn = pGen->pIn;` |
|     341 |  1956 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     225 |  1957 | `		if( pGen->pIn >= pNext ){` |
|      13 |  1958 | `			nEmpty++;` |
|     219 |  1959 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|      38 |  1960 | `			nKeyed++;` |
|      20 |  1961 | `		}else{` |
|     177 |  1962 | `			nPositional++;` |
|       - |  1963 | `		}` |
|     225 |  1964 | `		pGen->pIn = &pNext[1];` |
|       5 |  1965 | `	}` |
|     121 |  1966 | `	pGen->pIn = pClassifyIn;` |
|     121 |  1967 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|     ! 0 |  1968 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1969 | `			"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1970 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1971 | `	}` |
|     121 |  1972 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|     ! 0 |  1973 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1974 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|     ! 0 |  1975 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1976 | `	}` |
|     121 |  1977 | `	if( nKeyed > 0 ){` |
|      30 |  1978 | `		return GenStateCompileKeyedListBody(pGen);` |
|       - |  1979 | `	}` |
|      93 |  1980 | `	nExpr = 0;` |
|      93 |  1981 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     277 |  1982 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     189 |  1983 | `		if( pGen->pIn < pNext ){` |
|       - |  1984 | `			/* Check for nested list() */` |
|     177 |  1985 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  1986 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  1987 | `				/* Record this nested list for post-processing */` |
|       3 |  1988 | `				SyToken *pListEnd = 0;` |
|       3 |  1989 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  1990 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  1991 | `				}` |
|       3 |  1992 | `				if( pListEnd ){` |
|       - |  1993 | `					struct NestedListEntry sEntry;` |
|       3 |  1994 | `					sEntry.nIndex = nExpr;` |
|       3 |  1995 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  1996 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  1997 | `					sEntry.isShort = 0;` |
|       3 |  1998 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  1999 | `				}` |
|       - |  2000 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  2001 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     176 |  2002 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  2003 | `				/* Nested short destructuring [...] */` |
|      13 |  2004 | `				SyToken *pBracketEnd = 0;` |
|      13 |  2005 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  2006 | `				if( pBracketEnd ){` |
|       - |  2007 | `					struct NestedListEntry sEntry;` |
|      13 |  2008 | `					sEntry.nIndex = nExpr;` |
|      13 |  2009 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  2010 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  2011 | `					sEntry.isShort = 1;` |
|      13 |  2012 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  2013 | `				}` |
|       - |  2014 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  2015 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  2016 | `			}else{` |
|       - |  2017 | `				/* Compile the expression holding the variable */` |
|     163 |  2018 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     163 |  2019 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  2020 | `					SySetRelease(&sNested);` |
|     ! 0 |  2021 | `					return SXRET_OK;` |
|       - |  2022 | `				}` |
|       - |  2023 | `			}` |
|      91 |  2024 | `		}else{` |
|       - |  2025 | `			/* Empty entry,load NULL */` |
|      13 |  2026 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  2027 | `		}` |
|     189 |  2028 | `		nExpr++;` |
|       - |  2029 | `		/* Advance the stream cursor */` |
|     189 |  2030 | `		pGen->pIn = &pNext[1];` |
|       5 |  2031 | `	}` |
|       - |  2032 | `	/* Emit the LOAD_LIST instruction */` |
|      93 |  2033 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  2034 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  2035 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  2036 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  2037 | `	 */` |
|      93 |  2038 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  2039 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  2040 | `		sxu32 i;` |
|      27 |  2041 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  2042 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  2043 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  2044 | `			ph7_value *pIdx;` |
|       - |  2045 | `			sxu32 nConstIdx;` |
|       - |  2046 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  2047 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  2048 | `			/* Push the integer index for this nested entry */` |
|      15 |  2049 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  2050 | `			if( pIdx == 0 ){` |
|     ! 0 |  2051 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2052 | `				SySetRelease(&sNested);` |
|     ! 0 |  2053 | `				return SXERR_ABORT;` |
|       - |  2054 | `			}` |
|      15 |  2055 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  2056 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  2057 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  2058 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  2059 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  2060 | `			 */` |
|      15 |  2061 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  2062 | `			/* Recursively compile the inner list */` |
|      15 |  2063 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  2064 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  2065 | `			if( apNested[i].isShort ){` |
|      13 |  2066 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  2067 | `			}else{` |
|       3 |  2068 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  2069 | `			}` |
|      15 |  2070 | `			pGen->pIn = pSavedIn;` |
|      15 |  2071 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  2072 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2073 | `				SySetRelease(&sNested);` |
|     ! 0 |  2074 | `				return SXERR_ABORT;` |
|       - |  2075 | `			}` |
|       - |  2076 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  2077 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  2078 | `		}` |
|       6 |  2079 | `	}` |
|      93 |  2080 | `	SySetRelease(&sNested);` |
|       - |  2081 | `	/* Node successfully compiled */` |
|      93 |  2082 | `	return SXRET_OK;` |
|      63 |  2083 | `}` |
|      38 |  2084 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2085 | `{` |
|       - |  2086 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      43 |  2087 | `	pGen->pIn += 2;` |
|      43 |  2088 | `	pGen->pEnd--;` |
|      19 |  2089 | `	SXUNUSED(iCompileFlag);` |
|      43 |  2090 | `	return GenStateCompileListBody(pGen);` |
|       5 |  2091 | `}` |
|      78 |  2092 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2093 | `{` |
|       - |  2094 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      82 |  2095 | `	pGen->pIn++;` |
|      82 |  2096 | `	pGen->pEnd--;` |
|      39 |  2097 | `	SXUNUSED(iCompileFlag);` |
|      82 |  2098 | `	return GenStateCompileListBody(pGen);` |
|       4 |  2099 | `}` |
|       - |  2100 | `/* Forward declarations */` |
|       - |  2101 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  2102 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  2103 | `static int GenStateIsReadonly(SyToken *pTok);` |
|       - |  2104 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|       - |  2105 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|       - |  2106 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  2107 | `/*` |
|       - |  2108 | ` * Compile an annoynmous function or a closure.` |
|       - |  2109 | ` * According to the PHP language reference` |
|       - |  2110 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  2111 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  2112 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  2113 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  2114 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  2115 | ` *  Example Anonymous function variable assignment example` |
|       - |  2116 | ` * <?php` |
|       - |  2117 | ` * $greet = function($name)` |
|       - |  2118 | ` * {` |
|       - |  2119 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  2120 | ` * };` |
|       - |  2121 | ` * $greet('World');` |
|       - |  2122 | ` * $greet('PHP');` |
|       - |  2123 | ` * ?>` |
|       - |  2124 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  2125 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  2126 | ` */` |
|     320 |  2127 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2128 | `{` |
|       - |  2129 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  2130 | `	char zName[512];         /* Unique lambda name */` |
|       - |  2131 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  2132 | `							  * one thread is allowed to compile the script.` |
|       - |  2133 | `						      */` |
|       - |  2134 | `	SyString sName;` |
|       - |  2135 | `	sxu32 nLen;` |
|       - |  2136 | `	sxi32 rc;` |
|     160 |  2137 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2138 |  |
|     325 |  2139 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     325 |  2140 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  2141 | `		pGen->pIn++;` |
|     ! 0 |  2142 | `	}` |
|       - |  2143 | `	/* Generate a unique name */` |
|     325 |  2144 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  2145 | `	/* Make sure the generated name is unique */` |
|     325 |  2146 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  2147 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  2148 | `	}` |
|     325 |  2149 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  2150 | `	/* Compile the lambda body */` |
|     325 |  2151 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     325 |  2152 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2153 | `		return SXERR_ABORT;` |
|       - |  2154 | `	}` |
|       - |  2155 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|       - |  2156 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|       - |  2157 | `	 * the handler wraps either in a Closure instance. */` |
|     325 |  2158 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       - |  2159 | `	/* Node successfully compiled */` |
|     325 |  2160 | `	return SXRET_OK;` |
|     165 |  2161 | `}` |
|       - |  2162 | `/*` |
|       - |  2163 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  2164 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  2165 | ` * enclosing arrow level, or has already been captured.` |
|       - |  2166 | ` */` |
|     186 |  2167 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  2168 | `	ph7_gen_state *pGen,` |
|       - |  2169 | `	ph7_vm_func *pFunc,` |
|       - |  2170 | `	const char *zName,` |
|       - |  2171 | `	sxu32 nByte,` |
|       - |  2172 | `	SyString *aShadow,` |
|       - |  2173 | `	sxu32 nShadow)` |
|       2 |  2174 | `{` |
|       - |  2175 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2176 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  2177 | `	sxu32 n, nEnv;` |
|       - |  2178 | `	char *zDup;` |
|     188 |  2179 | `	if( nByte == 0 ){` |
|     ! 0 |  2180 | `		return SXRET_OK;` |
|       - |  2181 | `	}` |
|     186 |  2182 | `	if( nByte == sizeof("this")-1` |
|     101 |  2183 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  2184 | `		return SXRET_OK;` |
|       - |  2185 | `	}` |
|     234 |  2186 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     174 |  2187 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     167 |  2188 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|     128 |  2189 | `			return SXRET_OK;` |
|       - |  2190 | `		}` |
|      26 |  2191 | `	}` |
|      59 |  2192 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      59 |  2193 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      87 |  2194 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  2195 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  2196 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  2197 | `			return SXRET_OK;` |
|       - |  2198 | `		}` |
|      15 |  2199 | `	}` |
|      59 |  2200 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      59 |  2201 | `	if( zDup == 0 ){` |
|     ! 0 |  2202 | `		return SXERR_ABORT;` |
|       - |  2203 | `	}` |
|      59 |  2204 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      59 |  2205 | `	sEnv.iFlags = 0;` |
|      59 |  2206 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      59 |  2207 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      59 |  2208 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      59 |  2209 | `	return SXRET_OK;` |
|      95 |  2210 | `}` |
|       - |  2211 | `/*` |
|       - |  2212 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  2213 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  2214 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  2215 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  2216 | ` */` |
|      36 |  2217 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  2218 | `	ph7_gen_state *pGen,` |
|       - |  2219 | `	ph7_vm_func *pFunc,` |
|       - |  2220 | `	const char *zIn,` |
|       - |  2221 | `	const char *zEnd,` |
|       - |  2222 | `	SyString *aShadow,` |
|       - |  2223 | `	sxu32 nShadow)` |
|       2 |  2224 | `{` |
|       - |  2225 | `	sxi32 rc;` |
|     302 |  2226 | `	while( zIn < zEnd ){` |
|     266 |  2227 | `		if( zIn[0] == '\\' ){` |
|       5 |  2228 | `			zIn++;` |
|       5 |  2229 | `			if( zIn < zEnd ){` |
|       5 |  2230 | `				zIn++;` |
|       2 |  2231 | `			}` |
|       5 |  2232 | `			continue;` |
|       - |  2233 | `		}` |
|     260 |  2234 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      22 |  2235 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      20 |  2236 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  2237 | `			const char *zName;` |
|      22 |  2238 | `			zIn++; /* skip '$' */` |
|      22 |  2239 | `			zName = zIn;` |
|      74 |  2240 | `			while( zIn < zEnd ){` |
|      70 |  2241 | `				unsigned char c = (unsigned char)zIn[0];` |
|      70 |  2242 | `				if( c >= 0xc0 ){` |
|     ! 0 |  2243 | `					zIn++;` |
|     ! 0 |  2244 | `					while( zIn < zEnd` |
|     ! 0 |  2245 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  2246 | `						zIn++;` |
|     ! 0 |  2247 | `					}` |
|     ! 0 |  2248 | `					continue;` |
|       - |  2249 | `				}` |
|      70 |  2250 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      18 |  2251 | `					break;` |
|       - |  2252 | `				}` |
|      54 |  2253 | `				zIn++;` |
|       2 |  2254 | `			}` |
|      22 |  2255 | `			if( zIn > zName ){` |
|      32 |  2256 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      20 |  2257 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      22 |  2258 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2259 | `					return SXERR_ABORT;` |
|       - |  2260 | `				}` |
|      10 |  2261 | `			}` |
|      22 |  2262 | `			continue;` |
|       - |  2263 | `		}` |
|     242 |  2264 | `		zIn++;` |
|       2 |  2265 | `	}` |
|      38 |  2266 | `	return SXRET_OK;` |
|      20 |  2267 | `}` |
|       - |  2268 | `/*` |
|       - |  2269 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  2270 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  2271 | ` *   - plain $<id> pairs` |
|       - |  2272 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  2273 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  2274 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  2275 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  2276 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  2277 | ` *     are never mistakenly captured.` |
|       - |  2278 | ` */` |
|     200 |  2279 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  2280 | `	ph7_gen_state *pGen,` |
|       - |  2281 | `	ph7_vm_func *pFunc,` |
|       - |  2282 | `	SyToken *pStart,` |
|       - |  2283 | `	SyToken *pEnd,` |
|       - |  2284 | `	SyString *aShadow,` |
|       - |  2285 | `	sxu32 nShadow)` |
|       3 |  2286 | `{` |
|     203 |  2287 | `	SyToken *pScan = pStart;` |
|       - |  2288 | `	sxi32 rc;` |
|     847 |  2289 | `	while( pScan < pEnd ){` |
|     647 |  2290 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      56 |  2291 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|      18 |  2292 | `				pScan->sData.zString,` |
|      36 |  2293 | `				pScan->sData.zString + pScan->sData.nByte,` |
|      18 |  2294 | `				aShadow,nShadow);` |
|      38 |  2295 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2296 | `				return SXERR_ABORT;` |
|       - |  2297 | `			}` |
|      38 |  2298 | `			pScan++;` |
|      38 |  2299 | `			continue;` |
|       - |  2300 | `		}` |
|     611 |  2301 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      24 |  2302 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      24 |  2303 | `			SyToken *pFnKw = pScan;` |
|      22 |  2304 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2305 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       2 |  2306 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2307 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2308 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2309 | `			}` |
|      24 |  2310 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2311 | `				SyToken *pInnerSigStart;` |
|       - |  2312 | `				SyToken *pInnerSigEnd;` |
|       - |  2313 | `				SyToken *pInnerBodyEnd;` |
|       - |  2314 | `				SyString *aInnerShadow;` |
|       - |  2315 | `				sxu32 nInnerShadow;` |
|       - |  2316 | `				sxu32 nInnerParamMax;` |
|       - |  2317 | `				SyToken *p;` |
|       - |  2318 | `				int iNestInner;` |
|      19 |  2319 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2320 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2321 | `					pScan++;` |
|     ! 0 |  2322 | `				}` |
|      19 |  2323 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2324 | `					pScan++;` |
|     ! 0 |  2325 | `					continue;` |
|       - |  2326 | `				}` |
|      19 |  2327 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2328 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2329 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2330 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2331 | `					pScan = pEnd;` |
|     ! 0 |  2332 | `					continue;` |
|       - |  2333 | `				}` |
|       - |  2334 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2335 | `				nInnerParamMax = 0;` |
|      57 |  2336 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2337 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2338 | `						nInnerParamMax++;` |
|       6 |  2339 | `					}` |
|      20 |  2340 | `				}` |
|      19 |  2341 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2342 | `					&pGen->pVm->sAllocator,` |
|      18 |  2343 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2344 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2345 | `					return SXERR_ABORT;` |
|       - |  2346 | `				}` |
|      19 |  2347 | `				nInnerShadow = 0;` |
|      25 |  2348 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2349 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2350 | `				}` |
|      57 |  2351 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2352 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2353 | `						continue;` |
|       - |  2354 | `					}` |
|      13 |  2355 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2356 | `						break;` |
|       - |  2357 | `					}` |
|      13 |  2358 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2359 | `						continue;` |
|       - |  2360 | `					}` |
|      13 |  2361 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2362 | `				}` |
|      19 |  2363 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2364 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2365 | `					pScan++;` |
|     ! 0 |  2366 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2367 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2368 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2369 | `						pScan++;` |
|     ! 0 |  2370 | `					}` |
|     ! 0 |  2371 | `					if( pScan < pEnd` |
|     ! 0 |  2372 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2373 | `						pScan++;` |
|     ! 0 |  2374 | `					}` |
|     ! 0 |  2375 | `				}` |
|      19 |  2376 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2377 | `					pScan++; /* past '=>' */` |
|       9 |  2378 | `				}` |
|      19 |  2379 | `				pInnerBodyEnd = pScan;` |
|      19 |  2380 | `				iNestInner = 0;` |
|     131 |  2381 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2382 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2383 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2384 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2385 | `						break;` |
|       - |  2386 | `					}` |
|     113 |  2387 | `					if( pInnerBodyEnd->nType &` |
|       - |  2388 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2389 | `						iNestInner++;` |
|     112 |  2390 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2391 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2392 | `						iNestInner--;` |
|       1 |  2393 | `					}` |
|     113 |  2394 | `					pInnerBodyEnd++;` |
|       1 |  2395 | `				}` |
|       - |  2396 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2397 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2398 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2399 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2400 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2401 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2402 | `				 *` |
|       - |  2403 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2404 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2405 | `				 * range after the '=' sign. */` |
|       - |  2406 | `				{` |
|      19 |  2407 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2408 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2409 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2410 | `						SyToken *pEq = 0;` |
|      13 |  2411 | `						int iNestArg = 0;` |
|      49 |  2412 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2413 | `							if( iNestArg == 0` |
|      39 |  2414 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2415 | `								break;` |
|       - |  2416 | `							}` |
|      37 |  2417 | `							if( pArgEnd->nType &` |
|       - |  2418 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2419 | `								iNestArg++;` |
|      37 |  2420 | `							}else if( pArgEnd->nType &` |
|       - |  2421 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2422 | `								iNestArg--;` |
|     ! 0 |  2423 | `							}` |
|      36 |  2424 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2425 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2426 | `								pEq = pArgEnd;` |
|       3 |  2427 | `							}` |
|      37 |  2428 | `							pArgEnd++;` |
|       1 |  2429 | `						}` |
|      13 |  2430 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2431 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2432 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2433 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2434 | `								return SXERR_ABORT;` |
|       - |  2435 | `							}` |
|       3 |  2436 | `						}` |
|      13 |  2437 | `						pArgStart = pArgEnd;` |
|      12 |  2438 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2439 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2440 | `							pArgStart++;` |
|       1 |  2441 | `						}` |
|       1 |  2442 | `					}` |
|       - |  2443 | `				}` |
|      28 |  2444 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2445 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2446 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2447 | `					return SXERR_ABORT;` |
|       - |  2448 | `				}` |
|      19 |  2449 | `				pScan = pInnerBodyEnd;` |
|      19 |  2450 | `				continue;` |
|       - |  2451 | `			}` |
|       2 |  2452 | `		}` |
|     593 |  2453 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     427 |  2454 | `			pScan++;` |
|     427 |  2455 | `			continue;` |
|       - |  2456 | `		}` |
|       - |  2457 | `		{` |
|       - |  2458 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     168 |  2459 | `			SyToken *pDollar = pScan;` |
|     249 |  2460 | `			while( &pDollar[1] < pEnd` |
|     168 |  2461 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2462 | `				pDollar++;` |
|     ! 0 |  2463 | `			}` |
|     168 |  2464 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2465 | `				break;` |
|       - |  2466 | `			}` |
|     168 |  2467 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2468 | `				pScan = pDollar + 1;` |
|     ! 0 |  2469 | `				continue;` |
|       - |  2470 | `			}` |
|     251 |  2471 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     166 |  2472 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      83 |  2473 | `				aShadow,nShadow);` |
|     168 |  2474 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2475 | `				return SXERR_ABORT;` |
|       - |  2476 | `			}` |
|     168 |  2477 | `			pScan = pDollar + 2;` |
|       - |  2478 | `		}` |
|       2 |  2479 | `	}` |
|     203 |  2480 | `	return SXRET_OK;` |
|     103 |  2481 | `}` |
|       - |  2482 | `/*` |
|       - |  2483 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2484 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2485 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2486 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2487 | ` * $this is also made available.` |
|       - |  2488 | ` */` |
|     182 |  2489 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2490 | `{` |
|       - |  2491 | `	ph7_vm_func *pFunc;` |
|       - |  2492 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2493 | `	GenBlock *pBlock;` |
|       - |  2494 | `	SySet *pInstrContainer;` |
|       - |  2495 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2496 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2497 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2498 | `	SyToken *pSavedEnd;` |
|       - |  2499 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2500 | `	char zName[512];` |
|       - |  2501 | `	static int iCnt = 1;` |
|       - |  2502 | `	char *zDup;` |
|       - |  2503 | `	sxu32 nLen;` |
|       - |  2504 | `	sxu32 nLine;` |
|     186 |  2505 | `	sxi32 iFlags = 0;` |
|     186 |  2506 | `	int bStatic = 0;` |
|       - |  2507 | `	sxi32 rc;` |
|       - |  2508 | `	sxu32 n;` |
|      91 |  2509 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2510 |  |
|     186 |  2511 | `	nLine = pGen->pIn->nLine;` |
|       - |  2512 | `	/* Optional 'static' prefix */` |
|     182 |  2513 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     186 |  2514 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2515 | `		bStatic = 1;` |
|       3 |  2516 | `		pGen->pIn++;` |
|       1 |  2517 | `	}` |
|       - |  2518 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     182 |  2519 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     186 |  2520 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2521 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2522 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2523 | `		return SXERR_SYNTAX;` |
|       - |  2524 | `	}` |
|     186 |  2525 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2526 | `	/* Optional '&' — return by reference */` |
|     186 |  2527 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2528 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2529 | `		pGen->pIn++;` |
|     ! 0 |  2530 | `	}` |
|       - |  2531 | `	/* Expect '(' */` |
|     186 |  2532 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2533 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2534 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2535 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2536 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2537 | `		}else{` |
|     ! 0 |  2538 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2539 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2540 | `		}` |
|       3 |  2541 | `		return SXERR_SYNTAX;` |
|       - |  2542 | `	}` |
|     183 |  2543 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2544 | `	/* Delimit the parameter list */` |
|     183 |  2545 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     183 |  2546 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2547 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2548 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2549 | `		return SXERR_SYNTAX;` |
|       - |  2550 | `	}` |
|       - |  2551 | `	/* Allocate the function state */` |
|     181 |  2552 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     181 |  2553 | `	if( pFunc == 0 ){` |
|     ! 0 |  2554 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2555 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2556 | `		return SXERR_ABORT;` |
|       - |  2557 | `	}` |
|       - |  2558 | `	/* Generate a unique lambda name */` |
|     181 |  2559 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     279 |  2560 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     100 |  2561 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2562 | `	}` |
|     181 |  2563 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     181 |  2564 | `	if( zDup == 0 ){` |
|     ! 0 |  2565 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2566 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2567 | `		return SXERR_ABORT;` |
|       - |  2568 | `	}` |
|     181 |  2569 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2570 | `	/* Collect function arguments */` |
|     181 |  2571 | `	if( pGen->pIn < pSigEnd ){` |
|     105 |  2572 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|     105 |  2573 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2574 | `			return SXERR_ABORT;` |
|       - |  2575 | `		}` |
|      51 |  2576 | `	}` |
|       - |  2577 | `	/* Point past ')' and parse optional return type */` |
|     181 |  2578 | `	pGen->pIn = &pSigEnd[1];` |
|     181 |  2579 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     181 |  2580 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2581 | `		return SXERR_ABORT;` |
|     181 |  2582 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2583 | `		return SXERR_SYNTAX;` |
|       - |  2584 | `	}` |
|       - |  2585 | `	/* Expect '=>' */` |
|     181 |  2586 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2587 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2588 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2589 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2590 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2591 | `		}else{` |
|     ! 0 |  2592 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2593 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2594 | `		}` |
|       3 |  2595 | `		return SXERR_SYNTAX;` |
|       - |  2596 | `	}` |
|     179 |  2597 | `	pGen->pIn++; /* Jump '=>' */` |
|     179 |  2598 | `	pBodyStart = pGen->pIn;` |
|     179 |  2599 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2600 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2601 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2602 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2603 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     179 |  2604 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2605 | `	{` |
|     179 |  2606 | `		SyString *aShadow = 0;` |
|     179 |  2607 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     179 |  2608 | `		if( nShadow > 0 ){` |
|     102 |  2609 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|     100 |  2610 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|     102 |  2611 | `			if( aShadow == 0 ){` |
|     ! 0 |  2612 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2613 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2614 | `				return SXERR_ABORT;` |
|       - |  2615 | `			}` |
|     228 |  2616 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     128 |  2617 | `				aShadow[n] = aArgs[n].sName;` |
|      65 |  2618 | `			}` |
|      50 |  2619 | `		}` |
|     267 |  2620 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      88 |  2621 | `			aShadow,nShadow);` |
|     179 |  2622 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2623 | `			return SXERR_ABORT;` |
|       - |  2624 | `		}` |
|       - |  2625 | `	}` |
|       - |  2626 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2627 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2628 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2629 | `	 * $this. */` |
|     179 |  2630 | `	if( !bStatic ){` |
|       - |  2631 | `		char *zThisDup;` |
|     177 |  2632 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     177 |  2633 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2634 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2635 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2636 | `			return SXERR_ABORT;` |
|       - |  2637 | `		}` |
|     177 |  2638 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     177 |  2639 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     177 |  2640 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     177 |  2641 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     177 |  2642 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      87 |  2643 | `	}` |
|       - |  2644 | `	/* Arrow functions are always closures */` |
|     179 |  2645 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2646 | `	/* Compile the body expression as an implicit return */` |
|     267 |  2647 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      88 |  2648 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     179 |  2649 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2650 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2651 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2652 | `		return SXERR_ABORT;` |
|       - |  2653 | `	}` |
|     179 |  2654 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     179 |  2655 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     179 |  2656 | `	pSavedEnd = pGen->pEnd;` |
|     179 |  2657 | `	pGen->pIn = pBodyStart;` |
|     179 |  2658 | `	pGen->pEnd = pBodyEnd;` |
|     179 |  2659 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     179 |  2660 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2661 | `		return SXERR_ABORT;` |
|       - |  2662 | `	}` |
|       - |  2663 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2664 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2665 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2666 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     179 |  2667 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     179 |  2668 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     179 |  2669 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     179 |  2670 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     179 |  2671 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2672 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     179 |  2673 | `	pGen->pIn = pBodyEnd;` |
|     179 |  2674 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2675 | `	/* Emit the load-closure instruction */` |
|     179 |  2676 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     179 |  2677 | `	return SXRET_OK;` |
|      95 |  2678 | `}` |
|       - |  2679 | `/*` |
|       - |  2680 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2681 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2682 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2683 | ` * expression's value.` |
|       - |  2684 | ` */` |
|     346 |  2685 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2686 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  2687 | `{` |
|       - |  2688 | `	SySet *pInstrContainer;` |
|       - |  2689 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2690 | `	GenBlock *pArmBlock;` |
|       - |  2691 | `	sxi32 rc;` |
|     349 |  2692 | `	pTmpIn  = pGen->pIn;` |
|     349 |  2693 | `	pTmpEnd = pGen->pEnd;` |
|     349 |  2694 | `	pGen->pIn  = pStart;` |
|     349 |  2695 | `	pGen->pEnd = pStop;` |
|     349 |  2696 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     349 |  2697 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2698 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2699 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2700 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2701 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2702 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     522 |  2703 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2704 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     349 |  2705 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2706 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2707 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2708 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2709 | `		return SXERR_ABORT;` |
|       - |  2710 | `	}` |
|     349 |  2711 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     349 |  2712 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     349 |  2713 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     349 |  2714 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     349 |  2715 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     349 |  2716 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     349 |  2717 | `	pGen->pIn  = pTmpIn;` |
|     349 |  2718 | `	pGen->pEnd = pTmpEnd;` |
|     349 |  2719 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2720 | `		return SXERR_ABORT;` |
|       - |  2721 | `	}` |
|     349 |  2722 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2723 | `		return SXERR_EMPTY;` |
|       - |  2724 | `	}` |
|     349 |  2725 | `	return SXRET_OK;` |
|     176 |  2726 | `}` |
|       - |  2727 | `/*` |
|       - |  2728 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2729 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2730 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2731 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2732 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2733 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2734 | ` */` |
|       - |  2735 | `/*` |
|       - |  2736 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2737 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2738 | ` * caller can bail out of the current expression.` |
|       - |  2739 | ` */` |
|       2 |  2740 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2741 | `{` |
|       - |  2742 | `	va_list ap;` |
|       - |  2743 | `	sxi32 rc;` |
|       - |  2744 | `	SyBlob sMsg;` |
|       3 |  2745 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2746 | `	va_start(ap,zFmt);` |
|       3 |  2747 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2748 | `	va_end(ap);` |
|       3 |  2749 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2750 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2751 | `	SyBlobRelease(&sMsg);` |
|       3 |  2752 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2753 | `		return SXERR_ABORT;` |
|       - |  2754 | `	}` |
|       3 |  2755 | `	return SXERR_SYNTAX;` |
|       2 |  2756 | `}` |
|       - |  2757 | `/*` |
|       - |  2758 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2759 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2760 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2761 | ` */` |
|     348 |  2762 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  2763 | `{` |
|     352 |  2764 | `	SyToken *pCur = pStart;` |
|     352 |  2765 | `	int iNest = 0;` |
|     814 |  2766 | `	while( pCur < pEnd ){` |
|     780 |  2767 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2768 | `			iNest++;` |
|     774 |  2769 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2770 | `			iNest--;` |
|     762 |  2771 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     317 |  2772 | `			return pCur;` |
|       - |  2773 | `		}` |
|     466 |  2774 | `		pCur++;` |
|       4 |  2775 | `	}` |
|      37 |  2776 | `	return pEnd;` |
|     178 |  2777 | `}` |
|      70 |  2778 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2779 | `{` |
|       - |  2780 | `	ph7_match *pMatch;` |
|       - |  2781 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      75 |  2782 | `	int bHasDefault = 0;` |
|       - |  2783 | `	sxu32 nLine;` |
|       - |  2784 | `	sxi32 rc;` |
|      35 |  2785 | `	SXUNUSED(iCompileFlag);` |
|      75 |  2786 | `	nLine = pGen->pIn->nLine;` |
|      75 |  2787 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2788 | `	/* Expect '(' */` |
|      75 |  2789 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2790 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2791 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2792 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2793 | `	}` |
|      75 |  2794 | `	pGen->pIn++; /* Jump '(' */` |
|      75 |  2795 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      75 |  2796 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2797 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2798 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2799 | `	}` |
|      75 |  2800 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2801 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2802 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2803 | `	}` |
|       - |  2804 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      75 |  2805 | `	pSavedEnd = pGen->pEnd;` |
|      75 |  2806 | `	pGen->pEnd = pSubjEnd;` |
|      75 |  2807 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      75 |  2808 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2809 | `		return SXERR_ABORT;` |
|       - |  2810 | `	}` |
|      75 |  2811 | `	pGen->pEnd = pSavedEnd;` |
|      75 |  2812 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2813 | `	/* Expect '{' */` |
|      75 |  2814 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2815 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2816 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2817 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2818 | `	}` |
|      75 |  2819 | `	pGen->pIn++; /* Jump '{' */` |
|      75 |  2820 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      75 |  2821 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2822 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2823 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2824 | `	}` |
|       - |  2825 | `	/* Allocate ph7_match container */` |
|      75 |  2826 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      75 |  2827 | `	if( pMatch == 0 ){` |
|     ! 0 |  2828 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2829 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2830 | `		return SXERR_ABORT;` |
|       - |  2831 | `	}` |
|      75 |  2832 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      75 |  2833 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2834 | `	/* Iterate arms */` |
|     253 |  2835 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2836 | `		ph7_match_arm sArm;` |
|       - |  2837 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     186 |  2838 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     186 |  2839 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     186 |  2840 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     186 |  2841 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2842 | `		/* 'default' arm? */` |
|     182 |  2843 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 |  2844 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2845 | `			if( bHasDefault ){` |
|       3 |  2846 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2847 | `					"Match expressions may only contain one default arm");` |
|       4 |  2848 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2849 | `			}` |
|      20 |  2850 | `			sArm.bDefault = 1;` |
|      20 |  2851 | `			bHasDefault = 1;` |
|      20 |  2852 | `			pGen->pIn++;` |
|      20 |  2853 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2854 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2855 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2856 | `			}` |
|      20 |  2857 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2858 | `		}else{` |
|       - |  2859 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     166 |  2860 | `			pCondStart = pGen->pIn;` |
|     166 |  2861 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2862 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     174 |  2863 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2864 | `				SySet sCondBc;` |
|       9 |  2865 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2866 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2867 | `						"syntax error, empty match condition expression");` |
|       - |  2868 | `				}` |
|       9 |  2869 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2870 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2871 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2872 | `					return SXERR_ABORT;` |
|       - |  2873 | `				}` |
|       9 |  2874 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2875 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2876 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2877 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2878 | `			}` |
|     166 |  2879 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2880 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2881 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2882 | `			}` |
|     163 |  2883 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2884 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2885 | `					"syntax error, empty match condition expression");` |
|       - |  2886 | `			}` |
|       - |  2887 | `			{` |
|       - |  2888 | `				SySet sCondBc;` |
|     163 |  2889 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     163 |  2890 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     163 |  2891 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2892 | `					return SXERR_ABORT;` |
|       - |  2893 | `				}` |
|     163 |  2894 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2895 | `			}` |
|     163 |  2896 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2897 | `		}` |
|       - |  2898 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     181 |  2899 | `		pResStart = pGen->pIn;` |
|     181 |  2900 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     181 |  2901 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2902 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2903 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2904 | `		}` |
|     181 |  2905 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     181 |  2906 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2907 | `			return SXERR_ABORT;` |
|       - |  2908 | `		}` |
|     181 |  2909 | `		pGen->pIn = pResEnd;` |
|     181 |  2910 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     149 |  2911 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2912 | `		}` |
|     181 |  2913 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       3 |  2914 | `	}` |
|      69 |  2915 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      69 |  2916 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      69 |  2917 | `	return SXRET_OK;` |
|      40 |  2918 | `}` |
|       - |  2919 | `/*` |
|       - |  2920 | ` * Compile a backtick quoted string.` |
|       - |  2921 | ` */` |
|       4 |  2922 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2923 | `{` |
|       - |  2924 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2925 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2926 | `	 */` |
|       8 |  2927 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2928 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2929 | `		ph7_lib_version()` |
|       - |  2930 | `		);` |
|       - |  2931 | `	/* Load NULL */` |
|       6 |  2932 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2933 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2934 | `	/* Node successfully compiled */` |
|       6 |  2935 | `	return SXRET_OK;` |
|       2 |  2936 | `}` |
|       - |  2937 | `/*` |
|       - |  2938 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2939 | ` * construct.` |
|       - |  2940 | ` */` |
|      82 |  2941 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2942 | `{` |
|       - |  2943 | `	SyString *pName;` |
|       - |  2944 | `	sxu32 nKeyID;` |
|       - |  2945 | `	sxi32 rc;` |
|       - |  2946 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      87 |  2947 | `	pName = &pGen->pIn->sData;` |
|      87 |  2948 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      87 |  2949 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      87 |  2950 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2951 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2952 | `		/* Compile arguments one after one */` |
|       9 |  2953 | `		pTmp = pGen->pEnd;` |
|       - |  2954 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2955 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2956 | `		 *  mean that the following expression is valid:` |
|       - |  2957 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2958 | `		 */` |
|       9 |  2959 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2960 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2961 | `			if( pGen->pIn < pNext ){` |
|       9 |  2962 | `				pGen->pEnd = pNext;` |
|       9 |  2963 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2964 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2965 | `					return SXERR_ABORT;` |
|       - |  2966 | `				}` |
|       9 |  2967 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2968 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2969 | `					 * without the overhead of a function call.` |
|       - |  2970 | `					 * This is a very powerful optimization that improve` |
|       - |  2971 | `					 * performance greatly.` |
|       - |  2972 | `					 */` |
|       9 |  2973 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2974 | `				}` |
|       4 |  2975 | `			}` |
|       - |  2976 | `			/* Jump trailing commas */` |
|       9 |  2977 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2978 | `				pNext++;` |
|     ! 0 |  2979 | `			}` |
|       9 |  2980 | `			pGen->pIn = pNext;` |
|       1 |  2981 | `		}` |
|       - |  2982 | `		/* Restore token stream */` |
|       9 |  2983 | `		pGen->pEnd = pTmp;` |
|       5 |  2984 | `	}else{` |
|      79 |  2985 | `		sxi32 nArg = 0;` |
|      79 |  2986 | `		sxu32 nIdx = 0;` |
|      79 |  2987 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      79 |  2988 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2989 | `			return SXERR_ABORT;` |
|      79 |  2990 | `		}else if(rc != SXERR_EMPTY ){` |
|      79 |  2991 | `			nArg = 1;` |
|      37 |  2992 | `		}` |
|      79 |  2993 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2994 | `			ph7_value *pObj;` |
|       - |  2995 | `			/* Emit the call instruction */` |
|      31 |  2996 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      31 |  2997 | `			if( pObj == 0 ){` |
|     ! 0 |  2998 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2999 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3000 | `				return SXERR_ABORT;` |
|       - |  3001 | `			}` |
|      31 |  3002 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  3003 | `			/* Install in the literal table */` |
|      31 |  3004 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      13 |  3005 | `		}` |
|       - |  3006 | `		/* Emit the call instruction */` |
|      79 |  3007 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      79 |  3008 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  3009 | `	}` |
|       - |  3010 | `	/* Node successfully compiled */` |
|      87 |  3011 | `	return SXRET_OK;` |
|      46 |  3012 | `}` |
|       - |  3013 | `/*` |
|       - |  3014 | ` * Compile a node holding a variable declaration.` |
|       - |  3015 | ` * According to the PHP language reference` |
|       - |  3016 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  3017 | ` *  The variable name is case-sensitive.` |
|       - |  3018 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  3019 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3020 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  3021 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  3022 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  3023 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  3024 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  3025 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  3026 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  3027 | ` *  the chapter on Expressions.` |
|       - |  3028 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  3029 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  3030 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  3031 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  3032 | ` *  is being assigned (the source variable).` |
|       - |  3033 | ` */` |
| 1223896 |  3034 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3035 | `{` |
| 1223901 |  3036 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3037 | `	sxi32 iVv;` |
|       - |  3038 | `	sxi32 iP1;` |
|       - |  3039 | `	void *p3;` |
|       - |  3040 | `	sxi32 rc;` |
| 1223901 |  3041 | `	iVv = -1; /* Variable variable counter */` |
| 2447809 |  3042 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1223913 |  3043 | `		pGen->pIn++;` |
| 1223913 |  3044 | `		iVv++;` |
|       5 |  3045 | `	}` |
| 1223901 |  3046 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  3047 | `		/* Invalid variable name */` |
|     ! 0 |  3048 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  3049 | `		if( rc == SXERR_ABORT ){` |
|       - |  3050 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3051 | `			return SXERR_ABORT;` |
|       - |  3052 | `		}` |
|     ! 0 |  3053 | `		return SXRET_OK;` |
|       - |  3054 | `	}` |
| 1223901 |  3055 | `	p3  = 0;` |
| 1223901 |  3056 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  3057 | `		/* Dynamic variable creation */` |
|      19 |  3058 | `		pGen->pIn++;  /* Jump the open curly */` |
|      19 |  3059 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      19 |  3060 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3061 | `			/* Empty expression */` |
|       3 |  3062 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  3063 | `			return SXRET_OK;` |
|       - |  3064 | `		}` |
|       - |  3065 | `		/* Compile the expression holding the variable name */` |
|      16 |  3066 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  3067 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3068 | `			return SXERR_ABORT;` |
|      16 |  3069 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  3070 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  3071 | `			return SXRET_OK;` |
|       - |  3072 | `		}` |
|       7 |  3073 | `	}else{` |
|       - |  3074 | `		SyHashEntry *pEntry;` |
|       - |  3075 | `		SyString *pName;` |
| 1223885 |  3076 | `		char *zName = 0;` |
|       - |  3077 | `		/* Extract variable name */` |
| 1223885 |  3078 | `		pName = &pGen->pIn->sData;` |
|       - |  3079 | `		/* Advance the stream cursor */` |
| 1223885 |  3080 | `		pGen->pIn++;` |
| 1223885 |  3081 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1223885 |  3082 | `		if( pEntry == 0 ){` |
|       - |  3083 | `			/* Duplicate name */` |
|  176331 |  3084 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  176331 |  3085 | `			if( zName == 0 ){` |
|     ! 0 |  3086 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3087 | `				return SXERR_ABORT;` |
|       - |  3088 | `			}` |
|       - |  3089 | `			/* Install in the hashtable */` |
|  176331 |  3090 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   88168 |  3091 | `		}else{` |
|       - |  3092 | `			/* Name already available */` |
| 1047559 |  3093 | `			zName = (char *)pEntry->pUserData;` |
|       - |  3094 | `		}` |
| 1223885 |  3095 | `		p3 = (void *)zName;` |
|       - |  3096 | `	}` |
| 1223897 |  3097 | `	iP1 = 0;` |
| 1223897 |  3098 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  476987 |  3099 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  3100 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  476969 |  3101 | `			iP1 = 1;` |
|  238482 |  3102 | `		}` |
|  238491 |  3103 | `	}` |
|       - |  3104 | `	/* Emit the load instruction */` |
| 1223897 |  3105 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1223909 |  3106 | `	while( iVv > 0 ){` |
|      13 |  3107 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  3108 | `		iVv--;` |
|       1 |  3109 | `	}` |
|       - |  3110 | `	/* Node successfully compiled */` |
| 1223897 |  3111 | `	return SXRET_OK;` |
|  611953 |  3112 | `}` |
|       - |  3113 | `/*` |
|       - |  3114 | ` * Load a literal.` |
|       - |  3115 | ` */` |
|  844736 |  3116 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  3117 | `{` |
|  844741 |  3118 | `	SyToken *pToken = pGen->pIn;` |
|       - |  3119 | `	ph7_value *pObj;` |
|       - |  3120 | `	SyString *pStr;` |
|       - |  3121 | `	sxu32 nIdx;` |
|       - |  3122 | `	/* Extract token value */` |
|  844741 |  3123 | `	pStr = &pToken->sData;` |
|       - |  3124 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  844741 |  3125 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  179043 |  3126 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  3127 | `			/* NULL constant are always indexed at 0 */` |
|   65809 |  3128 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   65809 |  3129 | `			return SXRET_OK;` |
|  113239 |  3130 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  3131 | `			/* TRUE constant are always indexed at 1 */` |
|     837 |  3132 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     837 |  3133 | `			return SXRET_OK;` |
|       5 |  3134 | `		}` |
|  779054 |  3135 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  114300 |  3136 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  3137 | `			/* FALSE constant are always indexed at 2 */` |
|   50353 |  3138 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   50353 |  3139 | `			return SXRET_OK;` |
|  675304 |  3140 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  119898 |  3141 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  3142 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|   11495 |  3143 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   11495 |  3144 | `			if( pObj == 0 ){` |
|     ! 0 |  3145 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3146 | `				return SXERR_ABORT;` |
|       - |  3147 | `			}` |
|   11495 |  3148 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  3149 | `			/* Emit the load constant instruction */` |
|   11495 |  3150 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   11495 |  3151 | `			return SXRET_OK;` |
|  623225 |  3152 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   38720 |  3153 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  3154 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 |  3155 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 |  3156 | `			if( pObj == 0 ){` |
|     ! 0 |  3157 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3158 | `				return SXERR_ABORT;` |
|       - |  3159 | `			}` |
|       7 |  3160 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  3161 | `				SyString sNs;` |
|       7 |  3162 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 |  3163 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 |  3164 | `			}else{` |
|     ! 0 |  3165 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  3166 | `			}` |
|       7 |  3167 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 |  3168 | `			return SXRET_OK;` |
|  622593 |  3169 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   16221 |  3170 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  614478 |  3171 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   21262 |  3172 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  3173 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  3174 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  3175 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  3176 | `				/* Point to the upper block */` |
|      11 |  3177 | `				pBlock = pBlock->pParent;` |
|       1 |  3178 | `			}` |
|      11 |  3179 | `			if( pBlock == 0 ){` |
|       - |  3180 | `				/* Called in the global scope,load NULL */` |
|       5 |  3181 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  3182 | `			}else{` |
|       - |  3183 | `				/* Extract the target function/method */` |
|       7 |  3184 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  3185 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  3186 | `					/* Not a class method,Load null */` |
|       3 |  3187 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  3188 | `				}else{` |
|       5 |  3189 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  3190 | `					if( pObj == 0 ){` |
|     ! 0 |  3191 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3192 | `						return SXERR_ABORT;` |
|       - |  3193 | `					}` |
|       5 |  3194 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  3195 | `					/* Emit the load constant instruction */` |
|       5 |  3196 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  3197 | `				}` |
|       - |  3198 | `			}` |
|      11 |  3199 | `			return SXRET_OK;` |
|       - |  3200 | `	}` |
|       - |  3201 | `	/* Query literal table */` |
|  716251 |  3202 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  3203 | `		ph7_value *pLitObj;` |
|       - |  3204 | `		/* Unknown literal,install it in the literal table */` |
|  305259 |  3205 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  305259 |  3206 | `		if( pLitObj == 0 ){` |
|     ! 0 |  3207 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3208 | `			return SXERR_ABORT;` |
|       - |  3209 | `		}` |
|  305259 |  3210 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  305259 |  3211 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  152627 |  3212 | `	}` |
|       - |  3213 | `	/* Emit the load constant instruction */` |
|  716251 |  3214 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  716251 |  3215 | `	return SXRET_OK;` |
|  422373 |  3216 | `}` |
|       - |  3217 | `/*` |
|       - |  3218 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  3219 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  3220 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  3221 | ` * Otherwise, load the simple literal directly.` |
|       - |  3222 | ` */` |
|  848612 |  3223 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  3224 | `{` |
|       - |  3225 | `	sxi32 rc;` |
|  848617 |  3226 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3227 | `		return SXRET_OK;` |
|       - |  3228 | `	}` |
|       - |  3229 | `	/* Check if this is a multi-token namespace path */` |
|  848617 |  3230 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  3231 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|    3881 |  3232 | `		SyBlob *pWorker = &pGen->sWorker;` |
|    3881 |  3233 | `		int isAbsolute = 0;` |
|    3881 |  3234 | `		SyBlobReset(pWorker);` |
|       - |  3235 | `		/* Check for leading backslash (absolute path) */` |
|    3881 |  3236 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|    3879 |  3237 | `			isAbsolute = 1;` |
|    3879 |  3238 | `			pGen->pIn++; /* Skip leading backslash */` |
|    1937 |  3239 | `		}` |
|       - |  3240 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|    3881 |  3241 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  3242 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  3243 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  3244 | `		}` |
|       - |  3245 | `		/* Collect all path components */` |
|    3989 |  3246 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    3989 |  3247 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      59 |  3248 | `				SyBlobAppend(pWorker,"\\",1);` |
|      32 |  3249 | `			}else{` |
|    3935 |  3250 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  3251 | `			}` |
|    3989 |  3252 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|    3881 |  3253 | `				pGen->pIn++;` |
|    3881 |  3254 | `				break;` |
|       - |  3255 | `			}` |
|     113 |  3256 | `			pGen->pIn++;` |
|       5 |  3257 | `		}` |
|    3881 |  3258 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  3259 | `			ph7_value *pObj;` |
|       - |  3260 | `			SyString sPath;` |
|       - |  3261 | `			sxu32 nIdx;` |
|    3881 |  3262 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  3263 | `			/* Install in the literal table */` |
|    3881 |  3264 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|    3853 |  3265 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    3853 |  3266 | `				if( pObj == 0 ){` |
|     ! 0 |  3267 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3268 | `					return SXERR_ABORT;` |
|       - |  3269 | `				}` |
|    3853 |  3270 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|    3853 |  3271 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|    1924 |  3272 | `			}` |
|       - |  3273 | `			/* Emit the load constant instruction.` |
|       - |  3274 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  3275 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|    5819 |  3276 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|    1938 |  3277 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|    1938 |  3278 | `				nIdx,0,0);` |
|    3881 |  3279 | `			return SXRET_OK;` |
|       - |  3280 | `		}` |
|     ! 0 |  3281 | `	}` |
|       - |  3282 | `	/* Single-token literal: load directly */` |
|  844741 |  3283 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  844741 |  3284 | `	return rc;` |
|  424311 |  3285 | `}` |
|       - |  3286 | `/*` |
|       - |  3287 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  3288 | ` */` |
|       - |  3289 | `/*` |
|       - |  3290 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|       - |  3291 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|       - |  3292 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|       - |  3293 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|       - |  3294 | ` */` |
|     ! 0 |  3295 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|     ! 0 |  3296 | `{` |
|     ! 0 |  3297 | `	SXUNUSED(iCompileFlag);` |
|     ! 0 |  3298 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|       - |  3299 | `		"Cannot use the first-class callable syntax '...' here");` |
|     ! 0 |  3300 | `	return SXERR_SYNTAX;` |
|     ! 0 |  3301 | `}` |
|  848612 |  3302 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3303 | `{` |
|       - |  3304 | `	sxi32 rc;` |
|  848617 |  3305 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  848617 |  3306 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3307 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3308 | `		return rc;` |
|       - |  3309 | `	}` |
|       - |  3310 | `	/* Node successfully compiled */` |
|  848617 |  3311 | `	return SXRET_OK;` |
|  424311 |  3312 | `}` |
|       - |  3313 | `/*` |
|       - |  3314 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3315 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3316 | ` */` |
|       8 |  3317 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3318 | `{` |
|       - |  3319 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3320 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3321 | `		pGen->pIn++;` |
|       1 |  3322 | `	}` |
|       9 |  3323 | `	return SXRET_OK;` |
|       1 |  3324 | `}` |
|       - |  3325 | `/*` |
|       - |  3326 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3327 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3328 | ` */` |
|     134 |  3329 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3330 | `{` |
|     139 |  3331 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      34 |  3332 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3333 | `			return TRUE;` |
|      32 |  3334 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       6 |  3335 | `			return TRUE;` |
|       3 |  3336 | `		}` |
|     121 |  3337 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3338 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3339 | `			return TRUE;` |
|       - |  3340 | `		}` |
|     ! 0 |  3341 | `	}` |
|       - |  3342 | `	/* Not a reserved constant */` |
|     131 |  3343 | `	return FALSE;` |
|      72 |  3344 | `}` |
|       - |  3345 | `/*` |
|       - |  3346 | ` * Compile the 'const' statement.` |
|       - |  3347 | ` * According to the PHP language reference` |
|       - |  3348 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3349 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3350 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3351 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3352 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3353 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3354 | ` *  Syntax` |
|       - |  3355 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3356 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3357 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3358 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3359 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3360 | ` *  to get a list of all defined constants.` |
|       - |  3361 | ` *` |
|       - |  3362 | ` * Symisc eXtension.` |
|       - |  3363 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3364 | ` *  would allow only simple scalar value.` |
|       - |  3365 | ` *  Example` |
|       - |  3366 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3367 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3368 | ` */` |
|      38 |  3369 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3370 | `{` |
|       - |  3371 | `	SySet *pConsCode,*pInstrContainer;` |
|      43 |  3372 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3373 | `	SyString *pName;` |
|       - |  3374 | `	sxi32 rc;` |
|      43 |  3375 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      43 |  3376 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3377 | `		/* Invalid constant name */` |
|       9 |  3378 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       9 |  3379 | `		if( rc == SXERR_ABORT ){` |
|       - |  3380 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3381 | `			return SXERR_ABORT;` |
|       - |  3382 | `		}` |
|       9 |  3383 | `		goto Synchronize;` |
|       - |  3384 | `	}` |
|       - |  3385 | `	/* Peek constant name */` |
|      37 |  3386 | `	pName = &pGen->pIn->sData;` |
|       - |  3387 | `	/* Make sure the constant name isn't reserved */` |
|      37 |  3388 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3389 | `		/* Reserved constant */` |
|      10 |  3390 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|      10 |  3391 | `		if( rc == SXERR_ABORT ){` |
|       - |  3392 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3393 | `			return SXERR_ABORT;` |
|       - |  3394 | `		}` |
|      10 |  3395 | `		goto Synchronize;` |
|       - |  3396 | `	}` |
|      28 |  3397 | `	pGen->pIn++;` |
|      28 |  3398 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3399 | `		/* Invalid statement*/` |
|       6 |  3400 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3401 | `		if( rc == SXERR_ABORT ){` |
|       - |  3402 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3403 | `			return SXERR_ABORT;` |
|       - |  3404 | `		}` |
|       6 |  3405 | `		goto Synchronize;` |
|       - |  3406 | `	}` |
|      22 |  3407 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3408 | `	/* Allocate a new constant value container */` |
|      22 |  3409 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      22 |  3410 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3411 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3412 | `		return SXERR_ABORT;` |
|       - |  3413 | `	}` |
|      22 |  3414 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3415 | `	/* Swap bytecode container */` |
|      22 |  3416 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      22 |  3417 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3418 | `	/* Compile constant value */` |
|      22 |  3419 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3420 | `	/* Emit the done instruction */` |
|      22 |  3421 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      22 |  3422 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      22 |  3423 | `	if( rc == SXERR_ABORT ){` |
|       - |  3424 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3425 | `		return SXERR_ABORT;` |
|       - |  3426 | `	}` |
|      22 |  3427 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3428 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3429 | `	{` |
|       - |  3430 | `		SyBlob sFQN;` |
|       - |  3431 | `		SyString sFQNStr;` |
|      22 |  3432 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      22 |  3433 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      22 |  3434 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      22 |  3435 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      22 |  3436 | `		SyBlobRelease(&sFQN);` |
|       - |  3437 | `	}` |
|      22 |  3438 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3439 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3440 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3441 | `	}` |
|      22 |  3442 | `	return SXRET_OK;` |
|       9 |  3443 | `Synchronize:` |
|       - |  3444 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3445 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      41 |  3446 | `		pGen->pIn++;` |
|       3 |  3447 | `	}` |
|      22 |  3448 | `	return SXRET_OK;` |
|      24 |  3449 | `}` |
|       - |  3450 | `/*` |
|       - |  3451 | ` * Compile the 'continue' statement.` |
|       - |  3452 | ` * According to the PHP language reference` |
|       - |  3453 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3454 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3455 | ` *  iteration.` |
|       - |  3456 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3457 | ` *  the purposes of continue.` |
|       - |  3458 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3459 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3460 | ` *  Note:` |
|       - |  3461 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3462 | ` */` |
|       - |  3463 | `/*` |
|       - |  3464 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3465 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3466 | ` * break/continue crosses a try boundary.` |
|       - |  3467 | ` *` |
|       - |  3468 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3469 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3470 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3471 | ` */` |
|    3974 |  3472 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3473 | `{` |
|    3979 |  3474 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    3979 |  3475 | `	int nInlineTry = 0;` |
|   23347 |  3476 | `	while( pBlock && pBlock != pTarget ){` |
|   19373 |  3477 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       6 |  3478 | `			if( pBlock->pUserData ){` |
|       - |  3479 | `				/* A try block with an exception context. In a generator its catch/finally` |
|       - |  3480 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|       - |  3481 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|       - |  3482 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|       6 |  3483 | `				if( pGen->bInGenerator ){` |
|       3 |  3484 | `					nInlineTry++;` |
|       2 |  3485 | `				}else{` |
|       3 |  3486 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       - |  3487 | `				}` |
|       4 |  3488 | `			}else{` |
|       - |  3489 | `				/* A catch/finally block compiled into a separate bytecode container` |
|       - |  3490 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|     ! 0 |  3491 | `				break;` |
|       - |  3492 | `			}` |
|       2 |  3493 | `		}` |
|   19373 |  3494 | `		pBlock = pBlock->pParent;` |
|       5 |  3495 | `	}` |
|    3979 |  3496 | `	return nInlineTry;` |
|       5 |  3497 | `}` |
|    3876 |  3498 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3499 | `{` |
|       - |  3500 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3501 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3502 | `	sxu32 nLineLocal;` |
|       - |  3503 | `	sxi32 rc;` |
|    3881 |  3504 | `	nLineLocal = pGen->pIn->nLine;` |
|    3881 |  3505 | `	iLevel = 0;` |
|       - |  3506 | `	/* Jump the 'continue' keyword */` |
|    3881 |  3507 | `	pGen->pIn++;` |
|    3881 |  3508 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3509 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3510 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3511 | `		 */` |
|       - |  3512 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3513 | `		char *zAlloc = 0;` |
|       - |  3514 | `		SyString sNum;` |
|      17 |  3515 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3516 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3517 | `			return SXERR_ABORT;` |
|       - |  3518 | `		}` |
|      17 |  3519 | `		if( rc == SXRET_OK ){` |
|      20 |  3520 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3521 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3522 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3523 | `				return SXERR_ABORT;` |
|       - |  3524 | `			}` |
|      14 |  3525 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3526 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3527 | `		}` |
|      17 |  3528 | `		if( iLevel < 2 ){` |
|       3 |  3529 | `			iLevel = 0;` |
|       1 |  3530 | `		}` |
|      17 |  3531 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3532 | `	}` |
|       - |  3533 | `	/* Point to the target loop */` |
|    3881 |  3534 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3881 |  3535 | `	if( pLoop == 0 ){` |
|       - |  3536 | `		/* Illegal continue */` |
|      13 |  3537 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      13 |  3538 | `		if( rc == SXERR_ABORT ){` |
|       - |  3539 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3540 | `			return SXERR_ABORT;` |
|       - |  3541 | `		}` |
|       8 |  3542 | `	}else{` |
|    3871 |  3543 | `		sxu32 nInstrIdx = 0;` |
|       - |  3544 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    3871 |  3545 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|       - |  3546 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|       - |  3547 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    3871 |  3548 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    3871 |  3549 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3550 | `			/* According to the PHP language reference manual` |
|       - |  3551 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3552 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3553 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3554 | `			 */` |
|       5 |  3555 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|       5 |  3556 | `			if( rc == SXRET_OK ){` |
|       5 |  3557 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3558 | `			}` |
|       3 |  3559 | `		}else{` |
|       - |  3560 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3867 |  3561 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3867 |  3562 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3563 | `				JumpFixup sJumpFix;` |
|       - |  3564 | `				/* Post-continue */` |
|      14 |  3565 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3566 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3567 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3568 | `			}` |
|       - |  3569 | `		}` |
|       - |  3570 | `	}` |
|    3881 |  3571 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3572 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3573 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3574 | `	}` |
|       - |  3575 | `	/* Statement successfully compiled */` |
|    3881 |  3576 | `	return SXRET_OK;` |
|    1943 |  3577 | `}` |
|       - |  3578 | `/*` |
|       - |  3579 | ` * Compile the 'break' statement.` |
|       - |  3580 | ` * According to the PHP language reference` |
|       - |  3581 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3582 | ` *  structure.` |
|       - |  3583 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3584 | ` *  enclosing structures are to be broken out of.` |
|       - |  3585 | ` */` |
|     124 |  3586 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3587 | `{` |
|       - |  3588 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3589 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3590 | `	sxi32 rc;` |
|     129 |  3591 | `	iLevel = 0;` |
|       - |  3592 | `	/* Jump the 'break' keyword */` |
|     129 |  3593 | `	pGen->pIn++;` |
|     129 |  3594 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3595 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3596 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3597 | `		 */` |
|       - |  3598 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3599 | `		char *zAlloc = 0;` |
|       - |  3600 | `		SyString sNum;` |
|      17 |  3601 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3602 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3603 | `			return SXERR_ABORT;` |
|       - |  3604 | `		}` |
|      17 |  3605 | `		if( rc == SXRET_OK ){` |
|      21 |  3606 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3607 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      15 |  3608 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3609 | `				return SXERR_ABORT;` |
|       - |  3610 | `			}` |
|      15 |  3611 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      15 |  3612 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3613 | `		}` |
|      17 |  3614 | `		if( iLevel < 2 ){` |
|       3 |  3615 | `			iLevel = 0;` |
|       1 |  3616 | `		}` |
|      17 |  3617 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3618 | `	}` |
|       - |  3619 | `	/* Extract the target loop */` |
|     129 |  3620 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     129 |  3621 | `	if( pLoop == 0 ){` |
|       - |  3622 | `		/* Illegal break */` |
|      19 |  3623 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      19 |  3624 | `		if( rc == SXERR_ABORT ){` |
|       - |  3625 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3626 | `			return SXERR_ABORT;` |
|       - |  3627 | `		}` |
|      11 |  3628 | `	}else{` |
|       - |  3629 | `		sxu32 nInstrIdx;` |
|       - |  3630 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     113 |  3631 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|       - |  3632 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     113 |  3633 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     113 |  3634 | `		if( rc == SXRET_OK ){` |
|       - |  3635 | `			/* Fix the jump later when the jump destination is resolved */` |
|     113 |  3636 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      54 |  3637 | `		}` |
|       - |  3638 | `	}` |
|     129 |  3639 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3640 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3641 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3642 | `	}` |
|       - |  3643 | `	/* Statement successfully compiled */` |
|     129 |  3644 | `	return SXRET_OK;` |
|      67 |  3645 | `}` |
|       - |  3646 | `/*` |
|       - |  3647 | ` * Compile or record a label.` |
|       - |  3648 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3649 | ` * Example` |
|       - |  3650 | ` *  goto LABEL;` |
|       - |  3651 | ` *   echo 'Foo';` |
|       - |  3652 | ` *  LABEL:` |
|       - |  3653 | ` *   echo 'Bar';` |
|       - |  3654 | ` */` |
|     112 |  3655 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  3656 | `{` |
|       - |  3657 | `	GenBlock *pBlock;` |
|       - |  3658 | `	Label sLabel;` |
|       - |  3659 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     117 |  3660 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     117 |  3661 | `	if( pBlock ){` |
|       - |  3662 | `		sxi32 rc;` |
|       8 |  3663 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3664 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       6 |  3665 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3666 | `			return SXERR_ABORT;` |
|       - |  3667 | `		}` |
|       4 |  3668 | `	}else{` |
|     113 |  3669 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3670 | `		char *zDup;` |
|       - |  3671 | `		/* Initialize label fields */` |
|     113 |  3672 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3673 | `		/* Duplicate label name */` |
|     113 |  3674 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     113 |  3675 | `		if( zDup == 0 ){` |
|     ! 0 |  3676 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3677 | `			return SXERR_ABORT;` |
|       - |  3678 | `		}` |
|     113 |  3679 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     113 |  3680 | `		sLabel.bRef  = FALSE;` |
|     113 |  3681 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     113 |  3682 | `		pBlock = pGen->pCurrent;` |
|     221 |  3683 | `		while( pBlock ){` |
|     133 |  3684 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      24 |  3685 | `				break;` |
|       - |  3686 | `			}` |
|       - |  3687 | `			/* Point to the upper block */` |
|     113 |  3688 | `			pBlock = pBlock->pParent;` |
|       5 |  3689 | `		}` |
|     113 |  3690 | `		if( pBlock ){` |
|      24 |  3691 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      14 |  3692 | `		}else{` |
|      93 |  3693 | `			sLabel.pFunc = 0;` |
|       - |  3694 | `		}` |
|       - |  3695 | `		/* Insert in label set */` |
|     113 |  3696 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3697 | `	}` |
|     117 |  3698 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     117 |  3699 | `	return SXRET_OK;` |
|      61 |  3700 | `}` |
|       - |  3701 | `/*` |
|       - |  3702 | ` * Compile the so hated 'goto' statement.` |
|       - |  3703 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3704 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3705 | ` * a compiler it has to do this.` |
|       - |  3706 | ` * According to the PHP language reference manual` |
|       - |  3707 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3708 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3709 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3710 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3711 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3712 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3713 | ` *   of a multi-level break` |
|       - |  3714 | ` */` |
|     152 |  3715 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  3716 | `{` |
|       - |  3717 | `	JumpFixup sJump;` |
|       - |  3718 | `	sxi32 rc;` |
|     157 |  3719 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     157 |  3720 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3721 | `		/* Missing label */` |
|     ! 0 |  3722 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3723 | `		if( rc == SXERR_ABORT ){` |
|       - |  3724 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3725 | `			return SXERR_ABORT;` |
|       - |  3726 | `		}` |
|     ! 0 |  3727 | `		return SXRET_OK;` |
|       - |  3728 | `	}` |
|     157 |  3729 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       6 |  3730 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       6 |  3731 | `		if( rc == SXERR_ABORT ){` |
|       - |  3732 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3733 | `			return SXERR_ABORT;` |
|       - |  3734 | `		}` |
|       4 |  3735 | `	}else{` |
|     153 |  3736 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3737 | `		GenBlock *pBlock;` |
|       - |  3738 | `		char *zDup;` |
|       - |  3739 | `		/* Prepare the jump destination */` |
|     153 |  3740 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     153 |  3741 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3742 | `		/* Duplicate label name */` |
|     153 |  3743 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     153 |  3744 | `		if( zDup == 0 ){` |
|     ! 0 |  3745 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3746 | `			return SXERR_ABORT;` |
|       - |  3747 | `		}` |
|     153 |  3748 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     153 |  3749 | `		pBlock = pGen->pCurrent;` |
|     315 |  3750 | `		while( pBlock ){` |
|     199 |  3751 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      37 |  3752 | `				break;` |
|       - |  3753 | `			}` |
|       - |  3754 | `			/* Point to the upper block */` |
|     167 |  3755 | `			pBlock = pBlock->pParent;` |
|       5 |  3756 | `		}` |
|     153 |  3757 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       9 |  3758 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       9 |  3759 | `			if( rc == SXERR_ABORT ){` |
|       - |  3760 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3761 | `				return SXERR_ABORT;` |
|       - |  3762 | `			}` |
|       3 |  3763 | `		}` |
|     153 |  3764 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      30 |  3765 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      17 |  3766 | `		}else{` |
|     127 |  3767 | `			sJump.pFunc = 0;` |
|       - |  3768 | `		}` |
|       - |  3769 | `		/* Emit the unconditional jump */` |
|     153 |  3770 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     153 |  3771 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3772 | `		}` |
|       - |  3773 | `	}` |
|     157 |  3774 | `	pGen->pIn++; /* Jump the label name */` |
|     157 |  3775 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3776 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3777 | `	}` |
|       - |  3778 | `	/* Statement successfully compiled */` |
|     157 |  3779 | `	return SXRET_OK;` |
|      81 |  3780 | `}` |
|       - |  3781 | `/*` |
|       - |  3782 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3783 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3784 | ` * failure.` |
|       - |  3785 | ` */` |
|      20 |  3786 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       2 |  3787 | `{` |
|       - |  3788 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3789 | `	sxu32 nRawObj;` |
|      10 |  3790 | `	sxu32 nObjIdx;` |
|       - |  3791 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3792 | `	 * a PHP block.` |
|       - |  3793 | `	 */` |
|      10 |  3794 | `Consume:` |
|      22 |  3795 | `	nRawObj = nObjIdx = 0;` |
|      22 |  3796 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3797 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3798 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3799 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3800 | `			return SXERR_ABORT;` |
|       - |  3801 | `		}` |
|       - |  3802 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3803 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3804 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3805 | `		++nRawObj;` |
|     ! 0 |  3806 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3807 | `	}` |
|      22 |  3808 | `	if( nRawObj > 0 ){` |
|       - |  3809 | `		/* Emit the consume instruction */` |
|     ! 0 |  3810 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3811 | `	}` |
|      22 |  3812 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3813 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3814 | `		/* Reset the token set */` |
|     ! 0 |  3815 | `		SySetReset(pTokenSet);` |
|       - |  3816 | `		/* Tokenize input */` |
|     ! 0 |  3817 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3818 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3819 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3820 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3821 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3822 | `		/* Advance the stream cursor */` |
|     ! 0 |  3823 | `		pGen->pRawIn++;` |
|       - |  3824 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3825 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3826 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3827 | `			sxi32 rc;` |
|       - |  3828 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3829 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3830 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3831 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3832 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3833 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3834 | `				return SXERR_ABORT;` |
|     ! 0 |  3835 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3836 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3837 | `			}` |
|     ! 0 |  3838 | `			goto Consume;` |
|       - |  3839 | `		}` |
|     ! 0 |  3840 | `	}else{` |
|       - |  3841 | `		/* No more chunks to process */` |
|      22 |  3842 | `		pGen->pIn = pGen->pEnd;` |
|      22 |  3843 | `		return SXERR_EOF;` |
|       - |  3844 | `	}` |
|     ! 0 |  3845 | `	return SXRET_OK;` |
|      12 |  3846 | `}` |
|       - |  3847 | `/*` |
|       - |  3848 | ` * Compile a PHP block.` |
|       - |  3849 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3850 | ` * optionally delimited by braces {}.` |
|       - |  3851 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3852 | ` * and this function takes care of generating the appropriate error` |
|       - |  3853 | ` * message.` |
|       - |  3854 | ` */` |
|  468680 |  3855 | `static sxi32 PH7_CompileBlock(` |
|       - |  3856 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3857 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3858 | `	)` |
|       5 |  3859 | `{` |
|       - |  3860 | `	sxi32 rc;` |
|       - |  3861 | `	sxu32 nLine;` |
|  468685 |  3862 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  467217 |  3863 | `		nLine = pGen->pIn->nLine;` |
|  467217 |  3864 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  467217 |  3865 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3866 | `			return SXERR_ABORT;` |
|       - |  3867 | `		}` |
|  467217 |  3868 | `		pGen->pIn++;` |
|       - |  3869 | `		/* Compile until we hit the closing braces '}' */` |
|  638474 |  3870 | `		for(;;){` |
| 1276953 |  3871 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      22 |  3872 | `				rc = GenStateNextChunk(&(*pGen));` |
|      22 |  3873 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3874 | `			 	   return SXERR_ABORT;` |
|       - |  3875 | `				}` |
|      22 |  3876 | `				if( rc == SXERR_EOF ){` |
|       - |  3877 | `					/* No more token to process. Missing closing braces */` |
|      22 |  3878 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      22 |  3879 | `					break;` |
|       - |  3880 | `				}` |
|     ! 0 |  3881 | `			}` |
| 1276933 |  3882 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3883 | `				/* Closing braces found,break immediately*/` |
|  467197 |  3884 | `				pGen->pIn++;` |
|  467197 |  3885 | `				break;` |
|       - |  3886 | `			}` |
|       - |  3887 | `			/* Compile a single statement */` |
|  809741 |  3888 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  809741 |  3889 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3890 | `				return SXERR_ABORT;` |
|       - |  3891 | `			}` |
|       5 |  3892 | `		}` |
|  467217 |  3893 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  235079 |  3894 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3895 | `		pGen->pIn++;` |
|     ! 0 |  3896 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3897 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3898 | `			return SXERR_ABORT;` |
|       - |  3899 | `		}` |
|       - |  3900 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3901 | `		for(;;){` |
|     ! 0 |  3902 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3903 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3904 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3905 | `			 	   return SXERR_ABORT;` |
|       - |  3906 | `				}` |
|     ! 0 |  3907 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3908 | `					/* No more token to process */` |
|     ! 0 |  3909 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3910 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3911 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3912 | `					}` |
|     ! 0 |  3913 | `					break;` |
|       - |  3914 | `				}` |
|     ! 0 |  3915 | `			}` |
|     ! 0 |  3916 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3917 | `				sxi32 nKwrd;` |
|       - |  3918 | `				/* Keyword found */` |
|     ! 0 |  3919 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3920 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3921 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3922 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3923 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3924 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3925 | `						}` |
|     ! 0 |  3926 | `						break;` |
|       - |  3927 | `				}` |
|     ! 0 |  3928 | `			}` |
|       - |  3929 | `			/* Compile a single statement */` |
|     ! 0 |  3930 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3931 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3932 | `				return SXERR_ABORT;` |
|       - |  3933 | `			}` |
|     ! 0 |  3934 | `		}` |
|     ! 0 |  3935 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3936 | `	}else{` |
|       - |  3937 | `		/* Compile a single statement */` |
|    1473 |  3938 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1473 |  3939 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3940 | `			return SXERR_ABORT;` |
|       - |  3941 | `		}` |
|       - |  3942 | `	}` |
|       - |  3943 | `	/* Jump trailing semi-colons ';' */` |
|  468685 |  3944 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3945 | `		pGen->pIn++;` |
|     ! 0 |  3946 | `	}` |
|  468685 |  3947 | `	return SXRET_OK;` |
|  234345 |  3948 | `}` |
|       - |  3949 | `/*` |
|       - |  3950 | ` * Compile the gentle 'while' statement.` |
|       - |  3951 | ` * According to the PHP language reference` |
|       - |  3952 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3953 | ` *  The basic form of a while statement is:` |
|       - |  3954 | ` *  while (expr)` |
|       - |  3955 | ` *   statement` |
|       - |  3956 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3957 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3958 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3959 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3960 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3961 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3962 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3963 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3964 | ` *  while (expr):` |
|       - |  3965 | ` *    statement` |
|       - |  3966 | ` *   endwhile;` |
|       - |  3967 | ` */` |
|   15444 |  3968 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3969 | `{` |
|   15449 |  3970 | `	GenBlock *pWhileBlock = 0;` |
|   15449 |  3971 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3972 | `	sxu32 nFalseJump;` |
|       - |  3973 | `	sxu32 nLine;` |
|       - |  3974 | `	sxi32 rc;` |
|   15449 |  3975 | `	nLine = pGen->pIn->nLine;` |
|       - |  3976 | `	/* Jump the 'while' keyword */` |
|   15449 |  3977 | `	pGen->pIn++;` |
|   15449 |  3978 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3979 | `		/* Syntax error */` |
|     ! 0 |  3980 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3981 | `		if( rc == SXERR_ABORT ){` |
|       - |  3982 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3983 | `			return SXERR_ABORT;` |
|       - |  3984 | `		}` |
|     ! 0 |  3985 | `		goto Synchronize;` |
|       - |  3986 | `	}` |
|       - |  3987 | `	/* Jump the left parenthesis '(' */` |
|   15449 |  3988 | `	pGen->pIn++;` |
|       - |  3989 | `	/* Create the loop block */` |
|   15449 |  3990 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   15449 |  3991 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3992 | `		return SXERR_ABORT;` |
|       - |  3993 | `	}` |
|       - |  3994 | `	/* Delimit the condition */` |
|   15449 |  3995 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   15449 |  3996 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3997 | `		/* Empty expression */` |
|       3 |  3998 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3999 | `		if( rc == SXERR_ABORT ){` |
|       - |  4000 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4001 | `			return SXERR_ABORT;` |
|       - |  4002 | `		}` |
|       1 |  4003 | `	}` |
|       - |  4004 | `	/* Swap token streams */` |
|   15449 |  4005 | `	pTmp = pGen->pEnd;` |
|   15449 |  4006 | `	pGen->pEnd = pEnd;` |
|       - |  4007 | `	/* Compile the expression */` |
|   15449 |  4008 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   15449 |  4009 | `	if( rc == SXERR_ABORT ){` |
|       - |  4010 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4011 | `		return SXERR_ABORT;` |
|       - |  4012 | `	}` |
|       - |  4013 | `	/* Update token stream */` |
|   15449 |  4014 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  4015 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4016 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4017 | `			return SXERR_ABORT;` |
|       - |  4018 | `		}` |
|     ! 0 |  4019 | `		pGen->pIn++;` |
|     ! 0 |  4020 | `	}` |
|       - |  4021 | `	/* Synchronize pointers */` |
|   15449 |  4022 | `	pGen->pIn  = &pEnd[1];` |
|   15449 |  4023 | `	pGen->pEnd = pTmp;` |
|       - |  4024 | `	/* Emit the false jump */` |
|   15449 |  4025 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4026 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   15449 |  4027 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  4028 | `	/* Compile the loop body */` |
|   15449 |  4029 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   15449 |  4030 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4031 | `		return SXERR_ABORT;` |
|       - |  4032 | `	}` |
|       - |  4033 | `	/* Emit the unconditional jump to the start of the loop */` |
|   15449 |  4034 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  4035 | `	/* Fix all jumps now the destination is resolved */` |
|   15449 |  4036 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4037 | `	/* Release the loop block */` |
|   15449 |  4038 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4039 | `	/* Statement successfully compiled */` |
|   15449 |  4040 | `	return SXRET_OK;` |
|     ! 0 |  4041 | `Synchronize:` |
|       - |  4042 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4043 | `	 * compiling this erroneous block.` |
|       - |  4044 | `	 */` |
|     ! 0 |  4045 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4046 | `		pGen->pIn++;` |
|     ! 0 |  4047 | `	}` |
|     ! 0 |  4048 | `	return SXRET_OK;` |
|    7727 |  4049 | `}` |
|       - |  4050 | `/*` |
|       - |  4051 | ` * Compile the ugly do..while() statement.` |
|       - |  4052 | ` * According to the PHP language reference` |
|       - |  4053 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  4054 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  4055 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  4056 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  4057 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  4058 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  4059 | ` *  would end immediately).` |
|       - |  4060 | ` *  There is just one syntax for do-while loops:` |
|       - |  4061 | ` *  <?php` |
|       - |  4062 | ` *  $i = 0;` |
|       - |  4063 | ` *  do {` |
|       - |  4064 | ` *   echo $i;` |
|       - |  4065 | ` *  } while ($i > 0);` |
|       - |  4066 | ` * ?>` |
|       - |  4067 | ` */` |
|       2 |  4068 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  4069 | `{` |
|       3 |  4070 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  4071 | `	GenBlock *pDoBlock = 0;` |
|       - |  4072 | `	sxu32 nLine;` |
|       - |  4073 | `	sxi32 rc;` |
|       3 |  4074 | `	nLine = pGen->pIn->nLine;` |
|       - |  4075 | `	/* Jump the 'do' keyword */` |
|       3 |  4076 | `	pGen->pIn++;` |
|       - |  4077 | `	/* Create the loop block */` |
|       3 |  4078 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  4079 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4080 | `		return SXERR_ABORT;` |
|       - |  4081 | `	}` |
|       - |  4082 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  4083 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  4084 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  4085 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4086 | `		return SXERR_ABORT;` |
|       - |  4087 | `	}` |
|       3 |  4088 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4089 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  4090 | `	}` |
|       3 |  4091 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  4092 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  4093 | `			/* Missing 'while' statement */` |
|       3 |  4094 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  4095 | `			if( rc == SXERR_ABORT ){` |
|       - |  4096 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4097 | `				return SXERR_ABORT;` |
|       - |  4098 | `			}` |
|       3 |  4099 | `			goto Synchronize;` |
|       - |  4100 | `	}` |
|       - |  4101 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  4102 | `	pGen->pIn++;` |
|     ! 0 |  4103 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4104 | `		/* Syntax error */` |
|     ! 0 |  4105 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  4106 | `		if( rc == SXERR_ABORT ){` |
|       - |  4107 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4108 | `			return SXERR_ABORT;` |
|       - |  4109 | `		}` |
|     ! 0 |  4110 | `		goto Synchronize;` |
|       - |  4111 | `	}` |
|       - |  4112 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  4113 | `	pGen->pIn++;` |
|       - |  4114 | `	/* Delimit the condition */` |
|     ! 0 |  4115 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  4116 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4117 | `		/* Empty expression */` |
|     ! 0 |  4118 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  4119 | `		if( rc == SXERR_ABORT ){` |
|       - |  4120 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4121 | `			return SXERR_ABORT;` |
|       - |  4122 | `		}` |
|     ! 0 |  4123 | `		goto Synchronize;` |
|       - |  4124 | `	}` |
|       - |  4125 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  4126 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  4127 | `		JumpFixup *aPost;` |
|       - |  4128 | `		VmInstr *pInstr;` |
|       - |  4129 | `		sxu32 nJumpDest;` |
|       - |  4130 | `		sxu32 n;` |
|     ! 0 |  4131 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  4132 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  4133 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  4134 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  4135 | `			if( pInstr ){` |
|       - |  4136 | `				/* Fix */` |
|     ! 0 |  4137 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  4138 | `			}` |
|     ! 0 |  4139 | `		}` |
|     ! 0 |  4140 | `	}` |
|       - |  4141 | `	/* Swap token streams */` |
|     ! 0 |  4142 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  4143 | `	pGen->pEnd = pEnd;` |
|       - |  4144 | `	/* Compile the expression */` |
|     ! 0 |  4145 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4146 | `	if( rc == SXERR_ABORT ){` |
|       - |  4147 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4148 | `		return SXERR_ABORT;` |
|       - |  4149 | `	}` |
|       - |  4150 | `	/* Update token stream */` |
|     ! 0 |  4151 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  4152 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4153 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4154 | `			return SXERR_ABORT;` |
|       - |  4155 | `		}` |
|     ! 0 |  4156 | `		pGen->pIn++;` |
|     ! 0 |  4157 | `	}` |
|     ! 0 |  4158 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  4159 | `	pGen->pEnd = pTmp;` |
|       - |  4160 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  4161 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  4162 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  4163 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4164 | `	/* Release the loop block */` |
|     ! 0 |  4165 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4166 | `	/* Statement successfully compiled */` |
|     ! 0 |  4167 | `	return SXRET_OK;` |
|       1 |  4168 | `Synchronize:` |
|       - |  4169 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4170 | `	 * compiling this erroneous block.` |
|       - |  4171 | `	 */` |
|       3 |  4172 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4173 | `		pGen->pIn++;` |
|     ! 0 |  4174 | `	}` |
|       3 |  4175 | `	return SXRET_OK;` |
|       2 |  4176 | `}` |
|       - |  4177 | `/*` |
|       - |  4178 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  4179 | ` * According to the PHP language reference` |
|       - |  4180 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  4181 | ` *  The syntax of a for loop is:` |
|       - |  4182 | ` *  for (expr1; expr2; expr3)` |
|       - |  4183 | ` *   statement` |
|       - |  4184 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  4185 | ` *  the beginning of the loop.` |
|       - |  4186 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  4187 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  4188 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  4189 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  4190 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  4191 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  4192 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  4193 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  4194 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  4195 | ` *  of using the for truth expression.` |
|       - |  4196 | ` */` |
|   15444 |  4197 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  4198 | `{` |
|   15449 |  4199 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   15449 |  4200 | `	GenBlock *pForBlock = 0;` |
|       - |  4201 | `	sxu32 nFalseJump;` |
|       - |  4202 | `	sxu32 nLine;` |
|       - |  4203 | `	sxi32 rc;` |
|   15449 |  4204 | `	nLine = pGen->pIn->nLine;` |
|       - |  4205 | `	/* Jump the 'for' keyword */` |
|   15449 |  4206 | `	pGen->pIn++;` |
|   15449 |  4207 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4208 | `		/* Syntax error */` |
|     ! 0 |  4209 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  4210 | `		if( rc == SXERR_ABORT ){` |
|       - |  4211 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4212 | `			return SXERR_ABORT;` |
|       - |  4213 | `		}` |
|     ! 0 |  4214 | `		return SXRET_OK;` |
|       - |  4215 | `	}` |
|       - |  4216 | `	/* Jump the left parenthesis '(' */` |
|   15449 |  4217 | `	pGen->pIn++;` |
|       - |  4218 | `	/* Delimit the init-expr;condition;post-expr */` |
|   15449 |  4219 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   15449 |  4220 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4221 | `		/* Empty expression */` |
|     ! 0 |  4222 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  4223 | `		if( rc == SXERR_ABORT ){` |
|       - |  4224 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4225 | `			return SXERR_ABORT;` |
|       - |  4226 | `		}` |
|       - |  4227 | `		/* Synchronize */` |
|     ! 0 |  4228 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4229 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4230 | `			pGen->pIn++;` |
|     ! 0 |  4231 | `		}` |
|     ! 0 |  4232 | `		return SXRET_OK;` |
|       - |  4233 | `	}` |
|       - |  4234 | `	/* Swap token streams */` |
|   15449 |  4235 | `	pTmp = pGen->pEnd;` |
|   15449 |  4236 | `	pGen->pEnd = pEnd;` |
|       - |  4237 | `	/* Compile initialization expressions if available */` |
|   15449 |  4238 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4239 | `	/* Pop operand lvalues */` |
|   15449 |  4240 | `	if( rc == SXERR_ABORT ){` |
|       - |  4241 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4242 | `		return SXERR_ABORT;` |
|   15449 |  4243 | `	}else if( rc != SXERR_EMPTY ){` |
|   15447 |  4244 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7721 |  4245 | `	}` |
|   15449 |  4246 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4247 | `		/* Syntax error */` |
|     ! 0 |  4248 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4249 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  4250 | `		if( rc == SXERR_ABORT ){` |
|       - |  4251 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4252 | `			return SXERR_ABORT;` |
|       - |  4253 | `		}` |
|     ! 0 |  4254 | `		return SXRET_OK;` |
|       - |  4255 | `	}` |
|       - |  4256 | `	/* Jump the trailing ';' */` |
|   15449 |  4257 | `	pGen->pIn++;` |
|       - |  4258 | `	/* Create the loop block */` |
|   15449 |  4259 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   15449 |  4260 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4261 | `		return SXERR_ABORT;` |
|       - |  4262 | `	}` |
|       - |  4263 | `	/* Deffer continue jumps */` |
|   15449 |  4264 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  4265 | `	/* Compile the condition */` |
|   15449 |  4266 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   15449 |  4267 | `	if( rc == SXERR_ABORT ){` |
|       - |  4268 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4269 | `		return SXERR_ABORT;` |
|   15449 |  4270 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  4271 | `		/* Emit the false jump */` |
|   15447 |  4272 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4273 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   15447 |  4274 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    7721 |  4275 | `	}` |
|   15449 |  4276 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4277 | `		/* Syntax error */` |
|       6 |  4278 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4279 | `			"for: Expected ';' after conditionals expressions");` |
|       6 |  4280 | `		if( rc == SXERR_ABORT ){` |
|       - |  4281 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4282 | `			return SXERR_ABORT;` |
|       - |  4283 | `		}` |
|       6 |  4284 | `		return SXRET_OK;` |
|       - |  4285 | `	}` |
|       - |  4286 | `	/* Jump the trailing ';' */` |
|   15445 |  4287 | `	pGen->pIn++;` |
|       - |  4288 | `	/* Save the post condition stream */` |
|   15445 |  4289 | `	pPostStart = pGen->pIn;` |
|       - |  4290 | `	/* Compile the loop body */` |
|   15445 |  4291 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   15445 |  4292 | `	pGen->pEnd = pTmp;` |
|   15445 |  4293 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   15445 |  4294 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4295 | `		return SXERR_ABORT;` |
|       - |  4296 | `	}` |
|       - |  4297 | `	/* Fix post-continue jumps */` |
|   15445 |  4298 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  4299 | `		JumpFixup *aPost;` |
|       - |  4300 | `		VmInstr *pInstr;` |
|       - |  4301 | `		sxu32 nJumpDest;` |
|       - |  4302 | `		sxu32 n;` |
|      14 |  4303 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  4304 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  4305 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  4306 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  4307 | `			if( pInstr ){` |
|       - |  4308 | `				/* Fix jump */` |
|      14 |  4309 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  4310 | `			}` |
|       8 |  4311 | `		}` |
|       6 |  4312 | `	}` |
|       - |  4313 | `	/* compile the post-expressions if available */` |
|   15445 |  4314 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4315 | `		pPostStart++;` |
|     ! 0 |  4316 | `	}` |
|   15445 |  4317 | `	if( pPostStart < pEnd ){` |
|       - |  4318 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   15445 |  4319 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   15445 |  4320 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   15445 |  4321 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4322 | `			/* Syntax error */` |
|     ! 0 |  4323 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4324 | `			if( rc == SXERR_ABORT ){` |
|       - |  4325 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4326 | `				return SXERR_ABORT;` |
|       - |  4327 | `			}` |
|     ! 0 |  4328 | `			return SXRET_OK;` |
|       - |  4329 | `		}` |
|   15445 |  4330 | `		RE_SWAP_DELIMITER(pGen);` |
|   15445 |  4331 | `		if( rc == SXERR_ABORT ){` |
|       - |  4332 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4333 | `			return SXERR_ABORT;` |
|   15445 |  4334 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4335 | `			/* Pop operand lvalue */` |
|   15445 |  4336 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7720 |  4337 | `		}` |
|    7720 |  4338 | `	}` |
|       - |  4339 | `	/* Emit the unconditional jump to the start of the loop */` |
|   15445 |  4340 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4341 | `	/* Fix all jumps now the destination is resolved */` |
|   15445 |  4342 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4343 | `	/* Release the loop block */` |
|   15445 |  4344 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4345 | `	/* Statement successfully compiled */` |
|   15445 |  4346 | `	return SXRET_OK;` |
|    7727 |  4347 | `}` |
|       - |  4348 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4349 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4350 | ` * are allowed.` |
|       - |  4351 | ` */` |
|    8286 |  4352 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4353 | `{` |
|    8291 |  4354 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    8291 |  4355 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4356 | `		/* Unexpected expression */` |
|     ! 0 |  4357 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4358 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4359 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4360 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4361 | `		}` |
|     ! 0 |  4362 | `	}` |
|    8291 |  4363 | `	return rc;` |
|       5 |  4364 | `}` |
|       - |  4365 | `/*` |
|       - |  4366 | ` * Compile the 'foreach' statement.` |
|       - |  4367 | ` * According to the PHP language reference` |
|       - |  4368 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4369 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4370 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4371 | ` *  is a minor but useful extension of the first:` |
|       - |  4372 | ` *  foreach (array_expression as $value)` |
|       - |  4373 | ` *    statement` |
|       - |  4374 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4375 | ` *   statement` |
|       - |  4376 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4377 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4378 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4379 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4380 | ` *  to the variable $key on each loop.` |
|       - |  4381 | ` *  Note:` |
|       - |  4382 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4383 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4384 | ` *  Note:` |
|       - |  4385 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4386 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4387 | ` *  or after the foreach without resetting it.` |
|       - |  4388 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4389 | ` *  of copying the value.` |
|       - |  4390 | ` */` |
|    4266 |  4391 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4392 | `{` |
|    4271 |  4393 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    4271 |  4394 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    4271 |  4395 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4396 | `	ph7_foreach_info *pInfo;` |
|       - |  4397 | `	sxu32 nFalseJump;` |
|       - |  4398 | `	VmInstr *pInstr;` |
|       - |  4399 | `	sxu32 nLine;` |
|       - |  4400 | `	sxi32 rc;` |
|    4271 |  4401 | `	nLine = pGen->pIn->nLine;` |
|       - |  4402 | `	/* Jump the 'foreach' keyword */` |
|    4271 |  4403 | `	pGen->pIn++;` |
|    4271 |  4404 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4405 | `		/* Syntax error */` |
|     ! 0 |  4406 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4407 | `		if( rc == SXERR_ABORT ){` |
|       - |  4408 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4409 | `			return SXERR_ABORT;` |
|       - |  4410 | `		}` |
|     ! 0 |  4411 | `		goto Synchronize;` |
|       - |  4412 | `	}` |
|       - |  4413 | `	/* Jump the left parenthesis '(' */` |
|    4271 |  4414 | `	pGen->pIn++;` |
|       - |  4415 | `	/* Create the loop block */` |
|    4271 |  4416 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    4271 |  4417 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4418 | `		return SXERR_ABORT;` |
|       - |  4419 | `	}` |
|       - |  4420 | `	/* Delimit the expression */` |
|    4271 |  4421 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    4271 |  4422 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4423 | `		/* Empty expression */` |
|     ! 0 |  4424 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4425 | `		if( rc == SXERR_ABORT ){` |
|       - |  4426 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4427 | `			return SXERR_ABORT;` |
|       - |  4428 | `		}` |
|       - |  4429 | `		/* Synchronize */` |
|     ! 0 |  4430 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4431 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4432 | `			pGen->pIn++;` |
|     ! 0 |  4433 | `		}` |
|     ! 0 |  4434 | `		return SXRET_OK;` |
|       - |  4435 | `	}` |
|       - |  4436 | `	/* Compile the array expression */` |
|    4271 |  4437 | `	pCur = pGen->pIn;` |
|   29291 |  4438 | `	while( pCur < pEnd ){` |
|   29291 |  4439 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    4285 |  4440 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    4285 |  4441 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4442 | `				/* Break with the first 'as' found */` |
|    4271 |  4443 | `				break;` |
|       - |  4444 | `			}` |
|       7 |  4445 | `		}` |
|       - |  4446 | `		/* Advance the stream cursor */` |
|   25025 |  4447 | `		pCur++;` |
|       5 |  4448 | `	}` |
|    4271 |  4449 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4450 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4451 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4452 | `		if( rc == SXERR_ABORT ){` |
|       - |  4453 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4454 | `			return SXERR_ABORT;` |
|       - |  4455 | `		}` |
|     ! 0 |  4456 | `		goto Synchronize;` |
|       - |  4457 | `	}` |
|       - |  4458 | `	/* Swap token streams */` |
|    4271 |  4459 | `	pTmp = pGen->pEnd;` |
|    4271 |  4460 | `	pGen->pEnd = pCur;` |
|    4271 |  4461 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    4271 |  4462 | `	if( rc == SXERR_ABORT ){` |
|       - |  4463 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4464 | `		return SXERR_ABORT;` |
|       - |  4465 | `	}` |
|       - |  4466 | `	/* Update token stream */` |
|    4271 |  4467 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4468 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4469 | `		if( rc == SXERR_ABORT ){` |
|       - |  4470 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4471 | `			return SXERR_ABORT;` |
|       - |  4472 | `		}` |
|     ! 0 |  4473 | `		pGen->pIn++;` |
|     ! 0 |  4474 | `	}` |
|    4271 |  4475 | `	pCur++; /* Jump the 'as' keyword */` |
|    4271 |  4476 | `	pGen->pIn = pCur;` |
|    4271 |  4477 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4478 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4479 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4480 | `			return SXERR_ABORT;` |
|       - |  4481 | `		}` |
|     ! 0 |  4482 | `	}` |
|       - |  4483 | `	/* Create the foreach context */` |
|    4271 |  4484 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    4271 |  4485 | `	if( pInfo == 0 ){` |
|     ! 0 |  4486 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4487 | `		return SXERR_ABORT;` |
|       - |  4488 | `	}` |
|       - |  4489 | `	/* Zero the structure */` |
|    4271 |  4490 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4491 | `	/* Initialize structure fields */` |
|    4271 |  4492 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4493 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - |  4494 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - |  4495 | `	 * '=>'. */` |
|    4271 |  4496 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    4271 |  4497 | `	if( pCur < pEnd ){` |
|       - |  4498 | `		/* Compile the expression holding the key name */` |
|    4045 |  4499 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4500 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4501 | `			if( rc == SXERR_ABORT ){` |
|       - |  4502 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4503 | `				return SXERR_ABORT;` |
|       - |  4504 | `			}` |
|     ! 0 |  4505 | `		}else{` |
|    4045 |  4506 | `			pGen->pEnd = pCur;` |
|    4045 |  4507 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    4045 |  4508 | `			if( rc == SXERR_ABORT ){` |
|       - |  4509 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4510 | `				return SXERR_ABORT;` |
|       - |  4511 | `			}` |
|    4045 |  4512 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    4045 |  4513 | `			if( pInstr->p3 ){` |
|       - |  4514 | `				/* Record key name */` |
|    4045 |  4515 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    2020 |  4516 | `			}` |
|    4045 |  4517 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4518 | `		}` |
|    4045 |  4519 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    2020 |  4520 | `	}` |
|    4271 |  4521 | `	pGen->pEnd = pEnd;` |
|    4271 |  4522 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4523 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4524 | `		if( rc == SXERR_ABORT ){` |
|       - |  4525 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4526 | `			return SXERR_ABORT;` |
|       - |  4527 | `		}` |
|     ! 0 |  4528 | `		goto Synchronize;` |
|       - |  4529 | `	}` |
|    4271 |  4530 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4531 | `		pGen->pIn++;` |
|       - |  4532 | `		/* Pass by reference  */` |
|      11 |  4533 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4534 | `	}` |
|       - |  4535 | `	/* Check if the value target is list() */` |
|    4271 |  4536 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4537 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4538 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4539 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4540 | `		 */` |
|       - |  4541 | `		static int iForeachListCnt = 0;` |
|       - |  4542 | `		char zTmp[128];` |
|       - |  4543 | `		sxu32 nLen;` |
|       - |  4544 | `		char *zDup;` |
|      10 |  4545 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4546 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4547 | `		if( zDup == 0 ){` |
|     ! 0 |  4548 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4549 | `			return SXERR_ABORT;` |
|       - |  4550 | `		}` |
|      10 |  4551 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4552 | `		/* Save list() token boundaries */` |
|      10 |  4553 | `		pListStart = pGen->pIn;` |
|       - |  4554 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4555 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4556 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4557 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4558 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4559 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4560 | `				return SXERR_ABORT;` |
|       - |  4561 | `			}` |
|       3 |  4562 | `			goto Synchronize;` |
|       - |  4563 | `		}` |
|       7 |  4564 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4565 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4566 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4567 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4568 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4569 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4570 | `				return SXERR_ABORT;` |
|       - |  4571 | `			}` |
|     ! 0 |  4572 | `			goto Synchronize;` |
|       - |  4573 | `		}` |
|       7 |  4574 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4575 | `		pListEnd = pGen->pIn;` |
|       7 |  4576 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    4266 |  4577 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4578 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4579 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4580 | `		 */` |
|       - |  4581 | `		static int iForeachShortListCnt = 0;` |
|       - |  4582 | `		char zTmp[128];` |
|       - |  4583 | `		sxu32 nLen;` |
|       - |  4584 | `		char *zDup;` |
|      13 |  4585 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|      13 |  4586 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      13 |  4587 | `		if( zDup == 0 ){` |
|     ! 0 |  4588 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4589 | `			return SXERR_ABORT;` |
|       - |  4590 | `		}` |
|      13 |  4591 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4592 | `		/* Save [...] token boundaries */` |
|      13 |  4593 | `		pListStart = pGen->pIn;` |
|       - |  4594 | `		/* Advance past [...] */` |
|      13 |  4595 | `		pGen->pIn++; /* Jump '[' */` |
|      13 |  4596 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|      13 |  4597 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4598 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4599 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4600 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4601 | `				return SXERR_ABORT;` |
|       - |  4602 | `			}` |
|     ! 0 |  4603 | `			goto Synchronize;` |
|       - |  4604 | `		}` |
|      13 |  4605 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|      13 |  4606 | `		pListEnd = pGen->pIn;` |
|      13 |  4607 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       7 |  4608 | `	}else{` |
|       - |  4609 | `		/* Compile the expression holding the value name */` |
|    4251 |  4610 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    4251 |  4611 | `		if( rc == SXERR_ABORT ){` |
|       - |  4612 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4613 | `			return SXERR_ABORT;` |
|       - |  4614 | `		}` |
|    4251 |  4615 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    4251 |  4616 | `		if( pInstr->p3 ){` |
|       - |  4617 | `			/* Record value name */` |
|    4251 |  4618 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    2123 |  4619 | `		}` |
|       - |  4620 | `	}` |
|       - |  4621 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    4269 |  4622 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4623 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    4269 |  4624 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4625 | `	/* Record the first instruction to execute */` |
|    4269 |  4626 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4627 | `	/* Emit the FOREACH_STEP instruction */` |
|    4269 |  4628 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4629 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    4269 |  4630 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4631 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    4269 |  4632 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4633 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4634 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4635 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4636 | `		 */` |
|      19 |  4637 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4638 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4639 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4640 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4641 | `		 */` |
|      19 |  4642 | `		pSavedIn = pGen->pIn;` |
|      19 |  4643 | `		pSavedEnd = pGen->pEnd;` |
|      19 |  4644 | `		pGen->pIn = pListStart;` |
|      19 |  4645 | `		pGen->pEnd = pListEnd;` |
|      19 |  4646 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|      13 |  4647 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  4648 | `		}else{` |
|       7 |  4649 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4650 | `		}` |
|      19 |  4651 | `		pGen->pIn = pSavedIn;` |
|      19 |  4652 | `		pGen->pEnd = pSavedEnd;` |
|      19 |  4653 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4654 | `			return SXERR_ABORT;` |
|       - |  4655 | `		}` |
|       - |  4656 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      19 |  4657 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       9 |  4658 | `	}` |
|       - |  4659 | `	/* Compile the loop body */` |
|    4269 |  4660 | `	pGen->pIn = &pEnd[1];` |
|    4269 |  4661 | `	pGen->pEnd = pTmp;` |
|    4269 |  4662 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    4269 |  4663 | `	if( rc == SXERR_ABORT ){` |
|       - |  4664 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4665 | `		return SXERR_ABORT;` |
|       - |  4666 | `	}` |
|       - |  4667 | `	/* Emit the unconditional jump to the start of the loop */` |
|    4269 |  4668 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4669 | `	/* Fix all jumps now the destination is resolved */` |
|    4269 |  4670 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4671 | `	/* Release the loop block */` |
|    4269 |  4672 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4673 | `	/* Statement successfully compiled */` |
|    4269 |  4674 | `	return SXRET_OK;` |
|       1 |  4675 | `Synchronize:` |
|       - |  4676 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4677 | `	 * compiling this erroneous block.` |
|       - |  4678 | `	 */` |
|       3 |  4679 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4680 | `		pGen->pIn++;` |
|     ! 0 |  4681 | `	}` |
|       3 |  4682 | `	return SXRET_OK;` |
|    2138 |  4683 | `}` |
|       - |  4684 | `/*` |
|       - |  4685 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4686 | ` * According to the PHP language reference` |
|       - |  4687 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4688 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4689 | ` *  that is similar to that of C:` |
|       - |  4690 | ` *  if (expr)` |
|       - |  4691 | ` *   statement` |
|       - |  4692 | ` *  else construct:` |
|       - |  4693 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4694 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4695 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4696 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4697 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4698 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4699 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4700 | ` *  elseif` |
|       - |  4701 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4702 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4703 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4704 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4705 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4706 | ` *   <?php` |
|       - |  4707 | ` *    if ($a > $b) {` |
|       - |  4708 | ` *     echo "a is bigger than b";` |
|       - |  4709 | ` *    } elseif ($a == $b) {` |
|       - |  4710 | ` *     echo "a is equal to b";` |
|       - |  4711 | ` *    } else {` |
|       - |  4712 | ` *     echo "a is smaller than b";` |
|       - |  4713 | ` *    }` |
|       - |  4714 | ` *    ?>` |
|       - |  4715 | ` */` |
|  159568 |  4716 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4717 | `{` |
|  159573 |  4718 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  159573 |  4719 | `	GenBlock *pCondBlock = 0;` |
|       - |  4720 | `	sxu32 nJumpIdx;` |
|       - |  4721 | `	sxu32 nKeyID;` |
|       - |  4722 | `	sxi32 rc;` |
|       - |  4723 | `	/* Jump the 'if' keyword */` |
|  159573 |  4724 | `	pGen->pIn++;` |
|  159573 |  4725 | `	pToken = pGen->pIn;` |
|       - |  4726 | `	/* Create the conditional block */` |
|  159573 |  4727 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  159573 |  4728 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4729 | `		return SXERR_ABORT;` |
|       - |  4730 | `	}` |
|       - |  4731 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   87499 |  4732 | `	for(;;){` |
|  175003 |  4733 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4734 | `			/* Syntax error */` |
|     ! 0 |  4735 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4736 | `				pToken--;` |
|     ! 0 |  4737 | `			}` |
|     ! 0 |  4738 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4739 | `			if( rc == SXERR_ABORT ){` |
|       - |  4740 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4741 | `				return SXERR_ABORT;` |
|       - |  4742 | `			}` |
|     ! 0 |  4743 | `			goto Synchronize;` |
|       - |  4744 | `		}` |
|       - |  4745 | `		/* Jump the left parenthesis '(' */` |
|  175003 |  4746 | `		pToken++;` |
|       - |  4747 | `		/* Delimit the condition */` |
|  175003 |  4748 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  175003 |  4749 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4750 | `			/* Syntax error */` |
|     ! 0 |  4751 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4752 | `				pToken--;` |
|     ! 0 |  4753 | `			}` |
|     ! 0 |  4754 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4755 | `			if( rc == SXERR_ABORT ){` |
|       - |  4756 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4757 | `				return SXERR_ABORT;` |
|       - |  4758 | `			}` |
|     ! 0 |  4759 | `			goto Synchronize;` |
|       - |  4760 | `		}` |
|       - |  4761 | `		/* Swap token streams */` |
|  175003 |  4762 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4763 | `		/* Compile the condition */` |
|  175003 |  4764 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4765 | `		/* Update token stream */` |
|  175003 |  4766 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4767 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4768 | `			pGen->pIn++;` |
|     ! 0 |  4769 | `		}` |
|  175003 |  4770 | `		pGen->pIn  = &pEnd[1];` |
|  175003 |  4771 | `		pGen->pEnd = pTmp;` |
|  175003 |  4772 | `		if( rc == SXERR_ABORT ){` |
|       - |  4773 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4774 | `			return SXERR_ABORT;` |
|       - |  4775 | `		}` |
|       - |  4776 | `		/* Emit the false jump */` |
|  175003 |  4777 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4778 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  175003 |  4779 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4780 | `		/* Compile the body */` |
|  175003 |  4781 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  175003 |  4782 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4783 | `			return SXERR_ABORT;` |
|       - |  4784 | `		}` |
|  175003 |  4785 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   48753 |  4786 | `			break;` |
|       - |  4787 | `		}` |
|       - |  4788 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   77507 |  4789 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   77507 |  4790 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   50111 |  4791 | `			break;` |
|       - |  4792 | `		}` |
|       - |  4793 | `		/* Emit the unconditional jump */` |
|   27401 |  4794 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4795 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   27401 |  4796 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   27401 |  4797 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   19627 |  4798 | `			pToken = &pGen->pIn[1];` |
|   19627 |  4799 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    7696 |  4800 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5988 |  4801 | `					break;` |
|       - |  4802 | `			}` |
|    7661 |  4803 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3828 |  4804 | `		}` |
|   15435 |  4805 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4806 | `		/* Synchronize cursors */` |
|   15435 |  4807 | `		pToken = pGen->pIn;` |
|       - |  4808 | `		/* Fix the false jump */` |
|   15435 |  4809 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4810 | `	} /* For(;;) */` |
|       - |  4811 | `	/* Fix the false jump */` |
|  159573 |  4812 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  159573 |  4813 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   62072 |  4814 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4815 | `			/* Compile the else block */` |
|   11971 |  4816 | `			pGen->pIn++;` |
|   11971 |  4817 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   11971 |  4818 | `			if( rc == SXERR_ABORT ){` |
|       - |  4819 |  |
|     ! 0 |  4820 | `				return SXERR_ABORT;` |
|       - |  4821 | `			}` |
|    5983 |  4822 | `	}` |
|  159573 |  4823 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4824 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  159573 |  4825 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4826 | `	/* Release the conditional block */` |
|  159573 |  4827 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4828 | `	/* Statement successfully compiled */` |
|  159573 |  4829 | `	return SXRET_OK;` |
|     ! 0 |  4830 | `Synchronize:` |
|       - |  4831 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4832 | `	 */` |
|     ! 0 |  4833 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4834 | `		pGen->pIn++;` |
|     ! 0 |  4835 | `	}` |
|     ! 0 |  4836 | `	return SXRET_OK;` |
|   79789 |  4837 | `}` |
|       - |  4838 | `/*` |
|       - |  4839 | ` * Compile the global construct.` |
|       - |  4840 | ` * According to the PHP language reference` |
|       - |  4841 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4842 | ` *  to be used in that function.` |
|       - |  4843 | ` *  Example #1 Using global` |
|       - |  4844 | ` *  <?php` |
|       - |  4845 | ` *   $a = 1;` |
|       - |  4846 | ` *   $b = 2;` |
|       - |  4847 | ` *   function Sum()` |
|       - |  4848 | ` *   {` |
|       - |  4849 | ` *    global $a, $b;` |
|       - |  4850 | ` *    $b = $a + $b;` |
|       - |  4851 | ` *   }` |
|       - |  4852 | ` *   Sum();` |
|       - |  4853 | ` *   echo $b;` |
|       - |  4854 | ` *  ?>` |
|       - |  4855 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4856 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4857 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4858 | ` */` |
|      36 |  4859 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 |  4860 | `{` |
|      41 |  4861 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4862 | `	sxi32 nExpr;` |
|       - |  4863 | `	sxi32 rc;` |
|       - |  4864 | `	/* Jump the 'global' keyword */` |
|      41 |  4865 | `	pGen->pIn++;` |
|      41 |  4866 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4867 | `		/* Nothing to process */` |
|     ! 0 |  4868 | `		return SXRET_OK;` |
|       - |  4869 | `	}` |
|      41 |  4870 | `	pTmp = pGen->pEnd;` |
|      41 |  4871 | `	nExpr = 0;` |
|      87 |  4872 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      51 |  4873 | `		if( pGen->pIn < pNext ){` |
|      51 |  4874 | `			pGen->pEnd = pNext;` |
|      51 |  4875 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4876 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4877 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4878 | `					return SXERR_ABORT;` |
|       - |  4879 | `				}` |
|     ! 0 |  4880 | `			}else{` |
|      51 |  4881 | `				pGen->pIn++;` |
|      51 |  4882 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4883 | `					/* Emit a warning */` |
|     ! 0 |  4884 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4885 | `				}else{` |
|      51 |  4886 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 |  4887 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4888 | `						return SXERR_ABORT;` |
|      51 |  4889 | `					}else if(rc != SXERR_EMPTY ){` |
|      51 |  4890 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      51 |  4891 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4892 | `							/* Variable name, not a constant */` |
|      51 |  4893 | `							pLast->iP1 = 0;` |
|      23 |  4894 | `						}` |
|      51 |  4895 | `						nExpr++;` |
|      23 |  4896 | `					}` |
|       - |  4897 | `				}` |
|       - |  4898 | `			}` |
|      23 |  4899 | `		}` |
|       - |  4900 | `		/* Next expression in the stream */` |
|      51 |  4901 | `		pGen->pIn = pNext;` |
|       - |  4902 | `		/* Jump trailing commas */` |
|      61 |  4903 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  4904 | `			pGen->pIn++;` |
|       5 |  4905 | `		}` |
|       5 |  4906 | `	}` |
|       - |  4907 | `	/* Restore token stream */` |
|      41 |  4908 | `	pGen->pEnd = pTmp;` |
|      41 |  4909 | `	if( nExpr > 0 ){` |
|       - |  4910 | `		/* Emit the uplink instruction */` |
|      41 |  4911 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  4912 | `	}` |
|      41 |  4913 | `	return SXRET_OK;` |
|      23 |  4914 | `}` |
|       - |  4915 | `/*` |
|       - |  4916 | ` * Compile the return statement.` |
|       - |  4917 | ` * According to the PHP language reference` |
|       - |  4918 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4919 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4920 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4921 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4922 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4923 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4924 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4925 | ` *  from within the main script file, then script execution end.` |
|       - |  4926 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4927 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4928 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4929 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4930 | ` */` |
|  254264 |  4931 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4932 | `{` |
|  254269 |  4933 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4934 | `	sxi32 rc;` |
|  254269 |  4935 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  254269 |  4936 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|       - |  4937 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|       - |  4938 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|       - |  4939 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|       - |  4940 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|       - |  4941 | `	 * normally below so token processing stays consistent. */` |
|  654771 |  4942 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  400507 |  4943 | `		pFuncBlock = pFuncBlock->pParent;` |
|       5 |  4944 | `	}` |
|  254264 |  4945 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  254237 |  4946 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|       3 |  4947 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  4948 | `			"A never-returning function must not return");` |
|       3 |  4949 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4950 | `			return SXERR_ABORT;` |
|       - |  4951 | `		}` |
|       1 |  4952 | `	}` |
|       - |  4953 | `	/* Jump the 'return' keyword */` |
|  254269 |  4954 | `	pGen->pIn++;` |
|  254269 |  4955 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4956 | `		/* Compile the expression */` |
|  254239 |  4957 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  254239 |  4958 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4959 | `			return SXERR_ABORT;` |
|  254239 |  4960 | `		}else if(rc != SXERR_EMPTY ){` |
|  254239 |  4961 | `			nRet = 1;` |
|  127117 |  4962 | `		}` |
|  127117 |  4963 | `	}` |
|       - |  4964 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|       - |  4965 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|       - |  4966 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|       - |  4967 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  254269 |  4968 | `	if( pGen->bInGenerator ){` |
|      30 |  4969 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      30 |  4970 | `		return SXRET_OK;` |
|       - |  4971 | `	}` |
|       - |  4972 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4973 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4974 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4975 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4976 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  254243 |  4977 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  254243 |  4978 | `	return SXRET_OK;` |
|  127137 |  4979 | `}` |
|       - |  4980 | `/*` |
|       - |  4981 | ` * Compile a yield expression.` |
|       - |  4982 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4983 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4984 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4985 | ` */` |
|     328 |  4986 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4987 | `{` |
|       - |  4988 | `	SyToken *pTmp, *pSplit;` |
|     333 |  4989 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     333 |  4990 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4991 | `	sxi32 rc;` |
|     164 |  4992 | `	(void)iCompileFlag;` |
|       - |  4993 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     333 |  4994 | `	pGen->pIn++;` |
|       - |  4995 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4996 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - |  4997 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - |  4998 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - |  4999 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     358 |  5000 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     194 |  5001 | `		&& pGen->pIn->sData.nByte == 4` |
|      66 |  5002 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      64 |  5003 | `		pGen->pIn++; /* Skip 'from' */` |
|      64 |  5004 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      64 |  5005 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5006 | `			return SXERR_ABORT;` |
|       - |  5007 | `		}` |
|      64 |  5008 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  5009 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 |  5010 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - |  5011 | `				"Missing expression after 'yield from'");` |
|     ! 0 |  5012 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5013 | `				return SXERR_ABORT;` |
|       - |  5014 | `			}` |
|     ! 0 |  5015 | `		}` |
|      64 |  5016 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      64 |  5017 | `		return SXRET_OK;` |
|       - |  5018 | `	}` |
|     273 |  5019 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5020 | `		/* Bare yield — no value */` |
|       3 |  5021 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       3 |  5022 | `		return SXRET_OK;` |
|       - |  5023 | `	}` |
|       - |  5024 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     271 |  5025 | `	pSplit = 0;` |
|       - |  5026 | `	{` |
|     271 |  5027 | `		SyToken *pCur = pGen->pIn;` |
|     271 |  5028 | `		sxi32 nNest = 0;` |
|     569 |  5029 | `		while( pCur < pGen->pEnd ){` |
|     317 |  5030 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  5031 | `				nNest++;` |
|     316 |  5032 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  5033 | `				nNest--;` |
|     314 |  5034 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  5035 | `				pSplit = pCur;` |
|      16 |  5036 | `				break;` |
|       - |  5037 | `			}` |
|     303 |  5038 | `			pCur++;` |
|       5 |  5039 | `		}` |
|       - |  5040 | `	}` |
|     271 |  5041 | `	pTmp = pGen->pEnd;` |
|     271 |  5042 | `	if( pSplit ){` |
|       - |  5043 | `		/* yield $key => $value */` |
|      16 |  5044 | `		pGen->pEnd = pSplit;` |
|      16 |  5045 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  5046 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  5047 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  5048 | `		pGen->pEnd = pTmp;` |
|      16 |  5049 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  5050 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  5051 | `		iP1 = 1;` |
|      16 |  5052 | `		iP2 = 1;` |
|       9 |  5053 | `	}else{` |
|       - |  5054 | `		/* yield $value */` |
|     257 |  5055 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     257 |  5056 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     257 |  5057 | `		if( rc != SXERR_EMPTY ){` |
|     257 |  5058 | `			iP1 = 1;` |
|     126 |  5059 | `		}` |
|       - |  5060 | `	}` |
|     271 |  5061 | `	pGen->pEnd = pTmp;` |
|     271 |  5062 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     271 |  5063 | `	return SXRET_OK;` |
|     169 |  5064 | `}` |
|       - |  5065 | `/*` |
|       - |  5066 | ` * Compile the die/exit language construct.` |
|       - |  5067 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  5068 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  5069 | ` */` |
|     122 |  5070 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  5071 | `{` |
|     127 |  5072 | `	sxi32 nExpr = 0;` |
|       - |  5073 | `	sxi32 rc;` |
|       - |  5074 | `	/* Jump the die/exit keyword */` |
|     127 |  5075 | `	pGen->pIn++;` |
|     127 |  5076 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  5077 | `		/* Compile the expression */` |
|     127 |  5078 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     127 |  5079 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5080 | `			return SXERR_ABORT;` |
|     127 |  5081 | `		}else if(rc != SXERR_EMPTY ){` |
|     127 |  5082 | `			nExpr = 1;` |
|      61 |  5083 | `		}` |
|      61 |  5084 | `	}` |
|       - |  5085 | `	/* Emit the HALT instruction */` |
|     127 |  5086 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     127 |  5087 | `	return SXRET_OK;` |
|      66 |  5088 | `}` |
|       - |  5089 | `/*` |
|       - |  5090 | ` * Compile the 'echo' language construct.` |
|       - |  5091 | ` */` |
|   14764 |  5092 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  5093 | `{` |
|   14769 |  5094 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  5095 | `	sxi32 rc;` |
|       - |  5096 | `	/* Jump the 'echo' keyword */` |
|   14769 |  5097 | `	pGen->pIn++;` |
|       - |  5098 | `	/* Compile arguments one after one */` |
|   14769 |  5099 | `	pTmp = pGen->pEnd;` |
|   33993 |  5100 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   19229 |  5101 | `		if( pGen->pIn < pNext ){` |
|   19229 |  5102 | `			pGen->pEnd = pNext;` |
|   19229 |  5103 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   19229 |  5104 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5105 | `				return SXERR_ABORT;` |
|   19229 |  5106 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  5107 | `				/* Emit the consume instruction */` |
|   19205 |  5108 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    9600 |  5109 | `			}` |
|    9612 |  5110 | `		}` |
|       - |  5111 | `		/* Jump trailing commas */` |
|   23689 |  5112 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    4465 |  5113 | `			pNext++;` |
|       5 |  5114 | `		}` |
|   19229 |  5115 | `		pGen->pIn = pNext;` |
|       5 |  5116 | `	}` |
|       - |  5117 | `	/* Restore token stream */` |
|   14769 |  5118 | `	pGen->pEnd = pTmp;` |
|   14769 |  5119 | `	return SXRET_OK;` |
|    7387 |  5120 | `}` |
|       - |  5121 | `/*` |
|       - |  5122 | ` * Compile the static statement.` |
|       - |  5123 | ` * According to the PHP language reference` |
|       - |  5124 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  5125 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  5126 | ` *  when program execution leaves this scope.` |
|       - |  5127 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  5128 | ` * Symisc eXtension.` |
|       - |  5129 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  5130 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  5131 | ` *  Example` |
|       - |  5132 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  5133 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  5134 | ` */` |
|       8 |  5135 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       3 |  5136 | `{` |
|       - |  5137 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  5138 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  5139 | `	GenBlock *pBlock;` |
|       - |  5140 | `	SyString *pName;` |
|       - |  5141 | `	char *zDup;` |
|       - |  5142 | `	sxu32 nLine;` |
|       - |  5143 | `	sxi32 rc;` |
|       - |  5144 | `	/* Jump the static keyword */` |
|      11 |  5145 | `	nLine = pGen->pIn->nLine;` |
|      11 |  5146 | `	pGen->pIn++;` |
|       - |  5147 | `	/* Extract the enclosing function if any */` |
|      11 |  5148 | `	pBlock = pGen->pCurrent;` |
|      19 |  5149 | `	while( pBlock ){` |
|      19 |  5150 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|      11 |  5151 | `			break;` |
|       - |  5152 | `		}` |
|       - |  5153 | `		/* Point to the upper block */` |
|      11 |  5154 | `		pBlock = pBlock->pParent;` |
|       3 |  5155 | `	}` |
|      11 |  5156 | `	if( pBlock == 0 ){` |
|       - |  5157 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  5158 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  5159 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  5160 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5161 | `				return SXERR_ABORT;` |
|       - |  5162 | `			}` |
|     ! 0 |  5163 | `			goto Synchronize;` |
|       - |  5164 | `		}` |
|       - |  5165 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  5166 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  5167 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5168 | `			return SXERR_ABORT;` |
|     ! 0 |  5169 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  5170 | `			/* Emit the POP instruction */` |
|     ! 0 |  5171 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  5172 | `		}` |
|     ! 0 |  5173 | `		return SXRET_OK;` |
|       - |  5174 | `	}` |
|      11 |  5175 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  5176 | `	/* Make sure we are dealing with a valid statement */` |
|      11 |  5177 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       6 |  5178 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  5179 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  5180 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5181 | `				return SXERR_ABORT;` |
|       - |  5182 | `			}` |
|       3 |  5183 | `			goto Synchronize;` |
|       - |  5184 | `	}` |
|       8 |  5185 | `	pGen->pIn++;` |
|       - |  5186 | `	/* Extract variable name */` |
|       8 |  5187 | `	pName = &pGen->pIn->sData;` |
|       8 |  5188 | `	pGen->pIn++; /* Jump the var name */` |
|       8 |  5189 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  5190 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  5191 | `		goto Synchronize;` |
|       - |  5192 | `	}` |
|       - |  5193 | `	/* Initialize the structure describing the static variable */` |
|       8 |  5194 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       8 |  5195 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  5196 | `	/* Duplicate variable name */` |
|       8 |  5197 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       8 |  5198 | `	if( zDup == 0 ){` |
|     ! 0 |  5199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  5200 | `		return SXERR_ABORT;` |
|       - |  5201 | `	}` |
|       8 |  5202 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  5203 | `	/* Check if we have an expression to compile */` |
|       8 |  5204 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  5205 | `		SySet *pInstrContainer;` |
|       - |  5206 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  5207 | `		 * Static variable can take any complex expression including function` |
|       - |  5208 | `		 * call as their initialization value.` |
|       - |  5209 | `		 * Example:` |
|       - |  5210 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  5211 | `		 */` |
|       8 |  5212 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  5213 | `		/* Swap bytecode container */` |
|       8 |  5214 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       8 |  5215 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  5216 | `		/* Compile the expression */` |
|       8 |  5217 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5218 | `		/* Emit the done instruction */` |
|       8 |  5219 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  5220 | `		/* Restore default bytecode container */` |
|       8 |  5221 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       3 |  5222 | `	}` |
|       - |  5223 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       8 |  5224 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       8 |  5225 | `	return SXRET_OK;` |
|       1 |  5226 | `Synchronize:` |
|       - |  5227 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  5228 | `	 * statement.` |
|       - |  5229 | `	 */` |
|       5 |  5230 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  5231 | `		pGen->pIn++;` |
|       1 |  5232 | `	}` |
|       3 |  5233 | `	return SXRET_OK;` |
|       7 |  5234 | `}` |
|       - |  5235 | `/*` |
|       - |  5236 | ` * Compile the var statement.` |
|       - |  5237 | ` * Symisc Extension:` |
|       - |  5238 | ` *      var statement can be used outside of a class definition.` |
|       - |  5239 | ` */` |
|       4 |  5240 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  5241 | `{` |
|       - |  5242 | `	sxu32 nLine;` |
|       - |  5243 | `	sxi32 rc;` |
|       5 |  5244 | `	nLine = pGen->pIn->nLine;` |
|       - |  5245 | `	/* Jump the 'var' keyword */` |
|       5 |  5246 | `	pGen->pIn++;` |
|       5 |  5247 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  5248 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  5249 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  5250 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  5251 | `			pGen->pIn++;` |
|     ! 0 |  5252 | `		}` |
|     ! 0 |  5253 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5254 | `			return SXERR_ABORT;` |
|       - |  5255 | `		}` |
|     ! 0 |  5256 | `	}else{` |
|       - |  5257 | `		/* Compile the expression */` |
|       5 |  5258 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  5259 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5260 | `			return SXERR_ABORT;` |
|       5 |  5261 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  5262 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  5263 | `		}` |
|       - |  5264 | `	}` |
|       5 |  5265 | `	return SXRET_OK;` |
|       3 |  5266 | `}` |
|       - |  5267 | `/*` |
|       - |  5268 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  5269 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  5270 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  5271 | ` */` |
|       - |  5272 | `/*` |
|       - |  5273 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  5274 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  5275 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  5276 | ` * qualified name and updates the instruction's operand index.` |
|       - |  5277 | ` *` |
|       - |  5278 | ` * Resolution order:` |
|       - |  5279 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  5280 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  5281 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  5282 | ` *` |
|       - |  5283 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  5284 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  5285 | ` * Returns the (possibly new) literal index.` |
|       - |  5286 | ` */` |
|  493468 |  5287 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  5288 | `{` |
|       - |  5289 | `	ph7_value *pLit;` |
|       - |  5290 | `	const char *zLit;` |
|       - |  5291 | `	SyString sQualified;` |
|       - |  5292 | `	sxu32 nLit;` |
|       - |  5293 | `	sxu32 k;` |
|       - |  5294 | `	sxu32 nNewIdx;` |
|       - |  5295 | `	int hasNsSep;` |
|       - |  5296 | `	SyHashEntry *pImport;` |
|       - |  5297 | `	ph7_value *pNew;` |
|  493473 |  5298 | `	if( pFromImport ){` |
|  472199 |  5299 | `		*pFromImport = 0;` |
|  236097 |  5300 | `	}` |
|  493473 |  5301 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  493473 |  5302 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  5303 | `		return nOrigIdx;` |
|       - |  5304 | `	}` |
|  493473 |  5305 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  493473 |  5306 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  5307 | `	/* Skip if already qualified (contains backslash) */` |
|  493473 |  5308 | `	hasNsSep = 0;` |
| 5445741 |  5309 | `	for( k = 0; k < nLit; k++ ){` |
| 4952281 |  5310 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2476139 |  5311 | `	}` |
|  493473 |  5312 | `	if( hasNsSep ){` |
|      10 |  5313 | `		return nOrigIdx;` |
|       - |  5314 | `	}` |
|       - |  5315 | `	/* Check use imports first (works even outside namespaces) */` |
|  493465 |  5316 | `	SyBlobReset(&pGen->sWorker);` |
|  493465 |  5317 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  493465 |  5318 | `	if( pImport ){` |
|      41 |  5319 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  5320 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  5321 | `		if( pFromImport ){` |
|      18 |  5322 | `			*pFromImport = 1;` |
|       8 |  5323 | `		}` |
|      23 |  5324 | `	}else{` |
|  493429 |  5325 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  493339 |  5326 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  5327 | `		}` |
|       - |  5328 | `		/* Prepend current namespace */` |
|      95 |  5329 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  5330 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  5331 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  5332 | `	}` |
|       - |  5333 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  5334 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  5335 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  5336 | `		return nNewIdx; /* Already interned */` |
|       - |  5337 | `	}` |
|      79 |  5338 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  5339 | `	if( pNew == 0 ){` |
|     ! 0 |  5340 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  5341 | `	}` |
|      79 |  5342 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  5343 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  5344 | `	return nNewIdx;` |
|  246739 |  5345 | `}` |
|       - |  5346 | `/*` |
|       - |  5347 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  5348 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  5349 | ` */` |
|  104422 |  5350 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5351 | `{` |
|       - |  5352 | `	SyHashEntry *pImport;` |
|       - |  5353 | `	/* Check use imports first */` |
|  104427 |  5354 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|  104427 |  5355 | `	if( pImport ){` |
|      19 |  5356 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      19 |  5357 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      19 |  5358 | `		return;` |
|       - |  5359 | `	}` |
|       - |  5360 | `	/* Prepend current namespace if active */` |
|  104411 |  5361 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  5362 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  5363 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  5364 | `	}` |
|  104411 |  5365 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   52216 |  5366 | `}` |
|       - |  5367 | `/*` |
|       - |  5368 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5369 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5370 | ` * The caller must release pOut when done.` |
|       - |  5371 | ` */` |
|  154602 |  5372 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5373 | `{` |
|  154607 |  5374 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|    3891 |  5375 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|    3891 |  5376 | `		SyBlobAppend(pOut,"\\",1);` |
|    1943 |  5377 | `	}` |
|  154607 |  5378 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  154607 |  5379 | `}` |
|       - |  5380 | `/*` |
|       - |  5381 | ` * Compile a namespace statement` |
|       - |  5382 | ` * According to the PHP language reference manual` |
|       - |  5383 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5384 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5385 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5386 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5387 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5388 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5389 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5390 | ` *  programming world.` |
|       - |  5391 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5392 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5393 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5394 | ` *  classes/functions/constants.` |
|       - |  5395 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5396 | ` *  readability of source code.` |
|       - |  5397 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5398 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5399 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5400 | ` *       class MyClass {}` |
|       - |  5401 | ` *       function myfunction() {}` |
|       - |  5402 | ` *       const MYCONST = 1;` |
|       - |  5403 | ` *       $a = new MyClass;` |
|       - |  5404 | ` *       $c = new \my\name\MyClass;` |
|       - |  5405 | ` *       $a = strlen('hi');` |
|       - |  5406 | ` *       $d = namespace\MYCONST;` |
|       - |  5407 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5408 | ` *       echo constant($d);` |
|       - |  5409 | ` * NOTE` |
|       - |  5410 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5411 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5412 | ` */` |
|       - |  5413 | `/*` |
|       - |  5414 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5415 | ` */` |
|      14 |  5416 | `static const char * TokenTypeName(sxu32 nType)` |
|       3 |  5417 | `{` |
|      17 |  5418 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      10 |  5419 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      10 |  5420 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      10 |  5421 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      10 |  5422 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      10 |  5423 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5424 | `	return "token";` |
|      10 |  5425 | `}` |
|    3934 |  5426 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5427 | `{` |
|       - |  5428 | `	sxu32 nLine;` |
|       - |  5429 | `	sxi32 rc;` |
|    3939 |  5430 | `	nLine = pGen->pIn->nLine;` |
|    3939 |  5431 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5432 | `	/* Reset namespace and clear previous use imports */` |
|    3939 |  5433 | `	SyBlobReset(&pGen->sNamespace);` |
|    3939 |  5434 | `	SyHashRelease(&pGen->hUseImports);` |
|    3939 |  5435 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|    3939 |  5436 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|    3939 |  5437 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|    3939 |  5438 | `	SyHashRelease(&pGen->hUseConstImports);` |
|    3939 |  5439 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|    3939 |  5440 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5441 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5442 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5443 | `		return SXRET_OK;` |
|       - |  5444 | `	}` |
|    3939 |  5445 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5446 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5447 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5448 | `		return SXRET_OK;` |
|       - |  5449 | `	}` |
|    3939 |  5450 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5451 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5452 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5453 | `		return SXRET_OK;` |
|       - |  5454 | `	}` |
|       - |  5455 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|    7915 |  5456 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|    3981 |  5457 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5458 | `			/* Append backslash separator */` |
|      27 |  5459 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      27 |  5460 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5461 | `			}` |
|      16 |  5462 | `		}else{` |
|       - |  5463 | `			/* Append identifier */` |
|    3959 |  5464 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5465 | `		}` |
|    3981 |  5466 | `		pGen->pIn++;` |
|       5 |  5467 | `	}` |
|       - |  5468 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5469 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5470 | `	{` |
|    3939 |  5471 | `		char *zNsDup = 0;` |
|    3939 |  5472 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|    5903 |  5473 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    3932 |  5474 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|    1966 |  5475 | `		}` |
|    3939 |  5476 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5477 | `	}` |
|    3939 |  5478 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5479 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5480 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5481 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5482 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5483 | `			return SXERR_ABORT;` |
|       - |  5484 | `		}` |
|       2 |  5485 | `	}` |
|    3939 |  5486 | `	return SXRET_OK;` |
|    1972 |  5487 | `}` |
|       - |  5488 | `/*` |
|       - |  5489 | ` * Compile the 'use' statement` |
|       - |  5490 | ` * According to the PHP language reference manual` |
|       - |  5491 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5492 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5493 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5494 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5495 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5496 | ` *  a function or constant is not supported.` |
|       - |  5497 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5498 | ` * NOTE` |
|       - |  5499 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5500 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5501 | ` */` |
|      72 |  5502 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5503 | `{` |
|       - |  5504 | `	sxu32 nLine;` |
|       - |  5505 | `	sxi32 rc;` |
|       - |  5506 | `	SyBlob sPath;` |
|       - |  5507 | `	SyString sAlias;` |
|       - |  5508 | `	SyToken *pLast;` |
|       - |  5509 | `	char *zDup;` |
|       - |  5510 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5511 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5512 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      77 |  5513 | `	nLine = pGen->pIn->nLine;` |
|      77 |  5514 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5515 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      77 |  5516 | `	iUseType = 0;` |
|      77 |  5517 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5518 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5519 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5520 | `			iUseType = 1;` |
|      16 |  5521 | `			pGen->pIn++;` |
|      23 |  5522 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5523 | `			iUseType = 2;` |
|      16 |  5524 | `			pGen->pIn++;` |
|       7 |  5525 | `		}` |
|      14 |  5526 | `	}` |
|       - |  5527 | `	/* Select target hash tables based on import type */` |
|      77 |  5528 | `	switch( iUseType ){` |
|       7 |  5529 | `		case 1:` |
|      16 |  5530 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5531 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5532 | `			break;` |
|       7 |  5533 | `		case 2:` |
|      16 |  5534 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5535 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5536 | `			break;` |
|      22 |  5537 | `		default:` |
|      49 |  5538 | `			pGenHash = &pGen->hUseImports;` |
|      49 |  5539 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      44 |  5540 | `			break;` |
|       - |  5541 | `	}` |
|      77 |  5542 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5543 | `	/* Process one or more use declarations separated by commas */` |
|      37 |  5544 | `	for(;;){` |
|      79 |  5545 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5546 | `			break;` |
|       - |  5547 | `		}` |
|      79 |  5548 | `		SyBlobReset(&sPath);` |
|      79 |  5549 | `		pLast = 0;` |
|       - |  5550 | `		/* Collect the full namespace path */` |
|     269 |  5551 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     195 |  5552 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     135 |  5553 | `				pLast = pGen->pIn;` |
|     135 |  5554 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5555 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5556 | `				}` |
|     135 |  5557 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      65 |  5558 | `			}` |
|     195 |  5559 | `			pGen->pIn++;` |
|       5 |  5560 | `		}` |
|      79 |  5561 | `		if( pLast == 0 ){` |
|       - |  5562 | `			/* Empty path */` |
|       6 |  5563 | `			break;` |
|       - |  5564 | `		}` |
|       - |  5565 | `		/* Default alias is the last component of the path */` |
|      75 |  5566 | `		sAlias = pLast->sData;` |
|       - |  5567 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      70 |  5568 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      50 |  5569 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      24 |  5570 | `			pGen->pIn++; /* Jump 'as' */` |
|      24 |  5571 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      24 |  5572 | `				sAlias = pGen->pIn->sData;` |
|      24 |  5573 | `				pGen->pIn++;` |
|      10 |  5574 | `			}` |
|      10 |  5575 | `		}` |
|       - |  5576 | `		/* Check for duplicate import alias (per-type) */` |
|      75 |  5577 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5578 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5579 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5580 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5581 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5582 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5583 | `				return SXERR_ABORT;` |
|       - |  5584 | `			}` |
|       2 |  5585 | `		}` |
|       - |  5586 | `		/* Register the import: alias -> FQN.` |
|       - |  5587 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5588 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5589 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     110 |  5590 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      70 |  5591 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      75 |  5592 | `		if( zDup ){` |
|      75 |  5593 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      75 |  5594 | `			if( pVmHash ){` |
|       - |  5595 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5596 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      47 |  5597 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      47 |  5598 | `				if( zAliasDup ){` |
|      47 |  5599 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      21 |  5600 | `				}` |
|      21 |  5601 | `			}` |
|      75 |  5602 | `			if( iUseType == 2 ){` |
|       - |  5603 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5604 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5605 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5606 | `				if( zAliasDup ){` |
|       - |  5607 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5608 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5609 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5610 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5611 | `					if( azPair ){` |
|      16 |  5612 | `						azPair[0] = zAliasDup;` |
|      16 |  5613 | `						azPair[1] = zDup;` |
|      16 |  5614 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5615 | `					}` |
|       7 |  5616 | `				}` |
|       7 |  5617 | `			}` |
|      35 |  5618 | `		}` |
|       - |  5619 | `		/* Check for comma (multiple use declarations) */` |
|      75 |  5620 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5621 | `			pGen->pIn++;` |
|       2 |  5622 | `		}else{` |
|      39 |  5623 | `			break;` |
|       - |  5624 | `		}` |
|       1 |  5625 | `	}` |
|      77 |  5626 | `	SyBlobRelease(&sPath);` |
|      77 |  5627 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5628 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5629 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5630 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5631 | `			return SXERR_ABORT;` |
|       - |  5632 | `		}` |
|       1 |  5633 | `	}` |
|      77 |  5634 | `	return SXRET_OK;` |
|      41 |  5635 | `}` |
|       - |  5636 | `/*` |
|       - |  5637 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5638 | ` *` |
|       - |  5639 | ` * According to the PHP language reference manual.` |
|       - |  5640 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5641 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5642 | ` *  declare (directive)` |
|       - |  5643 | ` *   statement` |
|       - |  5644 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5645 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5646 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5647 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5648 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5649 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5650 | ` * <?php` |
|       - |  5651 | ` * // these are the same:` |
|       - |  5652 | ` * // you can use this:` |
|       - |  5653 | ` * declare(ticks=1) {` |
|       - |  5654 | ` *   // entire script here` |
|       - |  5655 | ` * }` |
|       - |  5656 | ` * // or you can use this:` |
|       - |  5657 | ` * declare(ticks=1);` |
|       - |  5658 | ` * // entire script here` |
|       - |  5659 | ` * ?>` |
|       - |  5660 | ` *` |
|       - |  5661 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5662 | ` */` |
|       - |  5663 | `/*` |
|       - |  5664 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5665 | ` */` |
|      72 |  5666 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5667 | `{` |
|     109 |  5668 | `	return SyStringLength(pName) == nWant` |
|      72 |  5669 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5670 | `}` |
|       - |  5671 |  |
|      42 |  5672 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5673 | `{` |
|      47 |  5674 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      47 |  5675 | `	SyToken *pBodyEnd = 0;` |
|       - |  5676 | `	SyToken *pBodyStart;` |
|       - |  5677 | `	SyToken *pCursor;` |
|       - |  5678 | `	int bHasStrictTypes;` |
|       - |  5679 | `	int bBlockForm;` |
|       - |  5680 | `	int bPlacementOk;` |
|       - |  5681 | `	sxi32 rc;` |
|      47 |  5682 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      47 |  5683 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 |  5684 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       6 |  5685 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5686 | `			return SXERR_ABORT;` |
|       - |  5687 | `		}` |
|       6 |  5688 | `		goto Synchro;` |
|       - |  5689 | `	}` |
|      43 |  5690 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      43 |  5691 | `	pBodyStart = pGen->pIn;` |
|       - |  5692 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      43 |  5693 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      43 |  5694 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5695 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5696 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5697 | `			return SXERR_ABORT;` |
|       - |  5698 | `		}` |
|     ! 0 |  5699 | `		return SXRET_OK;` |
|       - |  5700 | `	}` |
|       - |  5701 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5702 | `	 * now delimits the comma-separated directive list. */` |
|      43 |  5703 | `	pGen->pIn = &pBodyEnd[1];` |
|      43 |  5704 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5705 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5706 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5707 | `			return SXERR_ABORT;` |
|       - |  5708 | `		}` |
|     ! 0 |  5709 | `	}` |
|      43 |  5710 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      43 |  5711 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      43 |  5712 | `	bHasStrictTypes = 0;` |
|       - |  5713 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5714 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5715 | `	 * directive appears anywhere in the list, before validating values. */` |
|      43 |  5716 | `	pCursor = pBodyStart;` |
|      55 |  5717 | `	while( pCursor < pBodyEnd ){` |
|      51 |  5718 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      43 |  5719 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      39 |  5720 | `				bHasStrictTypes = 1;` |
|      39 |  5721 | `				break;` |
|       - |  5722 | `			}` |
|       2 |  5723 | `		}` |
|      14 |  5724 | `		pCursor++;` |
|       2 |  5725 | `	}` |
|      43 |  5726 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5727 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5728 | `			"strict_types declaration must not use block mode");` |
|       3 |  5729 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5730 | `		return SXRET_OK;` |
|       - |  5731 | `	}` |
|      41 |  5732 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5733 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5734 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5735 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5736 | `		return SXRET_OK;` |
|       - |  5737 | `	}` |
|       - |  5738 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      37 |  5739 | `	pCursor = pBodyStart;` |
|      69 |  5740 | `	while( pCursor < pBodyEnd ){` |
|       - |  5741 | `		SyToken *pNameTok;` |
|       - |  5742 | `		SyToken *pEqTok;` |
|       - |  5743 | `		SyToken *pValTok;` |
|       - |  5744 | `		SyString *pDirName;` |
|       - |  5745 | `		int bIsStrict;` |
|       - |  5746 | `		int iStrictValue;` |
|      39 |  5747 | `		pNameTok = pCursor;` |
|      39 |  5748 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5749 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5750 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5751 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5752 | `			return SXRET_OK;` |
|       - |  5753 | `		}` |
|      39 |  5754 | `		pEqTok = pNameTok + 1;` |
|      39 |  5755 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5756 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5757 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5758 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5759 | `			return SXRET_OK;` |
|       - |  5760 | `		}` |
|      39 |  5761 | `		pValTok = pEqTok + 1;` |
|      39 |  5762 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5763 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5764 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5765 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5766 | `			return SXRET_OK;` |
|       - |  5767 | `		}` |
|      39 |  5768 | `		pDirName = &pNameTok->sData;` |
|      39 |  5769 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      39 |  5770 | `		if( bIsStrict ){` |
|       - |  5771 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5772 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      35 |  5773 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5774 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5775 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5776 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5777 | `				return SXRET_OK;` |
|       - |  5778 | `			}` |
|      35 |  5779 | `			iStrictValue = -1;` |
|      35 |  5780 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      35 |  5781 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      35 |  5782 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      35 |  5783 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      33 |  5784 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      15 |  5785 | `			}` |
|      35 |  5786 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5787 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5788 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5789 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5790 | `				return SXRET_OK;` |
|       - |  5791 | `			}` |
|      32 |  5792 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      18 |  5793 | `		}else{` |
|       - |  5794 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5795 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5796 | `			 * behavior don't regress. */` |
|       8 |  5797 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5798 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5799 | `				ph7_lib_version()` |
|       - |  5800 | `				);` |
|       - |  5801 | `		}` |
|      37 |  5802 | `		pCursor = pValTok + 1;` |
|       - |  5803 | `		/* Consume separating comma (or end). */` |
|      37 |  5804 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5805 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5806 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5807 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5808 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5809 | `				return SXRET_OK;` |
|       - |  5810 | `			}` |
|       3 |  5811 | `			pCursor++;` |
|       1 |  5812 | `		}` |
|       5 |  5813 | `	}` |
|       - |  5814 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5815 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5816 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      35 |  5817 | `	return SXRET_OK;` |
|       2 |  5818 | `Synchro:` |
|       - |  5819 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 |  5820 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 |  5821 | `		pGen->pIn++;` |
|       2 |  5822 | `	}` |
|       6 |  5823 | `	return SXRET_OK;` |
|      26 |  5824 | `}` |
|       - |  5825 | `/*` |
|       - |  5826 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5827 | ` * as follows:` |
|       - |  5828 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5829 | ` * {` |
|       - |  5830 | ` *   return "Making a cup of $type.\n";` |
|       - |  5831 | ` * }` |
|       - |  5832 | ` * Symisc eXtension.` |
|       - |  5833 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5834 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5835 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5836 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5837 | ` *      {` |
|       - |  5838 | ` *       var_dump($a);` |
|       - |  5839 | ` *      }` |
|       - |  5840 | ` *     //call test without args` |
|       - |  5841 | ` *      test();` |
|       - |  5842 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5843 | ` *      Example:` |
|       - |  5844 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5845 | ` * 3 -) Function overloading!!` |
|       - |  5846 | ` *      Example:` |
|       - |  5847 | ` *      function foo($a) {` |
|       - |  5848 | ` *   	  return $a.PHP_EOL;` |
|       - |  5849 | ` *	    }` |
|       - |  5850 | ` *	    function foo($a, $b) {` |
|       - |  5851 | ` *   	  return $a + $b;` |
|       - |  5852 | ` *	    }` |
|       - |  5853 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5854 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5855 | ` *      // Same arg` |
|       - |  5856 | ` *	   function foo(string $a)` |
|       - |  5857 | ` *	   {` |
|       - |  5858 | ` *	     echo "a is a string\n";` |
|       - |  5859 | ` *	     var_dump($a);` |
|       - |  5860 | ` *	   }` |
|       - |  5861 | ` *	  function foo(int $a)` |
|       - |  5862 | ` *	  {` |
|       - |  5863 | ` *	    echo "a is integer\n";` |
|       - |  5864 | ` *	    var_dump($a);` |
|       - |  5865 | ` *	  }` |
|       - |  5866 | ` *	  function foo(array $a)` |
|       - |  5867 | ` *	  {` |
|       - |  5868 | ` * 	    echo "a is an array\n";` |
|       - |  5869 | ` * 	    var_dump($a);` |
|       - |  5870 | ` *	  }` |
|       - |  5871 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5872 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5873 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5874 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5875 | ` * introduced by the PH7 engine.` |
|       - |  5876 | ` */` |
|   80462 |  5877 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5878 | `{` |
|       - |  5879 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5880 | `	SySet *pInstrContainer;` |
|       - |  5881 | `	sxi32 rc;` |
|       - |  5882 | `	/* Swap token stream */` |
|   80467 |  5883 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   80467 |  5884 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   80467 |  5885 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5886 | `	/* Compile the expression holding the argument value */` |
|   80467 |  5887 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5888 | `	/* Emit the done instruction */` |
|   80467 |  5889 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   80467 |  5890 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   80467 |  5891 | `	RE_SWAP_DELIMITER(pGen);` |
|   80467 |  5892 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5893 | `		return SXERR_ABORT;` |
|       - |  5894 | `	}` |
|   80467 |  5895 | `	return SXRET_OK;` |
|   40236 |  5896 | `}` |
|       - |  5897 | `/*` |
|       - |  5898 | ` * Collect function arguments one after one.` |
|       - |  5899 | ` * According to the PHP language reference manual.` |
|       - |  5900 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5901 | ` * list of expressions.` |
|       - |  5902 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5903 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5904 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5905 | ` * for more information.` |
|       - |  5906 | ` * Example #1 Passing arrays to functions` |
|       - |  5907 | ` * <?php` |
|       - |  5908 | ` * function takes_array($input)` |
|       - |  5909 | ` * {` |
|       - |  5910 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5911 | ` * }` |
|       - |  5912 | ` * ?>` |
|       - |  5913 | ` * Making arguments be passed by reference` |
|       - |  5914 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5915 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5916 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5917 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5918 | ` * to the argument name in the function definition:` |
|       - |  5919 | ` * Example #2 Passing function parameters by reference` |
|       - |  5920 | ` * <?php` |
|       - |  5921 | ` * function add_some_extra(&$string)` |
|       - |  5922 | ` * {` |
|       - |  5923 | ` *   $string .= 'and something extra.';` |
|       - |  5924 | ` * }` |
|       - |  5925 | ` * $str = 'This is a string, ';` |
|       - |  5926 | ` * add_some_extra($str);` |
|       - |  5927 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5928 | ` * ?>` |
|       - |  5929 | ` *` |
|       - |  5930 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5931 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5932 | ` * on these extension.` |
|       - |  5933 | ` */` |
|  112544 |  5934 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5935 | `{` |
|       - |  5936 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5937 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5938 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5939 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5940 | `	sxi32 rc;` |
|       - |  5941 |  |
|  112549 |  5942 | `	pIn = pGen->pIn;` |
|  112549 |  5943 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5944 | `	/* Process arguments one after one */` |
|  145478 |  5945 | `	for(;;){` |
|  290961 |  5946 | `		if( pIn >= pEnd ){` |
|       - |  5947 | `			/* No more arguments to process */` |
|  112533 |  5948 | `			break;` |
|       - |  5949 | `		}` |
|  178433 |  5950 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  178433 |  5951 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  178433 |  5952 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  178433 |  5953 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5954 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  5955 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  5956 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  5957 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  5958 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  5959 | `		{` |
|  178433 |  5960 | `			int bReadonly = 0, bVisSeen = 0;` |
|  178433 |  5961 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  178433 |  5962 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  5963 | `				bReadonly = 1;` |
|       3 |  5964 | `				pIn++;` |
|       1 |  5965 | `			}` |
|  178433 |  5966 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   69213 |  5967 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   69213 |  5968 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      79 |  5969 | `					bVisSeen = 1;` |
|      79 |  5970 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|     105 |  5971 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      34 |  5972 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      79 |  5973 | `					pIn++;` |
|      79 |  5974 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      18 |  5975 | `						bReadonly = 1;` |
|      18 |  5976 | `						pIn++;` |
|       7 |  5977 | `					}` |
|      37 |  5978 | `				}` |
|   34604 |  5979 | `			}` |
|  178433 |  5980 | `			if( bVisSeen \|\| bReadonly ){` |
|      81 |  5981 | `				if( !bCtorCtx ){` |
|       6 |  5982 | `					if( bAbstractCtx ){` |
|       3 |  5983 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5984 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5985 | `					}else{` |
|       3 |  5986 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5987 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5988 | `					}` |
|       6 |  5989 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5990 | `						return SXERR_ABORT;` |
|       - |  5991 | `					}` |
|       6 |  5992 | `					return SXERR_SYNTAX;` |
|       - |  5993 | `				}` |
|      77 |  5994 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      77 |  5995 | `				sArg.iPromoteVis = iVis;` |
|      77 |  5996 | `				if( bReadonly ){` |
|      20 |  5997 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       8 |  5998 | `				}` |
|      36 |  5999 | `			}` |
|       - |  6000 | `		}` |
|       - |  6001 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  222659 |  6002 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  135374 |  6003 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   90402 |  6004 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   84617 |  6005 | `			sxu32 nLineLocal = pIn->nLine;` |
|   84617 |  6006 | `			sxi32 iTFlags = 0;` |
|   84617 |  6007 | `			pGen->pIn = pIn;` |
|   84617 |  6008 | `			rc = GenStateParseUnionTypeDecl(` |
|   42306 |  6009 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   42306 |  6010 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  6011 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  6012 | `				/* bAllowVoid */ 0,` |
|   42306 |  6013 | `						nLineLocal);` |
|   84617 |  6014 | `			pIn = pGen->pIn;` |
|   84617 |  6015 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6016 | `				return SXERR_ABORT;` |
|   84617 |  6017 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  6018 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  6019 | `				return SXERR_SYNTAX;` |
|   84615 |  6020 | `			}else if( rc == SXERR_SYNTAX ){` |
|      12 |  6021 | `				if( pIn < pEnd ){` |
|      16 |  6022 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  6023 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       4 |  6024 | `						&pIn->sData);` |
|       8 |  6025 | `				}else{` |
|     ! 0 |  6026 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  6027 | `						"syntax error, unexpected end of file");` |
|       - |  6028 | `				}` |
|      12 |  6029 | `				return SXERR_SYNTAX;` |
|       - |  6030 | `			}` |
|   84607 |  6031 | `			sArg.iFlags \|= iTFlags;` |
|   42301 |  6032 | `		}` |
|  178419 |  6033 | `		if( pIn >= pEnd ){` |
|     ! 0 |  6034 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  6035 | `			return rc;` |
|       - |  6036 | `		}` |
|  178419 |  6037 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  6038 | `			/* Pass by reference,record that */` |
|    3863 |  6039 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3863 |  6040 | `			pIn++;` |
|    1929 |  6041 | `		}` |
|  178419 |  6042 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  6043 | `			/* Variadic parameter: ...$args */` |
|    3883 |  6044 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    3883 |  6045 | `			pIn++;` |
|    1939 |  6046 | `		}` |
|  178419 |  6047 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6048 | `			/* Invalid argument */` |
|     ! 0 |  6049 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  6050 | `			return rc;` |
|       - |  6051 | `		}` |
|  178419 |  6052 | `		pIn++; /* Jump the dollar sign */` |
|       - |  6053 | `		/* Copy argument name */` |
|  178419 |  6054 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  178419 |  6055 | `		if( zDup == 0 ){` |
|     ! 0 |  6056 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  6057 | `			return SXERR_ABORT;` |
|       - |  6058 | `		}` |
|  178419 |  6059 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  178419 |  6060 | `		pIn++;` |
|  178419 |  6061 | `		if( pIn < pEnd ){` |
|  108063 |  6062 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  6063 | `				SyToken *pDefend;` |
|   80469 |  6064 | `				sxi32 iNest = 0;` |
|   80469 |  6065 | `				pIn++; /* Jump the equal sign */` |
|   80469 |  6066 | `				pDefend = pIn;` |
|       - |  6067 | `				/* Process the default value associated with this argument */` |
|  168601 |  6068 | `				while( pDefend < pEnd ){` |
|  126433 |  6069 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   38301 |  6070 | `						break;` |
|       - |  6071 | `					}` |
|   88137 |  6072 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  6073 | `						/* Increment nesting level */` |
|    3839 |  6074 | `						iNest++;` |
|   86220 |  6075 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  6076 | `						/* Decrement nesting level */` |
|    3839 |  6077 | `						iNest--;` |
|    1917 |  6078 | `					}` |
|   88137 |  6079 | `					pDefend++;` |
|       5 |  6080 | `				}` |
|   80469 |  6081 | `				if( pIn >= pDefend ){` |
|       3 |  6082 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  6083 | `					return rc;` |
|       - |  6084 | `				}` |
|       - |  6085 | `				/* Process default value */` |
|   80467 |  6086 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   80467 |  6087 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  6088 | `					return rc;` |
|       - |  6089 | `				}` |
|       - |  6090 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|       - |  6091 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|       - |  6092 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|       - |  6093 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|       - |  6094 | `				 * arg-type check lets null through. */` |
|   88121 |  6095 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|   63212 |  6096 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|   63211 |  6097 | `					&& &pIn[1] == pDefend` |
|   44046 |  6098 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|   34467 |  6099 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|   21065 |  6100 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|   15323 |  6101 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|    7659 |  6102 | `				}` |
|       - |  6103 | `				/* Point beyond the default value */` |
|   80467 |  6104 | `				pIn = pDefend;` |
|   40231 |  6105 | `			}` |
|  108061 |  6106 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  6107 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  6108 | `				return rc;` |
|       - |  6109 | `			}` |
|  108061 |  6110 | `			pIn++; /* Jump the trailing comma */` |
|   54028 |  6111 | `		}` |
|       - |  6112 | `		/* Append argument signature */` |
|  178417 |  6113 | `		if( sArg.nType > 0 ){` |
|   84551 |  6114 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  6115 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|   15383 |  6116 | `				int marker = 'o';` |
|   15383 |  6117 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|   15383 |  6118 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    7694 |  6119 | `			}else{` |
|       - |  6120 | `				int c;` |
|   69173 |  6121 | `				c = 'n'; /* cc warning */` |
|       - |  6122 | `				/* Type leading character */` |
|   69173 |  6123 | `				switch(sArg.nType){` |
|       4 |  6124 | `				case MEMOBJ_HASHMAP:` |
|       - |  6125 | `					/* Hashmap aka 'array' */` |
|       9 |  6126 | `					c = 'h';` |
|       9 |  6127 | `					break;` |
|    9657 |  6128 | `				case MEMOBJ_INT:` |
|       - |  6129 | `					/* Integer */` |
|   19319 |  6130 | `					c = 'i';` |
|   19319 |  6131 | `					break;` |
|       2 |  6132 | `				case MEMOBJ_BOOL:` |
|       - |  6133 | `					/* Bool */` |
|       5 |  6134 | `					c = 'b';` |
|       5 |  6135 | `					break;` |
|       5 |  6136 | `				case MEMOBJ_REAL:` |
|       - |  6137 | `					/* Float */` |
|      12 |  6138 | `					c = 'f';` |
|      12 |  6139 | `					break;` |
|   24908 |  6140 | `				case MEMOBJ_STRING:` |
|       - |  6141 | `					/* String */` |
|   49821 |  6142 | `					c = 's';` |
|   49821 |  6143 | `					break;` |
|       7 |  6144 | `				case MEMOBJ_OBJ:` |
|       - |  6145 | `					/* Object */` |
|      16 |  6146 | `					c = 'o';` |
|      14 |  6147 | `					break;` |
|       1 |  6148 | `				default:` |
|       2 |  6149 | `					break;` |
|       - |  6150 | `				}` |
|   69173 |  6151 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  6152 | `			}` |
|   42278 |  6153 | `		}else{` |
|       - |  6154 | `			/* No type is associated with this parameter which mean` |
|       - |  6155 | `			 * that this function is not condidate for overloading.` |
|       - |  6156 | `			 */` |
|   93871 |  6157 | `			SyBlobRelease(&sSig);` |
|       - |  6158 | `		}` |
|       - |  6159 | `		/* Save in the argument set */` |
|  178417 |  6160 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  6161 | `	}` |
|  112533 |  6162 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  6163 | `		/* Save function signature */` |
|   53877 |  6164 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   26936 |  6165 | `	}` |
|  112533 |  6166 | `	return SXRET_OK;` |
|   56277 |  6167 | `}` |
|       - |  6168 | `/*` |
|       - |  6169 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|       - |  6170 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|       - |  6171 | ` * the enclosing function. Returns the token just past the nested construct.` |
|       - |  6172 | ` */` |
|      20 |  6173 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|       2 |  6174 | `{` |
|      22 |  6175 | `	sxi32 iParen = 0;` |
|      22 |  6176 | `	pIn++; /* past 'function'/'fn' */` |
|       - |  6177 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|       - |  6178 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|       - |  6179 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|      82 |  6180 | `	while( pIn < pEnd ){` |
|      82 |  6181 | `		sxu32 t = pIn->nType;` |
|      82 |  6182 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|      62 |  6183 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|      42 |  6184 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|      22 |  6185 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|      62 |  6186 | `		pIn++;` |
|       2 |  6187 | `	}` |
|      22 |  6188 | `	if( pIn >= pEnd ){ return pIn; }` |
|       - |  6189 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|       - |  6190 | `	{` |
|      22 |  6191 | `		sxi32 d = 0;` |
|     210 |  6192 | `		while( pIn < pEnd ){` |
|     210 |  6193 | `			sxu32 t = pIn->nType;` |
|     210 |  6194 | `			if( t & PH7_TK_OCB ){ d++; }` |
|     186 |  6195 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|     190 |  6196 | `			pIn++;` |
|       2 |  6197 | `		}` |
|       - |  6198 | `	}` |
|      22 |  6199 | `	return pIn;` |
|      12 |  6200 | `}` |
|       - |  6201 | `/*` |
|       - |  6202 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|       - |  6203 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|       - |  6204 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|       - |  6205 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|       - |  6206 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|       - |  6207 | ` * detached-mini-program path untouched.` |
|       - |  6208 | ` */` |
|       - |  6209 | `/*` |
|       - |  6210 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|       - |  6211 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|       - |  6212 | ` * mixed, object.` |
|       - |  6213 | ` */` |
|      28 |  6214 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|       3 |  6215 | `{` |
|       - |  6216 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|       - |  6217 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|       - |  6218 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|       - |  6219 | `	};` |
|       - |  6220 | `	sxu32 i;` |
|      31 |  6221 | `	if( nName > 0 && zName[0] == '\\' ){` |
|     ! 0 |  6222 | `		zName++;` |
|     ! 0 |  6223 | `		nName--;` |
|     ! 0 |  6224 | `	}` |
|      63 |  6225 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|      59 |  6226 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|      27 |  6227 | `			return 1;` |
|       - |  6228 | `		}` |
|      17 |  6229 | `	}` |
|       5 |  6230 | `	return 0;` |
|      17 |  6231 | `}` |
|       - |  6232 | `/*` |
|       - |  6233 | ` * One atom of a generator's declared return type: is it a supertype of` |
|       - |  6234 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|       - |  6235 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|       - |  6236 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|       - |  6237 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|       - |  6238 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|       - |  6239 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|       - |  6240 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|       - |  6241 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|       - |  6242 | ` * both rather than fatal on valid code (divergence recorded in PLAN.md).` |
|       - |  6243 | ` */` |
|      26 |  6244 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|       4 |  6245 | `{` |
|      30 |  6246 | `	if( nType == MEMOBJ_OBJ ){` |
|     ! 0 |  6247 | ``		return 1; /* bare `object` */`` |
|       - |  6248 | `	}` |
|      30 |  6249 | `	if( nType != SXU32_HIGH ){` |
|       3 |  6250 | `		return 0; /* scalar/array/void/never/null/... */` |
|       - |  6251 | `	}` |
|      27 |  6252 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|      23 |  6253 | `		return 1;` |
|       - |  6254 | `	}` |
|       - |  6255 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|       - |  6256 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|       - |  6257 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|       - |  6258 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|       - |  6259 | `	{` |
|       - |  6260 | `		SyBlob sFQN;` |
|       - |  6261 | `		int bOk;` |
|       5 |  6262 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       5 |  6263 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|       5 |  6264 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|       5 |  6265 | `		SyBlobRelease(&sFQN);` |
|       5 |  6266 | `		return bOk;` |
|       - |  6267 | `	}` |
|      17 |  6268 | `}` |
|       - |  6269 | `/*` |
|       - |  6270 | ` * php 8: a generator function may only declare a return type that is a` |
|       - |  6271 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|       - |  6272 | ` * group qualifies only if every member does. Anything else is php's exact` |
|       - |  6273 | ` * compile-time fatal "Generator return type must be a supertype of` |
|       - |  6274 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|       - |  6275 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|       - |  6276 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|       - |  6277 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|       - |  6278 | ` */` |
|     212 |  6279 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|       5 |  6280 | `{` |
|     217 |  6281 | `	int bOk = 0;` |
|       - |  6282 | `	sxu32 nLine;` |
|       - |  6283 | `	sxi32 rc;` |
|     217 |  6284 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|     191 |  6285 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|       - |  6286 | `	}` |
|      30 |  6287 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|     ! 0 |  6288 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|     ! 0 |  6289 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|       - |  6290 | `		sxu32 i,j;` |
|     ! 0 |  6291 | `		for( i = 0; i < n && !bOk; i++ ){` |
|       - |  6292 | `			int bGroupOk;` |
|     ! 0 |  6293 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|     ! 0 |  6294 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|       - |  6295 | `			}` |
|     ! 0 |  6296 | `			bGroupOk = 1;` |
|     ! 0 |  6297 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|     ! 0 |  6298 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|     ! 0 |  6299 | `					bGroupOk = 0;` |
|     ! 0 |  6300 | `					break;` |
|       - |  6301 | `				}` |
|     ! 0 |  6302 | `			}` |
|     ! 0 |  6303 | `			bOk = bGroupOk;` |
|     ! 0 |  6304 | `		}` |
|     ! 0 |  6305 | `	}else{` |
|      30 |  6306 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|       - |  6307 | `	}` |
|      30 |  6308 | `	if( bOk ){` |
|      27 |  6309 | `		return SXRET_OK;` |
|       - |  6310 | `	}` |
|       - |  6311 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|       - |  6312 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|       - |  6313 | `	 * token of this stream — its line is the function's closing brace. php` |
|       - |  6314 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|       - |  6315 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|       3 |  6316 | `	nLine = pGen->pIn[-1].nLine;` |
|       - |  6317 | `	{` |
|       3 |  6318 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|       3 |  6319 | `		if( sGiven.nByte < 1 ){` |
|     ! 0 |  6320 | `			sGiven = pFunc->sReturnClass;` |
|     ! 0 |  6321 | `		}` |
|       3 |  6322 | `		if( sGiven.nByte < 1 ){` |
|       - |  6323 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|       - |  6324 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|       - |  6325 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|     ! 0 |  6326 | `			const char *zScalar =` |
|     ! 0 |  6327 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|     ! 0 |  6328 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|     ! 0 |  6329 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|     ! 0 |  6330 | `		}` |
|       3 |  6331 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  6332 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|       - |  6333 | `	}` |
|       3 |  6334 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|     111 |  6335 | `}` |
|  240082 |  6336 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|       5 |  6337 | `{` |
|  240087 |  6338 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|  240087 |  6339 | `	SyToken *pEnd = pGen->pEnd;` |
|  240087 |  6340 | `	sxi32 iDepth = 0;` |
|  240087 |  6341 | `	int bStarted = 0;` |
| 7971841 |  6342 | `	while( pIn < pEnd ){` |
| 7971841 |  6343 | `		sxu32 t = pIn->nType;` |
| 7971841 |  6344 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 7512655 |  6345 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 7053785 |  6346 | `		if( t & PH7_TK_KEYWORD ){` |
|  559549 |  6347 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  559549 |  6348 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  559337 |  6349 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|       - |  6350 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|  279656 |  6351 | `		}` |
| 7053553 |  6352 | `		pIn++;` |
|       5 |  6353 | `	}` |
|  239875 |  6354 | `	return FALSE;` |
|  120046 |  6355 | `}` |
|       - |  6356 | `/*` |
|       - |  6357 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  6358 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  6359 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  6360 | ` */` |
|  240082 |  6361 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  6362 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  6363 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  6364 | `	)` |
|       5 |  6365 | `{` |
|       - |  6366 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  6367 | `	GenBlock *pBlock;` |
|       - |  6368 | `	sxu32 nGotoOfft;` |
|       - |  6369 | `	sxi32 rc;` |
|       - |  6370 | `	/* Attach the new function */` |
|  240087 |  6371 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  240087 |  6372 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6373 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  6374 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6375 | `		return SXERR_ABORT;` |
|       - |  6376 | `	}` |
|  240087 |  6377 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  6378 | `	/* Swap bytecode containers */` |
|  240087 |  6379 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  240087 |  6380 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  6381 | `	/* Emit constructor property promotion prologue:` |
|       - |  6382 | `	 *   $this->NAME = $NAME;` |
|       - |  6383 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  6384 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  6385 | `	{` |
|  240087 |  6386 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  6387 | `		sxu32 i;` |
|  387725 |  6388 | `		for( i = 0; i < nArg; i++ ){` |
|  147643 |  6389 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  6390 | `			char *zSrc;` |
|       - |  6391 | `			sxu32 nSrc,nName;` |
|       - |  6392 | `			SySet sToken;` |
|       - |  6393 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  6394 | `			sxi32 rcPromote;` |
|  147643 |  6395 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  147581 |  6396 | `				continue;` |
|       - |  6397 | `			}` |
|       - |  6398 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  6399 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  6400 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  6401 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  6402 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      67 |  6403 | `			nName = SyStringLength(&pArg->sName);` |
|      67 |  6404 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      67 |  6405 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      67 |  6406 | `			if( zSrc == 0 ){` |
|     ! 0 |  6407 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6408 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6409 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  6410 | `				return SXERR_ABORT;` |
|       - |  6411 | `			}` |
|       - |  6412 | `			{` |
|      67 |  6413 | `				char *z = zSrc;` |
|      67 |  6414 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      67 |  6415 | `				z += sizeof("$this->")-1;` |
|      67 |  6416 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      67 |  6417 | `				z += nName;` |
|      67 |  6418 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      67 |  6419 | `				z += sizeof(" = $")-1;` |
|      67 |  6420 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      67 |  6421 | `				z += nName;` |
|      67 |  6422 | `				*z = 0;` |
|       - |  6423 | `			}` |
|      67 |  6424 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      67 |  6425 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      67 |  6426 | `			pTmpIn = pGen->pIn;` |
|      67 |  6427 | `			pTmpEnd = pGen->pEnd;` |
|      67 |  6428 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      67 |  6429 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      67 |  6430 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      67 |  6431 | `			pGen->pIn = pTmpIn;` |
|      67 |  6432 | `			pGen->pEnd = pTmpEnd;` |
|      67 |  6433 | `			SySetRelease(&sToken);` |
|      67 |  6434 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  6435 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6436 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6437 | `				return SXERR_ABORT;` |
|       - |  6438 | `			}` |
|       - |  6439 | `			/* Discard the assignment result — this is a statement expression. */` |
|      67 |  6440 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      36 |  6441 | `		}` |
|       - |  6442 | `	}` |
|       - |  6443 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|       - |  6444 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|       - |  6445 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|       - |  6446 | `	 * generator — and vice versa — is classified independently. */` |
|       - |  6447 | `	{` |
|  240087 |  6448 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|  240087 |  6449 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|       - |  6450 | `		/* Compile the body */` |
|  240087 |  6451 | `		PH7_CompileBlock(&(*pGen),0);` |
|  240087 |  6452 | `		pGen->bInGenerator = bSavedGen;` |
|       - |  6453 | `	}` |
|       - |  6454 | `	/* Fix exception jumps now the destination is resolved */` |
|  240087 |  6455 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  6456 | `	/* Emit the final return if not yet done */` |
|  240087 |  6457 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  6458 | `	/* Fix gotos jumps now the destination is resolved */` |
|  240087 |  6459 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  6460 | `		rc = SXERR_ABORT;` |
|     ! 0 |  6461 | `	}` |
|  240087 |  6462 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  6463 | `	/* Restore the default container */` |
|  240087 |  6464 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  6465 | `	/* Leave function block */` |
|  240087 |  6466 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  240087 |  6467 | `	if( rc == SXERR_ABORT ){` |
|       - |  6468 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6469 | `		return SXERR_ABORT;` |
|       - |  6470 | `	}` |
|       - |  6471 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  6472 | `	{` |
|  240087 |  6473 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  6474 | `		sxu32 i;` |
| 4714131 |  6475 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 4474261 |  6476 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     217 |  6477 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     217 |  6478 | `				break;` |
|       - |  6479 | `			}` |
| 2237027 |  6480 | `		}` |
|       - |  6481 | `	}` |
|  240087 |  6482 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|       - |  6483 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     217 |  6484 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|     ! 0 |  6485 | `			return SXERR_ABORT;` |
|       - |  6486 | `		}` |
|     106 |  6487 | `	}` |
|       - |  6488 | `	/* All done, function body compiled */` |
|  240087 |  6489 | `	return SXRET_OK;` |
|  120046 |  6490 | `}` |
|       - |  6491 | `/*` |
|       - |  6492 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  6493 | ` * According to the PHP language reference manual.` |
|       - |  6494 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  6495 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  6496 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  6497 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  6498 | ` *  Functions need not be defined before they are referenced.` |
|       - |  6499 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  6500 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  6501 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  6502 | ` *  calls with over 32-64 recursion levels.` |
|       - |  6503 | ` *` |
|       - |  6504 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  6505 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  6506 | ` * on these extension.` |
|       - |  6507 | ` */` |
|       - |  6508 | `/*` |
|       - |  6509 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  6510 | ` */` |
|     532 |  6511 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  6512 | `{` |
|       - |  6513 | `	sxu32 i;` |
|    1507 |  6514 | `	for( i = 0; i < n; i++ ){` |
|    1293 |  6515 | `		int a = zA[i], b = zB[i];` |
|    1293 |  6516 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|    1293 |  6517 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|    1293 |  6518 | `		if( a != b ) return a - b;` |
|     490 |  6519 | `	}` |
|     219 |  6520 | `	return 0;` |
|     271 |  6521 | `}` |
|       - |  6522 | `/*` |
|       - |  6523 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  6524 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  6525 | ` * (which are positive bit values stored in sxu32).` |
|       - |  6526 | ` */` |
|       - |  6527 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  6528 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  6529 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  6530 |  |
|       - |  6531 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|       - |  6532 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|       - |  6533 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|       - |  6534 |  |
|       - |  6535 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  6536 | `struct PhlTypeAtom {` |
|       - |  6537 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  6538 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  6539 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  6540 | `	sxu32 nCanon;` |
|       - |  6541 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|       - |  6542 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|       - |  6543 | `};` |
|       - |  6544 |  |
|       - |  6545 | `/*` |
|       - |  6546 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  6547 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  6548 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  6549 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  6550 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  6551 | ` * already be consumed by the caller.` |
|       - |  6552 | ` */` |
|   85572 |  6553 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  6554 | `{` |
|   85577 |  6555 | `	SyToken *pIn = pGen->pIn;` |
|   85577 |  6556 | `	SyZero(pOut, sizeof(*pOut));` |
|   85577 |  6557 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   85577 |  6558 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6559 | `		return SXERR_SYNTAX;` |
|       - |  6560 | `	}` |
|       - |  6561 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   85577 |  6562 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  6563 | `		pIn++;` |
|       8 |  6564 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6565 | `			return SXERR_SYNTAX;` |
|       - |  6566 | `		}` |
|       3 |  6567 | `	}` |
|   85577 |  6568 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6569 | `		return SXERR_SYNTAX;` |
|       - |  6570 | `	}` |
|   85577 |  6571 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   69773 |  6572 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   69773 |  6573 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      34 |  6574 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   69758 |  6575 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      75 |  6576 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   69708 |  6577 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   19605 |  6578 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   59873 |  6579 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   49995 |  6580 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   25078 |  6581 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      39 |  6582 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      65 |  6583 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  6584 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      35 |  6585 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|      12 |  6586 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      21 |  6587 | `			pOut->nType = SXU32_HIGH;` |
|      21 |  6588 | `			pOut->sClass = pIn->sData;` |
|      12 |  6589 | `		}else{` |
|       3 |  6590 | `			return SXERR_SYNTAX;` |
|       - |  6591 | `		}` |
|   69771 |  6592 | `		pIn++;` |
|   34888 |  6593 | `	}else{` |
|       - |  6594 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6595 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   15809 |  6596 | `		SyString *pT = &pIn->sData;` |
|   15809 |  6597 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      32 |  6598 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      32 |  6599 | `			pIn++;` |
|   15795 |  6600 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     165 |  6601 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     165 |  6602 | `			pIn++;` |
|   15701 |  6603 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|      24 |  6604 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|      24 |  6605 | `			pIn++;` |
|      14 |  6606 | `		}else{` |
|       - |  6607 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   15601 |  6608 | `			SyToken *pFirst = pIn;` |
|   15601 |  6609 | `			SyToken *pLast = pIn;` |
|   15601 |  6610 | `			pOut->nType = SXU32_HIGH;` |
|   15601 |  6611 | `			pOut->sClass = pIn->sData;` |
|   15601 |  6612 | `			pIn++;` |
|   23397 |  6613 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   15604 |  6614 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6615 | `				pLast = &pIn[1];` |
|       3 |  6616 | `				pIn += 2;` |
|       1 |  6617 | `			}` |
|   15601 |  6618 | `			if( pLast != pFirst ){` |
|       3 |  6619 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6620 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6621 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6622 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6623 | `			}` |
|       - |  6624 | `		}` |
|       - |  6625 | `	}` |
|   85575 |  6626 | `	pGen->pIn = pIn;` |
|   85575 |  6627 | `	return SXRET_OK;` |
|   42791 |  6628 | `}` |
|       - |  6629 |  |
|       - |  6630 | `/*` |
|       - |  6631 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6632 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6633 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6634 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6635 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6636 | ` */` |
|   85406 |  6637 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6638 | `{` |
|       - |  6639 | `	int i;` |
|   85411 |  6640 | `	int nNonNull = 0;` |
|   85411 |  6641 | `	int bAnyIntersection = 0;` |
|       - |  6642 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   85411 |  6643 | `	sxu32 nMaxGroup = 0;` |
| 2818403 |  6644 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  170957 |  6645 | `	for( i = 0; i < nAtoms; i++ ){` |
|   85551 |  6646 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   85523 |  6647 | `			nNonNull++;` |
|   85523 |  6648 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   85523 |  6649 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   85523 |  6650 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   42759 |  6651 | `			}` |
|   42759 |  6652 | `		}` |
|   42778 |  6653 | `	}` |
|  170915 |  6654 | `	for( i = 0; i < nAtoms; i++ ){` |
|   85529 |  6655 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      24 |  6656 | `			bAnyIntersection = 1;` |
|      24 |  6657 | `			break;` |
|       - |  6658 | `		}` |
|   42757 |  6659 | `	}` |
|   85411 |  6660 | `	if( bAnyIntersection ){` |
|       - |  6661 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|       - |  6662 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|       - |  6663 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|      24 |  6664 | `		sxu32 g, nGroups = 0;` |
|      24 |  6665 | `		int bFirstGroup = 1;` |
|      48 |  6666 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|      48 |  6667 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|      28 |  6668 | `			int bFirstMember = 1;` |
|       - |  6669 | `			int bWrap;` |
|      28 |  6670 | `			if( aGroupCount[g] == 0 ) continue;` |
|       - |  6671 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|       - |  6672 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|       - |  6673 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|       - |  6674 | `			 * parens, matching PHP's canonical text. */` |
|      38 |  6675 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|      28 |  6676 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|      28 |  6677 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      84 |  6678 | `			for( i = 0; i < nAtoms; i++ ){` |
|      60 |  6679 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|      48 |  6680 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|      48 |  6681 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      46 |  6682 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      25 |  6683 | `				}else{` |
|       3 |  6684 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6685 | `				}` |
|      48 |  6686 | `				bFirstMember = 0;` |
|      26 |  6687 | `			}` |
|      28 |  6688 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|      28 |  6689 | `			bFirstGroup = 0;` |
|      16 |  6690 | `		}` |
|      24 |  6691 | `		if( bNullable ){` |
|     ! 0 |  6692 | `			SyBlobAppend(pBlob, "\|", 1);` |
|     ! 0 |  6693 | `			SyBlobAppend(pBlob, "null", 4);` |
|     ! 0 |  6694 | `		}` |
|      64 |  6695 | `		return;` |
|       - |  6696 | `	}` |
|   85391 |  6697 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6698 | `		/* Shorthand: ?T */` |
|      85 |  6699 | `		for( i = 0; i < nAtoms; i++ ){` |
|      85 |  6700 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      85 |  6701 | `			SyBlobAppend(pBlob, "?", 1);` |
|      85 |  6702 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      21 |  6703 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      12 |  6704 | `			}else{` |
|      67 |  6705 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6706 | `			}` |
|      85 |  6707 | `			return;` |
|     ! 0 |  6708 | `		}` |
|     ! 0 |  6709 | `	}` |
|       - |  6710 | `	{` |
|   85311 |  6711 | `		int bFirst = 1;` |
|       - |  6712 | `		/* 1) Classes in declaration order */` |
|  170721 |  6713 | `		for( i = 0; i < nAtoms; i++ ){` |
|   85415 |  6714 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   15559 |  6715 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   15559 |  6716 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   15559 |  6717 | `				bFirst = 0;` |
|    7777 |  6718 | `			}` |
|   42710 |  6719 | `		}` |
|       - |  6720 | `		/* 2) Built-ins in canonical order */` |
|       - |  6721 | `		{` |
|       - |  6722 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6723 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6724 | `			int k;` |
|  597147 |  6725 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  954525 |  6726 | `				for( i = 0; i < nAtoms; i++ ){` |
|  512357 |  6727 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   69673 |  6728 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   69673 |  6729 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   69673 |  6730 | `						bFirst = 0;` |
|   69673 |  6731 | `						break;` |
|       - |  6732 | `					}` |
|  221347 |  6733 | `				}` |
|  255923 |  6734 | `			}` |
|       - |  6735 | `		}` |
|       - |  6736 | `		/* 3) null suffix */` |
|   85311 |  6737 | `		if( bNullable ){` |
|      19 |  6738 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      19 |  6739 | `			SyBlobAppend(pBlob, "null", 4);` |
|       8 |  6740 | `		}` |
|       - |  6741 | `	}` |
|   42708 |  6742 | `}` |
|       - |  6743 |  |
|       - |  6744 | `/*` |
|       - |  6745 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|       - |  6746 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|       - |  6747 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|       - |  6748 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|       - |  6749 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|       - |  6750 | ` * whether it was parenthesized.` |
|       - |  6751 | ` *` |
|       - |  6752 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|       - |  6753 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|       - |  6754 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|       - |  6755 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|       - |  6756 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|       - |  6757 | ` */` |
|   85550 |  6758 | `static sxi32 GenStateParsePart(` |
|       - |  6759 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|       - |  6760 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|       5 |  6761 | `{` |
|       - |  6762 | `	sxi32 rc;` |
|   85555 |  6763 | `	int nMembers = 0;` |
|   85555 |  6764 | `	int bParen = 0;` |
|   85555 |  6765 | `	*pnMembers = 0;` |
|   85555 |  6766 | `	*pbParen = 0;` |
|   85555 |  6767 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       6 |  6768 | `		bParen = 1;` |
|       6 |  6769 | `		pGen->pIn++; /* skip '(' */` |
|       2 |  6770 | `	}` |
|   42775 |  6771 | `	for(;;){` |
|   85577 |  6772 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6773 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6774 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6775 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6776 | `		}` |
|   85577 |  6777 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   85577 |  6778 | `		if( rc != SXRET_OK ){` |
|       3 |  6779 | `			return rc;` |
|       - |  6780 | `		}` |
|   85575 |  6781 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   85575 |  6782 | `		(*pnAtoms)++;` |
|   85575 |  6783 | `		nMembers++;` |
|       - |  6784 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   85575 |  6785 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      30 |  6786 | `			SyToken *pNext = &pGen->pIn[1];` |
|      26 |  6787 | `			if( pNext < pGen->pEnd` |
|      30 |  6788 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      26 |  6789 | `				pGen->pIn++; /* skip '&' */` |
|      26 |  6790 | `				continue;` |
|       - |  6791 | `			}` |
|       2 |  6792 | `		}` |
|   85553 |  6793 | `		break;` |
|     ! 0 |  6794 | `	}` |
|   85553 |  6795 | `	if( bParen ){` |
|       6 |  6796 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6797 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6798 | `				"Malformed DNF type: expecting ')'");` |
|     ! 0 |  6799 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6800 | `		}` |
|       6 |  6801 | `		pGen->pIn++; /* skip ')' */` |
|       6 |  6802 | `		if( nMembers < 2 ){` |
|     ! 0 |  6803 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6804 | `				"Parenthesized type must be an intersection of at least two types");` |
|     ! 0 |  6805 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6806 | `		}` |
|       2 |  6807 | `	}` |
|   85553 |  6808 | `	*pnMembers = nMembers;` |
|   85553 |  6809 | `	*pbParen = bParen;` |
|   85553 |  6810 | `	return SXRET_OK;` |
|   42780 |  6811 | `}` |
|       - |  6812 |  |
|       - |  6813 | `/*` |
|       - |  6814 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6815 | ` *` |
|       - |  6816 | ` * Outputs:` |
|       - |  6817 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6818 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6819 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6820 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6821 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6822 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6823 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6824 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6825 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6826 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6827 | ` *` |
|       - |  6828 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6829 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6830 | ` */` |
|   85422 |  6831 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6832 | `	ph7_gen_state *pGen,` |
|       - |  6833 | `	sxu32 *pnType,` |
|       - |  6834 | `	SyString *pClass,` |
|       - |  6835 | `	SySet *pAlts,` |
|       - |  6836 | `	sxi32 *piTypeFlags,` |
|       - |  6837 | `	SyString *pTypeText,` |
|       - |  6838 | `	int iNullableFlag,` |
|       - |  6839 | `	int iUnionFlag,` |
|       - |  6840 | `	int bAllowVoid,` |
|       - |  6841 | `	sxu32 nLine` |
|       5 |  6842 | `){` |
|       - |  6843 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   85427 |  6844 | `	int nAtoms = 0;` |
|   85427 |  6845 | `	int bShortNullable = 0;` |
|   85427 |  6846 | `	int bExplicitNull = 0;` |
|       - |  6847 | `	sxi32 rc;` |
|   85427 |  6848 | `	*pnType = 0;` |
|   85427 |  6849 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   85427 |  6850 | `	*piTypeFlags = 0;` |
|   85427 |  6851 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6852 |  |
|   85427 |  6853 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6854 | `		return SXRET_OK;` |
|       - |  6855 | `	}` |
|       - |  6856 | ``	/* Optional `?` shorthand prefix */`` |
|   85422 |  6857 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      75 |  6858 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      75 |  6859 | `		bShortNullable = 1;` |
|      75 |  6860 | `		pGen->pIn++;` |
|      75 |  6861 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6862 | `			return SXERR_SYNTAX;` |
|       - |  6863 | `		}` |
|      35 |  6864 | `	}` |
|       - |  6865 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|       - |  6866 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|       - |  6867 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|       - |  6868 | `	{` |
|       - |  6869 | `		int nMembers, bParen;` |
|   85427 |  6870 | `		sxu32 iGroup = 0;` |
|   85427 |  6871 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   85427 |  6872 | `		if( rc != SXRET_OK ){` |
|       4 |  6873 | `			return rc;` |
|       - |  6874 | `		}` |
|       - |  6875 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|       - |  6876 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|       - |  6877 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|       - |  6878 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|       - |  6879 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|  128325 |  6880 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   85620 |  6881 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     135 |  6882 | `			if( bShortNullable ){` |
|       - |  6883 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6884 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6885 | `				 * already reported" so callers skip their own error emission. */` |
|       3 |  6886 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6887 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6888 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6889 | `			}` |
|     133 |  6890 | `			if( nMembers >= 2 && !bParen ){` |
|     ! 0 |  6891 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|       - |  6892 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6893 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6894 | `			}` |
|     133 |  6895 | ``			pGen->pIn++; /* skip `\|` */`` |
|     133 |  6896 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|     133 |  6897 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  6898 | `				return rc;` |
|       - |  6899 | `			}` |
|       5 |  6900 | `		}` |
|   85423 |  6901 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|     ! 0 |  6902 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6903 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6904 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6905 | `		}` |
|       - |  6906 | `	}` |
|       - |  6907 | `	/* Validation pass.` |
|       - |  6908 | `	 *` |
|       - |  6909 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6910 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6911 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6912 | `	 */` |
|       - |  6913 | `	{` |
|       - |  6914 | `		int i, j;` |
|   85423 |  6915 | `		int bHasNonNull = 0;` |
|   85423 |  6916 | `		int bAnyIntersection = 0;` |
|       - |  6917 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|       - |  6918 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|       - |  6919 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 2818799 |  6920 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  170991 |  6921 | `		for( i = 0; i < nAtoms; i++ ){` |
|   85573 |  6922 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   42789 |  6923 | `		}` |
|  170945 |  6924 | `		for( i = 0; i < nAtoms; i++ ){` |
|   85549 |  6925 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   42766 |  6926 | `		}` |
|       - |  6927 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|       - |  6928 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   85423 |  6929 | `		if( bShortNullable && bAnyIntersection ){` |
|     ! 0 |  6930 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6931 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|     ! 0 |  6932 | `			return SXERR_SYNTAX;` |
|       - |  6933 | `		}` |
|  170977 |  6934 | `		for( i = 0; i < nAtoms; i++ ){` |
|       - |  6935 | `			/* Intersection members must be class/interface types (PHP rejects` |
|       - |  6936 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|       - |  6937 | ``			 * `true`/`false` in an intersection). */`` |
|   85571 |  6938 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      46 |  6939 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|      46 |  6940 | `				if( bClassLike ){` |
|      44 |  6941 | `					SyString *pC = &aAtoms[i].sClass;` |
|      40 |  6942 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|      40 |  6943 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|      40 |  6944 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|      44 |  6945 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|     ! 0 |  6946 | `						bClassLike = 0;` |
|     ! 0 |  6947 | `					}` |
|      20 |  6948 | `				}` |
|      46 |  6949 | `				if( !bClassLike ){` |
|       - |  6950 | `					const char *zName; sxu32 nName;` |
|       3 |  6951 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6952 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6953 | `					}else{` |
|       3 |  6954 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|       - |  6955 | `					}` |
|       4 |  6956 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6957 | `						"Type %.*s cannot be part of an intersection type",` |
|       1 |  6958 | `						(int)nName, zName);` |
|       3 |  6959 | `					return SXERR_SYNTAX;` |
|       - |  6960 | `				}` |
|      20 |  6961 | `			}` |
|   85569 |  6962 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     165 |  6963 | `				if( nAtoms > 1 ){` |
|       3 |  6964 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6965 | `						"Void can only be used as a standalone type");` |
|       3 |  6966 | `					return SXERR_SYNTAX;` |
|       - |  6967 | `				}` |
|     163 |  6968 | `				if( !bAllowVoid ){` |
|     ! 0 |  6969 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6970 | `						"void cannot be used here");` |
|     ! 0 |  6971 | `					return SXERR_SYNTAX;` |
|       - |  6972 | `				}` |
|     163 |  6973 | `				if( bShortNullable ){` |
|     ! 0 |  6974 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6975 | `						"Void type cannot be nullable");` |
|     ! 0 |  6976 | `					return SXERR_SYNTAX;` |
|       - |  6977 | `				}` |
|      79 |  6978 | `			}` |
|   85567 |  6979 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6980 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|       - |  6981 | `				 * type (never = the function does not return). Mirrors the void` |
|       - |  6982 | `				 * validation above; accepted here and enforced at compile time` |
|       - |  6983 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|      24 |  6984 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|       - |  6985 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|       - |  6986 | `					 * same as any other non-standalone use. */` |
|       5 |  6987 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6988 | `						"never can only be used as a standalone type");` |
|       5 |  6989 | `					return SXERR_SYNTAX;` |
|       - |  6990 | `				}` |
|      19 |  6991 | `				if( !bAllowVoid ){` |
|       - |  6992 | `					/* Return-only: params call with bAllowVoid=0. */` |
|       3 |  6993 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6994 | `						"never cannot be used as a parameter type");` |
|       3 |  6995 | `					return SXERR_SYNTAX;` |
|       - |  6996 | `				}` |
|       7 |  6997 | `			}` |
|   85561 |  6998 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      32 |  6999 | `				bExplicitNull = 1;` |
|      18 |  7000 | `			}else{` |
|   85533 |  7001 | `				bHasNonNull = 1;` |
|       - |  7002 | `			}` |
|       - |  7003 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|       - |  7004 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|       - |  7005 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|       - |  7006 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|       - |  7007 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   85747 |  7008 | `			for( j = 0; j < i; j++ ){` |
|     193 |  7009 | `				int bDup = 0;` |
|     193 |  7010 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|     369 |  7011 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|     188 |  7012 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|     193 |  7013 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|     185 |  7014 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      47 |  7015 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      40 |  7016 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      42 |  7017 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|      17 |  7018 | `								aAtoms[j].sClass.zString,` |
|      34 |  7019 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  7020 | `							bDup = 1;` |
|     ! 0 |  7021 | `						}` |
|      25 |  7022 | `					}else{` |
|       3 |  7023 | `						bDup = 1;` |
|       - |  7024 | `					}` |
|      21 |  7025 | `				}` |
|     185 |  7026 | `				if( bDup ){` |
|       - |  7027 | `					const char *zName;` |
|       - |  7028 | `					sxu32 nName;` |
|       3 |  7029 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  7030 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  7031 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  7032 | `					}else{` |
|       3 |  7033 | `						zName = aAtoms[i].zCanon;` |
|       3 |  7034 | `						nName = aAtoms[i].nCanon;` |
|       - |  7035 | `					}` |
|       4 |  7036 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  7037 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  7038 | `					return SXERR_SYNTAX;` |
|       - |  7039 | `				}` |
|      94 |  7040 | `			}` |
|   42782 |  7041 | `		}` |
|   85411 |  7042 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  7043 | `			if( bShortNullable ){` |
|       - |  7044 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  7045 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  7046 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  7047 | `				return SXERR_SYNTAX;` |
|       - |  7048 | `			}` |
|       - |  7049 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  7050 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  7051 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  7052 | `			 * atom, so set it here. */` |
|       7 |  7053 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  7054 | `		}` |
|       - |  7055 | `	}` |
|       - |  7056 | `	/* Compute nullability flag */` |
|   85411 |  7057 | `	if( bShortNullable \|\| bExplicitNull ){` |
|     101 |  7058 | `		*piTypeFlags \|= iNullableFlag;` |
|      48 |  7059 | `	}` |
|       - |  7060 | `	/* Build canonical type text */` |
|   85411 |  7061 | `	if( pTypeText ){` |
|       - |  7062 | `		SyBlob sBlob;` |
|   85411 |  7063 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  128080 |  7064 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   42703 |  7065 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   85411 |  7066 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  127856 |  7067 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   85234 |  7068 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   85239 |  7069 | `			if( zDup ){` |
|   85239 |  7070 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   42617 |  7071 | `			}` |
|   42617 |  7072 | `		}` |
|   85411 |  7073 | `		SyBlobRelease(&sBlob);` |
|   42703 |  7074 | `	}` |
|       - |  7075 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  7076 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  7077 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  7078 | `	{` |
|   85411 |  7079 | `		int nNonNull = 0;` |
|   85411 |  7080 | `		int iNonNullIdx = -1;` |
|       - |  7081 | `		int i;` |
|  170957 |  7082 | `		for( i = 0; i < nAtoms; i++ ){` |
|   85551 |  7083 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   85523 |  7084 | `				nNonNull++;` |
|   85523 |  7085 | `				iNonNullIdx = i;` |
|   42759 |  7086 | `			}` |
|   42778 |  7087 | `		}` |
|   85411 |  7088 | `		if( nNonNull <= 1 ){` |
|       - |  7089 | `			/* Fast path: store as single type. */` |
|   85313 |  7090 | `			if( iNonNullIdx >= 0 ){` |
|   85307 |  7091 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   85307 |  7092 | `				if( pA->nType == SXU32_HIGH ){` |
|   23297 |  7093 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    7764 |  7094 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   15533 |  7095 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   15533 |  7096 | `					*pnType = SXU32_HIGH;` |
|   15533 |  7097 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   77543 |  7098 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     163 |  7099 | `					*pnType = MEMOBJ_VOID;` |
|   69700 |  7100 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|      16 |  7101 | `					*pnType = MEMOBJ_NEVER;` |
|       9 |  7102 | `				}else{` |
|   69607 |  7103 | `					*pnType = pA->nType;` |
|       - |  7104 | `				}` |
|   42651 |  7105 | `			}` |
|   42659 |  7106 | `		}else{` |
|       - |  7107 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|     103 |  7108 | `			*piTypeFlags \|= iUnionFlag;` |
|     329 |  7109 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  7110 | `				ph7_type_alt sAlt;` |
|     231 |  7111 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     221 |  7112 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     221 |  7113 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|     221 |  7114 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|     134 |  7115 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      43 |  7116 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      91 |  7117 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      91 |  7118 | `					sAlt.nType = SXU32_HIGH;` |
|      91 |  7119 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      48 |  7120 | `				}else{` |
|     135 |  7121 | `					sAlt.nType = aAtoms[i].nType;` |
|     135 |  7122 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  7123 | `				}` |
|     221 |  7124 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|     113 |  7125 | `			}` |
|       - |  7126 | `		}` |
|       - |  7127 | `	}` |
|   85411 |  7128 | `	return SXRET_OK;` |
|   42716 |  7129 | `}` |
|       - |  7130 |  |
|       - |  7131 | `/*` |
|       - |  7132 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  7133 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  7134 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  7135 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  7136 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  7137 | `` *          and union types `: T\|U`.`` |
|       - |  7138 | ` */` |
|  339868 |  7139 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  7140 | `{` |
|  339873 |  7141 | `	sxi32 iFlags = 0;` |
|       - |  7142 | `	sxi32 rc;` |
|       - |  7143 | `	sxu32 nLine;` |
|  339873 |  7144 | `	pFunc->nReturnType = 0;` |
|  339873 |  7145 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  339873 |  7146 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|       - |  7147 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|       - |  7148 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|       - |  7149 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|       - |  7150 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|       - |  7151 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|  339873 |  7152 | `	SySetReset(&pFunc->aReturnUnion);` |
|  339873 |  7153 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|  339873 |  7154 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  339287 |  7155 | `		return SXRET_OK;` |
|       - |  7156 | `	}` |
|     591 |  7157 | `	pGen->pIn++; /* Skip ':' */` |
|     591 |  7158 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7159 | `		return SXRET_OK;` |
|       - |  7160 | `	}` |
|     591 |  7161 | `	nLine = pGen->pIn->nLine;` |
|     591 |  7162 | `	rc = GenStateParseUnionTypeDecl(` |
|     293 |  7163 | `		pGen,` |
|     293 |  7164 | `		&pFunc->nReturnType,` |
|     293 |  7165 | `		&pFunc->sReturnClass,` |
|     293 |  7166 | `		&pFunc->aReturnUnion,` |
|       - |  7167 | `		&iFlags,` |
|     293 |  7168 | `		&pFunc->sReturnTypeName,` |
|       - |  7169 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|       - |  7170 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|       - |  7171 | `		/* iUnionFlag */ 0,` |
|       - |  7172 | `		/* bAllowVoid */ 1,` |
|     293 |  7173 | `		nLine);` |
|     591 |  7174 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7175 | `		return SXERR_ABORT;` |
|       - |  7176 | `	}` |
|     591 |  7177 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  7178 | `		/* Error already reported */` |
|     ! 0 |  7179 | `		return SXERR_SYNTAX;` |
|       - |  7180 | `	}` |
|     591 |  7181 | `	if( rc == SXERR_SYNTAX ){` |
|       8 |  7182 | `		if( pGen->pIn < pGen->pEnd ){` |
|      11 |  7183 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  7184 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       6 |  7185 | `				&pGen->pIn->sData);` |
|       5 |  7186 | `		}else{` |
|     ! 0 |  7187 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  7188 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  7189 | `		}` |
|       8 |  7190 | `		return SXERR_SYNTAX;` |
|       - |  7191 | `	}` |
|     585 |  7192 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     585 |  7193 | `	return SXRET_OK;` |
|  169939 |  7194 | `}` |
|       - |  7195 |  |
|   51380 |  7196 | `static sxi32 GenStateCompileFunc(` |
|       - |  7197 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7198 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  7199 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  7200 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  7201 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  7202 | `	)` |
|       5 |  7203 | `{` |
|       - |  7204 | `	ph7_vm_func *pFunc;` |
|       - |  7205 | `	SyToken *pEnd;` |
|       - |  7206 | `	sxu32 nLine;` |
|       - |  7207 | `	char *zName;` |
|       - |  7208 | `	sxi32 rc;` |
|       - |  7209 | `	/* Extract line number */` |
|   51385 |  7210 | `	nLine = pGen->pIn->nLine;` |
|       - |  7211 | `	/* Jump the left parenthesis '(' */` |
|   51385 |  7212 | `	pGen->pIn++;` |
|       - |  7213 | `	/* Delimit the function signature */` |
|   51385 |  7214 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   51385 |  7215 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7216 | `		/* Syntax error */` |
|       8 |  7217 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       8 |  7218 | `		if( rc == SXERR_ABORT ){` |
|       - |  7219 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7220 | `			return SXERR_ABORT;` |
|       - |  7221 | `		}` |
|       8 |  7222 | `		pGen->pIn = pGen->pEnd;` |
|       8 |  7223 | `		return SXRET_OK;` |
|       - |  7224 | `	}` |
|       - |  7225 | `	/* Create the function state */` |
|   51379 |  7226 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   51379 |  7227 | `	if( pFunc == 0 ){` |
|     ! 0 |  7228 | `		goto OutOfMem;` |
|       - |  7229 | `	}` |
|       - |  7230 | `	/* Build the function name, prepending namespace if active */` |
|   51386 |  7231 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  7232 | `		SyBlob sFQN;` |
|       - |  7233 | `		sxu32 nLen;` |
|      16 |  7234 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  7235 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  7236 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  7237 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  7238 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  7239 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  7240 | `		SyBlobRelease(&sFQN);` |
|      16 |  7241 | `		if( zName == 0 ){` |
|     ! 0 |  7242 | `			goto OutOfMem;` |
|       - |  7243 | `		}` |
|      16 |  7244 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  7245 | `	}else{` |
|   51365 |  7246 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   51365 |  7247 | `		if( zName == 0 ){` |
|     ! 0 |  7248 | `			goto OutOfMem;` |
|       - |  7249 | `		}` |
|   51365 |  7250 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  7251 | `	}` |
|   51379 |  7252 | `	if( pGen->pIn < pEnd ){` |
|       - |  7253 | `		/* Collect function arguments */` |
|   35381 |  7254 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   35381 |  7255 | `		if( rc == SXERR_ABORT ){` |
|       - |  7256 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7257 | `			return SXERR_ABORT;` |
|       - |  7258 | `		}` |
|   17688 |  7259 | `	}` |
|       - |  7260 | `	/* Point past ')' and parse optional return type ': type' */` |
|   51379 |  7261 | `	pGen->pIn = &pEnd[1];` |
|       - |  7262 | `	{` |
|   51379 |  7263 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   51379 |  7264 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7265 | `			return SXERR_ABORT;` |
|   51379 |  7266 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       8 |  7267 | `			return SXERR_SYNTAX;` |
|       - |  7268 | `		}` |
|       - |  7269 | `	}` |
|   51373 |  7270 | `	if( bHandleClosure ){` |
|       - |  7271 | `		ph7_vm_func_closure_env sEnv;` |
|     325 |  7272 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     320 |  7273 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     177 |  7274 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      29 |  7275 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  7276 | `				/* Closure,record environment variable */` |
|      29 |  7277 | `				pGen->pIn++;` |
|      29 |  7278 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  7279 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  7280 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7281 | `						return SXERR_ABORT;` |
|       - |  7282 | `					}` |
|     ! 0 |  7283 | `				}` |
|      29 |  7284 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  7285 | `				/* Compile until we hit the first closing parenthesis */` |
|      57 |  7286 | `				while( pGen->pIn < pGen->pEnd ){` |
|      57 |  7287 | `					int iFlagsLocal = 0;` |
|      57 |  7288 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      29 |  7289 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      29 |  7290 | `						break;` |
|       - |  7291 | `					}` |
|      33 |  7292 | `					nLineLocal = pGen->pIn->nLine;` |
|      33 |  7293 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  7294 | `						/* Pass by reference,record that */` |
|     ! 0 |  7295 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  7296 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  7297 | `							);` |
|     ! 0 |  7298 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  7299 | `						pGen->pIn++;` |
|     ! 0 |  7300 | `					}` |
|      28 |  7301 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      33 |  7302 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7303 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  7304 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  7305 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  7306 | `								return SXERR_ABORT;` |
|       - |  7307 | `							}` |
|       - |  7308 | `							/* Find the closing parenthesis */` |
|     ! 0 |  7309 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  7310 | `								pGen->pIn++;` |
|     ! 0 |  7311 | `							}` |
|     ! 0 |  7312 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  7313 | `								pGen->pIn++;` |
|     ! 0 |  7314 | `							}` |
|     ! 0 |  7315 | `							break;` |
|       - |  7316 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  7317 | `					}else{` |
|       - |  7318 | `						SyString *pNameLocal;` |
|       - |  7319 | `						char *zDup;` |
|       - |  7320 | `						/* Duplicate variable name */` |
|      33 |  7321 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      33 |  7322 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      33 |  7323 | `						if( zDup ){` |
|       - |  7324 | `							/* Zero the structure */` |
|      33 |  7325 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      33 |  7326 | `							sEnv.iFlags = iFlagsLocal;` |
|      33 |  7327 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      33 |  7328 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      33 |  7329 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  7330 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  7331 | `									got_this = 1;` |
|     ! 0 |  7332 | `							}` |
|       - |  7333 | `							/* Save imported variable */` |
|      33 |  7334 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      19 |  7335 | `						}else{` |
|     ! 0 |  7336 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7337 | `							 return SXERR_ABORT;` |
|       - |  7338 | `						}` |
|       - |  7339 | `					}` |
|      33 |  7340 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      39 |  7341 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7342 | `						/* Ignore trailing commas */` |
|       7 |  7343 | `						pGen->pIn++;` |
|       1 |  7344 | `					}` |
|       5 |  7345 | `				}` |
|      29 |  7346 | `				if( !got_this ){` |
|       - |  7347 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  7348 | `					 * available to the closure environment.` |
|       - |  7349 | `					 */` |
|      29 |  7350 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      29 |  7351 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      29 |  7352 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      29 |  7353 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      29 |  7354 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      12 |  7355 | `				}` |
|      29 |  7356 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  7357 | `					/* Mark as closure */` |
|      29 |  7358 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|      12 |  7359 | `				}` |
|       - |  7360 | `				/* php 7.1+: the return type follows the use clause —` |
|       - |  7361 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|       - |  7362 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|       - |  7363 | `				 * so an unconditional call would wipe a type parsed at the` |
|       - |  7364 | `				 * legacy pre-use position. */` |
|      29 |  7365 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|       7 |  7366 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|       7 |  7367 | `					if( rcRt2 == SXERR_ABORT ){` |
|     ! 0 |  7368 | `						return SXERR_ABORT;` |
|       7 |  7369 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|     ! 0 |  7370 | `						return SXERR_SYNTAX;` |
|       - |  7371 | `					}` |
|       3 |  7372 | `				}` |
|      12 |  7373 | `		}` |
|     160 |  7374 | `	}` |
|       - |  7375 | `	/* Compile the body */` |
|   51373 |  7376 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   51373 |  7377 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7378 | `		return SXERR_ABORT;` |
|       - |  7379 | `	}` |
|   51373 |  7380 | `	if( ppFunc ){` |
|     325 |  7381 | `		*ppFunc = pFunc;` |
|     160 |  7382 | `	}` |
|   51373 |  7383 | `	rc = SXRET_OK;` |
|   51373 |  7384 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  7385 | `		/* Finally register the function */` |
|   51349 |  7386 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   25672 |  7387 | `	}` |
|   51373 |  7388 | `	if( rc == SXRET_OK ){` |
|   51373 |  7389 | `		return SXRET_OK;` |
|       - |  7390 | `	}` |
|       - |  7391 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  7392 | `OutOfMem:` |
|       - |  7393 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  7394 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  7395 | `	 */` |
|     ! 0 |  7396 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  7397 | `	return SXERR_ABORT;` |
|   25695 |  7398 | `}` |
|       - |  7399 | `/*` |
|       - |  7400 | ` * Compile a standard PHP function.` |
|       - |  7401 | ` *  Refer to the block-comment above for more information.` |
|       - |  7402 | ` */` |
|   51068 |  7403 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  7404 | `{` |
|       - |  7405 | `	SyString *pName;` |
|       - |  7406 | `	sxi32 iFlags;` |
|       - |  7407 | `	sxu32 nLine;` |
|       - |  7408 | `	sxi32 rc;` |
|       - |  7409 |  |
|   51073 |  7410 | `	nLine = pGen->pIn->nLine;` |
|   51073 |  7411 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   51073 |  7412 | `	iFlags = 0;` |
|   51073 |  7413 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7414 | `		/* Return by reference,remember that */` |
|      10 |  7415 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7416 | `		/* Jump the '&' token */` |
|      10 |  7417 | `		pGen->pIn++;` |
|       4 |  7418 | `	}` |
|   51073 |  7419 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7420 | `		/* Invalid function name */` |
|       8 |  7421 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       8 |  7422 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7423 | `			return SXERR_ABORT;` |
|       - |  7424 | `		}` |
|       - |  7425 | `		/* Sychronize with the next semi-colon or braces*/` |
|      22 |  7426 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      16 |  7427 | `			pGen->pIn++;` |
|       2 |  7428 | `		}` |
|       8 |  7429 | `		return SXRET_OK;` |
|       - |  7430 | `	}` |
|   51067 |  7431 | `	pName = &pGen->pIn->sData;` |
|   51067 |  7432 | `	nLine = pGen->pIn->nLine;` |
|       - |  7433 | `	/* Jump the function name */` |
|   51067 |  7434 | `	pGen->pIn++;` |
|   51067 |  7435 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7436 | `		/* Syntax error */` |
|       3 |  7437 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  7438 | `		if( rc == SXERR_ABORT ){` |
|       - |  7439 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7440 | `			return SXERR_ABORT;` |
|       - |  7441 | `		}` |
|       - |  7442 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  7443 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  7444 | `			pGen->pIn++;` |
|     ! 0 |  7445 | `		}` |
|       3 |  7446 | `		return SXRET_OK;` |
|       - |  7447 | `	}` |
|       - |  7448 | `	/* Compile function body */` |
|   51065 |  7449 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   51065 |  7450 | `	return rc;` |
|   25539 |  7451 | `}` |
|       - |  7452 | `/*` |
|       - |  7453 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  7454 | ` * According to the PHP language reference manual` |
|       - |  7455 | ` *  Visibility:` |
|       - |  7456 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  7457 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  7458 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  7459 | ` *  Members declared protected can be accessed only within the class` |
|       - |  7460 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  7461 | ` *  may only be accessed by the class that defines the member.` |
|       - |  7462 | ` */` |
|  369510 |  7463 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  7464 | `{` |
|  369515 |  7465 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   23087 |  7466 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  346433 |  7467 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   49823 |  7468 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  7469 | `	}` |
|       - |  7470 | `	/* Assume public by default */` |
|  296615 |  7471 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  184760 |  7472 | `}` |
|       - |  7473 | `/*` |
|       - |  7474 | ` * Compile a class constant.` |
|       - |  7475 | ` * According to the PHP language reference manual` |
|       - |  7476 | ` *  Class Constants` |
|       - |  7477 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  7478 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  7479 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  7480 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  7481 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  7482 | ` *   It's also possible for interfaces to have constants.` |
|       - |  7483 | ` * Symisc eXtension.` |
|       - |  7484 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  7485 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7486 | ` *  Example:` |
|       - |  7487 | ` *   class Test{` |
|       - |  7488 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7489 | ` *   };` |
|       - |  7490 | ` *   var_dump(TEST::MyConst);` |
|       - |  7491 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7492 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7493 | ` */` |
|       - |  7494 | `/*` |
|       - |  7495 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  7496 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  7497 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  7498 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  7499 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  7500 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  7501 | ` */` |
|     100 |  7502 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  7503 | `{` |
|       - |  7504 | `	SyToken *p0, *p1;` |
|     105 |  7505 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7506 | `		return 0;` |
|       - |  7507 | `	}` |
|     105 |  7508 | `	p0 = pGen->pIn;` |
|       - |  7509 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|     105 |  7510 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  7511 | `		return 1;` |
|       - |  7512 | `	}` |
|     105 |  7513 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  7514 | `		return 1;` |
|       - |  7515 | `	}` |
|       - |  7516 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  7517 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  7518 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|     101 |  7519 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     101 |  7520 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|     101 |  7521 | `		if( p1 ){` |
|     101 |  7522 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      34 |  7523 | `				return 1;` |
|       - |  7524 | `			}` |
|      71 |  7525 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  7526 | `				return 1;` |
|       - |  7527 | `			}` |
|      31 |  7528 | `		}` |
|      31 |  7529 | `	}` |
|      67 |  7530 | `	return 0;` |
|      55 |  7531 | `}` |
|       - |  7532 | `/*` |
|       - |  7533 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|       - |  7534 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|       - |  7535 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|       - |  7536 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|       - |  7537 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|       - |  7538 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|       - |  7539 | ` * Peek only; never consumes tokens.` |
|       - |  7540 | ` */` |
|      24 |  7541 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|       4 |  7542 | `{` |
|      28 |  7543 | `	SyToken *p = pGen->pIn;` |
|      39 |  7544 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      20 |  7545 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|       3 |  7546 | `		p++; /* skip leading unary sign(s) */` |
|       1 |  7547 | `	}` |
|      28 |  7548 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|      23 |  7549 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|       - |  7550 | `	}` |
|       6 |  7551 | `	p++;` |
|       - |  7552 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|       6 |  7553 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|      16 |  7554 | `}` |
|       - |  7555 | `/*` |
|       - |  7556 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|       - |  7557 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|       - |  7558 | `` * `$o->new`), not a `new` expression.`` |
|       - |  7559 | ` */` |
|       6 |  7560 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|       3 |  7561 | `{` |
|       - |  7562 | `	sxi32 iOp;` |
|       9 |  7563 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|     ! 0 |  7564 | `		return 0;` |
|       - |  7565 | `	}` |
|       9 |  7566 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       9 |  7567 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|       6 |  7568 | `}` |
|       - |  7569 | `/*` |
|       - |  7570 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|       - |  7571 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|       - |  7572 | ` * interface-constant and (instance/static) property-default initializers` |
|       - |  7573 | ` * ("New expressions are not supported in this context") while still allowing it` |
|       - |  7574 | ` * in global constants, parameter defaults and static-local initializers (which` |
|       - |  7575 | ` * are compiled by different functions and left untouched). The scan is` |
|       - |  7576 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|       - |  7577 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|       - |  7578 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|       - |  7579 | ` *` |
|       - |  7580 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|       - |  7581 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|       - |  7582 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|       - |  7583 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|       - |  7584 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|       - |  7585 | ` */` |
|   23576 |  7586 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|       5 |  7587 | `{` |
|   23581 |  7588 | `	SyToken *p = pGen->pIn;` |
|   23581 |  7589 | `	int iDepth = 0;` |
|   70935 |  7590 | `	while( p < pGen->pEnd ){` |
|   70935 |  7591 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   23573 |  7592 | `			break; /* end of this initializer */` |
|       - |  7593 | `		}` |
|   47367 |  7594 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   23691 |  7595 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      10 |  7596 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|       - |  7597 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|       - |  7598 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|       - |  7599 | `			 * expression. */` |
|       3 |  7600 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|       3 |  7601 | `			p++;` |
|       3 |  7602 | `			if( bArrow ){` |
|       - |  7603 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|       - |  7604 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|       3 |  7605 | `				int iBase = iDepth;` |
|      17 |  7606 | `				while( p < pGen->pEnd ){` |
|      17 |  7607 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       5 |  7608 | `						iDepth++;` |
|      15 |  7609 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       5 |  7610 | `						if( iDepth <= iBase ){` |
|     ! 0 |  7611 | `							break; /* closes an enclosing group, not the fn's own */` |
|       - |  7612 | `						}` |
|       5 |  7613 | `						iDepth--;` |
|      11 |  7614 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|       3 |  7615 | `						break;` |
|       - |  7616 | `					}` |
|      15 |  7617 | `					p++;` |
|       1 |  7618 | `				}` |
|       2 |  7619 | `			}else{` |
|       - |  7620 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|       - |  7621 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|       - |  7622 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|       - |  7623 | `				 * then skip the balanced brace block. */` |
|     ! 0 |  7624 | `				int iLocal = 0;` |
|     ! 0 |  7625 | `				while( p < pGen->pEnd ){` |
|     ! 0 |  7626 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|     ! 0 |  7627 | `						break; /* body brace */` |
|       - |  7628 | `					}` |
|     ! 0 |  7629 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  7630 | `						iLocal++;` |
|     ! 0 |  7631 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  7632 | `						if( iLocal > 0 ){` |
|     ! 0 |  7633 | `							iLocal--;` |
|     ! 0 |  7634 | `						}` |
|     ! 0 |  7635 | `					}` |
|     ! 0 |  7636 | `					p++;` |
|     ! 0 |  7637 | `				}` |
|     ! 0 |  7638 | `				if( p < pGen->pEnd ){` |
|     ! 0 |  7639 | `					int iBrace = 0; /* p is on the body '{' */` |
|     ! 0 |  7640 | `					while( p < pGen->pEnd ){` |
|     ! 0 |  7641 | `						if( p->nType & PH7_TK_OCB ){` |
|     ! 0 |  7642 | `							iBrace++;` |
|     ! 0 |  7643 | `						}else if( p->nType & PH7_TK_CCB ){` |
|     ! 0 |  7644 | `							iBrace--;` |
|     ! 0 |  7645 | `							if( iBrace == 0 ){` |
|     ! 0 |  7646 | `								p++;` |
|     ! 0 |  7647 | `								break;` |
|       - |  7648 | `							}` |
|     ! 0 |  7649 | `						}` |
|     ! 0 |  7650 | `						p++;` |
|     ! 0 |  7651 | `					}` |
|     ! 0 |  7652 | `				}` |
|       - |  7653 | `			}` |
|       3 |  7654 | `			continue;` |
|       - |  7655 | `		}` |
|   47365 |  7656 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      69 |  7657 | `			iDepth++;` |
|   47333 |  7658 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      67 |  7659 | `			if( iDepth > 0 ){` |
|      67 |  7660 | `				iDepth--;` |
|      31 |  7661 | `			}` |
|   47270 |  7662 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|   23551 |  7663 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|       - |  7664 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|       - |  7665 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|       - |  7666 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|      11 |  7667 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|      11 |  7668 | `				return 1;` |
|       - |  7669 | `			}` |
|     ! 0 |  7670 | `		}` |
|   47357 |  7671 | `		p++;` |
|       5 |  7672 | `	}` |
|   23573 |  7673 | `	return 0;` |
|   11793 |  7674 | `}` |
|       - |  7675 | `/*` |
|       - |  7676 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  7677 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  7678 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  7679 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  7680 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  7681 | ` * share the same backing.` |
|       - |  7682 | ` */` |
|     214 |  7683 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  7684 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  7685 | `{` |
|     219 |  7686 | `	pAttr->nType = nType;` |
|     219 |  7687 | `	pAttr->sClass = *pClass;` |
|     219 |  7688 | `	pAttr->sTypeName = *pTypeName;` |
|     219 |  7689 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7690 | `		sxu32 i;` |
|      67 |  7691 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      47 |  7692 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      47 |  7693 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      26 |  7694 | `		}` |
|      10 |  7695 | `	}` |
|     219 |  7696 | `}` |
|     100 |  7697 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7698 | `{` |
|     105 |  7699 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7700 | `	SySet *pInstrContainer;` |
|       - |  7701 | `	ph7_class_attr *pCons;` |
|       - |  7702 | `	SyString *pName;` |
|       - |  7703 | `	sxi32 rc;` |
|     105 |  7704 | `	sxu32 nType = 0;` |
|       - |  7705 | `	SyString sTypeClass;` |
|       - |  7706 | `	SyString sTypeText;` |
|       - |  7707 | `	SySet aUnionAlts;` |
|     105 |  7708 | `	sxi32 iTypeFlags = 0;` |
|     105 |  7709 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|     105 |  7710 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|     105 |  7711 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7712 | `	/* Extract visibility level */` |
|     105 |  7713 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7714 | `	/* Mark as constant */` |
|     105 |  7715 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|     105 |  7716 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  7717 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  7718 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|     124 |  7719 | `	if( GenStateClassConstHasType(pGen) ){` |
|      61 |  7720 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      38 |  7721 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  7722 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  7723 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  7724 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  7725 | `		 * and success paths release. */` |
|      42 |  7726 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7727 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7728 | `			goto Synchronize;` |
|      42 |  7729 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7730 | `			return SXERR_ABORT;` |
|      42 |  7731 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7732 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7733 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  7734 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7735 | `				return SXERR_ABORT;` |
|       - |  7736 | `			}` |
|     ! 0 |  7737 | `			goto Synchronize;` |
|       - |  7738 | `		}` |
|      42 |  7739 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      19 |  7740 | `	}` |
|      50 |  7741 | `loop:` |
|     107 |  7742 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7743 | `		/* Invalid constant name */` |
|     ! 0 |  7744 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  7745 | `		if( rc == SXERR_ABORT ){` |
|       - |  7746 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7747 | `			return SXERR_ABORT;` |
|       - |  7748 | `		}` |
|     ! 0 |  7749 | `		goto Synchronize;` |
|       - |  7750 | `	}` |
|       - |  7751 | `	/* Peek constant name */` |
|     107 |  7752 | `	pName = &pGen->pIn->sData;` |
|       - |  7753 | `	/* Make sure the constant name isn't reserved */` |
|     107 |  7754 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  7755 | `		/* Reserved constant name */` |
|     ! 0 |  7756 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  7757 | `		if( rc == SXERR_ABORT ){` |
|       - |  7758 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7759 | `			return SXERR_ABORT;` |
|       - |  7760 | `		}` |
|     ! 0 |  7761 | `		goto Synchronize;` |
|       - |  7762 | `	}` |
|       - |  7763 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|     107 |  7764 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      61 |  7765 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      38 |  7766 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      19 |  7767 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      42 |  7768 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7769 | `			return SXERR_ABORT;` |
|      42 |  7770 | `		}else if( rc != SXRET_OK ){` |
|       3 |  7771 | `			goto Synchronize;` |
|       - |  7772 | `		}` |
|      18 |  7773 | `	}` |
|       - |  7774 | `	/* Advance the stream cursor */` |
|     105 |  7775 | `	pGen->pIn++;` |
|     105 |  7776 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  7777 | `		/* Invalid declaration */` |
|     ! 0 |  7778 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  7779 | `		if( rc == SXERR_ABORT ){` |
|       - |  7780 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7781 | `			return SXERR_ABORT;` |
|       - |  7782 | `		}` |
|     ! 0 |  7783 | `		goto Synchronize;` |
|       - |  7784 | `	}` |
|     105 |  7785 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  7786 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|       - |  7787 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|       - |  7788 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|       - |  7789 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|     112 |  7790 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|      39 |  7791 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|       8 |  7792 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7793 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|       2 |  7794 | `			&pClass->sName,pName,&sTypeText);` |
|       6 |  7795 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7796 | `			return SXERR_ABORT;` |
|       - |  7797 | `		}` |
|       6 |  7798 | `		goto Synchronize;` |
|       - |  7799 | `	}` |
|       - |  7800 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|       - |  7801 | `	 * constant initializer ("New expressions are not supported in this context").` |
|       - |  7802 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|     101 |  7803 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|       5 |  7804 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7805 | `			"New expressions are not supported in this context");` |
|       5 |  7806 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7807 | `			return SXERR_ABORT;` |
|       - |  7808 | `		}` |
|       5 |  7809 | `		goto Synchronize;` |
|       - |  7810 | `	}` |
|       - |  7811 | `	/* Allocate a new class attribute */` |
|      97 |  7812 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      97 |  7813 | `	if( pCons == 0 ){` |
|     ! 0 |  7814 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7815 | `		return SXERR_ABORT;` |
|       - |  7816 | `	}` |
|      97 |  7817 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      35 |  7818 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      16 |  7819 | `	}` |
|       - |  7820 | `	/* Swap bytecode container */` |
|      97 |  7821 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 |  7822 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  7823 | `	/* Compile constant value.` |
|       - |  7824 | `	 */` |
|      97 |  7825 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      97 |  7826 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  7827 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  7828 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7829 | `			return SXERR_ABORT;` |
|       - |  7830 | `		}` |
|       1 |  7831 | `	}` |
|       - |  7832 | `	/* Emit the done instruction */` |
|      97 |  7833 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      97 |  7834 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      97 |  7835 | `	if( rc == SXERR_ABORT ){` |
|       - |  7836 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7837 | `		return SXERR_ABORT;` |
|       - |  7838 | `	}` |
|       - |  7839 | `	/* All done,install the constant */` |
|      97 |  7840 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      97 |  7841 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7842 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7843 | `		return SXERR_ABORT;` |
|       - |  7844 | `	}` |
|      97 |  7845 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7846 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  7847 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  7848 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7849 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7850 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7851 | `				pTok--;` |
|     ! 0 |  7852 | `			}` |
|     ! 0 |  7853 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7854 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  7855 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7856 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7857 | `				return SXERR_ABORT;` |
|       - |  7858 | `			}` |
|     ! 0 |  7859 | `		}else{` |
|       3 |  7860 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  7861 | `				goto loop;` |
|       - |  7862 | `			}` |
|       - |  7863 | `		}` |
|     ! 0 |  7864 | `	}` |
|      95 |  7865 | `	SySetRelease(&aUnionAlts);` |
|      95 |  7866 | `	return SXRET_OK;` |
|       5 |  7867 | `Synchronize:` |
|      13 |  7868 | `	SySetRelease(&aUnionAlts);` |
|       - |  7869 | `	/* Synchronize with the first semi-colon */` |
|      45 |  7870 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      35 |  7871 | `		pGen->pIn++;` |
|       3 |  7872 | `	}` |
|      13 |  7873 | `	return SXERR_CORRUPT;` |
|      55 |  7874 | `}` |
|       - |  7875 | `/*` |
|       - |  7876 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  7877 | ` * According to the PHP language reference manual` |
|       - |  7878 | ` *  Properties` |
|       - |  7879 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  7880 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  7881 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  7882 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  7883 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  7884 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  7885 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  7886 | ` * Symisc eXtension.` |
|       - |  7887 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  7888 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7889 | ` *  Example:` |
|       - |  7890 | ` *   class Test{` |
|       - |  7891 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7892 | ` *   };` |
|       - |  7893 | ` *   var_dump(TEST::myVar);` |
|       - |  7894 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7895 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7896 | ` */` |
|       - |  7897 | `/*` |
|       - |  7898 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  7899 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  7900 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  7901 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  7902 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  7903 | ` */` |
|  200230 |  7904 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  7905 | `{` |
|  200235 |  7906 | `	SyToken *p = pStart;` |
|  200235 |  7907 | `	int bFirst = 1;` |
|  200235 |  7908 | `	if( p >= pEnd ) return 0;` |
|       - |  7909 | ``	/* Optional nullable `?` shorthand. */`` |
|  200235 |  7910 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      19 |  7911 | `		p++;` |
|      19 |  7912 | `		if( p >= pEnd ) return 0;` |
|       8 |  7913 | `	}` |
|       - |  7914 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  7915 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  7916 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  7917 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|  100115 |  7918 | `	for(;;){` |
|  200253 |  7919 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  7920 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  7921 | `			p++;` |
|       9 |  7922 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  7923 | `			if( p >= pEnd ) return 0;` |
|       3 |  7924 | `			p++; /* skip ')' */` |
|       2 |  7925 | `		}else{` |
|       - |  7926 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - |  7927 | ``			 * then any `&`-joined intersection members. */`` |
|  200251 |  7928 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  200251 |  7929 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7930 | `				return 0;` |
|       - |  7931 | `			}` |
|       - |  7932 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - |  7933 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - |  7934 | `			 * may still appear at the initial dispatch site). */` |
|  200251 |  7935 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  200205 |  7936 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  200278 |  7937 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|   11714 |  7938 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  200049 |  7939 | `					return 0;` |
|       - |  7940 | `				}` |
|      78 |  7941 | `			}` |
|     207 |  7942 | `			p++;` |
|     209 |  7943 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7944 | `				p += 2;` |
|       1 |  7945 | `			}` |
|     306 |  7946 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     210 |  7947 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7948 | `				p++; /* skip '&' */` |
|       3 |  7949 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       3 |  7950 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       3 |  7951 | `				p++;` |
|       3 |  7952 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  7953 | `					p += 2;` |
|     ! 0 |  7954 | `				}` |
|       1 |  7955 | `			}` |
|       - |  7956 | `		}` |
|     209 |  7957 | `		bFirst = 0;` |
|     204 |  7958 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      23 |  7959 | `			&& p->sData.zString[0] == '\|' ){` |
|      23 |  7960 | ``			p++; /* next `\|`-separated part */`` |
|      23 |  7961 | `			continue;` |
|       - |  7962 | `		}` |
|     191 |  7963 | `		break;` |
|     ! 0 |  7964 | `	}` |
|     191 |  7965 | `	if( p >= pEnd ) return 0;` |
|     191 |  7966 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|  100120 |  7967 | `}` |
|       - |  7968 |  |
|       - |  7969 | `/*` |
|       - |  7970 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  7971 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  7972 | ` * if not). Recognized forms:` |
|       - |  7973 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  7974 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  7975 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  7976 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  7977 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  7978 | ` * on unrecoverable error.` |
|       - |  7979 | ` *` |
|       - |  7980 | ` * When a type is parsed:` |
|       - |  7981 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  7982 | ` *   *pClass is set to the class name (for class types)` |
|       - |  7983 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  7984 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  7985 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  7986 | ` */` |
|     186 |  7987 | `static sxi32 GenStateParsePropertyType(` |
|       - |  7988 | `	ph7_gen_state *pGen,` |
|       - |  7989 | `	sxu32 *pnType,` |
|       - |  7990 | `	SyString *pClass,` |
|       - |  7991 | `	sxi32 *piTypeFlags,` |
|       - |  7992 | `	SyString *pTypeText,` |
|       - |  7993 | `	SySet *pAlts` |
|       5 |  7994 | `){` |
|     191 |  7995 | `	sxi32 iFlags = 0;` |
|       - |  7996 | `	sxi32 rc;` |
|     191 |  7997 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7998 | `		return SXRET_OK;` |
|       - |  7999 | `	}` |
|       - |  8000 | `	/* If the first token is '$', there's no type */` |
|     191 |  8001 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  8002 | `		return SXRET_OK;` |
|       - |  8003 | `	}` |
|     191 |  8004 | `	rc = GenStateParseUnionTypeDecl(` |
|      93 |  8005 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  8006 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  8007 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  8008 | `		/* bAllowVoid */ 0,` |
|     186 |  8009 | `		pGen->pIn->nLine);` |
|     191 |  8010 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8011 | `		return rc;` |
|       - |  8012 | `	}` |
|       - |  8013 | `	/* Verify next token is '$' (start of property name) */` |
|     191 |  8014 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  8015 | `		return SXERR_SYNTAX;` |
|       - |  8016 | `	}` |
|     191 |  8017 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     191 |  8018 | `	return SXRET_OK;` |
|      98 |  8019 | `}` |
|       - |  8020 |  |
|       - |  8021 | `/*` |
|       - |  8022 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  8023 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  8024 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  8025 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  8026 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  8027 | ` * by the type parser itself before reaching here.` |
|       - |  8028 | ` *` |
|       - |  8029 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  8030 | ` * use in the error message.` |
|       - |  8031 | ` */` |
|     346 |  8032 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  8033 | `	sxu32 nType,` |
|       - |  8034 | `	const SyString *pClass,` |
|       - |  8035 | `	const char **pzName,` |
|       - |  8036 | `	sxu32 *pnName)` |
|       5 |  8037 | `{` |
|       - |  8038 | `	const char *z;` |
|       - |  8039 | `	sxu32 n;` |
|     351 |  8040 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     297 |  8041 | `		return 0;` |
|       - |  8042 | `	}` |
|      59 |  8043 | `	z = pClass->zString;` |
|      59 |  8044 | `	n = pClass->nByte;` |
|      59 |  8045 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  8046 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  8047 | `	}` |
|       - |  8048 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  8049 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  8050 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      52 |  8051 | `	return 0;` |
|     178 |  8052 | `}` |
|       - |  8053 |  |
|       - |  8054 | `/*` |
|       - |  8055 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  8056 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  8057 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  8058 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  8059 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  8060 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  8061 | ` *` |
|       - |  8062 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  8063 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  8064 | ` */` |
|     288 |  8065 | `static sxi32 GenStateValidateMemberType(` |
|       - |  8066 | `	ph7_gen_state *pGen,` |
|       - |  8067 | `	ph7_class *pClass,` |
|       - |  8068 | `	const SyString *pMemberName,` |
|       - |  8069 | `	sxu32 nType,` |
|       - |  8070 | `	const SyString *pTypeClass,` |
|       - |  8071 | `	const SyString *pTypeText,` |
|       - |  8072 | `	SySet *pUnionAlts,` |
|       - |  8073 | `	const char *zErrFmt,` |
|       - |  8074 | `	sxu32 nLine)` |
|       5 |  8075 | `{` |
|     293 |  8076 | `	const char *zBad = 0;` |
|     293 |  8077 | `	sxu32 nBad = 0;` |
|       - |  8078 | `	SyString sFallback;` |
|       - |  8079 | `	const SyString *pBad;` |
|       - |  8080 | `	sxi32 rc;` |
|     293 |  8081 | `	int bDisallowed = 0;` |
|     293 |  8082 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  8083 | `		bDisallowed = 1;` |
|     291 |  8084 | `	}else if( pUnionAlts ){` |
|       - |  8085 | `		sxu32 i;` |
|      89 |  8086 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      63 |  8087 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      63 |  8088 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  8089 | `				bDisallowed = 1;` |
|       3 |  8090 | `				break;` |
|       - |  8091 | `			}` |
|      33 |  8092 | `		}` |
|      14 |  8093 | `	}` |
|     293 |  8094 | `	if( !bDisallowed ){` |
|     287 |  8095 | `		return SXRET_OK;` |
|       - |  8096 | `	}` |
|       - |  8097 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  8098 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  8099 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  8100 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  8101 | `		pBad = pTypeText;` |
|       5 |  8102 | `	}else{` |
|     ! 0 |  8103 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  8104 | `		pBad = &sFallback;` |
|       - |  8105 | `	}` |
|      11 |  8106 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  8107 | `		zErrFmt,` |
|       3 |  8108 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  8109 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8110 | `		return SXERR_ABORT;` |
|       - |  8111 | `	}` |
|       8 |  8112 | `	return SXERR_SYNTAX;` |
|     149 |  8113 | `}` |
|       - |  8114 | `/*` |
|       - |  8115 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  8116 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  8117 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  8118 | ` * than promoted to a lexer keyword.` |
|       - |  8119 | ` */` |
| 1778960 |  8120 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  8121 | `{` |
| 1815400 |  8122 | `	return (pTok->nType & PH7_TK_ID)` |
|  925915 |  8123 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1815395 |  8124 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  8125 | `}` |
|   81098 |  8126 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  8127 | `{` |
|   81103 |  8128 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8129 | `	ph7_class_attr *pAttr;` |
|       - |  8130 | `	SyString *pName;` |
|       - |  8131 | `	sxi32 rc;` |
|   81103 |  8132 | `	sxu32 nType = 0;` |
|       - |  8133 | `	SyString sTypeClass;` |
|       - |  8134 | `	SyString sTypeText;` |
|       - |  8135 | `	SySet aUnionAlts;` |
|   81103 |  8136 | `	sxi32 iTypeFlags = 0;` |
|   81103 |  8137 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   81103 |  8138 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   81103 |  8139 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  8140 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  8141 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  8142 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   81103 |  8143 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  8144 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  8145 | `	}` |
|       - |  8146 | `	/* Extract visibility level */` |
|   81103 |  8147 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  8148 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   81196 |  8149 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     191 |  8150 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     191 |  8151 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  8152 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  8153 | `			goto Synchronize;` |
|     191 |  8154 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  8155 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8156 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  8157 | `				&pGen->pIn->sData);` |
|     ! 0 |  8158 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8159 | `				return SXERR_ABORT;` |
|       - |  8160 | `			}` |
|     ! 0 |  8161 | `			goto Synchronize;` |
|     191 |  8162 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  8163 | `			return SXERR_ABORT;` |
|       - |  8164 | `		}` |
|      93 |  8165 | `	}` |
|     ! 0 |  8166 | `loop:` |
|   81107 |  8167 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  8168 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  8169 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8170 | `			return SXERR_ABORT;` |
|       - |  8171 | `		}` |
|     ! 0 |  8172 | `		goto Synchronize;` |
|       - |  8173 | `	}` |
|   81107 |  8174 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   81107 |  8175 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  8176 | `		/* Invalid attribute name */` |
|     ! 0 |  8177 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  8178 | `		if( rc == SXERR_ABORT ){` |
|       - |  8179 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8180 | `			return SXERR_ABORT;` |
|       - |  8181 | `		}` |
|     ! 0 |  8182 | `		goto Synchronize;` |
|       - |  8183 | `	}` |
|       - |  8184 | `	/* Peek attribute name */` |
|   81107 |  8185 | `	pName = &pGen->pIn->sData;` |
|       - |  8186 | `	/* Advance the stream cursor */` |
|   81107 |  8187 | `	pGen->pIn++;` |
|   81107 |  8188 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  8189 | `		/* Invalid declaration */` |
|       3 |  8190 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  8191 | `		if( rc == SXERR_ABORT ){` |
|       - |  8192 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8193 | `			return SXERR_ABORT;` |
|       - |  8194 | `		}` |
|       3 |  8195 | `		goto Synchronize;` |
|       - |  8196 | `	}` |
|       - |  8197 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  8198 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   81105 |  8199 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  8200 | `		const char *zRoErr = 0;` |
|      39 |  8201 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  8202 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  8203 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  8204 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  8205 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  8206 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  8207 | `		}` |
|      39 |  8208 | `		if( zRoErr ){` |
|      13 |  8209 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  8210 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8211 | `				return SXERR_ABORT;` |
|       - |  8212 | `			}` |
|      13 |  8213 | `			goto Synchronize;` |
|       - |  8214 | `		}` |
|      12 |  8215 | `	}` |
|       - |  8216 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  8217 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  8218 | `	 * by the type parser. */` |
|   81095 |  8219 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     281 |  8220 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  8221 | `			&sTypeText,` |
|     184 |  8222 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      92 |  8223 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     189 |  8224 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8225 | `			return SXERR_ABORT;` |
|     189 |  8226 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  8227 | `			goto Synchronize;` |
|       - |  8228 | `		}` |
|      92 |  8229 | `	}` |
|       - |  8230 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   81095 |  8231 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  8232 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8233 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  8234 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8235 | `			return SXERR_ABORT;` |
|       - |  8236 | `		}` |
|       3 |  8237 | `		goto Synchronize;` |
|       - |  8238 | `	}` |
|       - |  8239 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|       - |  8240 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|       - |  8241 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|       - |  8242 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|       - |  8243 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|       - |  8244 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   81093 |  8245 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|       6 |  8246 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8247 | `			"New expressions are not supported in this context");` |
|       6 |  8248 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8249 | `			return SXERR_ABORT;` |
|       - |  8250 | `		}` |
|       6 |  8251 | `		goto Synchronize;` |
|       - |  8252 | `	}` |
|       - |  8253 | `	/* Allocate a new class attribute */` |
|   81089 |  8254 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   81089 |  8255 | `	if( pAttr == 0 ){` |
|     ! 0 |  8256 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  8257 | `		return SXERR_ABORT;` |
|       - |  8258 | `	}` |
|   81089 |  8259 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     187 |  8260 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      91 |  8261 | `	}` |
|   81089 |  8262 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  8263 | `		SySet *pInstrContainer;` |
|   23481 |  8264 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  8265 | `		/* Swap bytecode container */` |
|   23481 |  8266 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   23481 |  8267 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  8268 | `		/* Compile attribute value.` |
|       - |  8269 | `		 */` |
|   23481 |  8270 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   23481 |  8271 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  8272 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  8273 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8274 | `				return SXERR_ABORT;` |
|       - |  8275 | `			}` |
|     ! 0 |  8276 | `		}` |
|       - |  8277 | `		/* Emit the done instruction */` |
|   23481 |  8278 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   23481 |  8279 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   11738 |  8280 | `	}` |
|       - |  8281 | `	/* All done,install the attribute */` |
|   81089 |  8282 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   81089 |  8283 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8284 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8285 | `		return SXERR_ABORT;` |
|       - |  8286 | `	}` |
|   81089 |  8287 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  8288 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  8289 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  8290 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  8291 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  8292 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  8293 | `				pTok--;` |
|     ! 0 |  8294 | `			}` |
|     ! 0 |  8295 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8296 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8297 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  8298 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8299 | `				return SXERR_ABORT;` |
|       - |  8300 | `			}` |
|     ! 0 |  8301 | `		}else{` |
|       5 |  8302 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  8303 | `				goto loop;` |
|       - |  8304 | `			}` |
|       - |  8305 | `		}` |
|     ! 0 |  8306 | `	}` |
|   81085 |  8307 | `	SySetRelease(&aUnionAlts);` |
|   81085 |  8308 | `	return SXRET_OK;` |
|       9 |  8309 | `Synchronize:` |
|       - |  8310 | `	/* Synchronize with the first semi-colon */` |
|      56 |  8311 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      37 |  8312 | `		pGen->pIn++;` |
|       3 |  8313 | `	}` |
|      22 |  8314 | `	SySetRelease(&aUnionAlts);` |
|      22 |  8315 | `	return SXERR_CORRUPT;` |
|   40554 |  8316 | `}` |
|       - |  8317 | `/*` |
|       - |  8318 | ` * Compile a class method.` |
|       - |  8319 | ` *` |
|       - |  8320 | ` * Refer to the official documentation for more information` |
|       - |  8321 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  8322 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  8323 | ` * overloading and many more.` |
|       - |  8324 | ` */` |
|  288312 |  8325 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  8326 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  8327 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  8328 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  8329 | `	int doBody,          /* TRUE to process method body */` |
|       - |  8330 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  8331 | `	)` |
|       5 |  8332 | `{` |
|  288317 |  8333 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8334 | `	ph7_class_method *pMeth;` |
|       - |  8335 | `	sxi32 iFuncFlags;` |
|       - |  8336 | `	SyString *pName;` |
|       - |  8337 | `	SyToken *pEnd;` |
|       - |  8338 | `	sxi32 rc;` |
|       - |  8339 | `	/* Extract visibility level */` |
|  288317 |  8340 | `	iProtection = GetProtectionLevel(iProtection);` |
|  288317 |  8341 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  288317 |  8342 | `	iFuncFlags = 0;` |
|  288317 |  8343 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8344 | `		/* Invalid method name */` |
|     ! 0 |  8345 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  8346 | `		if( rc == SXERR_ABORT ){` |
|       - |  8347 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8348 | `			return SXERR_ABORT;` |
|       - |  8349 | `		}` |
|     ! 0 |  8350 | `		goto Synchronize;` |
|       - |  8351 | `	}` |
|  288317 |  8352 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  8353 | `		/* Return by reference,remember that */` |
|     ! 0 |  8354 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  8355 | `		/* Jump the '&' token */` |
|     ! 0 |  8356 | `		pGen->pIn++;` |
|     ! 0 |  8357 | `	}` |
|  288317 |  8358 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  8359 | `		/* Invalid method name */` |
|     ! 0 |  8360 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  8361 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8362 | `			return SXERR_ABORT;` |
|       - |  8363 | `		}` |
|     ! 0 |  8364 | `		goto Synchronize;` |
|       - |  8365 | `	}` |
|       - |  8366 | `	/* Peek method name */` |
|  288317 |  8367 | `	pName = &pGen->pIn->sData;` |
|  288317 |  8368 | `	nLine = pGen->pIn->nLine;` |
|       - |  8369 | `	/* Jump the method name */` |
|  288317 |  8370 | `	pGen->pIn++;` |
|  288317 |  8371 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  8372 | `		/* Abstract method */` |
|   99591 |  8373 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  8374 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8375 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  8376 | `				&pClass->sName,pName);` |
|     ! 0 |  8377 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8378 | `				return SXERR_ABORT;` |
|       - |  8379 | `			}` |
|     ! 0 |  8380 | `		}` |
|       - |  8381 | `		/* Assemble method signature only */` |
|   99591 |  8382 | `		doBody = FALSE;` |
|   49793 |  8383 | `	}` |
|  288317 |  8384 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  8385 | `		/* Syntax error */` |
|     ! 0 |  8386 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  8387 | `		if( rc == SXERR_ABORT ){` |
|       - |  8388 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8389 | `			return SXERR_ABORT;` |
|       - |  8390 | `		}` |
|     ! 0 |  8391 | `		goto Synchronize;` |
|       - |  8392 | `	}` |
|       - |  8393 | `	/* Allocate a new class_method instance */` |
|  288317 |  8394 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  288317 |  8395 | `	if( pMeth == 0 ){` |
|     ! 0 |  8396 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8397 | `		return SXERR_ABORT;` |
|       - |  8398 | `	}` |
|       - |  8399 | `	/* Jump the left parenthesis '(' */` |
|  288317 |  8400 | `	pGen->pIn++;` |
|  288317 |  8401 | `	pEnd = 0; /* cc warning */` |
|       - |  8402 | `	/* Delimit the method signature */` |
|  288317 |  8403 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  288317 |  8404 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8405 | `		/* Syntax error */` |
|       3 |  8406 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  8407 | `		if( rc == SXERR_ABORT ){` |
|       - |  8408 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8409 | `			return SXERR_ABORT;` |
|       - |  8410 | `		}` |
|       3 |  8411 | `		goto Synchronize;` |
|       - |  8412 | `	}` |
|       - |  8413 | `	{` |
|  288315 |  8414 | `		int bIsCtor = 0;` |
|  288315 |  8415 | `		int bAbstractCtor = 0;` |
|  420896 |  8416 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  171083 |  8417 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  276746 |  8418 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   23143 |  8419 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  8420 | `				bAbstractCtor = 1;` |
|       2 |  8421 | `			}else{` |
|   23141 |  8422 | `				bIsCtor = 1;` |
|       - |  8423 | `			}` |
|   11569 |  8424 | `		}` |
|  288315 |  8425 | `		if( pGen->pIn < pEnd ){` |
|       - |  8426 | `			/* Collect method arguments */` |
|   77071 |  8427 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   77071 |  8428 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8429 | `				return SXERR_ABORT;` |
|       - |  8430 | `			}` |
|   38533 |  8431 | `		}` |
|       - |  8432 | `	}` |
|       - |  8433 | `	/* Point past ')' and parse optional return type ': type' */` |
|  288315 |  8434 | `	pGen->pIn = &pEnd[1];` |
|       - |  8435 | `	{` |
|  288315 |  8436 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  288315 |  8437 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  8438 | `			return SXERR_ABORT;` |
|  288315 |  8439 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  8440 | `			goto Synchronize;` |
|       - |  8441 | `		}` |
|       - |  8442 | `	}` |
|       - |  8443 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  8444 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  8445 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  8446 | `	{` |
|  288315 |  8447 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  8448 | `		sxu32 i;` |
|  419093 |  8449 | `		for( i = 0; i < nArg; i++ ){` |
|  130793 |  8450 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  8451 | `			ph7_class_attr *pAttr;` |
|  130793 |  8452 | `			sxi32 iAttrFlags = 0;` |
|       - |  8453 | `			int bArgTyped;` |
|  130793 |  8454 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  130721 |  8455 | `				continue;` |
|       - |  8456 | `			}` |
|       - |  8457 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - |  8458 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - |  8459 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      53 |  8460 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      78 |  8461 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|      77 |  8462 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  8463 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8464 | `					"Cannot declare variadic promoted property");` |
|       3 |  8465 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8466 | `					return SXERR_ABORT;` |
|       - |  8467 | `				}` |
|       3 |  8468 | `				goto Synchronize;` |
|       - |  8469 | `			}` |
|       - |  8470 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  8471 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  8472 | `			 * appear as an alternative of a union type. */` |
|      75 |  8473 | `			if( bArgTyped ){` |
|     104 |  8474 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      66 |  8475 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      66 |  8476 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      33 |  8477 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      71 |  8478 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8479 | `					return SXERR_ABORT;` |
|      71 |  8480 | `				}else if( rc != SXRET_OK ){` |
|       6 |  8481 | `					goto Synchronize;` |
|       - |  8482 | `				}` |
|      31 |  8483 | `			}` |
|       - |  8484 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      71 |  8485 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  8486 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8487 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  8488 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8489 | `					return SXERR_ABORT;` |
|       - |  8490 | `				}` |
|       3 |  8491 | `				goto Synchronize;` |
|       - |  8492 | `			}` |
|      69 |  8493 | `			if( bArgTyped ){` |
|      65 |  8494 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      30 |  8495 | `			}` |
|      69 |  8496 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  8497 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  8498 | `			}` |
|      69 |  8499 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 |  8500 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 |  8501 | `			}` |
|      69 |  8502 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  8503 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  8504 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      26 |  8505 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  8506 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8507 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  8508 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8509 | `						return SXERR_ABORT;` |
|       - |  8510 | `					}` |
|       3 |  8511 | `					goto Synchronize;` |
|       - |  8512 | `				}` |
|      24 |  8513 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|      10 |  8514 | `			}` |
|      67 |  8515 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      67 |  8516 | `			if( pAttr == 0 ){` |
|     ! 0 |  8517 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8518 | `				return SXERR_ABORT;` |
|       - |  8519 | `			}` |
|      67 |  8520 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      65 |  8521 | `				pAttr->nType = pArg->nType;` |
|      65 |  8522 | `				pAttr->sClass = pArg->sClass;` |
|      65 |  8523 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      65 |  8524 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  8525 | `					sxu32 k;` |
|      20 |  8526 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 |  8527 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 |  8528 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 |  8529 | `					}` |
|       3 |  8530 | `				}` |
|      30 |  8531 | `			}` |
|      67 |  8532 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      67 |  8533 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8534 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8535 | `				return SXERR_ABORT;` |
|       - |  8536 | `			}` |
|      36 |  8537 | `		}` |
|       - |  8538 | `	}` |
|  288305 |  8539 | `	if( doBody ){` |
|       - |  8540 | `		/* Compile method body */` |
|  188719 |  8541 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  188719 |  8542 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8543 | `			return SXERR_ABORT;` |
|       - |  8544 | `		}` |
|   94362 |  8545 | `	}else{` |
|       - |  8546 | `		/* Only method signature is allowed */` |
|   99591 |  8547 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  8548 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8549 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  8550 | `				if( rc == SXERR_ABORT ){` |
|       - |  8551 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8552 | `					return SXERR_ABORT;` |
|       - |  8553 | `				}` |
|     ! 0 |  8554 | `				return SXERR_CORRUPT;` |
|       - |  8555 | `			}` |
|       - |  8556 | `	}` |
|       - |  8557 | `	/* All done,install the method */` |
|  288305 |  8558 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  288305 |  8559 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8560 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8561 | `		return SXERR_ABORT;` |
|       - |  8562 | `	}` |
|  288305 |  8563 | `	return SXRET_OK;` |
|       6 |  8564 | `Synchronize:` |
|       - |  8565 | `	/* Synchronize with the first semi-colon */` |
|      40 |  8566 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  8567 | `		pGen->pIn++;` |
|       4 |  8568 | `	}` |
|      16 |  8569 | `	return SXERR_CORRUPT;` |
|  144161 |  8570 | `}` |
|       - |  8571 | `/*` |
|       - |  8572 | ` * Compile an object interface.` |
|       - |  8573 | ` *  According to the PHP language reference manual` |
|       - |  8574 | ` *   Object Interfaces:` |
|       - |  8575 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  8576 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  8577 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  8578 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  8579 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  8580 | ` */` |
|   42192 |  8581 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  8582 | `{` |
|   42197 |  8583 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8584 | `	ph7_class *pClass,*pBase;` |
|       - |  8585 | `	SyToken *pEnd,*pTmp;` |
|       - |  8586 | `	SyString *pName;` |
|       - |  8587 | `	sxi32 nKwrd;` |
|       - |  8588 | `	sxi32 rc;` |
|       - |  8589 | `	/* Jump the 'interface' keyword */` |
|   42197 |  8590 | `	pGen->pIn++;` |
|       - |  8591 | `	/* Extract interface name */` |
|   42197 |  8592 | `	pName = &pGen->pIn->sData;` |
|       - |  8593 | `	/* Advance the stream cursor */` |
|   42197 |  8594 | `	pGen->pIn++;` |
|       - |  8595 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8596 | `		SyBlob sFQN;` |
|       - |  8597 | `		SyString sFQNStr;` |
|   42197 |  8598 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   42197 |  8599 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   42197 |  8600 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   42197 |  8601 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   42197 |  8602 | `		SyBlobRelease(&sFQN);` |
|       - |  8603 | `	}` |
|   42197 |  8604 | `	if( pClass == 0 ){` |
|     ! 0 |  8605 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8606 | `		return SXERR_ABORT;` |
|       - |  8607 | `	}` |
|       - |  8608 | `	/* Mark as an interface */` |
|   42197 |  8609 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  8610 | `	/* Assume no base class is given */` |
|   42197 |  8611 | `	pBase = 0;` |
|   42197 |  8612 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   11497 |  8613 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   11497 |  8614 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  8615 | `			SyBlob sResolved;` |
|       - |  8616 | `			SyString sBaseName;` |
|       - |  8617 | `			sxu32 nRefLine;` |
|       - |  8618 | `			/* Extract base interface */` |
|   11497 |  8619 | `			pGen->pIn++;` |
|   11497 |  8620 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   11497 |  8621 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   11497 |  8622 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8623 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  8624 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8625 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  8626 | `					pName);` |
|     ! 0 |  8627 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8628 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8629 | `					return SXERR_ABORT;` |
|       - |  8630 | `				}` |
|     ! 0 |  8631 | `				return SXRET_OK;` |
|       - |  8632 | `			}` |
|   17243 |  8633 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   11492 |  8634 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   11497 |  8635 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8636 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8637 | `			/* Only interfaces is allowed */` |
|   11497 |  8638 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8639 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8640 | `			}` |
|   11497 |  8641 | `			if( pBase == 0 ){` |
|     ! 0 |  8642 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8643 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  8644 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8645 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8646 | `					return SXERR_ABORT;` |
|       - |  8647 | `				}` |
|     ! 0 |  8648 | `			}` |
|   11497 |  8649 | `			SyBlobRelease(&sResolved);` |
|    5746 |  8650 | `		}` |
|    5746 |  8651 | `	}` |
|   42197 |  8652 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8653 | `		/* Syntax error */` |
|     ! 0 |  8654 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  8655 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8656 | `		if( rc == SXERR_ABORT ){` |
|       - |  8657 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8658 | `			return SXERR_ABORT;` |
|       - |  8659 | `		}` |
|     ! 0 |  8660 | `		return SXRET_OK;` |
|       - |  8661 | `	}` |
|   42197 |  8662 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   42197 |  8663 | `	pEnd = 0; /* cc warning */` |
|       - |  8664 | `	/* Delimit the interface body */` |
|   42197 |  8665 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   42197 |  8666 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8667 | `		/* Syntax error */` |
|     ! 0 |  8668 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  8669 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8670 | `		if( rc == SXERR_ABORT ){` |
|       - |  8671 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8672 | `			return SXERR_ABORT;` |
|       - |  8673 | `		}` |
|     ! 0 |  8674 | `		return SXRET_OK;` |
|       - |  8675 | `	}` |
|       - |  8676 | `	/* Swap token stream */` |
|   42197 |  8677 | `	pTmp = pGen->pEnd;` |
|   42197 |  8678 | `	pGen->pEnd = pEnd;` |
|       - |  8679 | `	/* Start the parse process` |
|       - |  8680 | `	 * Note (According to the PHP reference manual):` |
|       - |  8681 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  8682 | `	 *  Only 'public' visibility is allowed.` |
|       - |  8683 | `	 */` |
|   70884 |  8684 | `	for(;;){` |
|       - |  8685 | `		/* Jump leading/trailing semi-colons */` |
|  241349 |  8686 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   99581 |  8687 | `			pGen->pIn++;` |
|       5 |  8688 | `		}` |
|  141773 |  8689 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8690 | `			/* End of interface body */` |
|   42193 |  8691 | `			break;` |
|       - |  8692 | `		}` |
|   99585 |  8693 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8694 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8695 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  8696 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8697 | `			if( rc == SXERR_ABORT ){` |
|       - |  8698 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8699 | `				return SXERR_ABORT;` |
|       - |  8700 | `			}` |
|     ! 0 |  8701 | `			goto done;` |
|       - |  8702 | `		}` |
|       - |  8703 | `		/* Extract the current keyword */` |
|   99585 |  8704 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   99585 |  8705 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  8706 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  8707 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  8708 | `			const char *zKind = "member";` |
|       3 |  8709 | `			SyString *pMemberName = 0;` |
|       3 |  8710 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  8711 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  8712 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  8713 | `					zKind = "constant";` |
|       3 |  8714 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  8715 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  8716 | `					}` |
|       1 |  8717 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8718 | `					zKind = "method";` |
|     ! 0 |  8719 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  8720 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  8721 | `					}` |
|     ! 0 |  8722 | `				}` |
|       1 |  8723 | `			}` |
|       3 |  8724 | `			if( pMemberName ){` |
|       4 |  8725 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  8726 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  8727 | `			}else{` |
|     ! 0 |  8728 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8729 | `					"Access type for interface %s must be public",zKind);` |
|       - |  8730 | `			}` |
|       3 |  8731 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8732 | `				return SXERR_ABORT;` |
|       - |  8733 | `			}` |
|       3 |  8734 | `			goto done;` |
|       - |  8735 | `		}` |
|   99583 |  8736 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8737 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8738 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8739 | `			if( rc == SXERR_ABORT ){` |
|       - |  8740 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8741 | `				return SXERR_ABORT;` |
|       - |  8742 | `			}` |
|     ! 0 |  8743 | `			goto done;` |
|       - |  8744 | `		}` |
|   99583 |  8745 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  8746 | `			/* Advance the stream cursor */` |
|   99571 |  8747 | `			pGen->pIn++;` |
|   99571 |  8748 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8749 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8750 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8751 | `				if( rc == SXERR_ABORT ){` |
|       - |  8752 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8753 | `					return SXERR_ABORT;` |
|       - |  8754 | `				}` |
|     ! 0 |  8755 | `				goto done;` |
|       - |  8756 | `			}` |
|   99571 |  8757 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   99571 |  8758 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8759 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8760 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8761 | `				if( rc == SXERR_ABORT ){` |
|       - |  8762 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8763 | `					return SXERR_ABORT;` |
|       - |  8764 | `				}` |
|     ! 0 |  8765 | `				goto done;` |
|       - |  8766 | `			}` |
|   49783 |  8767 | `		}` |
|   99583 |  8768 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8769 | `			/* Parse constant */` |
|      10 |  8770 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|      10 |  8771 | `			if( rc != SXRET_OK ){` |
|       3 |  8772 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8773 | `					return SXERR_ABORT;` |
|       - |  8774 | `				}` |
|       3 |  8775 | `				goto done;` |
|       - |  8776 | `			}` |
|       4 |  8777 | `		}else{` |
|   99575 |  8778 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   99575 |  8779 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8780 | `				/* Static method,record that */` |
|   11489 |  8781 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  8782 | `				/* Advance the stream cursor */` |
|   11489 |  8783 | `				pGen->pIn++;` |
|   11484 |  8784 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|   11489 |  8785 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8786 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8787 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8788 | `						if( rc == SXERR_ABORT ){` |
|       - |  8789 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8790 | `							return SXERR_ABORT;` |
|       - |  8791 | `						}` |
|     ! 0 |  8792 | `						goto done;` |
|       - |  8793 | `				}` |
|    5742 |  8794 | `			}` |
|       - |  8795 | `			/* Process method signature (no body for interface methods) */` |
|   99575 |  8796 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   99575 |  8797 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8798 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8799 | `					return SXERR_ABORT;` |
|       - |  8800 | `				}` |
|     ! 0 |  8801 | `				goto done;` |
|       - |  8802 | `			}` |
|       - |  8803 | `		}` |
|       5 |  8804 | `	}` |
|       - |  8805 | `	/* Install the interface */` |
|   42193 |  8806 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   42193 |  8807 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  8808 | `		/* Inherit from the base interface */` |
|   11497 |  8809 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    5746 |  8810 | `	}` |
|   42193 |  8811 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8812 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8813 | `		return SXERR_ABORT;` |
|       - |  8814 | `	}` |
|   21094 |  8815 | `done:` |
|       - |  8816 | `	/* Point beyond the interface body */` |
|   42197 |  8817 | `	pGen->pIn  = &pEnd[1];` |
|   42197 |  8818 | `	pGen->pEnd = pTmp;` |
|   42197 |  8819 | `	return PH7_OK;` |
|   21101 |  8820 | `}` |
|       - |  8821 | `/*` |
|       - |  8822 | ` * Compile a user-defined class.` |
|       - |  8823 | ` * According to the PHP language reference manual` |
|       - |  8824 | ` *  class` |
|       - |  8825 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  8826 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  8827 | ` *  of the properties and methods belonging to the class.` |
|       - |  8828 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  8829 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  8830 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  8831 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  8832 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  8833 | ` *  (called "methods").` |
|       - |  8834 | ` */` |
|       - |  8835 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  8836 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  8837 | `struct TraitUseEntry {` |
|       - |  8838 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  8839 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  8840 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  8841 | `};` |
|       - |  8842 | `/*` |
|       - |  8843 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  8844 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  8845 | ` */` |
|  112312 |  8846 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8847 | `{` |
|       - |  8848 | `	ph7_class **apIface;` |
|       - |  8849 | `	sxu32 nIface,i;` |
|       - |  8850 | `	sxi32 rc;` |
|  112317 |  8851 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  8852 | `		return SXRET_OK;` |
|       - |  8853 | `	}` |
|  112317 |  8854 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  112317 |  8855 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  215935 |  8856 | `	for(i = 0; i < nIface; i++){` |
|  103623 |  8857 | `		ph7_class *pIface = apIface[i];` |
|       - |  8858 | `		SyHashEntry *pEntry;` |
|  103623 |  8859 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  276357 |  8860 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  172739 |  8861 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  8862 | `			ph7_class_method *pImplMeth;` |
|  172739 |  8863 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  8864 | `			/* Find the implementing method in the class */` |
|  172739 |  8865 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  172739 |  8866 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  8867 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  8868 | `			}` |
|       - |  8869 | `			/* Check visibility: interface methods must be implemented as public */` |
|  172725 |  8870 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  8871 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8872 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  8873 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  8874 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8875 | `					return SXERR_ABORT;` |
|       - |  8876 | `				}` |
|       1 |  8877 | `			}` |
|       - |  8878 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  8879 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  8880 | `			 */` |
|       - |  8881 | `			{` |
|  172725 |  8882 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  172725 |  8883 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  172725 |  8884 | `				int sigError = 0;` |
|  172725 |  8885 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  8886 | `					sigError = 1;` |
|  172724 |  8887 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  8888 | `					/* Extra parameters must all have default values */` |
|       6 |  8889 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  8890 | `					sxu32 k;` |
|       8 |  8891 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  8892 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  8893 | `							sigError = 1;` |
|       3 |  8894 | `							break;` |
|       - |  8895 | `						}` |
|       2 |  8896 | `					}` |
|       2 |  8897 | `				}` |
|  172725 |  8898 | `				if( sigError ){` |
|       - |  8899 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  8900 | `					ph7_vm_func_arg *aArgs;` |
|       - |  8901 | `					sxu32 j;` |
|       6 |  8902 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  8903 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  8904 | `					/* Build implementing method signature */` |
|       6 |  8905 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  8906 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  8907 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  8908 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  8909 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8910 | `					}` |
|       - |  8911 | `					/* Build interface method signature */` |
|       6 |  8912 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  8913 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  8914 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  8915 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  8916 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8917 | `					}` |
|       8 |  8918 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8919 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  8920 | `						&pClass->sName,pMName,` |
|       4 |  8921 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  8922 | `						&pIface->sName,pMName,` |
|       4 |  8923 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  8924 | `					SyBlobRelease(&sImplSig);` |
|       6 |  8925 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  8926 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8927 | `						return SXERR_ABORT;` |
|       - |  8928 | `					}` |
|       2 |  8929 | `				}` |
|       - |  8930 | `			}` |
|       5 |  8931 | `		}` |
|   51814 |  8932 | `	}` |
|  112317 |  8933 | `	return SXRET_OK;` |
|   56161 |  8934 | `}` |
|       - |  8935 | `/*` |
|       - |  8936 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  8937 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  8938 | ` */` |
|  112312 |  8939 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8940 | `{` |
|       - |  8941 | `	ph7_class_method *pMeth;` |
|       - |  8942 | `	SyHashEntry *pEntry;` |
|       - |  8943 | `	sxu32 nAbstract;` |
|       - |  8944 | `	SyBlob sMsg;` |
|       - |  8945 | `	sxi32 rc;` |
|       - |  8946 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|  112317 |  8947 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      37 |  8948 | `		return SXRET_OK;` |
|       - |  8949 | `	}` |
|       - |  8950 | `	/* Count abstract methods */` |
|  112285 |  8951 | `	nAbstract = 0;` |
|  112285 |  8952 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
| 1055775 |  8953 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  943495 |  8954 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  943495 |  8955 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  8956 | `			nAbstract++;` |
|       8 |  8957 | `		}` |
|       5 |  8958 | `	}` |
|  112285 |  8959 | `	if( nAbstract == 0 ){` |
|  112271 |  8960 | `		return SXRET_OK;` |
|       - |  8961 | `	}` |
|       - |  8962 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  8963 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  8964 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  8965 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  8966 | `		&pClass->sName,nAbstract,` |
|       7 |  8967 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  8968 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  8969 | `	/* Second pass: list methods with origins */` |
|       - |  8970 | `	{` |
|      18 |  8971 | `		sxu32 nListed = 0;` |
|      18 |  8972 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  8973 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  8974 | `			ph7_class *pOrigin = 0;` |
|       - |  8975 | `			SyString *pMName;` |
|      22 |  8976 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  8977 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  8978 | `				continue;` |
|       - |  8979 | `			}` |
|      20 |  8980 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  8981 | `			if( nListed > 0 ){` |
|       3 |  8982 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  8983 | `			}` |
|       - |  8984 | `			/* Find the origin of this abstract method.` |
|       - |  8985 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  8986 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  8987 | `			 * methods. Abstract class methods only win when the class` |
|       - |  8988 | `			 * itself declared the abstract method (not inherited from` |
|       - |  8989 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  8990 | `			 * class's namespace.` |
|       - |  8991 | `			 */` |
|       - |  8992 | `			{` |
|       - |  8993 | `				ph7_class **apIface;` |
|       - |  8994 | `				ph7_class **apTrait;` |
|       - |  8995 | `				ph7_class *pWalk;` |
|       - |  8996 | `				sxu32 i;` |
|       - |  8997 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  8998 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  8999 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  9000 | `				 */` |
|      20 |  9001 | `				if( pClass->pBase ){` |
|      11 |  9002 | `					pWalk = pClass->pBase;` |
|      19 |  9003 | `					while( pWalk ){` |
|       - |  9004 | `						ph7_class_method *pParentMeth;` |
|      13 |  9005 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  9006 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  9007 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  9008 | `							 * in this class's ancestor chain.` |
|       - |  9009 | `							 */` |
|      13 |  9010 | `							int fromIface = 0;` |
|      13 |  9011 | `							ph7_class *pAnc = pWalk;` |
|      17 |  9012 | `							while( pAnc ){` |
|       - |  9013 | `								ph7_class **apPI;` |
|       - |  9014 | `								sxu32 j;` |
|      15 |  9015 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  9016 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  9017 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  9018 | `										fromIface = 1;` |
|      10 |  9019 | `										break;` |
|       - |  9020 | `									}` |
|     ! 0 |  9021 | `								}` |
|      15 |  9022 | `								if( fromIface ) break;` |
|       6 |  9023 | `								pAnc = pAnc->pBase;` |
|       2 |  9024 | `							}` |
|      13 |  9025 | `							if( !fromIface ){` |
|       3 |  9026 | `								pOrigin = pWalk;` |
|       3 |  9027 | `								break;` |
|       - |  9028 | `							}` |
|       4 |  9029 | `						}` |
|      10 |  9030 | `						pWalk = pWalk->pBase;` |
|       2 |  9031 | `					}` |
|       4 |  9032 | `				}` |
|       - |  9033 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  9034 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  9035 | `				 */` |
|      20 |  9036 | `				if( !pOrigin ){` |
|      18 |  9037 | `					pWalk = pClass;` |
|      40 |  9038 | `					while( pWalk && !pOrigin ){` |
|      26 |  9039 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  9040 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  9041 | `							ph7_class *pIface = apIface[i];` |
|      16 |  9042 | `							ph7_class *pDeepest = 0;` |
|      28 |  9043 | `							while( pIface ){` |
|      16 |  9044 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  9045 | `									pDeepest = pIface;` |
|       6 |  9046 | `								}` |
|      16 |  9047 | `								pIface = pIface->pBase;` |
|       4 |  9048 | `							}` |
|      16 |  9049 | `							if( pDeepest ){` |
|      16 |  9050 | `								pOrigin = pDeepest;` |
|      16 |  9051 | `								break;` |
|       - |  9052 | `							}` |
|     ! 0 |  9053 | `						}` |
|      26 |  9054 | `						pWalk = pWalk->pBase;` |
|       4 |  9055 | `					}` |
|       7 |  9056 | `				}` |
|       - |  9057 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  9058 | `				if( !pOrigin ){` |
|       3 |  9059 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  9060 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  9061 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  9062 | `							pOrigin = pClass;` |
|       3 |  9063 | `							break;` |
|       - |  9064 | `						}` |
|     ! 0 |  9065 | `					}` |
|       1 |  9066 | `				}` |
|       - |  9067 | `			}` |
|      20 |  9068 | `			if( pOrigin ){` |
|      20 |  9069 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  9070 | `			}else{` |
|       - |  9071 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  9072 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  9073 | `			}` |
|      20 |  9074 | `			nListed++;` |
|       4 |  9075 | `		}` |
|       - |  9076 | `	}` |
|      18 |  9077 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  9078 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  9079 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  9080 | `	SyBlobRelease(&sMsg);` |
|      18 |  9081 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9082 | `		return SXERR_ABORT;` |
|       - |  9083 | `	}` |
|      18 |  9084 | `	return SXRET_OK;` |
|   56161 |  9085 | `}` |
|       - |  9086 | `/*` |
|       - |  9087 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  9088 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  9089 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  9090 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  9091 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  9092 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  9093 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  9094 | ` */` |
|  108624 |  9095 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  9096 | `{` |
|  108629 |  9097 | `	int isAbsolute = 0;` |
|  108629 |  9098 | `	SyToken *pStart = pGen->pIn;` |
|       - |  9099 | `	SyBlob sName;` |
|  108629 |  9100 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|    4271 |  9101 | `		isAbsolute = 1;` |
|    4271 |  9102 | `		pGen->pIn++;` |
|    2133 |  9103 | `	}` |
|  108629 |  9104 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       8 |  9105 | `		pGen->pIn = pStart;` |
|       8 |  9106 | `		return SXERR_INVALID;` |
|       - |  9107 | `	}` |
|  108623 |  9108 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|  108623 |  9109 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|  108623 |  9110 | `	pGen->pIn++;` |
|  162948 |  9111 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   54335 |  9112 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      16 |  9113 | `		SyBlobAppend(&sName,"\\",1);` |
|      16 |  9114 | `		pGen->pIn++;` |
|      16 |  9115 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      16 |  9116 | `		pGen->pIn++;` |
|       2 |  9117 | `	}` |
|  108623 |  9118 | `	if( isAbsolute ){` |
|    4269 |  9119 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    2137 |  9120 | `	}else{` |
|       - |  9121 | `		SyString sRaw;` |
|  104359 |  9122 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|  104359 |  9123 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  9124 | `	}` |
|  108623 |  9125 | `	SyBlobRelease(&sName);` |
|  108623 |  9126 | `	return SXRET_OK;` |
|   54317 |  9127 | `}` |
|       - |  9128 | `/*` |
|       - |  9129 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  9130 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  9131 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  9132 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  9133 | ` * either direction cannot run unbounded.` |
|       - |  9134 | ` */` |
|       - |  9135 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   11664 |  9136 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  9137 | `{` |
|       - |  9138 | `	ph7_class **apParent;` |
|       - |  9139 | `	sxu32 n;` |
|   19539 |  9140 | `	while( pInterface ){` |
|   15541 |  9141 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  9142 | `			return FALSE;` |
|       - |  9143 | `		}` |
|   19384 |  9144 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    7686 |  9145 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    7671 |  9146 | `			return TRUE;` |
|       - |  9147 | `		}` |
|    7875 |  9148 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    7875 |  9149 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  9150 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  9151 | `				return TRUE;` |
|       - |  9152 | `			}` |
|     ! 0 |  9153 | `		}` |
|    7875 |  9154 | `		pInterface = pInterface->pBase;` |
|    7875 |  9155 | `		iDepth++;` |
|       5 |  9156 | `	}` |
|    4003 |  9157 | `	return FALSE;` |
|    5837 |  9158 | `}` |
|   11664 |  9159 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  9160 | `{` |
|   11669 |  9161 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  9162 | `}` |
|       - |  9163 | `/*` |
|       - |  9164 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  9165 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  9166 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  9167 | ` */` |
|    7666 |  9168 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  9169 | `{` |
|    7675 |  9170 | `	while( pBase ){` |
|      10 |  9171 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  9172 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  9173 | `			return TRUE;` |
|       - |  9174 | `		}` |
|      10 |  9175 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  9176 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  9177 | `			return TRUE;` |
|       - |  9178 | `		}` |
|       5 |  9179 | `		pBase = pBase->pBase;` |
|       1 |  9180 | `	}` |
|    7667 |  9181 | `	return FALSE;` |
|    3838 |  9182 | `}` |
|       - |  9183 | `/*` |
|       - |  9184 | ` * Compile a class declaration, named or anonymous.` |
|       - |  9185 | ` *` |
|       - |  9186 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - |  9187 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - |  9188 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - |  9189 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - |  9190 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - |  9191 | ` * implements, body, install) is shared by both paths.` |
|       - |  9192 | ` */` |
|  112352 |  9193 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - |  9194 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 |  9195 | `{` |
|  112357 |  9196 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9197 | `	ph7_class *pClass,*pBase;` |
|       - |  9198 | `	SyToken *pEnd,*pTmp;` |
|       - |  9199 | `	sxi32 iProtection;` |
|       - |  9200 | `	SySet aInterfaces;` |
|       - |  9201 | `	SySet aUseEntries;` |
|       - |  9202 | `	sxi32 iAttrflags;` |
|       - |  9203 | `	SyString *pName;` |
|       - |  9204 | `	sxi32 nKwrd;` |
|       - |  9205 | `	sxi32 rc;` |
|       - |  9206 | `	/* Jump the 'class' keyword */` |
|  112357 |  9207 | `	pGen->pIn++;` |
|  112357 |  9208 | `	if( pAnonName ){` |
|       - |  9209 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - |  9210 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - |  9211 | `		 * then use the synthesized name. */` |
|      30 |  9212 | `		*ppArgStart = *ppArgEnd = 0;` |
|      30 |  9213 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       7 |  9214 | `			pGen->pIn++; /* Jump '(' */` |
|       7 |  9215 | `			*ppArgStart = pGen->pIn;` |
|      10 |  9216 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       3 |  9217 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       7 |  9218 | `			pGen->pIn = *ppArgEnd;` |
|       7 |  9219 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       3 |  9220 | `		}` |
|      30 |  9221 | `		pName = pAnonName;` |
|      30 |  9222 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      17 |  9223 | `	}else{` |
|  112331 |  9224 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  9225 | `			/* Syntax error */` |
|     ! 0 |  9226 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  9227 | `			if( rc == SXERR_ABORT ){` |
|       - |  9228 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  9229 | `				return SXERR_ABORT;` |
|       - |  9230 | `			}` |
|       - |  9231 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  9232 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  9233 | `				pGen->pIn++;` |
|     ! 0 |  9234 | `			}` |
|     ! 0 |  9235 | `			return SXRET_OK;` |
|       - |  9236 | `		}` |
|       - |  9237 | `		/* Extract class name */` |
|  112331 |  9238 | `		pName = &pGen->pIn->sData;` |
|       - |  9239 | `		/* Advance the stream cursor */` |
|  112331 |  9240 | `		pGen->pIn++;` |
|       - |  9241 | `		/* Build FQN and obtain a raw class */ {` |
|       - |  9242 | `			SyBlob sFQN;` |
|       - |  9243 | `			SyString sFQNStr;` |
|  112331 |  9244 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|  112331 |  9245 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|  112331 |  9246 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|  112331 |  9247 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|  112331 |  9248 | `			SyBlobRelease(&sFQN);` |
|       - |  9249 | `		}` |
|       - |  9250 | `	}` |
|  112357 |  9251 | `	if( pClass == 0 ){` |
|     ! 0 |  9252 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9253 | `		return SXERR_ABORT;` |
|       - |  9254 | `	}` |
|       - |  9255 | `	/* implemented interfaces and per-use-statement trait containers */` |
|  112357 |  9256 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|  112357 |  9257 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  9258 | `	/* Assume a standalone class */` |
|  112357 |  9259 | `	pBase = 0;` |
|  112357 |  9260 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   96055 |  9261 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   96055 |  9262 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  9263 | `			SyBlob sResolved;` |
|       - |  9264 | `			SyString sBaseName;` |
|       - |  9265 | `			sxu32 nRefLine;` |
|   84411 |  9266 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   84411 |  9267 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   84411 |  9268 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   84411 |  9269 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  9270 | `				SyBlobRelease(&sResolved);` |
|       4 |  9271 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  9272 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  9273 | `					pName);` |
|       3 |  9274 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  9275 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9276 | `					return SXERR_ABORT;` |
|       - |  9277 | `				}` |
|       3 |  9278 | `				return SXRET_OK;` |
|       - |  9279 | `			}` |
|  126611 |  9280 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   84404 |  9281 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   84409 |  9282 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  9283 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  9284 | `			/* Interfaces are not allowed */` |
|   84409 |  9285 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  9286 | `				pBase = pBase->pNextName;` |
|     ! 0 |  9287 | `			}` |
|   84409 |  9288 | `			if( pBase == 0 ){` |
|     ! 0 |  9289 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  9290 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  9291 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9292 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  9293 | `					return SXERR_ABORT;` |
|       - |  9294 | `				}` |
|     ! 0 |  9295 | `			}else{` |
|   84409 |  9296 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  9297 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  9298 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  9299 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9300 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  9301 | `						return SXERR_ABORT;` |
|       - |  9302 | `					}` |
|     ! 0 |  9303 | `				}` |
|       - |  9304 | `			}` |
|   84409 |  9305 | `			SyBlobRelease(&sResolved);` |
|   42202 |  9306 | `		}` |
|   96053 |  9307 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  9308 | `			ph7_class *pInterface;` |
|       - |  9309 | `			/* Interface implementation */` |
|   11657 |  9310 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5838 |  9311 | `			for(;;){` |
|       - |  9312 | `				SyBlob sResolved;` |
|       - |  9313 | `				SyString sIntName;` |
|       - |  9314 | `				sxu32 nRefLine;` |
|   11669 |  9315 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   11669 |  9316 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   11669 |  9317 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  9318 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  9319 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  9320 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  9321 | `						pName);` |
|     ! 0 |  9322 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9323 | `						return SXERR_ABORT;` |
|       - |  9324 | `					}` |
|     ! 0 |  9325 | `					break;` |
|       - |  9326 | `				}` |
|   23333 |  9327 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   11664 |  9328 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   11669 |  9329 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  9330 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  9331 | `				/* Only interfaces are allowed */` |
|   11669 |  9332 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  9333 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  9334 | `				}` |
|   11669 |  9335 | `				if( pInterface == 0 ){` |
|     ! 0 |  9336 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  9337 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  9338 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9339 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  9340 | `						return SXERR_ABORT;` |
|       - |  9341 | `					}` |
|     ! 0 |  9342 | `				}else{` |
|       - |  9343 | `					/* Reject user classes that try to implement Throwable` |
|       - |  9344 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  9345 | `					 * unless they already extend Exception or Error.` |
|       - |  9346 | `					 * Exception and Error themselves are compiled from the` |
|       - |  9347 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  9348 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   11669 |  9349 | `					SyString *pFqn = &pClass->sName;` |
|   11669 |  9350 | `					int bIsExceptionOrError =` |
|    9664 |  9351 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   19414 |  9352 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    9757 |  9353 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3842 |  9354 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   19328 |  9355 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|   11502 |  9356 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3831 |  9357 | `						!bIsExceptionOrError ){` |
|      12 |  9358 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9359 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  9360 | `							&pClass->sName);` |
|       9 |  9361 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9362 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  9363 | `							return SXERR_ABORT;` |
|       - |  9364 | `						}` |
|       - |  9365 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  9366 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  9367 | `					}else{` |
|   11663 |  9368 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  9369 | `					}` |
|       - |  9370 | `				}` |
|   11669 |  9371 | `				SyBlobRelease(&sResolved);` |
|   11669 |  9372 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5831 |  9373 | `					break;` |
|       - |  9374 | `				}` |
|      16 |  9375 | `				pGen->pIn++;/* Jump the comma */` |
|       4 |  9376 | `			}` |
|    5826 |  9377 | `		}` |
|   48024 |  9378 | `	}` |
|  112355 |  9379 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  9380 | `		/* Syntax error */` |
|     ! 0 |  9381 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  9382 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9383 | `		if( rc == SXERR_ABORT ){` |
|       - |  9384 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9385 | `			return SXERR_ABORT;` |
|       - |  9386 | `		}` |
|     ! 0 |  9387 | `		return SXRET_OK;` |
|       - |  9388 | `	}` |
|  112355 |  9389 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|  112355 |  9390 | `	pEnd = 0; /* cc warning */` |
|       - |  9391 | `	/* Delimit the class body */` |
|  112355 |  9392 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|  112355 |  9393 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  9394 | `		/* Syntax error */` |
|     ! 0 |  9395 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  9396 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9397 | `		if( rc == SXERR_ABORT ){` |
|       - |  9398 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9399 | `			return SXERR_ABORT;` |
|       - |  9400 | `		}` |
|     ! 0 |  9401 | `		return SXRET_OK;` |
|       - |  9402 | `	}` |
|       - |  9403 | `	/* Swap token stream */` |
|  112355 |  9404 | `	pTmp = pGen->pEnd;` |
|  112355 |  9405 | `	pGen->pEnd = pEnd;` |
|       - |  9406 | `	/* Set the inherited flags */` |
|  112355 |  9407 | `	pClass->iFlags = iFlags;` |
|       - |  9408 | `	/* Start the parse process */` |
|  150548 |  9409 | `	for(;;){` |
|       - |  9410 | `		/* Jump leading/trailing semi-colons */` |
|  463423 |  9411 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   81207 |  9412 | `			pGen->pIn++;` |
|       5 |  9413 | `		}` |
|  382221 |  9414 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9415 | `			/* End of class body */` |
|  112317 |  9416 | `			break;` |
|       - |  9417 | `		}` |
|  269904 |  9418 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  134957 |  9419 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  9420 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9421 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  9422 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9423 | `			if( rc == SXERR_ABORT ){` |
|       - |  9424 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  9425 | `				return SXERR_ABORT;` |
|       - |  9426 | `			}` |
|     ! 0 |  9427 | `			goto done;` |
|       - |  9428 | `		}` |
|       - |  9429 | `		/* Assume public visibility */` |
|  269909 |  9430 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  269909 |  9431 | `		iAttrflags = 0;` |
|       - |  9432 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  9433 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  9434 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  9435 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  269909 |  9436 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9437 | `			int bMod = 0;` |
|     ! 0 |  9438 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9439 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  9440 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  9441 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  9442 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  9443 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  9444 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  9445 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  9446 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  9447 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  9448 | `			}` |
|     ! 0 |  9449 | `			if( !bMod ){` |
|     ! 0 |  9450 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9451 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9452 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9453 | `						return SXERR_ABORT;` |
|       - |  9454 | `					}` |
|     ! 0 |  9455 | `					goto done;` |
|       - |  9456 | `				}` |
|     ! 0 |  9457 | `				continue;` |
|       - |  9458 | `			}` |
|     ! 0 |  9459 | `		}` |
|  269909 |  9460 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9461 | `			/* Extract the current keyword */` |
|  269909 |  9462 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  269909 |  9463 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9464 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  9465 | `				TraitUseEntry sUse;` |
|      57 |  9466 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      57 |  9467 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      57 |  9468 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      34 |  9469 | `				for(;;){` |
|       - |  9470 | `					ph7_class *pTrait;` |
|       - |  9471 | `					SyString *pTraitName;` |
|      65 |  9472 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9473 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9474 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  9475 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9476 | `							return SXERR_ABORT;` |
|       - |  9477 | `						}` |
|     ! 0 |  9478 | `						break;` |
|       - |  9479 | `					}` |
|      65 |  9480 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  9481 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  9482 | `						SyBlob sResolved;` |
|      65 |  9483 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      65 |  9484 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     125 |  9485 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      60 |  9486 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      65 |  9487 | `						SyBlobRelease(&sResolved);` |
|       - |  9488 | `					}` |
|       - |  9489 | `					/* Only traits are allowed */` |
|      65 |  9490 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9491 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  9492 | `					}` |
|      65 |  9493 | `					if( pTrait == 0 ){` |
|     ! 0 |  9494 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9495 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  9496 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9497 | `							return SXERR_ABORT;` |
|       - |  9498 | `						}` |
|     ! 0 |  9499 | `					}else{` |
|      65 |  9500 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  9501 | `					}` |
|      65 |  9502 | `					pGen->pIn++; /* Advance past trait name */` |
|      65 |  9503 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      31 |  9504 | `						break;` |
|       - |  9505 | `					}` |
|      10 |  9506 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  9507 | `				}` |
|       - |  9508 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      57 |  9509 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  9510 | `					SyToken *pBlock;` |
|      13 |  9511 | `					pGen->pIn++; /* Jump '{' */` |
|      13 |  9512 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      13 |  9513 | `					sUse.pResolvStart = pGen->pIn;` |
|      13 |  9514 | `					sUse.pResolvEnd = pBlock;` |
|      13 |  9515 | `					if( pBlock < pGen->pEnd ){` |
|      13 |  9516 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       8 |  9517 | `					}else{` |
|     ! 0 |  9518 | `						pGen->pIn = pGen->pEnd;` |
|       - |  9519 | `					}` |
|       5 |  9520 | `				}` |
|      57 |  9521 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  9522 | `				/* The semicolon will be consumed by the outer loop */` |
|      57 |  9523 | `				continue;` |
|       - |  9524 | `			}` |
|  269857 |  9525 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  269511 |  9526 | `				iProtection = nKwrd;` |
|  269511 |  9527 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  9528 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  269511 |  9529 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  9530 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  9531 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  9532 | `				}` |
|  269506 |  9533 | `				if( pGen->pIn >= pGen->pEnd` |
|  269511 |  9534 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9535 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9536 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  9537 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9538 | `					if( rc == SXERR_ABORT ){` |
|       - |  9539 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  9540 | `						return SXERR_ABORT;` |
|       - |  9541 | `					}` |
|     ! 0 |  9542 | `					goto done;` |
|       - |  9543 | `				}` |
|  269511 |  9544 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9545 | `					/* Attribute declaration (untyped) */` |
|   80889 |  9546 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   80889 |  9547 | `					if( rc != SXRET_OK ){` |
|      11 |  9548 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9549 | `							return SXERR_ABORT;` |
|       - |  9550 | `						}` |
|      11 |  9551 | `						goto done;` |
|       - |  9552 | `					}` |
|   80881 |  9553 | `					continue;` |
|       - |  9554 | `				}` |
|  188627 |  9555 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9556 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     175 |  9557 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     175 |  9558 | `					if( rc != SXRET_OK ){` |
|       8 |  9559 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9560 | `							return SXERR_ABORT;` |
|       - |  9561 | `						}` |
|       8 |  9562 | `						goto done;` |
|       - |  9563 | `					}` |
|     169 |  9564 | `					continue;` |
|       - |  9565 | `				}` |
|       - |  9566 | `				/* Extract the keyword */` |
|  188457 |  9567 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   94226 |  9568 | `			}` |
|  188803 |  9569 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  9570 | `				/* Process constant declaration */` |
|      87 |  9571 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      87 |  9572 | `				if( rc != SXRET_OK ){` |
|      11 |  9573 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9574 | `						return SXERR_ABORT;` |
|       - |  9575 | `					}` |
|      11 |  9576 | `					goto done;` |
|       - |  9577 | `				}` |
|      42 |  9578 | `			}else{` |
|  188721 |  9579 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  9580 | `					/* Static method or attribute,record that */` |
|   11563 |  9581 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|   11563 |  9582 | `					pGen->pIn++; /* Jump the static keyword */` |
|   11563 |  9583 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9584 | `						/* Extract the keyword */` |
|   11551 |  9585 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   11551 |  9586 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9587 | `							iProtection = nKwrd;` |
|     ! 0 |  9588 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  9589 | `						}` |
|    5773 |  9590 | `					}` |
|       - |  9591 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  9592 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  9593 | `					 * than a generic "expecting method" parse error. */` |
|   11563 |  9594 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9595 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9596 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  9597 | `					}` |
|   11558 |  9598 | `					if( pGen->pIn >= pGen->pEnd` |
|   11563 |  9599 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9600 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9601 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  9602 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9603 | `						if( rc == SXERR_ABORT ){` |
|       - |  9604 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9605 | `							return SXERR_ABORT;` |
|       - |  9606 | `						}` |
|     ! 0 |  9607 | `						goto done;` |
|       - |  9608 | `					}` |
|   11563 |  9609 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9610 | `						/* Attribute declaration */` |
|      13 |  9611 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      13 |  9612 | `						if( rc != SXRET_OK ){` |
|       3 |  9613 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9614 | `								return SXERR_ABORT;` |
|       - |  9615 | `							}` |
|       3 |  9616 | `							goto done;` |
|       - |  9617 | `						}` |
|      10 |  9618 | `						continue;` |
|       - |  9619 | `					}` |
|   11553 |  9620 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9621 | `						/* Typed static attribute declaration */` |
|      15 |  9622 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  9623 | `						if( rc != SXRET_OK ){` |
|       3 |  9624 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9625 | `								return SXERR_ABORT;` |
|       - |  9626 | `							}` |
|       3 |  9627 | `							goto done;` |
|       - |  9628 | `						}` |
|      13 |  9629 | `						continue;` |
|       - |  9630 | `					}` |
|       - |  9631 | `					/* Extract the keyword */` |
|   11541 |  9632 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  182931 |  9633 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  9634 | `					/* Abstract method,record that */` |
|      15 |  9635 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  9636 | `					/* Mark the whole class as abstract */` |
|      15 |  9637 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  9638 | `					/* Advance the stream cursor */` |
|      15 |  9639 | `					pGen->pIn++;` |
|      15 |  9640 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      15 |  9641 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      15 |  9642 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      13 |  9643 | `							iProtection = nKwrd;` |
|      13 |  9644 | `							pGen->pIn++; /* Jump the visibility token */` |
|       5 |  9645 | `						}` |
|       6 |  9646 | `					}` |
|      15 |  9647 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      12 |  9648 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9649 | `							/* Static method */` |
|     ! 0 |  9650 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9651 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9652 | `					}` |
|      15 |  9653 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      12 |  9654 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9655 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9656 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  9657 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9658 | `							if( rc == SXERR_ABORT ){` |
|       - |  9659 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9660 | `								return SXERR_ABORT;` |
|       - |  9661 | `							}` |
|     ! 0 |  9662 | `							goto done;` |
|       - |  9663 | `					}` |
|      15 |  9664 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  177157 |  9665 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  9666 | `					/* final method ,record that */` |
|      17 |  9667 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      17 |  9668 | `					pGen->pIn++; /* Jump the final keyword */` |
|      17 |  9669 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9670 | `						/* Extract the keyword */` |
|      17 |  9671 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      17 |  9672 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       9 |  9673 | `							iProtection = nKwrd;` |
|       9 |  9674 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  9675 | `						}` |
|       7 |  9676 | `					}` |
|      17 |  9677 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  9678 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  9679 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  9680 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  9681 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  9682 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9683 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  9684 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  9685 | `									return SXERR_ABORT;` |
|       - |  9686 | `								}` |
|     ! 0 |  9687 | `								goto done;` |
|       - |  9688 | `							}` |
|      12 |  9689 | `							continue;` |
|       - |  9690 | `					}` |
|       6 |  9691 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  9692 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9693 | `							/* Static method */` |
|     ! 0 |  9694 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9695 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9696 | `					}` |
|       6 |  9697 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9698 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9699 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9700 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  9701 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9702 | `							if( rc == SXERR_ABORT ){` |
|       - |  9703 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9704 | `								return SXERR_ABORT;` |
|       - |  9705 | `							}` |
|     ! 0 |  9706 | `							goto done;` |
|       - |  9707 | `					}` |
|       6 |  9708 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9709 | `				}` |
|  188689 |  9710 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9711 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9712 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  9713 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9714 | `						if( rc == SXERR_ABORT ){` |
|       - |  9715 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9716 | `							return SXERR_ABORT;` |
|       - |  9717 | `						}` |
|     ! 0 |  9718 | `						goto done;` |
|       - |  9719 | `				}` |
|  188689 |  9720 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  9721 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  9722 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  9723 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9724 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9725 | `						if( rc == SXERR_ABORT ){` |
|       - |  9726 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9727 | `							return SXERR_ABORT;` |
|       - |  9728 | `						}` |
|     ! 0 |  9729 | `						goto done;` |
|       - |  9730 | `					}` |
|       - |  9731 | `					/* Attribute declaration */` |
|       7 |  9732 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  9733 | `				}else{` |
|       - |  9734 | `					/* Process method declaration */` |
|  188683 |  9735 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9736 | `				}` |
|  188689 |  9737 | `				if( rc != SXRET_OK ){` |
|      16 |  9738 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9739 | `						return SXERR_ABORT;` |
|       - |  9740 | `					}` |
|      16 |  9741 | `					goto done;` |
|       - |  9742 | `				}` |
|       - |  9743 | `			}` |
|   94378 |  9744 | `		}else{` |
|       - |  9745 | `			/* Attribute declaration */` |
|     ! 0 |  9746 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9747 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9748 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9749 | `					return SXERR_ABORT;` |
|       - |  9750 | `				}` |
|     ! 0 |  9751 | `				goto done;` |
|       - |  9752 | `			}` |
|       - |  9753 | `		}` |
|       5 |  9754 | `	}` |
|       - |  9755 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  9756 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  9757 | `	 */` |
|       - |  9758 | `	{` |
|       - |  9759 | `		TraitUseEntry *apUse;` |
|       - |  9760 | `		sxu32 nU;` |
|  112317 |  9761 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|  112369 |  9762 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      57 |  9763 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      57 |  9764 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      57 |  9765 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      57 |  9766 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  9767 | `			sxu32 nT;` |
|      57 |  9768 | `			if( !hasResolution ){` |
|       - |  9769 | `				/* No conflict resolution block: use standard trait application */` |
|      95 |  9770 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      53 |  9771 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      53 |  9772 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9773 | `						break;` |
|       - |  9774 | `					}` |
|      29 |  9775 | `				}` |
|      26 |  9776 | `			}else{` |
|       - |  9777 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  9778 | `				 * then use the block to resolve method conflicts.` |
|       - |  9779 | `				 */` |
|       - |  9780 | `				SyToken *pR;` |
|      25 |  9781 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      15 |  9782 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  9783 | `					ph7_class_attr *pAR;` |
|       - |  9784 | `					SyHashEntry *pER;` |
|       - |  9785 | `					SyString *pNR;` |
|      15 |  9786 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      21 |  9787 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  9788 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  9789 | `						pNR = &pAR->sName;` |
|     ! 0 |  9790 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  9791 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  9792 | `						}` |
|     ! 0 |  9793 | `					}` |
|      15 |  9794 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       9 |  9795 | `				}` |
|       - |  9796 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      13 |  9797 | `				pR = pUse->pResolvStart;` |
|      27 |  9798 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9799 | `					SyString sTrait,sMethod;` |
|       - |  9800 | `					ph7_class *pSrcTrait;` |
|       - |  9801 | `					ph7_class_method *pMeth;` |
|       - |  9802 | `					sxi32 nRKwrd;` |
|      41 |  9803 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9804 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9805 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9806 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9807 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9808 | `					sMethod = pR->sData;` |
|      17 |  9809 | `					pR++;` |
|      17 |  9810 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9811 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9812 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9813 | `							sTrait = sMethod;` |
|       7 |  9814 | `							pR++;` |
|       7 |  9815 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9816 | `							sMethod = pR->sData;` |
|       7 |  9817 | `							pR++;` |
|       3 |  9818 | `						}` |
|       3 |  9819 | `					}` |
|      17 |  9820 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9821 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9822 | `						continue;` |
|       - |  9823 | `					}` |
|      17 |  9824 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9825 | `					pR++;` |
|      17 |  9826 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  9827 | `						pSrcTrait = 0;` |
|       7 |  9828 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  9829 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  9830 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  9831 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  9832 | `								pSrcTrait = apTrait[nT];` |
|       5 |  9833 | `								break;` |
|       - |  9834 | `							}` |
|       2 |  9835 | `						}` |
|       5 |  9836 | `						if( pSrcTrait ){` |
|       5 |  9837 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  9838 | `							if( pMeth ){` |
|       5 |  9839 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  9840 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  9841 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  9842 | `								}` |
|       2 |  9843 | `							}` |
|       2 |  9844 | `						}` |
|       2 |  9845 | `					}` |
|      35 |  9846 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9847 | `				}` |
|       - |  9848 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      25 |  9849 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  9850 | `					ph7_class_method *pMR;` |
|       - |  9851 | `					SyHashEntry *pER;` |
|       - |  9852 | `					SyString *pNR;` |
|      15 |  9853 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      41 |  9854 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      23 |  9855 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      23 |  9856 | `						pNR = &pMR->sFunc.sName;` |
|      23 |  9857 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      14 |  9858 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       6 |  9859 | `						}` |
|       3 |  9860 | `					}` |
|       9 |  9861 | `				}` |
|       - |  9862 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      13 |  9863 | `				pR = pUse->pResolvStart;` |
|      27 |  9864 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9865 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  9866 | `					ph7_class *pSrcTrait;` |
|       - |  9867 | `					ph7_class_method *pMeth;` |
|      27 |  9868 | `					int hasQual = 0;` |
|       - |  9869 | `					sxi32 nRKwrd;` |
|      41 |  9870 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9871 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9872 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9873 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9874 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      17 |  9875 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9876 | `					sMethod = pR->sData;` |
|      17 |  9877 | `					pR++;` |
|      17 |  9878 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9879 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9880 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9881 | `							sTrait = sMethod;` |
|       7 |  9882 | `							hasQual = 1;` |
|       7 |  9883 | `							pR++;` |
|       7 |  9884 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9885 | `							sMethod = pR->sData;` |
|       7 |  9886 | `							pR++;` |
|       3 |  9887 | `						}` |
|       3 |  9888 | `					}` |
|      17 |  9889 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9890 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9891 | `						continue;` |
|       - |  9892 | `					}` |
|      17 |  9893 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9894 | `					pR++;` |
|      17 |  9895 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      13 |  9896 | `						sxi32 iNewVis = -1;` |
|      13 |  9897 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  9898 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  9899 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  9900 | `								iNewVis = nAK;` |
|       7 |  9901 | `								pR++;` |
|       3 |  9902 | `							}` |
|       3 |  9903 | `						}` |
|      13 |  9904 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      11 |  9905 | `							sAlias = pR->sData;` |
|      11 |  9906 | `							pR++;` |
|       4 |  9907 | `						}` |
|      13 |  9908 | `						pMeth = 0;` |
|      13 |  9909 | `						if( hasQual ){` |
|       3 |  9910 | `							pSrcTrait = 0;` |
|       5 |  9911 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  9912 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  9913 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  9914 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  9915 | `									pSrcTrait = apTrait[nT];` |
|       3 |  9916 | `									break;` |
|       - |  9917 | `								}` |
|       2 |  9918 | `							}` |
|       3 |  9919 | `							if( pSrcTrait ){` |
|       3 |  9920 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  9921 | `							}` |
|       2 |  9922 | `						}else{` |
|      10 |  9923 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  9924 | `						}` |
|      13 |  9925 | `						if( pMeth ){` |
|      13 |  9926 | `							if( sAlias.nByte > 0 ){` |
|       - |  9927 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  9928 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  9929 | `								 */` |
|       - |  9930 | `								ph7_class_method *pAlias;` |
|       - |  9931 | `								char *zAliasDup;` |
|      11 |  9932 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      11 |  9933 | `								if( pAlias ){` |
|      11 |  9934 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      11 |  9935 | `									if( iNewVis >= 0 ){` |
|       5 |  9936 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9937 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9938 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  9939 | `									}` |
|      11 |  9940 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      11 |  9941 | `									if( zAliasDup ){` |
|      11 |  9942 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       4 |  9943 | `									}` |
|       7 |  9944 | `								}` |
|       7 |  9945 | `							}else if( iNewVis >= 0 ){` |
|       - |  9946 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  9947 | `								ph7_class_method *pCopy;` |
|       3 |  9948 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  9949 | `								if( pCopy ){` |
|       3 |  9950 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  9951 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  9952 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9953 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9954 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  9955 | `									/* Replace the method in the class hash */` |
|       3 |  9956 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  9957 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  9958 | `								}` |
|       1 |  9959 | `							}` |
|       5 |  9960 | `						}` |
|       5 |  9961 | `						SXUNUSED(hasQual);` |
|       5 |  9962 | `					}` |
|      21 |  9963 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9964 | `				}` |
|       - |  9965 | `			}` |
|      57 |  9966 | `			SySetRelease(&pUse->aTraits);` |
|      31 |  9967 | `		}` |
|       - |  9968 | `	}` |
|       - |  9969 | `	/* Install the class */` |
|  112317 |  9970 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|  112317 |  9971 | `	if( rc == SXRET_OK ){` |
|       - |  9972 | `		ph7_class **apInterface;` |
|       - |  9973 | `		sxu32 n;` |
|  112317 |  9974 | `		if( pBase ){` |
|       - |  9975 | `			/* Inherit from base class and mark as a subclass */` |
|   84409 |  9976 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   42202 |  9977 | `		}` |
|  112317 |  9978 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  123975 |  9979 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  9980 | `			/* Implements one or more interface */` |
|   11663 |  9981 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   11663 |  9982 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9983 | `				break;` |
|       - |  9984 | `			}` |
|    5834 |  9985 | `		}` |
|       - |  9986 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  9987 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  168468 |  9988 | `		if( rc == SXRET_OK` |
|  112312 |  9989 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|  112317 |  9990 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   91967 |  9991 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  9992 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   91967 |  9993 | `			if( pStringable ){` |
|   91967 |  9994 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   91967 |  9995 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  9996 | `				sxu32 i;` |
|   91967 |  9997 | `				int bAlready = 0;` |
|   99627 |  9998 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    7667 |  9999 | `					if( apImpl[i] == pStringable ){` |
|       3 | 10000 | `						bAlready = 1;` |
|       3 | 10001 | `						break;` |
|       - | 10002 | `					}` |
|    3835 | 10003 | `				}` |
|   91967 | 10004 | `				if( !bAlready ){` |
|   91965 | 10005 | `					PH7_ClassImplement(pClass,pStringable);` |
|   45980 | 10006 | `				}` |
|   45981 | 10007 | `			}` |
|   45981 | 10008 | `		}` |
|       - | 10009 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|  112317 | 10010 | `		if( rc == SXRET_OK ){` |
|  112317 | 10011 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|  112317 | 10012 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 10013 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 10014 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 10015 | `				return SXERR_ABORT;` |
|       - | 10016 | `			}` |
|   56156 | 10017 | `		}` |
|       - | 10018 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|  112317 | 10019 | `		if( rc == SXRET_OK ){` |
|  112317 | 10020 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|  112317 | 10021 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 10022 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 10023 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 10024 | `				return SXERR_ABORT;` |
|       - | 10025 | `			}` |
|   56156 | 10026 | `		}` |
|   56156 | 10027 | `	}` |
|  112317 | 10028 | `	SySetRelease(&aUseEntries);` |
|  112317 | 10029 | `	SySetRelease(&aInterfaces);` |
|  112317 | 10030 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10031 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10032 | `		return SXERR_ABORT;` |
|       - | 10033 | `	}` |
|   56156 | 10034 | `done:` |
|       - | 10035 | `	/* Point beyond the class body */` |
|  112355 | 10036 | `	pGen->pIn = &pEnd[1];` |
|  112355 | 10037 | `	pGen->pEnd = pTmp;` |
|  112355 | 10038 | `	return PH7_OK;` |
|   56181 | 10039 | `}` |
|       - | 10040 | `/* Compile a named class declaration (the common case). */` |
|  112326 | 10041 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 | 10042 | `{` |
|  112331 | 10043 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 | 10044 | `}` |
|       - | 10045 | `/*` |
|       - | 10046 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - | 10047 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - | 10048 | ` * compile + install the class body once (at compile time, like every other` |
|       - | 10049 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - | 10050 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - | 10051 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - | 10052 | ` */` |
|      26 | 10053 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 | 10054 | `{` |
|       - | 10055 | `	char zName[128];         /* Synthesized class name */` |
|       - | 10056 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - | 10057 | `	SyString sName;` |
|       - | 10058 | `	SyToken *pArgStart,*pArgEnd;` |
|       - | 10059 | `	ph7_value *pObj;` |
|      30 | 10060 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10061 | `	sxu32 nIdx,nLen;` |
|       - | 10062 | `	sxi32 nArg,rc;` |
|      13 | 10063 | `	SXUNUSED(iCompileFlag);` |
|       - | 10064 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|      30 | 10065 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      30 | 10066 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 10067 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 | 10068 | `	}` |
|      30 | 10069 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - | 10070 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - | 10071 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - | 10072 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|      30 | 10073 | `	pArgStart = pArgEnd = 0;` |
|      30 | 10074 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      30 | 10075 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10076 | `		return rc;` |
|       - | 10077 | `	}` |
|       - | 10078 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - | 10079 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      30 | 10080 | `	nArg = 0;` |
|      30 | 10081 | `	if( pArgStart < pArgEnd ){` |
|       7 | 10082 | `		SyToken *pSavedIn = pGen->pIn;` |
|       7 | 10083 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 10084 | `		SyToken *pArgNext;` |
|       7 | 10085 | `		pGen->pIn = pArgStart;` |
|       7 | 10086 | `		pGen->pEnd = pArgEnd;` |
|      13 | 10087 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       7 | 10088 | `			if( pGen->pIn < pArgNext ){` |
|       7 | 10089 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       7 | 10090 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10091 | `					pGen->pIn = pSavedIn;` |
|     ! 0 | 10092 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 | 10093 | `					return SXERR_ABORT;` |
|       - | 10094 | `				}` |
|       7 | 10095 | `				nArg++;` |
|       3 | 10096 | `			}` |
|       7 | 10097 | `			pGen->pIn = &pArgNext[1];` |
|       1 | 10098 | `		}` |
|       7 | 10099 | `		pGen->pIn = pSavedIn;` |
|       7 | 10100 | `		pGen->pEnd = pSavedEnd;` |
|       3 | 10101 | `	}` |
|       - | 10102 | `	/* Load the synthesized class name */` |
|      30 | 10103 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      30 | 10104 | `	if( pObj == 0 ){` |
|     ! 0 | 10105 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10106 | `		return SXERR_ABORT;` |
|       - | 10107 | `	}` |
|      30 | 10108 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      30 | 10109 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 10110 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      30 | 10111 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      30 | 10112 | `	return SXRET_OK;` |
|      17 | 10113 | `}` |
|       - | 10114 | `/*` |
|       - | 10115 | ` * Compile a user-defined abstract class.` |
|       - | 10116 | ` *  According to the PHP language reference manual` |
|       - | 10117 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 10118 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 10119 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 10120 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 10121 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 10122 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 10123 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 10124 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 10125 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 10126 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 10127 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 10128 | ` *   could differ.` |
|       - | 10129 | ` */` |
|       - | 10130 | `/*` |
|       - | 10131 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - | 10132 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - | 10133 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - | 10134 | ` */` |
| 1057230 | 10135 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 | 10136 | `{` |
| 1057235 | 10137 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  709563 | 10138 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  709563 | 10139 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  701885 | 10140 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  350909 | 10141 | `	}` |
| 1049495 | 10142 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
| 1049435 | 10143 | `	return FALSE;` |
|  528620 | 10144 | `}` |
|       - | 10145 | `/*` |
|       - | 10146 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - | 10147 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - | 10148 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - | 10149 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - | 10150 | ` */` |
| 1049430 | 10151 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 | 10152 | `{` |
| 1049435 | 10153 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
| 1049435 | 10154 | `	sxi32 iFlags = 0,iFlag;` |
| 1057235 | 10155 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    7805 | 10156 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 | 10157 | `			pDup = pIn;` |
|       2 | 10158 | `		}` |
|    7805 | 10159 | `		iFlags \|= iFlag;` |
|    7805 | 10160 | `		pIn++;` |
|       5 | 10161 | `	}` |
| 1049435 | 10162 | `	*ppIn = pIn;` |
| 1049435 | 10163 | `	if( ppDup ){ *ppDup = pDup; }` |
| 1049435 | 10164 | `	return iFlags;` |
|       5 | 10165 | `}` |
|       - | 10166 | `/*` |
|       - | 10167 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - | 10168 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - | 10169 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - | 10170 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - | 10171 | `` * `readonly`) to their existing handlers.`` |
|       - | 10172 | ` */` |
| 1045540 | 10173 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 | 10174 | `{` |
| 1045545 | 10175 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  526667 | 10176 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
| 1047487 | 10177 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 | 10178 | `}` |
|       - | 10179 | `/*` |
|       - | 10180 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - | 10181 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - | 10182 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - | 10183 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - | 10184 | `` * `abstract`+`final` pair, like PHP.`` |
|       - | 10185 | ` */` |
|    3890 | 10186 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 | 10187 | `{` |
|       - | 10188 | `	SyToken *pDup;` |
|    3895 | 10189 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - | 10190 | `	sxi32 rc;` |
|    3895 | 10191 | `	if( pDup ){` |
|       4 | 10192 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 | 10193 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 | 10194 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10195 | `			return SXERR_ABORT;` |
|       - | 10196 | `		}` |
|       1 | 10197 | `	}` |
|    5835 | 10198 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    1950 | 10199 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 | 10200 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10201 | `			"Cannot use the final modifier on an abstract class");` |
|       3 | 10202 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10203 | `			return SXERR_ABORT;` |
|       - | 10204 | `		}` |
|       1 | 10205 | `	}` |
|    3895 | 10206 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    1950 | 10207 | `}` |
|       - | 10208 | `/*` |
|       - | 10209 | ` * Compile a user-defined trait.` |
|       - | 10210 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 10211 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 10212 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 10213 | ` */` |
|      64 | 10214 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 | 10215 | `{` |
|      69 | 10216 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10217 | `	ph7_class *pClass;` |
|       - | 10218 | `	SyToken *pEnd,*pTmp;` |
|       - | 10219 | `	sxi32 iProtection;` |
|       - | 10220 | `	sxi32 iAttrflags;` |
|       - | 10221 | `	SyString *pName;` |
|       - | 10222 | `	sxi32 nKwrd;` |
|       - | 10223 | `	sxi32 rc;` |
|       - | 10224 | `	/* Jump the 'trait' keyword */` |
|      69 | 10225 | `	pGen->pIn++;` |
|      69 | 10226 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 10227 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 10228 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10229 | `			return SXERR_ABORT;` |
|       - | 10230 | `		}` |
|     ! 0 | 10231 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 10232 | `			pGen->pIn++;` |
|     ! 0 | 10233 | `		}` |
|     ! 0 | 10234 | `		return SXRET_OK;` |
|       - | 10235 | `	}` |
|       - | 10236 | `	/* Extract trait name */` |
|      69 | 10237 | `	pName = &pGen->pIn->sData;` |
|      69 | 10238 | `	pGen->pIn++;` |
|       - | 10239 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 10240 | `		SyBlob sFQN;` |
|       - | 10241 | `		SyString sFQNStr;` |
|      69 | 10242 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      69 | 10243 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      69 | 10244 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      69 | 10245 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      69 | 10246 | `		SyBlobRelease(&sFQN);` |
|       - | 10247 | `	}` |
|      69 | 10248 | `	if( pClass == 0 ){` |
|     ! 0 | 10249 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10250 | `		return SXERR_ABORT;` |
|       - | 10251 | `	}` |
|       - | 10252 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      69 | 10253 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 10254 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 10255 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 10256 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10257 | `			return SXERR_ABORT;` |
|       - | 10258 | `		}` |
|     ! 0 | 10259 | `		return SXRET_OK;` |
|       - | 10260 | `	}` |
|      69 | 10261 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      69 | 10262 | `	pEnd = 0;` |
|      69 | 10263 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      69 | 10264 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 10265 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 10266 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 10267 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10268 | `			return SXERR_ABORT;` |
|       - | 10269 | `		}` |
|     ! 0 | 10270 | `		return SXRET_OK;` |
|       - | 10271 | `	}` |
|       - | 10272 | `	/* Swap token stream */` |
|      69 | 10273 | `	pTmp = pGen->pEnd;` |
|      69 | 10274 | `	pGen->pEnd = pEnd;` |
|       - | 10275 | `	/* Mark as trait */` |
|      69 | 10276 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 10277 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      64 | 10278 | `	for(;;){` |
|     177 | 10279 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 | 10280 | `			pGen->pIn++;` |
|       4 | 10281 | `		}` |
|     153 | 10282 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      69 | 10283 | `			break;` |
|       - | 10284 | `		}` |
|      89 | 10285 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 10286 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10287 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 10288 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 10289 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10290 | `				return SXERR_ABORT;` |
|       - | 10291 | `			}` |
|     ! 0 | 10292 | `			goto done;` |
|       - | 10293 | `		}` |
|      89 | 10294 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      89 | 10295 | `		iAttrflags = 0;` |
|      89 | 10296 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      89 | 10297 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      89 | 10298 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 10299 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 10300 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 10301 | `				for(;;){` |
|       - | 10302 | `					ph7_class *pUsedTrait;` |
|       - | 10303 | `					SyString *pUsedName;` |
|       5 | 10304 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 10305 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 10306 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 10307 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10308 | `							return SXERR_ABORT;` |
|       - | 10309 | `						}` |
|     ! 0 | 10310 | `						break;` |
|       - | 10311 | `					}` |
|       5 | 10312 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 10313 | `					{` |
|       - | 10314 | `						SyBlob sResolved;` |
|       5 | 10315 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 10316 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 10317 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 10318 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 10319 | `						SyBlobRelease(&sResolved);` |
|       - | 10320 | `					}` |
|       5 | 10321 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 10322 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 10323 | `					}` |
|       5 | 10324 | `					if( pUsedTrait == 0 ){` |
|       4 | 10325 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 10326 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 10327 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10328 | `							return SXERR_ABORT;` |
|       - | 10329 | `						}` |
|       2 | 10330 | `					}else{` |
|       3 | 10331 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 10332 | `					}` |
|       5 | 10333 | `					pGen->pIn++;` |
|       5 | 10334 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 10335 | `						break;` |
|       - | 10336 | `					}` |
|     ! 0 | 10337 | `					pGen->pIn++;` |
|     ! 0 | 10338 | `				}` |
|       5 | 10339 | `				continue;` |
|       - | 10340 | `			}` |
|      85 | 10341 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 | 10342 | `				iProtection = nKwrd;` |
|      73 | 10343 | `				pGen->pIn++;` |
|      68 | 10344 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 | 10345 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 10346 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10347 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 10348 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 10349 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10350 | `						return SXERR_ABORT;` |
|       - | 10351 | `					}` |
|     ! 0 | 10352 | `					goto done;` |
|       - | 10353 | `				}` |
|      73 | 10354 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 | 10355 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 | 10356 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 10357 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10358 | `							return SXERR_ABORT;` |
|       - | 10359 | `						}` |
|     ! 0 | 10360 | `						goto done;` |
|       - | 10361 | `					}` |
|      12 | 10362 | `					continue;` |
|       - | 10363 | `				}` |
|      63 | 10364 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 | 10365 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 10366 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 10367 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10368 | `							return SXERR_ABORT;` |
|       - | 10369 | `						}` |
|     ! 0 | 10370 | `						goto done;` |
|       - | 10371 | `					}` |
|       5 | 10372 | `					continue;` |
|       - | 10373 | `				}` |
|      58 | 10374 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 | 10375 | `			}` |
|      71 | 10376 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 10377 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10378 | `					"Traits cannot have constants");` |
|     ! 0 | 10379 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10380 | `					return SXERR_ABORT;` |
|       - | 10381 | `				}` |
|     ! 0 | 10382 | `				goto done;` |
|     ! 0 | 10383 | `			}else{` |
|      71 | 10384 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 10385 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 10386 | `					pGen->pIn++;` |
|       5 | 10387 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 10388 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 10389 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 10390 | `							iProtection = nKwrd;` |
|     ! 0 | 10391 | `							pGen->pIn++;` |
|     ! 0 | 10392 | `						}` |
|       1 | 10393 | `					}` |
|       4 | 10394 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 | 10395 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 10396 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10397 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 10398 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 10399 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10400 | `							return SXERR_ABORT;` |
|       - | 10401 | `						}` |
|     ! 0 | 10402 | `						goto done;` |
|       - | 10403 | `					}` |
|       5 | 10404 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 10405 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 10406 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 10407 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 10408 | `								return SXERR_ABORT;` |
|       - | 10409 | `							}` |
|     ! 0 | 10410 | `							goto done;` |
|       - | 10411 | `						}` |
|       3 | 10412 | `						continue;` |
|       - | 10413 | `					}` |
|       3 | 10414 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 | 10415 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10416 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 10417 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 10418 | `								return SXERR_ABORT;` |
|       - | 10419 | `							}` |
|     ! 0 | 10420 | `							goto done;` |
|       - | 10421 | `						}` |
|     ! 0 | 10422 | `						continue;` |
|       - | 10423 | `					}` |
|       3 | 10424 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      68 | 10425 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 | 10426 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 | 10427 | `					pGen->pIn++;` |
|       6 | 10428 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 | 10429 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 | 10430 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 | 10431 | `							iProtection = nKwrd;` |
|       6 | 10432 | `							pGen->pIn++;` |
|       2 | 10433 | `						}` |
|       2 | 10434 | `					}` |
|       6 | 10435 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 10436 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 10437 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10438 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 10439 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 10440 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10441 | `							return SXERR_ABORT;` |
|       - | 10442 | `						}` |
|     ! 0 | 10443 | `						goto done;` |
|       - | 10444 | `					}` |
|       6 | 10445 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 10446 | `				}` |
|      69 | 10447 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 10448 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10449 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 10450 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 10451 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10452 | `						return SXERR_ABORT;` |
|       - | 10453 | `					}` |
|     ! 0 | 10454 | `					goto done;` |
|       - | 10455 | `				}` |
|      69 | 10456 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 10457 | `					pGen->pIn++;` |
|     ! 0 | 10458 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 10459 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10460 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 10461 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10462 | `							return SXERR_ABORT;` |
|       - | 10463 | `						}` |
|     ! 0 | 10464 | `						goto done;` |
|       - | 10465 | `					}` |
|     ! 0 | 10466 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10467 | `				}else{` |
|      69 | 10468 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 10469 | `				}` |
|      69 | 10470 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10471 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10472 | `						return SXERR_ABORT;` |
|       - | 10473 | `					}` |
|     ! 0 | 10474 | `					goto done;` |
|       - | 10475 | `				}` |
|       - | 10476 | `			}` |
|      37 | 10477 | `		}else{` |
|     ! 0 | 10478 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10479 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10480 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10481 | `					return SXERR_ABORT;` |
|       - | 10482 | `				}` |
|     ! 0 | 10483 | `				goto done;` |
|       - | 10484 | `			}` |
|       - | 10485 | `		}` |
|       5 | 10486 | `	}` |
|       - | 10487 | `	/* Install the trait */` |
|      69 | 10488 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      69 | 10489 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10490 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10491 | `		return SXERR_ABORT;` |
|       - | 10492 | `	}` |
|      32 | 10493 | `done:` |
|       - | 10494 | `	/* Point beyond the trait body */` |
|      69 | 10495 | `	pGen->pIn = &pEnd[1];` |
|      69 | 10496 | `	pGen->pEnd = pTmp;` |
|      69 | 10497 | `	return PH7_OK;` |
|      37 | 10498 | `}` |
|       - | 10499 | `/*` |
|       - | 10500 | ` * Compile a user-defined class.` |
|       - | 10501 | ` *  According to the PHP language reference manual` |
|       - | 10502 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 10503 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 10504 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 10505 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 10506 | ` *   and functions (called "methods").` |
|       - | 10507 | ` */` |
|  108436 | 10508 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 | 10509 | `{` |
|       - | 10510 | `	sxi32 rc;` |
|  108441 | 10511 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|  108441 | 10512 | `	return rc;` |
|       5 | 10513 | `}` |
|       - | 10514 | `/*` |
|       - | 10515 | ` * Exception handling.` |
|       - | 10516 | ` *  According to the PHP language reference manual` |
|       - | 10517 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 10518 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 10519 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 10520 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 10521 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 10522 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 10523 | ` *    (or re-thrown) within a catch block.` |
|       - | 10524 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 10525 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 10526 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 10527 | ` *    been defined with set_exception_handler().` |
|       - | 10528 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 10529 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 10530 | ` */` |
|       - | 10531 | `/*` |
|       - | 10532 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 10533 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 10534 | ` * indicates failure.` |
|       - | 10535 | ` */` |
|   15686 | 10536 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 10537 | `{` |
|   15691 | 10538 | `	sxi32 rc = SXRET_OK;` |
|   15691 | 10539 | `	if( pRoot->pOp ){` |
|   15679 | 10540 | `		switch( pRoot->pOp->iOp ){` |
|    7837 | 10541 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - | 10542 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - | 10543 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - | 10544 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - | 10545 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - | 10546 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   15679 | 10547 | `			break;` |
|     ! 0 | 10548 | `		default:` |
|       - | 10549 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - | 10550 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - | 10551 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 | 10552 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 10553 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 10554 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 10555 | `				rc = SXERR_INVALID;` |
|     ! 0 | 10556 | `			}` |
|     ! 0 | 10557 | `			break;` |
|       - | 10558 | `		}` |
|    7854 | 10559 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 10560 | `		/* Unexpected expression */` |
|     ! 0 | 10561 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 10562 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10563 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 10564 | `			rc = SXERR_INVALID;` |
|     ! 0 | 10565 | `		}` |
|     ! 0 | 10566 | `	}` |
|   15691 | 10567 | `	return rc;` |
|       5 | 10568 | `}` |
|       - | 10569 | `/*` |
|       - | 10570 | ` * Compile a 'throw' statement.` |
|       - | 10571 | ` * throw: This is how you trigger an exception.` |
|       - | 10572 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 10573 | ` */` |
|   15650 | 10574 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 | 10575 | `{` |
|   15655 | 10576 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10577 | `	GenBlock *pBlock;` |
|       - | 10578 | `	sxu32 nIdx;` |
|       - | 10579 | `	sxi32 rc;` |
|   15655 | 10580 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 10581 | `	/* Compile the expression */` |
|   15655 | 10582 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   15655 | 10583 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10584 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 10585 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10586 | `			return SXERR_ABORT;` |
|       - | 10587 | `		}` |
|     ! 0 | 10588 | `		return SXRET_OK;` |
|       - | 10589 | `	}` |
|   15655 | 10590 | `	pBlock = pGen->pCurrent;` |
|       - | 10591 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   61945 | 10592 | `	while(pBlock->pParent){` |
|   61941 | 10593 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   15651 | 10594 | `			break;` |
|       - | 10595 | `		}` |
|       - | 10596 | `		/* Point to the parent block */` |
|   46295 | 10597 | `		pBlock = pBlock->pParent;` |
|       5 | 10598 | `	}` |
|       - | 10599 | `	/* Emit the throw instruction */` |
|   15655 | 10600 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 10601 | `	/* Emit the jump */` |
|   15655 | 10602 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   15655 | 10603 | `	return SXRET_OK;` |
|    7830 | 10604 | `}` |
|       - | 10605 | `/*` |
|       - | 10606 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - | 10607 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - | 10608 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - | 10609 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - | 10610 | ` * the validator guarantees the operand is a valid exception target.` |
|       - | 10611 | ` */` |
|      36 | 10612 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 10613 | `{` |
|      38 | 10614 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10615 | `	GenBlock *pBlock;` |
|       - | 10616 | `	sxu32 nIdx;` |
|       - | 10617 | `	sxi32 rc;` |
|      18 | 10618 | `	(void)iCompileFlag;` |
|      38 | 10619 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 | 10620 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 10621 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10622 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10623 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10624 | `			return SXERR_ABORT;` |
|       - | 10625 | `		}` |
|     ! 0 | 10626 | `		return SXRET_OK;` |
|       - | 10627 | `	}` |
|      38 | 10628 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 | 10629 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10630 | `		return SXERR_ABORT;` |
|       - | 10631 | `	}` |
|      38 | 10632 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10633 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10634 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10635 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10636 | `			return SXERR_ABORT;` |
|       - | 10637 | `		}` |
|     ! 0 | 10638 | `		return SXRET_OK;` |
|       - | 10639 | `	}` |
|       - | 10640 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 | 10641 | `	pBlock = pGen->pCurrent;` |
|      60 | 10642 | `	while( pBlock->pParent ){` |
|      49 | 10643 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 | 10644 | `			break;` |
|       - | 10645 | `		}` |
|      23 | 10646 | `		pBlock = pBlock->pParent;` |
|       1 | 10647 | `	}` |
|      38 | 10648 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 | 10649 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 | 10650 | `	return SXRET_OK;` |
|      20 | 10651 | `}` |
|       - | 10652 | `/*` |
|       - | 10653 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|       - | 10654 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|       - | 10655 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|       - | 10656 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|       - | 10657 | ` * compile error propagated from the parser.` |
|       - | 10658 | ` */` |
|      46 | 10659 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|       4 | 10660 | `{` |
|       - | 10661 | `	SyString sClassName;` |
|       - | 10662 | `	SyToken *pToken;` |
|       - | 10663 | `	SyString *pName;` |
|       - | 10664 | `	char *zDup;` |
|       - | 10665 | `	sxi32 rc;` |
|      50 | 10666 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|      50 | 10667 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|      50 | 10668 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|      50 | 10669 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      50 | 10670 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 10671 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10672 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10673 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10674 | `		return SXERR_INVALID;` |
|       - | 10675 | `	}` |
|      50 | 10676 | `	pGen->pIn++; /* '(' */` |
|      23 | 10677 | `	for(;;){` |
|       - | 10678 | `		SyBlob sResolved;` |
|      50 | 10679 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      50 | 10680 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 10681 | `			SyBlobRelease(&sResolved);` |
|     ! 0 | 10682 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10683 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10684 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10685 | `			return SXERR_INVALID;` |
|       - | 10686 | `		}` |
|      73 | 10687 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      46 | 10688 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|      50 | 10689 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|      50 | 10690 | `		SyBlobRelease(&sResolved);` |
|      50 | 10691 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|      50 | 10692 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|      50 | 10693 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      46 | 10694 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|       4 | 10695 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     ! 0 | 10696 | `			pGen->pIn++; continue;` |
|       - | 10697 | `		}` |
|      50 | 10698 | `		break;` |
|     ! 0 | 10699 | `	}` |
|      69 | 10700 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|      50 | 10701 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 10702 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10703 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10704 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10705 | `		return SXERR_INVALID;` |
|       - | 10706 | `	}` |
|      50 | 10707 | `	pGen->pIn++; /* '$' */` |
|      50 | 10708 | `	pName = &pGen->pIn->sData;` |
|      50 | 10709 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      50 | 10710 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|      50 | 10711 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|      50 | 10712 | `	pGen->pIn++;` |
|      50 | 10713 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 10714 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10715 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10716 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10717 | `		return SXERR_INVALID;` |
|       - | 10718 | `	}` |
|      50 | 10719 | `	pGen->pIn++; /* ')' */` |
|      50 | 10720 | `	return SXRET_OK;` |
|      27 | 10721 | `}` |
|       - | 10722 | `/*` |
|       - | 10723 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|       - | 10724 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|       - | 10725 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|       - | 10726 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|       - | 10727 | ` * VmThrowException):` |
|       - | 10728 | ` *` |
|       - | 10729 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|       - | 10730 | ` *    <try body>` |
|       - | 10731 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|       - | 10732 | ` *    JMP  -> finally\|end` |
|       - | 10733 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|       - | 10734 | ` *    <catch body>` |
|       - | 10735 | ` *    JMP  -> finally\|end` |
|       - | 10736 | ` *    ... more catches ...` |
|       - | 10737 | ` *  Lfin: <finally body>` |
|       - | 10738 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|       - | 10739 | ` *  Lend:` |
|       - | 10740 | ` */` |
|      90 | 10741 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|       4 | 10742 | `{` |
|      94 | 10743 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10744 | `	GenBlock *pTry;` |
|       - | 10745 | `	VmInstr *pInstr;` |
|      94 | 10746 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|       - | 10747 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|       - | 10748 | `	sxi32 rc;` |
|      94 | 10749 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|       - | 10750 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      94 | 10751 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      94 | 10752 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      94 | 10753 | `	pTry->pUserData = pException;` |
|      94 | 10754 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      94 | 10755 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      94 | 10756 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      94 | 10757 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      94 | 10758 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      94 | 10759 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10760 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      94 | 10761 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      94 | 10762 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      94 | 10763 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      94 | 10764 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10765 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      94 | 10766 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|       - | 10767 | `	/* Catch clauses (inline) */` |
|      94 | 10768 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      90 | 10769 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|      50 | 10770 | `		sxu32 k = 0;` |
|      69 | 10771 | `		for(;;){` |
|       - | 10772 | `			ph7_exception_block sCatch;` |
|       - | 10773 | `			GenBlock *pCatchBlk;` |
|      96 | 10774 | `			sxu32 idxJmp = 0;` |
|      92 | 10775 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      88 | 10776 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      27 | 10777 | `				break;` |
|       - | 10778 | `			}` |
|      50 | 10779 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|      50 | 10780 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      50 | 10781 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|      50 | 10782 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|      50 | 10783 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|      50 | 10784 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|      50 | 10785 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       - | 10786 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|       - | 10787 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|       - | 10788 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|      50 | 10789 | `			pCatchBlk->pUserData = pException;` |
|      50 | 10790 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      50 | 10791 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      50 | 10792 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      50 | 10793 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10794 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|       - | 10795 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|      50 | 10796 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      50 | 10797 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|      50 | 10798 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|      50 | 10799 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      50 | 10800 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      50 | 10801 | `			k++;` |
|       4 | 10802 | `		}` |
|      23 | 10803 | `	}` |
|       - | 10804 | `	/* Finally (inline) */` |
|      94 | 10805 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      74 | 10806 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10807 | `		GenBlock *pFinBlk;` |
|      52 | 10808 | `		pGen->pIn++; /* Jump 'finally' */` |
|      52 | 10809 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|      52 | 10810 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|      52 | 10811 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      52 | 10812 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      52 | 10813 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      52 | 10814 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      52 | 10815 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      52 | 10816 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|      52 | 10817 | `		pException->iHasFinally = 1;` |
|      24 | 10818 | `	}` |
|      94 | 10819 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      94 | 10820 | `	pException->iInlined = 1;` |
|       - | 10821 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|       - | 10822 | `	{` |
|      94 | 10823 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|       - | 10824 | `		sxu32 *aJ; sxu32 n;` |
|      94 | 10825 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      94 | 10826 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      94 | 10827 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|     140 | 10828 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|      50 | 10829 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|      50 | 10830 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      27 | 10831 | `		}` |
|       - | 10832 | `	}` |
|      94 | 10833 | `	SySetRelease(&aCatchJmp);` |
|      94 | 10834 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|     ! 0 | 10835 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|     ! 0 | 10836 | `	}` |
|      94 | 10837 | `	return SXRET_OK;` |
|      49 | 10838 | `}` |
|       - | 10839 | `/*` |
|       - | 10840 | ` * Compile a 'catch' block.` |
|       - | 10841 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 10842 | ` * an object containing the exception information.` |
|       - | 10843 | ` */` |
|     988 | 10844 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 10845 | `{` |
|     993 | 10846 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10847 | `	ph7_exception_block sCatch;` |
|       - | 10848 | `	SySet *pInstrContainer;` |
|       - | 10849 | `	SyString sClassName;` |
|       - | 10850 | `	GenBlock *pCatch;` |
|       - | 10851 | `	SyToken *pToken;` |
|       - | 10852 | `	SyString *pName;` |
|       - | 10853 | `	char *zDup;` |
|       - | 10854 | `	sxi32 rc;` |
|     993 | 10855 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 10856 | `	/* Zero the structure */` |
|     993 | 10857 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 10858 | `	/* Initialize fields */` |
|     993 | 10859 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     993 | 10860 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     993 | 10861 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 10862 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10863 | `			pToken = pGen->pIn;` |
|     ! 0 | 10864 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10865 | `				pToken--;` |
|     ! 0 | 10866 | `			}` |
|     ! 0 | 10867 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10868 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10869 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10870 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10871 | `				return SXERR_ABORT;` |
|       - | 10872 | `			}` |
|     ! 0 | 10873 | `			return SXERR_INVALID;` |
|       - | 10874 | `	}` |
|       - | 10875 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     993 | 10876 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     508 | 10877 | `	for(;;){` |
|       - | 10878 | `		SyBlob sResolved;` |
|    1021 | 10879 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    1021 | 10880 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 10881 | `			SyBlobRelease(&sResolved);` |
|       6 | 10882 | `			pToken = pGen->pIn;` |
|       6 | 10883 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10884 | `				pToken--;` |
|     ! 0 | 10885 | `			}` |
|       8 | 10886 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10887 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 10888 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 10889 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10890 | `				return SXERR_ABORT;` |
|       - | 10891 | `			}` |
|       6 | 10892 | `			return SXERR_INVALID;` |
|       - | 10893 | `		}` |
|       - | 10894 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 10895 | `		 * transient SyBlob allocation. */` |
|    1523 | 10896 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    1012 | 10897 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    1017 | 10898 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    1017 | 10899 | `		SyBlobRelease(&sResolved);` |
|    1017 | 10900 | `		if( zDup == 0 ){` |
|     ! 0 | 10901 | `			goto Mem;` |
|       - | 10902 | `		}` |
|    1017 | 10903 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    1017 | 10904 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10905 | `			goto Mem;` |
|       - | 10906 | `		}` |
|       - | 10907 | `		/* Check for '\|' (multi-catch separator) */` |
|    1026 | 10908 | `		if( pGen->pIn < pGen->pEnd &&` |
|    1012 | 10909 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      33 | 10910 | `			pGen->pIn->sData.nByte == 1 &&` |
|      28 | 10911 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      30 | 10912 | `			pGen->pIn++; /* Consume the '\|' */` |
|      30 | 10913 | `			continue;` |
|       - | 10914 | `		}` |
|     989 | 10915 | `		break;` |
|     ! 0 | 10916 | `	}` |
|    1476 | 10917 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     989 | 10918 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 10919 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10920 | `			pToken = pGen->pIn;` |
|     ! 0 | 10921 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10922 | `				pToken--;` |
|     ! 0 | 10923 | `			}` |
|     ! 0 | 10924 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10925 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10926 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10927 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10928 | `				return SXERR_ABORT;` |
|       - | 10929 | `			}` |
|     ! 0 | 10930 | `			return SXERR_INVALID;` |
|       - | 10931 | `	}` |
|     989 | 10932 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 10933 | `	/* Duplicate instance name */` |
|     989 | 10934 | `	pName = &pGen->pIn->sData;` |
|     989 | 10935 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     989 | 10936 | `	if( zDup == 0 ){` |
|     ! 0 | 10937 | `		goto Mem;` |
|       - | 10938 | `	}` |
|     989 | 10939 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     989 | 10940 | `	pGen->pIn++;` |
|     989 | 10941 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 10942 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 10943 | `		pToken = pGen->pIn;` |
|     ! 0 | 10944 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10945 | `			pToken--;` |
|     ! 0 | 10946 | `		}` |
|     ! 0 | 10947 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10948 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10949 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10950 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10951 | `			return SXERR_ABORT;` |
|       - | 10952 | `		}` |
|     ! 0 | 10953 | `		return SXERR_INVALID;` |
|       - | 10954 | `	}` |
|       - | 10955 | `	/* Compile the block */` |
|     989 | 10956 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 10957 | `	/* Create the catch block */` |
|     989 | 10958 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     989 | 10959 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10960 | `		return SXERR_ABORT;` |
|       - | 10961 | `	}` |
|       - | 10962 | `	/* Swap bytecode container */` |
|     989 | 10963 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     989 | 10964 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 10965 | `	/* Compile the block */` |
|     989 | 10966 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 10967 | `	/* Fix forward jumps now the destination is resolved  */` |
|     989 | 10968 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10969 | `	/* Emit the DONE instruction */` |
|     989 | 10970 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10971 | `	/* Leave the block */` |
|     989 | 10972 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10973 | `	/* Restore the default container */` |
|     989 | 10974 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10975 | `	/* Install the catch block */` |
|     989 | 10976 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     989 | 10977 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10978 | `		goto Mem;` |
|       - | 10979 | `	}` |
|     989 | 10980 | `	return SXRET_OK;` |
|     ! 0 | 10981 | `Mem:` |
|     ! 0 | 10982 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10983 | `	return SXERR_ABORT;` |
|     499 | 10984 | `}` |
|       - | 10985 | `/*` |
|       - | 10986 | ` * Compile a 'try' block.` |
|       - | 10987 | ` * A function using an exception should be in a "try" block.` |
|       - | 10988 | ` * If the exception does not trigger, the code will continue` |
|       - | 10989 | ` * as normal. However if the exception triggers, an exception` |
|       - | 10990 | ` * is "thrown".` |
|       - | 10991 | ` */` |
|    1136 | 10992 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 10993 | `{` |
|       - | 10994 | `	ph7_exception *pException;` |
|    1141 | 10995 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10996 | `	GenBlock *pTry;` |
|       - | 10997 | `	sxu32 nJmpIdx;` |
|       - | 10998 | `	sxi32 rc;` |
|       - | 10999 | `	/* Create the exception container */` |
|    1141 | 11000 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    1141 | 11001 | `	if( pException == 0 ){` |
|     ! 0 | 11002 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 11003 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 11004 | `		return SXERR_ABORT;` |
|       - | 11005 | `	}` |
|       - | 11006 | `	/* Zero the structure */` |
|    1141 | 11007 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 11008 | `	/* Initialize fields */` |
|    1141 | 11009 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    1141 | 11010 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    1141 | 11011 | `	pException->iHasFinally = 0;` |
|    1141 | 11012 | `	pException->iFinallyDone = 0;` |
|    1141 | 11013 | `	pException->pVm = pGen->pVm;` |
|       - | 11014 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|       - | 11015 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|       - | 11016 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|       - | 11017 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|       - | 11018 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|       - | 11019 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|    1141 | 11020 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      94 | 11021 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|       - | 11022 | `	}` |
|       - | 11023 | `	/* Create the try block */` |
|    1051 | 11024 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|    1051 | 11025 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 11026 | `		return SXERR_ABORT;` |
|       - | 11027 | `	}` |
|       - | 11028 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|    1051 | 11029 | `	pTry->pUserData = pException;` |
|       - | 11030 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    1051 | 11031 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 11032 | `	/* Fix the jump later when the destination is resolved */` |
|    1051 | 11033 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    1051 | 11034 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 11035 | `	/* Compile the block */` |
|    1051 | 11036 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    1051 | 11037 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 11038 | `		return SXERR_ABORT;` |
|       - | 11039 | `	}` |
|       - | 11040 | `	/* Fix forward jumps now the destination is resolved */` |
|    1051 | 11041 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11042 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    1051 | 11043 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 11044 | `	/* Leave the block */` |
|    1051 | 11045 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 11046 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    1051 | 11047 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    1044 | 11048 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 11049 | `		/* Compile one or more catch blocks */` |
|     984 | 11050 | `		for(;;){` |
|    1968 | 11051 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    1414 | 11052 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     495 | 11053 | `					break;` |
|       - | 11054 | `			}` |
|     993 | 11055 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     993 | 11056 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11057 | `				return SXERR_ABORT;` |
|       - | 11058 | `			}` |
|       5 | 11059 | `		}` |
|     490 | 11060 | `	}` |
|       - | 11061 | `	/* Compile optional finally block */` |
|    1051 | 11062 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     422 | 11063 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 11064 | `		SySet *pInstrContainer;` |
|       - | 11065 | `		GenBlock *pFinBlock;` |
|     129 | 11066 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 11067 | `		/* Create the finally block for jump fixup bookkeeping */` |
|     129 | 11068 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     129 | 11069 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11070 | `			return SXERR_ABORT;` |
|       - | 11071 | `		}` |
|       - | 11072 | `		/* Swap bytecode container */` |
|     129 | 11073 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     129 | 11074 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 11075 | `		/* Compile the finally body */` |
|     129 | 11076 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     129 | 11077 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11078 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 11079 | `			return SXERR_ABORT;` |
|       - | 11080 | `		}` |
|       - | 11081 | `		/* Fix forward jumps now the destination is resolved */` |
|     129 | 11082 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11083 | `		/* Emit DONE to terminate the finally block */` |
|     129 | 11084 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 11085 | `		/* Leave the block */` |
|     129 | 11086 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 11087 | `		/* Restore the default container */` |
|     129 | 11088 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     129 | 11089 | `		pException->iHasFinally = 1;` |
|      62 | 11090 | `	}` |
|       - | 11091 | `	/* Must have at least one catch or finally */` |
|    1051 | 11092 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       8 | 11093 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 11094 | `			"Cannot use try without catch or finally");` |
|       8 | 11095 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11096 | `			return SXERR_ABORT;` |
|       - | 11097 | `		}` |
|       3 | 11098 | `	}` |
|    1051 | 11099 | `	return SXRET_OK;` |
|     573 | 11100 | `}` |
|       - | 11101 | `/*` |
|       - | 11102 | ` * Compile a switch block.` |
|       - | 11103 | ` *  (See block-comment below for more information)` |
|       - | 11104 | ` */` |
|     112 | 11105 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 11106 | `{` |
|     117 | 11107 | `	sxi32 rc = SXRET_OK;` |
|     117 | 11108 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 11109 | `		/* Unexpected token */` |
|     ! 0 | 11110 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 11111 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11112 | `			return SXERR_ABORT;` |
|       - | 11113 | `		}` |
|     ! 0 | 11114 | `		pGen->pIn++;` |
|     ! 0 | 11115 | `	}` |
|     117 | 11116 | `	pGen->pIn++;` |
|       - | 11117 | `	/* First instruction to execute in this block. */` |
|     117 | 11118 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 11119 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 11120 | `	 * or the '}' token */` |
|     206 | 11121 | `	for(;;){` |
|     417 | 11122 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11123 | `			/* No more input to process */` |
|     ! 0 | 11124 | `			break;` |
|       - | 11125 | `		}` |
|     417 | 11126 | `		rc = SXRET_OK;` |
|     417 | 11127 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 | 11128 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 | 11129 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 11130 | `					/* Unexpected token */` |
|     ! 0 | 11131 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 11132 | `						&pGen->pIn->sData);` |
|     ! 0 | 11133 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11134 | `						return SXERR_ABORT;` |
|       - | 11135 | `					}` |
|       - | 11136 | `					/* FALL THROUGH */` |
|     ! 0 | 11137 | `				}` |
|      31 | 11138 | `				rc = SXERR_EOF;` |
|      31 | 11139 | `				break;` |
|       - | 11140 | `			}` |
|      32 | 11141 | `		}else{` |
|       - | 11142 | `			sxi32 nKwrd;` |
|       - | 11143 | `			/* Extract the keyword */` |
|     337 | 11144 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 | 11145 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 | 11146 | `				break;` |
|       - | 11147 | `			}` |
|     253 | 11148 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 11149 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 11150 | `					/* Unexpected token */` |
|     ! 0 | 11151 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 11152 | `						&pGen->pIn->sData);` |
|     ! 0 | 11153 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11154 | `						return SXERR_ABORT;` |
|       - | 11155 | `					}` |
|       - | 11156 | `					/* FALL THROUGH */` |
|     ! 0 | 11157 | `				}` |
|       - | 11158 | `				/* Block compiled */` |
|       3 | 11159 | `				break;` |
|       - | 11160 | `			}` |
|       - | 11161 | `		}` |
|       - | 11162 | `		/* Compile block */` |
|     305 | 11163 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 | 11164 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11165 | `			return SXERR_ABORT;` |
|       - | 11166 | `		}` |
|       5 | 11167 | `	}` |
|     117 | 11168 | `	return rc;` |
|      61 | 11169 | `}` |
|       - | 11170 | `/*` |
|       - | 11171 | ` * Compile a case eXpression.` |
|       - | 11172 | ` *  (See block-comment below for more information)` |
|       - | 11173 | ` */` |
|      92 | 11174 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 11175 | `{` |
|       - | 11176 | `	SySet *pInstrContainer;` |
|       - | 11177 | `	SyToken *pEnd,*pTmp;` |
|      97 | 11178 | `	sxi32 iNest = 0;` |
|       - | 11179 | `	sxi32 rc;` |
|       - | 11180 | `	/* Delimit the expression */` |
|      97 | 11181 | `	pEnd = pGen->pIn;` |
|     197 | 11182 | `	while( pEnd < pGen->pEnd ){` |
|     197 | 11183 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 11184 | `			/* Increment nesting level */` |
|       3 | 11185 | `			iNest++;` |
|     196 | 11186 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 11187 | `			/* Decrement nesting level */` |
|       3 | 11188 | `			iNest--;` |
|     194 | 11189 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 | 11190 | `			break;` |
|       - | 11191 | `		}` |
|     105 | 11192 | `		pEnd++;` |
|       5 | 11193 | `	}` |
|      97 | 11194 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 11195 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 11196 | `		if( rc == SXERR_ABORT ){` |
|       - | 11197 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 11198 | `			return SXERR_ABORT;` |
|       - | 11199 | `		}` |
|     ! 0 | 11200 | `	}` |
|       - | 11201 | `	/* Swap token stream */` |
|      97 | 11202 | `	pTmp = pGen->pEnd;` |
|      97 | 11203 | `	pGen->pEnd = pEnd;` |
|      97 | 11204 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 | 11205 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 | 11206 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 11207 | `	/* Emit the done instruction */` |
|      97 | 11208 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 | 11209 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 11210 | `	/* Update token stream */` |
|      97 | 11211 | `	pGen->pIn  = pEnd;` |
|      97 | 11212 | `	pGen->pEnd = pTmp;` |
|      97 | 11213 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 11214 | `		return SXERR_ABORT;` |
|       - | 11215 | `	}` |
|      97 | 11216 | `	return SXRET_OK;` |
|      51 | 11217 | `}` |
|       - | 11218 | `/*` |
|       - | 11219 | ` * Compile the smart switch statement.` |
|       - | 11220 | ` * According to the PHP language reference manual` |
|       - | 11221 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 11222 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 11223 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 11224 | ` *  This is exactly what the switch statement is for.` |
|       - | 11225 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 11226 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 11227 | ` *  of the outer loop, use continue 2.` |
|       - | 11228 | ` *  Note that switch/case does loose comparision.` |
|       - | 11229 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 11230 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 11231 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 11232 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 11233 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 11234 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 11235 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 11236 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 11237 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 11238 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 11239 | ` *  list for the next case.` |
|       - | 11240 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 11241 | ` *  or floating-point numbers and strings.` |
|       - | 11242 | ` */` |
|      28 | 11243 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 11244 | `{` |
|       - | 11245 | `	GenBlock *pSwitchBlock;` |
|       - | 11246 | `	SyToken *pTmp,*pEnd;` |
|       - | 11247 | `	ph7_switch *pSwitch;` |
|       - | 11248 | `	sxu32 nToken;` |
|       - | 11249 | `	sxu32 nLine;` |
|       - | 11250 | `	sxi32 rc;` |
|      33 | 11251 | `	nLine = pGen->pIn->nLine;` |
|       - | 11252 | `	/* Jump the 'switch' keyword */` |
|      33 | 11253 | `	pGen->pIn++;` |
|      33 | 11254 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 11255 | `		/* Syntax error */` |
|     ! 0 | 11256 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 11257 | `		if( rc == SXERR_ABORT ){` |
|       - | 11258 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 11259 | `			return SXERR_ABORT;` |
|       - | 11260 | `		}` |
|     ! 0 | 11261 | `		goto Synchronize;` |
|       - | 11262 | `	}` |
|       - | 11263 | `	/* Jump the left parenthesis '(' */` |
|      33 | 11264 | `	pGen->pIn++;` |
|      33 | 11265 | `	pEnd = 0; /* cc warning */` |
|       - | 11266 | `	/* Create the loop block */` |
|      47 | 11267 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 11268 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 | 11269 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 11270 | `		return SXERR_ABORT;` |
|       - | 11271 | `	}` |
|       - | 11272 | `	/* Delimit the condition */` |
|      33 | 11273 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 | 11274 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 11275 | `		/* Empty expression */` |
|     ! 0 | 11276 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 11277 | `		if( rc == SXERR_ABORT ){` |
|       - | 11278 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 11279 | `			return SXERR_ABORT;` |
|       - | 11280 | `		}` |
|     ! 0 | 11281 | `	}` |
|       - | 11282 | `	/* Swap token streams */` |
|      33 | 11283 | `	pTmp = pGen->pEnd;` |
|      33 | 11284 | `	pGen->pEnd = pEnd;` |
|       - | 11285 | `	/* Compile the expression */` |
|      33 | 11286 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 | 11287 | `	if( rc == SXERR_ABORT ){` |
|       - | 11288 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 11289 | `		return SXERR_ABORT;` |
|       - | 11290 | `	}` |
|       - | 11291 | `	/* Update token stream */` |
|      33 | 11292 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 11293 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 11294 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 11295 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11296 | `			return SXERR_ABORT;` |
|       - | 11297 | `		}` |
|     ! 0 | 11298 | `		pGen->pIn++;` |
|     ! 0 | 11299 | `	}` |
|      33 | 11300 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 11301 | `	pGen->pEnd = pTmp;` |
|      33 | 11302 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 11303 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 11304 | `			pTmp = pGen->pIn;` |
|     ! 0 | 11305 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 11306 | `				pTmp--;` |
|     ! 0 | 11307 | `			}` |
|       - | 11308 | `			/* Unexpected token */` |
|     ! 0 | 11309 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 11310 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11311 | `				return SXERR_ABORT;` |
|       - | 11312 | `			}` |
|     ! 0 | 11313 | `			goto Synchronize;` |
|       - | 11314 | `	}` |
|       - | 11315 | `	/* Set the delimiter token */` |
|      33 | 11316 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 11317 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 11318 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 11319 | `	}else{` |
|      31 | 11320 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 11321 | `	}` |
|      33 | 11322 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 11323 | `	/* Create the switch blocks container */` |
|      33 | 11324 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 11325 | `	if( pSwitch == 0 ){` |
|       - | 11326 | `		/* Abort compilation */` |
|     ! 0 | 11327 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 11328 | `		return SXERR_ABORT;` |
|       - | 11329 | `	}` |
|       - | 11330 | `	/* Zero the structure */` |
|      33 | 11331 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 11332 | `	/* Initialize fields */` |
|      33 | 11333 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 11334 | `	/* Emit the switch instruction */` |
|      33 | 11335 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 11336 | `	/* Compile case blocks */` |
|     100 | 11337 | `	for(;;){` |
|       - | 11338 | `		sxu32 nKwrd;` |
|     119 | 11339 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11340 | `			/* No more input to process */` |
|     ! 0 | 11341 | `			break;` |
|       - | 11342 | `		}` |
|     119 | 11343 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 11344 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 11345 | `				/* Unexpected token */` |
|     ! 0 | 11346 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11347 | `					&pGen->pIn->sData);` |
|     ! 0 | 11348 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11349 | `					return SXERR_ABORT;` |
|       - | 11350 | `				}` |
|       - | 11351 | `				/* FALL THROUGH */` |
|     ! 0 | 11352 | `			}` |
|       - | 11353 | `			/* Block compiled */` |
|     ! 0 | 11354 | `			break;` |
|       - | 11355 | `		}` |
|       - | 11356 | `		/* Extract the keyword */` |
|     119 | 11357 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 11358 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 11359 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 11360 | `				/* Unexpected token */` |
|     ! 0 | 11361 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11362 | `					&pGen->pIn->sData);` |
|     ! 0 | 11363 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11364 | `					return SXERR_ABORT;` |
|       - | 11365 | `				}` |
|       - | 11366 | `				/* FALL THROUGH */` |
|     ! 0 | 11367 | `			}` |
|       - | 11368 | `			/* Block compiled */` |
|       3 | 11369 | `			break;` |
|       - | 11370 | `		}` |
|     117 | 11371 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 11372 | `			/*` |
|       - | 11373 | `			 * Accroding to the PHP language reference manual` |
|       - | 11374 | `			 *  A special case is the default case. This case matches anything` |
|       - | 11375 | `			 *  that wasn't matched by the other cases.` |
|       - | 11376 | `			 */` |
|      25 | 11377 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 11378 | `				/* Default case already compiled */` |
|     ! 0 | 11379 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 11380 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11381 | `					return SXERR_ABORT;` |
|       - | 11382 | `				}` |
|     ! 0 | 11383 | `			}` |
|      25 | 11384 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 11385 | `			/* Compile the default block */` |
|      25 | 11386 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 11387 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 11388 | `				return SXERR_ABORT;` |
|      25 | 11389 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 11390 | `				break;` |
|       1 | 11391 | `			}` |
|      98 | 11392 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 11393 | `			ph7_case_expr sCase;` |
|       - | 11394 | `			/* Standard case block */` |
|      97 | 11395 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 11396 | `			/* initialize the structure */` |
|      97 | 11397 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 11398 | `			/* Compile the case expression */` |
|      97 | 11399 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 11400 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11401 | `				return SXERR_ABORT;` |
|       - | 11402 | `			}` |
|       - | 11403 | `			/* Compile the case block */` |
|      97 | 11404 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 11405 | `			/* Insert in the switch container */` |
|      97 | 11406 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 11407 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 11408 | `				return SXERR_ABORT;` |
|      97 | 11409 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 11410 | `				break;` |
|       - | 11411 | `			}` |
|      47 | 11412 | `		}else{` |
|       - | 11413 | `			/* Unexpected token */` |
|     ! 0 | 11414 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11415 | `				&pGen->pIn->sData);` |
|     ! 0 | 11416 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11417 | `				return SXERR_ABORT;` |
|       - | 11418 | `			}` |
|     ! 0 | 11419 | `			break;` |
|       - | 11420 | `		}` |
|       5 | 11421 | `	}` |
|       - | 11422 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 11423 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 11424 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11425 | `	/* Release the loop block */` |
|      33 | 11426 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 11427 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 11428 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 11429 | `		pGen->pIn++;` |
|      14 | 11430 | `	}` |
|       - | 11431 | `	/* Statement successfully compiled */` |
|      33 | 11432 | `	return SXRET_OK;` |
|     ! 0 | 11433 | `Synchronize:` |
|       - | 11434 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 11435 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 11436 | `		pGen->pIn++;` |
|     ! 0 | 11437 | `	}` |
|     ! 0 | 11438 | `	return SXRET_OK;` |
|      19 | 11439 | `}` |
|       - | 11440 | `/*` |
|       - | 11441 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 11442 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 11443 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 11444 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 11445 | ` */` |
|       - | 11446 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 11447 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 11448 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 11449 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 11450 |  |
|       - | 11451 | `/*` |
|       - | 11452 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 11453 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 11454 | ` * patched entries from the pending set.` |
|       - | 11455 | ` */` |
| 2840044 | 11456 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 11457 | `{` |
| 2840049 | 11458 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 11459 | `	sxu32 nTarget;` |
|       - | 11460 | `	sxu32 *aIdx;` |
|       - | 11461 | `	sxu32 i;` |
| 2840049 | 11462 | `	if( nCur <= nBaseline ){` |
| 2839953 | 11463 | `		return;` |
|       - | 11464 | `	}` |
|     100 | 11465 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|     100 | 11466 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     204 | 11467 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     108 | 11468 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     108 | 11469 | `		if( pInstr ){` |
|     108 | 11470 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      52 | 11471 | `		}` |
|      56 | 11472 | `	}` |
|     100 | 11473 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1420027 | 11474 | `}` |
|       - | 11475 |  |
|       - | 11476 | `/*` |
|       - | 11477 | ` * By-reference out-parameters of builtin functions.` |
|       - | 11478 | ` *` |
|       - | 11479 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 11480 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 11481 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 11482 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 11483 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 11484 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 11485 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 11486 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 11487 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 11488 | ` * creates it" behaviour).` |
|       - | 11489 | ` *` |
|       - | 11490 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 11491 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 11492 | ` */` |
|  478222 | 11493 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 11494 | `{` |
|       - | 11495 | `	static const struct {` |
|       - | 11496 | `		const char *zName;` |
|       - | 11497 | `		sxu32 nByte;` |
|       - | 11498 | `		sxu32 mask;` |
|       - | 11499 | `	} aByRef[] = {` |
|       - | 11500 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 11501 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 11502 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 11503 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 11504 | `	};` |
|       - | 11505 | `	sxu32 i;` |
|  478227 | 11506 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    2329 | 11507 | `		return 0;` |
|       - | 11508 | `	}` |
| 2379223 | 11509 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1903414 | 11510 | `		if( pName->nByte == aByRef[i].nByte` |
|  975767 | 11511 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      99 | 11512 | `			return aByRef[i].mask;` |
|       - | 11513 | `		}` |
|  951665 | 11514 | `	}` |
|  475809 | 11515 | `	return 0;` |
|  239116 | 11516 | `}` |
|       - | 11517 | `/*` |
|       - | 11518 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 11519 | ` *` |
|       - | 11520 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 11521 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 11522 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 11523 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 11524 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 11525 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 11526 | ` */` |
|  478222 | 11527 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 11528 | `{` |
|       - | 11529 | `	SyToken *p, *pEnd;` |
|  478227 | 11530 | `	pOut->zString = 0;` |
|  478227 | 11531 | `	pOut->nByte = 0;` |
|  478227 | 11532 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 11533 | `		return;` |
|       - | 11534 | `	}` |
|  478227 | 11535 | `	p = pLeft->pStart;` |
|  478227 | 11536 | `	pEnd = pLeft->pEnd;` |
|       - | 11537 | `	/* Optional single leading namespace separator (absolute path). */` |
|  478227 | 11538 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|    3861 | 11539 | `		p++;` |
|    1928 | 11540 | `	}` |
|  478227 | 11541 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    2293 | 11542 | `		return;` |
|       - | 11543 | `	}` |
|       - | 11544 | `	/* Must be a single component: nothing follows the name token. */` |
|  475939 | 11545 | `	if( p + 1 != pEnd ){` |
|      41 | 11546 | `		return;` |
|       - | 11547 | `	}` |
|  475903 | 11548 | `	*pOut = p->sData;` |
|  239116 | 11549 | `}` |
|       - | 11550 | `/*` |
|       - | 11551 | ` * Generate bytecode for a given expression tree.` |
|       - | 11552 | ` * If something goes wrong while generating bytecode` |
|       - | 11553 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 11554 | ` * this function takes care of generating the appropriate` |
|       - | 11555 | ` * error message.` |
|       - | 11556 | ` */` |
| 3803402 | 11557 | `static sxi32 GenStateEmitExprCode(` |
|       - | 11558 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11559 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 11560 | `	sxi32 iFlags /* Control flags */` |
|       - | 11561 | `	)` |
|       5 | 11562 | `{` |
|       - | 11563 | `	VmInstr *pInstr;` |
|       - | 11564 | `	sxu32 nJmpIdx;` |
| 3803407 | 11565 | `	sxi32 iP1 = 0;` |
| 3803407 | 11566 | `	sxu32 iP2 = 0;` |
| 3803407 | 11567 | `	void *p3  = 0;` |
|       - | 11568 | `	sxi32 iVmOp;` |
|       - | 11569 | `	sxi32 rc;` |
| 3803407 | 11570 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3803407 | 11571 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 3803407 | 11572 | `	sxu32 nRhsNsBase = 0;` |
| 3803407 | 11573 | `	if( pNode->xCode ){` |
|       - | 11574 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 11575 | `		/* Compile node */` |
| 2373913 | 11576 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2373913 | 11577 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2373913 | 11578 | `		RE_SWAP_DELIMITER(pGen);` |
| 2373913 | 11579 | `		return rc;` |
|       - | 11580 | `	}` |
| 1429499 | 11581 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 11582 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 11583 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 11584 | `		return SXERR_ABORT;` |
|       - | 11585 | `	}` |
| 1429499 | 11586 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1429499 | 11587 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|       - | 11588 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|       - | 11589 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|       - | 11590 | `		 * and later errors are still reported. */` |
|       3 | 11591 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 11592 | `			"The (unset) cast is no longer supported");` |
|       3 | 11593 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11594 | `			return SXERR_ABORT;` |
|       - | 11595 | `		}` |
|       1 | 11596 | `	}` |
| 1429499 | 11597 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      65 | 11598 | `		sxu32 nJmp = 0;` |
|       - | 11599 | `		sxu32 nNcNsBase;` |
|       - | 11600 | `		VmInstr *pInstrFix;` |
|       - | 11601 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 11602 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 11603 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 11604 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 11605 | `		 * stack slot carries a writable nIdx. */` |
|      65 | 11606 | `		if( pNode->pRight ){` |
|      65 | 11607 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 11608 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|      65 | 11609 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11610 | `				return rc;` |
|       - | 11611 | `			}` |
|      65 | 11612 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 11613 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 11614 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 11615 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 11616 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 11617 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 11618 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 11619 | `			 * cascade for the actual write path stays correct. */` |
|      65 | 11620 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 11621 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      31 | 11622 | `				pInstrFix->iP2 = 3;` |
|      14 | 11623 | `			}` |
|      31 | 11624 | `		}` |
|       - | 11625 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      65 | 11626 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 11627 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      65 | 11628 | `		if( pNode->pLeft ){` |
|      65 | 11629 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 11630 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      65 | 11631 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11632 | `				return rc;` |
|       - | 11633 | `			}` |
|      65 | 11634 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      31 | 11635 | `		}` |
|       - | 11636 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      65 | 11637 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 11638 | `		/* Patch the short-circuit jump to land after the store. */` |
|      65 | 11639 | `		if( nJmp > 0 ){` |
|      65 | 11640 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      65 | 11641 | `			if( pInstrFix ){` |
|      65 | 11642 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      31 | 11643 | `			}` |
|      31 | 11644 | `		}` |
|      65 | 11645 | `		return SXRET_OK;` |
|       - | 11646 | `	}` |
| 1429437 | 11647 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 11648 | `		sxu32 nJz,nJmp;` |
|       - | 11649 | `		sxu32 nTernaryNsBase;` |
|       - | 11650 | `		/* Ternary operator require special handling */` |
|       - | 11651 | `		/* Phase#1: Compile the condition */` |
|    2651 | 11652 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2651 | 11653 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2651 | 11654 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11655 | `			return rc;` |
|       - | 11656 | `		}` |
|       - | 11657 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 11658 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 11659 | `		 * condition expression, not leak past the ternary. */` |
|    2651 | 11660 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2651 | 11661 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2651 | 11662 | `		if( pNode->pLeft ){` |
|       - | 11663 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 11664 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2583 | 11665 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 11666 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2583 | 11667 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2583 | 11668 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2583 | 11669 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11670 | `				return rc;` |
|       - | 11671 | `			}` |
|    2583 | 11672 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1294 | 11673 | `		}else{` |
|       - | 11674 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 11675 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 11676 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 11677 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 11678 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 11679 | `		}` |
|       - | 11680 | `		/* Phase#4: Emit the unconditional jump */` |
|    2651 | 11681 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 11682 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2651 | 11683 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2651 | 11684 | `		if( pInstr ){` |
|    2651 | 11685 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1323 | 11686 | `		}` |
|    2651 | 11687 | `		if( !pNode->pLeft ){` |
|       - | 11688 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 11689 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 11690 | `		}` |
|       - | 11691 | `		/* Phase#6: Compile the 'else' expression */` |
|    2651 | 11692 | `		if( pNode->pRight ){` |
|    2651 | 11693 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2651 | 11694 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2651 | 11695 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11696 | `				return rc;` |
|       - | 11697 | `			}` |
|    2651 | 11698 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1323 | 11699 | `		}` |
|    2651 | 11700 | `		if( nJmp > 0 ){` |
|       - | 11701 | `			/* Phase#7: Fix the unconditional jump */` |
|    2651 | 11702 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2651 | 11703 | `			if( pInstr ){` |
|    2651 | 11704 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1323 | 11705 | `			}` |
|    1323 | 11706 | `		}` |
|       - | 11707 | `		/* All done */` |
|    2651 | 11708 | `		return SXRET_OK;` |
|       - | 11709 | `	}` |
| 1426791 | 11710 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|       - | 11711 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|       - | 11712 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|       - | 11713 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|       - | 11714 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|       - | 11715 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|       - | 11716 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|       - | 11717 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|       - | 11718 | `		sxu32 nPipeNsBase;` |
|      27 | 11719 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|      27 | 11720 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|     ! 0 | 11721 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 11722 | `				"'\|>': Missing operand");` |
|     ! 0 | 11723 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - | 11724 | `		}` |
|       - | 11725 | `		/* Argument: the LHS value. */` |
|      27 | 11726 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      27 | 11727 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|      27 | 11728 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11729 | `			return rc;` |
|       - | 11730 | `		}` |
|      27 | 11731 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|       - | 11732 | `		/* Callable: the RHS. */` |
|      27 | 11733 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      27 | 11734 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|      27 | 11735 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11736 | `			return rc;` |
|       - | 11737 | `		}` |
|      27 | 11738 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|       - | 11739 | `		/* Invoke the callable with the single piped argument. */` |
|      27 | 11740 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      27 | 11741 | `		return SXRET_OK;` |
|       - | 11742 | `	}` |
| 1426765 | 11743 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 11744 | `	/* Generate code for the left tree */` |
| 1426765 | 11745 | `	if( pNode->pLeft ){` |
| 1426733 | 11746 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1426733 | 11747 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 11748 | `			ph7_expr_node **apNode;` |
|  482221 | 11749 | `			int hasSpread = 0;` |
|  482221 | 11750 | `			int hasNamed = 0;` |
|  482221 | 11751 | `			int bAnySpread = 0;` |
|  482221 | 11752 | `			sxu32 byRefMask = 0;` |
|       - | 11753 | `			sxi32 nArgs;` |
|       - | 11754 | `			sxi32 n;` |
|       - | 11755 | `			/* Recurse and generate bytecodes for function arguments */` |
|  482221 | 11756 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  482221 | 11757 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 11758 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|       - | 11759 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|       - | 11760 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  482221 | 11761 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|      77 | 11762 | `				bFcc = 1;` |
|      77 | 11763 | `				nArgs = 0;` |
|      38 | 11764 | `			}` |
|       - | 11765 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 11766 | `			{` |
|  482221 | 11767 | `				int seenNamed = 0;` |
|  977967 | 11768 | `				for( n = 0; n < nArgs; ++n ){` |
|  495753 | 11769 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     253 | 11770 | `						seenNamed = 1;` |
|     253 | 11771 | `						hasNamed = 1;` |
|  495629 | 11772 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|    3859 | 11773 | `						bAnySpread = 1;` |
|  493578 | 11774 | `					}else if( seenNamed ){` |
|       3 | 11775 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 11776 | `							"Cannot use positional argument after named argument");` |
|       3 | 11777 | `						return SXERR_SYNTAX;` |
|       - | 11778 | `					}` |
|  247878 | 11779 | `				}` |
|       - | 11780 | `			}` |
|       - | 11781 | `			/* Read-only load */` |
|  482219 | 11782 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 11783 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 11784 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 11785 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 11786 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  482219 | 11787 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  482219 | 11788 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  482214 | 11789 | `				if( pCallName->nByte == 5` |
|  263243 | 11790 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   23305 | 11791 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  470569 | 11792 | `				}else if( pCallName->nByte == 5` |
|  239943 | 11793 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      99 | 11794 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      47 | 11795 | `				}` |
|       - | 11796 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 11797 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 11798 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 11799 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 11800 | `				 * the compile-time positional index no longer maps to the` |
|       - | 11801 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  482219 | 11802 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 11803 | `					SyString sBuiltin;` |
|  478227 | 11804 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  478227 | 11805 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  239111 | 11806 | `				}` |
|  241107 | 11807 | `			}` |
|  977963 | 11808 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  495749 | 11809 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  495749 | 11810 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11811 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 11812 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|       - | 11813 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|       - | 11814 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|       - | 11815 | `				 * builtin to write back through. A plain $var target is unaffected` |
|       - | 11816 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  495749 | 11817 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      55 | 11818 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      55 | 11819 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      25 | 11820 | `				}` |
|  495749 | 11821 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  495749 | 11822 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11823 | `					return rc;` |
|       - | 11824 | `				}` |
|       - | 11825 | `				/* Each argument is an independent nullsafe scope. */` |
|  495749 | 11826 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  495749 | 11827 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 11828 | `					/* Emit spread opcode to unpack this array argument */` |
|    3859 | 11829 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|    3859 | 11830 | `					hasSpread = 1;` |
|    1927 | 11831 | `				}` |
|  247877 | 11832 | `			}` |
|       - | 11833 | `			/* Total number of given arguments */` |
|  482219 | 11834 | `			iP1 = nArgs;` |
|  482219 | 11835 | `			iP2 = hasSpread;` |
|       - | 11836 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 11837 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  482219 | 11838 | `			if( hasNamed ){` |
|     142 | 11839 | `				sxu32 nStrBytes = 0;` |
|       - | 11840 | `				char *zBuf;` |
|     424 | 11841 | `				for( n = 0; n < nArgs; ++n ){` |
|     286 | 11842 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     250 | 11843 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|     123 | 11844 | `					}` |
|     145 | 11845 | `				}` |
|       - | 11846 | `				{` |
|     142 | 11847 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     142 | 11848 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|     138 | 11849 | `					&pGen->pVm->sAllocator, mapSize);` |
|     142 | 11850 | `				if( pMap ){` |
|     142 | 11851 | `					SyZero(pMap, mapSize);` |
|     142 | 11852 | `					pMap->bHasNamed = 1;` |
|     142 | 11853 | `					pMap->nTotal = (sxu32)nArgs;` |
|     142 | 11854 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     142 | 11855 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     424 | 11856 | `					for( n = 0; n < nArgs; ++n ){` |
|     286 | 11857 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     250 | 11858 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     250 | 11859 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     250 | 11860 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     250 | 11861 | `							zBuf += nb;` |
|     123 | 11862 | `						}` |
|       - | 11863 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     145 | 11864 | `					}` |
|     142 | 11865 | `					p3 = (void *)pMap;` |
|      69 | 11866 | `				}` |
|       - | 11867 | `				}` |
|      69 | 11868 | `			}` |
|       - | 11869 | `			/* Remove stale flags now */` |
|  482219 | 11870 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  241107 | 11871 | `		}` |
|       - | 11872 | `		{` |
|       - | 11873 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|       - | 11874 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|       - | 11875 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|       - | 11876 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|       - | 11877 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|       - | 11878 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|       - | 11879 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|       - | 11880 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 1426731 | 11881 | `			sxi32 iLeftFlags = iFlags;` |
| 1610351 | 11882 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  905726 | 11883 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  376014 | 11884 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  367281 | 11885 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   17689 | 11886 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|    8842 | 11887 | `			}` |
|       - | 11888 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|       - | 11889 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|       - | 11890 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|       - | 11891 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|       - | 11892 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|       - | 11893 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|       - | 11894 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 2044589 | 11895 | `			if( pNode->pOp` |
| 1426731 | 11896 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 1331278 | 11897 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 1235778 | 11898 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  191383 | 11899 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   95689 | 11900 | `			}` |
| 1426731 | 11901 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|       - | 11902 | `		}` |
| 1426731 | 11903 | `		if( rc != SXRET_OK ){` |
|      34 | 11904 | `			return rc;` |
|       - | 11905 | `		}` |
| 1426701 | 11906 | `		if( !bIsChainOp ){` |
|       - | 11907 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 11908 | `			 * target the end of that LHS chain, which is right here. */` |
|  654739 | 11909 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  327367 | 11910 | `		}` |
| 1426701 | 11911 | `		if( iVmOp == PH7_OP_CALL ){` |
|  482219 | 11912 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  482219 | 11913 | `			if( pInstr ){` |
|  482219 | 11914 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  476055 | 11915 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 11916 | `					sxu32 nQual;` |
|  476055 | 11917 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11918 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 11919 | `					 * so the later NEW handler (if any) can see it. */` |
|  476055 | 11920 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 11921 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 11922 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 11923 | `					 * imports — class imports must NOT affect function` |
|       - | 11924 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 11925 | `					 * before NEW; we store the original literal index in the` |
|       - | 11926 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 11927 | `					 * the unqualified name and re-qualify with class imports. */` |
|  476055 | 11928 | `					if( bAbsolute ){` |
|    3861 | 11929 | `						pInstr->iP2 = (sxi32)nOrig;` |
|    1933 | 11930 | `					}else{` |
|  472199 | 11931 | `						int fromImport = 0;` |
|  472199 | 11932 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  472199 | 11933 | `						pInstr->iP2 = (sxi32)nQual;` |
|  472199 | 11934 | `						if( nQual != nOrig ){` |
|       - | 11935 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 11936 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 11937 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 11938 | `							if( !fromImport ){` |
|       - | 11939 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 11940 | `								if( p3 == 0 ){` |
|      67 | 11941 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 11942 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 11943 | `									if( pMap ){` |
|      67 | 11944 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 11945 | `										p3 = (void *)pMap;` |
|      31 | 11946 | `									}` |
|      31 | 11947 | `								}` |
|      67 | 11948 | `								if( p3 ){` |
|      67 | 11949 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 11950 | `								}` |
|      31 | 11951 | `							}` |
|      36 | 11952 | `						}` |
|       5 | 11953 | `					}` |
|  244194 | 11954 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 11955 | `					/* Method call,flag that */` |
|    1883 | 11956 | `					pInstr->iP2 = 1;` |
|     939 | 11957 | `				}` |
|  241112 | 11958 | `			}` |
| 1185594 | 11959 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 11960 | `			ph7_expr_node **apNode;` |
|       - | 11961 | `			sxi32 n;` |
|   98375 | 11962 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 11963 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 11964 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11965 | `			/* Recurse and generate bytecodes for array index */` |
|   98375 | 11966 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  177495 | 11967 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   79125 | 11968 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   79125 | 11969 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   79125 | 11970 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11971 | `					return rc;` |
|       - | 11972 | `				}` |
|       - | 11973 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   79125 | 11974 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   39565 | 11975 | `			}` |
|   98375 | 11976 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   79125 | 11977 | `				iP1 = 1; /* Node have an index associated with it */` |
|   39560 | 11978 | `			}` |
|   98375 | 11979 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 11980 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     245 | 11981 | `				iP2 = 4;` |
|   98255 | 11982 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 11983 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 11984 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      56 | 11985 | `				iP2 = 5;` |
|   98109 | 11986 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 11987 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 11988 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 11989 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 11990 | `				iP2 = 6;` |
|   98071 | 11991 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 11992 | `				/* Create an empty entry when the desired index is not found */` |
|   38805 | 11993 | `				iP2 = 1;` |
|   19405 | 11994 | `			}` |
|  895302 | 11995 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 11996 | `			/* POP the left node */` |
|      32 | 11997 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 11998 | `		}` |
|  713348 | 11999 | `	}` |
| 1426733 | 12000 | `	rc = SXRET_OK;` |
| 1426733 | 12001 | `	nJmpIdx = 0;` |
|       - | 12002 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 12003 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 12004 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1426733 | 12005 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     415 | 12006 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     415 | 12007 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     415 | 12008 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     415 | 12009 | `			int isSpecial = 0;` |
|     415 | 12010 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     323 | 12011 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     323 | 12012 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     335 | 12013 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     287 | 12014 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     146 | 12015 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|     111 | 12016 | `					isSpecial = 1;` |
|      53 | 12017 | `				}` |
|     182 | 12018 | `			}` |
|     461 | 12019 | `			pInstr->iP1 = 0;` |
|     461 | 12020 | `			if( !isSpecial ){` |
|     263 | 12021 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     129 | 12022 | `			}` |
|       - | 12023 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 12024 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     369 | 12025 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     263 | 12026 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     263 | 12027 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      46 | 12028 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      48 | 12029 | `					return SXRET_OK;` |
|       - | 12030 | `				}` |
|     107 | 12031 | `			}` |
|     160 | 12032 | `		}` |
|     231 | 12033 | `	}` |
|       - | 12034 | `	/* Generate code for the right tree */` |
| 1426657 | 12035 | `	if( pNode->pRight ){` |
|  769513 | 12036 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 12037 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   11997 | 12038 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  763517 | 12039 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 12040 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    4013 | 12041 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  755517 | 12042 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 12043 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     131 | 12044 | `			iVmOp = 0; /* No binary operator to emit */` |
|     131 | 12045 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  753502 | 12046 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 12047 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 12048 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 12049 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 12050 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 12051 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 12052 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     108 | 12053 | `			sxu32 nNsJmp = 0;` |
|     108 | 12054 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     108 | 12055 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  753335 | 12056 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|       - | 12057 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|       - | 12058 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|       - | 12059 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  320107 | 12060 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  160051 | 12061 | `		}` |
|  769513 | 12062 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  769513 | 12063 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  769513 | 12064 | `		if( !bIsChainOp ){` |
|       - | 12065 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 12066 | `			 * operator instruction is emitted. */` |
|  578179 | 12067 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  289087 | 12068 | `		}` |
|  769513 | 12069 | `		if( iVmOp == PH7_OP_STORE ){` |
|  316005 | 12070 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  315970 | 12071 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 12072 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 12073 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 12074 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 12075 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 12076 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 12077 | `				 */` |
|      85 | 12078 | `				iVmOp = 0;` |
|  315965 | 12079 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  315925 | 12080 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 12081 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   84731 | 12082 | `					iP2 = 1;` |
|   42368 | 12083 | `				}else{` |
|  231199 | 12084 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 12085 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   38727 | 12086 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   38727 | 12087 | `						iP1 = pInstr->iP1;` |
|   19366 | 12088 | `					}else{` |
|  192477 | 12089 | `						p3 = pInstr->p3;` |
|       - | 12090 | `					}` |
|       - | 12091 | `					/* POP the last dynamic load instruction */` |
|  231199 | 12092 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 12093 | `				}` |
|  157965 | 12094 | `			}` |
|  611513 | 12095 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      57 | 12096 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      57 | 12097 | `			if( pInstr ){` |
|      57 | 12098 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 12099 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 12100 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 12101 | `					 */` |
|      17 | 12102 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      17 | 12103 | `					iP1 = pInstr->iP1;` |
|      17 | 12104 | `					iP2 = pInstr->iP2;` |
|      17 | 12105 | `					p3  = pInstr->p3;` |
|       9 | 12106 | `				}else{` |
|      41 | 12107 | `					p3 = pInstr->p3;` |
|       - | 12108 | `				}` |
|      27 | 12109 | `			}` |
|      27 | 12110 | `		}` |
|  384754 | 12111 | `	}` |
| 1426652 | 12112 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   12474 | 12113 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|       - | 12114 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|       - | 12115 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|      30 | 12116 | `		iVmOp = 0;` |
|      13 | 12117 | `	}` |
| 1426657 | 12118 | `	if( iVmOp > 0 ){` |
| 1426395 | 12119 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   15725 | 12120 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 12121 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|   11509 | 12122 | `				iP1 = 1;` |
|    5757 | 12123 | `			}` |
| 1418535 | 12124 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 12125 | `			/* Namespace-qualify the class name for NEW */ {` |
|   24699 | 12126 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   24699 | 12127 | `				VmInstr *pCallInstr = 0;` |
|   24699 | 12128 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   24507 | 12129 | `					pCallInstr = pPeek;` |
|   24507 | 12130 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   12251 | 12131 | `				}` |
|   24699 | 12132 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   24695 | 12133 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 12134 | `					sxu32 nLitForClass;` |
|       - | 12135 | `					/* If the CALL handler already qualified the name using` |
|       - | 12136 | `					 * function imports, recover the original unqualified` |
|       - | 12137 | `					 * literal so we can re-qualify with class imports. */` |
|   24695 | 12138 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 12139 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 12140 | `					}else{` |
|   24663 | 12141 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 12142 | `					}` |
|   24695 | 12143 | `					pPeek->iP1 = 0;` |
|   24695 | 12144 | `					if( !bAbsolute ){` |
|   20843 | 12145 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|   10424 | 12146 | `					}else{` |
|    3857 | 12147 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 12148 | `					}` |
|   12345 | 12149 | `				}` |
|       - | 12150 | `			}` |
|   24699 | 12151 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   24699 | 12152 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 12153 | `				VmInstr *pPrev;` |
|   24507 | 12154 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   24507 | 12155 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 12156 | `					/* Pop the call instruction, preserve named-arg map */` |
|   24507 | 12157 | `					iP1 = pInstr->iP1;` |
|   24507 | 12158 | `					if( pInstr->p3 ){` |
|      43 | 12159 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 12160 | `					}` |
|   24507 | 12161 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   12251 | 12162 | `				}` |
|   12256 | 12163 | `			}` |
| 1398328 | 12164 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 12165 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 12166 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     203 | 12167 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     203 | 12168 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     203 | 12169 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     203 | 12170 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     203 | 12171 | `				int isSpecialIs = 0;` |
|     203 | 12172 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     203 | 12173 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     203 | 12174 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     203 | 12175 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     196 | 12176 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      99 | 12177 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 12178 | `						isSpecialIs = 1;` |
|       5 | 12179 | `					}` |
|      99 | 12180 | `				}` |
|     203 | 12181 | `				pInstr->iP1 = 0;` |
|     203 | 12182 | `				if( !isSpecialIs && !bAbsolute ){` |
|     183 | 12183 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      89 | 12184 | `				}` |
|     104 | 12185 | `			}` |
| 1385882 | 12186 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 12187 | `			/* Prevent constant expansion for member/property names.` |
|       - | 12188 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 12189 | `			 * should not trigger constant lookup. */` |
|  191339 | 12190 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  191339 | 12191 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  191289 | 12192 | `				pInstr->iP1 = 0;` |
|   95642 | 12193 | `			}` |
|  191339 | 12194 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 12195 | `				/* Static member access,remember that */` |
|     339 | 12196 | `				iP1 = 1;` |
|     339 | 12197 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     339 | 12198 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      44 | 12199 | `					p3 = pInstr->p3;` |
|      44 | 12200 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      20 | 12201 | `				}` |
|     167 | 12202 | `			}` |
|       - | 12203 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|       - | 12204 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|       - | 12205 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|       - | 12206 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  191339 | 12207 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  191339 | 12208 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|      30 | 12209 | `					iP2 = PH7_MEMBER_UNSET;` |
|  191325 | 12210 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|      77 | 12211 | `					iP2 = PH7_MEMBER_ISSET;` |
|  191275 | 12212 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|      13 | 12213 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  191233 | 12214 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|       - | 12215 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   84811 | 12216 | `					iP2 = PH7_MEMBER_WRITE;` |
|   42403 | 12217 | `				}` |
|   95667 | 12218 | `			}` |
|   95667 | 12219 | `		}` |
|       - | 12220 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|       - | 12221 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|       - | 12222 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|       - | 12223 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|       - | 12224 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 1426395 | 12225 | `		if( bFcc ){` |
|      77 | 12226 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|      77 | 12227 | `			iP2 = 0;` |
|      77 | 12228 | `			p3 = 0;` |
|      77 | 12229 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      77 | 12230 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 12231 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|       - | 12232 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|       - | 12233 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|       - | 12234 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|      35 | 12235 | `				void *pMemberName = pInstr->p3;` |
|      35 | 12236 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|      35 | 12237 | `				if( pMemberName ){` |
|       3 | 12238 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       1 | 12239 | `				}` |
|      35 | 12240 | `				iP1 = 2;` |
|      18 | 12241 | `			}else{` |
|      43 | 12242 | `				iP1 = 1;` |
|       - | 12243 | `			}` |
|      38 | 12244 | `		}` |
|       - | 12245 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 12246 | `		 * This is the primary emit path for user-visible calls. */` |
| 1426395 | 12247 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  506837 | 12248 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  253416 | 12249 | `		}` |
|       - | 12250 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1426395 | 12251 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  713195 | 12252 | `	}` |
| 1426657 | 12253 | `	if( nJmpIdx > 0 ){` |
|       - | 12254 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   16131 | 12255 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   16131 | 12256 | `		if( pInstr ){` |
|   16131 | 12257 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    8063 | 12258 | `		}` |
|    8063 | 12259 | `	}` |
| 1426657 | 12260 | `	return rc;` |
| 1901690 | 12261 | `}` |
|       - | 12262 | `/*` |
|       - | 12263 | ` * Compile a PHP expression.` |
|       - | 12264 | ` * According to the PHP language reference manual:` |
|       - | 12265 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 12266 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 12267 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 12268 | ` *  is "anything that has a value".` |
|       - | 12269 | ` * If something goes wrong while compiling the expression,this` |
|       - | 12270 | ` * function takes care of generating the appropriate error` |
|       - | 12271 | ` * message.` |
|       - | 12272 | ` */` |
| 1024418 | 12273 | `static sxi32 PH7_CompileExpr(` |
|       - | 12274 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 12275 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 12276 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 12277 | `	)` |
|       5 | 12278 | `{` |
|       - | 12279 | `	ph7_expr_node *pRoot;` |
|       - | 12280 | `	SySet sExprNode;` |
|       - | 12281 | `	SyToken *pEnd;` |
|       - | 12282 | `	sxi32 nExpr;` |
|       - | 12283 | `	sxi32 iNest;` |
|       - | 12284 | `	sxi32 rc;` |
|       - | 12285 | `	sxu32 nNullsafeBase;` |
|       - | 12286 | `	/* Initialize worker variables */` |
| 1024423 | 12287 | `	nExpr = 0;` |
| 1024423 | 12288 | `	pRoot = 0;` |
|       - | 12289 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 12290 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
| 1024423 | 12291 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1024423 | 12292 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
| 1024423 | 12293 | `	SySetAlloc(&sExprNode,0x10);` |
| 1024423 | 12294 | `	rc = SXRET_OK;` |
|       - | 12295 | `	/* Delimit the expression */` |
| 1024423 | 12296 | `	pEnd = pGen->pIn;` |
| 1024423 | 12297 | `	iNest = 0;` |
| 6912573 | 12298 | `	while( pEnd < pGen->pEnd ){` |
| 6560545 | 12299 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 12300 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     557 | 12301 | `			iNest++;` |
| 6560269 | 12302 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     565 | 12303 | `			iNest--;` |
| 6559713 | 12304 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  672823 | 12305 | `			if( iNest <= 0 ){` |
|  672395 | 12306 | `				break;` |
|       - | 12307 | `			}` |
|     214 | 12308 | `		}` |
| 5888155 | 12309 | `		pEnd++;` |
|       5 | 12310 | `	}` |
| 1024423 | 12311 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   23573 | 12312 | `		SyToken *pEnd2 = pGen->pIn;` |
|   23573 | 12313 | `		iNest = 0;` |
|       - | 12314 | `		/* Stop at the first comma */` |
|   47459 | 12315 | `		while( pEnd2 < pEnd ){` |
|   23897 | 12316 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      71 | 12317 | `				iNest++;` |
|   23864 | 12318 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      71 | 12319 | `				iNest--;` |
|   23798 | 12320 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      57 | 12321 | `				if( iNest <= 0 ){` |
|       7 | 12322 | `					break;` |
|       - | 12323 | `				}` |
|      23 | 12324 | `			}` |
|   23891 | 12325 | `			pEnd2++;` |
|       5 | 12326 | `		}` |
|   23573 | 12327 | `		if( pEnd2 <pEnd ){` |
|       7 | 12328 | `			pEnd = pEnd2;` |
|       3 | 12329 | `		}` |
|   11784 | 12330 | `	}` |
| 1024423 | 12331 | `	if( pEnd > pGen->pIn ){` |
| 1024413 | 12332 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 12333 | `		/* Swap delimiter */` |
| 1024413 | 12334 | `		pGen->pEnd = pEnd;` |
|       - | 12335 | `		/* Try to get an expression tree */` |
| 1024413 | 12336 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
| 1024413 | 12337 | `		if( rc == SXRET_OK && pRoot ){` |
| 1024231 | 12338 | `			rc = SXRET_OK;` |
| 1024231 | 12339 | `			if( xTreeValidator ){` |
|       - | 12340 | `				/* Call the upper layer validator callback */` |
|   30901 | 12341 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   15448 | 12342 | `			}` |
| 1024231 | 12343 | `			if( rc != SXERR_ABORT ){` |
|       - | 12344 | `				/* Generate code for the given tree */` |
| 1024231 | 12345 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 12346 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 12347 | `				 * expression so they short-circuit to its end. */` |
| 1024231 | 12348 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  512113 | 12349 | `			}` |
| 1024231 | 12350 | `			nExpr = 1;` |
|  512113 | 12351 | `		}` |
|       - | 12352 | `		/* Release the whole tree */` |
| 1024413 | 12353 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 12354 | `		/* Synchronize token stream */` |
| 1024413 | 12355 | `		pGen->pEnd = pTmp;` |
| 1024413 | 12356 | `		pGen->pIn  = pEnd;` |
| 1024413 | 12357 | `		if( rc == SXERR_ABORT ){` |
|      13 | 12358 | `			SySetRelease(&sExprNode);` |
|      13 | 12359 | `			return SXERR_ABORT;` |
|       - | 12360 | `		}` |
|  512199 | 12361 | `	}` |
| 1024413 | 12362 | `	SySetRelease(&sExprNode);` |
| 1024413 | 12363 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  512214 | 12364 | `}` |
|       - | 12365 | `/*` |
|       - | 12366 | ` * Return a pointer to the node construct handler associated` |
|       - | 12367 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 12368 | ` */` |
|  266670 | 12369 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 12370 | `{` |
|  266675 | 12371 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 12372 | `		/* Numeric literal: Either real or integer */` |
|  135009 | 12373 | `		return PH7_CompileNumLiteral;` |
|  131671 | 12374 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 12375 | `		/* Double quoted string */` |
|   24925 | 12376 | `		return PH7_CompileString;` |
|  106751 | 12377 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 12378 | `		/* Single quoted string */` |
|  106631 | 12379 | `		return PH7_CompileSimpleString;` |
|     125 | 12380 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 12381 | `		/* Heredoc */` |
|      71 | 12382 | `		return PH7_CompileHereDoc;` |
|      59 | 12383 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 12384 | `		/* Nowdoc */` |
|      51 | 12385 | `		return PH7_CompileNowDoc;` |
|       9 | 12386 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 12387 | `		/* Backtick quoted string */` |
|       6 | 12388 | `		return PH7_CompileBacktic;` |
|       - | 12389 | `	}` |
|       3 | 12390 | `	return 0;` |
|  133340 | 12391 | `}` |
|       - | 12392 | `/*` |
|       - | 12393 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 12394 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 12395 | ` * in write context" parse error.` |
|       - | 12396 | ` */` |
|    6698 | 12397 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 12398 | `{` |
|       - | 12399 | `	sxi32 rc;` |
|    6703 | 12400 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6701 | 12401 | `		return SXRET_OK;` |
|       - | 12402 | `	}` |
|       5 | 12403 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 12404 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 12405 | `		"Can't use nullsafe operator in write context");` |
|       3 | 12406 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3354 | 12407 | `}` |
|       - | 12408 | `/*` |
|       - | 12409 | ` * Compile an unset() statement.` |
|       - | 12410 | ` * unset($var, $arr[$key], ...);` |
|       - | 12411 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 12412 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 12413 | ` * parent array before extracting the element to unset.` |
|       - | 12414 | ` */` |
|    2882 | 12415 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 12416 | `{` |
|    2887 | 12417 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2887 | 12418 | `	sxu32 nIdx = 0;` |
|       - | 12419 | `	SyString sName;` |
|       - | 12420 | `	sxi32 rc;` |
|       - | 12421 | `	/* Jump the 'unset' keyword */` |
|    2887 | 12422 | `	pGen->pIn++;` |
|       - | 12423 | `	/* Save delimiter */` |
|    2887 | 12424 | `	pTmp = pGen->pEnd;` |
|       - | 12425 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2887 | 12426 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2887 | 12427 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 12428 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 12429 | `		SyToken *pClose;` |
|    2887 | 12430 | `		pGen->pIn++;   /* Skip '(' */` |
|    2887 | 12431 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2887 | 12432 | `		pEnd = pClose; /* Stop at ')' */` |
|    1441 | 12433 | `	}` |
|    2887 | 12434 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 12435 | `	/* Resolve the 'unset' builtin name once */` |
|    2887 | 12436 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     371 | 12437 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     371 | 12438 | `		if( pObj == 0 ){` |
|     ! 0 | 12439 | `			return SXERR_ABORT;` |
|       - | 12440 | `		}` |
|     371 | 12441 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     371 | 12442 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     183 | 12443 | `	}` |
|       - | 12444 | `	/* Compile each comma-separated argument */` |
|    9587 | 12445 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6705 | 12446 | `		if( pGen->pIn < pNext ){` |
|    6705 | 12447 | `			pGen->pEnd = pNext;` |
|    6705 | 12448 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 12449 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 12450 | `				GenStateUnsetValidator);` |
|    6705 | 12451 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 12452 | `				return SXERR_ABORT;` |
|       - | 12453 | `			}` |
|    6705 | 12454 | `			if( rc != SXERR_EMPTY ){` |
|       - | 12455 | `				/* Emit call for this single argument */` |
|    6703 | 12456 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6703 | 12457 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6703 | 12458 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3349 | 12459 | `			}` |
|    3350 | 12460 | `		}` |
|       - | 12461 | `		/* Jump trailing commas */` |
|   10525 | 12462 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3825 | 12463 | `			pNext++;` |
|       5 | 12464 | `		}` |
|    6705 | 12465 | `		pGen->pIn = pNext;` |
|       5 | 12466 | `	}` |
|       - | 12467 | `	/* Skip past the closing ')' if present */` |
|    2887 | 12468 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2887 | 12469 | `		pGen->pIn++;` |
|    1441 | 12470 | `	}` |
|       - | 12471 | `	/* Restore token stream */` |
|    2887 | 12472 | `	pGen->pEnd = pTmp;` |
|    2887 | 12473 | `	return SXRET_OK;` |
|    1446 | 12474 | `}` |
|       - | 12475 | `/*` |
|       - | 12476 | ` * PHP Language construct table.` |
|       - | 12477 | ` */` |
|       - | 12478 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 12479 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 12480 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 12481 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 12482 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 12483 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 12484 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 12485 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 12486 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 12487 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 12488 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 12489 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 12490 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 12491 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 12492 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 12493 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 12494 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 12495 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 12496 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 12497 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 12498 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 12499 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 12500 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 12501 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 12502 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 12503 | `};` |
|       - | 12504 | `/*` |
|       - | 12505 | ` * Return a pointer to the statement handler routine associated` |
|       - | 12506 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 12507 | ` */` |
|  694042 | 12508 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 12509 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 12510 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 12511 | `	)` |
|       5 | 12512 | `{` |
|  694047 | 12513 | `	sxu32 n = 0;` |
| 3654459 | 12514 | `	for(;;){` |
| 7308923 | 12515 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  151123 | 12516 | `			break;` |
|       - | 12517 | `		}` |
| 7157805 | 12518 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  542929 | 12519 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 12520 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 12521 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 12522 | `					/* 'static' (class context),return null */` |
|     ! 0 | 12523 | `					return 0;` |
|       - | 12524 | `				}` |
|     ! 0 | 12525 | `			}` |
|  542924 | 12526 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       8 | 12527 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       9 | 12528 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 12529 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 12530 | `				return 0;` |
|       - | 12531 | `			}` |
|       - | 12532 | `			/* Return a pointer to the handler.` |
|       - | 12533 | `			*/` |
|  542929 | 12534 | `			return aLangConstruct[n].xConstruct;` |
|       - | 12535 | `		}` |
| 6614881 | 12536 | `		n++;` |
|       5 | 12537 | `	}` |
|  151123 | 12538 | `	if( pLookahed ){` |
|  151123 | 12539 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   42197 | 12540 | `			return PH7_CompileClassInterface;` |
|  108931 | 12541 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|  108441 | 12542 | `			return PH7_CompileClass;` |
|     495 | 12543 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      69 | 12544 | `			return PH7_CompileTrait;` |
|       - | 12545 | `		}` |
|       - | 12546 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 12547 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 12548 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 12549 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     213 | 12550 | `	}` |
|       - | 12551 | `	/* Not a language construct */` |
|     431 | 12552 | `	return 0;` |
|  347026 | 12553 | `}` |
|       - | 12554 | `/*` |
|       - | 12555 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 12556 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 12557 | ` */` |
|     426 | 12558 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 12559 | `{` |
|       - | 12560 | `	int rc;` |
|     431 | 12561 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     431 | 12562 | `	if( rc == FALSE ){` |
|     312 | 12563 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     311 | 12564 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 12565 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 12566 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 12567 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 12568 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 12569 | `			*/` |
|       - | 12570 | `			){` |
|     309 | 12571 | `				rc = TRUE;` |
|     152 | 12572 | `		}` |
|     156 | 12573 | `	}` |
|     431 | 12574 | `	return rc;` |
|       5 | 12575 | `}` |
|       - | 12576 | `/*` |
|       - | 12577 | ` * Compile a PHP chunk.` |
|       - | 12578 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 12579 | ` * takes care of generating the appropriate error message.` |
|       - | 12580 | ` */` |
|  829282 | 12581 | `static sxi32 GenStateCompileChunk(` |
|       - | 12582 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 12583 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 12584 | `	)` |
|       5 | 12585 | `{` |
|       - | 12586 | `	ProcLangConstruct xCons;` |
|       - | 12587 | `	sxi32 rc;` |
|  829287 | 12588 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  652813 | 12589 | `	for(;;){` |
| 1067459 | 12590 | `		int bStmtIsDeclare = 0;` |
| 1067459 | 12591 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 12592 | `			/* No more input to process */` |
|   18073 | 12593 | `			break;` |
|       - | 12594 | `		}` |
|       - | 12595 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 12596 | `		 * below doesn't fire before the directive has a chance to run. */` |
| 1049391 | 12597 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  697911 | 12598 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  697911 | 12599 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      47 | 12600 | `				bStmtIsDeclare = 1;` |
|      21 | 12601 | `			}` |
|  348953 | 12602 | `		}` |
| 1049391 | 12603 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 12604 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 12605 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  238145 | 12606 | `			pGen->bStrictTypesLocked = 1;` |
|  119070 | 12607 | `		}` |
| 1049391 | 12608 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 12609 | `			/* Compile block */` |
|    3851 | 12610 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|    3851 | 12611 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 12612 | `				break;` |
|       - | 12613 | `			}` |
|    1928 | 12614 | `		}else{` |
| 1045545 | 12615 | `			xCons = 0;` |
| 1045545 | 12616 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 12617 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 12618 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 12619 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    3895 | 12620 | `				xCons = PH7_CompileClassModifiers;` |
| 1043600 | 12621 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  694047 | 12622 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 12623 | `				/* Try to extract a language construct handler */` |
|  694047 | 12624 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  694047 | 12625 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 12626 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 12627 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 12628 | `						&pGen->pIn->sData);` |
|       9 | 12629 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 12630 | `						break;` |
|       - | 12631 | `					}` |
|       - | 12632 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 12633 | `					 * this erroneous statement.` |
|       - | 12634 | `					 */` |
|       9 | 12635 | `					xCons = PH7_ErrorRecover;` |
|       4 | 12636 | `				}` |
|  694634 | 12637 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   57377 | 12638 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 12639 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 12640 | `				xCons = PH7_CompileLabel;` |
|      56 | 12641 | `			}` |
| 1045545 | 12642 | `			if( xCons == 0 ){` |
|       - | 12643 | `				/* Assume an expression an try to compile it */` |
|  347919 | 12644 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  347919 | 12645 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 12646 | `					/* Pop l-value */` |
|  347769 | 12647 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  173882 | 12648 | `				}` |
|  173962 | 12649 | `			}else{` |
|       - | 12650 | `				/* Go compile the sucker */` |
|  697631 | 12651 | `				rc = xCons(&(*pGen));` |
|       - | 12652 | `			}` |
| 1045545 | 12653 | `			if( rc == SXERR_ABORT ){` |
|       - | 12654 | `				/* Request to abort compilation */` |
|      13 | 12655 | `				break;` |
|       - | 12656 | `			}` |
|       - | 12657 | `		}` |
|       - | 12658 | `		/* Ignore trailing semi-colons ';' */` |
| 1689393 | 12659 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  640017 | 12660 | `			pGen->pIn++;` |
|       5 | 12661 | `		}` |
| 1049381 | 12662 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 12663 | `			/* Compile a single statement and return */` |
|  811209 | 12664 | `			break;` |
|       - | 12665 | `		}` |
|       - | 12666 | `		/* LOOP ONE */` |
|       - | 12667 | `		/* LOOP TWO */` |
|       - | 12668 | `		/* LOOP THREE */` |
|       - | 12669 | `		/* LOOP FOUR */` |
|       5 | 12670 | `	}` |
|       - | 12671 | `	/* Return compilation status */` |
|  829287 | 12672 | `	return rc;` |
|       5 | 12673 | `}` |
|       - | 12674 | `/*` |
|       - | 12675 | ` * Compile a Raw PHP chunk.` |
|       - | 12676 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 12677 | ` * takes care of generating the appropriate error message.` |
|       - | 12678 | ` */` |
|   18080 | 12679 | `static sxi32 PH7_CompilePHP(` |
|       - | 12680 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 12681 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 12682 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 12683 | `	)` |
|       5 | 12684 | `{` |
|   18085 | 12685 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 12686 | `	sxi32 rc;` |
|       - | 12687 | `	/* Reset the token set */` |
|   18085 | 12688 | `	SySetReset(&(*pTokenSet));` |
|       - | 12689 | `	/* Mark as the default token set */` |
|   18085 | 12690 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 12691 | `	/* Advance the stream cursor */` |
|   18085 | 12692 | `	pGen->pRawIn++;` |
|       - | 12693 | `	/* Tokenize the PHP chunk first */` |
|   18085 | 12694 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 12695 | `	/* Point to the head and tail of the token stream. */` |
|   18085 | 12696 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   18085 | 12697 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   18085 | 12698 | `	if( is_expr ){` |
|     ! 0 | 12699 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 12700 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 12701 | `			/* A simple expression,compile it */` |
|     ! 0 | 12702 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 12703 | `		}` |
|       - | 12704 | `		/* Emit the DONE instruction */` |
|     ! 0 | 12705 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 12706 | `		return SXRET_OK;` |
|       - | 12707 | `	}` |
|   18085 | 12708 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 12709 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 12710 | `		/*` |
|       - | 12711 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 12712 | `		 * According to the PHP reference manual:` |
|       - | 12713 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 12714 | `		 *  immediately follow` |
|       - | 12715 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 12716 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 12717 | `		 * Symisc extension:` |
|       - | 12718 | `		 *   This short syntax works with all PHP opening` |
|       - | 12719 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 12720 | `		 *   only short tag.` |
|       - | 12721 | `		 */` |
|       - | 12722 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 12723 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 12724 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 12725 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 12726 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 12727 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 12728 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 12729 | `		}` |
|       3 | 12730 | `		return SXRET_OK;` |
|       - | 12731 | `	}` |
|       - | 12732 | `	/* Compile the PHP chunk */` |
|   18083 | 12733 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 12734 | `	/* Fix exceptions jumps */` |
|   18083 | 12735 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 12736 | `	/* Fix gotos now, the jump destination is resolved */` |
|   18083 | 12737 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 12738 | `		rc = SXERR_ABORT;` |
|       1 | 12739 | `	}` |
|       - | 12740 | `	/* Reset container */` |
|   18083 | 12741 | `	SySetReset(&pGen->aGoto);` |
|   18083 | 12742 | `	SySetReset(&pGen->aLabel);` |
|   18083 | 12743 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 12744 | `	/* Compilation result */` |
|   18083 | 12745 | `	return rc;` |
|    9045 | 12746 | `}` |
|       - | 12747 | `/*` |
|       - | 12748 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 12749 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 12750 | ` * This is the only compile interface exported from this file.` |
|       - | 12751 | ` */` |
|   21002 | 12752 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 12753 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 12754 | `	SyString *pScript,  /* Script to compile */` |
|       - | 12755 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 12756 | `	)` |
|       5 | 12757 | `{` |
|       - | 12758 | `	SySet aPhpToken,aRawToken;` |
|       - | 12759 | `	ph7_gen_state *pCodeGen;` |
|       - | 12760 | `	ph7_value *pRawObj;` |
|       - | 12761 | `	sxu32 nObjIdx;` |
|       - | 12762 | `	sxi32 nRawObj;` |
|       - | 12763 | `	int is_expr;` |
|       - | 12764 | `	sxi8 bSavedStrict;` |
|       - | 12765 | `	sxi8 bSavedStrictLocked;` |
|       - | 12766 | `	sxi32 rc;` |
|   21007 | 12767 | `	if( pScript->nByte < 1 ){` |
|       - | 12768 | `		/* Nothing to compile */` |
|     ! 0 | 12769 | `		return PH7_OK;` |
|       - | 12770 | `	}` |
|       - | 12771 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 12772 | `	 * file's flags so include/require restore them on return. */` |
|   21007 | 12773 | `	pCodeGen = &pVm->sCodeGen;` |
|   21007 | 12774 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   21007 | 12775 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   21007 | 12776 | `	pCodeGen->bStrictTypes = 0;` |
|   21007 | 12777 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 12778 | `	/* Initialize the tokens containers */` |
|   21007 | 12779 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   21007 | 12780 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   21007 | 12781 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   21007 | 12782 | `	is_expr = 0;` |
|   21007 | 12783 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 12784 | `		SyToken sTmp;` |
|       - | 12785 | `		/* PHP only: -*/` |
|    7759 | 12786 | `		sTmp.nLine = 1;` |
|    7759 | 12787 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    7759 | 12788 | `		sTmp.pUserData = 0;` |
|    7759 | 12789 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    7759 | 12790 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    7759 | 12791 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 12792 | `			/* A simple PHP expression */` |
|     ! 0 | 12793 | `			is_expr = 1;` |
|     ! 0 | 12794 | `		}` |
|    3882 | 12795 | `	}else{` |
|       - | 12796 | `		/* Tokenize raw text */` |
|   13253 | 12797 | `		SySetAlloc(&aRawToken,32);` |
|   13253 | 12798 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 12799 | `	}` |
|       - | 12800 | `	/* Process high-level tokens */` |
|   21007 | 12801 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   21007 | 12802 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   21007 | 12803 | `	rc = PH7_OK;` |
|   21007 | 12804 | `	if( is_expr ){` |
|       - | 12805 | `		/* Compile the expression */` |
|     ! 0 | 12806 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 12807 | `		goto cleanup;` |
|       - | 12808 | `	}` |
|   21007 | 12809 | `	nObjIdx = 0;` |
|       - | 12810 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 12811 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 12812 | `	 * preventing namespace bleeding across include()d files. */` |
|   21007 | 12813 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 12814 | `	/* Start the compilation process */` |
|   17131 | 12815 | `	for(;;){` |
|   52335 | 12816 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   20995 | 12817 | `			break; /* No more tokens to process */` |
|       - | 12818 | `		}` |
|   31345 | 12819 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 12820 | `			/* Compile the PHP chunk */` |
|   18085 | 12821 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   18085 | 12822 | `			if( rc == SXERR_ABORT ){` |
|      15 | 12823 | `				break;` |
|       - | 12824 | `			}` |
|   18073 | 12825 | `			continue;` |
|       - | 12826 | `		}` |
|       - | 12827 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13265 | 12828 | `		nRawObj = 0;` |
|   26567 | 12829 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 12830 | `			/* Consume the raw chunk without any processing */` |
|   13307 | 12831 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13307 | 12832 | `			if( pRawObj == 0 ){` |
|     ! 0 | 12833 | `				rc = SXERR_MEM;` |
|     ! 0 | 12834 | `				break;` |
|       - | 12835 | `			}` |
|       - | 12836 | `			/* Mark as constant and emit the load constant instruction */` |
|   13307 | 12837 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13307 | 12838 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13307 | 12839 | `			++nRawObj;` |
|   13307 | 12840 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 12841 | `		}` |
|   13265 | 12842 | `		if( nRawObj > 0 ){` |
|       - | 12843 | `			/* Emit the consume instruction */` |
|   13265 | 12844 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6630 | 12845 | `		}` |
|   10506 | 12846 | `	}` |
|   10501 | 12847 | `cleanup:` |
|   21007 | 12848 | `	SySetRelease(&aRawToken);` |
|   21007 | 12849 | `	SySetRelease(&aPhpToken);` |
|       - | 12850 | `	/* Restore outer file's strict_types scope */` |
|   21007 | 12851 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   21007 | 12852 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   21007 | 12853 | `	return rc;` |
|   10506 | 12854 | `}` |
|       - | 12855 | `/*` |
|       - | 12856 | ` * Utility routines.Initialize the code generator.` |
|       - | 12857 | ` */` |
|    3828 | 12858 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 12859 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12860 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12861 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12862 | `	)` |
|       5 | 12863 | `{` |
|    3833 | 12864 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12865 | `	/* Zero the structure */` |
|    3833 | 12866 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 12867 | `	/* Initial state */` |
|    3833 | 12868 | `	pGen->pVm  = &(*pVm);` |
|    3833 | 12869 | `	pGen->xErr = xErr;` |
|    3833 | 12870 | `	pGen->pErrData = pErrData;` |
|    3833 | 12871 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3833 | 12872 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3833 | 12873 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3833 | 12874 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3833 | 12875 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 12876 | `	/* Error log buffer */` |
|    3833 | 12877 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 12878 | `	/* General purpose working buffer */` |
|    3833 | 12879 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 12880 | `	/* Namespace state */` |
|    3833 | 12881 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3833 | 12882 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3833 | 12883 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3833 | 12884 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12885 | `	/* Create the global scope */` |
|    3833 | 12886 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 12887 | `	/* Point to the global scope */` |
|    3833 | 12888 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3833 | 12889 | `	return SXRET_OK;` |
|       5 | 12890 | `}` |
|       - | 12891 | `/*` |
|       - | 12892 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 12893 | ` */` |
|   24458 | 12894 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 12895 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12896 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12897 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12898 | `	)` |
|       5 | 12899 | `{` |
|   24463 | 12900 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12901 | `	GenBlock *pBlock,*pParent;` |
|       - | 12902 | `	/* Reset state */` |
|   24463 | 12903 | `	SySetReset(&pGen->aLabel);` |
|   24463 | 12904 | `	SySetReset(&pGen->aGoto);` |
|   24463 | 12905 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   24463 | 12906 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   24463 | 12907 | `	SyBlobRelease(&pGen->sWorker);` |
|   24463 | 12908 | `	SyBlobRelease(&pGen->sNamespace);` |
|   24463 | 12909 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   24463 | 12910 | `	SyHashRelease(&pGen->hUseImports);` |
|   24463 | 12911 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   24463 | 12912 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   24463 | 12913 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   24463 | 12914 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   24463 | 12915 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12916 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 12917 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 12918 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 12919 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 12920 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 12921 | `	 * number of unique names, which is acceptable. */` |
|       - | 12922 | `	/* Point to the global scope */` |
|   24463 | 12923 | `	pBlock = pGen->pCurrent;` |
|   24463 | 12924 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 12925 | `		pParent = pBlock->pParent;` |
|     ! 0 | 12926 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 12927 | `		pBlock = pParent;` |
|     ! 0 | 12928 | `	}` |
|   24463 | 12929 | `	pGen->xErr = xErr;` |
|   24463 | 12930 | `	pGen->pErrData = pErrData;` |
|   24463 | 12931 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   24463 | 12932 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   24463 | 12933 | `	pGen->pIn = pGen->pEnd = 0;` |
|   24463 | 12934 | `	pGen->nErr = 0;` |
|   24463 | 12935 | `	return SXRET_OK;` |
|       5 | 12936 | `}` |
|       - | 12937 | `/*` |
|       - | 12938 | ` * Generate a compile-time error message.` |
|       - | 12939 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 12940 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 12941 | ` * abort compilation immediately.` |
|       - | 12942 | ` */` |
|     642 | 12943 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 12944 | `{` |
|     647 | 12945 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     647 | 12946 | `	const char *zErr = "Error";` |
|       - | 12947 | `	SyString *pFile;` |
|       - | 12948 | `	va_list ap;` |
|       - | 12949 | `	sxi32 rc;` |
|       - | 12950 | `	/* Reset the working buffer */` |
|     647 | 12951 | `	SyBlobReset(pWorker);` |
|       - | 12952 | `	/* Peek the processed file path if available */` |
|     647 | 12953 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     647 | 12954 | `	if( nErrType == E_ERROR ){` |
|       - | 12955 | `		/* Increment the error counter */` |
|     533 | 12956 | `		pGen->nErr++;` |
|     533 | 12957 | `		if( pGen->nErr > 15 ){` |
|       - | 12958 | `			/* Error count limit reached */` |
|       6 | 12959 | `			if( pGen->xErr ){` |
|       6 | 12960 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       6 | 12961 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       6 | 12962 | `				if( pFile ){` |
|       6 | 12963 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 12964 | `				}` |
|       6 | 12965 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       6 | 12966 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       6 | 12967 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 12968 | `				}` |
|       2 | 12969 | `			}` |
|       - | 12970 | `			/* Abort immediately */` |
|       6 | 12971 | `			return SXERR_ABORT;` |
|       - | 12972 | `		}` |
|     262 | 12973 | `	}` |
|     643 | 12974 | `	if( pGen->xErr == 0 ){` |
|       - | 12975 | `		/* No available error consumer,return immediately */` |
|       3 | 12976 | `		return SXRET_OK;` |
|       - | 12977 | `	}` |
|     640 | 12978 | `	switch(nErrType){` |
|     526 | 12979 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      32 | 12980 | `	case E_WARNING: zErr = "Warning";     break;` |
|      82 | 12981 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      12 | 12982 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 12983 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 12984 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 12985 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 12986 | `	default:` |
|     ! 0 | 12987 | `		break;` |
|       - | 12988 | `	}` |
|     640 | 12989 | `	rc = SXRET_OK;` |
|       - | 12990 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     640 | 12991 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     640 | 12992 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     640 | 12993 | `	va_start(ap,zFormat);` |
|     640 | 12994 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     640 | 12995 | `	va_end(ap);` |
|     640 | 12996 | `	if( pFile ){` |
|     640 | 12997 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     318 | 12998 | `	}` |
|       - | 12999 | `	/* Append a new line */` |
|     640 | 13000 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     640 | 13001 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 13002 | `		/* Consume the generated error message */` |
|     640 | 13003 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     318 | 13004 | `	}` |
|     640 | 13005 | `	return rc;` |
|     326 | 13006 | `}` |
|       - | 13007 |  |
