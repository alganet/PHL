# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5736/7103 lines (80.75%)

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
|    3864 |   137 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   138 | `{` |
|    3869 |   139 | `	GenBlock *pBlock = pCurrent;` |
|   11009 |   140 | `	for(;;){` |
|   22023 |   141 | `		if( pBlock->iFlags & iBlockType ){` |
|    3761 |   142 | `			iCount--; /* Decrement nesting level */` |
|    3761 |   143 | `			if( iCount < 1 ){` |
|       - |   144 | `				/* Block meet with the desired criteria */` |
|    3735 |   145 | `				return pBlock;` |
|       - |   146 | `			}` |
|      13 |   147 | `		}` |
|       - |   148 | `		/* Point to the upper block */` |
|   18293 |   149 | `		pBlock = pBlock->pParent;` |
|   18293 |   150 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   151 | `			/* Forbidden */` |
|      72 |   152 | `			break;` |
|       - |   153 | `		}` |
|       5 |   154 | `	}` |
|       - |   155 | `	/* No such block */` |
|     139 |   156 | `	return 0;` |
|    1937 |   157 | `}` |
|       - |   158 | `/*` |
|       - |   159 | ` * Initialize a freshly allocated block instance.` |
|       - |   160 | ` */` |
|  847386 |   161 | `static void GenStateInitBlock(` |
|       - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   166 | `	void *pUserData      /* Upper layer private data */` |
|       - |   167 | `	)` |
|       5 |   168 | `{` |
|       - |   169 | `	/* Initialize block fields */` |
|  847391 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  847391 |   171 | `	pBlock->pUserData   = pUserData;` |
|  847391 |   172 | `	pBlock->pGen        = pGen;` |
|  847391 |   173 | `	pBlock->iFlags      = iType;` |
|  847391 |   174 | `	pBlock->pParent     = 0;` |
|  847391 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  847391 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  847391 |   177 | `}` |
|       - |   178 | `/*` |
|       - |   179 | ` * Allocate a new block instance.` |
|       - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   182 | ` * processing on failure.` |
|       - |   183 | ` */` |
|  843800 |   184 | `static sxi32 GenStateEnterBlock(` |
|       - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   190 | `	)` |
|       5 |   191 | `{` |
|       - |   192 | `	GenBlock *pBlock;` |
|       - |   193 | `	/* Allocate a new block instance */` |
|  843805 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  843805 |   195 | `	if( pBlock == 0 ){` |
|       - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   198 | `		 */` |
|     ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   200 | `		/* Abort processing immediately */` |
|     ! 0 |   201 | `		return SXERR_ABORT;` |
|       - |   202 | `	}` |
|       - |   203 | `	/* Zero the structure */` |
|  843805 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  843805 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   206 | `	/* Link to the parent block */` |
|  843805 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   208 | `	/* Mark as the current block */` |
|  843805 |   209 | `	pGen->pCurrent = pBlock;` |
|  843805 |   210 | `	if( ppBlock ){` |
|       - |   211 | `		/* Write a pointer to the new instance */` |
|  409875 |   212 | `		*ppBlock = pBlock;` |
|  204935 |   213 | `	}` |
|  843805 |   214 | `	return SXRET_OK;` |
|  421905 |   215 | `}` |
|       - |   216 | `/*` |
|       - |   217 | ` * Release block fields without freeing the whole instance.` |
|       - |   218 | ` */` |
|  843792 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   220 | `{` |
|  843797 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  843797 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  843797 |   223 | `}` |
|       - |   224 | `/*` |
|       - |   225 | ` * Release a block.` |
|       - |   226 | ` */` |
|  843792 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   228 | `{` |
|  843797 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  843797 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   231 | `	/* Free the instance */` |
|  843797 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  843797 |   233 | `}` |
|       - |   234 | `/*` |
|       - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   236 | ` */` |
|  843792 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   238 | `{` |
|  843797 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  843797 |   240 | `	if( pBlock == 0 ){` |
|       - |   241 | `		/* No more block to pop */` |
|     ! 0 |   242 | `		return SXERR_EMPTY;` |
|       - |   243 | `	}` |
|       - |   244 | `	/* Point to the upper block */` |
|  843797 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  843797 |   246 | `	if( ppBlock ){` |
|       - |   247 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   248 | `		*ppBlock = pBlock;` |
|     ! 0 |   249 | `	}else{` |
|       - |   250 | `		/* Safely release the block */` |
|  843797 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   252 | `	}` |
|  843797 |   253 | `	return SXRET_OK;` |
|  421901 |   254 | `}` |
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
|  243064 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   266 | `{` |
|       - |   267 | `	JumpFixup sJumpFix;` |
|       - |   268 | `	sxi32 rc;` |
|       - |   269 | `	/* Init the JumpFixup structure */` |
|  243069 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  243069 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   272 | `	/* Insert in the jump fixup table */` |
|  243069 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  243069 |   274 | `	return rc;` |
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
|  588976 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   288 | `{` |
|       - |   289 | `	JumpFixup *aFix;` |
|       - |   290 | `	VmInstr *pInstr;` |
|       - |   291 | `	sxu32 nFixed;` |
|       - |   292 | `	sxu32 n;` |
|       - |   293 | `	/* Point to the jump fixup table */` |
|  588981 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   295 | `	/* Fix the desired jumps */` |
| 1064111 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  475135 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   298 | `			/* Already fixed */` |
|  187903 |   299 | `			continue;` |
|       - |   300 | `		}` |
|  287237 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   302 | `			/* Not of our interest */` |
|   44175 |   303 | `			continue;` |
|       - |   304 | `		}` |
|       - |   305 | `		/* Point to the instruction to fix */` |
|  243067 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  243067 |   307 | `		if( pInstr ){` |
|  243067 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  243067 |   309 | `			nFixed++;` |
|       - |   310 | `			/* Mark as fixed */` |
|  243067 |   311 | `			aFix[n].nJumpType = -1;` |
|  121531 |   312 | `		}` |
|  121536 |   313 | `	}` |
|       - |   314 | `	/* Total number of fixed jumps */` |
|  588981 |   315 | `	return nFixed;` |
|       5 |   316 | `}` |
|       - |   317 | `/*` |
|       - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   319 | ` * The goto statement can be used to jump to another section` |
|       - |   320 | ` * in the program.` |
|       - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   322 | ` * statement for more information.` |
|       - |   323 | ` */` |
|  239100 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   325 | `{` |
|       - |   326 | `	JumpFixup *pJump,*aJumps;` |
|       - |   327 | `	Label *pLabel,*aLabel;` |
|       - |   328 | `	VmInstr *pInstr;` |
|       - |   329 | `	sxi32 rc;` |
|       - |   330 | `	sxu32 n;` |
|       - |   331 | `	/* Point to the goto table */` |
|  239105 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   333 | `	/* Fix */` |
|  239251 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  239103 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  239235 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   362 | `			/* Emit a warning */` |
|      40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   365 | `		}` |
|      71 |   366 | `	}` |
|  239103 |   367 | `	return SXRET_OK;` |
|  119555 |   368 | `}` |
|       - |   369 | `/*` |
|       - |   370 | ` * Check if a given token value is installed in the literal table.` |
|       - |   371 | ` */` |
|  771458 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   373 | `{` |
|       - |   374 | `	SyHashEntry *pEntry;` |
|  771463 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  771463 |   376 | `	if( pEntry == 0 ){` |
|  347325 |   377 | `		return SXERR_NOTFOUND;` |
|       - |   378 | `	}` |
|  424143 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  424143 |   380 | `	return SXRET_OK;` |
|  385734 |   381 | `}` |
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
|  347320 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   393 | `{` |
|  347325 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  347325 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  173660 |   396 | `	}` |
|  347325 |   397 | `	return SXRET_OK;` |
|       5 |   398 | `}` |
|       - |   399 | `/*` |
|       - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   401 | ` * in the constant table.` |
|       - |   402 | ` */` |
|  125950 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   404 | `{` |
|       - |   405 | `	ph7_value *pObj;` |
|  125955 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   407 | `	/* Reserve a new constant */` |
|  125955 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  125955 |   409 | `	if( pObj == 0 ){` |
|     ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   411 | `		return 0;` |
|       - |   412 | `	}` |
|  125955 |   413 | `	*pIdx = nIdx;` |
|       - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   416 | `	 */` |
|  125955 |   417 | `	return pObj;` |
|   62980 |   418 | `}` |
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
|  481644 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   434 | `{` |
|       - |   435 | `	VmCallArgMap *pMap;` |
|  481649 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|      33 |   437 | `	if( p3 == 0 ){` |
|      31 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      31 |   439 | `		if( pMap == 0 ) return 0;` |
|      31 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      31 |   441 | `		p3 = (void *)pMap;` |
|      14 |   442 | `	}` |
|      33 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      33 |   444 | `	return p3;` |
|  240827 |   445 | `}` |
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
|  126680 |   504 | `static int GenStateFindBadNumericSeparator(` |
|       - |   505 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   506 | `{` |
|  126685 |   507 | `	const char *z = pRaw->zString;` |
|  126685 |   508 | `	sxu32 n = pRaw->nByte;` |
|  126685 |   509 | `	int base = 10;` |
|       - |   510 | `	sxu32 i, start;` |
|  126685 |   511 | `	if( n < 2 ) return 0;` |
|   10565 |   512 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   513 | `		base = 16;` |
|   10530 |   514 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   515 | `		base = 2;` |
|     139 |   516 | `	}` |
|   38255 |   517 | `	for( i = 0; i < n; ++i ){` |
|   27709 |   518 | `		if( z[i] != '_' ) continue;` |
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
|   10551 |   535 | `	return 0;` |
|   63345 |   536 | `}` |
|       - |   537 | `/*` |
|       - |   538 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   539 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   540 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   541 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   542 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   543 | ` * so callers can bail from the current construct).` |
|       - |   544 | ` */` |
|  126680 |   545 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   546 | `{` |
|  126685 |   547 | `	const char *zBad = 0;` |
|  126685 |   548 | `	sxu32 nBad = 0;` |
|       - |   549 | `	SyString sBad;` |
|       - |   550 | `	sxi32 rc;` |
|  126685 |   551 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  126671 |   552 | `		return SXRET_OK;` |
|       - |   553 | `	}` |
|      18 |   554 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   555 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   556 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   557 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   558 | `		return SXERR_ABORT;` |
|       - |   559 | `	}` |
|      18 |   560 | `	return SXERR_SYNTAX;` |
|   63345 |   561 | `}` |
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
|  126666 |   578 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   579 | `	SyMemBackend *pAlloc,` |
|       - |   580 | `	const SyString *pToken,` |
|       - |   581 | `	char *zScratch, sxu32 nScratch,` |
|       - |   582 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   583 | `{` |
|       - |   584 | `	sxu32 i, j;` |
|  126671 |   585 | `	int hasUnderscore = 0;` |
|       - |   586 | `	char *zBuf;` |
|  126671 |   587 | `	*pzAlloc = 0;` |
|  268415 |   588 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  142001 |   589 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   70877 |   590 | `	}` |
|  126671 |   591 | `	if( !hasUnderscore ){` |
|  126419 |   592 | `		SyStringDupPtr(pOut, pToken);` |
|  126419 |   593 | `		return SXRET_OK;` |
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
|   63338 |   610 | `}` |
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
|  126652 |   627 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   628 | `{` |
|  126657 |   629 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  126657 |   630 | `	sxu32 nIdx = 0;` |
|       - |   631 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  126657 |   632 | `	char *zAlloc = 0;` |
|       - |   633 | `	SyString sNum;` |
|       - |   634 | `	sxi32 rc;` |
|   63326 |   635 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  126657 |   636 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  126657 |   637 | `	if( rc != SXRET_OK ){` |
|      14 |   638 | `		return rc;` |
|       - |   639 | `	}` |
|  189968 |   640 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   63321 |   641 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  126647 |   642 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   643 | `		return SXERR_ABORT;` |
|       - |   644 | `	}` |
|  126647 |   645 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   646 | `		ph7_value *pObj;` |
|       - |   647 | `		sxi64 iValue;` |
|  125955 |   648 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  125955 |   649 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  125955 |   650 | `		if( pObj == 0 ){` |
|     ! 0 |   651 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   652 | `			return SXERR_ABORT;` |
|       - |   653 | `		}` |
|  125955 |   654 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   62980 |   655 | `	}else{` |
|       - |   656 | `		/* Real number */` |
|       - |   657 | `		ph7_value *pObj;` |
|       - |   658 | `		/* Reserve a new constant */` |
|     696 |   659 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     696 |   660 | `		if( pObj == 0 ){` |
|     ! 0 |   661 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   662 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   663 | `			return SXERR_ABORT;` |
|       - |   664 | `		}` |
|     696 |   665 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     696 |   666 | `		PH7_MemObjToReal(pObj);` |
|       - |   667 | `	}` |
|  126647 |   668 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   669 | `	/* Emit the load constant instruction */` |
|  126647 |   670 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   671 | `	/* Node successfully compiled */` |
|  126647 |   672 | `	return SXRET_OK;` |
|   63331 |   673 | `}` |
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
|  101164 |   685 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   686 | `{` |
|  101169 |   687 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   688 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   689 | `	ph7_value *pObj;` |
|       - |   690 | `	sxu32 nIdx;` |
|  101169 |   691 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   692 | `	/* Delimit the string */` |
|  101169 |   693 | `	zIn  = pStr->zString;` |
|  101169 |   694 | `	zEnd = &zIn[pStr->nByte];` |
|  101169 |   695 | `	if( zIn >= zEnd ){` |
|       - |   696 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   697 | `		 * rather than reserving a new object each time. */` |
|    7343 |   698 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    7343 |   699 | `		return SXRET_OK;` |
|       - |   700 | `	}` |
|   93831 |   701 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   702 | `		/* Already processed,emit the load constant instruction` |
|       - |   703 | `		 * and return.` |
|       - |   704 | `		 */` |
|   36297 |   705 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   36297 |   706 | `		return SXRET_OK;` |
|       - |   707 | `	}` |
|       - |   708 | `	/* Reserve a new constant */` |
|   57539 |   709 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   57539 |   710 | `	if( pObj == 0 ){` |
|     ! 0 |   711 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   712 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   713 | `		return SXERR_ABORT;` |
|       - |   714 | `	}` |
|   57539 |   715 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   716 | `	/* Compile the node */` |
|   57591 |   717 | `	for(;;){` |
|  115187 |   718 | `		if( zIn >= zEnd ){` |
|       - |   719 | `			/* End of input */` |
|   57539 |   720 | `			break;` |
|       - |   721 | `		}` |
|   57653 |   722 | `		zCur = zIn;` |
|  984739 |   723 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  927091 |   724 | `			zIn++;` |
|       5 |   725 | `		}` |
|   57653 |   726 | `		if( zIn > zCur ){` |
|       - |   727 | `			/* Append raw contents*/` |
|   57629 |   728 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   28812 |   729 | `		}` |
|   57653 |   730 | `		zIn++;` |
|   57653 |   731 | `		if( zIn < zEnd ){` |
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
|   57653 |   746 | `		zIn++;` |
|       5 |   747 | `	}` |
|       - |   748 | `	/* Emit the load constant instruction */` |
|   57539 |   749 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   57539 |   750 | `	if( pStr->nByte < 1024 ){` |
|       - |   751 | `		/* Install in the literal table */` |
|   57539 |   752 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   28767 |   753 | `	}` |
|       - |   754 | `	/* Node successfully compiled */` |
|   57539 |   755 | `	return SXRET_OK;` |
|   50587 |   756 | `}` |
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
|       3 |   869 | `{` |
|       - |   870 | `	SyString sStripped;` |
|       - |   871 | `	SyString *pStr;` |
|       - |   872 | `	ph7_value *pObj;` |
|       - |   873 | `	sxu32 nIdx;` |
|       - |   874 | `	sxi32 rc;` |
|      49 |   875 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      49 |   876 | `	if( rc != SXRET_OK ){` |
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
|      26 |   899 | `}` |
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
|    2264 |   922 | `static sxi32 GenStateProcessStringExpression(` |
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
|    2269 |   933 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   934 | `	/* Preallocate some slots */` |
|    2269 |   935 | `	SySetAlloc(&sToken,0x08);` |
|       - |   936 | `	/* Tokenize the text */` |
|    2269 |   937 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   938 | `	/* Swap delimiter */` |
|    2269 |   939 | `	pTmpIn  = pGen->pIn;` |
|    2269 |   940 | `	pTmpEnd = pGen->pEnd;` |
|    2269 |   941 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2269 |   942 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   943 | `	/* Compile the expression */` |
|    2269 |   944 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   945 | `	/* Restore token stream */` |
|    2269 |   946 | `	pGen->pIn  = pTmpIn;` |
|    2269 |   947 | `	pGen->pEnd = pTmpEnd;` |
|       - |   948 | `	/* Release the token set */` |
|    2269 |   949 | `	SySetRelease(&sToken);` |
|       - |   950 | `	/* Compilation result */` |
|    2269 |   951 | `	return rc;` |
|       5 |   952 | `}` |
|       - |   953 | `/*` |
|       - |   954 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   955 | ` */` |
|   25388 |   956 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   957 | `{` |
|       - |   958 | `	ph7_value *pConstObj;` |
|   25393 |   959 | `	sxu32 nIdx = 0;` |
|       - |   960 | `	/* Reserve a new constant */` |
|   25393 |   961 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   25393 |   962 | `	if( pConstObj == 0 ){` |
|     ! 0 |   963 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   964 | `		return 0;` |
|       - |   965 | `	}` |
|   25393 |   966 | `	(*pCount)++;` |
|   25393 |   967 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   968 | `	/* Emit the load constant instruction */` |
|   25393 |   969 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   25393 |   970 | `	return pConstObj;` |
|   12699 |   971 | `}` |
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
|   23908 |  1010 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       5 |  1011 | `{` |
|   23913 |  1012 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1013 | `	const char *zIn,*zCur,*zEnd;` |
|   23913 |  1014 | `	ph7_value *pObj = 0;` |
|       - |  1015 | `	sxi32 iCons;` |
|       - |  1016 | `	sxi32 rc;` |
|       - |  1017 | `	/* Delimit the string */` |
|   23913 |  1018 | `	zIn  = pStr->zString;` |
|   23913 |  1019 | `	zEnd = &zIn[pStr->nByte];` |
|   23913 |  1020 | `	if( zIn >= zEnd ){` |
|       - |  1021 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1022 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1023 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1024 | `		 */` |
|     319 |  1025 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     319 |  1026 | `		return SXRET_OK;` |
|       - |  1027 | `	}` |
|   23599 |  1028 | `	zCur = 0;` |
|       - |  1029 | `	/* Compile the node */` |
|   23599 |  1030 | `	iCons = 0;` |
|   12929 |  1031 | `	for(;;){` |
|   38599 |  1032 | `		zCur = zIn;` |
|  180627 |  1033 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  144297 |  1034 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      66 |  1035 | `				break;` |
|  144173 |  1036 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2144 |  1037 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1073 |  1038 | `					break;` |
|       - |  1039 | `			}` |
|  142033 |  1040 | `			zIn++;` |
|       5 |  1041 | `		}` |
|   38599 |  1042 | `		if( zIn > zCur ){` |
|   18007 |  1043 | `			if( pObj == 0 ){` |
|   17519 |  1044 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   17519 |  1045 | `				if( pObj == 0 ){` |
|     ! 0 |  1046 | `					return SXERR_ABORT;` |
|       - |  1047 | `				}` |
|    8757 |  1048 | `			}` |
|   18007 |  1049 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    9001 |  1050 | `		}` |
|   38599 |  1051 | `		if( zIn >= zEnd ){` |
|   23599 |  1052 | `			break;` |
|       - |  1053 | `		}` |
|   15005 |  1054 | `		if( zIn[0] == '\\' ){` |
|   12741 |  1055 | `			const char *zPtr = 0;` |
|       - |  1056 | `			sxu32 n;` |
|   12741 |  1057 | `			zIn++;` |
|   12741 |  1058 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1059 | `				break;` |
|       - |  1060 | `			}` |
|   12741 |  1061 | `			if( pObj == 0 ){` |
|    7879 |  1062 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    7879 |  1063 | `				if( pObj == 0 ){` |
|     ! 0 |  1064 | `					return SXERR_ABORT;` |
|       - |  1065 | `				}` |
|    3937 |  1066 | `			}` |
|   12741 |  1067 | `			n = sizeof(char); /* size of conversion */` |
|   12741 |  1068 | `			switch( zIn[0] ){` |
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
|    5878 |  1089 | `			case 'n':` |
|       - |  1090 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   11761 |  1091 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   11761 |  1092 | `				break;` |
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
|   12741 |  1160 | `			zIn += n;` |
|   12741 |  1161 | `			continue;` |
|       - |  1162 | `		}` |
|    2269 |  1163 | `		if( zIn[0] == '{' ){` |
|       - |  1164 | `			/* Curly syntax */` |
|       - |  1165 | `			const char *zExpr;` |
|     130 |  1166 | `			sxi32 iNest = 1;` |
|     130 |  1167 | `			zIn++;` |
|     130 |  1168 | `			zExpr = zIn;` |
|       - |  1169 | `			/* Synchronize with the next closing curly braces */` |
|    1358 |  1170 | `			while( zIn < zEnd ){` |
|    1358 |  1171 | `				if( zIn[0] == '{' ){` |
|       - |  1172 | `					/* Increment nesting level */` |
|       9 |  1173 | `					iNest++;` |
|    1354 |  1174 | `				}else if(zIn[0] == '}' ){` |
|       - |  1175 | `					/* Decrement nesting level */` |
|     138 |  1176 | `					iNest--;` |
|     138 |  1177 | `					if( iNest <= 0 ){` |
|     130 |  1178 | `						break;` |
|       - |  1179 | `					}` |
|       4 |  1180 | `				}` |
|    1230 |  1181 | `				zIn++;` |
|       2 |  1182 | `			}` |
|       - |  1183 | `			/* Process the expression */` |
|     130 |  1184 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     130 |  1185 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1186 | `				return SXERR_ABORT;` |
|       - |  1187 | `			}` |
|     130 |  1188 | `			if( rc != SXERR_EMPTY ){` |
|     130 |  1189 | `				++iCons;` |
|      64 |  1190 | `			}` |
|     130 |  1191 | `			if( zIn < zEnd ){` |
|       - |  1192 | `				/* Jump the trailing curly */` |
|     130 |  1193 | `				zIn++;` |
|      64 |  1194 | `			}` |
|      66 |  1195 | `		}else{` |
|       - |  1196 | `			/* Simple syntax */` |
|    2141 |  1197 | `			const char *zExpr = zIn;` |
|       - |  1198 | `			/* Assemble variable name */` |
|    1078 |  1199 | `			for(;;){` |
|       - |  1200 | `				/* Jump leading dollars */` |
|    4297 |  1201 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2141 |  1202 | `					zIn++;` |
|       5 |  1203 | `				}` |
|    1078 |  1204 | `				for(;;){` |
|   11921 |  1205 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8687 |  1206 | `						zIn++;` |
|       5 |  1207 | `					}` |
|    2161 |  1208 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1209 | `						/* UTF-8 stream */` |
|     ! 0 |  1210 | `						zIn++;` |
|     ! 0 |  1211 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1212 | `							zIn++;` |
|     ! 0 |  1213 | `						}` |
|     ! 0 |  1214 | `						continue;` |
|       - |  1215 | `					}` |
|    2161 |  1216 | `					break;` |
|     ! 0 |  1217 | `				}` |
|    2161 |  1218 | `				if( zIn >= zEnd ){` |
|     210 |  1219 | `					break;` |
|       - |  1220 | `				}` |
|    1955 |  1221 | `				if( zIn[0] == '[' ){` |
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
|    1945 |  1239 | `				}else if(zIn[0] == '{' ){` |
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
|    1941 |  1257 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1258 | `					/* Member access operator '->' */` |
|      23 |  1259 | `					zIn += 2;` |
|    1931 |  1260 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1261 | `					/* Static member access operator '::' */` |
|     ! 0 |  1262 | `					zIn += 2;` |
|     ! 0 |  1263 | `				}else{` |
|     963 |  1264 | `					break;` |
|       - |  1265 | `				}` |
|       3 |  1266 | `			}` |
|       - |  1267 | `			/* Process the expression */` |
|    2141 |  1268 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2141 |  1269 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1270 | `				return SXERR_ABORT;` |
|       - |  1271 | `			}` |
|    2141 |  1272 | `			if( rc != SXERR_EMPTY ){` |
|    2139 |  1273 | `				++iCons;` |
|    1067 |  1274 | `			}` |
|       - |  1275 | `		}` |
|       - |  1276 | `		/* Invalidate the previously used constant */` |
|    2269 |  1277 | `		pObj = 0;` |
|       5 |  1278 | `	}/*for(;;)*/` |
|   23599 |  1279 | `	if( iCons > 1 ){` |
|       - |  1280 | `		/* Concatenate all compiled constants */` |
|    1681 |  1281 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     838 |  1282 | `	}` |
|       - |  1283 | `	/* Node successfully compiled */` |
|   23599 |  1284 | `	return SXRET_OK;` |
|   11959 |  1285 | `}` |
|       - |  1286 | `/*` |
|       - |  1287 | ` * Compile a double quoted string.` |
|       - |  1288 | ` *  See the block-comment above for more information.` |
|       - |  1289 | ` */` |
|   23848 |  1290 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1291 | `{` |
|       - |  1292 | `	sxi32 rc;` |
|   23853 |  1293 | `	rc = GenStateCompileString(&(*pGen));` |
|   11924 |  1294 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1295 | `	/* Compilation result */` |
|   23853 |  1296 | `	return rc;` |
|       5 |  1297 | `}` |
|       - |  1298 | `/*` |
|       - |  1299 | ` * Compile a Heredoc string.` |
|       - |  1300 | ` *  See the block-comment above for more information.` |
|       - |  1301 | ` */` |
|      64 |  1302 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1303 | `{` |
|       - |  1304 | `	SyString sOrig, sStripped;` |
|       - |  1305 | `	sxi32 rc;` |
|      69 |  1306 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      69 |  1307 | `	if( rc != SXRET_OK ){` |
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
|      37 |  1320 | `}` |
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
|   22156 |  1340 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   22161 |  1351 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1352 | `	/* Compile the expression*/` |
|   22161 |  1353 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1354 | `	/* Restore token stream */` |
|   22161 |  1355 | `	RE_SWAP_DELIMITER(pGen);` |
|   22161 |  1356 | `	return rc;` |
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
|      14 |  1370 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
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
|   24518 |  1397 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 |  1398 | `{` |
|   24523 |  1399 | `	SyToken *pCur = pStart;` |
|   24523 |  1400 | `	sxi32 iNest = 0;` |
|   69501 |  1401 | `	while( pCur < pEnd ){` |
|   50509 |  1402 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    5527 |  1403 | `			return pCur;` |
|       - |  1404 | `		}` |
|       - |  1405 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1406 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - |  1407 | `		 * not an entry separator. Skip past the signature.` |
|       - |  1408 | `		 */` |
|   44987 |  1409 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|   44981 |  1470 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     387 |  1471 | `			iNest++;` |
|   44790 |  1472 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1473 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - |  1474 | `			 * parser will shortly detect any syntax error. */` |
|     387 |  1475 | `			iNest--;` |
|     191 |  1476 | `		}` |
|   44981 |  1477 | `		pCur++;` |
|       5 |  1478 | `	}` |
|   18997 |  1479 | `	return pEnd;` |
|   12264 |  1480 | `}` |
|       - |  1481 | `/*` |
|       - |  1482 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1483 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1484 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1485 | ` */` |
|   31602 |  1486 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1487 | `{` |
|       - |  1488 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1489 | `	SyToken *pKey,*pCur;` |
|   31607 |  1490 | `	sxi32 iEmitRef = 0;` |
|   31607 |  1491 | `	sxi32 iSpread = 0;` |
|   31607 |  1492 | `	sxi32 nPair = 0;` |
|       - |  1493 | `	sxi32 rc;` |
|   31607 |  1494 | `	xValidator = 0;` |
|   25949 |  1495 | `	for(;;){` |
|       - |  1496 | `		/* Jump leading commas */` |
|   58979 |  1497 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    7081 |  1498 | `			pGen->pIn++;` |
|       5 |  1499 | `		}` |
|   51903 |  1500 | `		pCur = pGen->pIn;` |
|   51903 |  1501 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1502 | `			/* No more entry to process */` |
|   31591 |  1503 | `			break;` |
|       - |  1504 | `		}` |
|   20317 |  1505 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1506 | `			continue;` |
|       - |  1507 | `		}` |
|       - |  1508 | `		/* Compile the key if available */` |
|   20317 |  1509 | `		pKey = pCur;` |
|   20317 |  1510 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   20317 |  1511 | `		rc = SXERR_EMPTY;` |
|   20317 |  1512 | `		if( pCur < pGen->pIn ){` |
|    1661 |  1513 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1514 | `				/* Missing value */` |
|      12 |  1515 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      12 |  1516 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1517 | `					return SXERR_ABORT;` |
|       - |  1518 | `				}` |
|      12 |  1519 | `				return SXRET_OK;` |
|       - |  1520 | `			}` |
|       - |  1521 | `			/* Compile the expression holding the key */` |
|    1651 |  1522 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1523 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1651 |  1524 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1525 | `				return SXERR_ABORT;` |
|       - |  1526 | `			}` |
|    1651 |  1527 | `			pCur++; /* Jump the '=>' operator */` |
|   19484 |  1528 | `		}else if( pKey == pCur ){` |
|       - |  1529 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1530 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1531 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1532 | `		}else{` |
|       - |  1533 | `			/* Reset back the cursor and point to the entry value */` |
|   18661 |  1534 | `			pCur = pKey;` |
|       - |  1535 | `		}` |
|   20307 |  1536 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1537 | `			/* No available key,load NULL */` |
|   18663 |  1538 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    9329 |  1539 | `		}` |
|   20307 |  1540 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1541 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      45 |  1542 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      45 |  1543 | `			iEmitRef = 1;` |
|      45 |  1544 | `			pCur++; /* Jump the '&' token */` |
|      45 |  1545 | `			if( pCur >= pGen->pIn ){` |
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
|   20305 |  1559 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   20305 |  1560 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   20301 |  1573 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   20301 |  1574 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1575 | `			return SXERR_ABORT;` |
|       - |  1576 | `		}` |
|   20301 |  1577 | `		if( iSpread ){` |
|       - |  1578 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      65 |  1579 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   20270 |  1580 | `		}else if( iEmitRef ){` |
|       - |  1581 | `			/* Emit the load reference instruction */` |
|      40 |  1582 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1583 | `		}` |
|   20301 |  1584 | `		xValidator = 0;` |
|   20301 |  1585 | `		iEmitRef = 0;` |
|   20301 |  1586 | `		iSpread = 0;` |
|   20301 |  1587 | `		nPair++;` |
|       5 |  1588 | `	}` |
|       - |  1589 | `	/* Emit the load map instruction */` |
|   31591 |  1590 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1591 | `	/* Node successfully compiled */` |
|   31591 |  1592 | `	return SXRET_OK;` |
|   15806 |  1593 | `}` |
|       - |  1594 | `/*` |
|       - |  1595 | ` * Compile the 'array' language construct.` |
|       - |  1596 | ` *	 According to the PHP language reference manual` |
|       - |  1597 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1598 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1599 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1600 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1601 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1602 | ` */` |
|   30496 |  1603 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1604 | `{` |
|       - |  1605 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   30501 |  1606 | `	pGen->pIn += 2;` |
|   30501 |  1607 | `	pGen->pEnd--;` |
|   15248 |  1608 | `	SXUNUSED(iCompileFlag);` |
|   30501 |  1609 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1610 | `}` |
|       - |  1611 | `/*` |
|       - |  1612 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1613 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1614 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1615 | ` */` |
|    1106 |  1616 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1617 | `{` |
|       - |  1618 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    1111 |  1619 | `	pGen->pIn++;` |
|    1111 |  1620 | `	pGen->pEnd--;` |
|     553 |  1621 | `	SXUNUSED(iCompileFlag);` |
|    1111 |  1622 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1623 | `}` |
|       - |  1624 | `/*` |
|       - |  1625 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1626 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1627 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1628 | ` * error message.` |
|       - |  1629 | ` * See the routine responible of compiling the list language construct` |
|       - |  1630 | ` * for more inforation.` |
|       - |  1631 | ` */` |
|     172 |  1632 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       4 |  1633 | `{` |
|     176 |  1634 | `	sxi32 rc = SXRET_OK;` |
|     176 |  1635 | `	if( pRoot->pOp ){` |
|       4 |  1636 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|       2 |  1637 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1638 | `				/* Unexpected expression */` |
|     ! 0 |  1639 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1640 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1641 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1642 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1643 | `				}` |
|       1 |  1644 | `		}` |
|     174 |  1645 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1646 | `		/* Unexpected expression */` |
|       6 |  1647 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1648 | `			"list(): Expecting a variable not an expression");` |
|       6 |  1649 | `		if( rc != SXERR_ABORT ){` |
|       6 |  1650 | `			rc = SXERR_INVALID;` |
|       2 |  1651 | `		}` |
|       2 |  1652 | `	}` |
|     176 |  1653 | `	return rc;` |
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
|     108 |  1776 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       4 |  1777 | `{` |
|       - |  1778 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1779 | `	SyToken *pNext;` |
|       - |  1780 | `	SyToken *pClassifyIn;` |
|     112 |  1781 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|       - |  1782 | `	sxi32 nExpr;` |
|       - |  1783 | `	sxi32 rc;` |
|       - |  1784 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|       - |  1785 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|       - |  1786 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|       - |  1787 | `	 * list. */` |
|     112 |  1788 | `	pClassifyIn = pGen->pIn;` |
|     314 |  1789 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     206 |  1790 | `		if( pGen->pIn >= pNext ){` |
|      13 |  1791 | `			nEmpty++;` |
|     200 |  1792 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|      38 |  1793 | `			nKeyed++;` |
|      20 |  1794 | `		}else{` |
|     158 |  1795 | `			nPositional++;` |
|       - |  1796 | `		}` |
|     206 |  1797 | `		pGen->pIn = &pNext[1];` |
|       4 |  1798 | `	}` |
|     112 |  1799 | `	pGen->pIn = pClassifyIn;` |
|     112 |  1800 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|     ! 0 |  1801 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1802 | `			"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1803 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1804 | `	}` |
|     112 |  1805 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|     ! 0 |  1806 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1807 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|     ! 0 |  1808 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1809 | `	}` |
|     112 |  1810 | `	if( nKeyed > 0 ){` |
|      30 |  1811 | `		return GenStateCompileKeyedListBody(pGen);` |
|       - |  1812 | `	}` |
|      84 |  1813 | `	nExpr = 0;` |
|      84 |  1814 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     250 |  1815 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     170 |  1816 | `		if( pGen->pIn < pNext ){` |
|       - |  1817 | `			/* Check for nested list() */` |
|     158 |  1818 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|     157 |  1835 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
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
|     144 |  1851 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     144 |  1852 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  1853 | `					SySetRelease(&sNested);` |
|     ! 0 |  1854 | `					return SXRET_OK;` |
|       - |  1855 | `				}` |
|       - |  1856 | `			}` |
|      81 |  1857 | `		}else{` |
|       - |  1858 | `			/* Empty entry,load NULL */` |
|      13 |  1859 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  1860 | `		}` |
|     170 |  1861 | `		nExpr++;` |
|       - |  1862 | `		/* Advance the stream cursor */` |
|     170 |  1863 | `		pGen->pIn = &pNext[1];` |
|       4 |  1864 | `	}` |
|       - |  1865 | `	/* Emit the LOAD_LIST instruction */` |
|      84 |  1866 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  1867 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  1868 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  1869 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  1870 | `	 */` |
|      84 |  1871 | `	if( SySetUsed(&sNested) > 0 ){` |
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
|      84 |  1913 | `	SySetRelease(&sNested);` |
|       - |  1914 | `	/* Node successfully compiled */` |
|      84 |  1915 | `	return SXRET_OK;` |
|      58 |  1916 | `}` |
|      34 |  1917 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1918 | `{` |
|       - |  1919 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      36 |  1920 | `	pGen->pIn += 2;` |
|      36 |  1921 | `	pGen->pEnd--;` |
|      17 |  1922 | `	SXUNUSED(iCompileFlag);` |
|      36 |  1923 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1924 | `}` |
|      74 |  1925 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  1926 | `{` |
|       - |  1927 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      78 |  1928 | `	pGen->pIn++;` |
|      78 |  1929 | `	pGen->pEnd--;` |
|      37 |  1930 | `	SXUNUSED(iCompileFlag);` |
|      78 |  1931 | `	return GenStateCompileListBody(pGen);` |
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
|     190 |  2112 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  2113 | `	ph7_gen_state *pGen,` |
|       - |  2114 | `	ph7_vm_func *pFunc,` |
|       - |  2115 | `	SyToken *pStart,` |
|       - |  2116 | `	SyToken *pEnd,` |
|       - |  2117 | `	SyString *aShadow,` |
|       - |  2118 | `	sxu32 nShadow)` |
|       2 |  2119 | `{` |
|     192 |  2120 | `	SyToken *pScan = pStart;` |
|       - |  2121 | `	sxi32 rc;` |
|     794 |  2122 | `	while( pScan < pEnd ){` |
|     604 |  2123 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
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
|     568 |  2134 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
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
|     550 |  2286 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     386 |  2287 | `			pScan++;` |
|     386 |  2288 | `			continue;` |
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
|     192 |  2313 | `	return SXRET_OK;` |
|      97 |  2314 | `}` |
|       - |  2315 | `/*` |
|       - |  2316 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2317 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2318 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2319 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2320 | ` * $this is also made available.` |
|       - |  2321 | ` */` |
|     172 |  2322 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2323 | `{` |
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
|     176 |  2338 | `	sxi32 iFlags = 0;` |
|     176 |  2339 | `	int bStatic = 0;` |
|       - |  2340 | `	sxi32 rc;` |
|       - |  2341 | `	sxu32 n;` |
|      86 |  2342 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2343 |  |
|     176 |  2344 | `	nLine = pGen->pIn->nLine;` |
|       - |  2345 | `	/* Optional 'static' prefix */` |
|     172 |  2346 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     176 |  2347 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2348 | `		bStatic = 1;` |
|       3 |  2349 | `		pGen->pIn++;` |
|       1 |  2350 | `	}` |
|       - |  2351 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     172 |  2352 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     176 |  2353 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2354 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2355 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2356 | `		return SXERR_SYNTAX;` |
|       - |  2357 | `	}` |
|     176 |  2358 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2359 | `	/* Optional '&' — return by reference */` |
|     176 |  2360 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2361 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2362 | `		pGen->pIn++;` |
|     ! 0 |  2363 | `	}` |
|       - |  2364 | `	/* Expect '(' */` |
|     176 |  2365 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
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
|     174 |  2376 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2377 | `	/* Delimit the parameter list */` |
|     174 |  2378 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     174 |  2379 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2380 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2381 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2382 | `		return SXERR_SYNTAX;` |
|       - |  2383 | `	}` |
|       - |  2384 | `	/* Allocate the function state */` |
|     171 |  2385 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     171 |  2386 | `	if( pFunc == 0 ){` |
|     ! 0 |  2387 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2388 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2389 | `		return SXERR_ABORT;` |
|       - |  2390 | `	}` |
|       - |  2391 | `	/* Generate a unique lambda name */` |
|     171 |  2392 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     265 |  2393 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      96 |  2394 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2395 | `	}` |
|     171 |  2396 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     171 |  2397 | `	if( zDup == 0 ){` |
|     ! 0 |  2398 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2399 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2400 | `		return SXERR_ABORT;` |
|       - |  2401 | `	}` |
|     171 |  2402 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2403 | `	/* Collect function arguments */` |
|     171 |  2404 | `	if( pGen->pIn < pSigEnd ){` |
|     103 |  2405 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|     103 |  2406 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2407 | `			return SXERR_ABORT;` |
|       - |  2408 | `		}` |
|      50 |  2409 | `	}` |
|       - |  2410 | `	/* Point past ')' and parse optional return type */` |
|     171 |  2411 | `	pGen->pIn = &pSigEnd[1];` |
|     171 |  2412 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     171 |  2413 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2414 | `		return SXERR_ABORT;` |
|     171 |  2415 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2416 | `		return SXERR_SYNTAX;` |
|       - |  2417 | `	}` |
|       - |  2418 | `	/* Expect '=>' */` |
|     171 |  2419 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
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
|     168 |  2430 | `	pGen->pIn++; /* Jump '=>' */` |
|     168 |  2431 | `	pBodyStart = pGen->pIn;` |
|     168 |  2432 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2433 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2434 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2435 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2436 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     168 |  2437 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2438 | `	{` |
|     168 |  2439 | `		SyString *aShadow = 0;` |
|     168 |  2440 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     168 |  2441 | `		if( nShadow > 0 ){` |
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
|     251 |  2453 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      83 |  2454 | `			aShadow,nShadow);` |
|     168 |  2455 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2456 | `			return SXERR_ABORT;` |
|       - |  2457 | `		}` |
|       - |  2458 | `	}` |
|       - |  2459 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2460 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2461 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2462 | `	 * $this. */` |
|     168 |  2463 | `	if( !bStatic ){` |
|       - |  2464 | `		char *zThisDup;` |
|     166 |  2465 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     166 |  2466 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2467 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2468 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2469 | `			return SXERR_ABORT;` |
|       - |  2470 | `		}` |
|     166 |  2471 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     166 |  2472 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     166 |  2473 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     166 |  2474 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     166 |  2475 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      82 |  2476 | `	}` |
|       - |  2477 | `	/* Arrow functions are always closures */` |
|     168 |  2478 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2479 | `	/* Compile the body expression as an implicit return */` |
|     251 |  2480 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      83 |  2481 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     168 |  2482 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2483 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2484 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2485 | `		return SXERR_ABORT;` |
|       - |  2486 | `	}` |
|     168 |  2487 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     168 |  2488 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     168 |  2489 | `	pSavedEnd = pGen->pEnd;` |
|     168 |  2490 | `	pGen->pIn = pBodyStart;` |
|     168 |  2491 | `	pGen->pEnd = pBodyEnd;` |
|     168 |  2492 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     168 |  2493 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2494 | `		return SXERR_ABORT;` |
|       - |  2495 | `	}` |
|       - |  2496 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2497 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2498 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2499 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     168 |  2500 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     168 |  2501 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     168 |  2502 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     168 |  2503 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     168 |  2504 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2505 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     168 |  2506 | `	pGen->pIn = pBodyEnd;` |
|     168 |  2507 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2508 | `	/* Emit the load-closure instruction */` |
|     168 |  2509 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     168 |  2510 | `	return SXRET_OK;` |
|      90 |  2511 | `}` |
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
| 1147702 |  2867 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2868 | `{` |
| 1147707 |  2869 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2870 | `	sxi32 iVv;` |
|       - |  2871 | `	sxi32 iP1;` |
|       - |  2872 | `	void *p3;` |
|       - |  2873 | `	sxi32 rc;` |
| 1147707 |  2874 | `	iVv = -1; /* Variable variable counter */` |
| 2295421 |  2875 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1147719 |  2876 | `		pGen->pIn++;` |
| 1147719 |  2877 | `		iVv++;` |
|       5 |  2878 | `	}` |
| 1147707 |  2879 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2880 | `		/* Invalid variable name */` |
|     ! 0 |  2881 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2882 | `		if( rc == SXERR_ABORT ){` |
|       - |  2883 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2884 | `			return SXERR_ABORT;` |
|       - |  2885 | `		}` |
|     ! 0 |  2886 | `		return SXRET_OK;` |
|       - |  2887 | `	}` |
| 1147707 |  2888 | `	p3  = 0;` |
| 1147707 |  2889 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
| 1147691 |  2909 | `		char *zName = 0;` |
|       - |  2910 | `		/* Extract variable name */` |
| 1147691 |  2911 | `		pName = &pGen->pIn->sData;` |
|       - |  2912 | `		/* Advance the stream cursor */` |
| 1147691 |  2913 | `		pGen->pIn++;` |
| 1147691 |  2914 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1147691 |  2915 | `		if( pEntry == 0 ){` |
|       - |  2916 | `			/* Duplicate name */` |
|  165129 |  2917 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  165129 |  2918 | `			if( zName == 0 ){` |
|     ! 0 |  2919 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2920 | `				return SXERR_ABORT;` |
|       - |  2921 | `			}` |
|       - |  2922 | `			/* Install in the hashtable */` |
|  165129 |  2923 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   82567 |  2924 | `		}else{` |
|       - |  2925 | `			/* Name already available */` |
|  982567 |  2926 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2927 | `		}` |
| 1147691 |  2928 | `		p3 = (void *)zName;` |
|       - |  2929 | `	}` |
| 1147703 |  2930 | `	iP1 = 0;` |
| 1147703 |  2931 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  447853 |  2932 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2933 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  447835 |  2934 | `			iP1 = 1;` |
|  223915 |  2935 | `		}` |
|  223924 |  2936 | `	}` |
|       - |  2937 | `	/* Emit the load instruction */` |
| 1147703 |  2938 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1147715 |  2939 | `	while( iVv > 0 ){` |
|      13 |  2940 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2941 | `		iVv--;` |
|       1 |  2942 | `	}` |
|       - |  2943 | `	/* Node successfully compiled */` |
| 1147703 |  2944 | `	return SXRET_OK;` |
|  573856 |  2945 | `}` |
|       - |  2946 | `/*` |
|       - |  2947 | ` * Load a literal.` |
|       - |  2948 | ` */` |
|  791420 |  2949 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  2950 | `{` |
|  791425 |  2951 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2952 | `	ph7_value *pObj;` |
|       - |  2953 | `	SyString *pStr;` |
|       - |  2954 | `	sxu32 nIdx;` |
|       - |  2955 | `	/* Extract token value */` |
|  791425 |  2956 | `	pStr = &pToken->sData;` |
|       - |  2957 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  791425 |  2958 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  167751 |  2959 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2960 | `			/* NULL constant are always indexed at 0 */` |
|   61717 |  2961 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   61717 |  2962 | `			return SXRET_OK;` |
|  106039 |  2963 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2964 | `			/* TRUE constant are always indexed at 1 */` |
|     801 |  2965 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     801 |  2966 | `			return SXRET_OK;` |
|       5 |  2967 | `		}` |
|  729905 |  2968 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  107214 |  2969 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2970 | `			/* FALSE constant are always indexed at 2 */` |
|   47309 |  2971 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   47309 |  2972 | `			return SXRET_OK;` |
|  632549 |  2973 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  112348 |  2974 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2975 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|   10769 |  2976 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   10769 |  2977 | `			if( pObj == 0 ){` |
|     ! 0 |  2978 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2979 | `				return SXERR_ABORT;` |
|       - |  2980 | `			}` |
|   10769 |  2981 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2982 | `			/* Emit the load constant instruction */` |
|   10769 |  2983 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   10769 |  2984 | `			return SXRET_OK;` |
|  583759 |  2985 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   36296 |  2986 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
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
|  573174 |  3002 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   24886 |  3003 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  575326 |  3004 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   19466 |  3005 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|  670833 |  3035 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  3036 | `		ph7_value *pLitObj;` |
|       - |  3037 | `		/* Unknown literal,install it in the literal table */` |
|  285729 |  3038 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  285729 |  3039 | `		if( pLitObj == 0 ){` |
|     ! 0 |  3040 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3041 | `			return SXERR_ABORT;` |
|       - |  3042 | `		}` |
|  285729 |  3043 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  285729 |  3044 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  142862 |  3045 | `	}` |
|       - |  3046 | `	/* Emit the load constant instruction */` |
|  670833 |  3047 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  670833 |  3048 | `	return SXRET_OK;` |
|  395715 |  3049 | `}` |
|       - |  3050 | `/*` |
|       - |  3051 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  3052 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  3053 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  3054 | ` * Otherwise, load the simple literal directly.` |
|       - |  3055 | ` */` |
|  795046 |  3056 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  3057 | `{` |
|       - |  3058 | `	sxi32 rc;` |
|  795051 |  3059 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3060 | `		return SXRET_OK;` |
|       - |  3061 | `	}` |
|       - |  3062 | `	/* Check if this is a multi-token namespace path */` |
|  795051 |  3063 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  3064 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|    3631 |  3065 | `		SyBlob *pWorker = &pGen->sWorker;` |
|    3631 |  3066 | `		int isAbsolute = 0;` |
|    3631 |  3067 | `		SyBlobReset(pWorker);` |
|       - |  3068 | `		/* Check for leading backslash (absolute path) */` |
|    3631 |  3069 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|    3629 |  3070 | `			isAbsolute = 1;` |
|    3629 |  3071 | `			pGen->pIn++; /* Skip leading backslash */` |
|    1812 |  3072 | `		}` |
|       - |  3073 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|    3631 |  3074 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  3075 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  3076 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  3077 | `		}` |
|       - |  3078 | `		/* Collect all path components */` |
|    3727 |  3079 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    3727 |  3080 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      53 |  3081 | `				SyBlobAppend(pWorker,"\\",1);` |
|      29 |  3082 | `			}else{` |
|    3679 |  3083 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  3084 | `			}` |
|    3727 |  3085 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|    3631 |  3086 | `				pGen->pIn++;` |
|    3631 |  3087 | `				break;` |
|       - |  3088 | `			}` |
|     101 |  3089 | `			pGen->pIn++;` |
|       5 |  3090 | `		}` |
|    3631 |  3091 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  3092 | `			ph7_value *pObj;` |
|       - |  3093 | `			SyString sPath;` |
|       - |  3094 | `			sxu32 nIdx;` |
|    3631 |  3095 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  3096 | `			/* Install in the literal table */` |
|    3631 |  3097 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|    3607 |  3098 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    3607 |  3099 | `				if( pObj == 0 ){` |
|     ! 0 |  3100 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3101 | `					return SXERR_ABORT;` |
|       - |  3102 | `				}` |
|    3607 |  3103 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|    3607 |  3104 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|    1801 |  3105 | `			}` |
|       - |  3106 | `			/* Emit the load constant instruction.` |
|       - |  3107 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  3108 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|    5444 |  3109 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|    1813 |  3110 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|    1813 |  3111 | `				nIdx,0,0);` |
|    3631 |  3112 | `			return SXRET_OK;` |
|       - |  3113 | `		}` |
|     ! 0 |  3114 | `	}` |
|       - |  3115 | `	/* Single-token literal: load directly */` |
|  791425 |  3116 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  791425 |  3117 | `	return rc;` |
|  397528 |  3118 | `}` |
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
|  795046 |  3135 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3136 | `{` |
|       - |  3137 | `	sxi32 rc;` |
|  795051 |  3138 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  795051 |  3139 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3140 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3141 | `		return rc;` |
|       - |  3142 | `	}` |
|       - |  3143 | `	/* Node successfully compiled */` |
|  795051 |  3144 | `	return SXRET_OK;` |
|  397528 |  3145 | `}` |
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
|     116 |  3162 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3163 | `{` |
|     121 |  3164 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      30 |  3165 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3166 | `			return TRUE;` |
|      28 |  3167 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       6 |  3168 | `			return TRUE;` |
|       2 |  3169 | `		}` |
|     105 |  3170 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3171 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3172 | `			return TRUE;` |
|       - |  3173 | `		}` |
|     ! 0 |  3174 | `	}` |
|       - |  3175 | `	/* Not a reserved constant */` |
|     113 |  3176 | `	return FALSE;` |
|      63 |  3177 | `}` |
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
|      32 |  3202 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3203 | `{` |
|       - |  3204 | `	SySet *pConsCode,*pInstrContainer;` |
|      37 |  3205 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3206 | `	SyString *pName;` |
|       - |  3207 | `	sxi32 rc;` |
|      37 |  3208 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      37 |  3209 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3210 | `		/* Invalid constant name */` |
|       9 |  3211 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       9 |  3212 | `		if( rc == SXERR_ABORT ){` |
|       - |  3213 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3214 | `			return SXERR_ABORT;` |
|       - |  3215 | `		}` |
|       9 |  3216 | `		goto Synchronize;` |
|       - |  3217 | `	}` |
|       - |  3218 | `	/* Peek constant name */` |
|      31 |  3219 | `	pName = &pGen->pIn->sData;` |
|       - |  3220 | `	/* Make sure the constant name isn't reserved */` |
|      31 |  3221 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3222 | `		/* Reserved constant */` |
|      10 |  3223 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|      10 |  3224 | `		if( rc == SXERR_ABORT ){` |
|       - |  3225 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3226 | `			return SXERR_ABORT;` |
|       - |  3227 | `		}` |
|      10 |  3228 | `		goto Synchronize;` |
|       - |  3229 | `	}` |
|      21 |  3230 | `	pGen->pIn++;` |
|      21 |  3231 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3232 | `		/* Invalid statement*/` |
|       6 |  3233 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3234 | `		if( rc == SXERR_ABORT ){` |
|       - |  3235 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3236 | `			return SXERR_ABORT;` |
|       - |  3237 | `		}` |
|       6 |  3238 | `		goto Synchronize;` |
|       - |  3239 | `	}` |
|      15 |  3240 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3241 | `	/* Allocate a new constant value container */` |
|      15 |  3242 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  3243 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3244 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3245 | `		return SXERR_ABORT;` |
|       - |  3246 | `	}` |
|      15 |  3247 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3248 | `	/* Swap bytecode container */` |
|      15 |  3249 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  3250 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3251 | `	/* Compile constant value */` |
|      15 |  3252 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3253 | `	/* Emit the done instruction */` |
|      15 |  3254 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  3255 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  3256 | `	if( rc == SXERR_ABORT ){` |
|       - |  3257 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3258 | `		return SXERR_ABORT;` |
|       - |  3259 | `	}` |
|      15 |  3260 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3261 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3262 | `	{` |
|       - |  3263 | `		SyBlob sFQN;` |
|       - |  3264 | `		SyString sFQNStr;` |
|      15 |  3265 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  3266 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  3267 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  3268 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  3269 | `		SyBlobRelease(&sFQN);` |
|       - |  3270 | `	}` |
|      15 |  3271 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3272 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3273 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3274 | `	}` |
|      15 |  3275 | `	return SXRET_OK;` |
|       9 |  3276 | `Synchronize:` |
|       - |  3277 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3278 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      41 |  3279 | `		pGen->pIn++;` |
|       3 |  3280 | `	}` |
|      22 |  3281 | `	return SXRET_OK;` |
|      21 |  3282 | `}` |
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
|    3726 |  3305 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3306 | `{` |
|    3731 |  3307 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   21869 |  3308 | `	while( pBlock && pBlock != pTarget ){` |
|   18143 |  3309 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  3310 | `			if( pBlock->pUserData ){` |
|       - |  3311 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  3312 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  3313 | `			}else{` |
|       - |  3314 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  3315 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  3316 | `				 * exception context from a sub-execution.` |
|       - |  3317 | `				 */` |
|     ! 0 |  3318 | `				break;` |
|       - |  3319 | `			}` |
|       1 |  3320 | `		}` |
|   18143 |  3321 | `		pBlock = pBlock->pParent;` |
|       5 |  3322 | `	}` |
|    3731 |  3323 | `}` |
|    3630 |  3324 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3325 | `{` |
|       - |  3326 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3327 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3328 | `	sxu32 nLineLocal;` |
|       - |  3329 | `	sxi32 rc;` |
|    3635 |  3330 | `	nLineLocal = pGen->pIn->nLine;` |
|    3635 |  3331 | `	iLevel = 0;` |
|       - |  3332 | `	/* Jump the 'continue' keyword */` |
|    3635 |  3333 | `	pGen->pIn++;` |
|    3635 |  3334 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3335 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3336 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3337 | `		 */` |
|       - |  3338 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3339 | `		char *zAlloc = 0;` |
|       - |  3340 | `		SyString sNum;` |
|      17 |  3341 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3342 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3343 | `			return SXERR_ABORT;` |
|       - |  3344 | `		}` |
|      17 |  3345 | `		if( rc == SXRET_OK ){` |
|      20 |  3346 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3347 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3348 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3349 | `				return SXERR_ABORT;` |
|       - |  3350 | `			}` |
|      14 |  3351 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3352 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3353 | `		}` |
|      17 |  3354 | `		if( iLevel < 2 ){` |
|       3 |  3355 | `			iLevel = 0;` |
|       1 |  3356 | `		}` |
|      17 |  3357 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3358 | `	}` |
|       - |  3359 | `	/* Point to the target loop */` |
|    3635 |  3360 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3635 |  3361 | `	if( pLoop == 0 ){` |
|       - |  3362 | `		/* Illegal continue */` |
|      13 |  3363 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      13 |  3364 | `		if( rc == SXERR_ABORT ){` |
|       - |  3365 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3366 | `			return SXERR_ABORT;` |
|       - |  3367 | `		}` |
|       8 |  3368 | `	}else{` |
|    3625 |  3369 | `		sxu32 nInstrIdx = 0;` |
|       - |  3370 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    3625 |  3371 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    3625 |  3372 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3373 | `			/* According to the PHP language reference manual` |
|       - |  3374 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3375 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3376 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3377 | `			 */` |
|       5 |  3378 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  3379 | `			if( rc == SXRET_OK ){` |
|       5 |  3380 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3381 | `			}` |
|       3 |  3382 | `		}else{` |
|       - |  3383 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3621 |  3384 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3621 |  3385 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3386 | `				JumpFixup sJumpFix;` |
|       - |  3387 | `				/* Post-continue */` |
|      14 |  3388 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3389 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3390 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3391 | `			}` |
|       - |  3392 | `		}` |
|       - |  3393 | `	}` |
|    3635 |  3394 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3395 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3396 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3397 | `	}` |
|       - |  3398 | `	/* Statement successfully compiled */` |
|    3635 |  3399 | `	return SXRET_OK;` |
|    1820 |  3400 | `}` |
|       - |  3401 | `/*` |
|       - |  3402 | ` * Compile the 'break' statement.` |
|       - |  3403 | ` * According to the PHP language reference` |
|       - |  3404 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3405 | ` *  structure.` |
|       - |  3406 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3407 | ` *  enclosing structures are to be broken out of.` |
|       - |  3408 | ` */` |
|     122 |  3409 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3410 | `{` |
|       - |  3411 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3412 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3413 | `	sxi32 rc;` |
|     127 |  3414 | `	iLevel = 0;` |
|       - |  3415 | `	/* Jump the 'break' keyword */` |
|     127 |  3416 | `	pGen->pIn++;` |
|     127 |  3417 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3418 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3419 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3420 | `		 */` |
|       - |  3421 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      18 |  3422 | `		char *zAlloc = 0;` |
|       - |  3423 | `		SyString sNum;` |
|      18 |  3424 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      18 |  3425 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3426 | `			return SXERR_ABORT;` |
|       - |  3427 | `		}` |
|      18 |  3428 | `		if( rc == SXRET_OK ){` |
|      21 |  3429 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3430 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      15 |  3431 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3432 | `				return SXERR_ABORT;` |
|       - |  3433 | `			}` |
|      15 |  3434 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      15 |  3435 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3436 | `		}` |
|      18 |  3437 | `		if( iLevel < 2 ){` |
|       3 |  3438 | `			iLevel = 0;` |
|       1 |  3439 | `		}` |
|      18 |  3440 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3441 | `	}` |
|       - |  3442 | `	/* Extract the target loop */` |
|     127 |  3443 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     127 |  3444 | `	if( pLoop == 0 ){` |
|       - |  3445 | `		/* Illegal break */` |
|      19 |  3446 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      19 |  3447 | `		if( rc == SXERR_ABORT ){` |
|       - |  3448 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3449 | `			return SXERR_ABORT;` |
|       - |  3450 | `		}` |
|      11 |  3451 | `	}else{` |
|       - |  3452 | `		sxu32 nInstrIdx;` |
|       - |  3453 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|     111 |  3454 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|     111 |  3455 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|     111 |  3456 | `		if( rc == SXRET_OK ){` |
|       - |  3457 | `			/* Fix the jump later when the jump destination is resolved */` |
|     111 |  3458 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      53 |  3459 | `		}` |
|       - |  3460 | `	}` |
|     127 |  3461 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3462 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3463 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3464 | `	}` |
|       - |  3465 | `	/* Statement successfully compiled */` |
|     127 |  3466 | `	return SXRET_OK;` |
|      66 |  3467 | `}` |
|       - |  3468 | `/*` |
|       - |  3469 | ` * Compile or record a label.` |
|       - |  3470 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3471 | ` * Example` |
|       - |  3472 | ` *  goto LABEL;` |
|       - |  3473 | ` *   echo 'Foo';` |
|       - |  3474 | ` *  LABEL:` |
|       - |  3475 | ` *   echo 'Bar';` |
|       - |  3476 | ` */` |
|     112 |  3477 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  3478 | `{` |
|       - |  3479 | `	GenBlock *pBlock;` |
|       - |  3480 | `	Label sLabel;` |
|       - |  3481 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     117 |  3482 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     117 |  3483 | `	if( pBlock ){` |
|       - |  3484 | `		sxi32 rc;` |
|       8 |  3485 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3486 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       6 |  3487 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3488 | `			return SXERR_ABORT;` |
|       - |  3489 | `		}` |
|       4 |  3490 | `	}else{` |
|     113 |  3491 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3492 | `		char *zDup;` |
|       - |  3493 | `		/* Initialize label fields */` |
|     113 |  3494 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3495 | `		/* Duplicate label name */` |
|     113 |  3496 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     113 |  3497 | `		if( zDup == 0 ){` |
|     ! 0 |  3498 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3499 | `			return SXERR_ABORT;` |
|       - |  3500 | `		}` |
|     113 |  3501 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     113 |  3502 | `		sLabel.bRef  = FALSE;` |
|     113 |  3503 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     113 |  3504 | `		pBlock = pGen->pCurrent;` |
|     221 |  3505 | `		while( pBlock ){` |
|     133 |  3506 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      24 |  3507 | `				break;` |
|       - |  3508 | `			}` |
|       - |  3509 | `			/* Point to the upper block */` |
|     113 |  3510 | `			pBlock = pBlock->pParent;` |
|       5 |  3511 | `		}` |
|     113 |  3512 | `		if( pBlock ){` |
|      24 |  3513 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      14 |  3514 | `		}else{` |
|      93 |  3515 | `			sLabel.pFunc = 0;` |
|       - |  3516 | `		}` |
|       - |  3517 | `		/* Insert in label set */` |
|     113 |  3518 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3519 | `	}` |
|     117 |  3520 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     117 |  3521 | `	return SXRET_OK;` |
|      61 |  3522 | `}` |
|       - |  3523 | `/*` |
|       - |  3524 | ` * Compile the so hated 'goto' statement.` |
|       - |  3525 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3526 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3527 | ` * a compiler it has to do this.` |
|       - |  3528 | ` * According to the PHP language reference manual` |
|       - |  3529 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3530 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3531 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3532 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3533 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3534 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3535 | ` *   of a multi-level break` |
|       - |  3536 | ` */` |
|     152 |  3537 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  3538 | `{` |
|       - |  3539 | `	JumpFixup sJump;` |
|       - |  3540 | `	sxi32 rc;` |
|     157 |  3541 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     157 |  3542 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3543 | `		/* Missing label */` |
|     ! 0 |  3544 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3545 | `		if( rc == SXERR_ABORT ){` |
|       - |  3546 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3547 | `			return SXERR_ABORT;` |
|       - |  3548 | `		}` |
|     ! 0 |  3549 | `		return SXRET_OK;` |
|       - |  3550 | `	}` |
|     157 |  3551 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       6 |  3552 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       6 |  3553 | `		if( rc == SXERR_ABORT ){` |
|       - |  3554 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3555 | `			return SXERR_ABORT;` |
|       - |  3556 | `		}` |
|       4 |  3557 | `	}else{` |
|     153 |  3558 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3559 | `		GenBlock *pBlock;` |
|       - |  3560 | `		char *zDup;` |
|       - |  3561 | `		/* Prepare the jump destination */` |
|     153 |  3562 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     153 |  3563 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3564 | `		/* Duplicate label name */` |
|     153 |  3565 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     153 |  3566 | `		if( zDup == 0 ){` |
|     ! 0 |  3567 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3568 | `			return SXERR_ABORT;` |
|       - |  3569 | `		}` |
|     153 |  3570 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     153 |  3571 | `		pBlock = pGen->pCurrent;` |
|     315 |  3572 | `		while( pBlock ){` |
|     199 |  3573 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      37 |  3574 | `				break;` |
|       - |  3575 | `			}` |
|       - |  3576 | `			/* Point to the upper block */` |
|     167 |  3577 | `			pBlock = pBlock->pParent;` |
|       5 |  3578 | `		}` |
|     153 |  3579 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       9 |  3580 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       9 |  3581 | `			if( rc == SXERR_ABORT ){` |
|       - |  3582 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3583 | `				return SXERR_ABORT;` |
|       - |  3584 | `			}` |
|       3 |  3585 | `		}` |
|     153 |  3586 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      30 |  3587 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      17 |  3588 | `		}else{` |
|     127 |  3589 | `			sJump.pFunc = 0;` |
|       - |  3590 | `		}` |
|       - |  3591 | `		/* Emit the unconditional jump */` |
|     153 |  3592 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     153 |  3593 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3594 | `		}` |
|       - |  3595 | `	}` |
|     157 |  3596 | `	pGen->pIn++; /* Jump the label name */` |
|     157 |  3597 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3598 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3599 | `	}` |
|       - |  3600 | `	/* Statement successfully compiled */` |
|     157 |  3601 | `	return SXRET_OK;` |
|      81 |  3602 | `}` |
|       - |  3603 | `/*` |
|       - |  3604 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3605 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3606 | ` * failure.` |
|       - |  3607 | ` */` |
|      20 |  3608 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       2 |  3609 | `{` |
|       - |  3610 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3611 | `	sxu32 nRawObj;` |
|      10 |  3612 | `	sxu32 nObjIdx;` |
|       - |  3613 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3614 | `	 * a PHP block.` |
|       - |  3615 | `	 */` |
|      10 |  3616 | `Consume:` |
|      22 |  3617 | `	nRawObj = nObjIdx = 0;` |
|      22 |  3618 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3619 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3620 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3621 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3622 | `			return SXERR_ABORT;` |
|       - |  3623 | `		}` |
|       - |  3624 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3625 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3626 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3627 | `		++nRawObj;` |
|     ! 0 |  3628 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3629 | `	}` |
|      22 |  3630 | `	if( nRawObj > 0 ){` |
|       - |  3631 | `		/* Emit the consume instruction */` |
|     ! 0 |  3632 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3633 | `	}` |
|      22 |  3634 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3635 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3636 | `		/* Reset the token set */` |
|     ! 0 |  3637 | `		SySetReset(pTokenSet);` |
|       - |  3638 | `		/* Tokenize input */` |
|     ! 0 |  3639 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3640 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3641 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3642 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3643 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3644 | `		/* Advance the stream cursor */` |
|     ! 0 |  3645 | `		pGen->pRawIn++;` |
|       - |  3646 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3647 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3648 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3649 | `			sxi32 rc;` |
|       - |  3650 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3651 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3652 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3653 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3654 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3655 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3656 | `				return SXERR_ABORT;` |
|     ! 0 |  3657 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3658 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3659 | `			}` |
|     ! 0 |  3660 | `			goto Consume;` |
|       - |  3661 | `		}` |
|     ! 0 |  3662 | `	}else{` |
|       - |  3663 | `		/* No more chunks to process */` |
|      22 |  3664 | `		pGen->pIn = pGen->pEnd;` |
|      22 |  3665 | `		return SXERR_EOF;` |
|       - |  3666 | `	}` |
|     ! 0 |  3667 | `	return SXRET_OK;` |
|      12 |  3668 | `}` |
|       - |  3669 | `/*` |
|       - |  3670 | ` * Compile a PHP block.` |
|       - |  3671 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3672 | ` * optionally delimited by braces {}.` |
|       - |  3673 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3674 | ` * and this function takes care of generating the appropriate error` |
|       - |  3675 | ` * message.` |
|       - |  3676 | ` */` |
|  435622 |  3677 | `static sxi32 PH7_CompileBlock(` |
|       - |  3678 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3679 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3680 | `	)` |
|       5 |  3681 | `{` |
|       - |  3682 | `	sxi32 rc;` |
|       - |  3683 | `	sxu32 nLine;` |
|  435627 |  3684 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  433935 |  3685 | `		nLine = pGen->pIn->nLine;` |
|  433935 |  3686 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  433935 |  3687 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3688 | `			return SXERR_ABORT;` |
|       - |  3689 | `		}` |
|  433935 |  3690 | `		pGen->pIn++;` |
|       - |  3691 | `		/* Compile until we hit the closing braces '}' */` |
|  594268 |  3692 | `		for(;;){` |
| 1188541 |  3693 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      22 |  3694 | `				rc = GenStateNextChunk(&(*pGen));` |
|      22 |  3695 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3696 | `			 	   return SXERR_ABORT;` |
|       - |  3697 | `				}` |
|      22 |  3698 | `				if( rc == SXERR_EOF ){` |
|       - |  3699 | `					/* No more token to process. Missing closing braces */` |
|      22 |  3700 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      22 |  3701 | `					break;` |
|       - |  3702 | `				}` |
|     ! 0 |  3703 | `			}` |
| 1188521 |  3704 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3705 | `				/* Closing braces found,break immediately*/` |
|  433915 |  3706 | `				pGen->pIn++;` |
|  433915 |  3707 | `				break;` |
|       - |  3708 | `			}` |
|       - |  3709 | `			/* Compile a single statement */` |
|  754611 |  3710 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  754611 |  3711 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3712 | `				return SXERR_ABORT;` |
|       - |  3713 | `			}` |
|       5 |  3714 | `		}` |
|  433935 |  3715 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  218662 |  3716 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3717 | `		pGen->pIn++;` |
|     ! 0 |  3718 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3719 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3720 | `			return SXERR_ABORT;` |
|       - |  3721 | `		}` |
|       - |  3722 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3723 | `		for(;;){` |
|     ! 0 |  3724 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3725 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3726 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3727 | `			 	   return SXERR_ABORT;` |
|       - |  3728 | `				}` |
|     ! 0 |  3729 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3730 | `					/* No more token to process */` |
|     ! 0 |  3731 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3732 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3733 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3734 | `					}` |
|     ! 0 |  3735 | `					break;` |
|       - |  3736 | `				}` |
|     ! 0 |  3737 | `			}` |
|     ! 0 |  3738 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3739 | `				sxi32 nKwrd;` |
|       - |  3740 | `				/* Keyword found */` |
|     ! 0 |  3741 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3742 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3743 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3744 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3745 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3746 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3747 | `						}` |
|     ! 0 |  3748 | `						break;` |
|       - |  3749 | `				}` |
|     ! 0 |  3750 | `			}` |
|       - |  3751 | `			/* Compile a single statement */` |
|     ! 0 |  3752 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3753 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3754 | `				return SXERR_ABORT;` |
|       - |  3755 | `			}` |
|     ! 0 |  3756 | `		}` |
|     ! 0 |  3757 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3758 | `	}else{` |
|       - |  3759 | `		/* Compile a single statement */` |
|    1697 |  3760 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1697 |  3761 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3762 | `			return SXERR_ABORT;` |
|       - |  3763 | `		}` |
|       - |  3764 | `	}` |
|       - |  3765 | `	/* Jump trailing semi-colons ';' */` |
|  435627 |  3766 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3767 | `		pGen->pIn++;` |
|     ! 0 |  3768 | `	}` |
|  435627 |  3769 | `	return SXRET_OK;` |
|  217816 |  3770 | `}` |
|       - |  3771 | `/*` |
|       - |  3772 | ` * Compile the gentle 'while' statement.` |
|       - |  3773 | ` * According to the PHP language reference` |
|       - |  3774 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3775 | ` *  The basic form of a while statement is:` |
|       - |  3776 | ` *  while (expr)` |
|       - |  3777 | ` *   statement` |
|       - |  3778 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3779 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3780 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3781 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3782 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3783 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3784 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3785 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3786 | ` *  while (expr):` |
|       - |  3787 | ` *    statement` |
|       - |  3788 | ` *   endwhile;` |
|       - |  3789 | ` */` |
|   14464 |  3790 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3791 | `{` |
|   14469 |  3792 | `	GenBlock *pWhileBlock = 0;` |
|   14469 |  3793 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3794 | `	sxu32 nFalseJump;` |
|       - |  3795 | `	sxu32 nLine;` |
|       - |  3796 | `	sxi32 rc;` |
|   14469 |  3797 | `	nLine = pGen->pIn->nLine;` |
|       - |  3798 | `	/* Jump the 'while' keyword */` |
|   14469 |  3799 | `	pGen->pIn++;` |
|   14469 |  3800 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3801 | `		/* Syntax error */` |
|     ! 0 |  3802 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3803 | `		if( rc == SXERR_ABORT ){` |
|       - |  3804 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3805 | `			return SXERR_ABORT;` |
|       - |  3806 | `		}` |
|     ! 0 |  3807 | `		goto Synchronize;` |
|       - |  3808 | `	}` |
|       - |  3809 | `	/* Jump the left parenthesis '(' */` |
|   14469 |  3810 | `	pGen->pIn++;` |
|       - |  3811 | `	/* Create the loop block */` |
|   14469 |  3812 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   14469 |  3813 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3814 | `		return SXERR_ABORT;` |
|       - |  3815 | `	}` |
|       - |  3816 | `	/* Delimit the condition */` |
|   14469 |  3817 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14469 |  3818 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3819 | `		/* Empty expression */` |
|       3 |  3820 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3821 | `		if( rc == SXERR_ABORT ){` |
|       - |  3822 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3823 | `			return SXERR_ABORT;` |
|       - |  3824 | `		}` |
|       1 |  3825 | `	}` |
|       - |  3826 | `	/* Swap token streams */` |
|   14469 |  3827 | `	pTmp = pGen->pEnd;` |
|   14469 |  3828 | `	pGen->pEnd = pEnd;` |
|       - |  3829 | `	/* Compile the expression */` |
|   14469 |  3830 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14469 |  3831 | `	if( rc == SXERR_ABORT ){` |
|       - |  3832 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3833 | `		return SXERR_ABORT;` |
|       - |  3834 | `	}` |
|       - |  3835 | `	/* Update token stream */` |
|   14469 |  3836 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3837 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3838 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3839 | `			return SXERR_ABORT;` |
|       - |  3840 | `		}` |
|     ! 0 |  3841 | `		pGen->pIn++;` |
|     ! 0 |  3842 | `	}` |
|       - |  3843 | `	/* Synchronize pointers */` |
|   14469 |  3844 | `	pGen->pIn  = &pEnd[1];` |
|   14469 |  3845 | `	pGen->pEnd = pTmp;` |
|       - |  3846 | `	/* Emit the false jump */` |
|   14469 |  3847 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3848 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14469 |  3849 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3850 | `	/* Compile the loop body */` |
|   14469 |  3851 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   14469 |  3852 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3853 | `		return SXERR_ABORT;` |
|       - |  3854 | `	}` |
|       - |  3855 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14469 |  3856 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3857 | `	/* Fix all jumps now the destination is resolved */` |
|   14469 |  3858 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3859 | `	/* Release the loop block */` |
|   14469 |  3860 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3861 | `	/* Statement successfully compiled */` |
|   14469 |  3862 | `	return SXRET_OK;` |
|     ! 0 |  3863 | `Synchronize:` |
|       - |  3864 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3865 | `	 * compiling this erroneous block.` |
|       - |  3866 | `	 */` |
|     ! 0 |  3867 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3868 | `		pGen->pIn++;` |
|     ! 0 |  3869 | `	}` |
|     ! 0 |  3870 | `	return SXRET_OK;` |
|    7237 |  3871 | `}` |
|       - |  3872 | `/*` |
|       - |  3873 | ` * Compile the ugly do..while() statement.` |
|       - |  3874 | ` * According to the PHP language reference` |
|       - |  3875 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3876 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3877 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3878 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3879 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3880 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3881 | ` *  would end immediately).` |
|       - |  3882 | ` *  There is just one syntax for do-while loops:` |
|       - |  3883 | ` *  <?php` |
|       - |  3884 | ` *  $i = 0;` |
|       - |  3885 | ` *  do {` |
|       - |  3886 | ` *   echo $i;` |
|       - |  3887 | ` *  } while ($i > 0);` |
|       - |  3888 | ` * ?>` |
|       - |  3889 | ` */` |
|       2 |  3890 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3891 | `{` |
|       3 |  3892 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3893 | `	GenBlock *pDoBlock = 0;` |
|       - |  3894 | `	sxu32 nLine;` |
|       - |  3895 | `	sxi32 rc;` |
|       3 |  3896 | `	nLine = pGen->pIn->nLine;` |
|       - |  3897 | `	/* Jump the 'do' keyword */` |
|       3 |  3898 | `	pGen->pIn++;` |
|       - |  3899 | `	/* Create the loop block */` |
|       3 |  3900 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3901 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3902 | `		return SXERR_ABORT;` |
|       - |  3903 | `	}` |
|       - |  3904 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3905 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3906 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3907 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3908 | `		return SXERR_ABORT;` |
|       - |  3909 | `	}` |
|       3 |  3910 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3911 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3912 | `	}` |
|       3 |  3913 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3914 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3915 | `			/* Missing 'while' statement */` |
|       3 |  3916 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3917 | `			if( rc == SXERR_ABORT ){` |
|       - |  3918 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3919 | `				return SXERR_ABORT;` |
|       - |  3920 | `			}` |
|       3 |  3921 | `			goto Synchronize;` |
|       - |  3922 | `	}` |
|       - |  3923 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3924 | `	pGen->pIn++;` |
|     ! 0 |  3925 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3926 | `		/* Syntax error */` |
|     ! 0 |  3927 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3928 | `		if( rc == SXERR_ABORT ){` |
|       - |  3929 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3930 | `			return SXERR_ABORT;` |
|       - |  3931 | `		}` |
|     ! 0 |  3932 | `		goto Synchronize;` |
|       - |  3933 | `	}` |
|       - |  3934 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3935 | `	pGen->pIn++;` |
|       - |  3936 | `	/* Delimit the condition */` |
|     ! 0 |  3937 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3938 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3939 | `		/* Empty expression */` |
|     ! 0 |  3940 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3941 | `		if( rc == SXERR_ABORT ){` |
|       - |  3942 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3943 | `			return SXERR_ABORT;` |
|       - |  3944 | `		}` |
|     ! 0 |  3945 | `		goto Synchronize;` |
|       - |  3946 | `	}` |
|       - |  3947 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3948 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3949 | `		JumpFixup *aPost;` |
|       - |  3950 | `		VmInstr *pInstr;` |
|       - |  3951 | `		sxu32 nJumpDest;` |
|       - |  3952 | `		sxu32 n;` |
|     ! 0 |  3953 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3954 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3955 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3956 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3957 | `			if( pInstr ){` |
|       - |  3958 | `				/* Fix */` |
|     ! 0 |  3959 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3960 | `			}` |
|     ! 0 |  3961 | `		}` |
|     ! 0 |  3962 | `	}` |
|       - |  3963 | `	/* Swap token streams */` |
|     ! 0 |  3964 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3965 | `	pGen->pEnd = pEnd;` |
|       - |  3966 | `	/* Compile the expression */` |
|     ! 0 |  3967 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3968 | `	if( rc == SXERR_ABORT ){` |
|       - |  3969 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3970 | `		return SXERR_ABORT;` |
|       - |  3971 | `	}` |
|       - |  3972 | `	/* Update token stream */` |
|     ! 0 |  3973 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3974 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3975 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3976 | `			return SXERR_ABORT;` |
|       - |  3977 | `		}` |
|     ! 0 |  3978 | `		pGen->pIn++;` |
|     ! 0 |  3979 | `	}` |
|     ! 0 |  3980 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3981 | `	pGen->pEnd = pTmp;` |
|       - |  3982 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3983 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3984 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3985 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3986 | `	/* Release the loop block */` |
|     ! 0 |  3987 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3988 | `	/* Statement successfully compiled */` |
|     ! 0 |  3989 | `	return SXRET_OK;` |
|       1 |  3990 | `Synchronize:` |
|       - |  3991 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3992 | `	 * compiling this erroneous block.` |
|       - |  3993 | `	 */` |
|       3 |  3994 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3995 | `		pGen->pIn++;` |
|     ! 0 |  3996 | `	}` |
|       3 |  3997 | `	return SXRET_OK;` |
|       2 |  3998 | `}` |
|       - |  3999 | `/*` |
|       - |  4000 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  4001 | ` * According to the PHP language reference` |
|       - |  4002 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  4003 | ` *  The syntax of a for loop is:` |
|       - |  4004 | ` *  for (expr1; expr2; expr3)` |
|       - |  4005 | ` *   statement` |
|       - |  4006 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  4007 | ` *  the beginning of the loop.` |
|       - |  4008 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  4009 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  4010 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  4011 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  4012 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  4013 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  4014 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  4015 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  4016 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  4017 | ` *  of using the for truth expression.` |
|       - |  4018 | ` */` |
|   14464 |  4019 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  4020 | `{` |
|   14469 |  4021 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   14469 |  4022 | `	GenBlock *pForBlock = 0;` |
|       - |  4023 | `	sxu32 nFalseJump;` |
|       - |  4024 | `	sxu32 nLine;` |
|       - |  4025 | `	sxi32 rc;` |
|   14469 |  4026 | `	nLine = pGen->pIn->nLine;` |
|       - |  4027 | `	/* Jump the 'for' keyword */` |
|   14469 |  4028 | `	pGen->pIn++;` |
|   14469 |  4029 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4030 | `		/* Syntax error */` |
|     ! 0 |  4031 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  4032 | `		if( rc == SXERR_ABORT ){` |
|       - |  4033 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4034 | `			return SXERR_ABORT;` |
|       - |  4035 | `		}` |
|     ! 0 |  4036 | `		return SXRET_OK;` |
|       - |  4037 | `	}` |
|       - |  4038 | `	/* Jump the left parenthesis '(' */` |
|   14469 |  4039 | `	pGen->pIn++;` |
|       - |  4040 | `	/* Delimit the init-expr;condition;post-expr */` |
|   14469 |  4041 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14469 |  4042 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4043 | `		/* Empty expression */` |
|     ! 0 |  4044 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  4045 | `		if( rc == SXERR_ABORT ){` |
|       - |  4046 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4047 | `			return SXERR_ABORT;` |
|       - |  4048 | `		}` |
|       - |  4049 | `		/* Synchronize */` |
|     ! 0 |  4050 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4051 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4052 | `			pGen->pIn++;` |
|     ! 0 |  4053 | `		}` |
|     ! 0 |  4054 | `		return SXRET_OK;` |
|       - |  4055 | `	}` |
|       - |  4056 | `	/* Swap token streams */` |
|   14469 |  4057 | `	pTmp = pGen->pEnd;` |
|   14469 |  4058 | `	pGen->pEnd = pEnd;` |
|       - |  4059 | `	/* Compile initialization expressions if available */` |
|   14469 |  4060 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4061 | `	/* Pop operand lvalues */` |
|   14469 |  4062 | `	if( rc == SXERR_ABORT ){` |
|       - |  4063 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4064 | `		return SXERR_ABORT;` |
|   14469 |  4065 | `	}else if( rc != SXERR_EMPTY ){` |
|   14467 |  4066 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7231 |  4067 | `	}` |
|   14469 |  4068 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4069 | `		/* Syntax error */` |
|     ! 0 |  4070 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4071 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  4072 | `		if( rc == SXERR_ABORT ){` |
|       - |  4073 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4074 | `			return SXERR_ABORT;` |
|       - |  4075 | `		}` |
|     ! 0 |  4076 | `		return SXRET_OK;` |
|       - |  4077 | `	}` |
|       - |  4078 | `	/* Jump the trailing ';' */` |
|   14469 |  4079 | `	pGen->pIn++;` |
|       - |  4080 | `	/* Create the loop block */` |
|   14469 |  4081 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   14469 |  4082 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4083 | `		return SXERR_ABORT;` |
|       - |  4084 | `	}` |
|       - |  4085 | `	/* Deffer continue jumps */` |
|   14469 |  4086 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  4087 | `	/* Compile the condition */` |
|   14469 |  4088 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14469 |  4089 | `	if( rc == SXERR_ABORT ){` |
|       - |  4090 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4091 | `		return SXERR_ABORT;` |
|   14469 |  4092 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  4093 | `		/* Emit the false jump */` |
|   14467 |  4094 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4095 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14467 |  4096 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    7231 |  4097 | `	}` |
|   14469 |  4098 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4099 | `		/* Syntax error */` |
|       6 |  4100 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4101 | `			"for: Expected ';' after conditionals expressions");` |
|       6 |  4102 | `		if( rc == SXERR_ABORT ){` |
|       - |  4103 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4104 | `			return SXERR_ABORT;` |
|       - |  4105 | `		}` |
|       6 |  4106 | `		return SXRET_OK;` |
|       - |  4107 | `	}` |
|       - |  4108 | `	/* Jump the trailing ';' */` |
|   14465 |  4109 | `	pGen->pIn++;` |
|       - |  4110 | `	/* Save the post condition stream */` |
|   14465 |  4111 | `	pPostStart = pGen->pIn;` |
|       - |  4112 | `	/* Compile the loop body */` |
|   14465 |  4113 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   14465 |  4114 | `	pGen->pEnd = pTmp;` |
|   14465 |  4115 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   14465 |  4116 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4117 | `		return SXERR_ABORT;` |
|       - |  4118 | `	}` |
|       - |  4119 | `	/* Fix post-continue jumps */` |
|   14465 |  4120 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  4121 | `		JumpFixup *aPost;` |
|       - |  4122 | `		VmInstr *pInstr;` |
|       - |  4123 | `		sxu32 nJumpDest;` |
|       - |  4124 | `		sxu32 n;` |
|      14 |  4125 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  4126 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  4127 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  4128 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  4129 | `			if( pInstr ){` |
|       - |  4130 | `				/* Fix jump */` |
|      14 |  4131 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  4132 | `			}` |
|       8 |  4133 | `		}` |
|       6 |  4134 | `	}` |
|       - |  4135 | `	/* compile the post-expressions if available */` |
|   14465 |  4136 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4137 | `		pPostStart++;` |
|     ! 0 |  4138 | `	}` |
|   14465 |  4139 | `	if( pPostStart < pEnd ){` |
|       - |  4140 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   14465 |  4141 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   14465 |  4142 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14465 |  4143 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4144 | `			/* Syntax error */` |
|     ! 0 |  4145 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4146 | `			if( rc == SXERR_ABORT ){` |
|       - |  4147 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4148 | `				return SXERR_ABORT;` |
|       - |  4149 | `			}` |
|     ! 0 |  4150 | `			return SXRET_OK;` |
|       - |  4151 | `		}` |
|   14465 |  4152 | `		RE_SWAP_DELIMITER(pGen);` |
|   14465 |  4153 | `		if( rc == SXERR_ABORT ){` |
|       - |  4154 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4155 | `			return SXERR_ABORT;` |
|   14465 |  4156 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4157 | `			/* Pop operand lvalue */` |
|   14465 |  4158 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7230 |  4159 | `		}` |
|    7230 |  4160 | `	}` |
|       - |  4161 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14465 |  4162 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4163 | `	/* Fix all jumps now the destination is resolved */` |
|   14465 |  4164 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4165 | `	/* Release the loop block */` |
|   14465 |  4166 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4167 | `	/* Statement successfully compiled */` |
|   14465 |  4168 | `	return SXRET_OK;` |
|    7237 |  4169 | `}` |
|       - |  4170 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4171 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4172 | ` * are allowed.` |
|       - |  4173 | ` */` |
|    7758 |  4174 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4175 | `{` |
|    7763 |  4176 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    7763 |  4177 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4178 | `		/* Unexpected expression */` |
|     ! 0 |  4179 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4180 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4181 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4182 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4183 | `		}` |
|     ! 0 |  4184 | `	}` |
|    7763 |  4185 | `	return rc;` |
|       5 |  4186 | `}` |
|       - |  4187 | `/*` |
|       - |  4188 | ` * Compile the 'foreach' statement.` |
|       - |  4189 | ` * According to the PHP language reference` |
|       - |  4190 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4191 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4192 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4193 | ` *  is a minor but useful extension of the first:` |
|       - |  4194 | ` *  foreach (array_expression as $value)` |
|       - |  4195 | ` *    statement` |
|       - |  4196 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4197 | ` *   statement` |
|       - |  4198 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4199 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4200 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4201 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4202 | ` *  to the variable $key on each loop.` |
|       - |  4203 | ` *  Note:` |
|       - |  4204 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4205 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4206 | ` *  Note:` |
|       - |  4207 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4208 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4209 | ` *  or after the foreach without resetting it.` |
|       - |  4210 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4211 | ` *  of copying the value.` |
|       - |  4212 | ` */` |
|    3980 |  4213 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4214 | `{` |
|    3985 |  4215 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3985 |  4216 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3985 |  4217 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4218 | `	ph7_foreach_info *pInfo;` |
|       - |  4219 | `	sxu32 nFalseJump;` |
|       - |  4220 | `	VmInstr *pInstr;` |
|       - |  4221 | `	sxu32 nLine;` |
|       - |  4222 | `	sxi32 rc;` |
|    3985 |  4223 | `	nLine = pGen->pIn->nLine;` |
|       - |  4224 | `	/* Jump the 'foreach' keyword */` |
|    3985 |  4225 | `	pGen->pIn++;` |
|    3985 |  4226 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4227 | `		/* Syntax error */` |
|     ! 0 |  4228 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4229 | `		if( rc == SXERR_ABORT ){` |
|       - |  4230 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4231 | `			return SXERR_ABORT;` |
|       - |  4232 | `		}` |
|     ! 0 |  4233 | `		goto Synchronize;` |
|       - |  4234 | `	}` |
|       - |  4235 | `	/* Jump the left parenthesis '(' */` |
|    3985 |  4236 | `	pGen->pIn++;` |
|       - |  4237 | `	/* Create the loop block */` |
|    3985 |  4238 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3985 |  4239 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4240 | `		return SXERR_ABORT;` |
|       - |  4241 | `	}` |
|       - |  4242 | `	/* Delimit the expression */` |
|    3985 |  4243 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3985 |  4244 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4245 | `		/* Empty expression */` |
|     ! 0 |  4246 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4247 | `		if( rc == SXERR_ABORT ){` |
|       - |  4248 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4249 | `			return SXERR_ABORT;` |
|       - |  4250 | `		}` |
|       - |  4251 | `		/* Synchronize */` |
|     ! 0 |  4252 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4253 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4254 | `			pGen->pIn++;` |
|     ! 0 |  4255 | `		}` |
|     ! 0 |  4256 | `		return SXRET_OK;` |
|       - |  4257 | `	}` |
|       - |  4258 | `	/* Compile the array expression */` |
|    3985 |  4259 | `	pCur = pGen->pIn;` |
|   27335 |  4260 | `	while( pCur < pEnd ){` |
|   27335 |  4261 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3999 |  4262 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3999 |  4263 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4264 | `				/* Break with the first 'as' found */` |
|    3985 |  4265 | `				break;` |
|       - |  4266 | `			}` |
|       7 |  4267 | `		}` |
|       - |  4268 | `		/* Advance the stream cursor */` |
|   23355 |  4269 | `		pCur++;` |
|       5 |  4270 | `	}` |
|    3985 |  4271 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4272 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4273 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4274 | `		if( rc == SXERR_ABORT ){` |
|       - |  4275 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4276 | `			return SXERR_ABORT;` |
|       - |  4277 | `		}` |
|     ! 0 |  4278 | `		goto Synchronize;` |
|       - |  4279 | `	}` |
|       - |  4280 | `	/* Swap token streams */` |
|    3985 |  4281 | `	pTmp = pGen->pEnd;` |
|    3985 |  4282 | `	pGen->pEnd = pCur;` |
|    3985 |  4283 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3985 |  4284 | `	if( rc == SXERR_ABORT ){` |
|       - |  4285 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4286 | `		return SXERR_ABORT;` |
|       - |  4287 | `	}` |
|       - |  4288 | `	/* Update token stream */` |
|    3985 |  4289 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4290 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4291 | `		if( rc == SXERR_ABORT ){` |
|       - |  4292 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4293 | `			return SXERR_ABORT;` |
|       - |  4294 | `		}` |
|     ! 0 |  4295 | `		pGen->pIn++;` |
|     ! 0 |  4296 | `	}` |
|    3985 |  4297 | `	pCur++; /* Jump the 'as' keyword */` |
|    3985 |  4298 | `	pGen->pIn = pCur;` |
|    3985 |  4299 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4300 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4301 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4302 | `			return SXERR_ABORT;` |
|       - |  4303 | `		}` |
|     ! 0 |  4304 | `	}` |
|       - |  4305 | `	/* Create the foreach context */` |
|    3985 |  4306 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3985 |  4307 | `	if( pInfo == 0 ){` |
|     ! 0 |  4308 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4309 | `		return SXERR_ABORT;` |
|       - |  4310 | `	}` |
|       - |  4311 | `	/* Zero the structure */` |
|    3985 |  4312 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4313 | `	/* Initialize structure fields */` |
|    3985 |  4314 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4315 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - |  4316 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - |  4317 | `	 * '=>'. */` |
|    3985 |  4318 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    3985 |  4319 | `	if( pCur < pEnd ){` |
|       - |  4320 | `		/* Compile the expression holding the key name */` |
|    3799 |  4321 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4322 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4323 | `			if( rc == SXERR_ABORT ){` |
|       - |  4324 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4325 | `				return SXERR_ABORT;` |
|       - |  4326 | `			}` |
|     ! 0 |  4327 | `		}else{` |
|    3799 |  4328 | `			pGen->pEnd = pCur;` |
|    3799 |  4329 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3799 |  4330 | `			if( rc == SXERR_ABORT ){` |
|       - |  4331 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4332 | `				return SXERR_ABORT;` |
|       - |  4333 | `			}` |
|    3799 |  4334 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3799 |  4335 | `			if( pInstr->p3 ){` |
|       - |  4336 | `				/* Record key name */` |
|    3799 |  4337 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1897 |  4338 | `			}` |
|    3799 |  4339 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4340 | `		}` |
|    3799 |  4341 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1897 |  4342 | `	}` |
|    3985 |  4343 | `	pGen->pEnd = pEnd;` |
|    3985 |  4344 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4345 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4346 | `		if( rc == SXERR_ABORT ){` |
|       - |  4347 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4348 | `			return SXERR_ABORT;` |
|       - |  4349 | `		}` |
|     ! 0 |  4350 | `		goto Synchronize;` |
|       - |  4351 | `	}` |
|    3985 |  4352 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4353 | `		pGen->pIn++;` |
|       - |  4354 | `		/* Pass by reference  */` |
|      11 |  4355 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4356 | `	}` |
|       - |  4357 | `	/* Check if the value target is list() */` |
|    3985 |  4358 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4359 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4360 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4361 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4362 | `		 */` |
|       - |  4363 | `		static int iForeachListCnt = 0;` |
|       - |  4364 | `		char zTmp[128];` |
|       - |  4365 | `		sxu32 nLen;` |
|       - |  4366 | `		char *zDup;` |
|      10 |  4367 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4368 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4369 | `		if( zDup == 0 ){` |
|     ! 0 |  4370 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4371 | `			return SXERR_ABORT;` |
|       - |  4372 | `		}` |
|      10 |  4373 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4374 | `		/* Save list() token boundaries */` |
|      10 |  4375 | `		pListStart = pGen->pIn;` |
|       - |  4376 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4377 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4378 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4379 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4380 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4381 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4382 | `				return SXERR_ABORT;` |
|       - |  4383 | `			}` |
|       3 |  4384 | `			goto Synchronize;` |
|       - |  4385 | `		}` |
|       7 |  4386 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4387 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4388 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4389 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4390 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4391 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4392 | `				return SXERR_ABORT;` |
|       - |  4393 | `			}` |
|     ! 0 |  4394 | `			goto Synchronize;` |
|       - |  4395 | `		}` |
|       7 |  4396 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4397 | `		pListEnd = pGen->pIn;` |
|       7 |  4398 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3980 |  4399 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4400 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4401 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4402 | `		 */` |
|       - |  4403 | `		static int iForeachShortListCnt = 0;` |
|       - |  4404 | `		char zTmp[128];` |
|       - |  4405 | `		sxu32 nLen;` |
|       - |  4406 | `		char *zDup;` |
|       9 |  4407 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       9 |  4408 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       9 |  4409 | `		if( zDup == 0 ){` |
|     ! 0 |  4410 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4411 | `			return SXERR_ABORT;` |
|       - |  4412 | `		}` |
|       9 |  4413 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4414 | `		/* Save [...] token boundaries */` |
|       9 |  4415 | `		pListStart = pGen->pIn;` |
|       - |  4416 | `		/* Advance past [...] */` |
|       9 |  4417 | `		pGen->pIn++; /* Jump '[' */` |
|       9 |  4418 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       9 |  4419 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4420 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4421 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4422 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4423 | `				return SXERR_ABORT;` |
|       - |  4424 | `			}` |
|     ! 0 |  4425 | `			goto Synchronize;` |
|       - |  4426 | `		}` |
|       9 |  4427 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       9 |  4428 | `		pListEnd = pGen->pIn;` |
|       9 |  4429 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       5 |  4430 | `	}else{` |
|       - |  4431 | `		/* Compile the expression holding the value name */` |
|    3969 |  4432 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3969 |  4433 | `		if( rc == SXERR_ABORT ){` |
|       - |  4434 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4435 | `			return SXERR_ABORT;` |
|       - |  4436 | `		}` |
|    3969 |  4437 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3969 |  4438 | `		if( pInstr->p3 ){` |
|       - |  4439 | `			/* Record value name */` |
|    3969 |  4440 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1982 |  4441 | `		}` |
|       - |  4442 | `	}` |
|       - |  4443 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3983 |  4444 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4445 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3983 |  4446 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4447 | `	/* Record the first instruction to execute */` |
|    3983 |  4448 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4449 | `	/* Emit the FOREACH_STEP instruction */` |
|    3983 |  4450 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4451 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3983 |  4452 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4453 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3983 |  4454 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4455 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4456 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4457 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4458 | `		 */` |
|      15 |  4459 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4460 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4461 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4462 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4463 | `		 */` |
|      15 |  4464 | `		pSavedIn = pGen->pIn;` |
|      15 |  4465 | `		pSavedEnd = pGen->pEnd;` |
|      15 |  4466 | `		pGen->pIn = pListStart;` |
|      15 |  4467 | `		pGen->pEnd = pListEnd;` |
|      15 |  4468 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       9 |  4469 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       5 |  4470 | `		}else{` |
|       7 |  4471 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4472 | `		}` |
|      15 |  4473 | `		pGen->pIn = pSavedIn;` |
|      15 |  4474 | `		pGen->pEnd = pSavedEnd;` |
|      15 |  4475 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4476 | `			return SXERR_ABORT;` |
|       - |  4477 | `		}` |
|       - |  4478 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      15 |  4479 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       7 |  4480 | `	}` |
|       - |  4481 | `	/* Compile the loop body */` |
|    3983 |  4482 | `	pGen->pIn = &pEnd[1];` |
|    3983 |  4483 | `	pGen->pEnd = pTmp;` |
|    3983 |  4484 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3983 |  4485 | `	if( rc == SXERR_ABORT ){` |
|       - |  4486 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4487 | `		return SXERR_ABORT;` |
|       - |  4488 | `	}` |
|       - |  4489 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3983 |  4490 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4491 | `	/* Fix all jumps now the destination is resolved */` |
|    3983 |  4492 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4493 | `	/* Release the loop block */` |
|    3983 |  4494 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4495 | `	/* Statement successfully compiled */` |
|    3983 |  4496 | `	return SXRET_OK;` |
|       1 |  4497 | `Synchronize:` |
|       - |  4498 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4499 | `	 * compiling this erroneous block.` |
|       - |  4500 | `	 */` |
|       3 |  4501 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4502 | `		pGen->pIn++;` |
|     ! 0 |  4503 | `	}` |
|       3 |  4504 | `	return SXRET_OK;` |
|    1995 |  4505 | `}` |
|       - |  4506 | `/*` |
|       - |  4507 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4508 | ` * According to the PHP language reference` |
|       - |  4509 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4510 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4511 | ` *  that is similar to that of C:` |
|       - |  4512 | ` *  if (expr)` |
|       - |  4513 | ` *   statement` |
|       - |  4514 | ` *  else construct:` |
|       - |  4515 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4516 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4517 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4518 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4519 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4520 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4521 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4522 | ` *  elseif` |
|       - |  4523 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4524 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4525 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4526 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4527 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4528 | ` *   <?php` |
|       - |  4529 | ` *    if ($a > $b) {` |
|       - |  4530 | ` *     echo "a is bigger than b";` |
|       - |  4531 | ` *    } elseif ($a == $b) {` |
|       - |  4532 | ` *     echo "a is equal to b";` |
|       - |  4533 | ` *    } else {` |
|       - |  4534 | ` *     echo "a is smaller than b";` |
|       - |  4535 | ` *    }` |
|       - |  4536 | ` *    ?>` |
|       - |  4537 | ` */` |
|  150314 |  4538 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4539 | `{` |
|  150319 |  4540 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  150319 |  4541 | `	GenBlock *pCondBlock = 0;` |
|       - |  4542 | `	sxu32 nJumpIdx;` |
|       - |  4543 | `	sxu32 nKeyID;` |
|       - |  4544 | `	sxi32 rc;` |
|       - |  4545 | `	/* Jump the 'if' keyword */` |
|  150319 |  4546 | `	pGen->pIn++;` |
|  150319 |  4547 | `	pToken = pGen->pIn;` |
|       - |  4548 | `	/* Create the conditional block */` |
|  150319 |  4549 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  150319 |  4550 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4551 | `		return SXERR_ABORT;` |
|       - |  4552 | `	}` |
|       - |  4553 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   82386 |  4554 | `	for(;;){` |
|  164777 |  4555 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4556 | `			/* Syntax error */` |
|     ! 0 |  4557 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4558 | `				pToken--;` |
|     ! 0 |  4559 | `			}` |
|     ! 0 |  4560 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4561 | `			if( rc == SXERR_ABORT ){` |
|       - |  4562 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4563 | `				return SXERR_ABORT;` |
|       - |  4564 | `			}` |
|     ! 0 |  4565 | `			goto Synchronize;` |
|       - |  4566 | `		}` |
|       - |  4567 | `		/* Jump the left parenthesis '(' */` |
|  164777 |  4568 | `		pToken++;` |
|       - |  4569 | `		/* Delimit the condition */` |
|  164777 |  4570 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  164777 |  4571 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4572 | `			/* Syntax error */` |
|     ! 0 |  4573 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4574 | `				pToken--;` |
|     ! 0 |  4575 | `			}` |
|     ! 0 |  4576 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4577 | `			if( rc == SXERR_ABORT ){` |
|       - |  4578 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4579 | `				return SXERR_ABORT;` |
|       - |  4580 | `			}` |
|     ! 0 |  4581 | `			goto Synchronize;` |
|       - |  4582 | `		}` |
|       - |  4583 | `		/* Swap token streams */` |
|  164777 |  4584 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4585 | `		/* Compile the condition */` |
|  164777 |  4586 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4587 | `		/* Update token stream */` |
|  164777 |  4588 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4589 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4590 | `			pGen->pIn++;` |
|     ! 0 |  4591 | `		}` |
|  164777 |  4592 | `		pGen->pIn  = &pEnd[1];` |
|  164777 |  4593 | `		pGen->pEnd = pTmp;` |
|  164777 |  4594 | `		if( rc == SXERR_ABORT ){` |
|       - |  4595 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4596 | `			return SXERR_ABORT;` |
|       - |  4597 | `		}` |
|       - |  4598 | `		/* Emit the false jump */` |
|  164777 |  4599 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4600 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  164777 |  4601 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4602 | `		/* Compile the body */` |
|  164777 |  4603 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  164777 |  4604 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4605 | `			return SXERR_ABORT;` |
|       - |  4606 | `		}` |
|  164777 |  4607 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   45933 |  4608 | `			break;` |
|       - |  4609 | `		}` |
|       - |  4610 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   72921 |  4611 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   72921 |  4612 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   46941 |  4613 | `			break;` |
|       - |  4614 | `		}` |
|       - |  4615 | `		/* Emit the unconditional jump */` |
|   25985 |  4616 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4617 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   25985 |  4618 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   25985 |  4619 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   18699 |  4620 | `			pToken = &pGen->pIn[1];` |
|   18699 |  4621 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    7224 |  4622 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5766 |  4623 | `					break;` |
|       - |  4624 | `			}` |
|    7177 |  4625 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3586 |  4626 | `		}` |
|   14463 |  4627 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4628 | `		/* Synchronize cursors */` |
|   14463 |  4629 | `		pToken = pGen->pIn;` |
|       - |  4630 | `		/* Fix the false jump */` |
|   14463 |  4631 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4632 | `	} /* For(;;) */` |
|       - |  4633 | `	/* Fix the false jump */` |
|  150319 |  4634 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  150319 |  4635 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   58458 |  4636 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4637 | `			/* Compile the else block */` |
|   11527 |  4638 | `			pGen->pIn++;` |
|   11527 |  4639 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   11527 |  4640 | `			if( rc == SXERR_ABORT ){` |
|       - |  4641 |  |
|     ! 0 |  4642 | `				return SXERR_ABORT;` |
|       - |  4643 | `			}` |
|    5761 |  4644 | `	}` |
|  150319 |  4645 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4646 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  150319 |  4647 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4648 | `	/* Release the conditional block */` |
|  150319 |  4649 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4650 | `	/* Statement successfully compiled */` |
|  150319 |  4651 | `	return SXRET_OK;` |
|     ! 0 |  4652 | `Synchronize:` |
|       - |  4653 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4654 | `	 */` |
|     ! 0 |  4655 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4656 | `		pGen->pIn++;` |
|     ! 0 |  4657 | `	}` |
|     ! 0 |  4658 | `	return SXRET_OK;` |
|   75162 |  4659 | `}` |
|       - |  4660 | `/*` |
|       - |  4661 | ` * Compile the global construct.` |
|       - |  4662 | ` * According to the PHP language reference` |
|       - |  4663 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4664 | ` *  to be used in that function.` |
|       - |  4665 | ` *  Example #1 Using global` |
|       - |  4666 | ` *  <?php` |
|       - |  4667 | ` *   $a = 1;` |
|       - |  4668 | ` *   $b = 2;` |
|       - |  4669 | ` *   function Sum()` |
|       - |  4670 | ` *   {` |
|       - |  4671 | ` *    global $a, $b;` |
|       - |  4672 | ` *    $b = $a + $b;` |
|       - |  4673 | ` *   }` |
|       - |  4674 | ` *   Sum();` |
|       - |  4675 | ` *   echo $b;` |
|       - |  4676 | ` *  ?>` |
|       - |  4677 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4678 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4679 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4680 | ` */` |
|      36 |  4681 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 |  4682 | `{` |
|      41 |  4683 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4684 | `	sxi32 nExpr;` |
|       - |  4685 | `	sxi32 rc;` |
|       - |  4686 | `	/* Jump the 'global' keyword */` |
|      41 |  4687 | `	pGen->pIn++;` |
|      41 |  4688 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4689 | `		/* Nothing to process */` |
|     ! 0 |  4690 | `		return SXRET_OK;` |
|       - |  4691 | `	}` |
|      41 |  4692 | `	pTmp = pGen->pEnd;` |
|      41 |  4693 | `	nExpr = 0;` |
|      87 |  4694 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      51 |  4695 | `		if( pGen->pIn < pNext ){` |
|      51 |  4696 | `			pGen->pEnd = pNext;` |
|      51 |  4697 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4698 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4699 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4700 | `					return SXERR_ABORT;` |
|       - |  4701 | `				}` |
|     ! 0 |  4702 | `			}else{` |
|      51 |  4703 | `				pGen->pIn++;` |
|      51 |  4704 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4705 | `					/* Emit a warning */` |
|     ! 0 |  4706 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4707 | `				}else{` |
|      51 |  4708 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 |  4709 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4710 | `						return SXERR_ABORT;` |
|      51 |  4711 | `					}else if(rc != SXERR_EMPTY ){` |
|      51 |  4712 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      51 |  4713 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4714 | `							/* Variable name, not a constant */` |
|      51 |  4715 | `							pLast->iP1 = 0;` |
|      23 |  4716 | `						}` |
|      51 |  4717 | `						nExpr++;` |
|      23 |  4718 | `					}` |
|       - |  4719 | `				}` |
|       - |  4720 | `			}` |
|      23 |  4721 | `		}` |
|       - |  4722 | `		/* Next expression in the stream */` |
|      51 |  4723 | `		pGen->pIn = pNext;` |
|       - |  4724 | `		/* Jump trailing commas */` |
|      61 |  4725 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  4726 | `			pGen->pIn++;` |
|       5 |  4727 | `		}` |
|       5 |  4728 | `	}` |
|       - |  4729 | `	/* Restore token stream */` |
|      41 |  4730 | `	pGen->pEnd = pTmp;` |
|      41 |  4731 | `	if( nExpr > 0 ){` |
|       - |  4732 | `		/* Emit the uplink instruction */` |
|      41 |  4733 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  4734 | `	}` |
|      41 |  4735 | `	return SXRET_OK;` |
|      23 |  4736 | `}` |
|       - |  4737 | `/*` |
|       - |  4738 | ` * Compile the return statement.` |
|       - |  4739 | ` * According to the PHP language reference` |
|       - |  4740 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4741 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4742 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4743 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4744 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4745 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4746 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4747 | ` *  from within the main script file, then script execution end.` |
|       - |  4748 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4749 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4750 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4751 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4752 | ` */` |
|  238122 |  4753 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4754 | `{` |
|  238127 |  4755 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4756 | `	sxi32 rc;` |
|  238127 |  4757 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  238127 |  4758 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|       - |  4759 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|       - |  4760 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|       - |  4761 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|       - |  4762 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|       - |  4763 | `	 * normally below so token processing stays consistent. */` |
|  613211 |  4764 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  375089 |  4765 | `		pFuncBlock = pFuncBlock->pParent;` |
|       5 |  4766 | `	}` |
|  238122 |  4767 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  238095 |  4768 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|       3 |  4769 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  4770 | `			"A never-returning function must not return");` |
|       3 |  4771 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4772 | `			return SXERR_ABORT;` |
|       - |  4773 | `		}` |
|       1 |  4774 | `	}` |
|       - |  4775 | `	/* Jump the 'return' keyword */` |
|  238127 |  4776 | `	pGen->pIn++;` |
|  238127 |  4777 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4778 | `		/* Compile the expression */` |
|  238097 |  4779 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  238097 |  4780 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4781 | `			return SXERR_ABORT;` |
|  238097 |  4782 | `		}else if(rc != SXERR_EMPTY ){` |
|  238097 |  4783 | `			nRet = 1;` |
|  119046 |  4784 | `		}` |
|  119046 |  4785 | `	}` |
|       - |  4786 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4787 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4788 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4789 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4790 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  238127 |  4791 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  238127 |  4792 | `	return SXRET_OK;` |
|  119066 |  4793 | `}` |
|       - |  4794 | `/*` |
|       - |  4795 | ` * Compile a yield expression.` |
|       - |  4796 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4797 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4798 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4799 | ` */` |
|     170 |  4800 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4801 | `{` |
|       - |  4802 | `	SyToken *pTmp, *pSplit;` |
|     175 |  4803 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     175 |  4804 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4805 | `	sxi32 rc;` |
|      85 |  4806 | `	(void)iCompileFlag;` |
|       - |  4807 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     175 |  4808 | `	pGen->pIn++;` |
|       - |  4809 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4810 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - |  4811 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - |  4812 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - |  4813 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     170 |  4814 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     102 |  4815 | `		&& pGen->pIn->sData.nByte == 4` |
|      41 |  4816 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      40 |  4817 | `		pGen->pIn++; /* Skip 'from' */` |
|      40 |  4818 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      40 |  4819 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4820 | `			return SXERR_ABORT;` |
|       - |  4821 | `		}` |
|      40 |  4822 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  4823 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 |  4824 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - |  4825 | `				"Missing expression after 'yield from'");` |
|     ! 0 |  4826 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4827 | `				return SXERR_ABORT;` |
|       - |  4828 | `			}` |
|     ! 0 |  4829 | `		}` |
|      40 |  4830 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      40 |  4831 | `		return SXRET_OK;` |
|       - |  4832 | `	}` |
|     139 |  4833 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4834 | `		/* Bare yield — no value */` |
|       3 |  4835 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       3 |  4836 | `		return SXRET_OK;` |
|       - |  4837 | `	}` |
|       - |  4838 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     137 |  4839 | `	pSplit = 0;` |
|       - |  4840 | `	{` |
|     137 |  4841 | `		SyToken *pCur = pGen->pIn;` |
|     137 |  4842 | `		sxi32 nNest = 0;` |
|     285 |  4843 | `		while( pCur < pGen->pEnd ){` |
|     167 |  4844 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4845 | `				nNest++;` |
|     167 |  4846 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4847 | `				nNest--;` |
|     167 |  4848 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4849 | `				pSplit = pCur;` |
|      16 |  4850 | `				break;` |
|       - |  4851 | `			}` |
|     153 |  4852 | `			pCur++;` |
|       5 |  4853 | `		}` |
|       - |  4854 | `	}` |
|     137 |  4855 | `	pTmp = pGen->pEnd;` |
|     137 |  4856 | `	if( pSplit ){` |
|       - |  4857 | `		/* yield $key => $value */` |
|      16 |  4858 | `		pGen->pEnd = pSplit;` |
|      16 |  4859 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4860 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4861 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  4862 | `		pGen->pEnd = pTmp;` |
|      16 |  4863 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4864 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4865 | `		iP1 = 1;` |
|      16 |  4866 | `		iP2 = 1;` |
|       9 |  4867 | `	}else{` |
|       - |  4868 | `		/* yield $value */` |
|     123 |  4869 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     123 |  4870 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     123 |  4871 | `		if( rc != SXERR_EMPTY ){` |
|     123 |  4872 | `			iP1 = 1;` |
|      59 |  4873 | `		}` |
|       - |  4874 | `	}` |
|     137 |  4875 | `	pGen->pEnd = pTmp;` |
|     137 |  4876 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     137 |  4877 | `	return SXRET_OK;` |
|      90 |  4878 | `}` |
|       - |  4879 | `/*` |
|       - |  4880 | ` * Compile the die/exit language construct.` |
|       - |  4881 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4882 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4883 | ` */` |
|     120 |  4884 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  4885 | `{` |
|     125 |  4886 | `	sxi32 nExpr = 0;` |
|       - |  4887 | `	sxi32 rc;` |
|       - |  4888 | `	/* Jump the die/exit keyword */` |
|     125 |  4889 | `	pGen->pIn++;` |
|     125 |  4890 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4891 | `		/* Compile the expression */` |
|     125 |  4892 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     125 |  4893 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4894 | `			return SXERR_ABORT;` |
|     125 |  4895 | `		}else if(rc != SXERR_EMPTY ){` |
|     125 |  4896 | `			nExpr = 1;` |
|      60 |  4897 | `		}` |
|      60 |  4898 | `	}` |
|       - |  4899 | `	/* Emit the HALT instruction */` |
|     125 |  4900 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     125 |  4901 | `	return SXRET_OK;` |
|      65 |  4902 | `}` |
|       - |  4903 | `/*` |
|       - |  4904 | ` * Compile the 'echo' language construct.` |
|       - |  4905 | ` */` |
|   14758 |  4906 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  4907 | `{` |
|   14763 |  4908 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4909 | `	sxi32 rc;` |
|       - |  4910 | `	/* Jump the 'echo' keyword */` |
|   14763 |  4911 | `	pGen->pIn++;` |
|       - |  4912 | `	/* Compile arguments one after one */` |
|   14763 |  4913 | `	pTmp = pGen->pEnd;` |
|   32671 |  4914 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   17913 |  4915 | `		if( pGen->pIn < pNext ){` |
|   17913 |  4916 | `			pGen->pEnd = pNext;` |
|   17913 |  4917 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   17913 |  4918 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4919 | `				return SXERR_ABORT;` |
|   17913 |  4920 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4921 | `				/* Emit the consume instruction */` |
|   17889 |  4922 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    8942 |  4923 | `			}` |
|    8954 |  4924 | `		}` |
|       - |  4925 | `		/* Jump trailing commas */` |
|   21063 |  4926 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    3155 |  4927 | `			pNext++;` |
|       5 |  4928 | `		}` |
|   17913 |  4929 | `		pGen->pIn = pNext;` |
|       5 |  4930 | `	}` |
|       - |  4931 | `	/* Restore token stream */` |
|   14763 |  4932 | `	pGen->pEnd = pTmp;` |
|   14763 |  4933 | `	return SXRET_OK;` |
|    7384 |  4934 | `}` |
|       - |  4935 | `/*` |
|       - |  4936 | ` * Compile the static statement.` |
|       - |  4937 | ` * According to the PHP language reference` |
|       - |  4938 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4939 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4940 | ` *  when program execution leaves this scope.` |
|       - |  4941 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4942 | ` * Symisc eXtension.` |
|       - |  4943 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4944 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4945 | ` *  Example` |
|       - |  4946 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4947 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4948 | ` */` |
|       6 |  4949 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       2 |  4950 | `{` |
|       - |  4951 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4952 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4953 | `	GenBlock *pBlock;` |
|       - |  4954 | `	SyString *pName;` |
|       - |  4955 | `	char *zDup;` |
|       - |  4956 | `	sxu32 nLine;` |
|       - |  4957 | `	sxi32 rc;` |
|       - |  4958 | `	/* Jump the static keyword */` |
|       8 |  4959 | `	nLine = pGen->pIn->nLine;` |
|       8 |  4960 | `	pGen->pIn++;` |
|       - |  4961 | `	/* Extract the enclosing function if any */` |
|       8 |  4962 | `	pBlock = pGen->pCurrent;` |
|      14 |  4963 | `	while( pBlock ){` |
|      14 |  4964 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       8 |  4965 | `			break;` |
|       - |  4966 | `		}` |
|       - |  4967 | `		/* Point to the upper block */` |
|       8 |  4968 | `		pBlock = pBlock->pParent;` |
|       2 |  4969 | `	}` |
|       8 |  4970 | `	if( pBlock == 0 ){` |
|       - |  4971 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4972 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4973 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4974 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4975 | `				return SXERR_ABORT;` |
|       - |  4976 | `			}` |
|     ! 0 |  4977 | `			goto Synchronize;` |
|       - |  4978 | `		}` |
|       - |  4979 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4980 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4981 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4982 | `			return SXERR_ABORT;` |
|     ! 0 |  4983 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4984 | `			/* Emit the POP instruction */` |
|     ! 0 |  4985 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4986 | `		}` |
|     ! 0 |  4987 | `		return SXRET_OK;` |
|       - |  4988 | `	}` |
|       8 |  4989 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4990 | `	/* Make sure we are dealing with a valid statement */` |
|       8 |  4991 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       4 |  4992 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4993 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4994 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4995 | `				return SXERR_ABORT;` |
|       - |  4996 | `			}` |
|       3 |  4997 | `			goto Synchronize;` |
|       - |  4998 | `	}` |
|       5 |  4999 | `	pGen->pIn++;` |
|       - |  5000 | `	/* Extract variable name */` |
|       5 |  5001 | `	pName = &pGen->pIn->sData;` |
|       5 |  5002 | `	pGen->pIn++; /* Jump the var name */` |
|       5 |  5003 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  5004 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  5005 | `		goto Synchronize;` |
|       - |  5006 | `	}` |
|       - |  5007 | `	/* Initialize the structure describing the static variable */` |
|       5 |  5008 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       5 |  5009 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  5010 | `	/* Duplicate variable name */` |
|       5 |  5011 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       5 |  5012 | `	if( zDup == 0 ){` |
|     ! 0 |  5013 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  5014 | `		return SXERR_ABORT;` |
|       - |  5015 | `	}` |
|       5 |  5016 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  5017 | `	/* Check if we have an expression to compile */` |
|       5 |  5018 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  5019 | `		SySet *pInstrContainer;` |
|       - |  5020 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  5021 | `		 * Static variable can take any complex expression including function` |
|       - |  5022 | `		 * call as their initialization value.` |
|       - |  5023 | `		 * Example:` |
|       - |  5024 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  5025 | `		 */` |
|       5 |  5026 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  5027 | `		/* Swap bytecode container */` |
|       5 |  5028 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       5 |  5029 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  5030 | `		/* Compile the expression */` |
|       5 |  5031 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5032 | `		/* Emit the done instruction */` |
|       5 |  5033 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  5034 | `		/* Restore default bytecode container */` |
|       5 |  5035 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       2 |  5036 | `	}` |
|       - |  5037 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       5 |  5038 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       5 |  5039 | `	return SXRET_OK;` |
|       1 |  5040 | `Synchronize:` |
|       - |  5041 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  5042 | `	 * statement.` |
|       - |  5043 | `	 */` |
|       5 |  5044 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  5045 | `		pGen->pIn++;` |
|       1 |  5046 | `	}` |
|       3 |  5047 | `	return SXRET_OK;` |
|       5 |  5048 | `}` |
|       - |  5049 | `/*` |
|       - |  5050 | ` * Compile the var statement.` |
|       - |  5051 | ` * Symisc Extension:` |
|       - |  5052 | ` *      var statement can be used outside of a class definition.` |
|       - |  5053 | ` */` |
|       4 |  5054 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  5055 | `{` |
|       - |  5056 | `	sxu32 nLine;` |
|       - |  5057 | `	sxi32 rc;` |
|       5 |  5058 | `	nLine = pGen->pIn->nLine;` |
|       - |  5059 | `	/* Jump the 'var' keyword */` |
|       5 |  5060 | `	pGen->pIn++;` |
|       5 |  5061 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  5062 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  5063 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  5064 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  5065 | `			pGen->pIn++;` |
|     ! 0 |  5066 | `		}` |
|     ! 0 |  5067 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5068 | `			return SXERR_ABORT;` |
|       - |  5069 | `		}` |
|     ! 0 |  5070 | `	}else{` |
|       - |  5071 | `		/* Compile the expression */` |
|       5 |  5072 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  5073 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5074 | `			return SXERR_ABORT;` |
|       5 |  5075 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  5076 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  5077 | `		}` |
|       - |  5078 | `	}` |
|       5 |  5079 | `	return SXRET_OK;` |
|       3 |  5080 | `}` |
|       - |  5081 | `/*` |
|       - |  5082 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  5083 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  5084 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  5085 | ` */` |
|       - |  5086 | `/*` |
|       - |  5087 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  5088 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  5089 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  5090 | ` * qualified name and updates the instruction's operand index.` |
|       - |  5091 | ` *` |
|       - |  5092 | ` * Resolution order:` |
|       - |  5093 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  5094 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  5095 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  5096 | ` *` |
|       - |  5097 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  5098 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  5099 | ` * Returns the (possibly new) literal index.` |
|       - |  5100 | ` */` |
|  462698 |  5101 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  5102 | `{` |
|       - |  5103 | `	ph7_value *pLit;` |
|       - |  5104 | `	const char *zLit;` |
|       - |  5105 | `	SyString sQualified;` |
|       - |  5106 | `	sxu32 nLit;` |
|       - |  5107 | `	sxu32 k;` |
|       - |  5108 | `	sxu32 nNewIdx;` |
|       - |  5109 | `	int hasNsSep;` |
|       - |  5110 | `	SyHashEntry *pImport;` |
|       - |  5111 | `	ph7_value *pNew;` |
|  462703 |  5112 | `	if( pFromImport ){` |
|  442821 |  5113 | `		*pFromImport = 0;` |
|  221408 |  5114 | `	}` |
|  462703 |  5115 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  462703 |  5116 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  5117 | `		return nOrigIdx;` |
|       - |  5118 | `	}` |
|  462703 |  5119 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  462703 |  5120 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  5121 | `	/* Skip if already qualified (contains backslash) */` |
|  462703 |  5122 | `	hasNsSep = 0;` |
| 5110011 |  5123 | `	for( k = 0; k < nLit; k++ ){` |
| 4647321 |  5124 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2323659 |  5125 | `	}` |
|  462703 |  5126 | `	if( hasNsSep ){` |
|      11 |  5127 | `		return nOrigIdx;` |
|       - |  5128 | `	}` |
|       - |  5129 | `	/* Check use imports first (works even outside namespaces) */` |
|  462695 |  5130 | `	SyBlobReset(&pGen->sWorker);` |
|  462695 |  5131 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  462695 |  5132 | `	if( pImport ){` |
|      41 |  5133 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  5134 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  5135 | `		if( pFromImport ){` |
|      18 |  5136 | `			*pFromImport = 1;` |
|       8 |  5137 | `		}` |
|      23 |  5138 | `	}else{` |
|  462659 |  5139 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  462569 |  5140 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  5141 | `		}` |
|       - |  5142 | `		/* Prepend current namespace */` |
|      95 |  5143 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  5144 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  5145 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  5146 | `	}` |
|       - |  5147 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  5148 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  5149 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  5150 | `		return nNewIdx; /* Already interned */` |
|       - |  5151 | `	}` |
|      79 |  5152 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  5153 | `	if( pNew == 0 ){` |
|     ! 0 |  5154 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  5155 | `	}` |
|      79 |  5156 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  5157 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  5158 | `	return nNewIdx;` |
|  231354 |  5159 | `}` |
|       - |  5160 | `/*` |
|       - |  5161 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  5162 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  5163 | ` */` |
|   97778 |  5164 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5165 | `{` |
|       - |  5166 | `	SyHashEntry *pImport;` |
|       - |  5167 | `	/* Check use imports first */` |
|   97783 |  5168 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   97783 |  5169 | `	if( pImport ){` |
|      15 |  5170 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      15 |  5171 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      15 |  5172 | `		return;` |
|       - |  5173 | `	}` |
|       - |  5174 | `	/* Prepend current namespace if active */` |
|   97771 |  5175 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  5176 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  5177 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  5178 | `	}` |
|   97771 |  5179 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   48894 |  5180 | `}` |
|       - |  5181 | `/*` |
|       - |  5182 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5183 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5184 | ` * The caller must release pOut when done.` |
|       - |  5185 | ` */` |
|  141266 |  5186 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5187 | `{` |
|  141271 |  5188 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      63 |  5189 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      63 |  5190 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5191 | `	}` |
|  141271 |  5192 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  141271 |  5193 | `}` |
|       - |  5194 | `/*` |
|       - |  5195 | ` * Compile a namespace statement` |
|       - |  5196 | ` * According to the PHP language reference manual` |
|       - |  5197 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5198 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5199 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5200 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5201 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5202 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5203 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5204 | ` *  programming world.` |
|       - |  5205 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5206 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5207 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5208 | ` *  classes/functions/constants.` |
|       - |  5209 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5210 | ` *  readability of source code.` |
|       - |  5211 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5212 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5213 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5214 | ` *       class MyClass {}` |
|       - |  5215 | ` *       function myfunction() {}` |
|       - |  5216 | ` *       const MYCONST = 1;` |
|       - |  5217 | ` *       $a = new MyClass;` |
|       - |  5218 | ` *       $c = new \my\name\MyClass;` |
|       - |  5219 | ` *       $a = strlen('hi');` |
|       - |  5220 | ` *       $d = namespace\MYCONST;` |
|       - |  5221 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5222 | ` *       echo constant($d);` |
|       - |  5223 | ` * NOTE` |
|       - |  5224 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5225 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5226 | ` */` |
|       - |  5227 | `/*` |
|       - |  5228 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5229 | ` */` |
|      14 |  5230 | `static const char * TokenTypeName(sxu32 nType)` |
|       4 |  5231 | `{` |
|      18 |  5232 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      11 |  5233 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      11 |  5234 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      11 |  5235 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      11 |  5236 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      11 |  5237 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5238 | `	return "token";` |
|      11 |  5239 | `}` |
|     106 |  5240 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5241 | `{` |
|       - |  5242 | `	sxu32 nLine;` |
|       - |  5243 | `	sxi32 rc;` |
|     111 |  5244 | `	nLine = pGen->pIn->nLine;` |
|     111 |  5245 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5246 | `	/* Reset namespace and clear previous use imports */` |
|     111 |  5247 | `	SyBlobReset(&pGen->sNamespace);` |
|     111 |  5248 | `	SyHashRelease(&pGen->hUseImports);` |
|     111 |  5249 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5250 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     111 |  5251 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5252 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     111 |  5253 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5254 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5255 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5256 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5257 | `		return SXRET_OK;` |
|       - |  5258 | `	}` |
|     111 |  5259 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5260 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5261 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5262 | `		return SXRET_OK;` |
|       - |  5263 | `	}` |
|     111 |  5264 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5265 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5266 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5267 | `		return SXRET_OK;` |
|       - |  5268 | `	}` |
|       - |  5269 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     259 |  5270 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     153 |  5271 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5272 | `			/* Append backslash separator */` |
|      26 |  5273 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      26 |  5274 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5275 | `			}` |
|      15 |  5276 | `		}else{` |
|       - |  5277 | `			/* Append identifier */` |
|     131 |  5278 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5279 | `		}` |
|     153 |  5280 | `		pGen->pIn++;` |
|       5 |  5281 | `	}` |
|       - |  5282 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5283 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5284 | `	{` |
|     111 |  5285 | `		char *zNsDup = 0;` |
|     111 |  5286 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     161 |  5287 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     104 |  5288 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      52 |  5289 | `		}` |
|     111 |  5290 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5291 | `	}` |
|     111 |  5292 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5293 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5294 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5295 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5296 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5297 | `			return SXERR_ABORT;` |
|       - |  5298 | `		}` |
|       2 |  5299 | `	}` |
|     111 |  5300 | `	return SXRET_OK;` |
|      58 |  5301 | `}` |
|       - |  5302 | `/*` |
|       - |  5303 | ` * Compile the 'use' statement` |
|       - |  5304 | ` * According to the PHP language reference manual` |
|       - |  5305 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5306 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5307 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5308 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5309 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5310 | ` *  a function or constant is not supported.` |
|       - |  5311 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5312 | ` * NOTE` |
|       - |  5313 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5314 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5315 | ` */` |
|      68 |  5316 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5317 | `{` |
|       - |  5318 | `	sxu32 nLine;` |
|       - |  5319 | `	sxi32 rc;` |
|       - |  5320 | `	SyBlob sPath;` |
|       - |  5321 | `	SyString sAlias;` |
|       - |  5322 | `	SyToken *pLast;` |
|       - |  5323 | `	char *zDup;` |
|       - |  5324 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5325 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5326 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      73 |  5327 | `	nLine = pGen->pIn->nLine;` |
|      73 |  5328 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5329 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      73 |  5330 | `	iUseType = 0;` |
|      73 |  5331 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5332 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5333 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5334 | `			iUseType = 1;` |
|      16 |  5335 | `			pGen->pIn++;` |
|      23 |  5336 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5337 | `			iUseType = 2;` |
|      16 |  5338 | `			pGen->pIn++;` |
|       7 |  5339 | `		}` |
|      14 |  5340 | `	}` |
|       - |  5341 | `	/* Select target hash tables based on import type */` |
|      73 |  5342 | `	switch( iUseType ){` |
|       7 |  5343 | `		case 1:` |
|      16 |  5344 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5345 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5346 | `			break;` |
|       7 |  5347 | `		case 2:` |
|      16 |  5348 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5349 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5350 | `			break;` |
|      20 |  5351 | `		default:` |
|      45 |  5352 | `			pGenHash = &pGen->hUseImports;` |
|      45 |  5353 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5354 | `			break;` |
|       - |  5355 | `	}` |
|      73 |  5356 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5357 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5358 | `	for(;;){` |
|      75 |  5359 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5360 | `			break;` |
|       - |  5361 | `		}` |
|      75 |  5362 | `		SyBlobReset(&sPath);` |
|      75 |  5363 | `		pLast = 0;` |
|       - |  5364 | `		/* Collect the full namespace path */` |
|     261 |  5365 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     191 |  5366 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     131 |  5367 | `				pLast = pGen->pIn;` |
|     131 |  5368 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5369 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5370 | `				}` |
|     131 |  5371 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5372 | `			}` |
|     191 |  5373 | `			pGen->pIn++;` |
|       5 |  5374 | `		}` |
|      75 |  5375 | `		if( pLast == 0 ){` |
|       - |  5376 | `			/* Empty path */` |
|       6 |  5377 | `			break;` |
|       - |  5378 | `		}` |
|       - |  5379 | `		/* Default alias is the last component of the path */` |
|      71 |  5380 | `		sAlias = pLast->sData;` |
|       - |  5381 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5382 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      46 |  5383 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 |  5384 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 |  5385 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 |  5386 | `				sAlias = pGen->pIn->sData;` |
|      18 |  5387 | `				pGen->pIn++;` |
|       8 |  5388 | `			}` |
|       8 |  5389 | `		}` |
|       - |  5390 | `		/* Check for duplicate import alias (per-type) */` |
|      71 |  5391 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5392 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5393 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5394 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5395 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5396 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5397 | `				return SXERR_ABORT;` |
|       - |  5398 | `			}` |
|       2 |  5399 | `		}` |
|       - |  5400 | `		/* Register the import: alias -> FQN.` |
|       - |  5401 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5402 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5403 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     104 |  5404 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5405 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      71 |  5406 | `		if( zDup ){` |
|      71 |  5407 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      71 |  5408 | `			if( pVmHash ){` |
|       - |  5409 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5410 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      43 |  5411 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      43 |  5412 | `				if( zAliasDup ){` |
|      43 |  5413 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5414 | `				}` |
|      19 |  5415 | `			}` |
|      71 |  5416 | `			if( iUseType == 2 ){` |
|       - |  5417 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5418 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5419 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5420 | `				if( zAliasDup ){` |
|       - |  5421 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5422 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5423 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5424 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5425 | `					if( azPair ){` |
|      16 |  5426 | `						azPair[0] = zAliasDup;` |
|      16 |  5427 | `						azPair[1] = zDup;` |
|      16 |  5428 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5429 | `					}` |
|       7 |  5430 | `				}` |
|       7 |  5431 | `			}` |
|      33 |  5432 | `		}` |
|       - |  5433 | `		/* Check for comma (multiple use declarations) */` |
|      71 |  5434 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5435 | `			pGen->pIn++;` |
|       2 |  5436 | `		}else{` |
|      37 |  5437 | `			break;` |
|       - |  5438 | `		}` |
|       1 |  5439 | `	}` |
|      73 |  5440 | `	SyBlobRelease(&sPath);` |
|      73 |  5441 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5442 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5443 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5444 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5445 | `			return SXERR_ABORT;` |
|       - |  5446 | `		}` |
|       1 |  5447 | `	}` |
|      73 |  5448 | `	return SXRET_OK;` |
|      39 |  5449 | `}` |
|       - |  5450 | `/*` |
|       - |  5451 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5452 | ` *` |
|       - |  5453 | ` * According to the PHP language reference manual.` |
|       - |  5454 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5455 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5456 | ` *  declare (directive)` |
|       - |  5457 | ` *   statement` |
|       - |  5458 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5459 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5460 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5461 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5462 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5463 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5464 | ` * <?php` |
|       - |  5465 | ` * // these are the same:` |
|       - |  5466 | ` * // you can use this:` |
|       - |  5467 | ` * declare(ticks=1) {` |
|       - |  5468 | ` *   // entire script here` |
|       - |  5469 | ` * }` |
|       - |  5470 | ` * // or you can use this:` |
|       - |  5471 | ` * declare(ticks=1);` |
|       - |  5472 | ` * // entire script here` |
|       - |  5473 | ` * ?>` |
|       - |  5474 | ` *` |
|       - |  5475 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5476 | ` */` |
|       - |  5477 | `/*` |
|       - |  5478 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5479 | ` */` |
|      68 |  5480 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5481 | `{` |
|     103 |  5482 | `	return SyStringLength(pName) == nWant` |
|      68 |  5483 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5484 | `}` |
|       - |  5485 |  |
|      40 |  5486 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5487 | `{` |
|      45 |  5488 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      45 |  5489 | `	SyToken *pBodyEnd = 0;` |
|       - |  5490 | `	SyToken *pBodyStart;` |
|       - |  5491 | `	SyToken *pCursor;` |
|       - |  5492 | `	int bHasStrictTypes;` |
|       - |  5493 | `	int bBlockForm;` |
|       - |  5494 | `	int bPlacementOk;` |
|       - |  5495 | `	sxi32 rc;` |
|      45 |  5496 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      45 |  5497 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 |  5498 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       6 |  5499 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5500 | `			return SXERR_ABORT;` |
|       - |  5501 | `		}` |
|       6 |  5502 | `		goto Synchro;` |
|       - |  5503 | `	}` |
|      41 |  5504 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      41 |  5505 | `	pBodyStart = pGen->pIn;` |
|       - |  5506 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      41 |  5507 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      41 |  5508 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5509 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5510 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5511 | `			return SXERR_ABORT;` |
|       - |  5512 | `		}` |
|     ! 0 |  5513 | `		return SXRET_OK;` |
|       - |  5514 | `	}` |
|       - |  5515 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5516 | `	 * now delimits the comma-separated directive list. */` |
|      41 |  5517 | `	pGen->pIn = &pBodyEnd[1];` |
|      41 |  5518 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5519 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5520 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5521 | `			return SXERR_ABORT;` |
|       - |  5522 | `		}` |
|     ! 0 |  5523 | `	}` |
|      41 |  5524 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      41 |  5525 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      41 |  5526 | `	bHasStrictTypes = 0;` |
|       - |  5527 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5528 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5529 | `	 * directive appears anywhere in the list, before validating values. */` |
|      41 |  5530 | `	pCursor = pBodyStart;` |
|      53 |  5531 | `	while( pCursor < pBodyEnd ){` |
|      49 |  5532 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      41 |  5533 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      37 |  5534 | `				bHasStrictTypes = 1;` |
|      37 |  5535 | `				break;` |
|       - |  5536 | `			}` |
|       2 |  5537 | `		}` |
|      14 |  5538 | `		pCursor++;` |
|       2 |  5539 | `	}` |
|      41 |  5540 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5541 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5542 | `			"strict_types declaration must not use block mode");` |
|       3 |  5543 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5544 | `		return SXRET_OK;` |
|       - |  5545 | `	}` |
|      39 |  5546 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5547 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5548 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5549 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5550 | `		return SXRET_OK;` |
|       - |  5551 | `	}` |
|       - |  5552 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      35 |  5553 | `	pCursor = pBodyStart;` |
|      65 |  5554 | `	while( pCursor < pBodyEnd ){` |
|       - |  5555 | `		SyToken *pNameTok;` |
|       - |  5556 | `		SyToken *pEqTok;` |
|       - |  5557 | `		SyToken *pValTok;` |
|       - |  5558 | `		SyString *pDirName;` |
|       - |  5559 | `		int bIsStrict;` |
|       - |  5560 | `		int iStrictValue;` |
|      37 |  5561 | `		pNameTok = pCursor;` |
|      37 |  5562 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5563 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5564 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5565 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5566 | `			return SXRET_OK;` |
|       - |  5567 | `		}` |
|      37 |  5568 | `		pEqTok = pNameTok + 1;` |
|      37 |  5569 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5570 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5571 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5572 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5573 | `			return SXRET_OK;` |
|       - |  5574 | `		}` |
|      37 |  5575 | `		pValTok = pEqTok + 1;` |
|      37 |  5576 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5577 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5578 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5579 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5580 | `			return SXRET_OK;` |
|       - |  5581 | `		}` |
|      37 |  5582 | `		pDirName = &pNameTok->sData;` |
|      37 |  5583 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      37 |  5584 | `		if( bIsStrict ){` |
|       - |  5585 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5586 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      33 |  5587 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5588 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5589 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5590 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5591 | `				return SXRET_OK;` |
|       - |  5592 | `			}` |
|      33 |  5593 | `			iStrictValue = -1;` |
|      33 |  5594 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      33 |  5595 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      33 |  5596 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      33 |  5597 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      31 |  5598 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      14 |  5599 | `			}` |
|      33 |  5600 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5601 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5602 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5603 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5604 | `				return SXRET_OK;` |
|       - |  5605 | `			}` |
|      30 |  5606 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      17 |  5607 | `		}else{` |
|       - |  5608 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5609 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5610 | `			 * behavior don't regress. */` |
|       8 |  5611 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5612 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5613 | `				ph7_lib_version()` |
|       - |  5614 | `				);` |
|       - |  5615 | `		}` |
|      34 |  5616 | `		pCursor = pValTok + 1;` |
|       - |  5617 | `		/* Consume separating comma (or end). */` |
|      34 |  5618 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5619 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5620 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5621 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5622 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5623 | `				return SXRET_OK;` |
|       - |  5624 | `			}` |
|       3 |  5625 | `			pCursor++;` |
|       1 |  5626 | `		}` |
|       4 |  5627 | `	}` |
|       - |  5628 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5629 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5630 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      32 |  5631 | `	return SXRET_OK;` |
|       2 |  5632 | `Synchro:` |
|       - |  5633 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 |  5634 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 |  5635 | `		pGen->pIn++;` |
|       2 |  5636 | `	}` |
|       6 |  5637 | `	return SXRET_OK;` |
|      25 |  5638 | `}` |
|       - |  5639 | `/*` |
|       - |  5640 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5641 | ` * as follows:` |
|       - |  5642 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5643 | ` * {` |
|       - |  5644 | ` *   return "Making a cup of $type.\n";` |
|       - |  5645 | ` * }` |
|       - |  5646 | ` * Symisc eXtension.` |
|       - |  5647 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5648 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5649 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5650 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5651 | ` *      {` |
|       - |  5652 | ` *       var_dump($a);` |
|       - |  5653 | ` *      }` |
|       - |  5654 | ` *     //call test without args` |
|       - |  5655 | ` *      test();` |
|       - |  5656 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5657 | ` *      Example:` |
|       - |  5658 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5659 | ` * 3 -) Function overloading!!` |
|       - |  5660 | ` *      Example:` |
|       - |  5661 | ` *      function foo($a) {` |
|       - |  5662 | ` *   	  return $a.PHP_EOL;` |
|       - |  5663 | ` *	    }` |
|       - |  5664 | ` *	    function foo($a, $b) {` |
|       - |  5665 | ` *   	  return $a + $b;` |
|       - |  5666 | ` *	    }` |
|       - |  5667 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5668 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5669 | ` *      // Same arg` |
|       - |  5670 | ` *	   function foo(string $a)` |
|       - |  5671 | ` *	   {` |
|       - |  5672 | ` *	     echo "a is a string\n";` |
|       - |  5673 | ` *	     var_dump($a);` |
|       - |  5674 | ` *	   }` |
|       - |  5675 | ` *	  function foo(int $a)` |
|       - |  5676 | ` *	  {` |
|       - |  5677 | ` *	    echo "a is integer\n";` |
|       - |  5678 | ` *	    var_dump($a);` |
|       - |  5679 | ` *	  }` |
|       - |  5680 | ` *	  function foo(array $a)` |
|       - |  5681 | ` *	  {` |
|       - |  5682 | ` * 	    echo "a is an array\n";` |
|       - |  5683 | ` * 	    var_dump($a);` |
|       - |  5684 | ` *	  }` |
|       - |  5685 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5686 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5687 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5688 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5689 | ` * introduced by the PH7 engine.` |
|       - |  5690 | ` */` |
|   75346 |  5691 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5692 | `{` |
|       - |  5693 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5694 | `	SySet *pInstrContainer;` |
|       - |  5695 | `	sxi32 rc;` |
|       - |  5696 | `	/* Swap token stream */` |
|   75351 |  5697 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   75351 |  5698 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   75351 |  5699 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5700 | `	/* Compile the expression holding the argument value */` |
|   75351 |  5701 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5702 | `	/* Emit the done instruction */` |
|   75351 |  5703 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   75351 |  5704 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   75351 |  5705 | `	RE_SWAP_DELIMITER(pGen);` |
|   75351 |  5706 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5707 | `		return SXERR_ABORT;` |
|       - |  5708 | `	}` |
|   75351 |  5709 | `	return SXRET_OK;` |
|   37678 |  5710 | `}` |
|       - |  5711 | `/*` |
|       - |  5712 | ` * Collect function arguments one after one.` |
|       - |  5713 | ` * According to the PHP language reference manual.` |
|       - |  5714 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5715 | ` * list of expressions.` |
|       - |  5716 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5717 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5718 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5719 | ` * for more information.` |
|       - |  5720 | ` * Example #1 Passing arrays to functions` |
|       - |  5721 | ` * <?php` |
|       - |  5722 | ` * function takes_array($input)` |
|       - |  5723 | ` * {` |
|       - |  5724 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5725 | ` * }` |
|       - |  5726 | ` * ?>` |
|       - |  5727 | ` * Making arguments be passed by reference` |
|       - |  5728 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5729 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5730 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5731 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5732 | ` * to the argument name in the function definition:` |
|       - |  5733 | ` * Example #2 Passing function parameters by reference` |
|       - |  5734 | ` * <?php` |
|       - |  5735 | ` * function add_some_extra(&$string)` |
|       - |  5736 | ` * {` |
|       - |  5737 | ` *   $string .= 'and something extra.';` |
|       - |  5738 | ` * }` |
|       - |  5739 | ` * $str = 'This is a string, ';` |
|       - |  5740 | ` * add_some_extra($str);` |
|       - |  5741 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5742 | ` * ?>` |
|       - |  5743 | ` *` |
|       - |  5744 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5745 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5746 | ` * on these extension.` |
|       - |  5747 | ` */` |
|  105390 |  5748 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5749 | `{` |
|       - |  5750 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5751 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5752 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5753 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5754 | `	sxi32 rc;` |
|       - |  5755 |  |
|  105395 |  5756 | `	pIn = pGen->pIn;` |
|  105395 |  5757 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5758 | `	/* Process arguments one after one */` |
|  136241 |  5759 | `	for(;;){` |
|  272487 |  5760 | `		if( pIn >= pEnd ){` |
|       - |  5761 | `			/* No more arguments to process */` |
|  105379 |  5762 | `			break;` |
|       - |  5763 | `		}` |
|  167113 |  5764 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  167113 |  5765 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  167113 |  5766 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  167113 |  5767 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5768 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  5769 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  5770 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  5771 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  5772 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  5773 | `		{` |
|  167113 |  5774 | `			int bReadonly = 0, bVisSeen = 0;` |
|  167113 |  5775 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  167113 |  5776 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  5777 | `				bReadonly = 1;` |
|       3 |  5778 | `				pIn++;` |
|       1 |  5779 | `			}` |
|  167113 |  5780 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   64787 |  5781 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   64787 |  5782 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      71 |  5783 | `					bVisSeen = 1;` |
|      71 |  5784 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      95 |  5785 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      31 |  5786 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      71 |  5787 | `					pIn++;` |
|      71 |  5788 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      16 |  5789 | `						bReadonly = 1;` |
|      16 |  5790 | `						pIn++;` |
|       6 |  5791 | `					}` |
|      33 |  5792 | `				}` |
|   32391 |  5793 | `			}` |
|  167113 |  5794 | `			if( bVisSeen \|\| bReadonly ){` |
|      73 |  5795 | `				if( !bCtorCtx ){` |
|       6 |  5796 | `					if( bAbstractCtx ){` |
|       3 |  5797 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5798 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5799 | `					}else{` |
|       3 |  5800 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5801 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5802 | `					}` |
|       6 |  5803 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5804 | `						return SXERR_ABORT;` |
|       - |  5805 | `					}` |
|       6 |  5806 | `					return SXERR_SYNTAX;` |
|       - |  5807 | `				}` |
|      69 |  5808 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      69 |  5809 | `				sArg.iPromoteVis = iVis;` |
|      69 |  5810 | `				if( bReadonly ){` |
|      18 |  5811 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       7 |  5812 | `				}` |
|      32 |  5813 | `			}` |
|       - |  5814 | `		}` |
|       - |  5815 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  167104 |  5816 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  126765 |  5817 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   84625 |  5818 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   79209 |  5819 | `			sxu32 nLineLocal = pIn->nLine;` |
|   79209 |  5820 | `			sxi32 iTFlags = 0;` |
|   79209 |  5821 | `			pGen->pIn = pIn;` |
|   79209 |  5822 | `			rc = GenStateParseUnionTypeDecl(` |
|   39602 |  5823 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   39602 |  5824 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5825 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5826 | `				/* bAllowVoid */ 0,` |
|   39602 |  5827 | `						nLineLocal);` |
|   79209 |  5828 | `			pIn = pGen->pIn;` |
|   79209 |  5829 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5830 | `				return SXERR_ABORT;` |
|   79209 |  5831 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5832 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5833 | `				return SXERR_SYNTAX;` |
|   79207 |  5834 | `			}else if( rc == SXERR_SYNTAX ){` |
|      12 |  5835 | `				if( pIn < pEnd ){` |
|      16 |  5836 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5837 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       4 |  5838 | `						&pIn->sData);` |
|       8 |  5839 | `				}else{` |
|     ! 0 |  5840 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5841 | `						"syntax error, unexpected end of file");` |
|       - |  5842 | `				}` |
|      12 |  5843 | `				return SXERR_SYNTAX;` |
|       - |  5844 | `			}` |
|   79199 |  5845 | `			sArg.iFlags \|= iTFlags;` |
|   39597 |  5846 | `		}` |
|  167099 |  5847 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5848 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5849 | `			return rc;` |
|       - |  5850 | `		}` |
|  167099 |  5851 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5852 | `			/* Pass by reference,record that */` |
|    3619 |  5853 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3619 |  5854 | `			pIn++;` |
|    1807 |  5855 | `		}` |
|  167099 |  5856 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5857 | `			/* Variadic parameter: ...$args */` |
|    3635 |  5858 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    3635 |  5859 | `			pIn++;` |
|    1815 |  5860 | `		}` |
|  167099 |  5861 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5862 | `			/* Invalid argument */` |
|     ! 0 |  5863 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5864 | `			return rc;` |
|       - |  5865 | `		}` |
|  167099 |  5866 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5867 | `		/* Copy argument name */` |
|  167099 |  5868 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  167099 |  5869 | `		if( zDup == 0 ){` |
|     ! 0 |  5870 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5871 | `			return SXERR_ABORT;` |
|       - |  5872 | `		}` |
|  167099 |  5873 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  167099 |  5874 | `		pIn++;` |
|  167099 |  5875 | `		if( pIn < pEnd ){` |
|  101207 |  5876 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5877 | `				SyToken *pDefend;` |
|   75353 |  5878 | `				sxi32 iNest = 0;` |
|   75353 |  5879 | `				pIn++; /* Jump the equal sign */` |
|   75353 |  5880 | `				pDefend = pIn;` |
|       - |  5881 | `				/* Process the default value associated with this argument */` |
|  157871 |  5882 | `				while( pDefend < pEnd ){` |
|  118393 |  5883 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   35875 |  5884 | `						break;` |
|       - |  5885 | `					}` |
|   82523 |  5886 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5887 | `						/* Increment nesting level */` |
|    3591 |  5888 | `						iNest++;` |
|   80730 |  5889 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5890 | `						/* Decrement nesting level */` |
|    3591 |  5891 | `						iNest--;` |
|    1793 |  5892 | `					}` |
|   82523 |  5893 | `					pDefend++;` |
|       5 |  5894 | `				}` |
|   75353 |  5895 | `				if( pIn >= pDefend ){` |
|       3 |  5896 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5897 | `					return rc;` |
|       - |  5898 | `				}` |
|       - |  5899 | `				/* Process default value */` |
|   75351 |  5900 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   75351 |  5901 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5902 | `					return rc;` |
|       - |  5903 | `				}` |
|       - |  5904 | `				/* Point beyond the default value */` |
|   75351 |  5905 | `				pIn = pDefend;` |
|   37673 |  5906 | `			}` |
|  101205 |  5907 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5908 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5909 | `				return rc;` |
|       - |  5910 | `			}` |
|  101205 |  5911 | `			pIn++; /* Jump the trailing comma */` |
|   50600 |  5912 | `		}` |
|       - |  5913 | `		/* Append argument signature */` |
|  167097 |  5914 | `		if( sArg.nType > 0 ){` |
|   79145 |  5915 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5916 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|   14405 |  5917 | `				int marker = 'o';` |
|   14405 |  5918 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|   14405 |  5919 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    7205 |  5920 | `			}else{` |
|       - |  5921 | `				int c;` |
|   64745 |  5922 | `				c = 'n'; /* cc warning */` |
|       - |  5923 | `				/* Type leading character */` |
|   64745 |  5924 | `				switch(sArg.nType){` |
|       3 |  5925 | `				case MEMOBJ_HASHMAP:` |
|       - |  5926 | `					/* Hashmap aka 'array' */` |
|       7 |  5927 | `					c = 'h';` |
|       7 |  5928 | `					break;` |
|    9021 |  5929 | `				case MEMOBJ_INT:` |
|       - |  5930 | `					/* Integer */` |
|   18047 |  5931 | `					c = 'i';` |
|   18047 |  5932 | `					break;` |
|       2 |  5933 | `				case MEMOBJ_BOOL:` |
|       - |  5934 | `					/* Bool */` |
|       5 |  5935 | `					c = 'b';` |
|       5 |  5936 | `					break;` |
|       2 |  5937 | `				case MEMOBJ_REAL:` |
|       - |  5938 | `					/* Float */` |
|       5 |  5939 | `					c = 'f';` |
|       5 |  5940 | `					break;` |
|   23334 |  5941 | `				case MEMOBJ_STRING:` |
|       - |  5942 | `					/* String */` |
|   46673 |  5943 | `					c = 's';` |
|   46673 |  5944 | `					break;` |
|       7 |  5945 | `				case MEMOBJ_OBJ:` |
|       - |  5946 | `					/* Object */` |
|      16 |  5947 | `					c = 'o';` |
|      14 |  5948 | `					break;` |
|       1 |  5949 | `				default:` |
|       2 |  5950 | `					break;` |
|       - |  5951 | `				}` |
|   64745 |  5952 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5953 | `			}` |
|   39575 |  5954 | `		}else{` |
|       - |  5955 | `			/* No type is associated with this parameter which mean` |
|       - |  5956 | `			 * that this function is not condidate for overloading.` |
|       - |  5957 | `			 */` |
|   87957 |  5958 | `			SyBlobRelease(&sSig);` |
|       - |  5959 | `		}` |
|       - |  5960 | `		/* Save in the argument set */` |
|  167097 |  5961 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  5962 | `	}` |
|  105379 |  5963 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5964 | `		/* Save function signature */` |
|   50423 |  5965 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   25209 |  5966 | `	}` |
|  105379 |  5967 | `	return SXRET_OK;` |
|   52700 |  5968 | `}` |
|       - |  5969 | `/*` |
|       - |  5970 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5971 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5972 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5973 | ` */` |
|  224758 |  5974 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5975 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5976 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5977 | `	)` |
|       5 |  5978 | `{` |
|       - |  5979 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5980 | `	GenBlock *pBlock;` |
|       - |  5981 | `	sxu32 nGotoOfft;` |
|       - |  5982 | `	sxi32 rc;` |
|       - |  5983 | `	/* Attach the new function */` |
|  224763 |  5984 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  224763 |  5985 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5986 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5987 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5988 | `		return SXERR_ABORT;` |
|       - |  5989 | `	}` |
|  224763 |  5990 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5991 | `	/* Swap bytecode containers */` |
|  224763 |  5992 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  224763 |  5993 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5994 | `	/* Emit constructor property promotion prologue:` |
|       - |  5995 | `	 *   $this->NAME = $NAME;` |
|       - |  5996 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5997 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5998 | `	{` |
|  224763 |  5999 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  6000 | `		sxu32 i;` |
|  363019 |  6001 | `		for( i = 0; i < nArg; i++ ){` |
|  138261 |  6002 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  6003 | `			char *zSrc;` |
|       - |  6004 | `			sxu32 nSrc,nName;` |
|       - |  6005 | `			SySet sToken;` |
|       - |  6006 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  6007 | `			sxi32 rcPromote;` |
|  138261 |  6008 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  138207 |  6009 | `				continue;` |
|       - |  6010 | `			}` |
|       - |  6011 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  6012 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  6013 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  6014 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  6015 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      59 |  6016 | `			nName = SyStringLength(&pArg->sName);` |
|      59 |  6017 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      59 |  6018 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      59 |  6019 | `			if( zSrc == 0 ){` |
|     ! 0 |  6020 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6021 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6022 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  6023 | `				return SXERR_ABORT;` |
|       - |  6024 | `			}` |
|       - |  6025 | `			{` |
|      59 |  6026 | `				char *z = zSrc;` |
|      59 |  6027 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      59 |  6028 | `				z += sizeof("$this->")-1;` |
|      59 |  6029 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6030 | `				z += nName;` |
|      59 |  6031 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      59 |  6032 | `				z += sizeof(" = $")-1;` |
|      59 |  6033 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6034 | `				z += nName;` |
|      59 |  6035 | `				*z = 0;` |
|       - |  6036 | `			}` |
|      59 |  6037 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      59 |  6038 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      59 |  6039 | `			pTmpIn = pGen->pIn;` |
|      59 |  6040 | `			pTmpEnd = pGen->pEnd;` |
|      59 |  6041 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      59 |  6042 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      59 |  6043 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      59 |  6044 | `			pGen->pIn = pTmpIn;` |
|      59 |  6045 | `			pGen->pEnd = pTmpEnd;` |
|      59 |  6046 | `			SySetRelease(&sToken);` |
|      59 |  6047 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  6048 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6049 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6050 | `				return SXERR_ABORT;` |
|       - |  6051 | `			}` |
|       - |  6052 | `			/* Discard the assignment result — this is a statement expression. */` |
|      59 |  6053 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      32 |  6054 | `		}` |
|       - |  6055 | `	}` |
|       - |  6056 | `	/* Compile the body */` |
|  224763 |  6057 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  6058 | `	/* Fix exception jumps now the destination is resolved */` |
|  224763 |  6059 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  6060 | `	/* Emit the final return if not yet done */` |
|  224763 |  6061 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  6062 | `	/* Fix gotos jumps now the destination is resolved */` |
|  224763 |  6063 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  6064 | `		rc = SXERR_ABORT;` |
|     ! 0 |  6065 | `	}` |
|  224763 |  6066 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  6067 | `	/* Restore the default container */` |
|  224763 |  6068 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  6069 | `	/* Leave function block */` |
|  224763 |  6070 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  224763 |  6071 | `	if( rc == SXERR_ABORT ){` |
|       - |  6072 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6073 | `		return SXERR_ABORT;` |
|       - |  6074 | `	}` |
|       - |  6075 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  6076 | `	{` |
|  224763 |  6077 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  6078 | `		sxu32 i;` |
| 4414647 |  6079 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 4189989 |  6080 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     105 |  6081 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     105 |  6082 | `				break;` |
|       - |  6083 | `			}` |
| 2094947 |  6084 | `		}` |
|       - |  6085 | `	}` |
|       - |  6086 | `	/* All done, function body compiled */` |
|  224763 |  6087 | `	return SXRET_OK;` |
|  112384 |  6088 | `}` |
|       - |  6089 | `/*` |
|       - |  6090 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  6091 | ` * According to the PHP language reference manual.` |
|       - |  6092 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  6093 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  6094 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  6095 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  6096 | ` *  Functions need not be defined before they are referenced.` |
|       - |  6097 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  6098 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  6099 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  6100 | ` *  calls with over 32-64 recursion levels.` |
|       - |  6101 | ` *` |
|       - |  6102 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  6103 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  6104 | ` * on these extension.` |
|       - |  6105 | ` */` |
|       - |  6106 | `/*` |
|       - |  6107 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  6108 | ` */` |
|     510 |  6109 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  6110 | `{` |
|       - |  6111 | `	sxu32 i;` |
|    1453 |  6112 | `	for( i = 0; i < n; i++ ){` |
|    1247 |  6113 | `		int a = zA[i], b = zB[i];` |
|    1247 |  6114 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|    1247 |  6115 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|    1247 |  6116 | `		if( a != b ) return a - b;` |
|     474 |  6117 | `	}` |
|     211 |  6118 | `	return 0;` |
|     260 |  6119 | `}` |
|       - |  6120 | `/*` |
|       - |  6121 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  6122 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  6123 | ` * (which are positive bit values stored in sxu32).` |
|       - |  6124 | ` */` |
|       - |  6125 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  6126 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  6127 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  6128 |  |
|       - |  6129 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|       - |  6130 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|       - |  6131 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|       - |  6132 |  |
|       - |  6133 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  6134 | `struct PhlTypeAtom {` |
|       - |  6135 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  6136 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  6137 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  6138 | `	sxu32 nCanon;` |
|       - |  6139 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|       - |  6140 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|       - |  6141 | `};` |
|       - |  6142 |  |
|       - |  6143 | `/*` |
|       - |  6144 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  6145 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  6146 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  6147 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  6148 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  6149 | ` * already be consumed by the caller.` |
|       - |  6150 | ` */` |
|   80070 |  6151 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  6152 | `{` |
|   80075 |  6153 | `	SyToken *pIn = pGen->pIn;` |
|   80075 |  6154 | `	SyZero(pOut, sizeof(*pOut));` |
|   80075 |  6155 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   80075 |  6156 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6157 | `		return SXERR_SYNTAX;` |
|       - |  6158 | `	}` |
|       - |  6159 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   80075 |  6160 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  6161 | `		pIn++;` |
|       8 |  6162 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6163 | `			return SXERR_SYNTAX;` |
|       - |  6164 | `		}` |
|       3 |  6165 | `	}` |
|   80075 |  6166 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6167 | `		return SXERR_SYNTAX;` |
|       - |  6168 | `	}` |
|   80075 |  6169 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   65299 |  6170 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   65299 |  6171 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      33 |  6172 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   65285 |  6173 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      71 |  6174 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   65238 |  6175 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   18307 |  6176 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   56054 |  6177 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   46833 |  6178 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   23489 |  6179 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      32 |  6180 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      60 |  6181 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  6182 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      33 |  6183 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       9 |  6184 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      19 |  6185 | `			pOut->nType = SXU32_HIGH;` |
|      19 |  6186 | `			pOut->sClass = pIn->sData;` |
|      11 |  6187 | `		}else{` |
|       3 |  6188 | `			return SXERR_SYNTAX;` |
|       - |  6189 | `		}` |
|   65297 |  6190 | `		pIn++;` |
|   32651 |  6191 | `	}else{` |
|       - |  6192 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6193 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   14781 |  6194 | `		SyString *pT = &pIn->sData;` |
|   14781 |  6195 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      32 |  6196 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      32 |  6197 | `			pIn++;` |
|   14767 |  6198 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     157 |  6199 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     157 |  6200 | `			pIn++;` |
|   14677 |  6201 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|      24 |  6202 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|      24 |  6203 | `			pIn++;` |
|      14 |  6204 | `		}else{` |
|       - |  6205 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   14581 |  6206 | `			SyToken *pFirst = pIn;` |
|   14581 |  6207 | `			SyToken *pLast = pIn;` |
|   14581 |  6208 | `			pOut->nType = SXU32_HIGH;` |
|   14581 |  6209 | `			pOut->sClass = pIn->sData;` |
|   14581 |  6210 | `			pIn++;` |
|   21867 |  6211 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   14584 |  6212 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6213 | `				pLast = &pIn[1];` |
|       3 |  6214 | `				pIn += 2;` |
|       1 |  6215 | `			}` |
|   14581 |  6216 | `			if( pLast != pFirst ){` |
|       3 |  6217 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6218 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6219 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6220 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6221 | `			}` |
|       - |  6222 | `		}` |
|       - |  6223 | `	}` |
|   80073 |  6224 | `	pGen->pIn = pIn;` |
|   80073 |  6225 | `	return SXRET_OK;` |
|   40040 |  6226 | `}` |
|       - |  6227 |  |
|       - |  6228 | `/*` |
|       - |  6229 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6230 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6231 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6232 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6233 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6234 | ` */` |
|   79910 |  6235 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6236 | `{` |
|       - |  6237 | `	int i;` |
|   79915 |  6238 | `	int nNonNull = 0;` |
|   79915 |  6239 | `	int bAnyIntersection = 0;` |
|       - |  6240 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   79915 |  6241 | `	sxu32 nMaxGroup = 0;` |
| 2637035 |  6242 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  159959 |  6243 | `	for( i = 0; i < nAtoms; i++ ){` |
|   80049 |  6244 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   80021 |  6245 | `			nNonNull++;` |
|   80021 |  6246 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   80021 |  6247 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   80021 |  6248 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   40008 |  6249 | `			}` |
|   40008 |  6250 | `		}` |
|   40027 |  6251 | `	}` |
|  159925 |  6252 | `	for( i = 0; i < nAtoms; i++ ){` |
|   80031 |  6253 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      20 |  6254 | `			bAnyIntersection = 1;` |
|      20 |  6255 | `			break;` |
|       - |  6256 | `		}` |
|   40010 |  6257 | `	}` |
|   79915 |  6258 | `	if( bAnyIntersection ){` |
|       - |  6259 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|       - |  6260 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|       - |  6261 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|      20 |  6262 | `		sxu32 g, nGroups = 0;` |
|      20 |  6263 | `		int bFirstGroup = 1;` |
|      40 |  6264 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|      40 |  6265 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|      24 |  6266 | `			int bFirstMember = 1;` |
|       - |  6267 | `			int bWrap;` |
|      24 |  6268 | `			if( aGroupCount[g] == 0 ) continue;` |
|       - |  6269 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|       - |  6270 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|       - |  6271 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|       - |  6272 | `			 * parens, matching PHP's canonical text. */` |
|      32 |  6273 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|      24 |  6274 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|      24 |  6275 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      72 |  6276 | `			for( i = 0; i < nAtoms; i++ ){` |
|      52 |  6277 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|      40 |  6278 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|      40 |  6279 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      38 |  6280 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      21 |  6281 | `				}else{` |
|       3 |  6282 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6283 | `				}` |
|      40 |  6284 | `				bFirstMember = 0;` |
|      22 |  6285 | `			}` |
|      24 |  6286 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|      24 |  6287 | `			bFirstGroup = 0;` |
|      14 |  6288 | `		}` |
|      20 |  6289 | `		if( bNullable ){` |
|     ! 0 |  6290 | `			SyBlobAppend(pBlob, "\|", 1);` |
|     ! 0 |  6291 | `			SyBlobAppend(pBlob, "null", 4);` |
|     ! 0 |  6292 | `		}` |
|      58 |  6293 | `		return;` |
|       - |  6294 | `	}` |
|   79899 |  6295 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6296 | `		/* Shorthand: ?T */` |
|      81 |  6297 | `		for( i = 0; i < nAtoms; i++ ){` |
|      81 |  6298 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      81 |  6299 | `			SyBlobAppend(pBlob, "?", 1);` |
|      81 |  6300 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      21 |  6301 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      12 |  6302 | `			}else{` |
|      62 |  6303 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6304 | `			}` |
|      81 |  6305 | `			return;` |
|     ! 0 |  6306 | `		}` |
|     ! 0 |  6307 | `	}` |
|       - |  6308 | `	{` |
|   79823 |  6309 | `		int bFirst = 1;` |
|       - |  6310 | `		/* 1) Classes in declaration order */` |
|  159743 |  6311 | `		for( i = 0; i < nAtoms; i++ ){` |
|   79925 |  6312 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   14545 |  6313 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   14545 |  6314 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   14545 |  6315 | `				bFirst = 0;` |
|    7270 |  6316 | `			}` |
|   39965 |  6317 | `		}` |
|       - |  6318 | `		/* 2) Built-ins in canonical order */` |
|       - |  6319 | `		{` |
|       - |  6320 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6321 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6322 | `			int k;` |
|  558731 |  6323 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  893125 |  6324 | `				for( i = 0; i < nAtoms; i++ ){` |
|  479417 |  6325 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   65205 |  6326 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   65205 |  6327 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   65205 |  6328 | `						bFirst = 0;` |
|   65205 |  6329 | `						break;` |
|       - |  6330 | `					}` |
|  207111 |  6331 | `				}` |
|  239459 |  6332 | `			}` |
|       - |  6333 | `		}` |
|       - |  6334 | `		/* 3) null suffix */` |
|   79823 |  6335 | `		if( bNullable ){` |
|      20 |  6336 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      20 |  6337 | `			SyBlobAppend(pBlob, "null", 4);` |
|       8 |  6338 | `		}` |
|       - |  6339 | `	}` |
|   39960 |  6340 | `}` |
|       - |  6341 |  |
|       - |  6342 | `/*` |
|       - |  6343 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|       - |  6344 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|       - |  6345 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|       - |  6346 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|       - |  6347 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|       - |  6348 | ` * whether it was parenthesized.` |
|       - |  6349 | ` *` |
|       - |  6350 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|       - |  6351 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|       - |  6352 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|       - |  6353 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|       - |  6354 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|       - |  6355 | ` */` |
|   80052 |  6356 | `static sxi32 GenStateParsePart(` |
|       - |  6357 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|       - |  6358 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|       5 |  6359 | `{` |
|       - |  6360 | `	sxi32 rc;` |
|   80057 |  6361 | `	int nMembers = 0;` |
|   80057 |  6362 | `	int bParen = 0;` |
|   80057 |  6363 | `	*pnMembers = 0;` |
|   80057 |  6364 | `	*pbParen = 0;` |
|   80057 |  6365 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       6 |  6366 | `		bParen = 1;` |
|       6 |  6367 | `		pGen->pIn++; /* skip '(' */` |
|       2 |  6368 | `	}` |
|   40026 |  6369 | `	for(;;){` |
|   80075 |  6370 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6371 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6372 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6373 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6374 | `		}` |
|   80075 |  6375 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   80075 |  6376 | `		if( rc != SXRET_OK ){` |
|       3 |  6377 | `			return rc;` |
|       - |  6378 | `		}` |
|   80073 |  6379 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   80073 |  6380 | `		(*pnAtoms)++;` |
|   80073 |  6381 | `		nMembers++;` |
|       - |  6382 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   80073 |  6383 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      24 |  6384 | `			SyToken *pNext = &pGen->pIn[1];` |
|      20 |  6385 | `			if( pNext < pGen->pEnd` |
|      24 |  6386 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      22 |  6387 | `				pGen->pIn++; /* skip '&' */` |
|      22 |  6388 | `				continue;` |
|       - |  6389 | `			}` |
|       1 |  6390 | `		}` |
|   80055 |  6391 | `		break;` |
|     ! 0 |  6392 | `	}` |
|   80055 |  6393 | `	if( bParen ){` |
|       6 |  6394 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6395 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6396 | `				"Malformed DNF type: expecting ')'");` |
|     ! 0 |  6397 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6398 | `		}` |
|       6 |  6399 | `		pGen->pIn++; /* skip ')' */` |
|       6 |  6400 | `		if( nMembers < 2 ){` |
|     ! 0 |  6401 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6402 | `				"Parenthesized type must be an intersection of at least two types");` |
|     ! 0 |  6403 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6404 | `		}` |
|       2 |  6405 | `	}` |
|   80055 |  6406 | `	*pnMembers = nMembers;` |
|   80055 |  6407 | `	*pbParen = bParen;` |
|   80055 |  6408 | `	return SXRET_OK;` |
|   40031 |  6409 | `}` |
|       - |  6410 |  |
|       - |  6411 | `/*` |
|       - |  6412 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6413 | ` *` |
|       - |  6414 | ` * Outputs:` |
|       - |  6415 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6416 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6417 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6418 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6419 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6420 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6421 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6422 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6423 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6424 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6425 | ` *` |
|       - |  6426 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6427 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6428 | ` */` |
|   79926 |  6429 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6430 | `	ph7_gen_state *pGen,` |
|       - |  6431 | `	sxu32 *pnType,` |
|       - |  6432 | `	SyString *pClass,` |
|       - |  6433 | `	SySet *pAlts,` |
|       - |  6434 | `	sxi32 *piTypeFlags,` |
|       - |  6435 | `	SyString *pTypeText,` |
|       - |  6436 | `	int iNullableFlag,` |
|       - |  6437 | `	int iUnionFlag,` |
|       - |  6438 | `	int bAllowVoid,` |
|       - |  6439 | `	sxu32 nLine` |
|       5 |  6440 | `){` |
|       - |  6441 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   79931 |  6442 | `	int nAtoms = 0;` |
|   79931 |  6443 | `	int bShortNullable = 0;` |
|   79931 |  6444 | `	int bExplicitNull = 0;` |
|       - |  6445 | `	sxi32 rc;` |
|   79931 |  6446 | `	*pnType = 0;` |
|   79931 |  6447 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   79931 |  6448 | `	*piTypeFlags = 0;` |
|   79931 |  6449 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6450 |  |
|   79931 |  6451 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6452 | `		return SXRET_OK;` |
|       - |  6453 | `	}` |
|       - |  6454 | ``	/* Optional `?` shorthand prefix */`` |
|   79926 |  6455 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      71 |  6456 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      71 |  6457 | `		bShortNullable = 1;` |
|      71 |  6458 | `		pGen->pIn++;` |
|      71 |  6459 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6460 | `			return SXERR_SYNTAX;` |
|       - |  6461 | `		}` |
|      33 |  6462 | `	}` |
|       - |  6463 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|       - |  6464 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|       - |  6465 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|       - |  6466 | `	{` |
|       - |  6467 | `		int nMembers, bParen;` |
|   79931 |  6468 | `		sxu32 iGroup = 0;` |
|   79931 |  6469 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   79931 |  6470 | `		if( rc != SXRET_OK ){` |
|       4 |  6471 | `			return rc;` |
|       - |  6472 | `		}` |
|       - |  6473 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|       - |  6474 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|       - |  6475 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|       - |  6476 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|       - |  6477 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|  120077 |  6478 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   80120 |  6479 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     133 |  6480 | `			if( bShortNullable ){` |
|       - |  6481 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6482 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6483 | `				 * already reported" so callers skip their own error emission. */` |
|       3 |  6484 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6485 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6486 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6487 | `			}` |
|     131 |  6488 | `			if( nMembers >= 2 && !bParen ){` |
|     ! 0 |  6489 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|       - |  6490 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6491 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6492 | `			}` |
|     131 |  6493 | ``			pGen->pIn++; /* skip `\|` */`` |
|     131 |  6494 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|     131 |  6495 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  6496 | `				return rc;` |
|       - |  6497 | `			}` |
|       5 |  6498 | `		}` |
|   79927 |  6499 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|     ! 0 |  6500 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6501 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6502 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6503 | `		}` |
|       - |  6504 | `	}` |
|       - |  6505 | `	/* Validation pass.` |
|       - |  6506 | `	 *` |
|       - |  6507 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6508 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6509 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6510 | `	 */` |
|       - |  6511 | `	{` |
|       - |  6512 | `		int i, j;` |
|   79927 |  6513 | `		int bHasNonNull = 0;` |
|   79927 |  6514 | `		int bAnyIntersection = 0;` |
|       - |  6515 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|       - |  6516 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|       - |  6517 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 2637431 |  6518 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  159993 |  6519 | `		for( i = 0; i < nAtoms; i++ ){` |
|   80071 |  6520 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   40038 |  6521 | `		}` |
|  159955 |  6522 | `		for( i = 0; i < nAtoms; i++ ){` |
|   80051 |  6523 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   40019 |  6524 | `		}` |
|       - |  6525 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|       - |  6526 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   79927 |  6527 | `		if( bShortNullable && bAnyIntersection ){` |
|     ! 0 |  6528 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6529 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|     ! 0 |  6530 | `			return SXERR_SYNTAX;` |
|       - |  6531 | `		}` |
|  159979 |  6532 | `		for( i = 0; i < nAtoms; i++ ){` |
|       - |  6533 | `			/* Intersection members must be class/interface types (PHP rejects` |
|       - |  6534 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|       - |  6535 | ``			 * `true`/`false` in an intersection). */`` |
|   80069 |  6536 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      38 |  6537 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|      38 |  6538 | `				if( bClassLike ){` |
|      36 |  6539 | `					SyString *pC = &aAtoms[i].sClass;` |
|      32 |  6540 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|      32 |  6541 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|      32 |  6542 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|      36 |  6543 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|     ! 0 |  6544 | `						bClassLike = 0;` |
|     ! 0 |  6545 | `					}` |
|      16 |  6546 | `				}` |
|      38 |  6547 | `				if( !bClassLike ){` |
|       - |  6548 | `					const char *zName; sxu32 nName;` |
|       3 |  6549 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6550 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6551 | `					}else{` |
|       3 |  6552 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|       - |  6553 | `					}` |
|       4 |  6554 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6555 | `						"Type %.*s cannot be part of an intersection type",` |
|       1 |  6556 | `						(int)nName, zName);` |
|       3 |  6557 | `					return SXERR_SYNTAX;` |
|       - |  6558 | `				}` |
|      16 |  6559 | `			}` |
|   80067 |  6560 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     157 |  6561 | `				if( nAtoms > 1 ){` |
|       3 |  6562 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6563 | `						"Void can only be used as a standalone type");` |
|       3 |  6564 | `					return SXERR_SYNTAX;` |
|       - |  6565 | `				}` |
|     155 |  6566 | `				if( !bAllowVoid ){` |
|     ! 0 |  6567 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6568 | `						"void cannot be used here");` |
|     ! 0 |  6569 | `					return SXERR_SYNTAX;` |
|       - |  6570 | `				}` |
|     155 |  6571 | `				if( bShortNullable ){` |
|     ! 0 |  6572 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6573 | `						"Void type cannot be nullable");` |
|     ! 0 |  6574 | `					return SXERR_SYNTAX;` |
|       - |  6575 | `				}` |
|      75 |  6576 | `			}` |
|   80065 |  6577 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6578 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|       - |  6579 | `				 * type (never = the function does not return). Mirrors the void` |
|       - |  6580 | `				 * validation above; accepted here and enforced at compile time` |
|       - |  6581 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|      24 |  6582 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|       - |  6583 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|       - |  6584 | `					 * same as any other non-standalone use. */` |
|       5 |  6585 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6586 | `						"never can only be used as a standalone type");` |
|       5 |  6587 | `					return SXERR_SYNTAX;` |
|       - |  6588 | `				}` |
|      19 |  6589 | `				if( !bAllowVoid ){` |
|       - |  6590 | `					/* Return-only: params call with bAllowVoid=0. */` |
|       3 |  6591 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6592 | `						"never cannot be used as a parameter type");` |
|       3 |  6593 | `					return SXERR_SYNTAX;` |
|       - |  6594 | `				}` |
|       7 |  6595 | `			}` |
|   80059 |  6596 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      32 |  6597 | `				bExplicitNull = 1;` |
|      18 |  6598 | `			}else{` |
|   80031 |  6599 | `				bHasNonNull = 1;` |
|       - |  6600 | `			}` |
|       - |  6601 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|       - |  6602 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|       - |  6603 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|       - |  6604 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|       - |  6605 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   80239 |  6606 | `			for( j = 0; j < i; j++ ){` |
|     187 |  6607 | `				int bDup = 0;` |
|     187 |  6608 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|     359 |  6609 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|     182 |  6610 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|     187 |  6611 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|     179 |  6612 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      41 |  6613 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      34 |  6614 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      38 |  6615 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|      16 |  6616 | `								aAtoms[j].sClass.zString,` |
|      32 |  6617 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6618 | `							bDup = 1;` |
|     ! 0 |  6619 | `						}` |
|      22 |  6620 | `					}else{` |
|       3 |  6621 | `						bDup = 1;` |
|       - |  6622 | `					}` |
|      18 |  6623 | `				}` |
|     179 |  6624 | `				if( bDup ){` |
|       - |  6625 | `					const char *zName;` |
|       - |  6626 | `					sxu32 nName;` |
|       3 |  6627 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6628 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6629 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6630 | `					}else{` |
|       3 |  6631 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6632 | `						nName = aAtoms[i].nCanon;` |
|       - |  6633 | `					}` |
|       4 |  6634 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6635 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6636 | `					return SXERR_SYNTAX;` |
|       - |  6637 | `				}` |
|      91 |  6638 | `			}` |
|   40031 |  6639 | `		}` |
|   79915 |  6640 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6641 | `			if( bShortNullable ){` |
|       - |  6642 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6643 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6644 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6645 | `				return SXERR_SYNTAX;` |
|       - |  6646 | `			}` |
|       - |  6647 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6648 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6649 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6650 | `			 * atom, so set it here. */` |
|       7 |  6651 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6652 | `		}` |
|       - |  6653 | `	}` |
|       - |  6654 | `	/* Compute nullability flag */` |
|   79915 |  6655 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      97 |  6656 | `		*piTypeFlags \|= iNullableFlag;` |
|      46 |  6657 | `	}` |
|       - |  6658 | `	/* Build canonical type text */` |
|   79915 |  6659 | `	if( pTypeText ){` |
|       - |  6660 | `		SyBlob sBlob;` |
|   79915 |  6661 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  119838 |  6662 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   39955 |  6663 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   79915 |  6664 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  119624 |  6665 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   79746 |  6666 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   79751 |  6667 | `			if( zDup ){` |
|   79751 |  6668 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   39873 |  6669 | `			}` |
|   39873 |  6670 | `		}` |
|   79915 |  6671 | `		SyBlobRelease(&sBlob);` |
|   39955 |  6672 | `	}` |
|       - |  6673 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6674 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6675 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6676 | `	{` |
|   79915 |  6677 | `		int nNonNull = 0;` |
|   79915 |  6678 | `		int iNonNullIdx = -1;` |
|       - |  6679 | `		int i;` |
|  159959 |  6680 | `		for( i = 0; i < nAtoms; i++ ){` |
|   80049 |  6681 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   80021 |  6682 | `				nNonNull++;` |
|   80021 |  6683 | `				iNonNullIdx = i;` |
|   40008 |  6684 | `			}` |
|   40027 |  6685 | `		}` |
|   79915 |  6686 | `		if( nNonNull <= 1 ){` |
|       - |  6687 | `			/* Fast path: store as single type. */` |
|   79823 |  6688 | `			if( iNonNullIdx >= 0 ){` |
|   79817 |  6689 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   79817 |  6690 | `				if( pA->nType == SXU32_HIGH ){` |
|   21782 |  6691 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    7259 |  6692 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   14523 |  6693 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   14523 |  6694 | `					*pnType = SXU32_HIGH;` |
|   14523 |  6695 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   72558 |  6696 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     155 |  6697 | `					*pnType = MEMOBJ_VOID;` |
|   65224 |  6698 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|      16 |  6699 | `					*pnType = MEMOBJ_NEVER;` |
|       9 |  6700 | `				}else{` |
|   65135 |  6701 | `					*pnType = pA->nType;` |
|       - |  6702 | `				}` |
|   39906 |  6703 | `			}` |
|   39914 |  6704 | `		}else{` |
|       - |  6705 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      97 |  6706 | `			*piTypeFlags \|= iUnionFlag;` |
|     311 |  6707 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6708 | `				ph7_type_alt sAlt;` |
|     219 |  6709 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     209 |  6710 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     209 |  6711 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|     209 |  6712 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|     116 |  6713 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      37 |  6714 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      79 |  6715 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      79 |  6716 | `					sAlt.nType = SXU32_HIGH;` |
|      79 |  6717 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      42 |  6718 | `				}else{` |
|     135 |  6719 | `					sAlt.nType = aAtoms[i].nType;` |
|     135 |  6720 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6721 | `				}` |
|     209 |  6722 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|     107 |  6723 | `			}` |
|       - |  6724 | `		}` |
|       - |  6725 | `	}` |
|   79915 |  6726 | `	return SXRET_OK;` |
|   39968 |  6727 | `}` |
|       - |  6728 |  |
|       - |  6729 | `/*` |
|       - |  6730 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6731 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6732 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6733 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6734 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6735 | `` *          and union types `: T\|U`.`` |
|       - |  6736 | ` */` |
|  318236 |  6737 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  6738 | `{` |
|  318241 |  6739 | `	sxi32 iFlags = 0;` |
|       - |  6740 | `	sxi32 rc;` |
|       - |  6741 | `	sxu32 nLine;` |
|  318241 |  6742 | `	pFunc->nReturnType = 0;` |
|  318241 |  6743 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  318241 |  6744 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  318241 |  6745 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  317741 |  6746 | `		return SXRET_OK;` |
|       - |  6747 | `	}` |
|     505 |  6748 | `	pGen->pIn++; /* Skip ':' */` |
|     505 |  6749 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6750 | `		return SXRET_OK;` |
|       - |  6751 | `	}` |
|     505 |  6752 | `	nLine = pGen->pIn->nLine;` |
|     505 |  6753 | `	rc = GenStateParseUnionTypeDecl(` |
|     250 |  6754 | `		pGen,` |
|     250 |  6755 | `		&pFunc->nReturnType,` |
|     250 |  6756 | `		&pFunc->sReturnClass,` |
|     250 |  6757 | `		&pFunc->aReturnUnion,` |
|       - |  6758 | `		&iFlags,` |
|     250 |  6759 | `		&pFunc->sReturnTypeName,` |
|       - |  6760 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|       - |  6761 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|       - |  6762 | `		/* iUnionFlag */ 0,` |
|       - |  6763 | `		/* bAllowVoid */ 1,` |
|     250 |  6764 | `		nLine);` |
|     505 |  6765 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6766 | `		return SXERR_ABORT;` |
|       - |  6767 | `	}` |
|     505 |  6768 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6769 | `		/* Error already reported */` |
|     ! 0 |  6770 | `		return SXERR_SYNTAX;` |
|       - |  6771 | `	}` |
|     505 |  6772 | `	if( rc == SXERR_SYNTAX ){` |
|       8 |  6773 | `		if( pGen->pIn < pGen->pEnd ){` |
|      11 |  6774 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6775 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       6 |  6776 | `				&pGen->pIn->sData);` |
|       5 |  6777 | `		}else{` |
|     ! 0 |  6778 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6779 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6780 | `		}` |
|       8 |  6781 | `		return SXERR_SYNTAX;` |
|       - |  6782 | `	}` |
|     499 |  6783 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     499 |  6784 | `	return SXRET_OK;` |
|  159123 |  6785 | `}` |
|       - |  6786 |  |
|   47984 |  6787 | `static sxi32 GenStateCompileFunc(` |
|       - |  6788 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6789 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6790 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6791 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6792 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6793 | `	)` |
|       5 |  6794 | `{` |
|       - |  6795 | `	ph7_vm_func *pFunc;` |
|       - |  6796 | `	SyToken *pEnd;` |
|       - |  6797 | `	sxu32 nLine;` |
|       - |  6798 | `	char *zName;` |
|       - |  6799 | `	sxi32 rc;` |
|       - |  6800 | `	/* Extract line number */` |
|   47989 |  6801 | `	nLine = pGen->pIn->nLine;` |
|       - |  6802 | `	/* Jump the left parenthesis '(' */` |
|   47989 |  6803 | `	pGen->pIn++;` |
|       - |  6804 | `	/* Delimit the function signature */` |
|   47989 |  6805 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   47989 |  6806 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6807 | `		/* Syntax error */` |
|       8 |  6808 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       8 |  6809 | `		if( rc == SXERR_ABORT ){` |
|       - |  6810 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6811 | `			return SXERR_ABORT;` |
|       - |  6812 | `		}` |
|       8 |  6813 | `		pGen->pIn = pGen->pEnd;` |
|       8 |  6814 | `		return SXRET_OK;` |
|       - |  6815 | `	}` |
|       - |  6816 | `	/* Create the function state */` |
|   47983 |  6817 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   47983 |  6818 | `	if( pFunc == 0 ){` |
|     ! 0 |  6819 | `		goto OutOfMem;` |
|       - |  6820 | `	}` |
|       - |  6821 | `	/* Build the function name, prepending namespace if active */` |
|   47990 |  6822 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6823 | `		SyBlob sFQN;` |
|       - |  6824 | `		sxu32 nLen;` |
|      16 |  6825 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6826 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6827 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6828 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6829 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6830 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6831 | `		SyBlobRelease(&sFQN);` |
|      16 |  6832 | `		if( zName == 0 ){` |
|     ! 0 |  6833 | `			goto OutOfMem;` |
|       - |  6834 | `		}` |
|      16 |  6835 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6836 | `	}else{` |
|   47969 |  6837 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   47969 |  6838 | `		if( zName == 0 ){` |
|     ! 0 |  6839 | `			goto OutOfMem;` |
|       - |  6840 | `		}` |
|   47969 |  6841 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6842 | `	}` |
|   47983 |  6843 | `	if( pGen->pIn < pEnd ){` |
|       - |  6844 | `		/* Collect function arguments */` |
|   33105 |  6845 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   33105 |  6846 | `		if( rc == SXERR_ABORT ){` |
|       - |  6847 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6848 | `			return SXERR_ABORT;` |
|       - |  6849 | `		}` |
|   16550 |  6850 | `	}` |
|       - |  6851 | `	/* Point past ')' and parse optional return type ': type' */` |
|   47983 |  6852 | `	pGen->pIn = &pEnd[1];` |
|       - |  6853 | `	{` |
|   47983 |  6854 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   47983 |  6855 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6856 | `			return SXERR_ABORT;` |
|   47983 |  6857 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       8 |  6858 | `			return SXERR_SYNTAX;` |
|       - |  6859 | `		}` |
|       - |  6860 | `	}` |
|   47977 |  6861 | `	if( bHandleClosure ){` |
|       - |  6862 | `		ph7_vm_func_closure_env sEnv;` |
|     299 |  6863 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     294 |  6864 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     161 |  6865 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      23 |  6866 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6867 | `				/* Closure,record environment variable */` |
|      23 |  6868 | `				pGen->pIn++;` |
|      23 |  6869 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6870 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6871 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6872 | `						return SXERR_ABORT;` |
|       - |  6873 | `					}` |
|     ! 0 |  6874 | `				}` |
|      23 |  6875 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6876 | `				/* Compile until we hit the first closing parenthesis */` |
|      45 |  6877 | `				while( pGen->pIn < pGen->pEnd ){` |
|      45 |  6878 | `					int iFlagsLocal = 0;` |
|      45 |  6879 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      23 |  6880 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      23 |  6881 | `						break;` |
|       - |  6882 | `					}` |
|      27 |  6883 | `					nLineLocal = pGen->pIn->nLine;` |
|      27 |  6884 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6885 | `						/* Pass by reference,record that */` |
|     ! 0 |  6886 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6887 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6888 | `							);` |
|     ! 0 |  6889 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6890 | `						pGen->pIn++;` |
|     ! 0 |  6891 | `					}` |
|      22 |  6892 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      27 |  6893 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6894 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6895 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6896 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6897 | `								return SXERR_ABORT;` |
|       - |  6898 | `							}` |
|       - |  6899 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6900 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6901 | `								pGen->pIn++;` |
|     ! 0 |  6902 | `							}` |
|     ! 0 |  6903 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6904 | `								pGen->pIn++;` |
|     ! 0 |  6905 | `							}` |
|     ! 0 |  6906 | `							break;` |
|       - |  6907 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6908 | `					}else{` |
|       - |  6909 | `						SyString *pNameLocal;` |
|       - |  6910 | `						char *zDup;` |
|       - |  6911 | `						/* Duplicate variable name */` |
|      27 |  6912 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      27 |  6913 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      27 |  6914 | `						if( zDup ){` |
|       - |  6915 | `							/* Zero the structure */` |
|      27 |  6916 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      27 |  6917 | `							sEnv.iFlags = iFlagsLocal;` |
|      27 |  6918 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      27 |  6919 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      27 |  6920 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6921 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6922 | `									got_this = 1;` |
|     ! 0 |  6923 | `							}` |
|       - |  6924 | `							/* Save imported variable */` |
|      27 |  6925 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      16 |  6926 | `						}else{` |
|     ! 0 |  6927 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6928 | `							 return SXERR_ABORT;` |
|       - |  6929 | `						}` |
|       - |  6930 | `					}` |
|      27 |  6931 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      33 |  6932 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6933 | `						/* Ignore trailing commas */` |
|       7 |  6934 | `						pGen->pIn++;` |
|       1 |  6935 | `					}` |
|       5 |  6936 | `				}` |
|      23 |  6937 | `				if( !got_this ){` |
|       - |  6938 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6939 | `					 * available to the closure environment.` |
|       - |  6940 | `					 */` |
|      23 |  6941 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      23 |  6942 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      23 |  6943 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      23 |  6944 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      23 |  6945 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       9 |  6946 | `				}` |
|      23 |  6947 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6948 | `					/* Mark as closure */` |
|      23 |  6949 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       9 |  6950 | `				}` |
|       9 |  6951 | `		}` |
|     147 |  6952 | `	}` |
|       - |  6953 | `	/* Compile the body */` |
|   47977 |  6954 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   47977 |  6955 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6956 | `		return SXERR_ABORT;` |
|       - |  6957 | `	}` |
|   47977 |  6958 | `	if( ppFunc ){` |
|     299 |  6959 | `		*ppFunc = pFunc;` |
|     147 |  6960 | `	}` |
|   47977 |  6961 | `	rc = SXRET_OK;` |
|   47977 |  6962 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6963 | `		/* Finally register the function */` |
|   47959 |  6964 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   23977 |  6965 | `	}` |
|   47977 |  6966 | `	if( rc == SXRET_OK ){` |
|   47977 |  6967 | `		return SXRET_OK;` |
|       - |  6968 | `	}` |
|       - |  6969 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6970 | `OutOfMem:` |
|       - |  6971 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6972 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6973 | `	 */` |
|     ! 0 |  6974 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6975 | `	return SXERR_ABORT;` |
|   23997 |  6976 | `}` |
|       - |  6977 | `/*` |
|       - |  6978 | ` * Compile a standard PHP function.` |
|       - |  6979 | ` *  Refer to the block-comment above for more information.` |
|       - |  6980 | ` */` |
|   47698 |  6981 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  6982 | `{` |
|       - |  6983 | `	SyString *pName;` |
|       - |  6984 | `	sxi32 iFlags;` |
|       - |  6985 | `	sxu32 nLine;` |
|       - |  6986 | `	sxi32 rc;` |
|       - |  6987 |  |
|   47703 |  6988 | `	nLine = pGen->pIn->nLine;` |
|   47703 |  6989 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   47703 |  6990 | `	iFlags = 0;` |
|   47703 |  6991 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6992 | `		/* Return by reference,remember that */` |
|       7 |  6993 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6994 | `		/* Jump the '&' token */` |
|       7 |  6995 | `		pGen->pIn++;` |
|       3 |  6996 | `	}` |
|   47703 |  6997 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6998 | `		/* Invalid function name */` |
|       8 |  6999 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       8 |  7000 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7001 | `			return SXERR_ABORT;` |
|       - |  7002 | `		}` |
|       - |  7003 | `		/* Sychronize with the next semi-colon or braces*/` |
|      22 |  7004 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      16 |  7005 | `			pGen->pIn++;` |
|       2 |  7006 | `		}` |
|       8 |  7007 | `		return SXRET_OK;` |
|       - |  7008 | `	}` |
|   47697 |  7009 | `	pName = &pGen->pIn->sData;` |
|   47697 |  7010 | `	nLine = pGen->pIn->nLine;` |
|       - |  7011 | `	/* Jump the function name */` |
|   47697 |  7012 | `	pGen->pIn++;` |
|   47697 |  7013 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7014 | `		/* Syntax error */` |
|       3 |  7015 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  7016 | `		if( rc == SXERR_ABORT ){` |
|       - |  7017 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7018 | `			return SXERR_ABORT;` |
|       - |  7019 | `		}` |
|       - |  7020 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  7021 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  7022 | `			pGen->pIn++;` |
|     ! 0 |  7023 | `		}` |
|       3 |  7024 | `		return SXRET_OK;` |
|       - |  7025 | `	}` |
|       - |  7026 | `	/* Compile function body */` |
|   47695 |  7027 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   47695 |  7028 | `	return rc;` |
|   23854 |  7029 | `}` |
|       - |  7030 | `/*` |
|       - |  7031 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  7032 | ` * According to the PHP language reference manual` |
|       - |  7033 | ` *  Visibility:` |
|       - |  7034 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  7035 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  7036 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  7037 | ` *  Members declared protected can be accessed only within the class` |
|       - |  7038 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  7039 | ` *  may only be accessed by the class that defines the member.` |
|       - |  7040 | ` */` |
|  346176 |  7041 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  7042 | `{` |
|  346181 |  7043 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   21631 |  7044 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  324555 |  7045 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   46677 |  7046 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  7047 | `	}` |
|       - |  7048 | `	/* Assume public by default */` |
|  277883 |  7049 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  173093 |  7050 | `}` |
|       - |  7051 | `/*` |
|       - |  7052 | ` * Compile a class constant.` |
|       - |  7053 | ` * According to the PHP language reference manual` |
|       - |  7054 | ` *  Class Constants` |
|       - |  7055 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  7056 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  7057 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  7058 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  7059 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  7060 | ` *   It's also possible for interfaces to have constants.` |
|       - |  7061 | ` * Symisc eXtension.` |
|       - |  7062 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  7063 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7064 | ` *  Example:` |
|       - |  7065 | ` *   class Test{` |
|       - |  7066 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7067 | ` *   };` |
|       - |  7068 | ` *   var_dump(TEST::MyConst);` |
|       - |  7069 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7070 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7071 | ` */` |
|       - |  7072 | `/*` |
|       - |  7073 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  7074 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  7075 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  7076 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  7077 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  7078 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  7079 | ` */` |
|      88 |  7080 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  7081 | `{` |
|       - |  7082 | `	SyToken *p0, *p1;` |
|      93 |  7083 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7084 | `		return 0;` |
|       - |  7085 | `	}` |
|      93 |  7086 | `	p0 = pGen->pIn;` |
|       - |  7087 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|      93 |  7088 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  7089 | `		return 1;` |
|       - |  7090 | `	}` |
|      93 |  7091 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  7092 | `		return 1;` |
|       - |  7093 | `	}` |
|       - |  7094 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  7095 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  7096 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|      89 |  7097 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      89 |  7098 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      89 |  7099 | `		if( p1 ){` |
|      89 |  7100 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      34 |  7101 | `				return 1;` |
|       - |  7102 | `			}` |
|      59 |  7103 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  7104 | `				return 1;` |
|       - |  7105 | `			}` |
|      25 |  7106 | `		}` |
|      25 |  7107 | `	}` |
|      55 |  7108 | `	return 0;` |
|      49 |  7109 | `}` |
|       - |  7110 | `/*` |
|       - |  7111 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|       - |  7112 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|       - |  7113 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|       - |  7114 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|       - |  7115 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|       - |  7116 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|       - |  7117 | ` * Peek only; never consumes tokens.` |
|       - |  7118 | ` */` |
|      24 |  7119 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|       3 |  7120 | `{` |
|      27 |  7121 | `	SyToken *p = pGen->pIn;` |
|      39 |  7122 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      19 |  7123 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|       3 |  7124 | `		p++; /* skip leading unary sign(s) */` |
|       1 |  7125 | `	}` |
|      27 |  7126 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|      23 |  7127 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|       - |  7128 | `	}` |
|       6 |  7129 | `	p++;` |
|       - |  7130 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|       6 |  7131 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|      15 |  7132 | `}` |
|       - |  7133 | `/*` |
|       - |  7134 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  7135 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  7136 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  7137 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  7138 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  7139 | ` * share the same backing.` |
|       - |  7140 | ` */` |
|     212 |  7141 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  7142 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  7143 | `{` |
|     217 |  7144 | `	pAttr->nType = nType;` |
|     217 |  7145 | `	pAttr->sClass = *pClass;` |
|     217 |  7146 | `	pAttr->sTypeName = *pTypeName;` |
|     217 |  7147 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7148 | `		sxu32 i;` |
|      67 |  7149 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      47 |  7150 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      47 |  7151 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      26 |  7152 | `		}` |
|      10 |  7153 | `	}` |
|     217 |  7154 | `}` |
|      88 |  7155 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7156 | `{` |
|      93 |  7157 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7158 | `	SySet *pInstrContainer;` |
|       - |  7159 | `	ph7_class_attr *pCons;` |
|       - |  7160 | `	SyString *pName;` |
|       - |  7161 | `	sxi32 rc;` |
|      93 |  7162 | `	sxu32 nType = 0;` |
|       - |  7163 | `	SyString sTypeClass;` |
|       - |  7164 | `	SyString sTypeText;` |
|       - |  7165 | `	SySet aUnionAlts;` |
|      93 |  7166 | `	sxi32 iTypeFlags = 0;` |
|      93 |  7167 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|      93 |  7168 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|      93 |  7169 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7170 | `	/* Extract visibility level */` |
|      93 |  7171 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7172 | `	/* Mark as constant */` |
|      93 |  7173 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      93 |  7174 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  7175 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  7176 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|     112 |  7177 | `	if( GenStateClassConstHasType(pGen) ){` |
|      61 |  7178 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      38 |  7179 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  7180 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  7181 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  7182 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  7183 | `		 * and success paths release. */` |
|      42 |  7184 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7185 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7186 | `			goto Synchronize;` |
|      42 |  7187 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7188 | `			return SXERR_ABORT;` |
|      42 |  7189 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7190 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7191 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  7192 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7193 | `				return SXERR_ABORT;` |
|       - |  7194 | `			}` |
|     ! 0 |  7195 | `			goto Synchronize;` |
|       - |  7196 | `		}` |
|      42 |  7197 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      19 |  7198 | `	}` |
|      44 |  7199 | `loop:` |
|      95 |  7200 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7201 | `		/* Invalid constant name */` |
|     ! 0 |  7202 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  7203 | `		if( rc == SXERR_ABORT ){` |
|       - |  7204 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7205 | `			return SXERR_ABORT;` |
|       - |  7206 | `		}` |
|     ! 0 |  7207 | `		goto Synchronize;` |
|       - |  7208 | `	}` |
|       - |  7209 | `	/* Peek constant name */` |
|      95 |  7210 | `	pName = &pGen->pIn->sData;` |
|       - |  7211 | `	/* Make sure the constant name isn't reserved */` |
|      95 |  7212 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  7213 | `		/* Reserved constant name */` |
|     ! 0 |  7214 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  7215 | `		if( rc == SXERR_ABORT ){` |
|       - |  7216 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7217 | `			return SXERR_ABORT;` |
|       - |  7218 | `		}` |
|     ! 0 |  7219 | `		goto Synchronize;` |
|       - |  7220 | `	}` |
|       - |  7221 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|      95 |  7222 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      61 |  7223 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      38 |  7224 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      19 |  7225 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      42 |  7226 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7227 | `			return SXERR_ABORT;` |
|      42 |  7228 | `		}else if( rc != SXRET_OK ){` |
|       3 |  7229 | `			goto Synchronize;` |
|       - |  7230 | `		}` |
|      18 |  7231 | `	}` |
|       - |  7232 | `	/* Advance the stream cursor */` |
|      93 |  7233 | `	pGen->pIn++;` |
|      93 |  7234 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  7235 | `		/* Invalid declaration */` |
|     ! 0 |  7236 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  7237 | `		if( rc == SXERR_ABORT ){` |
|       - |  7238 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7239 | `			return SXERR_ABORT;` |
|       - |  7240 | `		}` |
|     ! 0 |  7241 | `		goto Synchronize;` |
|       - |  7242 | `	}` |
|      93 |  7243 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  7244 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|       - |  7245 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|       - |  7246 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|       - |  7247 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|      88 |  7248 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|      39 |  7249 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|       8 |  7250 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7251 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|       2 |  7252 | `			&pClass->sName,pName,&sTypeText);` |
|       6 |  7253 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7254 | `			return SXERR_ABORT;` |
|       - |  7255 | `		}` |
|       6 |  7256 | `		goto Synchronize;` |
|       - |  7257 | `	}` |
|       - |  7258 | `	/* Allocate a new class attribute */` |
|      89 |  7259 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      89 |  7260 | `	if( pCons == 0 ){` |
|     ! 0 |  7261 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7262 | `		return SXERR_ABORT;` |
|       - |  7263 | `	}` |
|      89 |  7264 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      35 |  7265 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      16 |  7266 | `	}` |
|       - |  7267 | `	/* Swap bytecode container */` |
|      89 |  7268 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      89 |  7269 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  7270 | `	/* Compile constant value.` |
|       - |  7271 | `	 */` |
|      89 |  7272 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      89 |  7273 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  7274 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  7275 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7276 | `			return SXERR_ABORT;` |
|       - |  7277 | `		}` |
|       1 |  7278 | `	}` |
|       - |  7279 | `	/* Emit the done instruction */` |
|      89 |  7280 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      89 |  7281 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      89 |  7282 | `	if( rc == SXERR_ABORT ){` |
|       - |  7283 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7284 | `		return SXERR_ABORT;` |
|       - |  7285 | `	}` |
|       - |  7286 | `	/* All done,install the constant */` |
|      89 |  7287 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      89 |  7288 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7289 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7290 | `		return SXERR_ABORT;` |
|       - |  7291 | `	}` |
|      89 |  7292 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7293 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  7294 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  7295 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7296 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7297 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7298 | `				pTok--;` |
|     ! 0 |  7299 | `			}` |
|     ! 0 |  7300 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7301 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  7302 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7303 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7304 | `				return SXERR_ABORT;` |
|       - |  7305 | `			}` |
|     ! 0 |  7306 | `		}else{` |
|       3 |  7307 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  7308 | `				goto loop;` |
|       - |  7309 | `			}` |
|       - |  7310 | `		}` |
|     ! 0 |  7311 | `	}` |
|      87 |  7312 | `	SySetRelease(&aUnionAlts);` |
|      87 |  7313 | `	return SXRET_OK;` |
|       3 |  7314 | `Synchronize:` |
|       9 |  7315 | `	SySetRelease(&aUnionAlts);` |
|       - |  7316 | `	/* Synchronize with the first semi-colon */` |
|      21 |  7317 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      15 |  7318 | `		pGen->pIn++;` |
|       3 |  7319 | `	}` |
|       9 |  7320 | `	return SXERR_CORRUPT;` |
|      49 |  7321 | `}` |
|       - |  7322 | `/*` |
|       - |  7323 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  7324 | ` * According to the PHP language reference manual` |
|       - |  7325 | ` *  Properties` |
|       - |  7326 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  7327 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  7328 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  7329 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  7330 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  7331 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  7332 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  7333 | ` * Symisc eXtension.` |
|       - |  7334 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  7335 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7336 | ` *  Example:` |
|       - |  7337 | ` *   class Test{` |
|       - |  7338 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7339 | ` *   };` |
|       - |  7340 | ` *   var_dump(TEST::myVar);` |
|       - |  7341 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7342 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7343 | ` */` |
|       - |  7344 | `/*` |
|       - |  7345 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  7346 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  7347 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  7348 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  7349 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  7350 | ` */` |
|  187592 |  7351 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  7352 | `{` |
|  187597 |  7353 | `	SyToken *p = pStart;` |
|  187597 |  7354 | `	int bFirst = 1;` |
|  187597 |  7355 | `	if( p >= pEnd ) return 0;` |
|       - |  7356 | ``	/* Optional nullable `?` shorthand. */`` |
|  187597 |  7357 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      18 |  7358 | `		p++;` |
|      18 |  7359 | `		if( p >= pEnd ) return 0;` |
|       8 |  7360 | `	}` |
|       - |  7361 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  7362 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  7363 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  7364 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   93796 |  7365 | `	for(;;){` |
|  187615 |  7366 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  7367 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  7368 | `			p++;` |
|       9 |  7369 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  7370 | `			if( p >= pEnd ) return 0;` |
|       3 |  7371 | `			p++; /* skip ')' */` |
|       2 |  7372 | `		}else{` |
|       - |  7373 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - |  7374 | ``			 * then any `&`-joined intersection members. */`` |
|  187613 |  7375 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  187613 |  7376 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7377 | `				return 0;` |
|       - |  7378 | `			}` |
|       - |  7379 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - |  7380 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - |  7381 | `			 * may still appear at the initial dispatch site). */` |
|  187613 |  7382 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  187567 |  7383 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  187562 |  7384 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|   10978 |  7385 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  187413 |  7386 | `					return 0;` |
|       - |  7387 | `				}` |
|      77 |  7388 | `			}` |
|     205 |  7389 | `			p++;` |
|     207 |  7390 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7391 | `				p += 2;` |
|       1 |  7392 | `			}` |
|     303 |  7393 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     208 |  7394 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7395 | `				p++; /* skip '&' */` |
|       3 |  7396 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       3 |  7397 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       3 |  7398 | `				p++;` |
|       3 |  7399 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  7400 | `					p += 2;` |
|     ! 0 |  7401 | `				}` |
|       1 |  7402 | `			}` |
|       - |  7403 | `		}` |
|     207 |  7404 | `		bFirst = 0;` |
|     202 |  7405 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      23 |  7406 | `			&& p->sData.zString[0] == '\|' ){` |
|      23 |  7407 | ``			p++; /* next `\|`-separated part */`` |
|      23 |  7408 | `			continue;` |
|       - |  7409 | `		}` |
|     189 |  7410 | `		break;` |
|     ! 0 |  7411 | `	}` |
|     189 |  7412 | `	if( p >= pEnd ) return 0;` |
|     189 |  7413 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   93801 |  7414 | `}` |
|       - |  7415 |  |
|       - |  7416 | `/*` |
|       - |  7417 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  7418 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  7419 | ` * if not). Recognized forms:` |
|       - |  7420 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  7421 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  7422 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  7423 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  7424 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  7425 | ` * on unrecoverable error.` |
|       - |  7426 | ` *` |
|       - |  7427 | ` * When a type is parsed:` |
|       - |  7428 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  7429 | ` *   *pClass is set to the class name (for class types)` |
|       - |  7430 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  7431 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  7432 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  7433 | ` */` |
|     184 |  7434 | `static sxi32 GenStateParsePropertyType(` |
|       - |  7435 | `	ph7_gen_state *pGen,` |
|       - |  7436 | `	sxu32 *pnType,` |
|       - |  7437 | `	SyString *pClass,` |
|       - |  7438 | `	sxi32 *piTypeFlags,` |
|       - |  7439 | `	SyString *pTypeText,` |
|       - |  7440 | `	SySet *pAlts` |
|       5 |  7441 | `){` |
|     189 |  7442 | `	sxi32 iFlags = 0;` |
|       - |  7443 | `	sxi32 rc;` |
|     189 |  7444 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7445 | `		return SXRET_OK;` |
|       - |  7446 | `	}` |
|       - |  7447 | `	/* If the first token is '$', there's no type */` |
|     189 |  7448 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  7449 | `		return SXRET_OK;` |
|       - |  7450 | `	}` |
|     189 |  7451 | `	rc = GenStateParseUnionTypeDecl(` |
|      92 |  7452 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  7453 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  7454 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  7455 | `		/* bAllowVoid */ 0,` |
|     184 |  7456 | `		pGen->pIn->nLine);` |
|     189 |  7457 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7458 | `		return rc;` |
|       - |  7459 | `	}` |
|       - |  7460 | `	/* Verify next token is '$' (start of property name) */` |
|     189 |  7461 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7462 | `		return SXERR_SYNTAX;` |
|       - |  7463 | `	}` |
|     189 |  7464 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     189 |  7465 | `	return SXRET_OK;` |
|      97 |  7466 | `}` |
|       - |  7467 |  |
|       - |  7468 | `/*` |
|       - |  7469 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  7470 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  7471 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  7472 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  7473 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  7474 | ` * by the type parser itself before reaching here.` |
|       - |  7475 | ` *` |
|       - |  7476 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  7477 | ` * use in the error message.` |
|       - |  7478 | ` */` |
|     336 |  7479 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  7480 | `	sxu32 nType,` |
|       - |  7481 | `	const SyString *pClass,` |
|       - |  7482 | `	const char **pzName,` |
|       - |  7483 | `	sxu32 *pnName)` |
|       5 |  7484 | `{` |
|       - |  7485 | `	const char *z;` |
|       - |  7486 | `	sxu32 n;` |
|     341 |  7487 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     287 |  7488 | `		return 0;` |
|       - |  7489 | `	}` |
|      59 |  7490 | `	z = pClass->zString;` |
|      59 |  7491 | `	n = pClass->nByte;` |
|      59 |  7492 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  7493 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  7494 | `	}` |
|       - |  7495 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  7496 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  7497 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      51 |  7498 | `	return 0;` |
|     173 |  7499 | `}` |
|       - |  7500 |  |
|       - |  7501 | `/*` |
|       - |  7502 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  7503 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  7504 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  7505 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  7506 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  7507 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  7508 | ` *` |
|       - |  7509 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  7510 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  7511 | ` */` |
|     278 |  7512 | `static sxi32 GenStateValidateMemberType(` |
|       - |  7513 | `	ph7_gen_state *pGen,` |
|       - |  7514 | `	ph7_class *pClass,` |
|       - |  7515 | `	const SyString *pMemberName,` |
|       - |  7516 | `	sxu32 nType,` |
|       - |  7517 | `	const SyString *pTypeClass,` |
|       - |  7518 | `	const SyString *pTypeText,` |
|       - |  7519 | `	SySet *pUnionAlts,` |
|       - |  7520 | `	const char *zErrFmt,` |
|       - |  7521 | `	sxu32 nLine)` |
|       5 |  7522 | `{` |
|     283 |  7523 | `	const char *zBad = 0;` |
|     283 |  7524 | `	sxu32 nBad = 0;` |
|       - |  7525 | `	SyString sFallback;` |
|       - |  7526 | `	const SyString *pBad;` |
|       - |  7527 | `	sxi32 rc;` |
|     283 |  7528 | `	int bDisallowed = 0;` |
|     283 |  7529 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  7530 | `		bDisallowed = 1;` |
|     281 |  7531 | `	}else if( pUnionAlts ){` |
|       - |  7532 | `		sxu32 i;` |
|      89 |  7533 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      63 |  7534 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      63 |  7535 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  7536 | `				bDisallowed = 1;` |
|       3 |  7537 | `				break;` |
|       - |  7538 | `			}` |
|      33 |  7539 | `		}` |
|      14 |  7540 | `	}` |
|     283 |  7541 | `	if( !bDisallowed ){` |
|     277 |  7542 | `		return SXRET_OK;` |
|       - |  7543 | `	}` |
|       - |  7544 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  7545 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  7546 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  7547 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  7548 | `		pBad = pTypeText;` |
|       5 |  7549 | `	}else{` |
|     ! 0 |  7550 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  7551 | `		pBad = &sFallback;` |
|       - |  7552 | `	}` |
|      11 |  7553 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  7554 | `		zErrFmt,` |
|       3 |  7555 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  7556 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7557 | `		return SXERR_ABORT;` |
|       - |  7558 | `	}` |
|       8 |  7559 | `	return SXERR_SYNTAX;` |
|     144 |  7560 | `}` |
|       - |  7561 | `/*` |
|       - |  7562 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  7563 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  7564 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  7565 | ` * than promoted to a lexer keyword.` |
|       - |  7566 | ` */` |
| 1660778 |  7567 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  7568 | `{` |
| 1694745 |  7569 | `	return (pTok->nType & PH7_TK_ID)` |
|  864351 |  7570 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1694740 |  7571 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  7572 | `}` |
|   75996 |  7573 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7574 | `{` |
|   76001 |  7575 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7576 | `	ph7_class_attr *pAttr;` |
|       - |  7577 | `	SyString *pName;` |
|       - |  7578 | `	sxi32 rc;` |
|   76001 |  7579 | `	sxu32 nType = 0;` |
|       - |  7580 | `	SyString sTypeClass;` |
|       - |  7581 | `	SyString sTypeText;` |
|       - |  7582 | `	SySet aUnionAlts;` |
|   76001 |  7583 | `	sxi32 iTypeFlags = 0;` |
|   76001 |  7584 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   76001 |  7585 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   76001 |  7586 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7587 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  7588 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  7589 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   76001 |  7590 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  7591 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7592 | `	}` |
|       - |  7593 | `	/* Extract visibility level */` |
|   76001 |  7594 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7595 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   76093 |  7596 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     189 |  7597 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     189 |  7598 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7599 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7600 | `			goto Synchronize;` |
|     189 |  7601 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7602 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7603 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7604 | `				&pGen->pIn->sData);` |
|     ! 0 |  7605 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7606 | `				return SXERR_ABORT;` |
|       - |  7607 | `			}` |
|     ! 0 |  7608 | `			goto Synchronize;` |
|     189 |  7609 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7610 | `			return SXERR_ABORT;` |
|       - |  7611 | `		}` |
|      92 |  7612 | `	}` |
|     ! 0 |  7613 | `loop:` |
|   76005 |  7614 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7615 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7616 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7617 | `			return SXERR_ABORT;` |
|       - |  7618 | `		}` |
|     ! 0 |  7619 | `		goto Synchronize;` |
|       - |  7620 | `	}` |
|   76005 |  7621 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   76005 |  7622 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7623 | `		/* Invalid attribute name */` |
|     ! 0 |  7624 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7625 | `		if( rc == SXERR_ABORT ){` |
|       - |  7626 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7627 | `			return SXERR_ABORT;` |
|       - |  7628 | `		}` |
|     ! 0 |  7629 | `		goto Synchronize;` |
|       - |  7630 | `	}` |
|       - |  7631 | `	/* Peek attribute name */` |
|   76005 |  7632 | `	pName = &pGen->pIn->sData;` |
|       - |  7633 | `	/* Advance the stream cursor */` |
|   76005 |  7634 | `	pGen->pIn++;` |
|   76005 |  7635 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7636 | `		/* Invalid declaration */` |
|       3 |  7637 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7638 | `		if( rc == SXERR_ABORT ){` |
|       - |  7639 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7640 | `			return SXERR_ABORT;` |
|       - |  7641 | `		}` |
|       3 |  7642 | `		goto Synchronize;` |
|       - |  7643 | `	}` |
|       - |  7644 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  7645 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   76003 |  7646 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  7647 | `		const char *zRoErr = 0;` |
|      39 |  7648 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  7649 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  7650 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  7651 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  7652 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  7653 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  7654 | `		}` |
|      39 |  7655 | `		if( zRoErr ){` |
|      13 |  7656 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  7657 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7658 | `				return SXERR_ABORT;` |
|       - |  7659 | `			}` |
|      13 |  7660 | `			goto Synchronize;` |
|       - |  7661 | `		}` |
|      12 |  7662 | `	}` |
|       - |  7663 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7664 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7665 | `	 * by the type parser. */` |
|   75993 |  7666 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     278 |  7667 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7668 | `			&sTypeText,` |
|     182 |  7669 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      91 |  7670 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     187 |  7671 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7672 | `			return SXERR_ABORT;` |
|     187 |  7673 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7674 | `			goto Synchronize;` |
|       - |  7675 | `		}` |
|      91 |  7676 | `	}` |
|       - |  7677 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   75993 |  7678 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7679 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7680 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7681 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7682 | `			return SXERR_ABORT;` |
|       - |  7683 | `		}` |
|       3 |  7684 | `		goto Synchronize;` |
|       - |  7685 | `	}` |
|       - |  7686 | `	/* Allocate a new class attribute */` |
|   75991 |  7687 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   75991 |  7688 | `	if( pAttr == 0 ){` |
|     ! 0 |  7689 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7690 | `		return SXERR_ABORT;` |
|       - |  7691 | `	}` |
|   75991 |  7692 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     185 |  7693 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      90 |  7694 | `	}` |
|   75991 |  7695 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7696 | `		SySet *pInstrContainer;` |
|   22013 |  7697 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7698 | `		/* Swap bytecode container */` |
|   22013 |  7699 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   22013 |  7700 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7701 | `		/* Compile attribute value.` |
|       - |  7702 | `		 */` |
|   22013 |  7703 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   22013 |  7704 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7705 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7706 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7707 | `				return SXERR_ABORT;` |
|       - |  7708 | `			}` |
|     ! 0 |  7709 | `		}` |
|       - |  7710 | `		/* Emit the done instruction */` |
|   22013 |  7711 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   22013 |  7712 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   11004 |  7713 | `	}` |
|       - |  7714 | `	/* All done,install the attribute */` |
|   75991 |  7715 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   75991 |  7716 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7717 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7718 | `		return SXERR_ABORT;` |
|       - |  7719 | `	}` |
|   75991 |  7720 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7721 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7722 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7723 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7724 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7725 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7726 | `				pTok--;` |
|     ! 0 |  7727 | `			}` |
|     ! 0 |  7728 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7729 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7730 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7731 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7732 | `				return SXERR_ABORT;` |
|       - |  7733 | `			}` |
|     ! 0 |  7734 | `		}else{` |
|       5 |  7735 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7736 | `				goto loop;` |
|       - |  7737 | `			}` |
|       - |  7738 | `		}` |
|     ! 0 |  7739 | `	}` |
|   75987 |  7740 | `	SySetRelease(&aUnionAlts);` |
|   75987 |  7741 | `	return SXRET_OK;` |
|       7 |  7742 | `Synchronize:` |
|       - |  7743 | `	/* Synchronize with the first semi-colon */` |
|      31 |  7744 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      16 |  7745 | `		pGen->pIn++;` |
|       2 |  7746 | `	}` |
|      17 |  7747 | `	SySetRelease(&aUnionAlts);` |
|      17 |  7748 | `	return SXERR_CORRUPT;` |
|   38003 |  7749 | `}` |
|       - |  7750 | `/*` |
|       - |  7751 | ` * Compile a class method.` |
|       - |  7752 | ` *` |
|       - |  7753 | ` * Refer to the official documentation for more information` |
|       - |  7754 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7755 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7756 | ` * overloading and many more.` |
|       - |  7757 | ` */` |
|  270092 |  7758 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7759 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7760 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7761 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7762 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7763 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7764 | `	)` |
|       5 |  7765 | `{` |
|  270097 |  7766 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7767 | `	ph7_class_method *pMeth;` |
|       - |  7768 | `	sxi32 iFuncFlags;` |
|       - |  7769 | `	SyString *pName;` |
|       - |  7770 | `	SyToken *pEnd;` |
|       - |  7771 | `	sxi32 rc;` |
|       - |  7772 | `	/* Extract visibility level */` |
|  270097 |  7773 | `	iProtection = GetProtectionLevel(iProtection);` |
|  270097 |  7774 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  270097 |  7775 | `	iFuncFlags = 0;` |
|  270097 |  7776 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7777 | `		/* Invalid method name */` |
|     ! 0 |  7778 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7779 | `		if( rc == SXERR_ABORT ){` |
|       - |  7780 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7781 | `			return SXERR_ABORT;` |
|       - |  7782 | `		}` |
|     ! 0 |  7783 | `		goto Synchronize;` |
|       - |  7784 | `	}` |
|  270097 |  7785 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7786 | `		/* Return by reference,remember that */` |
|     ! 0 |  7787 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7788 | `		/* Jump the '&' token */` |
|     ! 0 |  7789 | `		pGen->pIn++;` |
|     ! 0 |  7790 | `	}` |
|  270097 |  7791 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7792 | `		/* Invalid method name */` |
|     ! 0 |  7793 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7794 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7795 | `			return SXERR_ABORT;` |
|       - |  7796 | `		}` |
|     ! 0 |  7797 | `		goto Synchronize;` |
|       - |  7798 | `	}` |
|       - |  7799 | `	/* Peek method name */` |
|  270097 |  7800 | `	pName = &pGen->pIn->sData;` |
|  270097 |  7801 | `	nLine = pGen->pIn->nLine;` |
|       - |  7802 | `	/* Jump the method name */` |
|  270097 |  7803 | `	pGen->pIn++;` |
|  270097 |  7804 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7805 | `		/* Abstract method */` |
|   93299 |  7806 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7807 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7808 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7809 | `				&pClass->sName,pName);` |
|     ! 0 |  7810 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7811 | `				return SXERR_ABORT;` |
|       - |  7812 | `			}` |
|     ! 0 |  7813 | `		}` |
|       - |  7814 | `		/* Assemble method signature only */` |
|   93299 |  7815 | `		doBody = FALSE;` |
|   46647 |  7816 | `	}` |
|  270097 |  7817 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7818 | `		/* Syntax error */` |
|     ! 0 |  7819 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7820 | `		if( rc == SXERR_ABORT ){` |
|       - |  7821 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7822 | `			return SXERR_ABORT;` |
|       - |  7823 | `		}` |
|     ! 0 |  7824 | `		goto Synchronize;` |
|       - |  7825 | `	}` |
|       - |  7826 | `	/* Allocate a new class_method instance */` |
|  270097 |  7827 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  270097 |  7828 | `	if( pMeth == 0 ){` |
|     ! 0 |  7829 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7830 | `		return SXERR_ABORT;` |
|       - |  7831 | `	}` |
|       - |  7832 | `	/* Jump the left parenthesis '(' */` |
|  270097 |  7833 | `	pGen->pIn++;` |
|  270097 |  7834 | `	pEnd = 0; /* cc warning */` |
|       - |  7835 | `	/* Delimit the method signature */` |
|  270097 |  7836 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  270097 |  7837 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7838 | `		/* Syntax error */` |
|       3 |  7839 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7840 | `		if( rc == SXERR_ABORT ){` |
|       - |  7841 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7842 | `			return SXERR_ABORT;` |
|       - |  7843 | `		}` |
|       3 |  7844 | `		goto Synchronize;` |
|       - |  7845 | `	}` |
|       - |  7846 | `	{` |
|  270095 |  7847 | `		int bIsCtor = 0;` |
|  270095 |  7848 | `		int bAbstractCtor = 0;` |
|  270090 |  7849 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  160273 |  7850 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  259255 |  7851 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   21685 |  7852 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7853 | `				bAbstractCtor = 1;` |
|       2 |  7854 | `			}else{` |
|   21683 |  7855 | `				bIsCtor = 1;` |
|       - |  7856 | `			}` |
|   10840 |  7857 | `		}` |
|  270095 |  7858 | `		if( pGen->pIn < pEnd ){` |
|       - |  7859 | `			/* Collect method arguments */` |
|   72195 |  7860 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   72195 |  7861 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7862 | `				return SXERR_ABORT;` |
|       - |  7863 | `			}` |
|   36095 |  7864 | `		}` |
|       - |  7865 | `	}` |
|       - |  7866 | `	/* Point past ')' and parse optional return type ': type' */` |
|  270095 |  7867 | `	pGen->pIn = &pEnd[1];` |
|       - |  7868 | `	{` |
|  270095 |  7869 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  270095 |  7870 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7871 | `			return SXERR_ABORT;` |
|  270095 |  7872 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7873 | `			goto Synchronize;` |
|       - |  7874 | `		}` |
|       - |  7875 | `	}` |
|       - |  7876 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7877 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7878 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7879 | `	{` |
|  270095 |  7880 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7881 | `		sxu32 i;` |
|  392599 |  7882 | `		for( i = 0; i < nArg; i++ ){` |
|  122519 |  7883 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7884 | `			ph7_class_attr *pAttr;` |
|  122519 |  7885 | `			sxi32 iAttrFlags = 0;` |
|       - |  7886 | `			int bArgTyped;` |
|  122519 |  7887 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  122455 |  7888 | `				continue;` |
|       - |  7889 | `			}` |
|       - |  7890 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - |  7891 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - |  7892 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      49 |  7893 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      70 |  7894 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|      69 |  7895 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7896 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7897 | `					"Cannot declare variadic promoted property");` |
|       3 |  7898 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7899 | `					return SXERR_ABORT;` |
|       - |  7900 | `				}` |
|       3 |  7901 | `				goto Synchronize;` |
|       - |  7902 | `			}` |
|       - |  7903 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7904 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7905 | `			 * appear as an alternative of a union type. */` |
|      67 |  7906 | `			if( bArgTyped ){` |
|      92 |  7907 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      58 |  7908 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      58 |  7909 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      29 |  7910 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      63 |  7911 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7912 | `					return SXERR_ABORT;` |
|      63 |  7913 | `				}else if( rc != SXRET_OK ){` |
|       6 |  7914 | `					goto Synchronize;` |
|       - |  7915 | `				}` |
|      27 |  7916 | `			}` |
|       - |  7917 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      63 |  7918 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7919 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7920 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7921 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7922 | `					return SXERR_ABORT;` |
|       - |  7923 | `				}` |
|       3 |  7924 | `				goto Synchronize;` |
|       - |  7925 | `			}` |
|      61 |  7926 | `			if( bArgTyped ){` |
|      57 |  7927 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      26 |  7928 | `			}` |
|      61 |  7929 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7930 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7931 | `			}` |
|      61 |  7932 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 |  7933 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 |  7934 | `			}` |
|      61 |  7935 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  7936 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  7937 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      24 |  7938 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  7939 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7940 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  7941 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7942 | `						return SXERR_ABORT;` |
|       - |  7943 | `					}` |
|       3 |  7944 | `					goto Synchronize;` |
|       - |  7945 | `				}` |
|      22 |  7946 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7947 | `			}` |
|      59 |  7948 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      59 |  7949 | `			if( pAttr == 0 ){` |
|     ! 0 |  7950 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7951 | `				return SXERR_ABORT;` |
|       - |  7952 | `			}` |
|      59 |  7953 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      57 |  7954 | `				pAttr->nType = pArg->nType;` |
|      57 |  7955 | `				pAttr->sClass = pArg->sClass;` |
|      57 |  7956 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      57 |  7957 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7958 | `					sxu32 k;` |
|      20 |  7959 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 |  7960 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 |  7961 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 |  7962 | `					}` |
|       3 |  7963 | `				}` |
|      26 |  7964 | `			}` |
|      59 |  7965 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      59 |  7966 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7967 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7968 | `				return SXERR_ABORT;` |
|       - |  7969 | `			}` |
|      32 |  7970 | `		}` |
|       - |  7971 | `	}` |
|  270085 |  7972 | `	if( doBody ){` |
|       - |  7973 | `		/* Compile method body */` |
|  176791 |  7974 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  176791 |  7975 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7976 | `			return SXERR_ABORT;` |
|       - |  7977 | `		}` |
|   88398 |  7978 | `	}else{` |
|       - |  7979 | `		/* Only method signature is allowed */` |
|   93299 |  7980 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7981 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7982 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7983 | `				if( rc == SXERR_ABORT ){` |
|       - |  7984 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7985 | `					return SXERR_ABORT;` |
|       - |  7986 | `				}` |
|     ! 0 |  7987 | `				return SXERR_CORRUPT;` |
|       - |  7988 | `			}` |
|       - |  7989 | `	}` |
|       - |  7990 | `	/* All done,install the method */` |
|  270085 |  7991 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  270085 |  7992 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7993 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7994 | `		return SXERR_ABORT;` |
|       - |  7995 | `	}` |
|  270085 |  7996 | `	return SXRET_OK;` |
|       6 |  7997 | `Synchronize:` |
|       - |  7998 | `	/* Synchronize with the first semi-colon */` |
|      40 |  7999 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  8000 | `		pGen->pIn++;` |
|       4 |  8001 | `	}` |
|      16 |  8002 | `	return SXERR_CORRUPT;` |
|  135051 |  8003 | `}` |
|       - |  8004 | `/*` |
|       - |  8005 | ` * Compile an object interface.` |
|       - |  8006 | ` *  According to the PHP language reference manual` |
|       - |  8007 | ` *   Object Interfaces:` |
|       - |  8008 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  8009 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  8010 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  8011 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  8012 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  8013 | ` */` |
|   39528 |  8014 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  8015 | `{` |
|   39533 |  8016 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8017 | `	ph7_class *pClass,*pBase;` |
|       - |  8018 | `	SyToken *pEnd,*pTmp;` |
|       - |  8019 | `	SyString *pName;` |
|       - |  8020 | `	sxi32 nKwrd;` |
|       - |  8021 | `	sxi32 rc;` |
|       - |  8022 | `	/* Jump the 'interface' keyword */` |
|   39533 |  8023 | `	pGen->pIn++;` |
|       - |  8024 | `	/* Extract interface name */` |
|   39533 |  8025 | `	pName = &pGen->pIn->sData;` |
|       - |  8026 | `	/* Advance the stream cursor */` |
|   39533 |  8027 | `	pGen->pIn++;` |
|       - |  8028 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8029 | `		SyBlob sFQN;` |
|       - |  8030 | `		SyString sFQNStr;` |
|   39533 |  8031 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   39533 |  8032 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   39533 |  8033 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   39533 |  8034 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   39533 |  8035 | `		SyBlobRelease(&sFQN);` |
|       - |  8036 | `	}` |
|   39533 |  8037 | `	if( pClass == 0 ){` |
|     ! 0 |  8038 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8039 | `		return SXERR_ABORT;` |
|       - |  8040 | `	}` |
|       - |  8041 | `	/* Mark as an interface */` |
|   39533 |  8042 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  8043 | `	/* Assume no base class is given */` |
|   39533 |  8044 | `	pBase = 0;` |
|   39533 |  8045 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   10771 |  8046 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   10771 |  8047 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  8048 | `			SyBlob sResolved;` |
|       - |  8049 | `			SyString sBaseName;` |
|       - |  8050 | `			sxu32 nRefLine;` |
|       - |  8051 | `			/* Extract base interface */` |
|   10771 |  8052 | `			pGen->pIn++;` |
|   10771 |  8053 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10771 |  8054 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10771 |  8055 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8056 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  8057 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8058 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  8059 | `					pName);` |
|     ! 0 |  8060 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8061 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8062 | `					return SXERR_ABORT;` |
|       - |  8063 | `				}` |
|     ! 0 |  8064 | `				return SXRET_OK;` |
|       - |  8065 | `			}` |
|   16154 |  8066 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   10766 |  8067 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10771 |  8068 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8069 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8070 | `			/* Only interfaces is allowed */` |
|   10771 |  8071 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8072 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8073 | `			}` |
|   10771 |  8074 | `			if( pBase == 0 ){` |
|     ! 0 |  8075 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8076 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  8077 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8078 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8079 | `					return SXERR_ABORT;` |
|       - |  8080 | `				}` |
|     ! 0 |  8081 | `			}` |
|   10771 |  8082 | `			SyBlobRelease(&sResolved);` |
|    5383 |  8083 | `		}` |
|    5383 |  8084 | `	}` |
|   39533 |  8085 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8086 | `		/* Syntax error */` |
|     ! 0 |  8087 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  8088 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8089 | `		if( rc == SXERR_ABORT ){` |
|       - |  8090 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8091 | `			return SXERR_ABORT;` |
|       - |  8092 | `		}` |
|     ! 0 |  8093 | `		return SXRET_OK;` |
|       - |  8094 | `	}` |
|   39533 |  8095 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   39533 |  8096 | `	pEnd = 0; /* cc warning */` |
|       - |  8097 | `	/* Delimit the interface body */` |
|   39533 |  8098 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   39533 |  8099 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8100 | `		/* Syntax error */` |
|     ! 0 |  8101 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  8102 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8103 | `		if( rc == SXERR_ABORT ){` |
|       - |  8104 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8105 | `			return SXERR_ABORT;` |
|       - |  8106 | `		}` |
|     ! 0 |  8107 | `		return SXRET_OK;` |
|       - |  8108 | `	}` |
|       - |  8109 | `	/* Swap token stream */` |
|   39533 |  8110 | `	pTmp = pGen->pEnd;` |
|   39533 |  8111 | `	pGen->pEnd = pEnd;` |
|       - |  8112 | `	/* Start the parse process` |
|       - |  8113 | `	 * Note (According to the PHP reference manual):` |
|       - |  8114 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  8115 | `	 *  Only 'public' visibility is allowed.` |
|       - |  8116 | `	 */` |
|   66406 |  8117 | `	for(;;){` |
|       - |  8118 | `		/* Jump leading/trailing semi-colons */` |
|  226101 |  8119 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   93289 |  8120 | `			pGen->pIn++;` |
|       5 |  8121 | `		}` |
|  132817 |  8122 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8123 | `			/* End of interface body */` |
|   39531 |  8124 | `			break;` |
|       - |  8125 | `		}` |
|   93291 |  8126 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8127 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8128 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  8129 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8130 | `			if( rc == SXERR_ABORT ){` |
|       - |  8131 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8132 | `				return SXERR_ABORT;` |
|       - |  8133 | `			}` |
|     ! 0 |  8134 | `			goto done;` |
|       - |  8135 | `		}` |
|       - |  8136 | `		/* Extract the current keyword */` |
|   93291 |  8137 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   93291 |  8138 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  8139 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  8140 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  8141 | `			const char *zKind = "member";` |
|       3 |  8142 | `			SyString *pMemberName = 0;` |
|       3 |  8143 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  8144 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  8145 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  8146 | `					zKind = "constant";` |
|       3 |  8147 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  8148 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  8149 | `					}` |
|       1 |  8150 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8151 | `					zKind = "method";` |
|     ! 0 |  8152 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  8153 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  8154 | `					}` |
|     ! 0 |  8155 | `				}` |
|       1 |  8156 | `			}` |
|       3 |  8157 | `			if( pMemberName ){` |
|       4 |  8158 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  8159 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  8160 | `			}else{` |
|     ! 0 |  8161 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8162 | `					"Access type for interface %s must be public",zKind);` |
|       - |  8163 | `			}` |
|       3 |  8164 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8165 | `				return SXERR_ABORT;` |
|       - |  8166 | `			}` |
|       3 |  8167 | `			goto done;` |
|       - |  8168 | `		}` |
|   93289 |  8169 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8170 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8171 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8172 | `			if( rc == SXERR_ABORT ){` |
|       - |  8173 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8174 | `				return SXERR_ABORT;` |
|       - |  8175 | `			}` |
|     ! 0 |  8176 | `			goto done;` |
|       - |  8177 | `		}` |
|   93289 |  8178 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  8179 | `			/* Advance the stream cursor */` |
|   93279 |  8180 | `			pGen->pIn++;` |
|   93279 |  8181 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8182 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8183 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8184 | `				if( rc == SXERR_ABORT ){` |
|       - |  8185 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8186 | `					return SXERR_ABORT;` |
|       - |  8187 | `				}` |
|     ! 0 |  8188 | `				goto done;` |
|       - |  8189 | `			}` |
|   93279 |  8190 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   93279 |  8191 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8192 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8193 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8194 | `				if( rc == SXERR_ABORT ){` |
|       - |  8195 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8196 | `					return SXERR_ABORT;` |
|       - |  8197 | `				}` |
|     ! 0 |  8198 | `				goto done;` |
|       - |  8199 | `			}` |
|   46637 |  8200 | `		}` |
|   93289 |  8201 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8202 | `			/* Parse constant */` |
|       7 |  8203 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       7 |  8204 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8205 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8206 | `					return SXERR_ABORT;` |
|       - |  8207 | `				}` |
|     ! 0 |  8208 | `				goto done;` |
|       - |  8209 | `			}` |
|       4 |  8210 | `		}else{` |
|   93283 |  8211 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   93283 |  8212 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8213 | `				/* Static method,record that */` |
|   10763 |  8214 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  8215 | `				/* Advance the stream cursor */` |
|   10763 |  8216 | `				pGen->pIn++;` |
|   10758 |  8217 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|   10763 |  8218 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8219 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8220 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8221 | `						if( rc == SXERR_ABORT ){` |
|       - |  8222 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8223 | `							return SXERR_ABORT;` |
|       - |  8224 | `						}` |
|     ! 0 |  8225 | `						goto done;` |
|       - |  8226 | `				}` |
|    5379 |  8227 | `			}` |
|       - |  8228 | `			/* Process method signature (no body for interface methods) */` |
|   93283 |  8229 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   93283 |  8230 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8231 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8232 | `					return SXERR_ABORT;` |
|       - |  8233 | `				}` |
|     ! 0 |  8234 | `				goto done;` |
|       - |  8235 | `			}` |
|       - |  8236 | `		}` |
|       5 |  8237 | `	}` |
|       - |  8238 | `	/* Install the interface */` |
|   39531 |  8239 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   39531 |  8240 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  8241 | `		/* Inherit from the base interface */` |
|   10771 |  8242 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    5383 |  8243 | `	}` |
|   39531 |  8244 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8245 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8246 | `		return SXERR_ABORT;` |
|       - |  8247 | `	}` |
|   19763 |  8248 | `done:` |
|       - |  8249 | `	/* Point beyond the interface body */` |
|   39533 |  8250 | `	pGen->pIn  = &pEnd[1];` |
|   39533 |  8251 | `	pGen->pEnd = pTmp;` |
|   39533 |  8252 | `	return PH7_OK;` |
|   19769 |  8253 | `}` |
|       - |  8254 | `/*` |
|       - |  8255 | ` * Compile a user-defined class.` |
|       - |  8256 | ` * According to the PHP language reference manual` |
|       - |  8257 | ` *  class` |
|       - |  8258 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  8259 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  8260 | ` *  of the properties and methods belonging to the class.` |
|       - |  8261 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  8262 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  8263 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  8264 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  8265 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  8266 | ` *  (called "methods").` |
|       - |  8267 | ` */` |
|       - |  8268 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  8269 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  8270 | `struct TraitUseEntry {` |
|       - |  8271 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  8272 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  8273 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  8274 | `};` |
|       - |  8275 | `/*` |
|       - |  8276 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  8277 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  8278 | ` */` |
|  101652 |  8279 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8280 | `{` |
|       - |  8281 | `	ph7_class **apIface;` |
|       - |  8282 | `	sxu32 nIface,i;` |
|       - |  8283 | `	sxi32 rc;` |
|  101657 |  8284 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  8285 | `		return SXRET_OK;` |
|       - |  8286 | `	}` |
|  101657 |  8287 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  101657 |  8288 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  195143 |  8289 | `	for(i = 0; i < nIface; i++){` |
|   93491 |  8290 | `		ph7_class *pIface = apIface[i];` |
|       - |  8291 | `		SyHashEntry *pEntry;` |
|   93491 |  8292 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  251725 |  8293 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  158239 |  8294 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  8295 | `			ph7_class_method *pImplMeth;` |
|  158239 |  8296 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  8297 | `			/* Find the implementing method in the class */` |
|  158239 |  8298 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  158239 |  8299 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  8300 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  8301 | `			}` |
|       - |  8302 | `			/* Check visibility: interface methods must be implemented as public */` |
|  158225 |  8303 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  8304 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8305 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  8306 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  8307 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8308 | `					return SXERR_ABORT;` |
|       - |  8309 | `				}` |
|       1 |  8310 | `			}` |
|       - |  8311 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  8312 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  8313 | `			 */` |
|       - |  8314 | `			{` |
|  158225 |  8315 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  158225 |  8316 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  158225 |  8317 | `				int sigError = 0;` |
|  158225 |  8318 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  8319 | `					sigError = 1;` |
|  158224 |  8320 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  8321 | `					/* Extra parameters must all have default values */` |
|       6 |  8322 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  8323 | `					sxu32 k;` |
|       8 |  8324 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  8325 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  8326 | `							sigError = 1;` |
|       3 |  8327 | `							break;` |
|       - |  8328 | `						}` |
|       2 |  8329 | `					}` |
|       2 |  8330 | `				}` |
|  158225 |  8331 | `				if( sigError ){` |
|       - |  8332 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  8333 | `					ph7_vm_func_arg *aArgs;` |
|       - |  8334 | `					sxu32 j;` |
|       6 |  8335 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  8336 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  8337 | `					/* Build implementing method signature */` |
|       6 |  8338 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  8339 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  8340 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  8341 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  8342 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8343 | `					}` |
|       - |  8344 | `					/* Build interface method signature */` |
|       6 |  8345 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  8346 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  8347 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  8348 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  8349 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8350 | `					}` |
|       8 |  8351 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8352 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  8353 | `						&pClass->sName,pMName,` |
|       4 |  8354 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  8355 | `						&pIface->sName,pMName,` |
|       4 |  8356 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  8357 | `					SyBlobRelease(&sImplSig);` |
|       6 |  8358 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  8359 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8360 | `						return SXERR_ABORT;` |
|       - |  8361 | `					}` |
|       2 |  8362 | `				}` |
|       - |  8363 | `			}` |
|       5 |  8364 | `		}` |
|   46748 |  8365 | `	}` |
|  101657 |  8366 | `	return SXRET_OK;` |
|   50831 |  8367 | `}` |
|       - |  8368 | `/*` |
|       - |  8369 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  8370 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  8371 | ` */` |
|  101652 |  8372 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8373 | `{` |
|       - |  8374 | `	ph7_class_method *pMeth;` |
|       - |  8375 | `	SyHashEntry *pEntry;` |
|       - |  8376 | `	sxu32 nAbstract;` |
|       - |  8377 | `	SyBlob sMsg;` |
|       - |  8378 | `	sxi32 rc;` |
|       - |  8379 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|  101657 |  8380 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      37 |  8381 | `		return SXRET_OK;` |
|       - |  8382 | `	}` |
|       - |  8383 | `	/* Count abstract methods */` |
|  101625 |  8384 | `	nAbstract = 0;` |
|  101625 |  8385 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  953203 |  8386 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  851583 |  8387 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  851583 |  8388 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  8389 | `			nAbstract++;` |
|       8 |  8390 | `		}` |
|       5 |  8391 | `	}` |
|  101625 |  8392 | `	if( nAbstract == 0 ){` |
|  101611 |  8393 | `		return SXRET_OK;` |
|       - |  8394 | `	}` |
|       - |  8395 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  8396 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  8397 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  8398 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  8399 | `		&pClass->sName,nAbstract,` |
|       7 |  8400 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  8401 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  8402 | `	/* Second pass: list methods with origins */` |
|       - |  8403 | `	{` |
|      18 |  8404 | `		sxu32 nListed = 0;` |
|      18 |  8405 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  8406 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  8407 | `			ph7_class *pOrigin = 0;` |
|       - |  8408 | `			SyString *pMName;` |
|      22 |  8409 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  8410 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  8411 | `				continue;` |
|       - |  8412 | `			}` |
|      20 |  8413 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  8414 | `			if( nListed > 0 ){` |
|       3 |  8415 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  8416 | `			}` |
|       - |  8417 | `			/* Find the origin of this abstract method.` |
|       - |  8418 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  8419 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  8420 | `			 * methods. Abstract class methods only win when the class` |
|       - |  8421 | `			 * itself declared the abstract method (not inherited from` |
|       - |  8422 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  8423 | `			 * class's namespace.` |
|       - |  8424 | `			 */` |
|       - |  8425 | `			{` |
|       - |  8426 | `				ph7_class **apIface;` |
|       - |  8427 | `				ph7_class **apTrait;` |
|       - |  8428 | `				ph7_class *pWalk;` |
|       - |  8429 | `				sxu32 i;` |
|       - |  8430 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  8431 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  8432 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  8433 | `				 */` |
|      20 |  8434 | `				if( pClass->pBase ){` |
|      10 |  8435 | `					pWalk = pClass->pBase;` |
|      18 |  8436 | `					while( pWalk ){` |
|       - |  8437 | `						ph7_class_method *pParentMeth;` |
|      12 |  8438 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      12 |  8439 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  8440 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  8441 | `							 * in this class's ancestor chain.` |
|       - |  8442 | `							 */` |
|      12 |  8443 | `							int fromIface = 0;` |
|      12 |  8444 | `							ph7_class *pAnc = pWalk;` |
|      16 |  8445 | `							while( pAnc ){` |
|       - |  8446 | `								ph7_class **apPI;` |
|       - |  8447 | `								sxu32 j;` |
|      14 |  8448 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      14 |  8449 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  8450 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  8451 | `										fromIface = 1;` |
|      10 |  8452 | `										break;` |
|       - |  8453 | `									}` |
|     ! 0 |  8454 | `								}` |
|      14 |  8455 | `								if( fromIface ) break;` |
|       6 |  8456 | `								pAnc = pAnc->pBase;` |
|       2 |  8457 | `							}` |
|      12 |  8458 | `							if( !fromIface ){` |
|       3 |  8459 | `								pOrigin = pWalk;` |
|       3 |  8460 | `								break;` |
|       - |  8461 | `							}` |
|       4 |  8462 | `						}` |
|      10 |  8463 | `						pWalk = pWalk->pBase;` |
|       2 |  8464 | `					}` |
|       4 |  8465 | `				}` |
|       - |  8466 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  8467 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  8468 | `				 */` |
|      20 |  8469 | `				if( !pOrigin ){` |
|      18 |  8470 | `					pWalk = pClass;` |
|      40 |  8471 | `					while( pWalk && !pOrigin ){` |
|      26 |  8472 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  8473 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  8474 | `							ph7_class *pIface = apIface[i];` |
|      16 |  8475 | `							ph7_class *pDeepest = 0;` |
|      28 |  8476 | `							while( pIface ){` |
|      16 |  8477 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  8478 | `									pDeepest = pIface;` |
|       6 |  8479 | `								}` |
|      16 |  8480 | `								pIface = pIface->pBase;` |
|       4 |  8481 | `							}` |
|      16 |  8482 | `							if( pDeepest ){` |
|      16 |  8483 | `								pOrigin = pDeepest;` |
|      16 |  8484 | `								break;` |
|       - |  8485 | `							}` |
|     ! 0 |  8486 | `						}` |
|      26 |  8487 | `						pWalk = pWalk->pBase;` |
|       4 |  8488 | `					}` |
|       7 |  8489 | `				}` |
|       - |  8490 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  8491 | `				if( !pOrigin ){` |
|       3 |  8492 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  8493 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  8494 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  8495 | `							pOrigin = pClass;` |
|       3 |  8496 | `							break;` |
|       - |  8497 | `						}` |
|     ! 0 |  8498 | `					}` |
|       1 |  8499 | `				}` |
|       - |  8500 | `			}` |
|      20 |  8501 | `			if( pOrigin ){` |
|      20 |  8502 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  8503 | `			}else{` |
|       - |  8504 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  8505 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  8506 | `			}` |
|      20 |  8507 | `			nListed++;` |
|       4 |  8508 | `		}` |
|       - |  8509 | `	}` |
|      18 |  8510 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  8511 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  8512 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  8513 | `	SyBlobRelease(&sMsg);` |
|      18 |  8514 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8515 | `		return SXERR_ABORT;` |
|       - |  8516 | `	}` |
|      18 |  8517 | `	return SXRET_OK;` |
|   50831 |  8518 | `}` |
|       - |  8519 | `/*` |
|       - |  8520 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  8521 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  8522 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  8523 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  8524 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  8525 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  8526 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  8527 | ` */` |
|   97814 |  8528 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  8529 | `{` |
|   97819 |  8530 | `	int isAbsolute = 0;` |
|   97819 |  8531 | `	SyToken *pStart = pGen->pIn;` |
|       - |  8532 | `	SyBlob sName;` |
|   97819 |  8533 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     101 |  8534 | `		isAbsolute = 1;` |
|     101 |  8535 | `		pGen->pIn++;` |
|      48 |  8536 | `	}` |
|   97819 |  8537 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       8 |  8538 | `		pGen->pIn = pStart;` |
|       8 |  8539 | `		return SXERR_INVALID;` |
|       - |  8540 | `	}` |
|   97813 |  8541 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   97813 |  8542 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   97813 |  8543 | `	pGen->pIn++;` |
|  146730 |  8544 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   48927 |  8545 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  8546 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  8547 | `		pGen->pIn++;` |
|      13 |  8548 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  8549 | `		pGen->pIn++;` |
|       1 |  8550 | `	}` |
|   97813 |  8551 | `	if( isAbsolute ){` |
|      99 |  8552 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      52 |  8553 | `	}else{` |
|       - |  8554 | `		SyString sRaw;` |
|   97719 |  8555 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   97719 |  8556 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  8557 | `	}` |
|   97813 |  8558 | `	SyBlobRelease(&sName);` |
|   97813 |  8559 | `	return SXRET_OK;` |
|   48912 |  8560 | `}` |
|       - |  8561 | `/*` |
|       - |  8562 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  8563 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  8564 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  8565 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  8566 | ` * either direction cannot run unbounded.` |
|       - |  8567 | ` */` |
|       - |  8568 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   10930 |  8569 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  8570 | `{` |
|       - |  8571 | `	ph7_class **apParent;` |
|       - |  8572 | `	sxu32 n;` |
|   18311 |  8573 | `	while( pInterface ){` |
|   14563 |  8574 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  8575 | `			return FALSE;` |
|       - |  8576 | `		}` |
|   18163 |  8577 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    7200 |  8578 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    7187 |  8579 | `			return TRUE;` |
|       - |  8580 | `		}` |
|    7381 |  8581 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    7381 |  8582 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  8583 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  8584 | `				return TRUE;` |
|       - |  8585 | `			}` |
|     ! 0 |  8586 | `		}` |
|    7381 |  8587 | `		pInterface = pInterface->pBase;` |
|    7381 |  8588 | `		iDepth++;` |
|       5 |  8589 | `	}` |
|    3753 |  8590 | `	return FALSE;` |
|    5470 |  8591 | `}` |
|   10930 |  8592 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  8593 | `{` |
|   10935 |  8594 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  8595 | `}` |
|       - |  8596 | `/*` |
|       - |  8597 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  8598 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  8599 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  8600 | ` */` |
|    7182 |  8601 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  8602 | `{` |
|    7191 |  8603 | `	while( pBase ){` |
|      10 |  8604 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  8605 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  8606 | `			return TRUE;` |
|       - |  8607 | `		}` |
|      10 |  8608 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8609 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8610 | `			return TRUE;` |
|       - |  8611 | `		}` |
|       5 |  8612 | `		pBase = pBase->pBase;` |
|       1 |  8613 | `	}` |
|    7183 |  8614 | `	return FALSE;` |
|    3596 |  8615 | `}` |
|       - |  8616 | `/*` |
|       - |  8617 | ` * Compile a class declaration, named or anonymous.` |
|       - |  8618 | ` *` |
|       - |  8619 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - |  8620 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - |  8621 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - |  8622 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - |  8623 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - |  8624 | ` * implements, body, install) is shared by both paths.` |
|       - |  8625 | ` */` |
|  101686 |  8626 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - |  8627 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 |  8628 | `{` |
|  101691 |  8629 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8630 | `	ph7_class *pClass,*pBase;` |
|       - |  8631 | `	SyToken *pEnd,*pTmp;` |
|       - |  8632 | `	sxi32 iProtection;` |
|       - |  8633 | `	SySet aInterfaces;` |
|       - |  8634 | `	SySet aUseEntries;` |
|       - |  8635 | `	sxi32 iAttrflags;` |
|       - |  8636 | `	SyString *pName;` |
|       - |  8637 | `	sxi32 nKwrd;` |
|       - |  8638 | `	sxi32 rc;` |
|       - |  8639 | `	/* Jump the 'class' keyword */` |
|  101691 |  8640 | `	pGen->pIn++;` |
|  101691 |  8641 | `	if( pAnonName ){` |
|       - |  8642 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - |  8643 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - |  8644 | `		 * then use the synthesized name. */` |
|      30 |  8645 | `		*ppArgStart = *ppArgEnd = 0;` |
|      30 |  8646 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       7 |  8647 | `			pGen->pIn++; /* Jump '(' */` |
|       7 |  8648 | `			*ppArgStart = pGen->pIn;` |
|      10 |  8649 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       3 |  8650 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       7 |  8651 | `			pGen->pIn = *ppArgEnd;` |
|       7 |  8652 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       3 |  8653 | `		}` |
|      30 |  8654 | `		pName = pAnonName;` |
|      30 |  8655 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      17 |  8656 | `	}else{` |
|  101665 |  8657 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8658 | `			/* Syntax error */` |
|     ! 0 |  8659 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8660 | `			if( rc == SXERR_ABORT ){` |
|       - |  8661 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8662 | `				return SXERR_ABORT;` |
|       - |  8663 | `			}` |
|       - |  8664 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8665 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8666 | `				pGen->pIn++;` |
|     ! 0 |  8667 | `			}` |
|     ! 0 |  8668 | `			return SXRET_OK;` |
|       - |  8669 | `		}` |
|       - |  8670 | `		/* Extract class name */` |
|  101665 |  8671 | `		pName = &pGen->pIn->sData;` |
|       - |  8672 | `		/* Advance the stream cursor */` |
|  101665 |  8673 | `		pGen->pIn++;` |
|       - |  8674 | `		/* Build FQN and obtain a raw class */ {` |
|       - |  8675 | `			SyBlob sFQN;` |
|       - |  8676 | `			SyString sFQNStr;` |
|  101665 |  8677 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|  101665 |  8678 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|  101665 |  8679 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|  101665 |  8680 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|  101665 |  8681 | `			SyBlobRelease(&sFQN);` |
|       - |  8682 | `		}` |
|       - |  8683 | `	}` |
|  101691 |  8684 | `	if( pClass == 0 ){` |
|     ! 0 |  8685 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8686 | `		return SXERR_ABORT;` |
|       - |  8687 | `	}` |
|       - |  8688 | `	/* implemented interfaces and per-use-statement trait containers */` |
|  101691 |  8689 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|  101691 |  8690 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8691 | `	/* Assume a standalone class */` |
|  101691 |  8692 | `	pBase = 0;` |
|  101691 |  8693 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   86409 |  8694 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   86409 |  8695 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8696 | `			SyBlob sResolved;` |
|       - |  8697 | `			SyString sBaseName;` |
|       - |  8698 | `			sxu32 nRefLine;` |
|   75497 |  8699 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   75497 |  8700 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   75497 |  8701 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   75497 |  8702 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8703 | `				SyBlobRelease(&sResolved);` |
|       4 |  8704 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8705 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8706 | `					pName);` |
|       3 |  8707 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8708 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8709 | `					return SXERR_ABORT;` |
|       - |  8710 | `				}` |
|       3 |  8711 | `				return SXRET_OK;` |
|       - |  8712 | `			}` |
|  113240 |  8713 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   75490 |  8714 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   75495 |  8715 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8716 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8717 | `			/* Interfaces are not allowed */` |
|   75495 |  8718 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8719 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8720 | `			}` |
|   75495 |  8721 | `			if( pBase == 0 ){` |
|     ! 0 |  8722 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8723 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8724 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8725 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8726 | `					return SXERR_ABORT;` |
|       - |  8727 | `				}` |
|     ! 0 |  8728 | `			}else{` |
|   75495 |  8729 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8730 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8731 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8732 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8733 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8734 | `						return SXERR_ABORT;` |
|       - |  8735 | `					}` |
|     ! 0 |  8736 | `				}` |
|       - |  8737 | `			}` |
|   75495 |  8738 | `			SyBlobRelease(&sResolved);` |
|   37745 |  8739 | `		}` |
|   86407 |  8740 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8741 | `			ph7_class *pInterface;` |
|       - |  8742 | `			/* Interface implementation */` |
|   10925 |  8743 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5470 |  8744 | `			for(;;){` |
|       - |  8745 | `				SyBlob sResolved;` |
|       - |  8746 | `				SyString sIntName;` |
|       - |  8747 | `				sxu32 nRefLine;` |
|   10935 |  8748 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10935 |  8749 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10935 |  8750 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8751 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8752 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8753 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8754 | `						pName);` |
|     ! 0 |  8755 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8756 | `						return SXERR_ABORT;` |
|       - |  8757 | `					}` |
|     ! 0 |  8758 | `					break;` |
|       - |  8759 | `				}` |
|   21865 |  8760 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   10930 |  8761 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10935 |  8762 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8763 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8764 | `				/* Only interfaces are allowed */` |
|   10935 |  8765 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8766 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8767 | `				}` |
|   10935 |  8768 | `				if( pInterface == 0 ){` |
|     ! 0 |  8769 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8770 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8771 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8772 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8773 | `						return SXERR_ABORT;` |
|       - |  8774 | `					}` |
|     ! 0 |  8775 | `				}else{` |
|       - |  8776 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8777 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8778 | `					 * unless they already extend Exception or Error.` |
|       - |  8779 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8780 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8781 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   10935 |  8782 | `					SyString *pFqn = &pClass->sName;` |
|   10935 |  8783 | `					int bIsExceptionOrError =` |
|    9055 |  8784 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   18192 |  8785 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    9144 |  8786 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3600 |  8787 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   14521 |  8788 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|   10776 |  8789 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3589 |  8790 | `						!bIsExceptionOrError ){` |
|      12 |  8791 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8792 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8793 | `							&pClass->sName);` |
|       9 |  8794 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8795 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8796 | `							return SXERR_ABORT;` |
|       - |  8797 | `						}` |
|       - |  8798 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8799 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  8800 | `					}else{` |
|   10929 |  8801 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8802 | `					}` |
|       - |  8803 | `				}` |
|   10935 |  8804 | `				SyBlobRelease(&sResolved);` |
|   10935 |  8805 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5465 |  8806 | `					break;` |
|       - |  8807 | `				}` |
|      14 |  8808 | `				pGen->pIn++;/* Jump the comma */` |
|       4 |  8809 | `			}` |
|    5460 |  8810 | `		}` |
|   43201 |  8811 | `	}` |
|  101689 |  8812 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8813 | `		/* Syntax error */` |
|     ! 0 |  8814 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8815 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8816 | `		if( rc == SXERR_ABORT ){` |
|       - |  8817 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8818 | `			return SXERR_ABORT;` |
|       - |  8819 | `		}` |
|     ! 0 |  8820 | `		return SXRET_OK;` |
|       - |  8821 | `	}` |
|  101689 |  8822 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|  101689 |  8823 | `	pEnd = 0; /* cc warning */` |
|       - |  8824 | `	/* Delimit the class body */` |
|  101689 |  8825 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|  101689 |  8826 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8827 | `		/* Syntax error */` |
|     ! 0 |  8828 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8829 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8830 | `		if( rc == SXERR_ABORT ){` |
|       - |  8831 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8832 | `			return SXERR_ABORT;` |
|       - |  8833 | `		}` |
|     ! 0 |  8834 | `		return SXRET_OK;` |
|       - |  8835 | `	}` |
|       - |  8836 | `	/* Swap token stream */` |
|  101689 |  8837 | `	pTmp = pGen->pEnd;` |
|  101689 |  8838 | `	pGen->pEnd = pEnd;` |
|       - |  8839 | `	/* Set the inherited flags */` |
|  101689 |  8840 | `	pClass->iFlags = iFlags;` |
|       - |  8841 | `	/* Start the parse process */` |
|  139247 |  8842 | `	for(;;){` |
|       - |  8843 | `		/* Jump leading/trailing semi-colons */` |
|  430617 |  8844 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   76101 |  8845 | `			pGen->pIn++;` |
|       5 |  8846 | `		}` |
|  354521 |  8847 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8848 | `			/* End of class body */` |
|  101657 |  8849 | `			break;` |
|       - |  8850 | `		}` |
|  252864 |  8851 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  126437 |  8852 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  8853 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8854 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8855 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8856 | `			if( rc == SXERR_ABORT ){` |
|       - |  8857 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8858 | `				return SXERR_ABORT;` |
|       - |  8859 | `			}` |
|     ! 0 |  8860 | `			goto done;` |
|       - |  8861 | `		}` |
|       - |  8862 | `		/* Assume public visibility */` |
|  252869 |  8863 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  252869 |  8864 | `		iAttrflags = 0;` |
|       - |  8865 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  8866 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  8867 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  8868 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  252869 |  8869 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8870 | `			int bMod = 0;` |
|     ! 0 |  8871 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8872 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  8873 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  8874 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  8875 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  8876 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  8877 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  8878 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  8879 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  8880 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  8881 | `			}` |
|     ! 0 |  8882 | `			if( !bMod ){` |
|     ! 0 |  8883 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8884 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8885 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8886 | `						return SXERR_ABORT;` |
|       - |  8887 | `					}` |
|     ! 0 |  8888 | `					goto done;` |
|       - |  8889 | `				}` |
|     ! 0 |  8890 | `				continue;` |
|       - |  8891 | `			}` |
|     ! 0 |  8892 | `		}` |
|  252869 |  8893 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8894 | `			/* Extract the current keyword */` |
|  252869 |  8895 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  252869 |  8896 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8897 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8898 | `				TraitUseEntry sUse;` |
|      57 |  8899 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      57 |  8900 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      57 |  8901 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      34 |  8902 | `				for(;;){` |
|       - |  8903 | `					ph7_class *pTrait;` |
|       - |  8904 | `					SyString *pTraitName;` |
|      65 |  8905 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8906 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8907 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8908 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8909 | `							return SXERR_ABORT;` |
|       - |  8910 | `						}` |
|     ! 0 |  8911 | `						break;` |
|       - |  8912 | `					}` |
|      65 |  8913 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8914 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8915 | `						SyBlob sResolved;` |
|      65 |  8916 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      65 |  8917 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     125 |  8918 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      60 |  8919 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      65 |  8920 | `						SyBlobRelease(&sResolved);` |
|       - |  8921 | `					}` |
|       - |  8922 | `					/* Only traits are allowed */` |
|      65 |  8923 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8924 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8925 | `					}` |
|      65 |  8926 | `					if( pTrait == 0 ){` |
|     ! 0 |  8927 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8928 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8929 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8930 | `							return SXERR_ABORT;` |
|       - |  8931 | `						}` |
|     ! 0 |  8932 | `					}else{` |
|      65 |  8933 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8934 | `					}` |
|      65 |  8935 | `					pGen->pIn++; /* Advance past trait name */` |
|      65 |  8936 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      31 |  8937 | `						break;` |
|       - |  8938 | `					}` |
|      10 |  8939 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  8940 | `				}` |
|       - |  8941 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      57 |  8942 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8943 | `					SyToken *pBlock;` |
|      12 |  8944 | `					pGen->pIn++; /* Jump '{' */` |
|      12 |  8945 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      12 |  8946 | `					sUse.pResolvStart = pGen->pIn;` |
|      12 |  8947 | `					sUse.pResolvEnd = pBlock;` |
|      12 |  8948 | `					if( pBlock < pGen->pEnd ){` |
|      12 |  8949 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       7 |  8950 | `					}else{` |
|     ! 0 |  8951 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8952 | `					}` |
|       5 |  8953 | `				}` |
|      57 |  8954 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8955 | `				/* The semicolon will be consumed by the outer loop */` |
|      57 |  8956 | `				continue;` |
|       - |  8957 | `			}` |
|  252817 |  8958 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  252513 |  8959 | `				iProtection = nKwrd;` |
|  252513 |  8960 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  8961 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  252513 |  8962 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  8963 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  8964 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  8965 | `				}` |
|  252508 |  8966 | `				if( pGen->pIn >= pGen->pEnd` |
|  252513 |  8967 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  8968 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8969 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8970 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8971 | `					if( rc == SXERR_ABORT ){` |
|       - |  8972 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8973 | `						return SXERR_ABORT;` |
|       - |  8974 | `					}` |
|     ! 0 |  8975 | `					goto done;` |
|       - |  8976 | `				}` |
|  252513 |  8977 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8978 | `					/* Attribute declaration (untyped) */` |
|   75793 |  8979 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   75793 |  8980 | `					if( rc != SXRET_OK ){` |
|       9 |  8981 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8982 | `							return SXERR_ABORT;` |
|       - |  8983 | `						}` |
|       9 |  8984 | `						goto done;` |
|       - |  8985 | `					}` |
|   75787 |  8986 | `					continue;` |
|       - |  8987 | `				}` |
|  176725 |  8988 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8989 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     173 |  8990 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     173 |  8991 | `					if( rc != SXRET_OK ){` |
|       8 |  8992 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8993 | `							return SXERR_ABORT;` |
|       - |  8994 | `						}` |
|       8 |  8995 | `						goto done;` |
|       - |  8996 | `					}` |
|     167 |  8997 | `					continue;` |
|       - |  8998 | `				}` |
|       - |  8999 | `				/* Extract the keyword */` |
|  176557 |  9000 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   88276 |  9001 | `			}` |
|  176861 |  9002 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  9003 | `				/* Process constant declaration */` |
|      77 |  9004 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      77 |  9005 | `				if( rc != SXRET_OK ){` |
|       9 |  9006 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9007 | `						return SXERR_ABORT;` |
|       - |  9008 | `					}` |
|       9 |  9009 | `					goto done;` |
|       - |  9010 | `				}` |
|      38 |  9011 | `			}else{` |
|  176789 |  9012 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  9013 | `					/* Static method or attribute,record that */` |
|   10823 |  9014 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|   10823 |  9015 | `					pGen->pIn++; /* Jump the static keyword */` |
|   10823 |  9016 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9017 | `						/* Extract the keyword */` |
|   10815 |  9018 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   10815 |  9019 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9020 | `							iProtection = nKwrd;` |
|     ! 0 |  9021 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  9022 | `						}` |
|    5405 |  9023 | `					}` |
|       - |  9024 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  9025 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  9026 | `					 * than a generic "expecting method" parse error. */` |
|   10823 |  9027 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9028 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9029 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  9030 | `					}` |
|   10818 |  9031 | `					if( pGen->pIn >= pGen->pEnd` |
|   10823 |  9032 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9033 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9034 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  9035 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9036 | `						if( rc == SXERR_ABORT ){` |
|       - |  9037 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9038 | `							return SXERR_ABORT;` |
|       - |  9039 | `						}` |
|     ! 0 |  9040 | `						goto done;` |
|       - |  9041 | `					}` |
|   10823 |  9042 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9043 | `						/* Attribute declaration */` |
|       8 |  9044 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       8 |  9045 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9046 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9047 | `								return SXERR_ABORT;` |
|       - |  9048 | `							}` |
|     ! 0 |  9049 | `							goto done;` |
|       - |  9050 | `						}` |
|       8 |  9051 | `						continue;` |
|       - |  9052 | `					}` |
|   10817 |  9053 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9054 | `						/* Typed static attribute declaration */` |
|      15 |  9055 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  9056 | `						if( rc != SXRET_OK ){` |
|       3 |  9057 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9058 | `								return SXERR_ABORT;` |
|       - |  9059 | `							}` |
|       3 |  9060 | `							goto done;` |
|       - |  9061 | `						}` |
|      13 |  9062 | `						continue;` |
|       - |  9063 | `					}` |
|       - |  9064 | `					/* Extract the keyword */` |
|   10805 |  9065 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  171371 |  9066 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  9067 | `					/* Abstract method,record that */` |
|      15 |  9068 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  9069 | `					/* Mark the whole class as abstract */` |
|      15 |  9070 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  9071 | `					/* Advance the stream cursor */` |
|      15 |  9072 | `					pGen->pIn++;` |
|      15 |  9073 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      15 |  9074 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      15 |  9075 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      13 |  9076 | `							iProtection = nKwrd;` |
|      13 |  9077 | `							pGen->pIn++; /* Jump the visibility token */` |
|       5 |  9078 | `						}` |
|       6 |  9079 | `					}` |
|      15 |  9080 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      12 |  9081 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9082 | `							/* Static method */` |
|     ! 0 |  9083 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9084 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9085 | `					}` |
|      15 |  9086 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      12 |  9087 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9088 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9089 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  9090 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9091 | `							if( rc == SXERR_ABORT ){` |
|       - |  9092 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9093 | `								return SXERR_ABORT;` |
|       - |  9094 | `							}` |
|     ! 0 |  9095 | `							goto done;` |
|       - |  9096 | `					}` |
|      15 |  9097 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  165965 |  9098 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  9099 | `					/* final method ,record that */` |
|      17 |  9100 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      17 |  9101 | `					pGen->pIn++; /* Jump the final keyword */` |
|      17 |  9102 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9103 | `						/* Extract the keyword */` |
|      17 |  9104 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      17 |  9105 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       9 |  9106 | `							iProtection = nKwrd;` |
|       9 |  9107 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  9108 | `						}` |
|       7 |  9109 | `					}` |
|      17 |  9110 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  9111 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  9112 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  9113 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  9114 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  9115 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9116 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  9117 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  9118 | `									return SXERR_ABORT;` |
|       - |  9119 | `								}` |
|     ! 0 |  9120 | `								goto done;` |
|       - |  9121 | `							}` |
|      12 |  9122 | `							continue;` |
|       - |  9123 | `					}` |
|       6 |  9124 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  9125 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9126 | `							/* Static method */` |
|     ! 0 |  9127 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9128 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9129 | `					}` |
|       6 |  9130 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9131 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9132 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9133 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  9134 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9135 | `							if( rc == SXERR_ABORT ){` |
|       - |  9136 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9137 | `								return SXERR_ABORT;` |
|       - |  9138 | `							}` |
|     ! 0 |  9139 | `							goto done;` |
|       - |  9140 | `					}` |
|       6 |  9141 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9142 | `				}` |
|  176761 |  9143 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9144 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9145 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  9146 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9147 | `						if( rc == SXERR_ABORT ){` |
|       - |  9148 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9149 | `							return SXERR_ABORT;` |
|       - |  9150 | `						}` |
|     ! 0 |  9151 | `						goto done;` |
|       - |  9152 | `				}` |
|  176761 |  9153 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  9154 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  9155 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  9156 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9157 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9158 | `						if( rc == SXERR_ABORT ){` |
|       - |  9159 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9160 | `							return SXERR_ABORT;` |
|       - |  9161 | `						}` |
|     ! 0 |  9162 | `						goto done;` |
|       - |  9163 | `					}` |
|       - |  9164 | `					/* Attribute declaration */` |
|       7 |  9165 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  9166 | `				}else{` |
|       - |  9167 | `					/* Process method declaration */` |
|  176755 |  9168 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9169 | `				}` |
|  176761 |  9170 | `				if( rc != SXRET_OK ){` |
|      16 |  9171 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9172 | `						return SXERR_ABORT;` |
|       - |  9173 | `					}` |
|      16 |  9174 | `					goto done;` |
|       - |  9175 | `				}` |
|       - |  9176 | `			}` |
|   88410 |  9177 | `		}else{` |
|       - |  9178 | `			/* Attribute declaration */` |
|     ! 0 |  9179 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9180 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9181 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9182 | `					return SXERR_ABORT;` |
|       - |  9183 | `				}` |
|     ! 0 |  9184 | `				goto done;` |
|       - |  9185 | `			}` |
|       - |  9186 | `		}` |
|       5 |  9187 | `	}` |
|       - |  9188 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  9189 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  9190 | `	 */` |
|       - |  9191 | `	{` |
|       - |  9192 | `		TraitUseEntry *apUse;` |
|       - |  9193 | `		sxu32 nU;` |
|  101657 |  9194 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|  101709 |  9195 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      57 |  9196 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      57 |  9197 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      57 |  9198 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      57 |  9199 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  9200 | `			sxu32 nT;` |
|      57 |  9201 | `			if( !hasResolution ){` |
|       - |  9202 | `				/* No conflict resolution block: use standard trait application */` |
|      95 |  9203 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      53 |  9204 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      53 |  9205 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9206 | `						break;` |
|       - |  9207 | `					}` |
|      29 |  9208 | `				}` |
|      26 |  9209 | `			}else{` |
|       - |  9210 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  9211 | `				 * then use the block to resolve method conflicts.` |
|       - |  9212 | `				 */` |
|       - |  9213 | `				SyToken *pR;` |
|      24 |  9214 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      14 |  9215 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  9216 | `					ph7_class_attr *pAR;` |
|       - |  9217 | `					SyHashEntry *pER;` |
|       - |  9218 | `					SyString *pNR;` |
|      14 |  9219 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      20 |  9220 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  9221 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  9222 | `						pNR = &pAR->sName;` |
|     ! 0 |  9223 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  9224 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  9225 | `						}` |
|     ! 0 |  9226 | `					}` |
|      14 |  9227 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       8 |  9228 | `				}` |
|       - |  9229 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      12 |  9230 | `				pR = pUse->pResolvStart;` |
|      26 |  9231 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9232 | `					SyString sTrait,sMethod;` |
|       - |  9233 | `					ph7_class *pSrcTrait;` |
|       - |  9234 | `					ph7_class_method *pMeth;` |
|       - |  9235 | `					sxi32 nRKwrd;` |
|      40 |  9236 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      26 |  9237 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      16 |  9238 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      16 |  9239 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      16 |  9240 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      16 |  9241 | `					sMethod = pR->sData;` |
|      16 |  9242 | `					pR++;` |
|      16 |  9243 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9244 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9245 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9246 | `							sTrait = sMethod;` |
|       7 |  9247 | `							pR++;` |
|       7 |  9248 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9249 | `							sMethod = pR->sData;` |
|       7 |  9250 | `							pR++;` |
|       3 |  9251 | `						}` |
|       3 |  9252 | `					}` |
|      16 |  9253 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9254 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9255 | `						continue;` |
|       - |  9256 | `					}` |
|      16 |  9257 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      16 |  9258 | `					pR++;` |
|      16 |  9259 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  9260 | `						pSrcTrait = 0;` |
|       7 |  9261 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  9262 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  9263 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  9264 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  9265 | `								pSrcTrait = apTrait[nT];` |
|       5 |  9266 | `								break;` |
|       - |  9267 | `							}` |
|       2 |  9268 | `						}` |
|       5 |  9269 | `						if( pSrcTrait ){` |
|       5 |  9270 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  9271 | `							if( pMeth ){` |
|       5 |  9272 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  9273 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  9274 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  9275 | `								}` |
|       2 |  9276 | `							}` |
|       2 |  9277 | `						}` |
|       2 |  9278 | `					}` |
|      34 |  9279 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       2 |  9280 | `				}` |
|       - |  9281 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      24 |  9282 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  9283 | `					ph7_class_method *pMR;` |
|       - |  9284 | `					SyHashEntry *pER;` |
|       - |  9285 | `					SyString *pNR;` |
|      14 |  9286 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      40 |  9287 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      22 |  9288 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      22 |  9289 | `						pNR = &pMR->sFunc.sName;` |
|      22 |  9290 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      13 |  9291 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       6 |  9292 | `						}` |
|       2 |  9293 | `					}` |
|       8 |  9294 | `				}` |
|       - |  9295 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      12 |  9296 | `				pR = pUse->pResolvStart;` |
|      26 |  9297 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9298 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  9299 | `					ph7_class *pSrcTrait;` |
|       - |  9300 | `					ph7_class_method *pMeth;` |
|      26 |  9301 | `					int hasQual = 0;` |
|       - |  9302 | `					sxi32 nRKwrd;` |
|      40 |  9303 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      26 |  9304 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      16 |  9305 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      16 |  9306 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      16 |  9307 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      16 |  9308 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      16 |  9309 | `					sMethod = pR->sData;` |
|      16 |  9310 | `					pR++;` |
|      16 |  9311 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9312 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9313 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9314 | `							sTrait = sMethod;` |
|       7 |  9315 | `							hasQual = 1;` |
|       7 |  9316 | `							pR++;` |
|       7 |  9317 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9318 | `							sMethod = pR->sData;` |
|       7 |  9319 | `							pR++;` |
|       3 |  9320 | `						}` |
|       3 |  9321 | `					}` |
|      16 |  9322 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9323 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9324 | `						continue;` |
|       - |  9325 | `					}` |
|      16 |  9326 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      16 |  9327 | `					pR++;` |
|      16 |  9328 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      12 |  9329 | `						sxi32 iNewVis = -1;` |
|      12 |  9330 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  9331 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  9332 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  9333 | `								iNewVis = nAK;` |
|       7 |  9334 | `								pR++;` |
|       3 |  9335 | `							}` |
|       3 |  9336 | `						}` |
|      12 |  9337 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      10 |  9338 | `							sAlias = pR->sData;` |
|      10 |  9339 | `							pR++;` |
|       4 |  9340 | `						}` |
|      12 |  9341 | `						pMeth = 0;` |
|      12 |  9342 | `						if( hasQual ){` |
|       3 |  9343 | `							pSrcTrait = 0;` |
|       5 |  9344 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  9345 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  9346 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  9347 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  9348 | `									pSrcTrait = apTrait[nT];` |
|       3 |  9349 | `									break;` |
|       - |  9350 | `								}` |
|       2 |  9351 | `							}` |
|       3 |  9352 | `							if( pSrcTrait ){` |
|       3 |  9353 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  9354 | `							}` |
|       2 |  9355 | `						}else{` |
|       9 |  9356 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  9357 | `						}` |
|      12 |  9358 | `						if( pMeth ){` |
|      12 |  9359 | `							if( sAlias.nByte > 0 ){` |
|       - |  9360 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  9361 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  9362 | `								 */` |
|       - |  9363 | `								ph7_class_method *pAlias;` |
|       - |  9364 | `								char *zAliasDup;` |
|      10 |  9365 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      10 |  9366 | `								if( pAlias ){` |
|      10 |  9367 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      10 |  9368 | `									if( iNewVis >= 0 ){` |
|       5 |  9369 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9370 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9371 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  9372 | `									}` |
|      10 |  9373 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      10 |  9374 | `									if( zAliasDup ){` |
|      10 |  9375 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       4 |  9376 | `									}` |
|       6 |  9377 | `								}` |
|       7 |  9378 | `							}else if( iNewVis >= 0 ){` |
|       - |  9379 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  9380 | `								ph7_class_method *pCopy;` |
|       3 |  9381 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  9382 | `								if( pCopy ){` |
|       3 |  9383 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  9384 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  9385 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9386 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9387 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  9388 | `									/* Replace the method in the class hash */` |
|       3 |  9389 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  9390 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  9391 | `								}` |
|       1 |  9392 | `							}` |
|       5 |  9393 | `						}` |
|       5 |  9394 | `						SXUNUSED(hasQual);` |
|       5 |  9395 | `					}` |
|      20 |  9396 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       2 |  9397 | `				}` |
|       - |  9398 | `			}` |
|      57 |  9399 | `			SySetRelease(&pUse->aTraits);` |
|      31 |  9400 | `		}` |
|       - |  9401 | `	}` |
|       - |  9402 | `	/* Install the class */` |
|  101657 |  9403 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|  101657 |  9404 | `	if( rc == SXRET_OK ){` |
|       - |  9405 | `		ph7_class **apInterface;` |
|       - |  9406 | `		sxu32 n;` |
|  101657 |  9407 | `		if( pBase ){` |
|       - |  9408 | `			/* Inherit from base class and mark as a subclass */` |
|   75495 |  9409 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   37745 |  9410 | `		}` |
|  101657 |  9411 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  112581 |  9412 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  9413 | `			/* Implements one or more interface */` |
|   10929 |  9414 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   10929 |  9415 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9416 | `				break;` |
|       - |  9417 | `			}` |
|    5467 |  9418 | `		}` |
|       - |  9419 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  9420 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  101652 |  9421 | `		if( rc == SXRET_OK` |
|  101652 |  9422 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|  101657 |  9423 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   82569 |  9424 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  9425 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   82569 |  9426 | `			if( pStringable ){` |
|   82569 |  9427 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   82569 |  9428 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  9429 | `				sxu32 i;` |
|   82569 |  9430 | `				int bAlready = 0;` |
|   89745 |  9431 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    7183 |  9432 | `					if( apImpl[i] == pStringable ){` |
|       3 |  9433 | `						bAlready = 1;` |
|       3 |  9434 | `						break;` |
|       - |  9435 | `					}` |
|    3593 |  9436 | `				}` |
|   82569 |  9437 | `				if( !bAlready ){` |
|   82567 |  9438 | `					PH7_ClassImplement(pClass,pStringable);` |
|   41281 |  9439 | `				}` |
|   41282 |  9440 | `			}` |
|   41282 |  9441 | `		}` |
|       - |  9442 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|  101657 |  9443 | `		if( rc == SXRET_OK ){` |
|  101657 |  9444 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|  101657 |  9445 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9446 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9447 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9448 | `				return SXERR_ABORT;` |
|       - |  9449 | `			}` |
|   50826 |  9450 | `		}` |
|       - |  9451 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|  101657 |  9452 | `		if( rc == SXRET_OK ){` |
|  101657 |  9453 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|  101657 |  9454 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9455 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9456 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9457 | `				return SXERR_ABORT;` |
|       - |  9458 | `			}` |
|   50826 |  9459 | `		}` |
|   50826 |  9460 | `	}` |
|  101657 |  9461 | `	SySetRelease(&aUseEntries);` |
|  101657 |  9462 | `	SySetRelease(&aInterfaces);` |
|  101657 |  9463 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9464 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9465 | `		return SXERR_ABORT;` |
|       - |  9466 | `	}` |
|   50826 |  9467 | `done:` |
|       - |  9468 | `	/* Point beyond the class body */` |
|  101689 |  9469 | `	pGen->pIn = &pEnd[1];` |
|  101689 |  9470 | `	pGen->pEnd = pTmp;` |
|  101689 |  9471 | `	return PH7_OK;` |
|   50848 |  9472 | `}` |
|       - |  9473 | `/* Compile a named class declaration (the common case). */` |
|  101660 |  9474 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  9475 | `{` |
|  101665 |  9476 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 |  9477 | `}` |
|       - |  9478 | `/*` |
|       - |  9479 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - |  9480 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - |  9481 | ` * compile + install the class body once (at compile time, like every other` |
|       - |  9482 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - |  9483 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - |  9484 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - |  9485 | ` */` |
|      26 |  9486 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  9487 | `{` |
|       - |  9488 | `	char zName[128];         /* Synthesized class name */` |
|       - |  9489 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - |  9490 | `	SyString sName;` |
|       - |  9491 | `	SyToken *pArgStart,*pArgEnd;` |
|       - |  9492 | `	ph7_value *pObj;` |
|      30 |  9493 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9494 | `	sxu32 nIdx,nLen;` |
|       - |  9495 | `	sxi32 nArg,rc;` |
|      13 |  9496 | `	SXUNUSED(iCompileFlag);` |
|       - |  9497 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|      30 |  9498 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      30 |  9499 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  9500 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 |  9501 | `	}` |
|      30 |  9502 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  9503 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - |  9504 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - |  9505 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|      30 |  9506 | `	pArgStart = pArgEnd = 0;` |
|      30 |  9507 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      30 |  9508 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9509 | `		return rc;` |
|       - |  9510 | `	}` |
|       - |  9511 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - |  9512 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      30 |  9513 | `	nArg = 0;` |
|      30 |  9514 | `	if( pArgStart < pArgEnd ){` |
|       7 |  9515 | `		SyToken *pSavedIn = pGen->pIn;` |
|       7 |  9516 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  9517 | `		SyToken *pArgNext;` |
|       7 |  9518 | `		pGen->pIn = pArgStart;` |
|       7 |  9519 | `		pGen->pEnd = pArgEnd;` |
|      13 |  9520 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       7 |  9521 | `			if( pGen->pIn < pArgNext ){` |
|       7 |  9522 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       7 |  9523 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9524 | `					pGen->pIn = pSavedIn;` |
|     ! 0 |  9525 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 |  9526 | `					return SXERR_ABORT;` |
|       - |  9527 | `				}` |
|       7 |  9528 | `				nArg++;` |
|       3 |  9529 | `			}` |
|       7 |  9530 | `			pGen->pIn = &pArgNext[1];` |
|       1 |  9531 | `		}` |
|       7 |  9532 | `		pGen->pIn = pSavedIn;` |
|       7 |  9533 | `		pGen->pEnd = pSavedEnd;` |
|       3 |  9534 | `	}` |
|       - |  9535 | `	/* Load the synthesized class name */` |
|      30 |  9536 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      30 |  9537 | `	if( pObj == 0 ){` |
|     ! 0 |  9538 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9539 | `		return SXERR_ABORT;` |
|       - |  9540 | `	}` |
|      30 |  9541 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      30 |  9542 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  9543 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      30 |  9544 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      30 |  9545 | `	return SXRET_OK;` |
|      17 |  9546 | `}` |
|       - |  9547 | `/*` |
|       - |  9548 | ` * Compile a user-defined abstract class.` |
|       - |  9549 | ` *  According to the PHP language reference manual` |
|       - |  9550 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  9551 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  9552 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  9553 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  9554 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  9555 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  9556 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  9557 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  9558 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  9559 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  9560 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  9561 | ` *   could differ.` |
|       - |  9562 | ` */` |
|       - |  9563 | `/*` |
|       - |  9564 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - |  9565 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - |  9566 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - |  9567 | ` */` |
|  984666 |  9568 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 |  9569 | `{` |
|  984671 |  9570 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  658843 |  9571 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  658843 |  9572 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  651653 |  9573 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  325793 |  9574 | `	}` |
|  977419 |  9575 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  977359 |  9576 | `	return FALSE;` |
|  492338 |  9577 | `}` |
|       - |  9578 | `/*` |
|       - |  9579 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - |  9580 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - |  9581 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - |  9582 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - |  9583 | ` */` |
|  977354 |  9584 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 |  9585 | `{` |
|  977359 |  9586 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  977359 |  9587 | `	sxi32 iFlags = 0,iFlag;` |
|  984671 |  9588 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    7317 |  9589 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 |  9590 | `			pDup = pIn;` |
|       2 |  9591 | `		}` |
|    7317 |  9592 | `		iFlags \|= iFlag;` |
|    7317 |  9593 | `		pIn++;` |
|       5 |  9594 | `	}` |
|  977359 |  9595 | `	*ppIn = pIn;` |
|  977359 |  9596 | `	if( ppDup ){ *ppDup = pDup; }` |
|  977359 |  9597 | `	return iFlags;` |
|       5 |  9598 | `}` |
|       - |  9599 | `/*` |
|       - |  9600 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - |  9601 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - |  9602 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - |  9603 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - |  9604 | `` * `readonly`) to their existing handlers.`` |
|       - |  9605 | ` */` |
|  973708 |  9606 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 |  9607 | `{` |
|  973713 |  9608 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  490507 |  9609 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  975533 |  9610 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 |  9611 | `}` |
|       - |  9612 | `/*` |
|       - |  9613 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - |  9614 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - |  9615 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - |  9616 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - |  9617 | `` * `abstract`+`final` pair, like PHP.`` |
|       - |  9618 | ` */` |
|    3646 |  9619 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 |  9620 | `{` |
|       - |  9621 | `	SyToken *pDup;` |
|    3651 |  9622 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - |  9623 | `	sxi32 rc;` |
|    3651 |  9624 | `	if( pDup ){` |
|       4 |  9625 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 |  9626 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 |  9627 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9628 | `			return SXERR_ABORT;` |
|       - |  9629 | `		}` |
|       1 |  9630 | `	}` |
|    3646 |  9631 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    1828 |  9632 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 |  9633 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9634 | `			"Cannot use the final modifier on an abstract class");` |
|       3 |  9635 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9636 | `			return SXERR_ABORT;` |
|       - |  9637 | `		}` |
|       1 |  9638 | `	}` |
|    3651 |  9639 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    1828 |  9640 | `}` |
|       - |  9641 | `/*` |
|       - |  9642 | ` * Compile a user-defined trait.` |
|       - |  9643 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  9644 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  9645 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  9646 | ` */` |
|      64 |  9647 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 |  9648 | `{` |
|      69 |  9649 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9650 | `	ph7_class *pClass;` |
|       - |  9651 | `	SyToken *pEnd,*pTmp;` |
|       - |  9652 | `	sxi32 iProtection;` |
|       - |  9653 | `	sxi32 iAttrflags;` |
|       - |  9654 | `	SyString *pName;` |
|       - |  9655 | `	sxi32 nKwrd;` |
|       - |  9656 | `	sxi32 rc;` |
|       - |  9657 | `	/* Jump the 'trait' keyword */` |
|      69 |  9658 | `	pGen->pIn++;` |
|      69 |  9659 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9660 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  9661 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9662 | `			return SXERR_ABORT;` |
|       - |  9663 | `		}` |
|     ! 0 |  9664 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  9665 | `			pGen->pIn++;` |
|     ! 0 |  9666 | `		}` |
|     ! 0 |  9667 | `		return SXRET_OK;` |
|       - |  9668 | `	}` |
|       - |  9669 | `	/* Extract trait name */` |
|      69 |  9670 | `	pName = &pGen->pIn->sData;` |
|      69 |  9671 | `	pGen->pIn++;` |
|       - |  9672 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  9673 | `		SyBlob sFQN;` |
|       - |  9674 | `		SyString sFQNStr;` |
|      69 |  9675 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      69 |  9676 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      69 |  9677 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      69 |  9678 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      69 |  9679 | `		SyBlobRelease(&sFQN);` |
|       - |  9680 | `	}` |
|      69 |  9681 | `	if( pClass == 0 ){` |
|     ! 0 |  9682 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9683 | `		return SXERR_ABORT;` |
|       - |  9684 | `	}` |
|       - |  9685 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      69 |  9686 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  9687 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  9688 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9689 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9690 | `			return SXERR_ABORT;` |
|       - |  9691 | `		}` |
|     ! 0 |  9692 | `		return SXRET_OK;` |
|       - |  9693 | `	}` |
|      69 |  9694 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      69 |  9695 | `	pEnd = 0;` |
|      69 |  9696 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      69 |  9697 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  9698 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  9699 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9700 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9701 | `			return SXERR_ABORT;` |
|       - |  9702 | `		}` |
|     ! 0 |  9703 | `		return SXRET_OK;` |
|       - |  9704 | `	}` |
|       - |  9705 | `	/* Swap token stream */` |
|      69 |  9706 | `	pTmp = pGen->pEnd;` |
|      69 |  9707 | `	pGen->pEnd = pEnd;` |
|       - |  9708 | `	/* Mark as trait */` |
|      69 |  9709 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  9710 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      64 |  9711 | `	for(;;){` |
|     177 |  9712 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 |  9713 | `			pGen->pIn++;` |
|       4 |  9714 | `		}` |
|     153 |  9715 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      69 |  9716 | `			break;` |
|       - |  9717 | `		}` |
|      89 |  9718 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  9719 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9720 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9721 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9722 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9723 | `				return SXERR_ABORT;` |
|       - |  9724 | `			}` |
|     ! 0 |  9725 | `			goto done;` |
|       - |  9726 | `		}` |
|      89 |  9727 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      89 |  9728 | `		iAttrflags = 0;` |
|      89 |  9729 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      89 |  9730 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      89 |  9731 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9732 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  9733 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  9734 | `				for(;;){` |
|       - |  9735 | `					ph7_class *pUsedTrait;` |
|       - |  9736 | `					SyString *pUsedName;` |
|       5 |  9737 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9738 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9739 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  9740 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9741 | `							return SXERR_ABORT;` |
|       - |  9742 | `						}` |
|     ! 0 |  9743 | `						break;` |
|       - |  9744 | `					}` |
|       5 |  9745 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  9746 | `					{` |
|       - |  9747 | `						SyBlob sResolved;` |
|       5 |  9748 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  9749 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  9750 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  9751 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  9752 | `						SyBlobRelease(&sResolved);` |
|       - |  9753 | `					}` |
|       5 |  9754 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9755 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  9756 | `					}` |
|       5 |  9757 | `					if( pUsedTrait == 0 ){` |
|       4 |  9758 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  9759 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  9760 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9761 | `							return SXERR_ABORT;` |
|       - |  9762 | `						}` |
|       2 |  9763 | `					}else{` |
|       3 |  9764 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  9765 | `					}` |
|       5 |  9766 | `					pGen->pIn++;` |
|       5 |  9767 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  9768 | `						break;` |
|       - |  9769 | `					}` |
|     ! 0 |  9770 | `					pGen->pIn++;` |
|     ! 0 |  9771 | `				}` |
|       5 |  9772 | `				continue;` |
|       - |  9773 | `			}` |
|      85 |  9774 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 |  9775 | `				iProtection = nKwrd;` |
|      73 |  9776 | `				pGen->pIn++;` |
|      68 |  9777 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 |  9778 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9779 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9780 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9781 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9782 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9783 | `						return SXERR_ABORT;` |
|       - |  9784 | `					}` |
|     ! 0 |  9785 | `					goto done;` |
|       - |  9786 | `				}` |
|      73 |  9787 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 |  9788 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9789 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9790 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9791 | `							return SXERR_ABORT;` |
|       - |  9792 | `						}` |
|     ! 0 |  9793 | `						goto done;` |
|       - |  9794 | `					}` |
|      12 |  9795 | `					continue;` |
|       - |  9796 | `				}` |
|      63 |  9797 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  9798 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  9799 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9800 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9801 | `							return SXERR_ABORT;` |
|       - |  9802 | `						}` |
|     ! 0 |  9803 | `						goto done;` |
|       - |  9804 | `					}` |
|       5 |  9805 | `					continue;` |
|       - |  9806 | `				}` |
|      58 |  9807 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 |  9808 | `			}` |
|      71 |  9809 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  9810 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9811 | `					"Traits cannot have constants");` |
|     ! 0 |  9812 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9813 | `					return SXERR_ABORT;` |
|       - |  9814 | `				}` |
|     ! 0 |  9815 | `				goto done;` |
|     ! 0 |  9816 | `			}else{` |
|      71 |  9817 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  9818 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  9819 | `					pGen->pIn++;` |
|       5 |  9820 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  9821 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  9822 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9823 | `							iProtection = nKwrd;` |
|     ! 0 |  9824 | `							pGen->pIn++;` |
|     ! 0 |  9825 | `						}` |
|       1 |  9826 | `					}` |
|       4 |  9827 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  9828 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9829 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9830 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  9831 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9832 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9833 | `							return SXERR_ABORT;` |
|       - |  9834 | `						}` |
|     ! 0 |  9835 | `						goto done;` |
|       - |  9836 | `					}` |
|       5 |  9837 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  9838 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  9839 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9840 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9841 | `								return SXERR_ABORT;` |
|       - |  9842 | `							}` |
|     ! 0 |  9843 | `							goto done;` |
|       - |  9844 | `						}` |
|       3 |  9845 | `						continue;` |
|       - |  9846 | `					}` |
|       3 |  9847 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  9848 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9849 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9850 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9851 | `								return SXERR_ABORT;` |
|       - |  9852 | `							}` |
|     ! 0 |  9853 | `							goto done;` |
|       - |  9854 | `						}` |
|     ! 0 |  9855 | `						continue;` |
|       - |  9856 | `					}` |
|       3 |  9857 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      68 |  9858 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 |  9859 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 |  9860 | `					pGen->pIn++;` |
|       6 |  9861 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 |  9862 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 |  9863 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 |  9864 | `							iProtection = nKwrd;` |
|       6 |  9865 | `							pGen->pIn++;` |
|       2 |  9866 | `						}` |
|       2 |  9867 | `					}` |
|       6 |  9868 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9869 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9870 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9871 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  9872 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9873 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9874 | `							return SXERR_ABORT;` |
|       - |  9875 | `						}` |
|     ! 0 |  9876 | `						goto done;` |
|       - |  9877 | `					}` |
|       6 |  9878 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9879 | `				}` |
|      69 |  9880 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9881 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9882 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  9883 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9884 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9885 | `						return SXERR_ABORT;` |
|       - |  9886 | `					}` |
|     ! 0 |  9887 | `					goto done;` |
|       - |  9888 | `				}` |
|      69 |  9889 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  9890 | `					pGen->pIn++;` |
|     ! 0 |  9891 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  9892 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9893 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9894 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9895 | `							return SXERR_ABORT;` |
|       - |  9896 | `						}` |
|     ! 0 |  9897 | `						goto done;` |
|       - |  9898 | `					}` |
|     ! 0 |  9899 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9900 | `				}else{` |
|      69 |  9901 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9902 | `				}` |
|      69 |  9903 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9904 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9905 | `						return SXERR_ABORT;` |
|       - |  9906 | `					}` |
|     ! 0 |  9907 | `					goto done;` |
|       - |  9908 | `				}` |
|       - |  9909 | `			}` |
|      37 |  9910 | `		}else{` |
|     ! 0 |  9911 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9912 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9913 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9914 | `					return SXERR_ABORT;` |
|       - |  9915 | `				}` |
|     ! 0 |  9916 | `				goto done;` |
|       - |  9917 | `			}` |
|       - |  9918 | `		}` |
|       5 |  9919 | `	}` |
|       - |  9920 | `	/* Install the trait */` |
|      69 |  9921 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      69 |  9922 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9923 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9924 | `		return SXERR_ABORT;` |
|       - |  9925 | `	}` |
|      32 |  9926 | `done:` |
|       - |  9927 | `	/* Point beyond the trait body */` |
|      69 |  9928 | `	pGen->pIn = &pEnd[1];` |
|      69 |  9929 | `	pGen->pEnd = pTmp;` |
|      69 |  9930 | `	return PH7_OK;` |
|      37 |  9931 | `}` |
|       - |  9932 | `/*` |
|       - |  9933 | ` * Compile a user-defined class.` |
|       - |  9934 | ` *  According to the PHP language reference manual` |
|       - |  9935 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  9936 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  9937 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  9938 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  9939 | ` *   and functions (called "methods").` |
|       - |  9940 | ` */` |
|   98014 |  9941 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 |  9942 | `{` |
|       - |  9943 | `	sxi32 rc;` |
|   98019 |  9944 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   98019 |  9945 | `	return rc;` |
|       5 |  9946 | `}` |
|       - |  9947 | `/*` |
|       - |  9948 | ` * Exception handling.` |
|       - |  9949 | ` *  According to the PHP language reference manual` |
|       - |  9950 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  9951 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  9952 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  9953 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  9954 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  9955 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  9956 | ` *    (or re-thrown) within a catch block.` |
|       - |  9957 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  9958 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  9959 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  9960 | ` *    been defined with set_exception_handler().` |
|       - |  9961 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  9962 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  9963 | ` */` |
|       - |  9964 | `/*` |
|       - |  9965 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  9966 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  9967 | ` * indicates failure.` |
|       - |  9968 | ` */` |
|   14676 |  9969 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  9970 | `{` |
|   14681 |  9971 | `	sxi32 rc = SXRET_OK;` |
|   14681 |  9972 | `	if( pRoot->pOp ){` |
|   14671 |  9973 | `		switch( pRoot->pOp->iOp ){` |
|    7333 |  9974 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  9975 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  9976 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  9977 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  9978 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  9979 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   14671 |  9980 | `			break;` |
|     ! 0 |  9981 | `		default:` |
|       - |  9982 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  9983 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  9984 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  9985 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9986 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  9987 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  9988 | `				rc = SXERR_INVALID;` |
|     ! 0 |  9989 | `			}` |
|     ! 0 |  9990 | `			break;` |
|       - |  9991 | `		}` |
|    7348 |  9992 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  9993 | `		/* Unexpected expression */` |
|     ! 0 |  9994 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9995 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9996 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  9997 | `			rc = SXERR_INVALID;` |
|     ! 0 |  9998 | `		}` |
|     ! 0 |  9999 | `	}` |
|   14681 | 10000 | `	return rc;` |
|       5 | 10001 | `}` |
|       - | 10002 | `/*` |
|       - | 10003 | ` * Compile a 'throw' statement.` |
|       - | 10004 | ` * throw: This is how you trigger an exception.` |
|       - | 10005 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 10006 | ` */` |
|   14640 | 10007 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 | 10008 | `{` |
|   14645 | 10009 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10010 | `	GenBlock *pBlock;` |
|       - | 10011 | `	sxu32 nIdx;` |
|       - | 10012 | `	sxi32 rc;` |
|   14645 | 10013 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 10014 | `	/* Compile the expression */` |
|   14645 | 10015 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   14645 | 10016 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10017 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 10018 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10019 | `			return SXERR_ABORT;` |
|       - | 10020 | `		}` |
|     ! 0 | 10021 | `		return SXRET_OK;` |
|       - | 10022 | `	}` |
|   14645 | 10023 | `	pBlock = pGen->pCurrent;` |
|       - | 10024 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   57973 | 10025 | `	while(pBlock->pParent){` |
|   57969 | 10026 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   14641 | 10027 | `			break;` |
|       - | 10028 | `		}` |
|       - | 10029 | `		/* Point to the parent block */` |
|   43333 | 10030 | `		pBlock = pBlock->pParent;` |
|       5 | 10031 | `	}` |
|       - | 10032 | `	/* Emit the throw instruction */` |
|   14645 | 10033 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 10034 | `	/* Emit the jump */` |
|   14645 | 10035 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   14645 | 10036 | `	return SXRET_OK;` |
|    7325 | 10037 | `}` |
|       - | 10038 | `/*` |
|       - | 10039 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - | 10040 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - | 10041 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - | 10042 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - | 10043 | ` * the validator guarantees the operand is a valid exception target.` |
|       - | 10044 | ` */` |
|      36 | 10045 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 10046 | `{` |
|      38 | 10047 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10048 | `	GenBlock *pBlock;` |
|       - | 10049 | `	sxu32 nIdx;` |
|       - | 10050 | `	sxi32 rc;` |
|      18 | 10051 | `	(void)iCompileFlag;` |
|      38 | 10052 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 | 10053 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 10054 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10055 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10056 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10057 | `			return SXERR_ABORT;` |
|       - | 10058 | `		}` |
|     ! 0 | 10059 | `		return SXRET_OK;` |
|       - | 10060 | `	}` |
|      38 | 10061 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 | 10062 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10063 | `		return SXERR_ABORT;` |
|       - | 10064 | `	}` |
|      38 | 10065 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10066 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10067 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10068 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10069 | `			return SXERR_ABORT;` |
|       - | 10070 | `		}` |
|     ! 0 | 10071 | `		return SXRET_OK;` |
|       - | 10072 | `	}` |
|       - | 10073 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 | 10074 | `	pBlock = pGen->pCurrent;` |
|      60 | 10075 | `	while( pBlock->pParent ){` |
|      49 | 10076 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 | 10077 | `			break;` |
|       - | 10078 | `		}` |
|      23 | 10079 | `		pBlock = pBlock->pParent;` |
|       1 | 10080 | `	}` |
|      38 | 10081 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 | 10082 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 | 10083 | `	return SXRET_OK;` |
|      20 | 10084 | `}` |
|       - | 10085 | `/*` |
|       - | 10086 | ` * Compile a 'catch' block.` |
|       - | 10087 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 10088 | ` * an object containing the exception information.` |
|       - | 10089 | ` */` |
|     598 | 10090 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 10091 | `{` |
|     603 | 10092 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10093 | `	ph7_exception_block sCatch;` |
|       - | 10094 | `	SySet *pInstrContainer;` |
|       - | 10095 | `	SyString sClassName;` |
|       - | 10096 | `	GenBlock *pCatch;` |
|       - | 10097 | `	SyToken *pToken;` |
|       - | 10098 | `	SyString *pName;` |
|       - | 10099 | `	char *zDup;` |
|       - | 10100 | `	sxi32 rc;` |
|     603 | 10101 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 10102 | `	/* Zero the structure */` |
|     603 | 10103 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 10104 | `	/* Initialize fields */` |
|     603 | 10105 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     603 | 10106 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     603 | 10107 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 10108 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10109 | `			pToken = pGen->pIn;` |
|     ! 0 | 10110 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10111 | `				pToken--;` |
|     ! 0 | 10112 | `			}` |
|     ! 0 | 10113 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10114 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10115 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10116 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10117 | `				return SXERR_ABORT;` |
|       - | 10118 | `			}` |
|     ! 0 | 10119 | `			return SXERR_INVALID;` |
|       - | 10120 | `	}` |
|       - | 10121 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     603 | 10122 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     313 | 10123 | `	for(;;){` |
|       - | 10124 | `		SyBlob sResolved;` |
|     631 | 10125 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     631 | 10126 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 10127 | `			SyBlobRelease(&sResolved);` |
|       6 | 10128 | `			pToken = pGen->pIn;` |
|       6 | 10129 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10130 | `				pToken--;` |
|     ! 0 | 10131 | `			}` |
|       8 | 10132 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10133 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 10134 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 10135 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10136 | `				return SXERR_ABORT;` |
|       - | 10137 | `			}` |
|       6 | 10138 | `			return SXERR_INVALID;` |
|       - | 10139 | `		}` |
|       - | 10140 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 10141 | `		 * transient SyBlob allocation. */` |
|     938 | 10142 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     622 | 10143 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     627 | 10144 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     627 | 10145 | `		SyBlobRelease(&sResolved);` |
|     627 | 10146 | `		if( zDup == 0 ){` |
|     ! 0 | 10147 | `			goto Mem;` |
|       - | 10148 | `		}` |
|     627 | 10149 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     627 | 10150 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10151 | `			goto Mem;` |
|       - | 10152 | `		}` |
|       - | 10153 | `		/* Check for '\|' (multi-catch separator) */` |
|     622 | 10154 | `		if( pGen->pIn < pGen->pEnd &&` |
|     622 | 10155 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      33 | 10156 | `			pGen->pIn->sData.nByte == 1 &&` |
|      28 | 10157 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      30 | 10158 | `			pGen->pIn++; /* Consume the '\|' */` |
|      30 | 10159 | `			continue;` |
|       - | 10160 | `		}` |
|     599 | 10161 | `		break;` |
|     ! 0 | 10162 | `	}` |
|     594 | 10163 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     599 | 10164 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 10165 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10166 | `			pToken = pGen->pIn;` |
|     ! 0 | 10167 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10168 | `				pToken--;` |
|     ! 0 | 10169 | `			}` |
|     ! 0 | 10170 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10171 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10172 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10173 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10174 | `				return SXERR_ABORT;` |
|       - | 10175 | `			}` |
|     ! 0 | 10176 | `			return SXERR_INVALID;` |
|       - | 10177 | `	}` |
|     599 | 10178 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 10179 | `	/* Duplicate instance name */` |
|     599 | 10180 | `	pName = &pGen->pIn->sData;` |
|     599 | 10181 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     599 | 10182 | `	if( zDup == 0 ){` |
|     ! 0 | 10183 | `		goto Mem;` |
|       - | 10184 | `	}` |
|     599 | 10185 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     599 | 10186 | `	pGen->pIn++;` |
|     599 | 10187 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 10188 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 10189 | `		pToken = pGen->pIn;` |
|     ! 0 | 10190 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10191 | `			pToken--;` |
|     ! 0 | 10192 | `		}` |
|     ! 0 | 10193 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10194 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10195 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10196 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10197 | `			return SXERR_ABORT;` |
|       - | 10198 | `		}` |
|     ! 0 | 10199 | `		return SXERR_INVALID;` |
|       - | 10200 | `	}` |
|       - | 10201 | `	/* Compile the block */` |
|     599 | 10202 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 10203 | `	/* Create the catch block */` |
|     599 | 10204 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     599 | 10205 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10206 | `		return SXERR_ABORT;` |
|       - | 10207 | `	}` |
|       - | 10208 | `	/* Swap bytecode container */` |
|     599 | 10209 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     599 | 10210 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 10211 | `	/* Compile the block */` |
|     599 | 10212 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 10213 | `	/* Fix forward jumps now the destination is resolved  */` |
|     599 | 10214 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10215 | `	/* Emit the DONE instruction */` |
|     599 | 10216 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10217 | `	/* Leave the block */` |
|     599 | 10218 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10219 | `	/* Restore the default container */` |
|     599 | 10220 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10221 | `	/* Install the catch block */` |
|     599 | 10222 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     599 | 10223 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10224 | `		goto Mem;` |
|       - | 10225 | `	}` |
|     599 | 10226 | `	return SXRET_OK;` |
|     ! 0 | 10227 | `Mem:` |
|     ! 0 | 10228 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10229 | `	return SXERR_ABORT;` |
|     304 | 10230 | `}` |
|       - | 10231 | `/*` |
|       - | 10232 | ` * Compile a 'try' block.` |
|       - | 10233 | ` * A function using an exception should be in a "try" block.` |
|       - | 10234 | ` * If the exception does not trigger, the code will continue` |
|       - | 10235 | ` * as normal. However if the exception triggers, an exception` |
|       - | 10236 | ` * is "thrown".` |
|       - | 10237 | ` */` |
|     644 | 10238 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 10239 | `{` |
|       - | 10240 | `	ph7_exception *pException;` |
|     649 | 10241 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10242 | `	GenBlock *pTry;` |
|       - | 10243 | `	sxu32 nJmpIdx;` |
|       - | 10244 | `	sxi32 rc;` |
|       - | 10245 | `	/* Create the exception container */` |
|     649 | 10246 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     649 | 10247 | `	if( pException == 0 ){` |
|     ! 0 | 10248 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 10249 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10250 | `		return SXERR_ABORT;` |
|       - | 10251 | `	}` |
|       - | 10252 | `	/* Zero the structure */` |
|     649 | 10253 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 10254 | `	/* Initialize fields */` |
|     649 | 10255 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     649 | 10256 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     649 | 10257 | `	pException->iHasFinally = 0;` |
|     649 | 10258 | `	pException->iFinallyDone = 0;` |
|     649 | 10259 | `	pException->pVm = pGen->pVm;` |
|       - | 10260 | `	/* Create the try block */` |
|     649 | 10261 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     649 | 10262 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10263 | `		return SXERR_ABORT;` |
|       - | 10264 | `	}` |
|       - | 10265 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     649 | 10266 | `	pTry->pUserData = pException;` |
|       - | 10267 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     649 | 10268 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 10269 | `	/* Fix the jump later when the destination is resolved */` |
|     649 | 10270 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     649 | 10271 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 10272 | `	/* Compile the block */` |
|     649 | 10273 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     649 | 10274 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10275 | `		return SXERR_ABORT;` |
|       - | 10276 | `	}` |
|       - | 10277 | `	/* Fix forward jumps now the destination is resolved */` |
|     649 | 10278 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10279 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     649 | 10280 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 10281 | `	/* Leave the block */` |
|     649 | 10282 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10283 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     649 | 10284 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     642 | 10285 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 10286 | `		/* Compile one or more catch blocks */` |
|     594 | 10287 | `		for(;;){` |
|    1188 | 10288 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     962 | 10289 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     300 | 10290 | `					break;` |
|       - | 10291 | `			}` |
|     603 | 10292 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     603 | 10293 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10294 | `				return SXERR_ABORT;` |
|       - | 10295 | `			}` |
|       5 | 10296 | `		}` |
|     295 | 10297 | `	}` |
|       - | 10298 | `	/* Compile optional finally block */` |
|     649 | 10299 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     354 | 10300 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10301 | `		SySet *pInstrContainer;` |
|       - | 10302 | `		GenBlock *pFinBlock;` |
|     115 | 10303 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 10304 | `		/* Create the finally block for jump fixup bookkeeping */` |
|     115 | 10305 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     115 | 10306 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10307 | `			return SXERR_ABORT;` |
|       - | 10308 | `		}` |
|       - | 10309 | `		/* Swap bytecode container */` |
|     115 | 10310 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     115 | 10311 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 10312 | `		/* Compile the finally body */` |
|     115 | 10313 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     115 | 10314 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10315 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 10316 | `			return SXERR_ABORT;` |
|       - | 10317 | `		}` |
|       - | 10318 | `		/* Fix forward jumps now the destination is resolved */` |
|     115 | 10319 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10320 | `		/* Emit DONE to terminate the finally block */` |
|     115 | 10321 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10322 | `		/* Leave the block */` |
|     115 | 10323 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10324 | `		/* Restore the default container */` |
|     115 | 10325 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     115 | 10326 | `		pException->iHasFinally = 1;` |
|      55 | 10327 | `	}` |
|       - | 10328 | `	/* Must have at least one catch or finally */` |
|     649 | 10329 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       8 | 10330 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10331 | `			"Cannot use try without catch or finally");` |
|       8 | 10332 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10333 | `			return SXERR_ABORT;` |
|       - | 10334 | `		}` |
|       3 | 10335 | `	}` |
|     649 | 10336 | `	return SXRET_OK;` |
|     327 | 10337 | `}` |
|       - | 10338 | `/*` |
|       - | 10339 | ` * Compile a switch block.` |
|       - | 10340 | ` *  (See block-comment below for more information)` |
|       - | 10341 | ` */` |
|     112 | 10342 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 10343 | `{` |
|     117 | 10344 | `	sxi32 rc = SXRET_OK;` |
|     117 | 10345 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 10346 | `		/* Unexpected token */` |
|     ! 0 | 10347 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10348 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10349 | `			return SXERR_ABORT;` |
|       - | 10350 | `		}` |
|     ! 0 | 10351 | `		pGen->pIn++;` |
|     ! 0 | 10352 | `	}` |
|     117 | 10353 | `	pGen->pIn++;` |
|       - | 10354 | `	/* First instruction to execute in this block. */` |
|     117 | 10355 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10356 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 10357 | `	 * or the '}' token */` |
|     206 | 10358 | `	for(;;){` |
|     417 | 10359 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10360 | `			/* No more input to process */` |
|     ! 0 | 10361 | `			break;` |
|       - | 10362 | `		}` |
|     417 | 10363 | `		rc = SXRET_OK;` |
|     417 | 10364 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 | 10365 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 | 10366 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 10367 | `					/* Unexpected token */` |
|     ! 0 | 10368 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10369 | `						&pGen->pIn->sData);` |
|     ! 0 | 10370 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10371 | `						return SXERR_ABORT;` |
|       - | 10372 | `					}` |
|       - | 10373 | `					/* FALL THROUGH */` |
|     ! 0 | 10374 | `				}` |
|      31 | 10375 | `				rc = SXERR_EOF;` |
|      31 | 10376 | `				break;` |
|       - | 10377 | `			}` |
|      32 | 10378 | `		}else{` |
|       - | 10379 | `			sxi32 nKwrd;` |
|       - | 10380 | `			/* Extract the keyword */` |
|     337 | 10381 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 | 10382 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 | 10383 | `				break;` |
|       - | 10384 | `			}` |
|     253 | 10385 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10386 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 10387 | `					/* Unexpected token */` |
|     ! 0 | 10388 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10389 | `						&pGen->pIn->sData);` |
|     ! 0 | 10390 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10391 | `						return SXERR_ABORT;` |
|       - | 10392 | `					}` |
|       - | 10393 | `					/* FALL THROUGH */` |
|     ! 0 | 10394 | `				}` |
|       - | 10395 | `				/* Block compiled */` |
|       3 | 10396 | `				break;` |
|       - | 10397 | `			}` |
|       - | 10398 | `		}` |
|       - | 10399 | `		/* Compile block */` |
|     305 | 10400 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 | 10401 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10402 | `			return SXERR_ABORT;` |
|       - | 10403 | `		}` |
|       5 | 10404 | `	}` |
|     117 | 10405 | `	return rc;` |
|      61 | 10406 | `}` |
|       - | 10407 | `/*` |
|       - | 10408 | ` * Compile a case eXpression.` |
|       - | 10409 | ` *  (See block-comment below for more information)` |
|       - | 10410 | ` */` |
|      92 | 10411 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 10412 | `{` |
|       - | 10413 | `	SySet *pInstrContainer;` |
|       - | 10414 | `	SyToken *pEnd,*pTmp;` |
|      97 | 10415 | `	sxi32 iNest = 0;` |
|       - | 10416 | `	sxi32 rc;` |
|       - | 10417 | `	/* Delimit the expression */` |
|      97 | 10418 | `	pEnd = pGen->pIn;` |
|     197 | 10419 | `	while( pEnd < pGen->pEnd ){` |
|     197 | 10420 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 10421 | `			/* Increment nesting level */` |
|       3 | 10422 | `			iNest++;` |
|     196 | 10423 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 10424 | `			/* Decrement nesting level */` |
|       3 | 10425 | `			iNest--;` |
|     194 | 10426 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 | 10427 | `			break;` |
|       - | 10428 | `		}` |
|     105 | 10429 | `		pEnd++;` |
|       5 | 10430 | `	}` |
|      97 | 10431 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 10432 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 10433 | `		if( rc == SXERR_ABORT ){` |
|       - | 10434 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10435 | `			return SXERR_ABORT;` |
|       - | 10436 | `		}` |
|     ! 0 | 10437 | `	}` |
|       - | 10438 | `	/* Swap token stream */` |
|      97 | 10439 | `	pTmp = pGen->pEnd;` |
|      97 | 10440 | `	pGen->pEnd = pEnd;` |
|      97 | 10441 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 | 10442 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 | 10443 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 10444 | `	/* Emit the done instruction */` |
|      97 | 10445 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 | 10446 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10447 | `	/* Update token stream */` |
|      97 | 10448 | `	pGen->pIn  = pEnd;` |
|      97 | 10449 | `	pGen->pEnd = pTmp;` |
|      97 | 10450 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10451 | `		return SXERR_ABORT;` |
|       - | 10452 | `	}` |
|      97 | 10453 | `	return SXRET_OK;` |
|      51 | 10454 | `}` |
|       - | 10455 | `/*` |
|       - | 10456 | ` * Compile the smart switch statement.` |
|       - | 10457 | ` * According to the PHP language reference manual` |
|       - | 10458 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 10459 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 10460 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 10461 | ` *  This is exactly what the switch statement is for.` |
|       - | 10462 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 10463 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 10464 | ` *  of the outer loop, use continue 2.` |
|       - | 10465 | ` *  Note that switch/case does loose comparision.` |
|       - | 10466 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 10467 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 10468 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 10469 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 10470 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 10471 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 10472 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 10473 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 10474 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 10475 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 10476 | ` *  list for the next case.` |
|       - | 10477 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 10478 | ` *  or floating-point numbers and strings.` |
|       - | 10479 | ` */` |
|      28 | 10480 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 10481 | `{` |
|       - | 10482 | `	GenBlock *pSwitchBlock;` |
|       - | 10483 | `	SyToken *pTmp,*pEnd;` |
|       - | 10484 | `	ph7_switch *pSwitch;` |
|       - | 10485 | `	sxu32 nToken;` |
|       - | 10486 | `	sxu32 nLine;` |
|       - | 10487 | `	sxi32 rc;` |
|      33 | 10488 | `	nLine = pGen->pIn->nLine;` |
|       - | 10489 | `	/* Jump the 'switch' keyword */` |
|      33 | 10490 | `	pGen->pIn++;` |
|      33 | 10491 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 10492 | `		/* Syntax error */` |
|     ! 0 | 10493 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 10494 | `		if( rc == SXERR_ABORT ){` |
|       - | 10495 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10496 | `			return SXERR_ABORT;` |
|       - | 10497 | `		}` |
|     ! 0 | 10498 | `		goto Synchronize;` |
|       - | 10499 | `	}` |
|       - | 10500 | `	/* Jump the left parenthesis '(' */` |
|      33 | 10501 | `	pGen->pIn++;` |
|      33 | 10502 | `	pEnd = 0; /* cc warning */` |
|       - | 10503 | `	/* Create the loop block */` |
|      47 | 10504 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 10505 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 | 10506 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10507 | `		return SXERR_ABORT;` |
|       - | 10508 | `	}` |
|       - | 10509 | `	/* Delimit the condition */` |
|      33 | 10510 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 | 10511 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 10512 | `		/* Empty expression */` |
|     ! 0 | 10513 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 10514 | `		if( rc == SXERR_ABORT ){` |
|       - | 10515 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10516 | `			return SXERR_ABORT;` |
|       - | 10517 | `		}` |
|     ! 0 | 10518 | `	}` |
|       - | 10519 | `	/* Swap token streams */` |
|      33 | 10520 | `	pTmp = pGen->pEnd;` |
|      33 | 10521 | `	pGen->pEnd = pEnd;` |
|       - | 10522 | `	/* Compile the expression */` |
|      33 | 10523 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 | 10524 | `	if( rc == SXERR_ABORT ){` |
|       - | 10525 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 10526 | `		return SXERR_ABORT;` |
|       - | 10527 | `	}` |
|       - | 10528 | `	/* Update token stream */` |
|      33 | 10529 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 10530 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 10531 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10532 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10533 | `			return SXERR_ABORT;` |
|       - | 10534 | `		}` |
|     ! 0 | 10535 | `		pGen->pIn++;` |
|     ! 0 | 10536 | `	}` |
|      33 | 10537 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 10538 | `	pGen->pEnd = pTmp;` |
|      33 | 10539 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 10540 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 10541 | `			pTmp = pGen->pIn;` |
|     ! 0 | 10542 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 10543 | `				pTmp--;` |
|     ! 0 | 10544 | `			}` |
|       - | 10545 | `			/* Unexpected token */` |
|     ! 0 | 10546 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 10547 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10548 | `				return SXERR_ABORT;` |
|       - | 10549 | `			}` |
|     ! 0 | 10550 | `			goto Synchronize;` |
|       - | 10551 | `	}` |
|       - | 10552 | `	/* Set the delimiter token */` |
|      33 | 10553 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 10554 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 10555 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 10556 | `	}else{` |
|      31 | 10557 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 10558 | `	}` |
|      33 | 10559 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 10560 | `	/* Create the switch blocks container */` |
|      33 | 10561 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 10562 | `	if( pSwitch == 0 ){` |
|       - | 10563 | `		/* Abort compilation */` |
|     ! 0 | 10564 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10565 | `		return SXERR_ABORT;` |
|       - | 10566 | `	}` |
|       - | 10567 | `	/* Zero the structure */` |
|      33 | 10568 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 10569 | `	/* Initialize fields */` |
|      33 | 10570 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 10571 | `	/* Emit the switch instruction */` |
|      33 | 10572 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 10573 | `	/* Compile case blocks */` |
|     100 | 10574 | `	for(;;){` |
|       - | 10575 | `		sxu32 nKwrd;` |
|     119 | 10576 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10577 | `			/* No more input to process */` |
|     ! 0 | 10578 | `			break;` |
|       - | 10579 | `		}` |
|     119 | 10580 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 10581 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 10582 | `				/* Unexpected token */` |
|     ! 0 | 10583 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10584 | `					&pGen->pIn->sData);` |
|     ! 0 | 10585 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10586 | `					return SXERR_ABORT;` |
|       - | 10587 | `				}` |
|       - | 10588 | `				/* FALL THROUGH */` |
|     ! 0 | 10589 | `			}` |
|       - | 10590 | `			/* Block compiled */` |
|     ! 0 | 10591 | `			break;` |
|       - | 10592 | `		}` |
|       - | 10593 | `		/* Extract the keyword */` |
|     119 | 10594 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 10595 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10596 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 10597 | `				/* Unexpected token */` |
|     ! 0 | 10598 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10599 | `					&pGen->pIn->sData);` |
|     ! 0 | 10600 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10601 | `					return SXERR_ABORT;` |
|       - | 10602 | `				}` |
|       - | 10603 | `				/* FALL THROUGH */` |
|     ! 0 | 10604 | `			}` |
|       - | 10605 | `			/* Block compiled */` |
|       3 | 10606 | `			break;` |
|       - | 10607 | `		}` |
|     117 | 10608 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 10609 | `			/*` |
|       - | 10610 | `			 * Accroding to the PHP language reference manual` |
|       - | 10611 | `			 *  A special case is the default case. This case matches anything` |
|       - | 10612 | `			 *  that wasn't matched by the other cases.` |
|       - | 10613 | `			 */` |
|      25 | 10614 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 10615 | `				/* Default case already compiled */` |
|     ! 0 | 10616 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 10617 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10618 | `					return SXERR_ABORT;` |
|       - | 10619 | `				}` |
|     ! 0 | 10620 | `			}` |
|      25 | 10621 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 10622 | `			/* Compile the default block */` |
|      25 | 10623 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 10624 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10625 | `				return SXERR_ABORT;` |
|      25 | 10626 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 10627 | `				break;` |
|       1 | 10628 | `			}` |
|      98 | 10629 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 10630 | `			ph7_case_expr sCase;` |
|       - | 10631 | `			/* Standard case block */` |
|      97 | 10632 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 10633 | `			/* initialize the structure */` |
|      97 | 10634 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 10635 | `			/* Compile the case expression */` |
|      97 | 10636 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 10637 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10638 | `				return SXERR_ABORT;` |
|       - | 10639 | `			}` |
|       - | 10640 | `			/* Compile the case block */` |
|      97 | 10641 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 10642 | `			/* Insert in the switch container */` |
|      97 | 10643 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 10644 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10645 | `				return SXERR_ABORT;` |
|      97 | 10646 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 10647 | `				break;` |
|       - | 10648 | `			}` |
|      47 | 10649 | `		}else{` |
|       - | 10650 | `			/* Unexpected token */` |
|     ! 0 | 10651 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10652 | `				&pGen->pIn->sData);` |
|     ! 0 | 10653 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10654 | `				return SXERR_ABORT;` |
|       - | 10655 | `			}` |
|     ! 0 | 10656 | `			break;` |
|       - | 10657 | `		}` |
|       5 | 10658 | `	}` |
|       - | 10659 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 10660 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 10661 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10662 | `	/* Release the loop block */` |
|      33 | 10663 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 10664 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 10665 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 10666 | `		pGen->pIn++;` |
|      14 | 10667 | `	}` |
|       - | 10668 | `	/* Statement successfully compiled */` |
|      33 | 10669 | `	return SXRET_OK;` |
|     ! 0 | 10670 | `Synchronize:` |
|       - | 10671 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 10672 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 10673 | `		pGen->pIn++;` |
|     ! 0 | 10674 | `	}` |
|     ! 0 | 10675 | `	return SXRET_OK;` |
|      19 | 10676 | `}` |
|       - | 10677 | `/*` |
|       - | 10678 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 10679 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 10680 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 10681 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 10682 | ` */` |
|       - | 10683 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 10684 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 10685 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 10686 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 10687 |  |
|       - | 10688 | `/*` |
|       - | 10689 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 10690 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 10691 | ` * patched entries from the pending set.` |
|       - | 10692 | ` */` |
| 2666064 | 10693 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 10694 | `{` |
| 2666069 | 10695 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 10696 | `	sxu32 nTarget;` |
|       - | 10697 | `	sxu32 *aIdx;` |
|       - | 10698 | `	sxu32 i;` |
| 2666069 | 10699 | `	if( nCur <= nBaseline ){` |
| 2665975 | 10700 | `		return;` |
|       - | 10701 | `	}` |
|      98 | 10702 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      98 | 10703 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     200 | 10704 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     106 | 10705 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     106 | 10706 | `		if( pInstr ){` |
|     106 | 10707 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      51 | 10708 | `		}` |
|      55 | 10709 | `	}` |
|      98 | 10710 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1333037 | 10711 | `}` |
|       - | 10712 |  |
|       - | 10713 | `/*` |
|       - | 10714 | ` * By-reference out-parameters of builtin functions.` |
|       - | 10715 | ` *` |
|       - | 10716 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 10717 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 10718 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 10719 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 10720 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 10721 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 10722 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 10723 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 10724 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 10725 | ` * creates it" behaviour).` |
|       - | 10726 | ` *` |
|       - | 10727 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 10728 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 10729 | ` */` |
|  447928 | 10730 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 10731 | `{` |
|       - | 10732 | `	static const struct {` |
|       - | 10733 | `		const char *zName;` |
|       - | 10734 | `		sxu32 nByte;` |
|       - | 10735 | `		sxu32 mask;` |
|       - | 10736 | `	} aByRef[] = {` |
|       - | 10737 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10738 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10739 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10740 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10741 | `	};` |
|       - | 10742 | `	sxu32 i;` |
|  447933 | 10743 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1631 | 10744 | `		return 0;` |
|       - | 10745 | `	}` |
| 2231243 | 10746 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1785030 | 10747 | `		if( pName->nByte == aByRef[i].nByte` |
|  915022 | 10748 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      99 | 10749 | `			return aByRef[i].mask;` |
|       - | 10750 | `		}` |
|  892473 | 10751 | `	}` |
|  446213 | 10752 | `	return 0;` |
|  223969 | 10753 | `}` |
|       - | 10754 | `/*` |
|       - | 10755 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 10756 | ` *` |
|       - | 10757 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 10758 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 10759 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 10760 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 10761 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 10762 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 10763 | ` */` |
|  447928 | 10764 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 10765 | `{` |
|       - | 10766 | `	SyToken *p, *pEnd;` |
|  447933 | 10767 | `	pOut->zString = 0;` |
|  447933 | 10768 | `	pOut->nByte = 0;` |
|  447933 | 10769 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 10770 | `		return;` |
|       - | 10771 | `	}` |
|  447933 | 10772 | `	p = pLeft->pStart;` |
|  447933 | 10773 | `	pEnd = pLeft->pEnd;` |
|       - | 10774 | `	/* Optional single leading namespace separator (absolute path). */` |
|  447933 | 10775 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|    3613 | 10776 | `		p++;` |
|    1804 | 10777 | `	}` |
|  447933 | 10778 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1603 | 10779 | `		return;` |
|       - | 10780 | `	}` |
|       - | 10781 | `	/* Must be a single component: nothing follows the name token. */` |
|  446335 | 10782 | `	if( p + 1 != pEnd ){` |
|      32 | 10783 | `		return;` |
|       - | 10784 | `	}` |
|  446307 | 10785 | `	*pOut = p->sData;` |
|  223969 | 10786 | `}` |
|       - | 10787 | `/*` |
|       - | 10788 | ` * Generate bytecode for a given expression tree.` |
|       - | 10789 | ` * If something goes wrong while generating bytecode` |
|       - | 10790 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 10791 | ` * this function takes care of generating the appropriate` |
|       - | 10792 | ` * error message.` |
|       - | 10793 | ` */` |
| 3567534 | 10794 | `static sxi32 GenStateEmitExprCode(` |
|       - | 10795 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10796 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 10797 | `	sxi32 iFlags /* Control flags */` |
|       - | 10798 | `	)` |
|       5 | 10799 | `{` |
|       - | 10800 | `	VmInstr *pInstr;` |
|       - | 10801 | `	sxu32 nJmpIdx;` |
| 3567539 | 10802 | `	sxi32 iP1 = 0;` |
| 3567539 | 10803 | `	sxu32 iP2 = 0;` |
| 3567539 | 10804 | `	void *p3  = 0;` |
|       - | 10805 | `	sxi32 iVmOp;` |
|       - | 10806 | `	sxi32 rc;` |
| 3567539 | 10807 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3567539 | 10808 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 3567539 | 10809 | `	sxu32 nRhsNsBase = 0;` |
| 3567539 | 10810 | `	if( pNode->xCode ){` |
|       - | 10811 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 10812 | `		/* Compile node */` |
| 2227059 | 10813 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2227059 | 10814 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2227059 | 10815 | `		RE_SWAP_DELIMITER(pGen);` |
| 2227059 | 10816 | `		return rc;` |
|       - | 10817 | `	}` |
| 1340485 | 10818 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 10819 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 10820 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 10821 | `		return SXERR_ABORT;` |
|       - | 10822 | `	}` |
| 1340485 | 10823 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1340485 | 10824 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      65 | 10825 | `		sxu32 nJmp = 0;` |
|       - | 10826 | `		sxu32 nNcNsBase;` |
|       - | 10827 | `		VmInstr *pInstrFix;` |
|       - | 10828 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 10829 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 10830 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 10831 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 10832 | `		 * stack slot carries a writable nIdx. */` |
|      65 | 10833 | `		if( pNode->pRight ){` |
|      65 | 10834 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 10835 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|      65 | 10836 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10837 | `				return rc;` |
|       - | 10838 | `			}` |
|      65 | 10839 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 10840 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 10841 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 10842 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 10843 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 10844 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 10845 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 10846 | `			 * cascade for the actual write path stays correct. */` |
|      65 | 10847 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 10848 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      31 | 10849 | `				pInstrFix->iP2 = 3;` |
|      14 | 10850 | `			}` |
|      31 | 10851 | `		}` |
|       - | 10852 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      65 | 10853 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 10854 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      65 | 10855 | `		if( pNode->pLeft ){` |
|      65 | 10856 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 10857 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      65 | 10858 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10859 | `				return rc;` |
|       - | 10860 | `			}` |
|      65 | 10861 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      31 | 10862 | `		}` |
|       - | 10863 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      65 | 10864 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 10865 | `		/* Patch the short-circuit jump to land after the store. */` |
|      65 | 10866 | `		if( nJmp > 0 ){` |
|      65 | 10867 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      65 | 10868 | `			if( pInstrFix ){` |
|      65 | 10869 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      31 | 10870 | `			}` |
|      31 | 10871 | `		}` |
|      65 | 10872 | `		return SXRET_OK;` |
|       - | 10873 | `	}` |
| 1340423 | 10874 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 10875 | `		sxu32 nJz,nJmp;` |
|       - | 10876 | `		sxu32 nTernaryNsBase;` |
|       - | 10877 | `		/* Ternary operator require special handling */` |
|       - | 10878 | `		/* Phase#1: Compile the condition */` |
|    2669 | 10879 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2669 | 10880 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2669 | 10881 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10882 | `			return rc;` |
|       - | 10883 | `		}` |
|       - | 10884 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 10885 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 10886 | `		 * condition expression, not leak past the ternary. */` |
|    2669 | 10887 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2669 | 10888 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2669 | 10889 | `		if( pNode->pLeft ){` |
|       - | 10890 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 10891 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2601 | 10892 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10893 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2601 | 10894 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2601 | 10895 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2601 | 10896 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10897 | `				return rc;` |
|       - | 10898 | `			}` |
|    2601 | 10899 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1303 | 10900 | `		}else{` |
|       - | 10901 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 10902 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 10903 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 10904 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 10905 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10906 | `		}` |
|       - | 10907 | `		/* Phase#4: Emit the unconditional jump */` |
|    2669 | 10908 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 10909 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2669 | 10910 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2669 | 10911 | `		if( pInstr ){` |
|    2669 | 10912 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1332 | 10913 | `		}` |
|    2669 | 10914 | `		if( !pNode->pLeft ){` |
|       - | 10915 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 10916 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 10917 | `		}` |
|       - | 10918 | `		/* Phase#6: Compile the 'else' expression */` |
|    2669 | 10919 | `		if( pNode->pRight ){` |
|    2669 | 10920 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2669 | 10921 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2669 | 10922 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10923 | `				return rc;` |
|       - | 10924 | `			}` |
|    2669 | 10925 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1332 | 10926 | `		}` |
|    2669 | 10927 | `		if( nJmp > 0 ){` |
|       - | 10928 | `			/* Phase#7: Fix the unconditional jump */` |
|    2669 | 10929 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2669 | 10930 | `			if( pInstr ){` |
|    2669 | 10931 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1332 | 10932 | `			}` |
|    1332 | 10933 | `		}` |
|       - | 10934 | `		/* All done */` |
|    2669 | 10935 | `		return SXRET_OK;` |
|       - | 10936 | `	}` |
| 1337759 | 10937 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 10938 | `	/* Generate code for the left tree */` |
| 1337759 | 10939 | `	if( pNode->pLeft ){` |
| 1337719 | 10940 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1337719 | 10941 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 10942 | `			ph7_expr_node **apNode;` |
|  451661 | 10943 | `			int hasSpread = 0;` |
|  451661 | 10944 | `			int hasNamed = 0;` |
|  451661 | 10945 | `			int bAnySpread = 0;` |
|  451661 | 10946 | `			sxu32 byRefMask = 0;` |
|       - | 10947 | `			sxi32 nArgs;` |
|       - | 10948 | `			sxi32 n;` |
|       - | 10949 | `			/* Recurse and generate bytecodes for function arguments */` |
|  451661 | 10950 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  451661 | 10951 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 10952 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|       - | 10953 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|       - | 10954 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  451661 | 10955 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|      65 | 10956 | `				bFcc = 1;` |
|      65 | 10957 | `				nArgs = 0;` |
|      32 | 10958 | `			}` |
|       - | 10959 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 10960 | `			{` |
|  451661 | 10961 | `				int seenNamed = 0;` |
|  916509 | 10962 | `				for( n = 0; n < nArgs; ++n ){` |
|  464855 | 10963 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     216 | 10964 | `						seenNamed = 1;` |
|     216 | 10965 | `						hasNamed = 1;` |
|  464749 | 10966 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|    3615 | 10967 | `						bAnySpread = 1;` |
|  462838 | 10968 | `					}else if( seenNamed ){` |
|       3 | 10969 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 10970 | `							"Cannot use positional argument after named argument");` |
|       3 | 10971 | `						return SXERR_SYNTAX;` |
|       - | 10972 | `					}` |
|  232429 | 10973 | `				}` |
|       - | 10974 | `			}` |
|       - | 10975 | `			/* Read-only load */` |
|  451659 | 10976 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 10977 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 10978 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 10979 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 10980 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  451659 | 10981 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  451659 | 10982 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  451654 | 10983 | `				if( pCallName->nByte == 5` |
|  246591 | 10984 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   21849 | 10985 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  440737 | 10986 | `				}else if( pCallName->nByte == 5` |
|  224747 | 10987 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      91 | 10988 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      43 | 10989 | `				}` |
|       - | 10990 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 10991 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 10992 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 10993 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 10994 | `				 * the compile-time positional index no longer maps to the` |
|       - | 10995 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  451659 | 10996 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 10997 | `					SyString sBuiltin;` |
|  447933 | 10998 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  447933 | 10999 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  223964 | 11000 | `				}` |
|  225827 | 11001 | `			}` |
|  916505 | 11002 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  464851 | 11003 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  464851 | 11004 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11005 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 11006 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|       - | 11007 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|       - | 11008 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|       - | 11009 | `				 * builtin to write back through. A plain $var target is unaffected` |
|       - | 11010 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  464851 | 11011 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      55 | 11012 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      55 | 11013 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      25 | 11014 | `				}` |
|  464851 | 11015 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  464851 | 11016 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11017 | `					return rc;` |
|       - | 11018 | `				}` |
|       - | 11019 | `				/* Each argument is an independent nullsafe scope. */` |
|  464851 | 11020 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  464851 | 11021 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 11022 | `					/* Emit spread opcode to unpack this array argument */` |
|    3615 | 11023 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|    3615 | 11024 | `					hasSpread = 1;` |
|    1805 | 11025 | `				}` |
|  232428 | 11026 | `			}` |
|       - | 11027 | `			/* Total number of given arguments */` |
|  451659 | 11028 | `			iP1 = nArgs;` |
|  451659 | 11029 | `			iP2 = hasSpread;` |
|       - | 11030 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 11031 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  451659 | 11032 | `			if( hasNamed ){` |
|     119 | 11033 | `				sxu32 nStrBytes = 0;` |
|       - | 11034 | `				char *zBuf;` |
|     347 | 11035 | `				for( n = 0; n < nArgs; ++n ){` |
|     231 | 11036 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     213 | 11037 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|     105 | 11038 | `					}` |
|     117 | 11039 | `				}` |
|       - | 11040 | `				{` |
|     119 | 11041 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     119 | 11042 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|     116 | 11043 | `					&pGen->pVm->sAllocator, mapSize);` |
|     119 | 11044 | `				if( pMap ){` |
|     119 | 11045 | `					SyZero(pMap, mapSize);` |
|     119 | 11046 | `					pMap->bHasNamed = 1;` |
|     119 | 11047 | `					pMap->nTotal = (sxu32)nArgs;` |
|     119 | 11048 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     119 | 11049 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     347 | 11050 | `					for( n = 0; n < nArgs; ++n ){` |
|     231 | 11051 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     213 | 11052 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     213 | 11053 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     213 | 11054 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     213 | 11055 | `							zBuf += nb;` |
|     105 | 11056 | `						}` |
|       - | 11057 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     117 | 11058 | `					}` |
|     119 | 11059 | `					p3 = (void *)pMap;` |
|      58 | 11060 | `				}` |
|       - | 11061 | `				}` |
|      58 | 11062 | `			}` |
|       - | 11063 | `			/* Remove stale flags now */` |
|  451659 | 11064 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  225827 | 11065 | `		}` |
|       - | 11066 | `		{` |
|       - | 11067 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|       - | 11068 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|       - | 11069 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|       - | 11070 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|       - | 11071 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|       - | 11072 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|       - | 11073 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|       - | 11074 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 1337717 | 11075 | `			sxi32 iLeftFlags = iFlags;` |
| 1337712 | 11076 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
| 1021735 | 11077 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  352904 | 11078 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  344978 | 11079 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   16045 | 11080 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|    8020 | 11081 | `			}` |
|       - | 11082 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|       - | 11083 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|       - | 11084 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|       - | 11085 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|       - | 11086 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|       - | 11087 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|       - | 11088 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 1337712 | 11089 | `			if( pNode->pOp` |
| 1917354 | 11090 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 1248544 | 11091 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 1159325 | 11092 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  178771 | 11093 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   89383 | 11094 | `			}` |
| 1337717 | 11095 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|       - | 11096 | `		}` |
| 1337717 | 11097 | `		if( rc != SXRET_OK ){` |
|      34 | 11098 | `			return rc;` |
|       - | 11099 | `		}` |
| 1337687 | 11100 | `		if( !bIsChainOp ){` |
|       - | 11101 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 11102 | `			 * target the end of that LHS chain, which is right here. */` |
|  615009 | 11103 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  307502 | 11104 | `		}` |
| 1337687 | 11105 | `		if( iVmOp == PH7_OP_CALL ){` |
|  451659 | 11106 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  451659 | 11107 | `			if( pInstr ){` |
|  451659 | 11108 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  446429 | 11109 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 11110 | `					sxu32 nQual;` |
|  446429 | 11111 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11112 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 11113 | `					 * so the later NEW handler (if any) can see it. */` |
|  446429 | 11114 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 11115 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 11116 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 11117 | `					 * imports — class imports must NOT affect function` |
|       - | 11118 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 11119 | `					 * before NEW; we store the original literal index in the` |
|       - | 11120 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 11121 | `					 * the unqualified name and re-qualify with class imports. */` |
|  446429 | 11122 | `					if( bAbsolute ){` |
|    3613 | 11123 | `						pInstr->iP2 = (sxi32)nOrig;` |
|    1809 | 11124 | `					}else{` |
|  442821 | 11125 | `						int fromImport = 0;` |
|  442821 | 11126 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  442821 | 11127 | `						pInstr->iP2 = (sxi32)nQual;` |
|  442821 | 11128 | `						if( nQual != nOrig ){` |
|       - | 11129 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 11130 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 11131 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 11132 | `							if( !fromImport ){` |
|       - | 11133 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 11134 | `								if( p3 == 0 ){` |
|      67 | 11135 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 11136 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 11137 | `									if( pMap ){` |
|      67 | 11138 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 11139 | `										p3 = (void *)pMap;` |
|      31 | 11140 | `									}` |
|      31 | 11141 | `								}` |
|      67 | 11142 | `								if( p3 ){` |
|      67 | 11143 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 11144 | `								}` |
|      31 | 11145 | `							}` |
|      36 | 11146 | `						}` |
|       5 | 11147 | `					}` |
|  228447 | 11148 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 11149 | `					/* Method call,flag that */` |
|    1213 | 11150 | `					pInstr->iP2 = 1;` |
|     604 | 11151 | `				}` |
|  225832 | 11152 | `			}` |
| 1111860 | 11153 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 11154 | `			ph7_expr_node **apNode;` |
|       - | 11155 | `			sxi32 n;` |
|   92263 | 11156 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 11157 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 11158 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11159 | `			/* Recurse and generate bytecodes for array index */` |
|   92263 | 11160 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  166491 | 11161 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   74233 | 11162 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   74233 | 11163 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   74233 | 11164 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11165 | `					return rc;` |
|       - | 11166 | `				}` |
|       - | 11167 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   74233 | 11168 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   37119 | 11169 | `			}` |
|   92263 | 11170 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   74233 | 11171 | `				iP1 = 1; /* Node have an index associated with it */` |
|   37114 | 11172 | `			}` |
|   92263 | 11173 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 11174 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     243 | 11175 | `				iP2 = 4;` |
|   92144 | 11176 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 11177 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 11178 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      56 | 11179 | `				iP2 = 5;` |
|   91999 | 11180 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 11181 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 11182 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 11183 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 11184 | `				iP2 = 6;` |
|   91961 | 11185 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 11186 | `				/* Create an empty entry when the desired index is not found */` |
|   36367 | 11187 | `				iP2 = 1;` |
|   18186 | 11188 | `			}` |
|  839904 | 11189 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 11190 | `			/* POP the left node */` |
|      32 | 11191 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 11192 | `		}` |
|  668841 | 11193 | `	}` |
| 1337727 | 11194 | `	rc = SXRET_OK;` |
| 1337727 | 11195 | `	nJmpIdx = 0;` |
|       - | 11196 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 11197 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 11198 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1337727 | 11199 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     371 | 11200 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     371 | 11201 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     371 | 11202 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     371 | 11203 | `			int isSpecial = 0;` |
|     371 | 11204 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     275 | 11205 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     275 | 11206 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     270 | 11207 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     270 | 11208 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     129 | 11209 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      99 | 11210 | `					isSpecial = 1;` |
|      47 | 11211 | `				}` |
|     159 | 11212 | `			}` |
|     419 | 11213 | `			pInstr->iP1 = 0;` |
|     419 | 11214 | `			if( !isSpecial ){` |
|     229 | 11215 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     112 | 11216 | `			}` |
|       - | 11217 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 11218 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     323 | 11219 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     229 | 11220 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     229 | 11221 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      46 | 11222 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      48 | 11223 | `					return SXRET_OK;` |
|       - | 11224 | `				}` |
|      90 | 11225 | `			}` |
|     137 | 11226 | `		}` |
|     218 | 11227 | `	}` |
|       - | 11228 | `	/* Generate code for the right tree */` |
| 1337645 | 11229 | `	if( pNode->pRight ){` |
|  721941 | 11230 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 11231 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   11265 | 11232 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  716311 | 11233 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 11234 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3769 | 11235 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  708799 | 11236 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 11237 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 11238 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 11239 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  706906 | 11240 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 11241 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 11242 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 11243 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 11244 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 11245 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 11246 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     106 | 11247 | `			sxu32 nNsJmp = 0;` |
|     106 | 11248 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     106 | 11249 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  706742 | 11250 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|       - | 11251 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|       - | 11252 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|       - | 11253 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  300343 | 11254 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  150169 | 11255 | `		}` |
|  721941 | 11256 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  721941 | 11257 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  721941 | 11258 | `		if( !bIsChainOp ){` |
|       - | 11259 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 11260 | `			 * operator instruction is emitted. */` |
|  543219 | 11261 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  271607 | 11262 | `		}` |
|  721941 | 11263 | `		if( iVmOp == PH7_OP_STORE ){` |
|  296499 | 11264 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  296468 | 11265 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 11266 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 11267 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 11268 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 11269 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 11270 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 11271 | `				 */` |
|      80 | 11272 | `				iVmOp = 0;` |
|  296461 | 11273 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  296423 | 11274 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11275 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   79399 | 11276 | `					iP2 = 1;` |
|   39702 | 11277 | `				}else{` |
|  217029 | 11278 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11279 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   36291 | 11280 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   36291 | 11281 | `						iP1 = pInstr->iP1;` |
|   18148 | 11282 | `					}else{` |
|  180743 | 11283 | `						p3 = pInstr->p3;` |
|       - | 11284 | `					}` |
|       - | 11285 | `					/* POP the last dynamic load instruction */` |
|  217029 | 11286 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 11287 | `				}` |
|  148214 | 11288 | `			}` |
|  573694 | 11289 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      54 | 11290 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      54 | 11291 | `			if( pInstr ){` |
|      54 | 11292 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11293 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 11294 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 11295 | `					 */` |
|      17 | 11296 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      17 | 11297 | `					iP1 = pInstr->iP1;` |
|      17 | 11298 | `					iP2 = pInstr->iP2;` |
|      17 | 11299 | `					p3  = pInstr->p3;` |
|       9 | 11300 | `				}else{` |
|      38 | 11301 | `					p3 = pInstr->p3;` |
|       - | 11302 | `				}` |
|      26 | 11303 | `			}` |
|      26 | 11304 | `		}` |
|  360968 | 11305 | `	}` |
| 1337640 | 11306 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   11671 | 11307 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|       - | 11308 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|       - | 11309 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|      30 | 11310 | `		iVmOp = 0;` |
|      13 | 11311 | `	}` |
| 1337645 | 11312 | `	if( iVmOp > 0 ){` |
| 1337389 | 11313 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   14741 | 11314 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 11315 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|   10783 | 11316 | `				iP1 = 1;` |
|    5394 | 11317 | `			}` |
| 1330021 | 11318 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 11319 | `			/* Namespace-qualify the class name for NEW */ {` |
|   23093 | 11320 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   23093 | 11321 | `				VmInstr *pCallInstr = 0;` |
|   23093 | 11322 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   22901 | 11323 | `					pCallInstr = pPeek;` |
|   22901 | 11324 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   11448 | 11325 | `				}` |
|   23093 | 11326 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   23091 | 11327 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11328 | `					sxu32 nLitForClass;` |
|       - | 11329 | `					/* If the CALL handler already qualified the name using` |
|       - | 11330 | `					 * function imports, recover the original unqualified` |
|       - | 11331 | `					 * literal so we can re-qualify with class imports. */` |
|   23091 | 11332 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 11333 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 11334 | `					}else{` |
|   23059 | 11335 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 11336 | `					}` |
|   23091 | 11337 | `					pPeek->iP1 = 0;` |
|   23091 | 11338 | `					if( !bAbsolute ){` |
|   19487 | 11339 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    9746 | 11340 | `					}else{` |
|    3609 | 11341 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 11342 | `					}` |
|   11543 | 11343 | `				}` |
|       - | 11344 | `			}` |
|   23093 | 11345 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   23093 | 11346 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 11347 | `				VmInstr *pPrev;` |
|   22901 | 11348 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   22901 | 11349 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 11350 | `					/* Pop the call instruction, preserve named-arg map */` |
|   22901 | 11351 | `					iP1 = pInstr->iP1;` |
|   22901 | 11352 | `					if( pInstr->p3 ){` |
|      43 | 11353 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 11354 | `					}` |
|   22901 | 11355 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   11448 | 11356 | `				}` |
|   11453 | 11357 | `			}` |
| 1311109 | 11358 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 11359 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 11360 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     201 | 11361 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     201 | 11362 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     201 | 11363 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     201 | 11364 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     201 | 11365 | `				int isSpecialIs = 0;` |
|     201 | 11366 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     197 | 11367 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     197 | 11368 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     192 | 11369 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     197 | 11370 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      97 | 11371 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 11372 | `						isSpecialIs = 1;` |
|       5 | 11373 | `					}` |
|      97 | 11374 | `				}` |
|     203 | 11375 | `				pInstr->iP1 = 0;` |
|     203 | 11376 | `				if( !isSpecialIs && !bAbsolute ){` |
|     181 | 11377 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      88 | 11378 | `				}` |
|     102 | 11379 | `			}` |
| 1299470 | 11380 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 11381 | `			/* Prevent constant expansion for member/property names.` |
|       - | 11382 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 11383 | `			 * should not trigger constant lookup. */` |
|  178727 | 11384 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  178727 | 11385 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  178681 | 11386 | `				pInstr->iP1 = 0;` |
|   89338 | 11387 | `			}` |
|  178727 | 11388 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 11389 | `				/* Static member access,remember that */` |
|     289 | 11390 | `				iP1 = 1;` |
|     289 | 11391 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     289 | 11392 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      40 | 11393 | `					p3 = pInstr->p3;` |
|      40 | 11394 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      18 | 11395 | `				}` |
|     142 | 11396 | `			}` |
|       - | 11397 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|       - | 11398 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|       - | 11399 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|       - | 11400 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  178727 | 11401 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  178727 | 11402 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|      30 | 11403 | `					iP2 = PH7_MEMBER_UNSET;` |
|  178713 | 11404 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|      77 | 11405 | `					iP2 = PH7_MEMBER_ISSET;` |
|  178663 | 11406 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|      13 | 11407 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  178621 | 11408 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|       - | 11409 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   79479 | 11410 | `					iP2 = PH7_MEMBER_WRITE;` |
|   39737 | 11411 | `				}` |
|   89361 | 11412 | `			}` |
|   89361 | 11413 | `		}` |
|       - | 11414 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|       - | 11415 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|       - | 11416 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|       - | 11417 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|       - | 11418 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 1337387 | 11419 | `		if( bFcc ){` |
|      65 | 11420 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|      65 | 11421 | `			iP2 = 0;` |
|      65 | 11422 | `			p3 = 0;` |
|      65 | 11423 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 11424 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11425 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|       - | 11426 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|       - | 11427 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|       - | 11428 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|      31 | 11429 | `				void *pMemberName = pInstr->p3;` |
|      31 | 11430 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|      31 | 11431 | `				if( pMemberName ){` |
|       3 | 11432 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       1 | 11433 | `				}` |
|      31 | 11434 | `				iP1 = 2;` |
|      16 | 11435 | `			}else{` |
|      35 | 11436 | `				iP1 = 1;` |
|       - | 11437 | `			}` |
|      32 | 11438 | `		}` |
|       - | 11439 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 11440 | `		 * This is the primary emit path for user-visible calls. */` |
| 1337387 | 11441 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  474683 | 11442 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  237339 | 11443 | `		}` |
|       - | 11444 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1337387 | 11445 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  668691 | 11446 | `	}` |
| 1337643 | 11447 | `	if( nJmpIdx > 0 ){` |
|       - | 11448 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   15153 | 11449 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   15153 | 11450 | `		if( pInstr ){` |
|   15153 | 11451 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    7574 | 11452 | `		}` |
|    7574 | 11453 | `	}` |
| 1337643 | 11454 | `	return rc;` |
| 1783752 | 11455 | `}` |
|       - | 11456 | `/*` |
|       - | 11457 | ` * Compile a PHP expression.` |
|       - | 11458 | ` * According to the PHP language reference manual:` |
|       - | 11459 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 11460 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 11461 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 11462 | ` *  is "anything that has a value".` |
|       - | 11463 | ` * If something goes wrong while compiling the expression,this` |
|       - | 11464 | ` * function takes care of generating the appropriate error` |
|       - | 11465 | ` * message.` |
|       - | 11466 | ` */` |
|  960916 | 11467 | `static sxi32 PH7_CompileExpr(` |
|       - | 11468 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11469 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 11470 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 11471 | `	)` |
|       5 | 11472 | `{` |
|       - | 11473 | `	ph7_expr_node *pRoot;` |
|       - | 11474 | `	SySet sExprNode;` |
|       - | 11475 | `	SyToken *pEnd;` |
|       - | 11476 | `	sxi32 nExpr;` |
|       - | 11477 | `	sxi32 iNest;` |
|       - | 11478 | `	sxi32 rc;` |
|       - | 11479 | `	sxu32 nNullsafeBase;` |
|       - | 11480 | `	/* Initialize worker variables */` |
|  960921 | 11481 | `	nExpr = 0;` |
|  960921 | 11482 | `	pRoot = 0;` |
|       - | 11483 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 11484 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  960921 | 11485 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  960921 | 11486 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  960921 | 11487 | `	SySetAlloc(&sExprNode,0x10);` |
|  960921 | 11488 | `	rc = SXRET_OK;` |
|       - | 11489 | `	/* Delimit the expression */` |
|  960921 | 11490 | `	pEnd = pGen->pIn;` |
|  960921 | 11491 | `	iNest = 0;` |
| 6482985 | 11492 | `	while( pEnd < pGen->pEnd ){` |
| 6151875 | 11493 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11494 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     517 | 11495 | `			iNest++;` |
| 6151619 | 11496 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     525 | 11497 | `			iNest--;` |
| 6151103 | 11498 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  630183 | 11499 | `			if( iNest <= 0 ){` |
|  629811 | 11500 | `				break;` |
|       - | 11501 | `			}` |
|     186 | 11502 | `		}` |
| 5522069 | 11503 | `		pEnd++;` |
|       5 | 11504 | `	}` |
|  960921 | 11505 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   22097 | 11506 | `		SyToken *pEnd2 = pGen->pIn;` |
|   22097 | 11507 | `		iNest = 0;` |
|       - | 11508 | `		/* Stop at the first comma */` |
|   44493 | 11509 | `		while( pEnd2 < pEnd ){` |
|   22407 | 11510 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      67 | 11511 | `				iNest++;` |
|   22376 | 11512 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      67 | 11513 | `				iNest--;` |
|   22314 | 11514 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      57 | 11515 | `				if( iNest <= 0 ){` |
|       7 | 11516 | `					break;` |
|       - | 11517 | `				}` |
|      23 | 11518 | `			}` |
|   22401 | 11519 | `			pEnd2++;` |
|       5 | 11520 | `		}` |
|   22097 | 11521 | `		if( pEnd2 <pEnd ){` |
|       7 | 11522 | `			pEnd = pEnd2;` |
|       3 | 11523 | `		}` |
|   11046 | 11524 | `	}` |
|  960921 | 11525 | `	if( pEnd > pGen->pIn ){` |
|  960911 | 11526 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 11527 | `		/* Swap delimiter */` |
|  960911 | 11528 | `		pGen->pEnd = pEnd;` |
|       - | 11529 | `		/* Try to get an expression tree */` |
|  960911 | 11530 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  960911 | 11531 | `		if( rc == SXRET_OK && pRoot ){` |
|  960729 | 11532 | `			rc = SXRET_OK;` |
|  960729 | 11533 | `			if( xTreeValidator ){` |
|       - | 11534 | `				/* Call the upper layer validator callback */` |
|   29513 | 11535 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   14754 | 11536 | `			}` |
|  960729 | 11537 | `			if( rc != SXERR_ABORT ){` |
|       - | 11538 | `				/* Generate code for the given tree */` |
|  960729 | 11539 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 11540 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 11541 | `				 * expression so they short-circuit to its end. */` |
|  960729 | 11542 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  480362 | 11543 | `			}` |
|  960729 | 11544 | `			nExpr = 1;` |
|  480362 | 11545 | `		}` |
|       - | 11546 | `		/* Release the whole tree */` |
|  960911 | 11547 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 11548 | `		/* Synchronize token stream */` |
|  960911 | 11549 | `		pGen->pEnd = pTmp;` |
|  960911 | 11550 | `		pGen->pIn  = pEnd;` |
|  960911 | 11551 | `		if( rc == SXERR_ABORT ){` |
|      13 | 11552 | `			SySetRelease(&sExprNode);` |
|      13 | 11553 | `			return SXERR_ABORT;` |
|       - | 11554 | `		}` |
|  480448 | 11555 | `	}` |
|  960911 | 11556 | `	SySetRelease(&sExprNode);` |
|  960911 | 11557 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  480463 | 11558 | `}` |
|       - | 11559 | `/*` |
|       - | 11560 | ` * Return a pointer to the node construct handler associated` |
|       - | 11561 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 11562 | ` */` |
|  251876 | 11563 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 11564 | `{` |
|  251881 | 11565 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 11566 | `		/* Numeric literal: Either real or integer */` |
|  126747 | 11567 | `		return PH7_CompileNumLiteral;` |
|  125139 | 11568 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 11569 | `		/* Double quoted string */` |
|   23859 | 11570 | `		return PH7_CompileString;` |
|  101285 | 11571 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 11572 | `		/* Single quoted string */` |
|  101169 | 11573 | `		return PH7_CompileSimpleString;` |
|     121 | 11574 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 11575 | `		/* Heredoc */` |
|      69 | 11576 | `		return PH7_CompileHereDoc;` |
|      56 | 11577 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 11578 | `		/* Nowdoc */` |
|      49 | 11579 | `		return PH7_CompileNowDoc;` |
|       8 | 11580 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 11581 | `		/* Backtick quoted string */` |
|       6 | 11582 | `		return PH7_CompileBacktic;` |
|       - | 11583 | `	}` |
|       3 | 11584 | `	return 0;` |
|  125943 | 11585 | `}` |
|       - | 11586 | `/*` |
|       - | 11587 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 11588 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 11589 | ` * in write context" parse error.` |
|       - | 11590 | ` */` |
|    6866 | 11591 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 11592 | `{` |
|       - | 11593 | `	sxi32 rc;` |
|    6871 | 11594 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6869 | 11595 | `		return SXRET_OK;` |
|       - | 11596 | `	}` |
|       5 | 11597 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 11598 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 11599 | `		"Can't use nullsafe operator in write context");` |
|       3 | 11600 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3438 | 11601 | `}` |
|       - | 11602 | `/*` |
|       - | 11603 | ` * Compile an unset() statement.` |
|       - | 11604 | ` * unset($var, $arr[$key], ...);` |
|       - | 11605 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 11606 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 11607 | ` * parent array before extracting the element to unset.` |
|       - | 11608 | ` */` |
|    2978 | 11609 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 11610 | `{` |
|    2983 | 11611 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2983 | 11612 | `	sxu32 nIdx = 0;` |
|       - | 11613 | `	SyString sName;` |
|       - | 11614 | `	sxi32 rc;` |
|       - | 11615 | `	/* Jump the 'unset' keyword */` |
|    2983 | 11616 | `	pGen->pIn++;` |
|       - | 11617 | `	/* Save delimiter */` |
|    2983 | 11618 | `	pTmp = pGen->pEnd;` |
|       - | 11619 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2983 | 11620 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2983 | 11621 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 11622 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 11623 | `		SyToken *pClose;` |
|    2983 | 11624 | `		pGen->pIn++;   /* Skip '(' */` |
|    2983 | 11625 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2983 | 11626 | `		pEnd = pClose; /* Stop at ')' */` |
|    1489 | 11627 | `	}` |
|    2983 | 11628 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 11629 | `	/* Resolve the 'unset' builtin name once */` |
|    2983 | 11630 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     365 | 11631 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     365 | 11632 | `		if( pObj == 0 ){` |
|     ! 0 | 11633 | `			return SXERR_ABORT;` |
|       - | 11634 | `		}` |
|     365 | 11635 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     365 | 11636 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     180 | 11637 | `	}` |
|       - | 11638 | `	/* Compile each comma-separated argument */` |
|    9851 | 11639 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6873 | 11640 | `		if( pGen->pIn < pNext ){` |
|    6873 | 11641 | `			pGen->pEnd = pNext;` |
|    6873 | 11642 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 11643 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 11644 | `				GenStateUnsetValidator);` |
|    6873 | 11645 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11646 | `				return SXERR_ABORT;` |
|       - | 11647 | `			}` |
|    6873 | 11648 | `			if( rc != SXERR_EMPTY ){` |
|       - | 11649 | `				/* Emit call for this single argument */` |
|    6871 | 11650 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6871 | 11651 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6871 | 11652 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3433 | 11653 | `			}` |
|    3434 | 11654 | `		}` |
|       - | 11655 | `		/* Jump trailing commas */` |
|   10765 | 11656 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3897 | 11657 | `			pNext++;` |
|       5 | 11658 | `		}` |
|    6873 | 11659 | `		pGen->pIn = pNext;` |
|       5 | 11660 | `	}` |
|       - | 11661 | `	/* Skip past the closing ')' if present */` |
|    2983 | 11662 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2983 | 11663 | `		pGen->pIn++;` |
|    1489 | 11664 | `	}` |
|       - | 11665 | `	/* Restore token stream */` |
|    2983 | 11666 | `	pGen->pEnd = pTmp;` |
|    2983 | 11667 | `	return SXRET_OK;` |
|    1494 | 11668 | `}` |
|       - | 11669 | `/*` |
|       - | 11670 | ` * PHP Language construct table.` |
|       - | 11671 | ` */` |
|       - | 11672 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 11673 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 11674 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 11675 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 11676 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 11677 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 11678 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 11679 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 11680 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 11681 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 11682 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 11683 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 11684 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 11685 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 11686 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 11687 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 11688 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 11689 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 11690 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 11691 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 11692 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 11693 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 11694 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 11695 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 11696 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 11697 | `};` |
|       - | 11698 | `/*` |
|       - | 11699 | ` * Return a pointer to the statement handler routine associated` |
|       - | 11700 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 11701 | ` */` |
|  644298 | 11702 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 11703 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 11704 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 11705 | `	)` |
|       5 | 11706 | `{` |
|  644303 | 11707 | `	sxu32 n = 0;` |
| 3340374 | 11708 | `	for(;;){` |
| 6680753 | 11709 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  137895 | 11710 | `			break;` |
|       - | 11711 | `		}` |
| 6542863 | 11712 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  506413 | 11713 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 11714 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 11715 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 11716 | `					/* 'static' (class context),return null */` |
|     ! 0 | 11717 | `					return 0;` |
|       - | 11718 | `				}` |
|     ! 0 | 11719 | `			}` |
|  506408 | 11720 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       6 | 11721 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       8 | 11722 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 11723 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 11724 | `				return 0;` |
|       - | 11725 | `			}` |
|       - | 11726 | `			/* Return a pointer to the handler.` |
|       - | 11727 | `			*/` |
|  506413 | 11728 | `			return aLangConstruct[n].xConstruct;` |
|       - | 11729 | `		}` |
| 6036455 | 11730 | `		n++;` |
|       5 | 11731 | `	}` |
|  137895 | 11732 | `	if( pLookahed ){` |
|  137895 | 11733 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   39533 | 11734 | `			return PH7_CompileClassInterface;` |
|   98367 | 11735 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   98019 | 11736 | `			return PH7_CompileClass;` |
|     353 | 11737 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      69 | 11738 | `			return PH7_CompileTrait;` |
|       - | 11739 | `		}` |
|       - | 11740 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 11741 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 11742 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 11743 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     142 | 11744 | `	}` |
|       - | 11745 | `	/* Not a language construct */` |
|     289 | 11746 | `	return 0;` |
|  322154 | 11747 | `}` |
|       - | 11748 | `/*` |
|       - | 11749 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 11750 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 11751 | ` */` |
|     284 | 11752 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 11753 | `{` |
|       - | 11754 | `	int rc;` |
|     289 | 11755 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     289 | 11756 | `	if( rc == FALSE ){` |
|     174 | 11757 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     173 | 11758 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 11759 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 11760 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 11761 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 11762 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 11763 | `			*/` |
|       - | 11764 | `			){` |
|     171 | 11765 | `				rc = TRUE;` |
|      83 | 11766 | `		}` |
|      87 | 11767 | `	}` |
|     289 | 11768 | `	return rc;` |
|       5 | 11769 | `}` |
|       - | 11770 | `/*` |
|       - | 11771 | ` * Compile a PHP chunk.` |
|       - | 11772 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11773 | ` * takes care of generating the appropriate error message.` |
|       - | 11774 | ` */` |
|  770640 | 11775 | `static sxi32 GenStateCompileChunk(` |
|       - | 11776 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11777 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 11778 | `	)` |
|       5 | 11779 | `{` |
|       - | 11780 | `	ProcLangConstruct xCons;` |
|       - | 11781 | `	sxi32 rc;` |
|  770645 | 11782 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  602738 | 11783 | `	for(;;){` |
|  988063 | 11784 | `		int bStmtIsDeclare = 0;` |
|  988063 | 11785 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11786 | `			/* No more input to process */` |
|   14337 | 11787 | `			break;` |
|       - | 11788 | `		}` |
|       - | 11789 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 11790 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  973731 | 11791 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  647923 | 11792 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  647923 | 11793 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      45 | 11794 | `				bStmtIsDeclare = 1;` |
|      20 | 11795 | `			}` |
|  323959 | 11796 | `		}` |
|  973731 | 11797 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 11798 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 11799 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  217393 | 11800 | `			pGen->bStrictTypesLocked = 1;` |
|  108694 | 11801 | `		}` |
|  973731 | 11802 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11803 | `			/* Compile block */` |
|      23 | 11804 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      23 | 11805 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11806 | `				break;` |
|       - | 11807 | `			}` |
|      14 | 11808 | `		}else{` |
|  973713 | 11809 | `			xCons = 0;` |
|  973713 | 11810 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 11811 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 11812 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 11813 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    3651 | 11814 | `				xCons = PH7_CompileClassModifiers;` |
|  971890 | 11815 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  644303 | 11816 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 11817 | `				/* Try to extract a language construct handler */` |
|  644303 | 11818 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  644303 | 11819 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 11820 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 11821 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 11822 | `						&pGen->pIn->sData);` |
|       9 | 11823 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11824 | `						break;` |
|       - | 11825 | `					}` |
|       - | 11826 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 11827 | `					 * this erroneous statement.` |
|       - | 11828 | `					 */` |
|       9 | 11829 | `					xCons = PH7_ErrorRecover;` |
|       4 | 11830 | `				}` |
|  647918 | 11831 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   53413 | 11832 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 11833 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 11834 | `				xCons = PH7_CompileLabel;` |
|      56 | 11835 | `			}` |
|  973713 | 11836 | `			if( xCons == 0 ){` |
|       - | 11837 | `				/* Assume an expression an try to compile it */` |
|  325933 | 11838 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  325933 | 11839 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 11840 | `					/* Pop l-value */` |
|  325783 | 11841 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  162889 | 11842 | `				}` |
|  162969 | 11843 | `			}else{` |
|       - | 11844 | `				/* Go compile the sucker */` |
|  647785 | 11845 | `				rc = xCons(&(*pGen));` |
|       - | 11846 | `			}` |
|  973713 | 11847 | `			if( rc == SXERR_ABORT ){` |
|       - | 11848 | `				/* Request to abort compilation */` |
|      13 | 11849 | `				break;` |
|       - | 11850 | `			}` |
|       - | 11851 | `		}` |
|       - | 11852 | `		/* Ignore trailing semi-colons ';' */` |
| 1574421 | 11853 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  600705 | 11854 | `			pGen->pIn++;` |
|       5 | 11855 | `		}` |
|  973721 | 11856 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 11857 | `			/* Compile a single statement and return */` |
|  756303 | 11858 | `			break;` |
|       - | 11859 | `		}` |
|       - | 11860 | `		/* LOOP ONE */` |
|       - | 11861 | `		/* LOOP TWO */` |
|       - | 11862 | `		/* LOOP THREE */` |
|       - | 11863 | `		/* LOOP FOUR */` |
|       5 | 11864 | `	}` |
|       - | 11865 | `	/* Return compilation status */` |
|  770645 | 11866 | `	return rc;` |
|       5 | 11867 | `}` |
|       - | 11868 | `/*` |
|       - | 11869 | ` * Compile a Raw PHP chunk.` |
|       - | 11870 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11871 | ` * takes care of generating the appropriate error message.` |
|       - | 11872 | ` */` |
|   14344 | 11873 | `static sxi32 PH7_CompilePHP(` |
|       - | 11874 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11875 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 11876 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 11877 | `	)` |
|       5 | 11878 | `{` |
|   14349 | 11879 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 11880 | `	sxi32 rc;` |
|       - | 11881 | `	/* Reset the token set */` |
|   14349 | 11882 | `	SySetReset(&(*pTokenSet));` |
|       - | 11883 | `	/* Mark as the default token set */` |
|   14349 | 11884 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 11885 | `	/* Advance the stream cursor */` |
|   14349 | 11886 | `	pGen->pRawIn++;` |
|       - | 11887 | `	/* Tokenize the PHP chunk first */` |
|   14349 | 11888 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 11889 | `	/* Point to the head and tail of the token stream. */` |
|   14349 | 11890 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   14349 | 11891 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   14349 | 11892 | `	if( is_expr ){` |
|     ! 0 | 11893 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 11894 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 11895 | `			/* A simple expression,compile it */` |
|     ! 0 | 11896 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 11897 | `		}` |
|       - | 11898 | `		/* Emit the DONE instruction */` |
|     ! 0 | 11899 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 11900 | `		return SXRET_OK;` |
|       - | 11901 | `	}` |
|   14349 | 11902 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 11903 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 11904 | `		/*` |
|       - | 11905 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 11906 | `		 * According to the PHP reference manual:` |
|       - | 11907 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 11908 | `		 *  immediately follow` |
|       - | 11909 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 11910 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 11911 | `		 * Symisc extension:` |
|       - | 11912 | `		 *   This short syntax works with all PHP opening` |
|       - | 11913 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 11914 | `		 *   only short tag.` |
|       - | 11915 | `		 */` |
|       - | 11916 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 11917 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 11918 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 11919 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 11920 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 11921 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 11922 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 11923 | `		}` |
|       3 | 11924 | `		return SXRET_OK;` |
|       - | 11925 | `	}` |
|       - | 11926 | `	/* Compile the PHP chunk */` |
|   14347 | 11927 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 11928 | `	/* Fix exceptions jumps */` |
|   14347 | 11929 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11930 | `	/* Fix gotos now, the jump destination is resolved */` |
|   14347 | 11931 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 11932 | `		rc = SXERR_ABORT;` |
|       1 | 11933 | `	}` |
|       - | 11934 | `	/* Reset container */` |
|   14347 | 11935 | `	SySetReset(&pGen->aGoto);` |
|   14347 | 11936 | `	SySetReset(&pGen->aLabel);` |
|   14347 | 11937 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 11938 | `	/* Compilation result */` |
|   14347 | 11939 | `	return rc;` |
|    7177 | 11940 | `}` |
|       - | 11941 | `/*` |
|       - | 11942 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 11943 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 11944 | ` * This is the only compile interface exported from this file.` |
|       - | 11945 | ` */` |
|   17332 | 11946 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 11947 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 11948 | `	SyString *pScript,  /* Script to compile */` |
|       - | 11949 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 11950 | `	)` |
|       5 | 11951 | `{` |
|       - | 11952 | `	SySet aPhpToken,aRawToken;` |
|       - | 11953 | `	ph7_gen_state *pCodeGen;` |
|       - | 11954 | `	ph7_value *pRawObj;` |
|       - | 11955 | `	sxu32 nObjIdx;` |
|       - | 11956 | `	sxi32 nRawObj;` |
|       - | 11957 | `	int is_expr;` |
|       - | 11958 | `	sxi8 bSavedStrict;` |
|       - | 11959 | `	sxi8 bSavedStrictLocked;` |
|       - | 11960 | `	sxi32 rc;` |
|   17337 | 11961 | `	if( pScript->nByte < 1 ){` |
|       - | 11962 | `		/* Nothing to compile */` |
|     ! 0 | 11963 | `		return PH7_OK;` |
|       - | 11964 | `	}` |
|       - | 11965 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 11966 | `	 * file's flags so include/require restore them on return. */` |
|   17337 | 11967 | `	pCodeGen = &pVm->sCodeGen;` |
|   17337 | 11968 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   17337 | 11969 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   17337 | 11970 | `	pCodeGen->bStrictTypes = 0;` |
|   17337 | 11971 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 11972 | `	/* Initialize the tokens containers */` |
|   17337 | 11973 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17337 | 11974 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17337 | 11975 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   17337 | 11976 | `	is_expr = 0;` |
|   17337 | 11977 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 11978 | `		SyToken sTmp;` |
|       - | 11979 | `		/* PHP only: -*/` |
|    3659 | 11980 | `		sTmp.nLine = 1;` |
|    3659 | 11981 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3659 | 11982 | `		sTmp.pUserData = 0;` |
|    3659 | 11983 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3659 | 11984 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3659 | 11985 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 11986 | `			/* A simple PHP expression */` |
|     ! 0 | 11987 | `			is_expr = 1;` |
|     ! 0 | 11988 | `		}` |
|    1832 | 11989 | `	}else{` |
|       - | 11990 | `		/* Tokenize raw text */` |
|   13683 | 11991 | `		SySetAlloc(&aRawToken,32);` |
|   13683 | 11992 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 11993 | `	}` |
|       - | 11994 | `	/* Process high-level tokens */` |
|   17337 | 11995 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   17337 | 11996 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   17337 | 11997 | `	rc = PH7_OK;` |
|   17337 | 11998 | `	if( is_expr ){` |
|       - | 11999 | `		/* Compile the expression */` |
|     ! 0 | 12000 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 12001 | `		goto cleanup;` |
|       - | 12002 | `	}` |
|   17337 | 12003 | `	nObjIdx = 0;` |
|       - | 12004 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 12005 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 12006 | `	 * preventing namespace bleeding across include()d files. */` |
|   17337 | 12007 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 12008 | `	/* Start the compilation process */` |
|   15511 | 12009 | `	for(;;){` |
|   45359 | 12010 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   17325 | 12011 | `			break; /* No more tokens to process */` |
|       - | 12012 | `		}` |
|   28039 | 12013 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 12014 | `			/* Compile the PHP chunk */` |
|   14349 | 12015 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   14349 | 12016 | `			if( rc == SXERR_ABORT ){` |
|      15 | 12017 | `				break;` |
|       - | 12018 | `			}` |
|   14337 | 12019 | `			continue;` |
|       - | 12020 | `		}` |
|       - | 12021 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13695 | 12022 | `		nRawObj = 0;` |
|   27427 | 12023 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 12024 | `			/* Consume the raw chunk without any processing */` |
|   13737 | 12025 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13737 | 12026 | `			if( pRawObj == 0 ){` |
|     ! 0 | 12027 | `				rc = SXERR_MEM;` |
|     ! 0 | 12028 | `				break;` |
|       - | 12029 | `			}` |
|       - | 12030 | `			/* Mark as constant and emit the load constant instruction */` |
|   13737 | 12031 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13737 | 12032 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13737 | 12033 | `			++nRawObj;` |
|   13737 | 12034 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 12035 | `		}` |
|   13695 | 12036 | `		if( nRawObj > 0 ){` |
|       - | 12037 | `			/* Emit the consume instruction */` |
|   13695 | 12038 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6845 | 12039 | `		}` |
|    8671 | 12040 | `	}` |
|    8666 | 12041 | `cleanup:` |
|   17337 | 12042 | `	SySetRelease(&aRawToken);` |
|   17337 | 12043 | `	SySetRelease(&aPhpToken);` |
|       - | 12044 | `	/* Restore outer file's strict_types scope */` |
|   17337 | 12045 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   17337 | 12046 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   17337 | 12047 | `	return rc;` |
|    8671 | 12048 | `}` |
|       - | 12049 | `/*` |
|       - | 12050 | ` * Utility routines.Initialize the code generator.` |
|       - | 12051 | ` */` |
|    3586 | 12052 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 12053 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12054 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12055 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12056 | `	)` |
|       5 | 12057 | `{` |
|    3591 | 12058 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12059 | `	/* Zero the structure */` |
|    3591 | 12060 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 12061 | `	/* Initial state */` |
|    3591 | 12062 | `	pGen->pVm  = &(*pVm);` |
|    3591 | 12063 | `	pGen->xErr = xErr;` |
|    3591 | 12064 | `	pGen->pErrData = pErrData;` |
|    3591 | 12065 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3591 | 12066 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3591 | 12067 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3591 | 12068 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3591 | 12069 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 12070 | `	/* Error log buffer */` |
|    3591 | 12071 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 12072 | `	/* General purpose working buffer */` |
|    3591 | 12073 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 12074 | `	/* Namespace state */` |
|    3591 | 12075 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3591 | 12076 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3591 | 12077 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3591 | 12078 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12079 | `	/* Create the global scope */` |
|    3591 | 12080 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 12081 | `	/* Point to the global scope */` |
|    3591 | 12082 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3591 | 12083 | `	return SXRET_OK;` |
|       5 | 12084 | `}` |
|       - | 12085 | `/*` |
|       - | 12086 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 12087 | ` */` |
|   20562 | 12088 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 12089 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12090 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12091 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12092 | `	)` |
|       5 | 12093 | `{` |
|   20567 | 12094 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12095 | `	GenBlock *pBlock,*pParent;` |
|       - | 12096 | `	/* Reset state */` |
|   20567 | 12097 | `	SySetReset(&pGen->aLabel);` |
|   20567 | 12098 | `	SySetReset(&pGen->aGoto);` |
|   20567 | 12099 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   20567 | 12100 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   20567 | 12101 | `	SyBlobRelease(&pGen->sWorker);` |
|   20567 | 12102 | `	SyBlobRelease(&pGen->sNamespace);` |
|   20567 | 12103 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   20567 | 12104 | `	SyHashRelease(&pGen->hUseImports);` |
|   20567 | 12105 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   20567 | 12106 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   20567 | 12107 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   20567 | 12108 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   20567 | 12109 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12110 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 12111 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 12112 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 12113 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 12114 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 12115 | `	 * number of unique names, which is acceptable. */` |
|       - | 12116 | `	/* Point to the global scope */` |
|   20567 | 12117 | `	pBlock = pGen->pCurrent;` |
|   20567 | 12118 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 12119 | `		pParent = pBlock->pParent;` |
|     ! 0 | 12120 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 12121 | `		pBlock = pParent;` |
|     ! 0 | 12122 | `	}` |
|   20567 | 12123 | `	pGen->xErr = xErr;` |
|   20567 | 12124 | `	pGen->pErrData = pErrData;` |
|   20567 | 12125 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   20567 | 12126 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   20567 | 12127 | `	pGen->pIn = pGen->pEnd = 0;` |
|   20567 | 12128 | `	pGen->nErr = 0;` |
|   20567 | 12129 | `	return SXRET_OK;` |
|       5 | 12130 | `}` |
|       - | 12131 | `/*` |
|       - | 12132 | ` * Generate a compile-time error message.` |
|       - | 12133 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 12134 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 12135 | ` * abort compilation immediately.` |
|       - | 12136 | ` */` |
|     624 | 12137 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 12138 | `{` |
|     629 | 12139 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     629 | 12140 | `	const char *zErr = "Error";` |
|       - | 12141 | `	SyString *pFile;` |
|       - | 12142 | `	va_list ap;` |
|       - | 12143 | `	sxi32 rc;` |
|       - | 12144 | `	/* Reset the working buffer */` |
|     629 | 12145 | `	SyBlobReset(pWorker);` |
|       - | 12146 | `	/* Peek the processed file path if available */` |
|     629 | 12147 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     629 | 12148 | `	if( nErrType == E_ERROR ){` |
|       - | 12149 | `		/* Increment the error counter */` |
|     517 | 12150 | `		pGen->nErr++;` |
|     517 | 12151 | `		if( pGen->nErr > 15 ){` |
|       - | 12152 | `			/* Error count limit reached */` |
|       6 | 12153 | `			if( pGen->xErr ){` |
|       6 | 12154 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       6 | 12155 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       6 | 12156 | `				if( pFile ){` |
|       6 | 12157 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 12158 | `				}` |
|       6 | 12159 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       6 | 12160 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       6 | 12161 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 12162 | `				}` |
|       2 | 12163 | `			}` |
|       - | 12164 | `			/* Abort immediately */` |
|       6 | 12165 | `			return SXERR_ABORT;` |
|       - | 12166 | `		}` |
|     254 | 12167 | `	}` |
|     625 | 12168 | `	if( pGen->xErr == 0 ){` |
|       - | 12169 | `		/* No available error consumer,return immediately */` |
|       3 | 12170 | `		return SXRET_OK;` |
|       - | 12171 | `	}` |
|     622 | 12172 | `	switch(nErrType){` |
|     510 | 12173 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      30 | 12174 | `	case E_WARNING: zErr = "Warning";     break;` |
|      82 | 12175 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      11 | 12176 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 12177 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 12178 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 12179 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 12180 | `	default:` |
|     ! 0 | 12181 | `		break;` |
|       - | 12182 | `	}` |
|     622 | 12183 | `	rc = SXRET_OK;` |
|       - | 12184 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     622 | 12185 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     622 | 12186 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     622 | 12187 | `	va_start(ap,zFormat);` |
|     622 | 12188 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     622 | 12189 | `	va_end(ap);` |
|     622 | 12190 | `	if( pFile ){` |
|     622 | 12191 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     309 | 12192 | `	}` |
|       - | 12193 | `	/* Append a new line */` |
|     622 | 12194 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     622 | 12195 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 12196 | `		/* Consume the generated error message */` |
|     622 | 12197 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     309 | 12198 | `	}` |
|     622 | 12199 | `	return rc;` |
|     317 | 12200 | `}` |
|       - | 12201 |  |
