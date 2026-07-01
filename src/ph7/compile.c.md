# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5839/7361 lines (79.32%)

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
|     276 |   120 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
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
|    3922 |   137 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   138 | `{` |
|    3927 |   139 | `	GenBlock *pBlock = pCurrent;` |
|   11183 |   140 | `	for(;;){` |
|   22371 |   141 | `		if( pBlock->iFlags & iBlockType ){` |
|    3819 |   142 | `			iCount--; /* Decrement nesting level */` |
|    3819 |   143 | `			if( iCount < 1 ){` |
|       - |   144 | `				/* Block meet with the desired criteria */` |
|    3793 |   145 | `				return pBlock;` |
|       - |   146 | `			}` |
|      13 |   147 | `		}` |
|       - |   148 | `		/* Point to the upper block */` |
|   18583 |   149 | `		pBlock = pBlock->pParent;` |
|   18583 |   150 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   151 | `			/* Forbidden */` |
|      72 |   152 | `			break;` |
|       - |   153 | `		}` |
|       5 |   154 | `	}` |
|       - |   155 | `	/* No such block */` |
|     139 |   156 | `	return 0;` |
|    1966 |   157 | `}` |
|       - |   158 | `/*` |
|       - |   159 | ` * Initialize a freshly allocated block instance.` |
|       - |   160 | ` */` |
|  861144 |   161 | `static void GenStateInitBlock(` |
|       - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   166 | `	void *pUserData      /* Upper layer private data */` |
|       - |   167 | `	)` |
|       5 |   168 | `{` |
|       - |   169 | `	/* Initialize block fields */` |
|  861149 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  861149 |   171 | `	pBlock->pUserData   = pUserData;` |
|  861149 |   172 | `	pBlock->pGen        = pGen;` |
|  861149 |   173 | `	pBlock->iFlags      = iType;` |
|  861149 |   174 | `	pBlock->pParent     = 0;` |
|  861149 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  861149 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  861149 |   177 | `}` |
|       - |   178 | `/*` |
|       - |   179 | ` * Allocate a new block instance.` |
|       - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   182 | ` * processing on failure.` |
|       - |   183 | ` */` |
|  857500 |   184 | `static sxi32 GenStateEnterBlock(` |
|       - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   190 | `	)` |
|       5 |   191 | `{` |
|       - |   192 | `	GenBlock *pBlock;` |
|       - |   193 | `	/* Allocate a new block instance */` |
|  857505 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  857505 |   195 | `	if( pBlock == 0 ){` |
|       - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   198 | `		 */` |
|     ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   200 | `		/* Abort processing immediately */` |
|     ! 0 |   201 | `		return SXERR_ABORT;` |
|       - |   202 | `	}` |
|       - |   203 | `	/* Zero the structure */` |
|  857505 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  857505 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   206 | `	/* Link to the parent block */` |
|  857505 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   208 | `	/* Mark as the current block */` |
|  857505 |   209 | `	pGen->pCurrent = pBlock;` |
|  857505 |   210 | `	if( ppBlock ){` |
|       - |   211 | `		/* Write a pointer to the new instance */` |
|  416523 |   212 | `		*ppBlock = pBlock;` |
|  208259 |   213 | `	}` |
|  857505 |   214 | `	return SXRET_OK;` |
|  428755 |   215 | `}` |
|       - |   216 | `/*` |
|       - |   217 | ` * Release block fields without freeing the whole instance.` |
|       - |   218 | ` */` |
|  857492 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   220 | `{` |
|  857497 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  857497 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  857497 |   223 | `}` |
|       - |   224 | `/*` |
|       - |   225 | ` * Release a block.` |
|       - |   226 | ` */` |
|  857492 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   228 | `{` |
|  857497 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  857497 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   231 | `	/* Free the instance */` |
|  857497 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  857497 |   233 | `}` |
|       - |   234 | `/*` |
|       - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   236 | ` */` |
|  857492 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   238 | `{` |
|  857497 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  857497 |   240 | `	if( pBlock == 0 ){` |
|       - |   241 | `		/* No more block to pop */` |
|     ! 0 |   242 | `		return SXERR_EMPTY;` |
|       - |   243 | `	}` |
|       - |   244 | `	/* Point to the upper block */` |
|  857497 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  857497 |   246 | `	if( ppBlock ){` |
|       - |   247 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   248 | `		*ppBlock = pBlock;` |
|     ! 0 |   249 | `	}else{` |
|       - |   250 | `		/* Safely release the block */` |
|  857497 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   252 | `	}` |
|  857497 |   253 | `	return SXRET_OK;` |
|  428751 |   254 | `}` |
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
|  246974 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   266 | `{` |
|       - |   267 | `	JumpFixup sJumpFix;` |
|       - |   268 | `	sxi32 rc;` |
|       - |   269 | `	/* Init the JumpFixup structure */` |
|  246979 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  246979 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   272 | `	/* Insert in the jump fixup table */` |
|  246979 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  246979 |   274 | `	return rc;` |
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
|  598336 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   288 | `{` |
|       - |   289 | `	JumpFixup *aFix;` |
|       - |   290 | `	VmInstr *pInstr;` |
|       - |   291 | `	sxu32 nFixed;` |
|       - |   292 | `	sxu32 n;` |
|       - |   293 | `	/* Point to the jump fixup table */` |
|  598341 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   295 | `	/* Fix the desired jumps */` |
| 1080983 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  482647 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   298 | `			/* Already fixed */` |
|  190807 |   299 | `			continue;` |
|       - |   300 | `		}` |
|  291845 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   302 | `			/* Not of our interest */` |
|   44873 |   303 | `			continue;` |
|       - |   304 | `		}` |
|       - |   305 | `		/* Point to the instruction to fix */` |
|  246977 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  246977 |   307 | `		if( pInstr ){` |
|  246977 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  246977 |   309 | `			nFixed++;` |
|       - |   310 | `			/* Mark as fixed */` |
|  246977 |   311 | `			aFix[n].nJumpType = -1;` |
|  123486 |   312 | `		}` |
|  123491 |   313 | `	}` |
|       - |   314 | `	/* Total number of fixed jumps */` |
|  598341 |   315 | `	return nFixed;` |
|       5 |   316 | `}` |
|       - |   317 | `/*` |
|       - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   319 | ` * The goto statement can be used to jump to another section` |
|       - |   320 | ` * in the program.` |
|       - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   322 | ` * statement for more information.` |
|       - |   323 | ` */` |
|  242830 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   325 | `{` |
|       - |   326 | `	JumpFixup *pJump,*aJumps;` |
|       - |   327 | `	Label *pLabel,*aLabel;` |
|       - |   328 | `	VmInstr *pInstr;` |
|       - |   329 | `	sxi32 rc;` |
|       - |   330 | `	sxu32 n;` |
|       - |   331 | `	/* Point to the goto table */` |
|  242835 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   333 | `	/* Fix */` |
|  242981 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  242833 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  242965 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   362 | `			/* Emit a warning */` |
|      40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   365 | `		}` |
|      71 |   366 | `	}` |
|  242833 |   367 | `	return SXRET_OK;` |
|  121420 |   368 | `}` |
|       - |   369 | `/*` |
|       - |   370 | ` * Check if a given token value is installed in the literal table.` |
|       - |   371 | ` */` |
|  783602 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   373 | `{` |
|       - |   374 | `	SyHashEntry *pEntry;` |
|  783607 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  783607 |   376 | `	if( pEntry == 0 ){` |
|  352951 |   377 | `		return SXERR_NOTFOUND;` |
|       - |   378 | `	}` |
|  430661 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  430661 |   380 | `	return SXRET_OK;` |
|  391806 |   381 | `}` |
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
|  352946 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   393 | `{` |
|  352951 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  352951 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  176473 |   396 | `	}` |
|  352951 |   397 | `	return SXRET_OK;` |
|       5 |   398 | `}` |
|       - |   399 | `/*` |
|       - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   401 | ` * in the constant table.` |
|       - |   402 | ` */` |
|  127870 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   404 | `{` |
|       - |   405 | `	ph7_value *pObj;` |
|  127875 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   407 | `	/* Reserve a new constant */` |
|  127875 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  127875 |   409 | `	if( pObj == 0 ){` |
|     ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   411 | `		return 0;` |
|       - |   412 | `	}` |
|  127875 |   413 | `	*pIdx = nIdx;` |
|       - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   416 | `	 */` |
|  127875 |   417 | `	return pObj;` |
|   63940 |   418 | `}` |
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
|  489288 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   434 | `{` |
|       - |   435 | `	VmCallArgMap *pMap;` |
|  489293 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|      33 |   437 | `	if( p3 == 0 ){` |
|      31 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      31 |   439 | `		if( pMap == 0 ) return 0;` |
|      31 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      31 |   441 | `		p3 = (void *)pMap;` |
|      14 |   442 | `	}` |
|      33 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      33 |   444 | `	return p3;` |
|  244649 |   445 | `}` |
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
|  128600 |   504 | `static int GenStateFindBadNumericSeparator(` |
|       - |   505 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   506 | `{` |
|  128605 |   507 | `	const char *z = pRaw->zString;` |
|  128605 |   508 | `	sxu32 n = pRaw->nByte;` |
|  128605 |   509 | `	int base = 10;` |
|       - |   510 | `	sxu32 i, start;` |
|  128605 |   511 | `	if( n < 2 ) return 0;` |
|   10691 |   512 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   513 | `		base = 16;` |
|   10656 |   514 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   515 | `		base = 2;` |
|     139 |   516 | `	}` |
|   38633 |   517 | `	for( i = 0; i < n; ++i ){` |
|   27961 |   518 | `		if( z[i] != '_' ) continue;` |
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
|   10677 |   535 | `	return 0;` |
|   64305 |   536 | `}` |
|       - |   537 | `/*` |
|       - |   538 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   539 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   540 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   541 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   542 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   543 | ` * so callers can bail from the current construct).` |
|       - |   544 | ` */` |
|  128600 |   545 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   546 | `{` |
|  128605 |   547 | `	const char *zBad = 0;` |
|  128605 |   548 | `	sxu32 nBad = 0;` |
|       - |   549 | `	SyString sBad;` |
|       - |   550 | `	sxi32 rc;` |
|  128605 |   551 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  128591 |   552 | `		return SXRET_OK;` |
|       - |   553 | `	}` |
|      18 |   554 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   555 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   556 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   557 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   558 | `		return SXERR_ABORT;` |
|       - |   559 | `	}` |
|      18 |   560 | `	return SXERR_SYNTAX;` |
|   64305 |   561 | `}` |
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
|  128586 |   578 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   579 | `	SyMemBackend *pAlloc,` |
|       - |   580 | `	const SyString *pToken,` |
|       - |   581 | `	char *zScratch, sxu32 nScratch,` |
|       - |   582 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   583 | `{` |
|       - |   584 | `	sxu32 i, j;` |
|  128591 |   585 | `	int hasUnderscore = 0;` |
|       - |   586 | `	char *zBuf;` |
|  128591 |   587 | `	*pzAlloc = 0;` |
|  272381 |   588 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  144047 |   589 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   71900 |   590 | `	}` |
|  128591 |   591 | `	if( !hasUnderscore ){` |
|  128339 |   592 | `		SyStringDupPtr(pOut, pToken);` |
|  128339 |   593 | `		return SXRET_OK;` |
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
|   64298 |   610 | `}` |
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
|  128572 |   627 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   628 | `{` |
|  128577 |   629 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  128577 |   630 | `	sxu32 nIdx = 0;` |
|       - |   631 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  128577 |   632 | `	char *zAlloc = 0;` |
|       - |   633 | `	SyString sNum;` |
|       - |   634 | `	sxi32 rc;` |
|   64286 |   635 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  128577 |   636 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  128577 |   637 | `	if( rc != SXRET_OK ){` |
|      14 |   638 | `		return rc;` |
|       - |   639 | `	}` |
|  192848 |   640 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   64281 |   641 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  128567 |   642 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   643 | `		return SXERR_ABORT;` |
|       - |   644 | `	}` |
|  128567 |   645 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   646 | `		ph7_value *pObj;` |
|       - |   647 | `		sxi64 iValue;` |
|  127875 |   648 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  127875 |   649 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  127875 |   650 | `		if( pObj == 0 ){` |
|     ! 0 |   651 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   652 | `			return SXERR_ABORT;` |
|       - |   653 | `		}` |
|  127875 |   654 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   63940 |   655 | `	}else{` |
|       - |   656 | `		/* Real number */` |
|       - |   657 | `		ph7_value *pObj;` |
|       - |   658 | `		/* Reserve a new constant */` |
|     697 |   659 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     697 |   660 | `		if( pObj == 0 ){` |
|     ! 0 |   661 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   662 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   663 | `			return SXERR_ABORT;` |
|       - |   664 | `		}` |
|     697 |   665 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     697 |   666 | `		PH7_MemObjToReal(pObj);` |
|       - |   667 | `	}` |
|  128567 |   668 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   669 | `	/* Emit the load constant instruction */` |
|  128567 |   670 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   671 | `	/* Node successfully compiled */` |
|  128567 |   672 | `	return SXRET_OK;` |
|   64291 |   673 | `}` |
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
|  102618 |   685 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   686 | `{` |
|  102623 |   687 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   688 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   689 | `	ph7_value *pObj;` |
|       - |   690 | `	sxu32 nIdx;` |
|  102623 |   691 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   692 | `	/* Delimit the string */` |
|  102623 |   693 | `	zIn  = pStr->zString;` |
|  102623 |   694 | `	zEnd = &zIn[pStr->nByte];` |
|  102623 |   695 | `	if( zIn >= zEnd ){` |
|       - |   696 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   697 | `		 * rather than reserving a new object each time. */` |
|    7459 |   698 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    7459 |   699 | `		return SXRET_OK;` |
|       - |   700 | `	}` |
|   95169 |   701 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   702 | `		/* Already processed,emit the load constant instruction` |
|       - |   703 | `		 * and return.` |
|       - |   704 | `		 */` |
|   36761 |   705 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   36761 |   706 | `		return SXRET_OK;` |
|       - |   707 | `	}` |
|       - |   708 | `	/* Reserve a new constant */` |
|   58413 |   709 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   58413 |   710 | `	if( pObj == 0 ){` |
|     ! 0 |   711 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   712 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   713 | `		return SXERR_ABORT;` |
|       - |   714 | `	}` |
|   58413 |   715 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   716 | `	/* Compile the node */` |
|   58465 |   717 | `	for(;;){` |
|  116935 |   718 | `		if( zIn >= zEnd ){` |
|       - |   719 | `			/* End of input */` |
|   58413 |   720 | `			break;` |
|       - |   721 | `		}` |
|   58527 |   722 | `		zCur = zIn;` |
| 1000087 |   723 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  941565 |   724 | `			zIn++;` |
|       5 |   725 | `		}` |
|   58527 |   726 | `		if( zIn > zCur ){` |
|       - |   727 | `			/* Append raw contents*/` |
|   58503 |   728 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   29249 |   729 | `		}` |
|   58527 |   730 | `		zIn++;` |
|   58527 |   731 | `		if( zIn < zEnd ){` |
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
|   58527 |   746 | `		zIn++;` |
|       5 |   747 | `	}` |
|       - |   748 | `	/* Emit the load constant instruction */` |
|   58413 |   749 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   58413 |   750 | `	if( pStr->nByte < 1024 ){` |
|       - |   751 | `		/* Install in the literal table */` |
|   58413 |   752 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   29204 |   753 | `	}` |
|       - |   754 | `	/* Node successfully compiled */` |
|   58413 |   755 | `	return SXRET_OK;` |
|   51314 |   756 | `}` |
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
|    2274 |   922 | `static sxi32 GenStateProcessStringExpression(` |
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
|    2279 |   933 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   934 | `	/* Preallocate some slots */` |
|    2279 |   935 | `	SySetAlloc(&sToken,0x08);` |
|       - |   936 | `	/* Tokenize the text */` |
|    2279 |   937 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   938 | `	/* Swap delimiter */` |
|    2279 |   939 | `	pTmpIn  = pGen->pIn;` |
|    2279 |   940 | `	pTmpEnd = pGen->pEnd;` |
|    2279 |   941 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2279 |   942 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   943 | `	/* Compile the expression */` |
|    2279 |   944 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   945 | `	/* Restore token stream */` |
|    2279 |   946 | `	pGen->pIn  = pTmpIn;` |
|    2279 |   947 | `	pGen->pEnd = pTmpEnd;` |
|       - |   948 | `	/* Release the token set */` |
|    2279 |   949 | `	SySetRelease(&sToken);` |
|       - |   950 | `	/* Compilation result */` |
|    2279 |   951 | `	return rc;` |
|       5 |   952 | `}` |
|       - |   953 | `/*` |
|       - |   954 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   955 | ` */` |
|   25654 |   956 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   957 | `{` |
|       - |   958 | `	ph7_value *pConstObj;` |
|   25659 |   959 | `	sxu32 nIdx = 0;` |
|       - |   960 | `	/* Reserve a new constant */` |
|   25659 |   961 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   25659 |   962 | `	if( pConstObj == 0 ){` |
|     ! 0 |   963 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   964 | `		return 0;` |
|       - |   965 | `	}` |
|   25659 |   966 | `	(*pCount)++;` |
|   25659 |   967 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   968 | `	/* Emit the load constant instruction */` |
|   25659 |   969 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   25659 |   970 | `	return pConstObj;` |
|   12832 |   971 | `}` |
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
|   24170 |  1010 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       5 |  1011 | `{` |
|   24175 |  1012 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1013 | `	const char *zIn,*zCur,*zEnd;` |
|   24175 |  1014 | `	ph7_value *pObj = 0;` |
|       - |  1015 | `	sxi32 iCons;` |
|       - |  1016 | `	sxi32 rc;` |
|       - |  1017 | `	/* Delimit the string */` |
|   24175 |  1018 | `	zIn  = pStr->zString;` |
|   24175 |  1019 | `	zEnd = &zIn[pStr->nByte];` |
|   24175 |  1020 | `	if( zIn >= zEnd ){` |
|       - |  1021 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1022 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1023 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1024 | `		 */` |
|     319 |  1025 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     319 |  1026 | `		return SXRET_OK;` |
|       - |  1027 | `	}` |
|   23861 |  1028 | `	zCur = 0;` |
|       - |  1029 | `	/* Compile the node */` |
|   23861 |  1030 | `	iCons = 0;` |
|   13065 |  1031 | `	for(;;){` |
|   39005 |  1032 | `		zCur = zIn;` |
|  182235 |  1033 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  145509 |  1034 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      67 |  1035 | `				break;` |
|  145385 |  1036 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2154 |  1037 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1078 |  1038 | `					break;` |
|       - |  1039 | `			}` |
|  143235 |  1040 | `			zIn++;` |
|       5 |  1041 | `		}` |
|   39005 |  1042 | `		if( zIn > zCur ){` |
|   18179 |  1043 | `			if( pObj == 0 ){` |
|   17691 |  1044 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   17691 |  1045 | `				if( pObj == 0 ){` |
|     ! 0 |  1046 | `					return SXERR_ABORT;` |
|       - |  1047 | `				}` |
|    8843 |  1048 | `			}` |
|   18179 |  1049 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    9087 |  1050 | `		}` |
|   39005 |  1051 | `		if( zIn >= zEnd ){` |
|   23861 |  1052 | `			break;` |
|       - |  1053 | `		}` |
|   15149 |  1054 | `		if( zIn[0] == '\\' ){` |
|   12875 |  1055 | `			const char *zPtr = 0;` |
|       - |  1056 | `			sxu32 n;` |
|   12875 |  1057 | `			zIn++;` |
|   12875 |  1058 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1059 | `				break;` |
|       - |  1060 | `			}` |
|   12875 |  1061 | `			if( pObj == 0 ){` |
|    7973 |  1062 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    7973 |  1063 | `				if( pObj == 0 ){` |
|     ! 0 |  1064 | `					return SXERR_ABORT;` |
|       - |  1065 | `				}` |
|    3984 |  1066 | `			}` |
|   12875 |  1067 | `			n = sizeof(char); /* size of conversion */` |
|   12875 |  1068 | `			switch( zIn[0] ){` |
|       7 |  1069 | `			case '$':` |
|       - |  1070 | `				/* Dollar sign */` |
|      15 |  1071 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|      15 |  1072 | `				break;` |
|      56 |  1073 | `			case '\\':` |
|       - |  1074 | `				/* A literal backslash */` |
|     116 |  1075 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     116 |  1076 | `				break;` |
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
|    5945 |  1089 | `			case 'n':` |
|       - |  1090 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   11895 |  1091 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   11895 |  1092 | `				break;` |
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
|   12875 |  1160 | `			zIn += n;` |
|   12875 |  1161 | `			continue;` |
|       - |  1162 | `		}` |
|    2279 |  1163 | `		if( zIn[0] == '{' ){` |
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
|    2151 |  1197 | `			const char *zExpr = zIn;` |
|       - |  1198 | `			/* Assemble variable name */` |
|    1083 |  1199 | `			for(;;){` |
|       - |  1200 | `				/* Jump leading dollars */` |
|    4317 |  1201 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2151 |  1202 | `					zIn++;` |
|       5 |  1203 | `				}` |
|    1083 |  1204 | `				for(;;){` |
|   11958 |  1205 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8709 |  1206 | `						zIn++;` |
|       5 |  1207 | `					}` |
|    2171 |  1208 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1209 | `						/* UTF-8 stream */` |
|     ! 0 |  1210 | `						zIn++;` |
|     ! 0 |  1211 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1212 | `							zIn++;` |
|     ! 0 |  1213 | `						}` |
|     ! 0 |  1214 | `						continue;` |
|       - |  1215 | `					}` |
|    2171 |  1216 | `					break;` |
|     ! 0 |  1217 | `				}` |
|    2171 |  1218 | `				if( zIn >= zEnd ){` |
|     217 |  1219 | `					break;` |
|       - |  1220 | `				}` |
|    1959 |  1221 | `				if( zIn[0] == '[' ){` |
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
|    1949 |  1239 | `				}else if(zIn[0] == '{' ){` |
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
|    1945 |  1257 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1258 | `					/* Member access operator '->' */` |
|      23 |  1259 | `					zIn += 2;` |
|    1935 |  1260 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1261 | `					/* Static member access operator '::' */` |
|     ! 0 |  1262 | `					zIn += 2;` |
|     ! 0 |  1263 | `				}else{` |
|     965 |  1264 | `					break;` |
|       - |  1265 | `				}` |
|       3 |  1266 | `			}` |
|       - |  1267 | `			/* Process the expression */` |
|    2151 |  1268 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2151 |  1269 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1270 | `				return SXERR_ABORT;` |
|       - |  1271 | `			}` |
|    2151 |  1272 | `			if( rc != SXERR_EMPTY ){` |
|    2149 |  1273 | `				++iCons;` |
|    1072 |  1274 | `			}` |
|       - |  1275 | `		}` |
|       - |  1276 | `		/* Invalidate the previously used constant */` |
|    2279 |  1277 | `		pObj = 0;` |
|       5 |  1278 | `	}/*for(;;)*/` |
|   23861 |  1279 | `	if( iCons > 1 ){` |
|       - |  1280 | `		/* Concatenate all compiled constants */` |
|    1691 |  1281 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     843 |  1282 | `	}` |
|       - |  1283 | `	/* Node successfully compiled */` |
|   23861 |  1284 | `	return SXRET_OK;` |
|   12090 |  1285 | `}` |
|       - |  1286 | `/*` |
|       - |  1287 | ` * Compile a double quoted string.` |
|       - |  1288 | ` *  See the block-comment above for more information.` |
|       - |  1289 | ` */` |
|   24110 |  1290 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1291 | `{` |
|       - |  1292 | `	sxi32 rc;` |
|   24115 |  1293 | `	rc = GenStateCompileString(&(*pGen));` |
|   12055 |  1294 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1295 | `	/* Compilation result */` |
|   24115 |  1296 | `	return rc;` |
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
|   22388 |  1340 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   22393 |  1351 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1352 | `	/* Compile the expression*/` |
|   22393 |  1353 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1354 | `	/* Restore token stream */` |
|   22393 |  1355 | `	RE_SWAP_DELIMITER(pGen);` |
|   22393 |  1356 | `	return rc;` |
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
|   24812 |  1397 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 |  1398 | `{` |
|   24817 |  1399 | `	SyToken *pCur = pStart;` |
|   24817 |  1400 | `	sxi32 iNest = 0;` |
|   70383 |  1401 | `	while( pCur < pEnd ){` |
|   51155 |  1402 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    5585 |  1403 | `			return pCur;` |
|       - |  1404 | `		}` |
|       - |  1405 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1406 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - |  1407 | `		 * not an entry separator. Skip past the signature.` |
|       - |  1408 | `		 */` |
|   45575 |  1409 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|   45569 |  1470 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     387 |  1471 | `			iNest++;` |
|   45378 |  1472 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1473 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - |  1474 | `			 * parser will shortly detect any syntax error. */` |
|     387 |  1475 | `			iNest--;` |
|     191 |  1476 | `		}` |
|   45569 |  1477 | `		pCur++;` |
|       5 |  1478 | `	}` |
|   19233 |  1479 | `	return pEnd;` |
|   12411 |  1480 | `}` |
|       - |  1481 | `/*` |
|       - |  1482 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1483 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1484 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1485 | ` */` |
|   32066 |  1486 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1487 | `{` |
|       - |  1488 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1489 | `	SyToken *pKey,*pCur;` |
|   32071 |  1490 | `	sxi32 iEmitRef = 0;` |
|   32071 |  1491 | `	sxi32 iSpread = 0;` |
|   32071 |  1492 | `	sxi32 nPair = 0;` |
|       - |  1493 | `	sxi32 rc;` |
|   32071 |  1494 | `	xValidator = 0;` |
|   26297 |  1495 | `	for(;;){` |
|       - |  1496 | `		/* Jump leading commas */` |
|   59733 |  1497 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    7139 |  1498 | `			pGen->pIn++;` |
|       5 |  1499 | `		}` |
|   52599 |  1500 | `		pCur = pGen->pIn;` |
|   52599 |  1501 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1502 | `			/* No more entry to process */` |
|   32055 |  1503 | `			break;` |
|       - |  1504 | `		}` |
|   20549 |  1505 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1506 | `			continue;` |
|       - |  1507 | `		}` |
|       - |  1508 | `		/* Compile the key if available */` |
|   20549 |  1509 | `		pKey = pCur;` |
|   20549 |  1510 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   20549 |  1511 | `		rc = SXERR_EMPTY;` |
|   20549 |  1512 | `		if( pCur < pGen->pIn ){` |
|    1661 |  1513 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1514 | `				/* Missing value */` |
|      13 |  1515 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      13 |  1516 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1517 | `					return SXERR_ABORT;` |
|       - |  1518 | `				}` |
|      13 |  1519 | `				return SXRET_OK;` |
|       - |  1520 | `			}` |
|       - |  1521 | `			/* Compile the expression holding the key */` |
|    1651 |  1522 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1523 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1651 |  1524 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1525 | `				return SXERR_ABORT;` |
|       - |  1526 | `			}` |
|    1651 |  1527 | `			pCur++; /* Jump the '=>' operator */` |
|   19716 |  1528 | `		}else if( pKey == pCur ){` |
|       - |  1529 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1530 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1531 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1532 | `		}else{` |
|       - |  1533 | `			/* Reset back the cursor and point to the entry value */` |
|   18893 |  1534 | `			pCur = pKey;` |
|       - |  1535 | `		}` |
|   20539 |  1536 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1537 | `			/* No available key,load NULL */` |
|   18895 |  1538 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    9445 |  1539 | `		}` |
|   20539 |  1540 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   20537 |  1559 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   20537 |  1560 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   20533 |  1573 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   20533 |  1574 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1575 | `			return SXERR_ABORT;` |
|       - |  1576 | `		}` |
|   20533 |  1577 | `		if( iSpread ){` |
|       - |  1578 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      65 |  1579 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   20502 |  1580 | `		}else if( iEmitRef ){` |
|       - |  1581 | `			/* Emit the load reference instruction */` |
|      40 |  1582 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1583 | `		}` |
|   20533 |  1584 | `		xValidator = 0;` |
|   20533 |  1585 | `		iEmitRef = 0;` |
|   20533 |  1586 | `		iSpread = 0;` |
|   20533 |  1587 | `		nPair++;` |
|       5 |  1588 | `	}` |
|       - |  1589 | `	/* Emit the load map instruction */` |
|   32055 |  1590 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1591 | `	/* Node successfully compiled */` |
|   32055 |  1592 | `	return SXRET_OK;` |
|   16038 |  1593 | `}` |
|       - |  1594 | `/*` |
|       - |  1595 | ` * Compile the 'array' language construct.` |
|       - |  1596 | ` *	 According to the PHP language reference manual` |
|       - |  1597 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1598 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1599 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1600 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1601 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1602 | ` */` |
|   30960 |  1603 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1604 | `{` |
|       - |  1605 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   30965 |  1606 | `	pGen->pIn += 2;` |
|   30965 |  1607 | `	pGen->pEnd--;` |
|   15480 |  1608 | `	SXUNUSED(iCompileFlag);` |
|   30965 |  1609 | `	return GenStateCompileArrayBody(pGen);` |
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
|       3 |  2007 | `{` |
|       - |  2008 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2009 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  2010 | `	sxu32 n, nEnv;` |
|       - |  2011 | `	char *zDup;` |
|     187 |  2012 | `	if( nByte == 0 ){` |
|     ! 0 |  2013 | `		return SXRET_OK;` |
|       - |  2014 | `	}` |
|     184 |  2015 | `	if( nByte == sizeof("this")-1` |
|     101 |  2016 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  2017 | `		return SXRET_OK;` |
|       - |  2018 | `	}` |
|     233 |  2019 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     172 |  2020 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     166 |  2021 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|     127 |  2022 | `			return SXRET_OK;` |
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
|      95 |  2043 | `}` |
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
|       4 |  2119 | `{` |
|     196 |  2120 | `	SyToken *pScan = pStart;` |
|       - |  2121 | `	sxi32 rc;` |
|     806 |  2122 | `	while( pScan < pEnd ){` |
|     614 |  2123 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
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
|     578 |  2134 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
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
|     560 |  2286 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     396 |  2287 | `			pScan++;` |
|     396 |  2288 | `			continue;` |
|       - |  2289 | `		}` |
|       - |  2290 | `		{` |
|       - |  2291 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     167 |  2292 | `			SyToken *pDollar = pScan;` |
|     246 |  2293 | `			while( &pDollar[1] < pEnd` |
|     167 |  2294 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2295 | `				pDollar++;` |
|     ! 0 |  2296 | `			}` |
|     167 |  2297 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2298 | `				break;` |
|       - |  2299 | `			}` |
|     167 |  2300 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2301 | `				pScan = pDollar + 1;` |
|     ! 0 |  2302 | `				continue;` |
|       - |  2303 | `			}` |
|     249 |  2304 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     164 |  2305 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      82 |  2306 | `				aShadow,nShadow);` |
|     167 |  2307 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2308 | `				return SXERR_ABORT;` |
|       - |  2309 | `			}` |
|     167 |  2310 | `			pScan = pDollar + 2;` |
|       - |  2311 | `		}` |
|       3 |  2312 | `	}` |
|     196 |  2313 | `	return SXRET_OK;` |
|     100 |  2314 | `}` |
|       - |  2315 | `/*` |
|       - |  2316 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2317 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2318 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2319 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2320 | ` * $this is also made available.` |
|       - |  2321 | ` */` |
|     174 |  2322 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
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
|     178 |  2338 | `	sxi32 iFlags = 0;` |
|     178 |  2339 | `	int bStatic = 0;` |
|       - |  2340 | `	sxi32 rc;` |
|       - |  2341 | `	sxu32 n;` |
|      87 |  2342 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2343 |  |
|     178 |  2344 | `	nLine = pGen->pIn->nLine;` |
|       - |  2345 | `	/* Optional 'static' prefix */` |
|     174 |  2346 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     178 |  2347 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2348 | `		bStatic = 1;` |
|       3 |  2349 | `		pGen->pIn++;` |
|       1 |  2350 | `	}` |
|       - |  2351 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     174 |  2352 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     178 |  2353 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2354 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2355 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2356 | `		return SXERR_SYNTAX;` |
|       - |  2357 | `	}` |
|     178 |  2358 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2359 | `	/* Optional '&' — return by reference */` |
|     178 |  2360 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2361 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2362 | `		pGen->pIn++;` |
|     ! 0 |  2363 | `	}` |
|       - |  2364 | `	/* Expect '(' */` |
|     178 |  2365 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
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
|     176 |  2376 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2377 | `	/* Delimit the parameter list */` |
|     176 |  2378 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     176 |  2379 | `	if( pSigEnd >= pGen->pEnd ){` |
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
|     172 |  2430 | `	pGen->pIn++; /* Jump '=>' */` |
|     172 |  2431 | `	pBodyStart = pGen->pIn;` |
|     172 |  2432 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2433 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2434 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2435 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2436 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     172 |  2437 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2438 | `	{` |
|     172 |  2439 | `		SyString *aShadow = 0;` |
|     172 |  2440 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     172 |  2441 | `		if( nShadow > 0 ){` |
|     101 |  2442 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      98 |  2443 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|     101 |  2444 | `			if( aShadow == 0 ){` |
|     ! 0 |  2445 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2446 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2447 | `				return SXERR_ABORT;` |
|       - |  2448 | `			}` |
|     225 |  2449 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     127 |  2450 | `				aShadow[n] = aArgs[n].sName;` |
|      65 |  2451 | `			}` |
|      49 |  2452 | `		}` |
|     256 |  2453 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      84 |  2454 | `			aShadow,nShadow);` |
|     172 |  2455 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2456 | `			return SXERR_ABORT;` |
|       - |  2457 | `		}` |
|       - |  2458 | `	}` |
|       - |  2459 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2460 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2461 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2462 | `	 * $this. */` |
|     172 |  2463 | `	if( !bStatic ){` |
|       - |  2464 | `		char *zThisDup;` |
|     170 |  2465 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     170 |  2466 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2467 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2468 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2469 | `			return SXERR_ABORT;` |
|       - |  2470 | `		}` |
|     170 |  2471 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     170 |  2472 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     170 |  2473 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     170 |  2474 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     170 |  2475 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      83 |  2476 | `	}` |
|       - |  2477 | `	/* Arrow functions are always closures */` |
|     172 |  2478 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2479 | `	/* Compile the body expression as an implicit return */` |
|     256 |  2480 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      84 |  2481 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     172 |  2482 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2483 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2484 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2485 | `		return SXERR_ABORT;` |
|       - |  2486 | `	}` |
|     172 |  2487 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     172 |  2488 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     172 |  2489 | `	pSavedEnd = pGen->pEnd;` |
|     172 |  2490 | `	pGen->pIn = pBodyStart;` |
|     172 |  2491 | `	pGen->pEnd = pBodyEnd;` |
|     172 |  2492 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     172 |  2493 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2494 | `		return SXERR_ABORT;` |
|       - |  2495 | `	}` |
|       - |  2496 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2497 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2498 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2499 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     172 |  2500 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     172 |  2501 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     172 |  2502 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     172 |  2503 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     172 |  2504 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2505 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     172 |  2506 | `	pGen->pIn = pBodyEnd;` |
|     172 |  2507 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2508 | `	/* Emit the load-closure instruction */` |
|     172 |  2509 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     172 |  2510 | `	return SXRET_OK;` |
|      91 |  2511 | `}` |
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
| 1165940 |  2867 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2868 | `{` |
| 1165945 |  2869 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2870 | `	sxi32 iVv;` |
|       - |  2871 | `	sxi32 iP1;` |
|       - |  2872 | `	void *p3;` |
|       - |  2873 | `	sxi32 rc;` |
| 1165945 |  2874 | `	iVv = -1; /* Variable variable counter */` |
| 2331897 |  2875 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1165957 |  2876 | `		pGen->pIn++;` |
| 1165957 |  2877 | `		iVv++;` |
|       5 |  2878 | `	}` |
| 1165945 |  2879 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2880 | `		/* Invalid variable name */` |
|     ! 0 |  2881 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2882 | `		if( rc == SXERR_ABORT ){` |
|       - |  2883 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2884 | `			return SXERR_ABORT;` |
|       - |  2885 | `		}` |
|     ! 0 |  2886 | `		return SXRET_OK;` |
|       - |  2887 | `	}` |
| 1165945 |  2888 | `	p3  = 0;` |
| 1165945 |  2889 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
| 1165929 |  2909 | `		char *zName = 0;` |
|       - |  2910 | `		/* Extract variable name */` |
| 1165929 |  2911 | `		pName = &pGen->pIn->sData;` |
|       - |  2912 | `		/* Advance the stream cursor */` |
| 1165929 |  2913 | `		pGen->pIn++;` |
| 1165929 |  2914 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1165929 |  2915 | `		if( pEntry == 0 ){` |
|       - |  2916 | `			/* Duplicate name */` |
|  167825 |  2917 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  167825 |  2918 | `			if( zName == 0 ){` |
|     ! 0 |  2919 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2920 | `				return SXERR_ABORT;` |
|       - |  2921 | `			}` |
|       - |  2922 | `			/* Install in the hashtable */` |
|  167825 |  2923 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   83915 |  2924 | `		}else{` |
|       - |  2925 | `			/* Name already available */` |
|  998109 |  2926 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2927 | `		}` |
| 1165929 |  2928 | `		p3 = (void *)zName;` |
|       - |  2929 | `	}` |
| 1165941 |  2930 | `	iP1 = 0;` |
| 1165941 |  2931 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  454841 |  2932 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2933 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  454823 |  2934 | `			iP1 = 1;` |
|  227409 |  2935 | `		}` |
|  227418 |  2936 | `	}` |
|       - |  2937 | `	/* Emit the load instruction */` |
| 1165941 |  2938 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1165953 |  2939 | `	while( iVv > 0 ){` |
|      13 |  2940 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2941 | `		iVv--;` |
|       1 |  2942 | `	}` |
|       - |  2943 | `	/* Node successfully compiled */` |
| 1165941 |  2944 | `	return SXRET_OK;` |
|  582975 |  2945 | `}` |
|       - |  2946 | `/*` |
|       - |  2947 | ` * Load a literal.` |
|       - |  2948 | ` */` |
|  804112 |  2949 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  2950 | `{` |
|  804117 |  2951 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2952 | `	ph7_value *pObj;` |
|       - |  2953 | `	SyString *pStr;` |
|       - |  2954 | `	sxu32 nIdx;` |
|       - |  2955 | `	/* Extract token value */` |
|  804117 |  2956 | `	pStr = &pToken->sData;` |
|       - |  2957 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  804117 |  2958 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  170467 |  2959 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2960 | `			/* NULL constant are always indexed at 0 */` |
|   62703 |  2961 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   62703 |  2962 | `			return SXRET_OK;` |
|  107769 |  2963 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2964 | `			/* TRUE constant are always indexed at 1 */` |
|     831 |  2965 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     831 |  2966 | `			return SXRET_OK;` |
|       5 |  2967 | `		}` |
|  741589 |  2968 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  108930 |  2969 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2970 | `			/* FALSE constant are always indexed at 2 */` |
|   48063 |  2971 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   48063 |  2972 | `			return SXRET_OK;` |
|  642671 |  2973 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  114148 |  2974 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2975 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|   10943 |  2976 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   10943 |  2977 | `			if( pObj == 0 ){` |
|     ! 0 |  2978 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2979 | `				return SXERR_ABORT;` |
|       - |  2980 | `			}` |
|   10943 |  2981 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2982 | `			/* Emit the load constant instruction */` |
|   10943 |  2983 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   10943 |  2984 | `			return SXRET_OK;` |
|  593097 |  2985 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   36876 |  2986 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2987 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 |  2988 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 |  2989 | `			if( pObj == 0 ){` |
|     ! 0 |  2990 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2991 | `				return SXERR_ABORT;` |
|       - |  2992 | `			}` |
|       7 |  2993 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2994 | `				SyString sNs;` |
|       7 |  2995 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 |  2996 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 |  2997 | `			}else{` |
|     ! 0 |  2998 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  2999 | `			}` |
|       7 |  3000 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 |  3001 | `			return SXRET_OK;` |
|  582338 |  3002 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   25289 |  3003 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  584545 |  3004 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   19808 |  3005 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|  681581 |  3035 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  3036 | `		ph7_value *pLitObj;` |
|       - |  3037 | `		/* Unknown literal,install it in the literal table */` |
|  290423 |  3038 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  290423 |  3039 | `		if( pLitObj == 0 ){` |
|     ! 0 |  3040 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3041 | `			return SXERR_ABORT;` |
|       - |  3042 | `		}` |
|  290423 |  3043 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  290423 |  3044 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  145209 |  3045 | `	}` |
|       - |  3046 | `	/* Emit the load constant instruction */` |
|  681581 |  3047 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  681581 |  3048 | `	return SXRET_OK;` |
|  402061 |  3049 | `}` |
|       - |  3050 | `/*` |
|       - |  3051 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  3052 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  3053 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  3054 | ` * Otherwise, load the simple literal directly.` |
|       - |  3055 | ` */` |
|  807796 |  3056 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  3057 | `{` |
|       - |  3058 | `	sxi32 rc;` |
|  807801 |  3059 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3060 | `		return SXRET_OK;` |
|       - |  3061 | `	}` |
|       - |  3062 | `	/* Check if this is a multi-token namespace path */` |
|  807801 |  3063 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  3064 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|    3689 |  3065 | `		SyBlob *pWorker = &pGen->sWorker;` |
|    3689 |  3066 | `		int isAbsolute = 0;` |
|    3689 |  3067 | `		SyBlobReset(pWorker);` |
|       - |  3068 | `		/* Check for leading backslash (absolute path) */` |
|    3689 |  3069 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|    3687 |  3070 | `			isAbsolute = 1;` |
|    3687 |  3071 | `			pGen->pIn++; /* Skip leading backslash */` |
|    1841 |  3072 | `		}` |
|       - |  3073 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|    3689 |  3074 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  3075 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  3076 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  3077 | `		}` |
|       - |  3078 | `		/* Collect all path components */` |
|    3785 |  3079 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    3785 |  3080 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      53 |  3081 | `				SyBlobAppend(pWorker,"\\",1);` |
|      29 |  3082 | `			}else{` |
|    3737 |  3083 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  3084 | `			}` |
|    3785 |  3085 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|    3689 |  3086 | `				pGen->pIn++;` |
|    3689 |  3087 | `				break;` |
|       - |  3088 | `			}` |
|     101 |  3089 | `			pGen->pIn++;` |
|       5 |  3090 | `		}` |
|    3689 |  3091 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  3092 | `			ph7_value *pObj;` |
|       - |  3093 | `			SyString sPath;` |
|       - |  3094 | `			sxu32 nIdx;` |
|    3689 |  3095 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  3096 | `			/* Install in the literal table */` |
|    3689 |  3097 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|    3665 |  3098 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    3665 |  3099 | `				if( pObj == 0 ){` |
|     ! 0 |  3100 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3101 | `					return SXERR_ABORT;` |
|       - |  3102 | `				}` |
|    3665 |  3103 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|    3665 |  3104 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|    1830 |  3105 | `			}` |
|       - |  3106 | `			/* Emit the load constant instruction.` |
|       - |  3107 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  3108 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|    5531 |  3109 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|    1842 |  3110 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|    1842 |  3111 | `				nIdx,0,0);` |
|    3689 |  3112 | `			return SXRET_OK;` |
|       - |  3113 | `		}` |
|     ! 0 |  3114 | `	}` |
|       - |  3115 | `	/* Single-token literal: load directly */` |
|  804117 |  3116 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  804117 |  3117 | `	return rc;` |
|  403903 |  3118 | `}` |
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
|  807796 |  3135 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3136 | `{` |
|       - |  3137 | `	sxi32 rc;` |
|  807801 |  3138 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  807801 |  3139 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3140 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3141 | `		return rc;` |
|       - |  3142 | `	}` |
|       - |  3143 | `	/* Node successfully compiled */` |
|  807801 |  3144 | `	return SXRET_OK;` |
|  403903 |  3145 | `}` |
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
|      29 |  3165 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3166 | `			return TRUE;` |
|      27 |  3167 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 |  3168 | `			return TRUE;` |
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
|      32 |  3219 | `	pName = &pGen->pIn->sData;` |
|       - |  3220 | `	/* Make sure the constant name isn't reserved */` |
|      32 |  3221 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3222 | `		/* Reserved constant */` |
|       9 |  3223 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 |  3224 | `		if( rc == SXERR_ABORT ){` |
|       - |  3225 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3226 | `			return SXERR_ABORT;` |
|       - |  3227 | `		}` |
|       9 |  3228 | `		goto Synchronize;` |
|       - |  3229 | `	}` |
|      23 |  3230 | `	pGen->pIn++;` |
|      23 |  3231 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
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
|      40 |  3279 | `		pGen->pIn++;` |
|       2 |  3280 | `	}` |
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
|    3784 |  3305 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3306 | `{` |
|    3789 |  3307 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   22217 |  3308 | `	while( pBlock && pBlock != pTarget ){` |
|   18433 |  3309 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
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
|   18433 |  3321 | `		pBlock = pBlock->pParent;` |
|       5 |  3322 | `	}` |
|    3789 |  3323 | `}` |
|    3688 |  3324 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3325 | `{` |
|       - |  3326 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3327 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3328 | `	sxu32 nLineLocal;` |
|       - |  3329 | `	sxi32 rc;` |
|    3693 |  3330 | `	nLineLocal = pGen->pIn->nLine;` |
|    3693 |  3331 | `	iLevel = 0;` |
|       - |  3332 | `	/* Jump the 'continue' keyword */` |
|    3693 |  3333 | `	pGen->pIn++;` |
|    3693 |  3334 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|    3693 |  3360 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3693 |  3361 | `	if( pLoop == 0 ){` |
|       - |  3362 | `		/* Illegal continue */` |
|      12 |  3363 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      12 |  3364 | `		if( rc == SXERR_ABORT ){` |
|       - |  3365 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3366 | `			return SXERR_ABORT;` |
|       - |  3367 | `		}` |
|       7 |  3368 | `	}else{` |
|    3683 |  3369 | `		sxu32 nInstrIdx = 0;` |
|       - |  3370 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    3683 |  3371 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    3683 |  3372 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
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
|    3679 |  3384 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3679 |  3385 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3386 | `				JumpFixup sJumpFix;` |
|       - |  3387 | `				/* Post-continue */` |
|      14 |  3388 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3389 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3390 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3391 | `			}` |
|       - |  3392 | `		}` |
|       - |  3393 | `	}` |
|    3693 |  3394 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3395 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3396 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3397 | `	}` |
|       - |  3398 | `	/* Statement successfully compiled */` |
|    3693 |  3399 | `	return SXRET_OK;` |
|    1849 |  3400 | `}` |
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
|      17 |  3422 | `		char *zAlloc = 0;` |
|       - |  3423 | `		SyString sNum;` |
|      17 |  3424 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3425 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3426 | `			return SXERR_ABORT;` |
|       - |  3427 | `		}` |
|      17 |  3428 | `		if( rc == SXRET_OK ){` |
|      20 |  3429 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3430 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3431 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3432 | `				return SXERR_ABORT;` |
|       - |  3433 | `			}` |
|      14 |  3434 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3435 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3436 | `		}` |
|      17 |  3437 | `		if( iLevel < 2 ){` |
|       3 |  3438 | `			iLevel = 0;` |
|       1 |  3439 | `		}` |
|      17 |  3440 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
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
|      36 |  3574 | `				break;` |
|       - |  3575 | `			}` |
|       - |  3576 | `			/* Point to the upper block */` |
|     167 |  3577 | `			pBlock = pBlock->pParent;` |
|       5 |  3578 | `		}` |
|     153 |  3579 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       8 |  3580 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       8 |  3581 | `			if( rc == SXERR_ABORT ){` |
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
|  442676 |  3677 | `static sxi32 PH7_CompileBlock(` |
|       - |  3678 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3679 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3680 | `	)` |
|       5 |  3681 | `{` |
|       - |  3682 | `	sxi32 rc;` |
|       - |  3683 | `	sxu32 nLine;` |
|  442681 |  3684 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  440987 |  3685 | `		nLine = pGen->pIn->nLine;` |
|  440987 |  3686 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  440987 |  3687 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3688 | `			return SXERR_ABORT;` |
|       - |  3689 | `		}` |
|  440987 |  3690 | `		pGen->pIn++;` |
|       - |  3691 | `		/* Compile until we hit the closing braces '}' */` |
|  603930 |  3692 | `		for(;;){` |
| 1207865 |  3693 | `			if( pGen->pIn >= pGen->pEnd ){` |
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
| 1207845 |  3704 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3705 | `				/* Closing braces found,break immediately*/` |
|  440967 |  3706 | `				pGen->pIn++;` |
|  440967 |  3707 | `				break;` |
|       - |  3708 | `			}` |
|       - |  3709 | `			/* Compile a single statement */` |
|  766883 |  3710 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  766883 |  3711 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3712 | `				return SXERR_ABORT;` |
|       - |  3713 | `			}` |
|       5 |  3714 | `		}` |
|  440987 |  3715 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  222190 |  3716 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|    1699 |  3760 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1699 |  3761 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3762 | `			return SXERR_ABORT;` |
|       - |  3763 | `		}` |
|       - |  3764 | `	}` |
|       - |  3765 | `	/* Jump trailing semi-colons ';' */` |
|  442681 |  3766 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3767 | `		pGen->pIn++;` |
|     ! 0 |  3768 | `	}` |
|  442681 |  3769 | `	return SXRET_OK;` |
|  221343 |  3770 | `}` |
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
|   14700 |  3790 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3791 | `{` |
|   14705 |  3792 | `	GenBlock *pWhileBlock = 0;` |
|   14705 |  3793 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3794 | `	sxu32 nFalseJump;` |
|       - |  3795 | `	sxu32 nLine;` |
|       - |  3796 | `	sxi32 rc;` |
|   14705 |  3797 | `	nLine = pGen->pIn->nLine;` |
|       - |  3798 | `	/* Jump the 'while' keyword */` |
|   14705 |  3799 | `	pGen->pIn++;` |
|   14705 |  3800 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3801 | `		/* Syntax error */` |
|     ! 0 |  3802 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3803 | `		if( rc == SXERR_ABORT ){` |
|       - |  3804 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3805 | `			return SXERR_ABORT;` |
|       - |  3806 | `		}` |
|     ! 0 |  3807 | `		goto Synchronize;` |
|       - |  3808 | `	}` |
|       - |  3809 | `	/* Jump the left parenthesis '(' */` |
|   14705 |  3810 | `	pGen->pIn++;` |
|       - |  3811 | `	/* Create the loop block */` |
|   14705 |  3812 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   14705 |  3813 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3814 | `		return SXERR_ABORT;` |
|       - |  3815 | `	}` |
|       - |  3816 | `	/* Delimit the condition */` |
|   14705 |  3817 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14705 |  3818 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3819 | `		/* Empty expression */` |
|       3 |  3820 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3821 | `		if( rc == SXERR_ABORT ){` |
|       - |  3822 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3823 | `			return SXERR_ABORT;` |
|       - |  3824 | `		}` |
|       1 |  3825 | `	}` |
|       - |  3826 | `	/* Swap token streams */` |
|   14705 |  3827 | `	pTmp = pGen->pEnd;` |
|   14705 |  3828 | `	pGen->pEnd = pEnd;` |
|       - |  3829 | `	/* Compile the expression */` |
|   14705 |  3830 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14705 |  3831 | `	if( rc == SXERR_ABORT ){` |
|       - |  3832 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3833 | `		return SXERR_ABORT;` |
|       - |  3834 | `	}` |
|       - |  3835 | `	/* Update token stream */` |
|   14705 |  3836 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3837 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3838 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3839 | `			return SXERR_ABORT;` |
|       - |  3840 | `		}` |
|     ! 0 |  3841 | `		pGen->pIn++;` |
|     ! 0 |  3842 | `	}` |
|       - |  3843 | `	/* Synchronize pointers */` |
|   14705 |  3844 | `	pGen->pIn  = &pEnd[1];` |
|   14705 |  3845 | `	pGen->pEnd = pTmp;` |
|       - |  3846 | `	/* Emit the false jump */` |
|   14705 |  3847 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3848 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14705 |  3849 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3850 | `	/* Compile the loop body */` |
|   14705 |  3851 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   14705 |  3852 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3853 | `		return SXERR_ABORT;` |
|       - |  3854 | `	}` |
|       - |  3855 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14705 |  3856 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3857 | `	/* Fix all jumps now the destination is resolved */` |
|   14705 |  3858 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3859 | `	/* Release the loop block */` |
|   14705 |  3860 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3861 | `	/* Statement successfully compiled */` |
|   14705 |  3862 | `	return SXRET_OK;` |
|     ! 0 |  3863 | `Synchronize:` |
|       - |  3864 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3865 | `	 * compiling this erroneous block.` |
|       - |  3866 | `	 */` |
|     ! 0 |  3867 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3868 | `		pGen->pIn++;` |
|     ! 0 |  3869 | `	}` |
|     ! 0 |  3870 | `	return SXRET_OK;` |
|    7355 |  3871 | `}` |
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
|   14698 |  4019 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  4020 | `{` |
|   14703 |  4021 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   14703 |  4022 | `	GenBlock *pForBlock = 0;` |
|       - |  4023 | `	sxu32 nFalseJump;` |
|       - |  4024 | `	sxu32 nLine;` |
|       - |  4025 | `	sxi32 rc;` |
|   14703 |  4026 | `	nLine = pGen->pIn->nLine;` |
|       - |  4027 | `	/* Jump the 'for' keyword */` |
|   14703 |  4028 | `	pGen->pIn++;` |
|   14703 |  4029 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4030 | `		/* Syntax error */` |
|     ! 0 |  4031 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  4032 | `		if( rc == SXERR_ABORT ){` |
|       - |  4033 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4034 | `			return SXERR_ABORT;` |
|       - |  4035 | `		}` |
|     ! 0 |  4036 | `		return SXRET_OK;` |
|       - |  4037 | `	}` |
|       - |  4038 | `	/* Jump the left parenthesis '(' */` |
|   14703 |  4039 | `	pGen->pIn++;` |
|       - |  4040 | `	/* Delimit the init-expr;condition;post-expr */` |
|   14703 |  4041 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14703 |  4042 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|   14703 |  4057 | `	pTmp = pGen->pEnd;` |
|   14703 |  4058 | `	pGen->pEnd = pEnd;` |
|       - |  4059 | `	/* Compile initialization expressions if available */` |
|   14703 |  4060 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4061 | `	/* Pop operand lvalues */` |
|   14703 |  4062 | `	if( rc == SXERR_ABORT ){` |
|       - |  4063 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4064 | `		return SXERR_ABORT;` |
|   14703 |  4065 | `	}else if( rc != SXERR_EMPTY ){` |
|   14701 |  4066 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7348 |  4067 | `	}` |
|   14703 |  4068 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   14703 |  4079 | `	pGen->pIn++;` |
|       - |  4080 | `	/* Create the loop block */` |
|   14703 |  4081 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   14703 |  4082 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4083 | `		return SXERR_ABORT;` |
|       - |  4084 | `	}` |
|       - |  4085 | `	/* Deffer continue jumps */` |
|   14703 |  4086 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  4087 | `	/* Compile the condition */` |
|   14703 |  4088 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14703 |  4089 | `	if( rc == SXERR_ABORT ){` |
|       - |  4090 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4091 | `		return SXERR_ABORT;` |
|   14703 |  4092 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  4093 | `		/* Emit the false jump */` |
|   14701 |  4094 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4095 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14701 |  4096 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    7348 |  4097 | `	}` |
|   14703 |  4098 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   14699 |  4109 | `	pGen->pIn++;` |
|       - |  4110 | `	/* Save the post condition stream */` |
|   14699 |  4111 | `	pPostStart = pGen->pIn;` |
|       - |  4112 | `	/* Compile the loop body */` |
|   14699 |  4113 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   14699 |  4114 | `	pGen->pEnd = pTmp;` |
|   14699 |  4115 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   14699 |  4116 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4117 | `		return SXERR_ABORT;` |
|       - |  4118 | `	}` |
|       - |  4119 | `	/* Fix post-continue jumps */` |
|   14699 |  4120 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
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
|   14699 |  4136 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4137 | `		pPostStart++;` |
|     ! 0 |  4138 | `	}` |
|   14699 |  4139 | `	if( pPostStart < pEnd ){` |
|       - |  4140 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   14699 |  4141 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   14699 |  4142 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14699 |  4143 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4144 | `			/* Syntax error */` |
|     ! 0 |  4145 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4146 | `			if( rc == SXERR_ABORT ){` |
|       - |  4147 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4148 | `				return SXERR_ABORT;` |
|       - |  4149 | `			}` |
|     ! 0 |  4150 | `			return SXRET_OK;` |
|       - |  4151 | `		}` |
|   14699 |  4152 | `		RE_SWAP_DELIMITER(pGen);` |
|   14699 |  4153 | `		if( rc == SXERR_ABORT ){` |
|       - |  4154 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4155 | `			return SXERR_ABORT;` |
|   14699 |  4156 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4157 | `			/* Pop operand lvalue */` |
|   14699 |  4158 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7347 |  4159 | `		}` |
|    7347 |  4160 | `	}` |
|       - |  4161 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14699 |  4162 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4163 | `	/* Fix all jumps now the destination is resolved */` |
|   14699 |  4164 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4165 | `	/* Release the loop block */` |
|   14699 |  4166 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4167 | `	/* Statement successfully compiled */` |
|   14699 |  4168 | `	return SXRET_OK;` |
|    7354 |  4169 | `}` |
|       - |  4170 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4171 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4172 | ` * are allowed.` |
|       - |  4173 | ` */` |
|    7878 |  4174 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4175 | `{` |
|    7883 |  4176 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    7883 |  4177 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4178 | `		/* Unexpected expression */` |
|     ! 0 |  4179 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4180 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4181 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4182 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4183 | `		}` |
|     ! 0 |  4184 | `	}` |
|    7883 |  4185 | `	return rc;` |
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
|    4042 |  4213 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4214 | `{` |
|    4047 |  4215 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    4047 |  4216 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    4047 |  4217 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4218 | `	ph7_foreach_info *pInfo;` |
|       - |  4219 | `	sxu32 nFalseJump;` |
|       - |  4220 | `	VmInstr *pInstr;` |
|       - |  4221 | `	sxu32 nLine;` |
|       - |  4222 | `	sxi32 rc;` |
|    4047 |  4223 | `	nLine = pGen->pIn->nLine;` |
|       - |  4224 | `	/* Jump the 'foreach' keyword */` |
|    4047 |  4225 | `	pGen->pIn++;` |
|    4047 |  4226 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4227 | `		/* Syntax error */` |
|     ! 0 |  4228 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4229 | `		if( rc == SXERR_ABORT ){` |
|       - |  4230 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4231 | `			return SXERR_ABORT;` |
|       - |  4232 | `		}` |
|     ! 0 |  4233 | `		goto Synchronize;` |
|       - |  4234 | `	}` |
|       - |  4235 | `	/* Jump the left parenthesis '(' */` |
|    4047 |  4236 | `	pGen->pIn++;` |
|       - |  4237 | `	/* Create the loop block */` |
|    4047 |  4238 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    4047 |  4239 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4240 | `		return SXERR_ABORT;` |
|       - |  4241 | `	}` |
|       - |  4242 | `	/* Delimit the expression */` |
|    4047 |  4243 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    4047 |  4244 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|    4047 |  4259 | `	pCur = pGen->pIn;` |
|   27757 |  4260 | `	while( pCur < pEnd ){` |
|   27757 |  4261 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    4061 |  4262 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    4061 |  4263 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4264 | `				/* Break with the first 'as' found */` |
|    4047 |  4265 | `				break;` |
|       - |  4266 | `			}` |
|       7 |  4267 | `		}` |
|       - |  4268 | `		/* Advance the stream cursor */` |
|   23715 |  4269 | `		pCur++;` |
|       5 |  4270 | `	}` |
|    4047 |  4271 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4272 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4273 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4274 | `		if( rc == SXERR_ABORT ){` |
|       - |  4275 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4276 | `			return SXERR_ABORT;` |
|       - |  4277 | `		}` |
|     ! 0 |  4278 | `		goto Synchronize;` |
|       - |  4279 | `	}` |
|       - |  4280 | `	/* Swap token streams */` |
|    4047 |  4281 | `	pTmp = pGen->pEnd;` |
|    4047 |  4282 | `	pGen->pEnd = pCur;` |
|    4047 |  4283 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    4047 |  4284 | `	if( rc == SXERR_ABORT ){` |
|       - |  4285 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4286 | `		return SXERR_ABORT;` |
|       - |  4287 | `	}` |
|       - |  4288 | `	/* Update token stream */` |
|    4047 |  4289 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4290 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4291 | `		if( rc == SXERR_ABORT ){` |
|       - |  4292 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4293 | `			return SXERR_ABORT;` |
|       - |  4294 | `		}` |
|     ! 0 |  4295 | `		pGen->pIn++;` |
|     ! 0 |  4296 | `	}` |
|    4047 |  4297 | `	pCur++; /* Jump the 'as' keyword */` |
|    4047 |  4298 | `	pGen->pIn = pCur;` |
|    4047 |  4299 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4300 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4301 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4302 | `			return SXERR_ABORT;` |
|       - |  4303 | `		}` |
|     ! 0 |  4304 | `	}` |
|       - |  4305 | `	/* Create the foreach context */` |
|    4047 |  4306 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    4047 |  4307 | `	if( pInfo == 0 ){` |
|     ! 0 |  4308 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4309 | `		return SXERR_ABORT;` |
|       - |  4310 | `	}` |
|       - |  4311 | `	/* Zero the structure */` |
|    4047 |  4312 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4313 | `	/* Initialize structure fields */` |
|    4047 |  4314 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4315 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - |  4316 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - |  4317 | `	 * '=>'. */` |
|    4047 |  4318 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    4047 |  4319 | `	if( pCur < pEnd ){` |
|       - |  4320 | `		/* Compile the expression holding the key name */` |
|    3857 |  4321 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4322 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4323 | `			if( rc == SXERR_ABORT ){` |
|       - |  4324 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4325 | `				return SXERR_ABORT;` |
|       - |  4326 | `			}` |
|     ! 0 |  4327 | `		}else{` |
|    3857 |  4328 | `			pGen->pEnd = pCur;` |
|    3857 |  4329 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3857 |  4330 | `			if( rc == SXERR_ABORT ){` |
|       - |  4331 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4332 | `				return SXERR_ABORT;` |
|       - |  4333 | `			}` |
|    3857 |  4334 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3857 |  4335 | `			if( pInstr->p3 ){` |
|       - |  4336 | `				/* Record key name */` |
|    3857 |  4337 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1926 |  4338 | `			}` |
|    3857 |  4339 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4340 | `		}` |
|    3857 |  4341 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1926 |  4342 | `	}` |
|    4047 |  4343 | `	pGen->pEnd = pEnd;` |
|    4047 |  4344 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4345 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4346 | `		if( rc == SXERR_ABORT ){` |
|       - |  4347 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4348 | `			return SXERR_ABORT;` |
|       - |  4349 | `		}` |
|     ! 0 |  4350 | `		goto Synchronize;` |
|       - |  4351 | `	}` |
|    4047 |  4352 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4353 | `		pGen->pIn++;` |
|       - |  4354 | `		/* Pass by reference  */` |
|      11 |  4355 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4356 | `	}` |
|       - |  4357 | `	/* Check if the value target is list() */` |
|    4047 |  4358 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|    4042 |  4399 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
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
|    4031 |  4432 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    4031 |  4433 | `		if( rc == SXERR_ABORT ){` |
|       - |  4434 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4435 | `			return SXERR_ABORT;` |
|       - |  4436 | `		}` |
|    4031 |  4437 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    4031 |  4438 | `		if( pInstr->p3 ){` |
|       - |  4439 | `			/* Record value name */` |
|    4031 |  4440 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    2013 |  4441 | `		}` |
|       - |  4442 | `	}` |
|       - |  4443 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    4045 |  4444 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4445 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    4045 |  4446 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4447 | `	/* Record the first instruction to execute */` |
|    4045 |  4448 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4449 | `	/* Emit the FOREACH_STEP instruction */` |
|    4045 |  4450 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4451 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    4045 |  4452 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4453 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    4045 |  4454 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
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
|    4045 |  4482 | `	pGen->pIn = &pEnd[1];` |
|    4045 |  4483 | `	pGen->pEnd = pTmp;` |
|    4045 |  4484 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    4045 |  4485 | `	if( rc == SXERR_ABORT ){` |
|       - |  4486 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4487 | `		return SXERR_ABORT;` |
|       - |  4488 | `	}` |
|       - |  4489 | `	/* Emit the unconditional jump to the start of the loop */` |
|    4045 |  4490 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4491 | `	/* Fix all jumps now the destination is resolved */` |
|    4045 |  4492 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4493 | `	/* Release the loop block */` |
|    4045 |  4494 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4495 | `	/* Statement successfully compiled */` |
|    4045 |  4496 | `	return SXRET_OK;` |
|       1 |  4497 | `Synchronize:` |
|       - |  4498 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4499 | `	 * compiling this erroneous block.` |
|       - |  4500 | `	 */` |
|       3 |  4501 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4502 | `		pGen->pIn++;` |
|     ! 0 |  4503 | `	}` |
|       3 |  4504 | `	return SXRET_OK;` |
|    2026 |  4505 | `}` |
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
|  152696 |  4538 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4539 | `{` |
|  152701 |  4540 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  152701 |  4541 | `	GenBlock *pCondBlock = 0;` |
|       - |  4542 | `	sxu32 nJumpIdx;` |
|       - |  4543 | `	sxu32 nKeyID;` |
|       - |  4544 | `	sxi32 rc;` |
|       - |  4545 | `	/* Jump the 'if' keyword */` |
|  152701 |  4546 | `	pGen->pIn++;` |
|  152701 |  4547 | `	pToken = pGen->pIn;` |
|       - |  4548 | `	/* Create the conditional block */` |
|  152701 |  4549 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  152701 |  4550 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4551 | `		return SXERR_ABORT;` |
|       - |  4552 | `	}` |
|       - |  4553 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   83693 |  4554 | `	for(;;){` |
|  167391 |  4555 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
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
|  167391 |  4568 | `		pToken++;` |
|       - |  4569 | `		/* Delimit the condition */` |
|  167391 |  4570 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  167391 |  4571 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
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
|  167391 |  4584 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4585 | `		/* Compile the condition */` |
|  167391 |  4586 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4587 | `		/* Update token stream */` |
|  167391 |  4588 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4589 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4590 | `			pGen->pIn++;` |
|     ! 0 |  4591 | `		}` |
|  167391 |  4592 | `		pGen->pIn  = &pEnd[1];` |
|  167391 |  4593 | `		pGen->pEnd = pTmp;` |
|  167391 |  4594 | `		if( rc == SXERR_ABORT ){` |
|       - |  4595 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4596 | `			return SXERR_ABORT;` |
|       - |  4597 | `		}` |
|       - |  4598 | `		/* Emit the false jump */` |
|  167391 |  4599 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4600 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  167391 |  4601 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4602 | `		/* Compile the body */` |
|  167391 |  4603 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  167391 |  4604 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4605 | `			return SXERR_ABORT;` |
|       - |  4606 | `		}` |
|  167391 |  4607 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   46659 |  4608 | `			break;` |
|       - |  4609 | `		}` |
|       - |  4610 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   74083 |  4611 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   74083 |  4612 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   47695 |  4613 | `			break;` |
|       - |  4614 | `		}` |
|       - |  4615 | `		/* Emit the unconditional jump */` |
|   26393 |  4616 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4617 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   26393 |  4618 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   26393 |  4619 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   18991 |  4620 | `			pToken = &pGen->pIn[1];` |
|   18991 |  4621 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    7340 |  4622 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5854 |  4623 | `					break;` |
|       - |  4624 | `			}` |
|    7293 |  4625 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3644 |  4626 | `		}` |
|   14695 |  4627 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4628 | `		/* Synchronize cursors */` |
|   14695 |  4629 | `		pToken = pGen->pIn;` |
|       - |  4630 | `		/* Fix the false jump */` |
|   14695 |  4631 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4632 | `	} /* For(;;) */` |
|       - |  4633 | `	/* Fix the false jump */` |
|  152701 |  4634 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  152701 |  4635 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   59388 |  4636 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4637 | `			/* Compile the else block */` |
|   11703 |  4638 | `			pGen->pIn++;` |
|   11703 |  4639 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   11703 |  4640 | `			if( rc == SXERR_ABORT ){` |
|       - |  4641 |  |
|     ! 0 |  4642 | `				return SXERR_ABORT;` |
|       - |  4643 | `			}` |
|    5849 |  4644 | `	}` |
|  152701 |  4645 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4646 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  152701 |  4647 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4648 | `	/* Release the conditional block */` |
|  152701 |  4649 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4650 | `	/* Statement successfully compiled */` |
|  152701 |  4651 | `	return SXRET_OK;` |
|     ! 0 |  4652 | `Synchronize:` |
|       - |  4653 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4654 | `	 */` |
|     ! 0 |  4655 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4656 | `		pGen->pIn++;` |
|     ! 0 |  4657 | `	}` |
|     ! 0 |  4658 | `	return SXRET_OK;` |
|   76353 |  4659 | `}` |
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
|  241962 |  4753 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4754 | `{` |
|  241967 |  4755 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4756 | `	sxi32 rc;` |
|  241967 |  4757 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  241967 |  4758 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|       - |  4759 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|       - |  4760 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|       - |  4761 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|       - |  4762 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|       - |  4763 | `	 * normally below so token processing stays consistent. */` |
|  623103 |  4764 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  381141 |  4765 | `		pFuncBlock = pFuncBlock->pParent;` |
|       5 |  4766 | `	}` |
|  241962 |  4767 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  241935 |  4768 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|       3 |  4769 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  4770 | `			"A never-returning function must not return");` |
|       3 |  4771 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4772 | `			return SXERR_ABORT;` |
|       - |  4773 | `		}` |
|       1 |  4774 | `	}` |
|       - |  4775 | `	/* Jump the 'return' keyword */` |
|  241967 |  4776 | `	pGen->pIn++;` |
|  241967 |  4777 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4778 | `		/* Compile the expression */` |
|  241937 |  4779 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  241937 |  4780 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4781 | `			return SXERR_ABORT;` |
|  241937 |  4782 | `		}else if(rc != SXERR_EMPTY ){` |
|  241937 |  4783 | `			nRet = 1;` |
|  120966 |  4784 | `		}` |
|  120966 |  4785 | `	}` |
|       - |  4786 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4787 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4788 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4789 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4790 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  241967 |  4791 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  241967 |  4792 | `	return SXRET_OK;` |
|  120986 |  4793 | `}` |
|       - |  4794 | `/*` |
|       - |  4795 | ` * Compile a yield expression.` |
|       - |  4796 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4797 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4798 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4799 | ` */` |
|     212 |  4800 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4801 | `{` |
|       - |  4802 | `	SyToken *pTmp, *pSplit;` |
|     217 |  4803 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     217 |  4804 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4805 | `	sxi32 rc;` |
|     106 |  4806 | `	(void)iCompileFlag;` |
|       - |  4807 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     217 |  4808 | `	pGen->pIn++;` |
|       - |  4809 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4810 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - |  4811 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - |  4812 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - |  4813 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     212 |  4814 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     123 |  4815 | `		&& pGen->pIn->sData.nByte == 4` |
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
|     181 |  4833 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4834 | `		/* Bare yield — no value */` |
|       3 |  4835 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       3 |  4836 | `		return SXRET_OK;` |
|       - |  4837 | `	}` |
|       - |  4838 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     179 |  4839 | `	pSplit = 0;` |
|       - |  4840 | `	{` |
|     179 |  4841 | `		SyToken *pCur = pGen->pIn;` |
|     179 |  4842 | `		sxi32 nNest = 0;` |
|     375 |  4843 | `		while( pCur < pGen->pEnd ){` |
|     215 |  4844 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4845 | `				nNest++;` |
|     215 |  4846 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4847 | `				nNest--;` |
|     215 |  4848 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4849 | `				pSplit = pCur;` |
|      16 |  4850 | `				break;` |
|       - |  4851 | `			}` |
|     201 |  4852 | `			pCur++;` |
|       5 |  4853 | `		}` |
|       - |  4854 | `	}` |
|     179 |  4855 | `	pTmp = pGen->pEnd;` |
|     179 |  4856 | `	if( pSplit ){` |
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
|     165 |  4869 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     165 |  4870 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     165 |  4871 | `		if( rc != SXERR_EMPTY ){` |
|     165 |  4872 | `			iP1 = 1;` |
|      80 |  4873 | `		}` |
|       - |  4874 | `	}` |
|     179 |  4875 | `	pGen->pEnd = pTmp;` |
|     179 |  4876 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     179 |  4877 | `	return SXRET_OK;` |
|     111 |  4878 | `}` |
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
|   14902 |  4906 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  4907 | `{` |
|   14907 |  4908 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4909 | `	sxi32 rc;` |
|       - |  4910 | `	/* Jump the 'echo' keyword */` |
|   14907 |  4911 | `	pGen->pIn++;` |
|       - |  4912 | `	/* Compile arguments one after one */` |
|   14907 |  4913 | `	pTmp = pGen->pEnd;` |
|   33127 |  4914 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   18225 |  4915 | `		if( pGen->pIn < pNext ){` |
|   18225 |  4916 | `			pGen->pEnd = pNext;` |
|   18225 |  4917 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   18225 |  4918 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4919 | `				return SXERR_ABORT;` |
|   18225 |  4920 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4921 | `				/* Emit the consume instruction */` |
|   18201 |  4922 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    9098 |  4923 | `			}` |
|    9110 |  4924 | `		}` |
|       - |  4925 | `		/* Jump trailing commas */` |
|   21543 |  4926 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    3323 |  4927 | `			pNext++;` |
|       5 |  4928 | `		}` |
|   18225 |  4929 | `		pGen->pIn = pNext;` |
|       5 |  4930 | `	}` |
|       - |  4931 | `	/* Restore token stream */` |
|   14907 |  4932 | `	pGen->pEnd = pTmp;` |
|   14907 |  4933 | `	return SXRET_OK;` |
|    7456 |  4934 | `}` |
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
|       8 |  4949 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       3 |  4950 | `{` |
|       - |  4951 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4952 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4953 | `	GenBlock *pBlock;` |
|       - |  4954 | `	SyString *pName;` |
|       - |  4955 | `	char *zDup;` |
|       - |  4956 | `	sxu32 nLine;` |
|       - |  4957 | `	sxi32 rc;` |
|       - |  4958 | `	/* Jump the static keyword */` |
|      11 |  4959 | `	nLine = pGen->pIn->nLine;` |
|      11 |  4960 | `	pGen->pIn++;` |
|       - |  4961 | `	/* Extract the enclosing function if any */` |
|      11 |  4962 | `	pBlock = pGen->pCurrent;` |
|      19 |  4963 | `	while( pBlock ){` |
|      19 |  4964 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|      11 |  4965 | `			break;` |
|       - |  4966 | `		}` |
|       - |  4967 | `		/* Point to the upper block */` |
|      11 |  4968 | `		pBlock = pBlock->pParent;` |
|       3 |  4969 | `	}` |
|      11 |  4970 | `	if( pBlock == 0 ){` |
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
|      11 |  4989 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4990 | `	/* Make sure we are dealing with a valid statement */` |
|      11 |  4991 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       6 |  4992 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4993 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4994 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4995 | `				return SXERR_ABORT;` |
|       - |  4996 | `			}` |
|       3 |  4997 | `			goto Synchronize;` |
|       - |  4998 | `	}` |
|       8 |  4999 | `	pGen->pIn++;` |
|       - |  5000 | `	/* Extract variable name */` |
|       8 |  5001 | `	pName = &pGen->pIn->sData;` |
|       8 |  5002 | `	pGen->pIn++; /* Jump the var name */` |
|       8 |  5003 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  5004 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  5005 | `		goto Synchronize;` |
|       - |  5006 | `	}` |
|       - |  5007 | `	/* Initialize the structure describing the static variable */` |
|       8 |  5008 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       8 |  5009 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  5010 | `	/* Duplicate variable name */` |
|       8 |  5011 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       8 |  5012 | `	if( zDup == 0 ){` |
|     ! 0 |  5013 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  5014 | `		return SXERR_ABORT;` |
|       - |  5015 | `	}` |
|       8 |  5016 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  5017 | `	/* Check if we have an expression to compile */` |
|       8 |  5018 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  5019 | `		SySet *pInstrContainer;` |
|       - |  5020 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  5021 | `		 * Static variable can take any complex expression including function` |
|       - |  5022 | `		 * call as their initialization value.` |
|       - |  5023 | `		 * Example:` |
|       - |  5024 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  5025 | `		 */` |
|       8 |  5026 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  5027 | `		/* Swap bytecode container */` |
|       8 |  5028 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       8 |  5029 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  5030 | `		/* Compile the expression */` |
|       8 |  5031 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5032 | `		/* Emit the done instruction */` |
|       8 |  5033 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  5034 | `		/* Restore default bytecode container */` |
|       8 |  5035 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       3 |  5036 | `	}` |
|       - |  5037 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       8 |  5038 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       8 |  5039 | `	return SXRET_OK;` |
|       1 |  5040 | `Synchronize:` |
|       - |  5041 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  5042 | `	 * statement.` |
|       - |  5043 | `	 */` |
|       5 |  5044 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  5045 | `		pGen->pIn++;` |
|       1 |  5046 | `	}` |
|       3 |  5047 | `	return SXRET_OK;` |
|       7 |  5048 | `}` |
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
|  470062 |  5101 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
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
|  470067 |  5112 | `	if( pFromImport ){` |
|  449843 |  5113 | `		*pFromImport = 0;` |
|  224919 |  5114 | `	}` |
|  470067 |  5115 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  470067 |  5116 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  5117 | `		return nOrigIdx;` |
|       - |  5118 | `	}` |
|  470067 |  5119 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  470067 |  5120 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  5121 | `	/* Skip if already qualified (contains backslash) */` |
|  470067 |  5122 | `	hasNsSep = 0;` |
| 5190915 |  5123 | `	for( k = 0; k < nLit; k++ ){` |
| 4720861 |  5124 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2360429 |  5125 | `	}` |
|  470067 |  5126 | `	if( hasNsSep ){` |
|      10 |  5127 | `		return nOrigIdx;` |
|       - |  5128 | `	}` |
|       - |  5129 | `	/* Check use imports first (works even outside namespaces) */` |
|  470059 |  5130 | `	SyBlobReset(&pGen->sWorker);` |
|  470059 |  5131 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  470059 |  5132 | `	if( pImport ){` |
|      41 |  5133 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  5134 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  5135 | `		if( pFromImport ){` |
|      18 |  5136 | `			*pFromImport = 1;` |
|       8 |  5137 | `		}` |
|      23 |  5138 | `	}else{` |
|  470023 |  5139 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  469933 |  5140 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
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
|  235036 |  5159 | `}` |
|       - |  5160 | `/*` |
|       - |  5161 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  5162 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  5163 | ` */` |
|   99386 |  5164 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5165 | `{` |
|       - |  5166 | `	SyHashEntry *pImport;` |
|       - |  5167 | `	/* Check use imports first */` |
|   99391 |  5168 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   99391 |  5169 | `	if( pImport ){` |
|      15 |  5170 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      15 |  5171 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      15 |  5172 | `		return;` |
|       - |  5173 | `	}` |
|       - |  5174 | `	/* Prepend current namespace if active */` |
|   99379 |  5175 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  5176 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  5177 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  5178 | `	}` |
|   99379 |  5179 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   49698 |  5180 | `}` |
|       - |  5181 | `/*` |
|       - |  5182 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5183 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5184 | ` * The caller must release pOut when done.` |
|       - |  5185 | ` */` |
|  143554 |  5186 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5187 | `{` |
|  143559 |  5188 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      63 |  5189 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      63 |  5190 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5191 | `	}` |
|  143559 |  5192 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  143559 |  5193 | `}` |
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
|       3 |  5231 | `{` |
|      17 |  5232 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      10 |  5233 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      10 |  5234 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      10 |  5235 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      10 |  5236 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      10 |  5237 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5238 | `	return "token";` |
|      10 |  5239 | `}` |
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
|      27 |  5273 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      27 |  5274 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5275 | `			}` |
|      16 |  5276 | `		}else{` |
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
|       5 |  5377 | `			break;` |
|       - |  5378 | `		}` |
|       - |  5379 | `		/* Default alias is the last component of the path */` |
|      71 |  5380 | `		sAlias = pLast->sData;` |
|       - |  5381 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5382 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      46 |  5383 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      19 |  5384 | `			pGen->pIn++; /* Jump 'as' */` |
|      19 |  5385 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      19 |  5386 | `				sAlias = pGen->pIn->sData;` |
|      19 |  5387 | `				pGen->pIn++;` |
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
|      35 |  5616 | `		pCursor = pValTok + 1;` |
|       - |  5617 | `		/* Consume separating comma (or end). */` |
|      35 |  5618 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5619 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5620 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5621 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5622 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5623 | `				return SXRET_OK;` |
|       - |  5624 | `			}` |
|       3 |  5625 | `			pCursor++;` |
|       1 |  5626 | `		}` |
|       5 |  5627 | `	}` |
|       - |  5628 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5629 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5630 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      33 |  5631 | `	return SXRET_OK;` |
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
|   76566 |  5691 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5692 | `{` |
|       - |  5693 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5694 | `	SySet *pInstrContainer;` |
|       - |  5695 | `	sxi32 rc;` |
|       - |  5696 | `	/* Swap token stream */` |
|   76571 |  5697 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   76571 |  5698 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   76571 |  5699 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5700 | `	/* Compile the expression holding the argument value */` |
|   76571 |  5701 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5702 | `	/* Emit the done instruction */` |
|   76571 |  5703 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   76571 |  5704 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   76571 |  5705 | `	RE_SWAP_DELIMITER(pGen);` |
|   76571 |  5706 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5707 | `		return SXERR_ABORT;` |
|       - |  5708 | `	}` |
|   76571 |  5709 | `	return SXRET_OK;` |
|   38288 |  5710 | `}` |
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
|  107074 |  5748 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5749 | `{` |
|       - |  5750 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5751 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5752 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5753 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5754 | `	sxi32 rc;` |
|       - |  5755 |  |
|  107079 |  5756 | `	pIn = pGen->pIn;` |
|  107079 |  5757 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5758 | `	/* Process arguments one after one */` |
|  138418 |  5759 | `	for(;;){` |
|  276841 |  5760 | `		if( pIn >= pEnd ){` |
|       - |  5761 | `			/* No more arguments to process */` |
|  107063 |  5762 | `			break;` |
|       - |  5763 | `		}` |
|  169783 |  5764 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  169783 |  5765 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  169783 |  5766 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  169783 |  5767 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5768 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  5769 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  5770 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  5771 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  5772 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  5773 | `		{` |
|  169783 |  5774 | `			int bReadonly = 0, bVisSeen = 0;` |
|  169783 |  5775 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  169783 |  5776 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  5777 | `				bReadonly = 1;` |
|       3 |  5778 | `				pIn++;` |
|       1 |  5779 | `			}` |
|  169783 |  5780 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   65831 |  5781 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   65831 |  5782 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
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
|   32913 |  5793 | `			}` |
|  169783 |  5794 | `			if( bVisSeen \|\| bReadonly ){` |
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
|  169774 |  5816 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  128796 |  5817 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   85988 |  5818 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   80485 |  5819 | `			sxu32 nLineLocal = pIn->nLine;` |
|   80485 |  5820 | `			sxi32 iTFlags = 0;` |
|   80485 |  5821 | `			pGen->pIn = pIn;` |
|   80485 |  5822 | `			rc = GenStateParseUnionTypeDecl(` |
|   40240 |  5823 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   40240 |  5824 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5825 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5826 | `				/* bAllowVoid */ 0,` |
|   40240 |  5827 | `						nLineLocal);` |
|   80485 |  5828 | `			pIn = pGen->pIn;` |
|   80485 |  5829 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5830 | `				return SXERR_ABORT;` |
|   80485 |  5831 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5832 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5833 | `				return SXERR_SYNTAX;` |
|   80483 |  5834 | `			}else if( rc == SXERR_SYNTAX ){` |
|      11 |  5835 | `				if( pIn < pEnd ){` |
|      15 |  5836 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5837 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       4 |  5838 | `						&pIn->sData);` |
|       7 |  5839 | `				}else{` |
|     ! 0 |  5840 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5841 | `						"syntax error, unexpected end of file");` |
|       - |  5842 | `				}` |
|      11 |  5843 | `				return SXERR_SYNTAX;` |
|       - |  5844 | `			}` |
|   80475 |  5845 | `			sArg.iFlags \|= iTFlags;` |
|   40235 |  5846 | `		}` |
|  169769 |  5847 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5848 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5849 | `			return rc;` |
|       - |  5850 | `		}` |
|  169769 |  5851 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5852 | `			/* Pass by reference,record that */` |
|    3677 |  5853 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3677 |  5854 | `			pIn++;` |
|    1836 |  5855 | `		}` |
|  169769 |  5856 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5857 | `			/* Variadic parameter: ...$args */` |
|    3693 |  5858 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    3693 |  5859 | `			pIn++;` |
|    1844 |  5860 | `		}` |
|  169769 |  5861 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5862 | `			/* Invalid argument */` |
|     ! 0 |  5863 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5864 | `			return rc;` |
|       - |  5865 | `		}` |
|  169769 |  5866 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5867 | `		/* Copy argument name */` |
|  169769 |  5868 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  169769 |  5869 | `		if( zDup == 0 ){` |
|     ! 0 |  5870 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5871 | `			return SXERR_ABORT;` |
|       - |  5872 | `		}` |
|  169769 |  5873 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  169769 |  5874 | `		pIn++;` |
|  169769 |  5875 | `		if( pIn < pEnd ){` |
|  102833 |  5876 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5877 | `				SyToken *pDefend;` |
|   76573 |  5878 | `				sxi32 iNest = 0;` |
|   76573 |  5879 | `				pIn++; /* Jump the equal sign */` |
|   76573 |  5880 | `				pDefend = pIn;` |
|       - |  5881 | `				/* Process the default value associated with this argument */` |
|  160433 |  5882 | `				while( pDefend < pEnd ){` |
|  120315 |  5883 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   36455 |  5884 | `						break;` |
|       - |  5885 | `					}` |
|   83865 |  5886 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5887 | `						/* Increment nesting level */` |
|    3651 |  5888 | `						iNest++;` |
|   82042 |  5889 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5890 | `						/* Decrement nesting level */` |
|    3651 |  5891 | `						iNest--;` |
|    1823 |  5892 | `					}` |
|   83865 |  5893 | `					pDefend++;` |
|       5 |  5894 | `				}` |
|   76573 |  5895 | `				if( pIn >= pDefend ){` |
|       3 |  5896 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5897 | `					return rc;` |
|       - |  5898 | `				}` |
|       - |  5899 | `				/* Process default value */` |
|   76571 |  5900 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   76571 |  5901 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5902 | `					return rc;` |
|       - |  5903 | `				}` |
|       - |  5904 | `				/* Point beyond the default value */` |
|   76571 |  5905 | `				pIn = pDefend;` |
|   38283 |  5906 | `			}` |
|  102831 |  5907 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5908 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5909 | `				return rc;` |
|       - |  5910 | `			}` |
|  102831 |  5911 | `			pIn++; /* Jump the trailing comma */` |
|   51413 |  5912 | `		}` |
|       - |  5913 | `		/* Append argument signature */` |
|  169767 |  5914 | `		if( sArg.nType > 0 ){` |
|   80421 |  5915 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5916 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|   14637 |  5917 | `				int marker = 'o';` |
|   14637 |  5918 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|   14637 |  5919 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    7321 |  5920 | `			}else{` |
|       - |  5921 | `				int c;` |
|   65789 |  5922 | `				c = 'n'; /* cc warning */` |
|       - |  5923 | `				/* Type leading character */` |
|   65789 |  5924 | `				switch(sArg.nType){` |
|       3 |  5925 | `				case MEMOBJ_HASHMAP:` |
|       - |  5926 | `					/* Hashmap aka 'array' */` |
|       7 |  5927 | `					c = 'h';` |
|       7 |  5928 | `					break;` |
|    9166 |  5929 | `				case MEMOBJ_INT:` |
|       - |  5930 | `					/* Integer */` |
|   18337 |  5931 | `					c = 'i';` |
|   18337 |  5932 | `					break;` |
|       2 |  5933 | `				case MEMOBJ_BOOL:` |
|       - |  5934 | `					/* Bool */` |
|       5 |  5935 | `					c = 'b';` |
|       5 |  5936 | `					break;` |
|       2 |  5937 | `				case MEMOBJ_REAL:` |
|       - |  5938 | `					/* Float */` |
|       5 |  5939 | `					c = 'f';` |
|       5 |  5940 | `					break;` |
|   23711 |  5941 | `				case MEMOBJ_STRING:` |
|       - |  5942 | `					/* String */` |
|   47427 |  5943 | `					c = 's';` |
|   47427 |  5944 | `					break;` |
|       7 |  5945 | `				case MEMOBJ_OBJ:` |
|       - |  5946 | `					/* Object */` |
|      16 |  5947 | `					c = 'o';` |
|      14 |  5948 | `					break;` |
|       1 |  5949 | `				default:` |
|       2 |  5950 | `					break;` |
|       - |  5951 | `				}` |
|   65789 |  5952 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5953 | `			}` |
|   40213 |  5954 | `		}else{` |
|       - |  5955 | `			/* No type is associated with this parameter which mean` |
|       - |  5956 | `			 * that this function is not condidate for overloading.` |
|       - |  5957 | `			 */` |
|   89351 |  5958 | `			SyBlobRelease(&sSig);` |
|       - |  5959 | `		}` |
|       - |  5960 | `		/* Save in the argument set */` |
|  169767 |  5961 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  5962 | `	}` |
|  107063 |  5963 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5964 | `		/* Save function signature */` |
|   51235 |  5965 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   25615 |  5966 | `	}` |
|  107063 |  5967 | `	return SXRET_OK;` |
|   53542 |  5968 | `}` |
|       - |  5969 | `/*` |
|       - |  5970 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|       - |  5971 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|       - |  5972 | ` * the enclosing function. Returns the token just past the nested construct.` |
|       - |  5973 | ` */` |
|      14 |  5974 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|       2 |  5975 | `{` |
|      16 |  5976 | `	sxi32 iParen = 0;` |
|      16 |  5977 | `	pIn++; /* past 'function'/'fn' */` |
|       - |  5978 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|       - |  5979 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|       - |  5980 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|      54 |  5981 | `	while( pIn < pEnd ){` |
|      54 |  5982 | `		sxu32 t = pIn->nType;` |
|      54 |  5983 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|      40 |  5984 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|      26 |  5985 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|      12 |  5986 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|      40 |  5987 | `		pIn++;` |
|       2 |  5988 | `	}` |
|      16 |  5989 | `	if( pIn >= pEnd ){ return pIn; }` |
|       - |  5990 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|       - |  5991 | `	{` |
|      16 |  5992 | `		sxi32 d = 0;` |
|     108 |  5993 | `		while( pIn < pEnd ){` |
|     108 |  5994 | `			sxu32 t = pIn->nType;` |
|     108 |  5995 | `			if( t & PH7_TK_OCB ){ d++; }` |
|      94 |  5996 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|      94 |  5997 | `			pIn++;` |
|       2 |  5998 | `		}` |
|       - |  5999 | `	}` |
|      16 |  6000 | `	return pIn;` |
|       9 |  6001 | `}` |
|       - |  6002 | `/*` |
|       - |  6003 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|       - |  6004 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|       - |  6005 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|       - |  6006 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|       - |  6007 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|       - |  6008 | ` * detached-mini-program path untouched.` |
|       - |  6009 | ` */` |
|  228390 |  6010 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|       5 |  6011 | `{` |
|  228395 |  6012 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|  228395 |  6013 | `	SyToken *pEnd = pGen->pEnd;` |
|  228395 |  6014 | `	sxi32 iDepth = 0;` |
|  228395 |  6015 | `	int bStarted = 0;` |
| 7586039 |  6016 | `	while( pIn < pEnd ){` |
| 7586039 |  6017 | `		sxu32 t = pIn->nType;` |
| 7586039 |  6018 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 7149175 |  6019 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 6712501 |  6020 | `		if( t & PH7_TK_KEYWORD ){` |
|  532337 |  6021 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  532337 |  6022 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  532205 |  6023 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|       - |  6024 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|  266093 |  6025 | `		}` |
| 6712355 |  6026 | `		pIn++;` |
|       5 |  6027 | `	}` |
|  228263 |  6028 | `	return FALSE;` |
|  114200 |  6029 | `}` |
|       - |  6030 | `/*` |
|       - |  6031 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  6032 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  6033 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  6034 | ` */` |
|  228390 |  6035 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  6036 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  6037 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  6038 | `	)` |
|       5 |  6039 | `{` |
|       - |  6040 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  6041 | `	GenBlock *pBlock;` |
|       - |  6042 | `	sxu32 nGotoOfft;` |
|       - |  6043 | `	sxi32 rc;` |
|       - |  6044 | `	/* Attach the new function */` |
|  228395 |  6045 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  228395 |  6046 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6047 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  6048 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6049 | `		return SXERR_ABORT;` |
|       - |  6050 | `	}` |
|  228395 |  6051 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  6052 | `	/* Swap bytecode containers */` |
|  228395 |  6053 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  228395 |  6054 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  6055 | `	/* Emit constructor property promotion prologue:` |
|       - |  6056 | `	 *   $this->NAME = $NAME;` |
|       - |  6057 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  6058 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  6059 | `	{` |
|  228395 |  6060 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  6061 | `		sxu32 i;` |
|  368857 |  6062 | `		for( i = 0; i < nArg; i++ ){` |
|  140467 |  6063 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  6064 | `			char *zSrc;` |
|       - |  6065 | `			sxu32 nSrc,nName;` |
|       - |  6066 | `			SySet sToken;` |
|       - |  6067 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  6068 | `			sxi32 rcPromote;` |
|  140467 |  6069 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  140413 |  6070 | `				continue;` |
|       - |  6071 | `			}` |
|       - |  6072 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  6073 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  6074 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  6075 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  6076 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      59 |  6077 | `			nName = SyStringLength(&pArg->sName);` |
|      59 |  6078 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      59 |  6079 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      59 |  6080 | `			if( zSrc == 0 ){` |
|     ! 0 |  6081 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6082 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6083 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  6084 | `				return SXERR_ABORT;` |
|       - |  6085 | `			}` |
|       - |  6086 | `			{` |
|      59 |  6087 | `				char *z = zSrc;` |
|      59 |  6088 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      59 |  6089 | `				z += sizeof("$this->")-1;` |
|      59 |  6090 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6091 | `				z += nName;` |
|      59 |  6092 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      59 |  6093 | `				z += sizeof(" = $")-1;` |
|      59 |  6094 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6095 | `				z += nName;` |
|      59 |  6096 | `				*z = 0;` |
|       - |  6097 | `			}` |
|      59 |  6098 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      59 |  6099 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      59 |  6100 | `			pTmpIn = pGen->pIn;` |
|      59 |  6101 | `			pTmpEnd = pGen->pEnd;` |
|      59 |  6102 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      59 |  6103 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      59 |  6104 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      59 |  6105 | `			pGen->pIn = pTmpIn;` |
|      59 |  6106 | `			pGen->pEnd = pTmpEnd;` |
|      59 |  6107 | `			SySetRelease(&sToken);` |
|      59 |  6108 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  6109 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6110 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6111 | `				return SXERR_ABORT;` |
|       - |  6112 | `			}` |
|       - |  6113 | `			/* Discard the assignment result — this is a statement expression. */` |
|      59 |  6114 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      32 |  6115 | `		}` |
|       - |  6116 | `	}` |
|       - |  6117 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|       - |  6118 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|       - |  6119 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|       - |  6120 | `	 * generator — and vice versa — is classified independently. */` |
|       - |  6121 | `	{` |
|  228395 |  6122 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|  228395 |  6123 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|       - |  6124 | `		/* Compile the body */` |
|  228395 |  6125 | `		PH7_CompileBlock(&(*pGen),0);` |
|  228395 |  6126 | `		pGen->bInGenerator = bSavedGen;` |
|       - |  6127 | `	}` |
|       - |  6128 | `	/* Fix exception jumps now the destination is resolved */` |
|  228395 |  6129 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  6130 | `	/* Emit the final return if not yet done */` |
|  228395 |  6131 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  6132 | `	/* Fix gotos jumps now the destination is resolved */` |
|  228395 |  6133 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  6134 | `		rc = SXERR_ABORT;` |
|     ! 0 |  6135 | `	}` |
|  228395 |  6136 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  6137 | `	/* Restore the default container */` |
|  228395 |  6138 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  6139 | `	/* Leave function block */` |
|  228395 |  6140 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  228395 |  6141 | `	if( rc == SXERR_ABORT ){` |
|       - |  6142 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6143 | `		return SXERR_ABORT;` |
|       - |  6144 | `	}` |
|       - |  6145 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  6146 | `	{` |
|  228395 |  6147 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  6148 | `		sxu32 i;` |
| 4485855 |  6149 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 4257597 |  6150 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     137 |  6151 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     137 |  6152 | `				break;` |
|       - |  6153 | `			}` |
| 2128735 |  6154 | `		}` |
|       - |  6155 | `	}` |
|       - |  6156 | `	/* All done, function body compiled */` |
|  228395 |  6157 | `	return SXRET_OK;` |
|  114200 |  6158 | `}` |
|       - |  6159 | `/*` |
|       - |  6160 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  6161 | ` * According to the PHP language reference manual.` |
|       - |  6162 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  6163 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  6164 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  6165 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  6166 | ` *  Functions need not be defined before they are referenced.` |
|       - |  6167 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  6168 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  6169 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  6170 | ` *  calls with over 32-64 recursion levels.` |
|       - |  6171 | ` *` |
|       - |  6172 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  6173 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  6174 | ` * on these extension.` |
|       - |  6175 | ` */` |
|       - |  6176 | `/*` |
|       - |  6177 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  6178 | ` */` |
|     510 |  6179 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  6180 | `{` |
|       - |  6181 | `	sxu32 i;` |
|    1453 |  6182 | `	for( i = 0; i < n; i++ ){` |
|    1247 |  6183 | `		int a = zA[i], b = zB[i];` |
|    1247 |  6184 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|    1247 |  6185 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|    1247 |  6186 | `		if( a != b ) return a - b;` |
|     474 |  6187 | `	}` |
|     211 |  6188 | `	return 0;` |
|     260 |  6189 | `}` |
|       - |  6190 | `/*` |
|       - |  6191 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  6192 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  6193 | ` * (which are positive bit values stored in sxu32).` |
|       - |  6194 | ` */` |
|       - |  6195 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  6196 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  6197 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  6198 |  |
|       - |  6199 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|       - |  6200 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|       - |  6201 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|       - |  6202 |  |
|       - |  6203 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  6204 | `struct PhlTypeAtom {` |
|       - |  6205 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  6206 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  6207 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  6208 | `	sxu32 nCanon;` |
|       - |  6209 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|       - |  6210 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|       - |  6211 | `};` |
|       - |  6212 |  |
|       - |  6213 | `/*` |
|       - |  6214 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  6215 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  6216 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  6217 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  6218 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  6219 | ` * already be consumed by the caller.` |
|       - |  6220 | ` */` |
|   81346 |  6221 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  6222 | `{` |
|   81351 |  6223 | `	SyToken *pIn = pGen->pIn;` |
|   81351 |  6224 | `	SyZero(pOut, sizeof(*pOut));` |
|   81351 |  6225 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   81351 |  6226 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6227 | `		return SXERR_SYNTAX;` |
|       - |  6228 | `	}` |
|       - |  6229 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   81351 |  6230 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  6231 | `		pIn++;` |
|       8 |  6232 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6233 | `			return SXERR_SYNTAX;` |
|       - |  6234 | `		}` |
|       3 |  6235 | `	}` |
|   81351 |  6236 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6237 | `		return SXERR_SYNTAX;` |
|       - |  6238 | `	}` |
|   81351 |  6239 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   66343 |  6240 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   66343 |  6241 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      32 |  6242 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   66329 |  6243 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      71 |  6244 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   66282 |  6245 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   18597 |  6246 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   56953 |  6247 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   47587 |  6248 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   23866 |  6249 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      33 |  6250 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      61 |  6251 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  6252 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      33 |  6253 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|      10 |  6254 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      19 |  6255 | `			pOut->nType = SXU32_HIGH;` |
|      19 |  6256 | `			pOut->sClass = pIn->sData;` |
|      11 |  6257 | `		}else{` |
|       3 |  6258 | `			return SXERR_SYNTAX;` |
|       - |  6259 | `		}` |
|   66341 |  6260 | `		pIn++;` |
|   33173 |  6261 | `	}else{` |
|       - |  6262 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6263 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   15013 |  6264 | `		SyString *pT = &pIn->sData;` |
|   15013 |  6265 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      32 |  6266 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      32 |  6267 | `			pIn++;` |
|   14999 |  6268 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     157 |  6269 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     157 |  6270 | `			pIn++;` |
|   14909 |  6271 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|      24 |  6272 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|      24 |  6273 | `			pIn++;` |
|      14 |  6274 | `		}else{` |
|       - |  6275 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   14813 |  6276 | `			SyToken *pFirst = pIn;` |
|   14813 |  6277 | `			SyToken *pLast = pIn;` |
|   14813 |  6278 | `			pOut->nType = SXU32_HIGH;` |
|   14813 |  6279 | `			pOut->sClass = pIn->sData;` |
|   14813 |  6280 | `			pIn++;` |
|   22215 |  6281 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   14816 |  6282 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6283 | `				pLast = &pIn[1];` |
|       3 |  6284 | `				pIn += 2;` |
|       1 |  6285 | `			}` |
|   14813 |  6286 | `			if( pLast != pFirst ){` |
|       3 |  6287 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6288 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6289 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6290 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6291 | `			}` |
|       - |  6292 | `		}` |
|       - |  6293 | `	}` |
|   81349 |  6294 | `	pGen->pIn = pIn;` |
|   81349 |  6295 | `	return SXRET_OK;` |
|   40678 |  6296 | `}` |
|       - |  6297 |  |
|       - |  6298 | `/*` |
|       - |  6299 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6300 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6301 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6302 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6303 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6304 | ` */` |
|   81186 |  6305 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6306 | `{` |
|       - |  6307 | `	int i;` |
|   81191 |  6308 | `	int nNonNull = 0;` |
|   81191 |  6309 | `	int bAnyIntersection = 0;` |
|       - |  6310 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   81191 |  6311 | `	sxu32 nMaxGroup = 0;` |
| 2679143 |  6312 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  162511 |  6313 | `	for( i = 0; i < nAtoms; i++ ){` |
|   81325 |  6314 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   81297 |  6315 | `			nNonNull++;` |
|   81297 |  6316 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   81297 |  6317 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   81297 |  6318 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   40646 |  6319 | `			}` |
|   40646 |  6320 | `		}` |
|   40665 |  6321 | `	}` |
|  162477 |  6322 | `	for( i = 0; i < nAtoms; i++ ){` |
|   81307 |  6323 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      20 |  6324 | `			bAnyIntersection = 1;` |
|      20 |  6325 | `			break;` |
|       - |  6326 | `		}` |
|   40648 |  6327 | `	}` |
|   81191 |  6328 | `	if( bAnyIntersection ){` |
|       - |  6329 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|       - |  6330 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|       - |  6331 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|      20 |  6332 | `		sxu32 g, nGroups = 0;` |
|      20 |  6333 | `		int bFirstGroup = 1;` |
|      40 |  6334 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|      40 |  6335 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|      24 |  6336 | `			int bFirstMember = 1;` |
|       - |  6337 | `			int bWrap;` |
|      24 |  6338 | `			if( aGroupCount[g] == 0 ) continue;` |
|       - |  6339 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|       - |  6340 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|       - |  6341 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|       - |  6342 | `			 * parens, matching PHP's canonical text. */` |
|      32 |  6343 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|      24 |  6344 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|      24 |  6345 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      72 |  6346 | `			for( i = 0; i < nAtoms; i++ ){` |
|      52 |  6347 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|      40 |  6348 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|      40 |  6349 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      38 |  6350 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      21 |  6351 | `				}else{` |
|       3 |  6352 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6353 | `				}` |
|      40 |  6354 | `				bFirstMember = 0;` |
|      22 |  6355 | `			}` |
|      24 |  6356 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|      24 |  6357 | `			bFirstGroup = 0;` |
|      14 |  6358 | `		}` |
|      20 |  6359 | `		if( bNullable ){` |
|     ! 0 |  6360 | `			SyBlobAppend(pBlob, "\|", 1);` |
|     ! 0 |  6361 | `			SyBlobAppend(pBlob, "null", 4);` |
|     ! 0 |  6362 | `		}` |
|      58 |  6363 | `		return;` |
|       - |  6364 | `	}` |
|   81175 |  6365 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6366 | `		/* Shorthand: ?T */` |
|      81 |  6367 | `		for( i = 0; i < nAtoms; i++ ){` |
|      81 |  6368 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      81 |  6369 | `			SyBlobAppend(pBlob, "?", 1);` |
|      81 |  6370 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      22 |  6371 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      13 |  6372 | `			}else{` |
|      62 |  6373 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6374 | `			}` |
|      81 |  6375 | `			return;` |
|     ! 0 |  6376 | `		}` |
|     ! 0 |  6377 | `	}` |
|       - |  6378 | `	{` |
|   81099 |  6379 | `		int bFirst = 1;` |
|       - |  6380 | `		/* 1) Classes in declaration order */` |
|  162295 |  6381 | `		for( i = 0; i < nAtoms; i++ ){` |
|   81201 |  6382 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   14777 |  6383 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   14777 |  6384 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   14777 |  6385 | `				bFirst = 0;` |
|    7386 |  6386 | `			}` |
|   40603 |  6387 | `		}` |
|       - |  6388 | `		/* 2) Built-ins in canonical order */` |
|       - |  6389 | `		{` |
|       - |  6390 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6391 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6392 | `			int k;` |
|  567663 |  6393 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  907393 |  6394 | `				for( i = 0; i < nAtoms; i++ ){` |
|  487073 |  6395 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   66249 |  6396 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   66249 |  6397 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   66249 |  6398 | `						bFirst = 0;` |
|   66249 |  6399 | `						break;` |
|       - |  6400 | `					}` |
|  210417 |  6401 | `				}` |
|  243287 |  6402 | `			}` |
|       - |  6403 | `		}` |
|       - |  6404 | `		/* 3) null suffix */` |
|   81099 |  6405 | `		if( bNullable ){` |
|      20 |  6406 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      20 |  6407 | `			SyBlobAppend(pBlob, "null", 4);` |
|       8 |  6408 | `		}` |
|       - |  6409 | `	}` |
|   40598 |  6410 | `}` |
|       - |  6411 |  |
|       - |  6412 | `/*` |
|       - |  6413 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|       - |  6414 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|       - |  6415 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|       - |  6416 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|       - |  6417 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|       - |  6418 | ` * whether it was parenthesized.` |
|       - |  6419 | ` *` |
|       - |  6420 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|       - |  6421 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|       - |  6422 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|       - |  6423 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|       - |  6424 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|       - |  6425 | ` */` |
|   81328 |  6426 | `static sxi32 GenStateParsePart(` |
|       - |  6427 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|       - |  6428 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|       5 |  6429 | `{` |
|       - |  6430 | `	sxi32 rc;` |
|   81333 |  6431 | `	int nMembers = 0;` |
|   81333 |  6432 | `	int bParen = 0;` |
|   81333 |  6433 | `	*pnMembers = 0;` |
|   81333 |  6434 | `	*pbParen = 0;` |
|   81333 |  6435 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       6 |  6436 | `		bParen = 1;` |
|       6 |  6437 | `		pGen->pIn++; /* skip '(' */` |
|       2 |  6438 | `	}` |
|   40664 |  6439 | `	for(;;){` |
|   81351 |  6440 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6441 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6442 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6443 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6444 | `		}` |
|   81351 |  6445 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   81351 |  6446 | `		if( rc != SXRET_OK ){` |
|       3 |  6447 | `			return rc;` |
|       - |  6448 | `		}` |
|   81349 |  6449 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   81349 |  6450 | `		(*pnAtoms)++;` |
|   81349 |  6451 | `		nMembers++;` |
|       - |  6452 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   81349 |  6453 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      24 |  6454 | `			SyToken *pNext = &pGen->pIn[1];` |
|      20 |  6455 | `			if( pNext < pGen->pEnd` |
|      24 |  6456 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      22 |  6457 | `				pGen->pIn++; /* skip '&' */` |
|      22 |  6458 | `				continue;` |
|       - |  6459 | `			}` |
|       1 |  6460 | `		}` |
|   81331 |  6461 | `		break;` |
|     ! 0 |  6462 | `	}` |
|   81331 |  6463 | `	if( bParen ){` |
|       6 |  6464 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6465 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6466 | `				"Malformed DNF type: expecting ')'");` |
|     ! 0 |  6467 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6468 | `		}` |
|       6 |  6469 | `		pGen->pIn++; /* skip ')' */` |
|       6 |  6470 | `		if( nMembers < 2 ){` |
|     ! 0 |  6471 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6472 | `				"Parenthesized type must be an intersection of at least two types");` |
|     ! 0 |  6473 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6474 | `		}` |
|       2 |  6475 | `	}` |
|   81331 |  6476 | `	*pnMembers = nMembers;` |
|   81331 |  6477 | `	*pbParen = bParen;` |
|   81331 |  6478 | `	return SXRET_OK;` |
|   40669 |  6479 | `}` |
|       - |  6480 |  |
|       - |  6481 | `/*` |
|       - |  6482 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6483 | ` *` |
|       - |  6484 | ` * Outputs:` |
|       - |  6485 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6486 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6487 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6488 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6489 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6490 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6491 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6492 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6493 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6494 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6495 | ` *` |
|       - |  6496 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6497 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6498 | ` */` |
|   81202 |  6499 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6500 | `	ph7_gen_state *pGen,` |
|       - |  6501 | `	sxu32 *pnType,` |
|       - |  6502 | `	SyString *pClass,` |
|       - |  6503 | `	SySet *pAlts,` |
|       - |  6504 | `	sxi32 *piTypeFlags,` |
|       - |  6505 | `	SyString *pTypeText,` |
|       - |  6506 | `	int iNullableFlag,` |
|       - |  6507 | `	int iUnionFlag,` |
|       - |  6508 | `	int bAllowVoid,` |
|       - |  6509 | `	sxu32 nLine` |
|       5 |  6510 | `){` |
|       - |  6511 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   81207 |  6512 | `	int nAtoms = 0;` |
|   81207 |  6513 | `	int bShortNullable = 0;` |
|   81207 |  6514 | `	int bExplicitNull = 0;` |
|       - |  6515 | `	sxi32 rc;` |
|   81207 |  6516 | `	*pnType = 0;` |
|   81207 |  6517 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   81207 |  6518 | `	*piTypeFlags = 0;` |
|   81207 |  6519 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6520 |  |
|   81207 |  6521 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6522 | `		return SXRET_OK;` |
|       - |  6523 | `	}` |
|       - |  6524 | ``	/* Optional `?` shorthand prefix */`` |
|   81202 |  6525 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      71 |  6526 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      71 |  6527 | `		bShortNullable = 1;` |
|      71 |  6528 | `		pGen->pIn++;` |
|      71 |  6529 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6530 | `			return SXERR_SYNTAX;` |
|       - |  6531 | `		}` |
|      33 |  6532 | `	}` |
|       - |  6533 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|       - |  6534 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|       - |  6535 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|       - |  6536 | `	{` |
|       - |  6537 | `		int nMembers, bParen;` |
|   81207 |  6538 | `		sxu32 iGroup = 0;` |
|   81207 |  6539 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   81207 |  6540 | `		if( rc != SXRET_OK ){` |
|       4 |  6541 | `			return rc;` |
|       - |  6542 | `		}` |
|       - |  6543 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|       - |  6544 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|       - |  6545 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|       - |  6546 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|       - |  6547 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|  121991 |  6548 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   81396 |  6549 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     133 |  6550 | `			if( bShortNullable ){` |
|       - |  6551 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6552 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6553 | `				 * already reported" so callers skip their own error emission. */` |
|       3 |  6554 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6555 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6556 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6557 | `			}` |
|     131 |  6558 | `			if( nMembers >= 2 && !bParen ){` |
|     ! 0 |  6559 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|       - |  6560 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6561 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6562 | `			}` |
|     131 |  6563 | ``			pGen->pIn++; /* skip `\|` */`` |
|     131 |  6564 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|     131 |  6565 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  6566 | `				return rc;` |
|       - |  6567 | `			}` |
|       5 |  6568 | `		}` |
|   81203 |  6569 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|     ! 0 |  6570 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6571 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6572 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6573 | `		}` |
|       - |  6574 | `	}` |
|       - |  6575 | `	/* Validation pass.` |
|       - |  6576 | `	 *` |
|       - |  6577 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6578 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6579 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6580 | `	 */` |
|       - |  6581 | `	{` |
|       - |  6582 | `		int i, j;` |
|   81203 |  6583 | `		int bHasNonNull = 0;` |
|   81203 |  6584 | `		int bAnyIntersection = 0;` |
|       - |  6585 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|       - |  6586 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|       - |  6587 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 2679539 |  6588 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  162545 |  6589 | `		for( i = 0; i < nAtoms; i++ ){` |
|   81347 |  6590 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   40676 |  6591 | `		}` |
|  162507 |  6592 | `		for( i = 0; i < nAtoms; i++ ){` |
|   81327 |  6593 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   40657 |  6594 | `		}` |
|       - |  6595 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|       - |  6596 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   81203 |  6597 | `		if( bShortNullable && bAnyIntersection ){` |
|     ! 0 |  6598 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6599 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|     ! 0 |  6600 | `			return SXERR_SYNTAX;` |
|       - |  6601 | `		}` |
|  162531 |  6602 | `		for( i = 0; i < nAtoms; i++ ){` |
|       - |  6603 | `			/* Intersection members must be class/interface types (PHP rejects` |
|       - |  6604 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|       - |  6605 | ``			 * `true`/`false` in an intersection). */`` |
|   81345 |  6606 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      38 |  6607 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|      38 |  6608 | `				if( bClassLike ){` |
|      36 |  6609 | `					SyString *pC = &aAtoms[i].sClass;` |
|      32 |  6610 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|      32 |  6611 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|      32 |  6612 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|      36 |  6613 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|     ! 0 |  6614 | `						bClassLike = 0;` |
|     ! 0 |  6615 | `					}` |
|      16 |  6616 | `				}` |
|      38 |  6617 | `				if( !bClassLike ){` |
|       - |  6618 | `					const char *zName; sxu32 nName;` |
|       3 |  6619 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6620 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6621 | `					}else{` |
|       3 |  6622 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|       - |  6623 | `					}` |
|       4 |  6624 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6625 | `						"Type %.*s cannot be part of an intersection type",` |
|       1 |  6626 | `						(int)nName, zName);` |
|       3 |  6627 | `					return SXERR_SYNTAX;` |
|       - |  6628 | `				}` |
|      16 |  6629 | `			}` |
|   81343 |  6630 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     157 |  6631 | `				if( nAtoms > 1 ){` |
|       3 |  6632 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6633 | `						"Void can only be used as a standalone type");` |
|       3 |  6634 | `					return SXERR_SYNTAX;` |
|       - |  6635 | `				}` |
|     155 |  6636 | `				if( !bAllowVoid ){` |
|     ! 0 |  6637 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6638 | `						"void cannot be used here");` |
|     ! 0 |  6639 | `					return SXERR_SYNTAX;` |
|       - |  6640 | `				}` |
|     155 |  6641 | `				if( bShortNullable ){` |
|     ! 0 |  6642 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6643 | `						"Void type cannot be nullable");` |
|     ! 0 |  6644 | `					return SXERR_SYNTAX;` |
|       - |  6645 | `				}` |
|      75 |  6646 | `			}` |
|   81341 |  6647 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6648 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|       - |  6649 | `				 * type (never = the function does not return). Mirrors the void` |
|       - |  6650 | `				 * validation above; accepted here and enforced at compile time` |
|       - |  6651 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|      24 |  6652 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|       - |  6653 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|       - |  6654 | `					 * same as any other non-standalone use. */` |
|       5 |  6655 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6656 | `						"never can only be used as a standalone type");` |
|       5 |  6657 | `					return SXERR_SYNTAX;` |
|       - |  6658 | `				}` |
|      19 |  6659 | `				if( !bAllowVoid ){` |
|       - |  6660 | `					/* Return-only: params call with bAllowVoid=0. */` |
|       3 |  6661 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6662 | `						"never cannot be used as a parameter type");` |
|       3 |  6663 | `					return SXERR_SYNTAX;` |
|       - |  6664 | `				}` |
|       7 |  6665 | `			}` |
|   81335 |  6666 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      32 |  6667 | `				bExplicitNull = 1;` |
|      18 |  6668 | `			}else{` |
|   81307 |  6669 | `				bHasNonNull = 1;` |
|       - |  6670 | `			}` |
|       - |  6671 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|       - |  6672 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|       - |  6673 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|       - |  6674 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|       - |  6675 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   81515 |  6676 | `			for( j = 0; j < i; j++ ){` |
|     187 |  6677 | `				int bDup = 0;` |
|     187 |  6678 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|     359 |  6679 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|     182 |  6680 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|     187 |  6681 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|     179 |  6682 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      41 |  6683 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      34 |  6684 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      38 |  6685 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|      16 |  6686 | `								aAtoms[j].sClass.zString,` |
|      32 |  6687 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6688 | `							bDup = 1;` |
|     ! 0 |  6689 | `						}` |
|      22 |  6690 | `					}else{` |
|       3 |  6691 | `						bDup = 1;` |
|       - |  6692 | `					}` |
|      18 |  6693 | `				}` |
|     179 |  6694 | `				if( bDup ){` |
|       - |  6695 | `					const char *zName;` |
|       - |  6696 | `					sxu32 nName;` |
|       3 |  6697 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6698 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6699 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6700 | `					}else{` |
|       3 |  6701 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6702 | `						nName = aAtoms[i].nCanon;` |
|       - |  6703 | `					}` |
|       4 |  6704 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6705 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6706 | `					return SXERR_SYNTAX;` |
|       - |  6707 | `				}` |
|      91 |  6708 | `			}` |
|   40669 |  6709 | `		}` |
|   81191 |  6710 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6711 | `			if( bShortNullable ){` |
|       - |  6712 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6713 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6714 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6715 | `				return SXERR_SYNTAX;` |
|       - |  6716 | `			}` |
|       - |  6717 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6718 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6719 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6720 | `			 * atom, so set it here. */` |
|       7 |  6721 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6722 | `		}` |
|       - |  6723 | `	}` |
|       - |  6724 | `	/* Compute nullability flag */` |
|   81191 |  6725 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      97 |  6726 | `		*piTypeFlags \|= iNullableFlag;` |
|      46 |  6727 | `	}` |
|       - |  6728 | `	/* Build canonical type text */` |
|   81191 |  6729 | `	if( pTypeText ){` |
|       - |  6730 | `		SyBlob sBlob;` |
|   81191 |  6731 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  121752 |  6732 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   40593 |  6733 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   81191 |  6734 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  121538 |  6735 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   81022 |  6736 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   81027 |  6737 | `			if( zDup ){` |
|   81027 |  6738 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   40511 |  6739 | `			}` |
|   40511 |  6740 | `		}` |
|   81191 |  6741 | `		SyBlobRelease(&sBlob);` |
|   40593 |  6742 | `	}` |
|       - |  6743 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6744 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6745 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6746 | `	{` |
|   81191 |  6747 | `		int nNonNull = 0;` |
|   81191 |  6748 | `		int iNonNullIdx = -1;` |
|       - |  6749 | `		int i;` |
|  162511 |  6750 | `		for( i = 0; i < nAtoms; i++ ){` |
|   81325 |  6751 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   81297 |  6752 | `				nNonNull++;` |
|   81297 |  6753 | `				iNonNullIdx = i;` |
|   40646 |  6754 | `			}` |
|   40665 |  6755 | `		}` |
|   81191 |  6756 | `		if( nNonNull <= 1 ){` |
|       - |  6757 | `			/* Fast path: store as single type. */` |
|   81099 |  6758 | `			if( iNonNullIdx >= 0 ){` |
|   81093 |  6759 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   81093 |  6760 | `				if( pA->nType == SXU32_HIGH ){` |
|   22130 |  6761 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    7375 |  6762 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   14755 |  6763 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   14755 |  6764 | `					*pnType = SXU32_HIGH;` |
|   14755 |  6765 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   73718 |  6766 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     155 |  6767 | `					*pnType = MEMOBJ_VOID;` |
|   66268 |  6768 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|      16 |  6769 | `					*pnType = MEMOBJ_NEVER;` |
|       9 |  6770 | `				}else{` |
|   66179 |  6771 | `					*pnType = pA->nType;` |
|       - |  6772 | `				}` |
|   40544 |  6773 | `			}` |
|   40552 |  6774 | `		}else{` |
|       - |  6775 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      97 |  6776 | `			*piTypeFlags \|= iUnionFlag;` |
|     311 |  6777 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6778 | `				ph7_type_alt sAlt;` |
|     219 |  6779 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     209 |  6780 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     209 |  6781 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|     209 |  6782 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|     116 |  6783 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      37 |  6784 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      79 |  6785 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      79 |  6786 | `					sAlt.nType = SXU32_HIGH;` |
|      79 |  6787 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      42 |  6788 | `				}else{` |
|     135 |  6789 | `					sAlt.nType = aAtoms[i].nType;` |
|     135 |  6790 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6791 | `				}` |
|     209 |  6792 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|     107 |  6793 | `			}` |
|       - |  6794 | `		}` |
|       - |  6795 | `	}` |
|   81191 |  6796 | `	return SXRET_OK;` |
|   40606 |  6797 | `}` |
|       - |  6798 |  |
|       - |  6799 | `/*` |
|       - |  6800 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6801 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6802 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6803 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6804 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6805 | `` *          and union types `: T\|U`.`` |
|       - |  6806 | ` */` |
|  323378 |  6807 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  6808 | `{` |
|  323383 |  6809 | `	sxi32 iFlags = 0;` |
|       - |  6810 | `	sxi32 rc;` |
|       - |  6811 | `	sxu32 nLine;` |
|  323383 |  6812 | `	pFunc->nReturnType = 0;` |
|  323383 |  6813 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  323383 |  6814 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  323383 |  6815 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  322883 |  6816 | `		return SXRET_OK;` |
|       - |  6817 | `	}` |
|     505 |  6818 | `	pGen->pIn++; /* Skip ':' */` |
|     505 |  6819 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6820 | `		return SXRET_OK;` |
|       - |  6821 | `	}` |
|     505 |  6822 | `	nLine = pGen->pIn->nLine;` |
|     505 |  6823 | `	rc = GenStateParseUnionTypeDecl(` |
|     250 |  6824 | `		pGen,` |
|     250 |  6825 | `		&pFunc->nReturnType,` |
|     250 |  6826 | `		&pFunc->sReturnClass,` |
|     250 |  6827 | `		&pFunc->aReturnUnion,` |
|       - |  6828 | `		&iFlags,` |
|     250 |  6829 | `		&pFunc->sReturnTypeName,` |
|       - |  6830 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|       - |  6831 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|       - |  6832 | `		/* iUnionFlag */ 0,` |
|       - |  6833 | `		/* bAllowVoid */ 1,` |
|     250 |  6834 | `		nLine);` |
|     505 |  6835 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6836 | `		return SXERR_ABORT;` |
|       - |  6837 | `	}` |
|     505 |  6838 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6839 | `		/* Error already reported */` |
|     ! 0 |  6840 | `		return SXERR_SYNTAX;` |
|       - |  6841 | `	}` |
|     505 |  6842 | `	if( rc == SXERR_SYNTAX ){` |
|       8 |  6843 | `		if( pGen->pIn < pGen->pEnd ){` |
|      11 |  6844 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6845 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       6 |  6846 | `				&pGen->pIn->sData);` |
|       5 |  6847 | `		}else{` |
|     ! 0 |  6848 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6849 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6850 | `		}` |
|       8 |  6851 | `		return SXERR_SYNTAX;` |
|       - |  6852 | `	}` |
|     499 |  6853 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     499 |  6854 | `	return SXRET_OK;` |
|  161694 |  6855 | `}` |
|       - |  6856 |  |
|   48774 |  6857 | `static sxi32 GenStateCompileFunc(` |
|       - |  6858 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6859 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6860 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6861 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6862 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6863 | `	)` |
|       5 |  6864 | `{` |
|       - |  6865 | `	ph7_vm_func *pFunc;` |
|       - |  6866 | `	SyToken *pEnd;` |
|       - |  6867 | `	sxu32 nLine;` |
|       - |  6868 | `	char *zName;` |
|       - |  6869 | `	sxi32 rc;` |
|       - |  6870 | `	/* Extract line number */` |
|   48779 |  6871 | `	nLine = pGen->pIn->nLine;` |
|       - |  6872 | `	/* Jump the left parenthesis '(' */` |
|   48779 |  6873 | `	pGen->pIn++;` |
|       - |  6874 | `	/* Delimit the function signature */` |
|   48779 |  6875 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   48779 |  6876 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6877 | `		/* Syntax error */` |
|       9 |  6878 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       9 |  6879 | `		if( rc == SXERR_ABORT ){` |
|       - |  6880 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6881 | `			return SXERR_ABORT;` |
|       - |  6882 | `		}` |
|       9 |  6883 | `		pGen->pIn = pGen->pEnd;` |
|       9 |  6884 | `		return SXRET_OK;` |
|       - |  6885 | `	}` |
|       - |  6886 | `	/* Create the function state */` |
|   48773 |  6887 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   48773 |  6888 | `	if( pFunc == 0 ){` |
|     ! 0 |  6889 | `		goto OutOfMem;` |
|       - |  6890 | `	}` |
|       - |  6891 | `	/* Build the function name, prepending namespace if active */` |
|   48780 |  6892 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6893 | `		SyBlob sFQN;` |
|       - |  6894 | `		sxu32 nLen;` |
|      16 |  6895 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6896 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6897 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6898 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6899 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6900 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6901 | `		SyBlobRelease(&sFQN);` |
|      16 |  6902 | `		if( zName == 0 ){` |
|     ! 0 |  6903 | `			goto OutOfMem;` |
|       - |  6904 | `		}` |
|      16 |  6905 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6906 | `	}else{` |
|   48759 |  6907 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   48759 |  6908 | `		if( zName == 0 ){` |
|     ! 0 |  6909 | `			goto OutOfMem;` |
|       - |  6910 | `		}` |
|   48759 |  6911 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6912 | `	}` |
|   48773 |  6913 | `	if( pGen->pIn < pEnd ){` |
|       - |  6914 | `		/* Collect function arguments */` |
|   33629 |  6915 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   33629 |  6916 | `		if( rc == SXERR_ABORT ){` |
|       - |  6917 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6918 | `			return SXERR_ABORT;` |
|       - |  6919 | `		}` |
|   16812 |  6920 | `	}` |
|       - |  6921 | `	/* Point past ')' and parse optional return type ': type' */` |
|   48773 |  6922 | `	pGen->pIn = &pEnd[1];` |
|       - |  6923 | `	{` |
|   48773 |  6924 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   48773 |  6925 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6926 | `			return SXERR_ABORT;` |
|   48773 |  6927 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       8 |  6928 | `			return SXERR_SYNTAX;` |
|       - |  6929 | `		}` |
|       - |  6930 | `	}` |
|   48767 |  6931 | `	if( bHandleClosure ){` |
|       - |  6932 | `		ph7_vm_func_closure_env sEnv;` |
|     299 |  6933 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     294 |  6934 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     161 |  6935 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      23 |  6936 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6937 | `				/* Closure,record environment variable */` |
|      23 |  6938 | `				pGen->pIn++;` |
|      23 |  6939 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6940 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6941 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6942 | `						return SXERR_ABORT;` |
|       - |  6943 | `					}` |
|     ! 0 |  6944 | `				}` |
|      23 |  6945 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6946 | `				/* Compile until we hit the first closing parenthesis */` |
|      45 |  6947 | `				while( pGen->pIn < pGen->pEnd ){` |
|      45 |  6948 | `					int iFlagsLocal = 0;` |
|      45 |  6949 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      23 |  6950 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      23 |  6951 | `						break;` |
|       - |  6952 | `					}` |
|      27 |  6953 | `					nLineLocal = pGen->pIn->nLine;` |
|      27 |  6954 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6955 | `						/* Pass by reference,record that */` |
|     ! 0 |  6956 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6957 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6958 | `							);` |
|     ! 0 |  6959 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6960 | `						pGen->pIn++;` |
|     ! 0 |  6961 | `					}` |
|      22 |  6962 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      27 |  6963 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6964 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6965 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6966 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6967 | `								return SXERR_ABORT;` |
|       - |  6968 | `							}` |
|       - |  6969 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6970 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6971 | `								pGen->pIn++;` |
|     ! 0 |  6972 | `							}` |
|     ! 0 |  6973 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6974 | `								pGen->pIn++;` |
|     ! 0 |  6975 | `							}` |
|     ! 0 |  6976 | `							break;` |
|       - |  6977 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6978 | `					}else{` |
|       - |  6979 | `						SyString *pNameLocal;` |
|       - |  6980 | `						char *zDup;` |
|       - |  6981 | `						/* Duplicate variable name */` |
|      27 |  6982 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      27 |  6983 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      27 |  6984 | `						if( zDup ){` |
|       - |  6985 | `							/* Zero the structure */` |
|      27 |  6986 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      27 |  6987 | `							sEnv.iFlags = iFlagsLocal;` |
|      27 |  6988 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      27 |  6989 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      27 |  6990 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6991 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6992 | `									got_this = 1;` |
|     ! 0 |  6993 | `							}` |
|       - |  6994 | `							/* Save imported variable */` |
|      27 |  6995 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      16 |  6996 | `						}else{` |
|     ! 0 |  6997 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6998 | `							 return SXERR_ABORT;` |
|       - |  6999 | `						}` |
|       - |  7000 | `					}` |
|      27 |  7001 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      33 |  7002 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7003 | `						/* Ignore trailing commas */` |
|       7 |  7004 | `						pGen->pIn++;` |
|       1 |  7005 | `					}` |
|       5 |  7006 | `				}` |
|      23 |  7007 | `				if( !got_this ){` |
|       - |  7008 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  7009 | `					 * available to the closure environment.` |
|       - |  7010 | `					 */` |
|      23 |  7011 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      23 |  7012 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      23 |  7013 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      23 |  7014 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      23 |  7015 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       9 |  7016 | `				}` |
|      23 |  7017 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  7018 | `					/* Mark as closure */` |
|      23 |  7019 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       9 |  7020 | `				}` |
|       9 |  7021 | `		}` |
|     147 |  7022 | `	}` |
|       - |  7023 | `	/* Compile the body */` |
|   48767 |  7024 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   48767 |  7025 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7026 | `		return SXERR_ABORT;` |
|       - |  7027 | `	}` |
|   48767 |  7028 | `	if( ppFunc ){` |
|     299 |  7029 | `		*ppFunc = pFunc;` |
|     147 |  7030 | `	}` |
|   48767 |  7031 | `	rc = SXRET_OK;` |
|   48767 |  7032 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  7033 | `		/* Finally register the function */` |
|   48749 |  7034 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   24372 |  7035 | `	}` |
|   48767 |  7036 | `	if( rc == SXRET_OK ){` |
|   48767 |  7037 | `		return SXRET_OK;` |
|       - |  7038 | `	}` |
|       - |  7039 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  7040 | `OutOfMem:` |
|       - |  7041 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  7042 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  7043 | `	 */` |
|     ! 0 |  7044 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  7045 | `	return SXERR_ABORT;` |
|   24392 |  7046 | `}` |
|       - |  7047 | `/*` |
|       - |  7048 | ` * Compile a standard PHP function.` |
|       - |  7049 | ` *  Refer to the block-comment above for more information.` |
|       - |  7050 | ` */` |
|   48488 |  7051 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  7052 | `{` |
|       - |  7053 | `	SyString *pName;` |
|       - |  7054 | `	sxi32 iFlags;` |
|       - |  7055 | `	sxu32 nLine;` |
|       - |  7056 | `	sxi32 rc;` |
|       - |  7057 |  |
|   48493 |  7058 | `	nLine = pGen->pIn->nLine;` |
|   48493 |  7059 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   48493 |  7060 | `	iFlags = 0;` |
|   48493 |  7061 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7062 | `		/* Return by reference,remember that */` |
|       7 |  7063 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7064 | `		/* Jump the '&' token */` |
|       7 |  7065 | `		pGen->pIn++;` |
|       3 |  7066 | `	}` |
|   48493 |  7067 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7068 | `		/* Invalid function name */` |
|       8 |  7069 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       8 |  7070 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7071 | `			return SXERR_ABORT;` |
|       - |  7072 | `		}` |
|       - |  7073 | `		/* Sychronize with the next semi-colon or braces*/` |
|      22 |  7074 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      16 |  7075 | `			pGen->pIn++;` |
|       2 |  7076 | `		}` |
|       8 |  7077 | `		return SXRET_OK;` |
|       - |  7078 | `	}` |
|   48487 |  7079 | `	pName = &pGen->pIn->sData;` |
|   48487 |  7080 | `	nLine = pGen->pIn->nLine;` |
|       - |  7081 | `	/* Jump the function name */` |
|   48487 |  7082 | `	pGen->pIn++;` |
|   48487 |  7083 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7084 | `		/* Syntax error */` |
|       3 |  7085 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  7086 | `		if( rc == SXERR_ABORT ){` |
|       - |  7087 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7088 | `			return SXERR_ABORT;` |
|       - |  7089 | `		}` |
|       - |  7090 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  7091 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  7092 | `			pGen->pIn++;` |
|     ! 0 |  7093 | `		}` |
|       3 |  7094 | `		return SXRET_OK;` |
|       - |  7095 | `	}` |
|       - |  7096 | `	/* Compile function body */` |
|   48485 |  7097 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   48485 |  7098 | `	return rc;` |
|   24249 |  7099 | `}` |
|       - |  7100 | `/*` |
|       - |  7101 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  7102 | ` * According to the PHP language reference manual` |
|       - |  7103 | ` *  Visibility:` |
|       - |  7104 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  7105 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  7106 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  7107 | ` *  Members declared protected can be accessed only within the class` |
|       - |  7108 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  7109 | ` *  may only be accessed by the class that defines the member.` |
|       - |  7110 | ` */` |
|  351756 |  7111 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  7112 | `{` |
|  351761 |  7113 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   21979 |  7114 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  329787 |  7115 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   47431 |  7116 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  7117 | `	}` |
|       - |  7118 | `	/* Assume public by default */` |
|  282361 |  7119 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  175883 |  7120 | `}` |
|       - |  7121 | `/*` |
|       - |  7122 | ` * Compile a class constant.` |
|       - |  7123 | ` * According to the PHP language reference manual` |
|       - |  7124 | ` *  Class Constants` |
|       - |  7125 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  7126 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  7127 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  7128 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  7129 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  7130 | ` *   It's also possible for interfaces to have constants.` |
|       - |  7131 | ` * Symisc eXtension.` |
|       - |  7132 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  7133 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7134 | ` *  Example:` |
|       - |  7135 | ` *   class Test{` |
|       - |  7136 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7137 | ` *   };` |
|       - |  7138 | ` *   var_dump(TEST::MyConst);` |
|       - |  7139 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7140 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7141 | ` */` |
|       - |  7142 | `/*` |
|       - |  7143 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  7144 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  7145 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  7146 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  7147 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  7148 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  7149 | ` */` |
|      92 |  7150 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  7151 | `{` |
|       - |  7152 | `	SyToken *p0, *p1;` |
|      97 |  7153 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7154 | `		return 0;` |
|       - |  7155 | `	}` |
|      97 |  7156 | `	p0 = pGen->pIn;` |
|       - |  7157 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|      97 |  7158 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  7159 | `		return 1;` |
|       - |  7160 | `	}` |
|      97 |  7161 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  7162 | `		return 1;` |
|       - |  7163 | `	}` |
|       - |  7164 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  7165 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  7166 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|      93 |  7167 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      93 |  7168 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      93 |  7169 | `		if( p1 ){` |
|      93 |  7170 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      34 |  7171 | `				return 1;` |
|       - |  7172 | `			}` |
|      62 |  7173 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  7174 | `				return 1;` |
|       - |  7175 | `			}` |
|      27 |  7176 | `		}` |
|      27 |  7177 | `	}` |
|      58 |  7178 | `	return 0;` |
|      51 |  7179 | `}` |
|       - |  7180 | `/*` |
|       - |  7181 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|       - |  7182 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|       - |  7183 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|       - |  7184 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|       - |  7185 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|       - |  7186 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|       - |  7187 | ` * Peek only; never consumes tokens.` |
|       - |  7188 | ` */` |
|      24 |  7189 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|       4 |  7190 | `{` |
|      28 |  7191 | `	SyToken *p = pGen->pIn;` |
|      39 |  7192 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      20 |  7193 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|       3 |  7194 | `		p++; /* skip leading unary sign(s) */` |
|       1 |  7195 | `	}` |
|      28 |  7196 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|      23 |  7197 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|       - |  7198 | `	}` |
|       6 |  7199 | `	p++;` |
|       - |  7200 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|       6 |  7201 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|      16 |  7202 | `}` |
|       - |  7203 | `/*` |
|       - |  7204 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|       - |  7205 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|       - |  7206 | `` * `$o->new`), not a `new` expression.`` |
|       - |  7207 | ` */` |
|       6 |  7208 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|       3 |  7209 | `{` |
|       - |  7210 | `	sxi32 iOp;` |
|       9 |  7211 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|     ! 0 |  7212 | `		return 0;` |
|       - |  7213 | `	}` |
|       9 |  7214 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       9 |  7215 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|       6 |  7216 | `}` |
|       - |  7217 | `/*` |
|       - |  7218 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|       - |  7219 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|       - |  7220 | ` * interface-constant and (instance/static) property-default initializers` |
|       - |  7221 | ` * ("New expressions are not supported in this context") while still allowing it` |
|       - |  7222 | ` * in global constants, parameter defaults and static-local initializers (which` |
|       - |  7223 | ` * are compiled by different functions and left untouched). The scan is` |
|       - |  7224 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|       - |  7225 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|       - |  7226 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|       - |  7227 | ` *` |
|       - |  7228 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|       - |  7229 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|       - |  7230 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|       - |  7231 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|       - |  7232 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|       - |  7233 | ` */` |
|   22452 |  7234 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|       5 |  7235 | `{` |
|   22457 |  7236 | `	SyToken *p = pGen->pIn;` |
|   22457 |  7237 | `	int iDepth = 0;` |
|   67571 |  7238 | `	while( p < pGen->pEnd ){` |
|   67571 |  7239 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   22449 |  7240 | `			break; /* end of this initializer */` |
|       - |  7241 | `		}` |
|   45122 |  7242 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   22571 |  7243 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      10 |  7244 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|       - |  7245 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|       - |  7246 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|       - |  7247 | `			 * expression. */` |
|       3 |  7248 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|       3 |  7249 | `			p++;` |
|       3 |  7250 | `			if( bArrow ){` |
|       - |  7251 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|       - |  7252 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|       3 |  7253 | `				int iBase = iDepth;` |
|      17 |  7254 | `				while( p < pGen->pEnd ){` |
|      17 |  7255 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       5 |  7256 | `						iDepth++;` |
|      15 |  7257 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       5 |  7258 | `						if( iDepth <= iBase ){` |
|     ! 0 |  7259 | `							break; /* closes an enclosing group, not the fn's own */` |
|       - |  7260 | `						}` |
|       5 |  7261 | `						iDepth--;` |
|      11 |  7262 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|       3 |  7263 | `						break;` |
|       - |  7264 | `					}` |
|      15 |  7265 | `					p++;` |
|       1 |  7266 | `				}` |
|       2 |  7267 | `			}else{` |
|       - |  7268 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|       - |  7269 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|       - |  7270 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|       - |  7271 | `				 * then skip the balanced brace block. */` |
|     ! 0 |  7272 | `				int iLocal = 0;` |
|     ! 0 |  7273 | `				while( p < pGen->pEnd ){` |
|     ! 0 |  7274 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|     ! 0 |  7275 | `						break; /* body brace */` |
|       - |  7276 | `					}` |
|     ! 0 |  7277 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  7278 | `						iLocal++;` |
|     ! 0 |  7279 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  7280 | `						if( iLocal > 0 ){` |
|     ! 0 |  7281 | `							iLocal--;` |
|     ! 0 |  7282 | `						}` |
|     ! 0 |  7283 | `					}` |
|     ! 0 |  7284 | `					p++;` |
|     ! 0 |  7285 | `				}` |
|     ! 0 |  7286 | `				if( p < pGen->pEnd ){` |
|     ! 0 |  7287 | `					int iBrace = 0; /* p is on the body '{' */` |
|     ! 0 |  7288 | `					while( p < pGen->pEnd ){` |
|     ! 0 |  7289 | `						if( p->nType & PH7_TK_OCB ){` |
|     ! 0 |  7290 | `							iBrace++;` |
|     ! 0 |  7291 | `						}else if( p->nType & PH7_TK_CCB ){` |
|     ! 0 |  7292 | `							iBrace--;` |
|     ! 0 |  7293 | `							if( iBrace == 0 ){` |
|     ! 0 |  7294 | `								p++;` |
|     ! 0 |  7295 | `								break;` |
|       - |  7296 | `							}` |
|     ! 0 |  7297 | `						}` |
|     ! 0 |  7298 | `						p++;` |
|     ! 0 |  7299 | `					}` |
|     ! 0 |  7300 | `				}` |
|       - |  7301 | `			}` |
|       3 |  7302 | `			continue;` |
|       - |  7303 | `		}` |
|   45125 |  7304 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      69 |  7305 | `			iDepth++;` |
|   45093 |  7306 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      67 |  7307 | `			if( iDepth > 0 ){` |
|      67 |  7308 | `				iDepth--;` |
|      31 |  7309 | `			}` |
|   45030 |  7310 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|   22435 |  7311 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|       - |  7312 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|       - |  7313 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|       - |  7314 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|      11 |  7315 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|      11 |  7316 | `				return 1;` |
|       - |  7317 | `			}` |
|     ! 0 |  7318 | `		}` |
|   45117 |  7319 | `		p++;` |
|       5 |  7320 | `	}` |
|   22449 |  7321 | `	return 0;` |
|   11231 |  7322 | `}` |
|       - |  7323 | `/*` |
|       - |  7324 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  7325 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  7326 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  7327 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  7328 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  7329 | ` * share the same backing.` |
|       - |  7330 | ` */` |
|     212 |  7331 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  7332 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  7333 | `{` |
|     217 |  7334 | `	pAttr->nType = nType;` |
|     217 |  7335 | `	pAttr->sClass = *pClass;` |
|     217 |  7336 | `	pAttr->sTypeName = *pTypeName;` |
|     217 |  7337 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7338 | `		sxu32 i;` |
|      66 |  7339 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      46 |  7340 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      46 |  7341 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      25 |  7342 | `		}` |
|      10 |  7343 | `	}` |
|     217 |  7344 | `}` |
|      92 |  7345 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7346 | `{` |
|      97 |  7347 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7348 | `	SySet *pInstrContainer;` |
|       - |  7349 | `	ph7_class_attr *pCons;` |
|       - |  7350 | `	SyString *pName;` |
|       - |  7351 | `	sxi32 rc;` |
|      97 |  7352 | `	sxu32 nType = 0;` |
|       - |  7353 | `	SyString sTypeClass;` |
|       - |  7354 | `	SyString sTypeText;` |
|       - |  7355 | `	SySet aUnionAlts;` |
|      97 |  7356 | `	sxi32 iTypeFlags = 0;` |
|      97 |  7357 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|      97 |  7358 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|      97 |  7359 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7360 | `	/* Extract visibility level */` |
|      97 |  7361 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7362 | `	/* Mark as constant */` |
|      97 |  7363 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      97 |  7364 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  7365 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  7366 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|     116 |  7367 | `	if( GenStateClassConstHasType(pGen) ){` |
|      61 |  7368 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      38 |  7369 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  7370 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  7371 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  7372 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  7373 | `		 * and success paths release. */` |
|      42 |  7374 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7375 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7376 | `			goto Synchronize;` |
|      42 |  7377 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7378 | `			return SXERR_ABORT;` |
|      42 |  7379 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7380 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7381 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  7382 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7383 | `				return SXERR_ABORT;` |
|       - |  7384 | `			}` |
|     ! 0 |  7385 | `			goto Synchronize;` |
|       - |  7386 | `		}` |
|      42 |  7387 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      19 |  7388 | `	}` |
|      46 |  7389 | `loop:` |
|      99 |  7390 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7391 | `		/* Invalid constant name */` |
|     ! 0 |  7392 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  7393 | `		if( rc == SXERR_ABORT ){` |
|       - |  7394 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7395 | `			return SXERR_ABORT;` |
|       - |  7396 | `		}` |
|     ! 0 |  7397 | `		goto Synchronize;` |
|       - |  7398 | `	}` |
|       - |  7399 | `	/* Peek constant name */` |
|      99 |  7400 | `	pName = &pGen->pIn->sData;` |
|       - |  7401 | `	/* Make sure the constant name isn't reserved */` |
|      99 |  7402 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  7403 | `		/* Reserved constant name */` |
|     ! 0 |  7404 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  7405 | `		if( rc == SXERR_ABORT ){` |
|       - |  7406 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7407 | `			return SXERR_ABORT;` |
|       - |  7408 | `		}` |
|     ! 0 |  7409 | `		goto Synchronize;` |
|       - |  7410 | `	}` |
|       - |  7411 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|      99 |  7412 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      61 |  7413 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      38 |  7414 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      19 |  7415 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      42 |  7416 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7417 | `			return SXERR_ABORT;` |
|      42 |  7418 | `		}else if( rc != SXRET_OK ){` |
|       3 |  7419 | `			goto Synchronize;` |
|       - |  7420 | `		}` |
|      18 |  7421 | `	}` |
|       - |  7422 | `	/* Advance the stream cursor */` |
|      97 |  7423 | `	pGen->pIn++;` |
|      97 |  7424 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  7425 | `		/* Invalid declaration */` |
|     ! 0 |  7426 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  7427 | `		if( rc == SXERR_ABORT ){` |
|       - |  7428 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7429 | `			return SXERR_ABORT;` |
|       - |  7430 | `		}` |
|     ! 0 |  7431 | `		goto Synchronize;` |
|       - |  7432 | `	}` |
|      97 |  7433 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  7434 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|       - |  7435 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|       - |  7436 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|       - |  7437 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|      92 |  7438 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|      39 |  7439 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|       8 |  7440 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7441 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|       2 |  7442 | `			&pClass->sName,pName,&sTypeText);` |
|       6 |  7443 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7444 | `			return SXERR_ABORT;` |
|       - |  7445 | `		}` |
|       6 |  7446 | `		goto Synchronize;` |
|       - |  7447 | `	}` |
|       - |  7448 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|       - |  7449 | `	 * constant initializer ("New expressions are not supported in this context").` |
|       - |  7450 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|      93 |  7451 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|       5 |  7452 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7453 | `			"New expressions are not supported in this context");` |
|       5 |  7454 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7455 | `			return SXERR_ABORT;` |
|       - |  7456 | `		}` |
|       5 |  7457 | `		goto Synchronize;` |
|       - |  7458 | `	}` |
|       - |  7459 | `	/* Allocate a new class attribute */` |
|      89 |  7460 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      89 |  7461 | `	if( pCons == 0 ){` |
|     ! 0 |  7462 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7463 | `		return SXERR_ABORT;` |
|       - |  7464 | `	}` |
|      89 |  7465 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      35 |  7466 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      16 |  7467 | `	}` |
|       - |  7468 | `	/* Swap bytecode container */` |
|      89 |  7469 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      89 |  7470 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  7471 | `	/* Compile constant value.` |
|       - |  7472 | `	 */` |
|      89 |  7473 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      89 |  7474 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  7475 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  7476 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7477 | `			return SXERR_ABORT;` |
|       - |  7478 | `		}` |
|       1 |  7479 | `	}` |
|       - |  7480 | `	/* Emit the done instruction */` |
|      89 |  7481 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      89 |  7482 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      89 |  7483 | `	if( rc == SXERR_ABORT ){` |
|       - |  7484 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7485 | `		return SXERR_ABORT;` |
|       - |  7486 | `	}` |
|       - |  7487 | `	/* All done,install the constant */` |
|      89 |  7488 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      89 |  7489 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7490 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7491 | `		return SXERR_ABORT;` |
|       - |  7492 | `	}` |
|      89 |  7493 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7494 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  7495 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  7496 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7497 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7498 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7499 | `				pTok--;` |
|     ! 0 |  7500 | `			}` |
|     ! 0 |  7501 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7502 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  7503 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7504 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7505 | `				return SXERR_ABORT;` |
|       - |  7506 | `			}` |
|     ! 0 |  7507 | `		}else{` |
|       3 |  7508 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  7509 | `				goto loop;` |
|       - |  7510 | `			}` |
|       - |  7511 | `		}` |
|     ! 0 |  7512 | `	}` |
|      87 |  7513 | `	SySetRelease(&aUnionAlts);` |
|      87 |  7514 | `	return SXRET_OK;` |
|       5 |  7515 | `Synchronize:` |
|      13 |  7516 | `	SySetRelease(&aUnionAlts);` |
|       - |  7517 | `	/* Synchronize with the first semi-colon */` |
|      45 |  7518 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      35 |  7519 | `		pGen->pIn++;` |
|       3 |  7520 | `	}` |
|      13 |  7521 | `	return SXERR_CORRUPT;` |
|      51 |  7522 | `}` |
|       - |  7523 | `/*` |
|       - |  7524 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  7525 | ` * According to the PHP language reference manual` |
|       - |  7526 | ` *  Properties` |
|       - |  7527 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  7528 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  7529 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  7530 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  7531 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  7532 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  7533 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  7534 | ` * Symisc eXtension.` |
|       - |  7535 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  7536 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7537 | ` *  Example:` |
|       - |  7538 | ` *   class Test{` |
|       - |  7539 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7540 | ` *   };` |
|       - |  7541 | ` *   var_dump(TEST::myVar);` |
|       - |  7542 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7543 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7544 | ` */` |
|       - |  7545 | `/*` |
|       - |  7546 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  7547 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  7548 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  7549 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  7550 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  7551 | ` */` |
|  190610 |  7552 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  7553 | `{` |
|  190615 |  7554 | `	SyToken *p = pStart;` |
|  190615 |  7555 | `	int bFirst = 1;` |
|  190615 |  7556 | `	if( p >= pEnd ) return 0;` |
|       - |  7557 | ``	/* Optional nullable `?` shorthand. */`` |
|  190615 |  7558 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      19 |  7559 | `		p++;` |
|      19 |  7560 | `		if( p >= pEnd ) return 0;` |
|       8 |  7561 | `	}` |
|       - |  7562 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  7563 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  7564 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  7565 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   95305 |  7566 | `	for(;;){` |
|  190633 |  7567 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  7568 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  7569 | `			p++;` |
|       9 |  7570 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  7571 | `			if( p >= pEnd ) return 0;` |
|       3 |  7572 | `			p++; /* skip ')' */` |
|       2 |  7573 | `		}else{` |
|       - |  7574 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - |  7575 | ``			 * then any `&`-joined intersection members. */`` |
|  190631 |  7576 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  190631 |  7577 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7578 | `				return 0;` |
|       - |  7579 | `			}` |
|       - |  7580 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - |  7581 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - |  7582 | `			 * may still appear at the initial dispatch site). */` |
|  190631 |  7583 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  190585 |  7584 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  190580 |  7585 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|   11154 |  7586 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  190431 |  7587 | `					return 0;` |
|       - |  7588 | `				}` |
|      77 |  7589 | `			}` |
|     205 |  7590 | `			p++;` |
|     207 |  7591 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7592 | `				p += 2;` |
|       1 |  7593 | `			}` |
|     303 |  7594 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     208 |  7595 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7596 | `				p++; /* skip '&' */` |
|       3 |  7597 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       3 |  7598 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       3 |  7599 | `				p++;` |
|       3 |  7600 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  7601 | `					p += 2;` |
|     ! 0 |  7602 | `				}` |
|       1 |  7603 | `			}` |
|       - |  7604 | `		}` |
|     207 |  7605 | `		bFirst = 0;` |
|     202 |  7606 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      23 |  7607 | `			&& p->sData.zString[0] == '\|' ){` |
|      22 |  7608 | ``			p++; /* next `\|`-separated part */`` |
|      22 |  7609 | `			continue;` |
|       - |  7610 | `		}` |
|     189 |  7611 | `		break;` |
|     ! 0 |  7612 | `	}` |
|     189 |  7613 | `	if( p >= pEnd ) return 0;` |
|     189 |  7614 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   95310 |  7615 | `}` |
|       - |  7616 |  |
|       - |  7617 | `/*` |
|       - |  7618 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  7619 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  7620 | ` * if not). Recognized forms:` |
|       - |  7621 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  7622 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  7623 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  7624 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  7625 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  7626 | ` * on unrecoverable error.` |
|       - |  7627 | ` *` |
|       - |  7628 | ` * When a type is parsed:` |
|       - |  7629 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  7630 | ` *   *pClass is set to the class name (for class types)` |
|       - |  7631 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  7632 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  7633 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  7634 | ` */` |
|     184 |  7635 | `static sxi32 GenStateParsePropertyType(` |
|       - |  7636 | `	ph7_gen_state *pGen,` |
|       - |  7637 | `	sxu32 *pnType,` |
|       - |  7638 | `	SyString *pClass,` |
|       - |  7639 | `	sxi32 *piTypeFlags,` |
|       - |  7640 | `	SyString *pTypeText,` |
|       - |  7641 | `	SySet *pAlts` |
|       5 |  7642 | `){` |
|     189 |  7643 | `	sxi32 iFlags = 0;` |
|       - |  7644 | `	sxi32 rc;` |
|     189 |  7645 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7646 | `		return SXRET_OK;` |
|       - |  7647 | `	}` |
|       - |  7648 | `	/* If the first token is '$', there's no type */` |
|     189 |  7649 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  7650 | `		return SXRET_OK;` |
|       - |  7651 | `	}` |
|     189 |  7652 | `	rc = GenStateParseUnionTypeDecl(` |
|      92 |  7653 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  7654 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  7655 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  7656 | `		/* bAllowVoid */ 0,` |
|     184 |  7657 | `		pGen->pIn->nLine);` |
|     189 |  7658 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7659 | `		return rc;` |
|       - |  7660 | `	}` |
|       - |  7661 | `	/* Verify next token is '$' (start of property name) */` |
|     189 |  7662 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7663 | `		return SXERR_SYNTAX;` |
|       - |  7664 | `	}` |
|     189 |  7665 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     189 |  7666 | `	return SXRET_OK;` |
|      97 |  7667 | `}` |
|       - |  7668 |  |
|       - |  7669 | `/*` |
|       - |  7670 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  7671 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  7672 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  7673 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  7674 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  7675 | ` * by the type parser itself before reaching here.` |
|       - |  7676 | ` *` |
|       - |  7677 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  7678 | ` * use in the error message.` |
|       - |  7679 | ` */` |
|     336 |  7680 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  7681 | `	sxu32 nType,` |
|       - |  7682 | `	const SyString *pClass,` |
|       - |  7683 | `	const char **pzName,` |
|       - |  7684 | `	sxu32 *pnName)` |
|       5 |  7685 | `{` |
|       - |  7686 | `	const char *z;` |
|       - |  7687 | `	sxu32 n;` |
|     341 |  7688 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     287 |  7689 | `		return 0;` |
|       - |  7690 | `	}` |
|      59 |  7691 | `	z = pClass->zString;` |
|      59 |  7692 | `	n = pClass->nByte;` |
|      59 |  7693 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  7694 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  7695 | `	}` |
|       - |  7696 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  7697 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  7698 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      53 |  7699 | `	return 0;` |
|     173 |  7700 | `}` |
|       - |  7701 |  |
|       - |  7702 | `/*` |
|       - |  7703 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  7704 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  7705 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  7706 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  7707 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  7708 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  7709 | ` *` |
|       - |  7710 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  7711 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  7712 | ` */` |
|     278 |  7713 | `static sxi32 GenStateValidateMemberType(` |
|       - |  7714 | `	ph7_gen_state *pGen,` |
|       - |  7715 | `	ph7_class *pClass,` |
|       - |  7716 | `	const SyString *pMemberName,` |
|       - |  7717 | `	sxu32 nType,` |
|       - |  7718 | `	const SyString *pTypeClass,` |
|       - |  7719 | `	const SyString *pTypeText,` |
|       - |  7720 | `	SySet *pUnionAlts,` |
|       - |  7721 | `	const char *zErrFmt,` |
|       - |  7722 | `	sxu32 nLine)` |
|       5 |  7723 | `{` |
|     283 |  7724 | `	const char *zBad = 0;` |
|     283 |  7725 | `	sxu32 nBad = 0;` |
|       - |  7726 | `	SyString sFallback;` |
|       - |  7727 | `	const SyString *pBad;` |
|       - |  7728 | `	sxi32 rc;` |
|     283 |  7729 | `	int bDisallowed = 0;` |
|     283 |  7730 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  7731 | `		bDisallowed = 1;` |
|     281 |  7732 | `	}else if( pUnionAlts ){` |
|       - |  7733 | `		sxu32 i;` |
|      88 |  7734 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      62 |  7735 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      62 |  7736 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  7737 | `				bDisallowed = 1;` |
|       3 |  7738 | `				break;` |
|       - |  7739 | `			}` |
|      32 |  7740 | `		}` |
|      14 |  7741 | `	}` |
|     283 |  7742 | `	if( !bDisallowed ){` |
|     277 |  7743 | `		return SXRET_OK;` |
|       - |  7744 | `	}` |
|       - |  7745 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  7746 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  7747 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  7748 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  7749 | `		pBad = pTypeText;` |
|       5 |  7750 | `	}else{` |
|     ! 0 |  7751 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  7752 | `		pBad = &sFallback;` |
|       - |  7753 | `	}` |
|      11 |  7754 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  7755 | `		zErrFmt,` |
|       3 |  7756 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  7757 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7758 | `		return SXERR_ABORT;` |
|       - |  7759 | `	}` |
|       8 |  7760 | `	return SXERR_SYNTAX;` |
|     144 |  7761 | `}` |
|       - |  7762 | `/*` |
|       - |  7763 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  7764 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  7765 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  7766 | ` * than promoted to a lexer keyword.` |
|       - |  7767 | ` */` |
| 1687330 |  7768 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  7769 | `{` |
| 1721819 |  7770 | `	return (pTok->nType & PH7_TK_ID)` |
|  878149 |  7771 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1721814 |  7772 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  7773 | `}` |
|   77222 |  7774 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7775 | `{` |
|   77227 |  7776 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7777 | `	ph7_class_attr *pAttr;` |
|       - |  7778 | `	SyString *pName;` |
|       - |  7779 | `	sxi32 rc;` |
|   77227 |  7780 | `	sxu32 nType = 0;` |
|       - |  7781 | `	SyString sTypeClass;` |
|       - |  7782 | `	SyString sTypeText;` |
|       - |  7783 | `	SySet aUnionAlts;` |
|   77227 |  7784 | `	sxi32 iTypeFlags = 0;` |
|   77227 |  7785 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   77227 |  7786 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   77227 |  7787 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7788 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  7789 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  7790 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   77227 |  7791 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  7792 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7793 | `	}` |
|       - |  7794 | `	/* Extract visibility level */` |
|   77227 |  7795 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7796 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   77319 |  7797 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     189 |  7798 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     189 |  7799 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7800 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7801 | `			goto Synchronize;` |
|     189 |  7802 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7803 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7804 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7805 | `				&pGen->pIn->sData);` |
|     ! 0 |  7806 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7807 | `				return SXERR_ABORT;` |
|       - |  7808 | `			}` |
|     ! 0 |  7809 | `			goto Synchronize;` |
|     189 |  7810 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7811 | `			return SXERR_ABORT;` |
|       - |  7812 | `		}` |
|      92 |  7813 | `	}` |
|     ! 0 |  7814 | `loop:` |
|   77231 |  7815 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7816 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7817 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7818 | `			return SXERR_ABORT;` |
|       - |  7819 | `		}` |
|     ! 0 |  7820 | `		goto Synchronize;` |
|       - |  7821 | `	}` |
|   77231 |  7822 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   77231 |  7823 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7824 | `		/* Invalid attribute name */` |
|     ! 0 |  7825 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7826 | `		if( rc == SXERR_ABORT ){` |
|       - |  7827 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7828 | `			return SXERR_ABORT;` |
|       - |  7829 | `		}` |
|     ! 0 |  7830 | `		goto Synchronize;` |
|       - |  7831 | `	}` |
|       - |  7832 | `	/* Peek attribute name */` |
|   77231 |  7833 | `	pName = &pGen->pIn->sData;` |
|       - |  7834 | `	/* Advance the stream cursor */` |
|   77231 |  7835 | `	pGen->pIn++;` |
|   77231 |  7836 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7837 | `		/* Invalid declaration */` |
|       3 |  7838 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7839 | `		if( rc == SXERR_ABORT ){` |
|       - |  7840 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7841 | `			return SXERR_ABORT;` |
|       - |  7842 | `		}` |
|       3 |  7843 | `		goto Synchronize;` |
|       - |  7844 | `	}` |
|       - |  7845 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  7846 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   77229 |  7847 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  7848 | `		const char *zRoErr = 0;` |
|      39 |  7849 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  7850 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  7851 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  7852 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  7853 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  7854 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  7855 | `		}` |
|      39 |  7856 | `		if( zRoErr ){` |
|      13 |  7857 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  7858 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7859 | `				return SXERR_ABORT;` |
|       - |  7860 | `			}` |
|      13 |  7861 | `			goto Synchronize;` |
|       - |  7862 | `		}` |
|      12 |  7863 | `	}` |
|       - |  7864 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7865 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7866 | `	 * by the type parser. */` |
|   77219 |  7867 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     278 |  7868 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7869 | `			&sTypeText,` |
|     182 |  7870 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      91 |  7871 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     187 |  7872 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7873 | `			return SXERR_ABORT;` |
|     187 |  7874 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7875 | `			goto Synchronize;` |
|       - |  7876 | `		}` |
|      91 |  7877 | `	}` |
|       - |  7878 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   77219 |  7879 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7880 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7881 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7882 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7883 | `			return SXERR_ABORT;` |
|       - |  7884 | `		}` |
|       3 |  7885 | `		goto Synchronize;` |
|       - |  7886 | `	}` |
|       - |  7887 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|       - |  7888 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|       - |  7889 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|       - |  7890 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|       - |  7891 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|       - |  7892 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   77217 |  7893 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|       6 |  7894 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7895 | `			"New expressions are not supported in this context");` |
|       6 |  7896 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7897 | `			return SXERR_ABORT;` |
|       - |  7898 | `		}` |
|       6 |  7899 | `		goto Synchronize;` |
|       - |  7900 | `	}` |
|       - |  7901 | `	/* Allocate a new class attribute */` |
|   77213 |  7902 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   77213 |  7903 | `	if( pAttr == 0 ){` |
|     ! 0 |  7904 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7905 | `		return SXERR_ABORT;` |
|       - |  7906 | `	}` |
|   77213 |  7907 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     185 |  7908 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      90 |  7909 | `	}` |
|   77213 |  7910 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7911 | `		SySet *pInstrContainer;` |
|   22365 |  7912 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7913 | `		/* Swap bytecode container */` |
|   22365 |  7914 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   22365 |  7915 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7916 | `		/* Compile attribute value.` |
|       - |  7917 | `		 */` |
|   22365 |  7918 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   22365 |  7919 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7920 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7921 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7922 | `				return SXERR_ABORT;` |
|       - |  7923 | `			}` |
|     ! 0 |  7924 | `		}` |
|       - |  7925 | `		/* Emit the done instruction */` |
|   22365 |  7926 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   22365 |  7927 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   11180 |  7928 | `	}` |
|       - |  7929 | `	/* All done,install the attribute */` |
|   77213 |  7930 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   77213 |  7931 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7932 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7933 | `		return SXERR_ABORT;` |
|       - |  7934 | `	}` |
|   77213 |  7935 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7936 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7937 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7938 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7939 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7940 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7941 | `				pTok--;` |
|     ! 0 |  7942 | `			}` |
|     ! 0 |  7943 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7944 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7945 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7946 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7947 | `				return SXERR_ABORT;` |
|       - |  7948 | `			}` |
|     ! 0 |  7949 | `		}else{` |
|       5 |  7950 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7951 | `				goto loop;` |
|       - |  7952 | `			}` |
|       - |  7953 | `		}` |
|     ! 0 |  7954 | `	}` |
|   77209 |  7955 | `	SySetRelease(&aUnionAlts);` |
|   77209 |  7956 | `	return SXRET_OK;` |
|       9 |  7957 | `Synchronize:` |
|       - |  7958 | `	/* Synchronize with the first semi-colon */` |
|      56 |  7959 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      37 |  7960 | `		pGen->pIn++;` |
|       3 |  7961 | `	}` |
|      22 |  7962 | `	SySetRelease(&aUnionAlts);` |
|      22 |  7963 | `	return SXERR_CORRUPT;` |
|   38616 |  7964 | `}` |
|       - |  7965 | `/*` |
|       - |  7966 | ` * Compile a class method.` |
|       - |  7967 | ` *` |
|       - |  7968 | ` * Refer to the official documentation for more information` |
|       - |  7969 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7970 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7971 | ` * overloading and many more.` |
|       - |  7972 | ` */` |
|  274442 |  7973 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7974 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7975 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7976 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7977 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7978 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7979 | `	)` |
|       5 |  7980 | `{` |
|  274447 |  7981 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7982 | `	ph7_class_method *pMeth;` |
|       - |  7983 | `	sxi32 iFuncFlags;` |
|       - |  7984 | `	SyString *pName;` |
|       - |  7985 | `	SyToken *pEnd;` |
|       - |  7986 | `	sxi32 rc;` |
|       - |  7987 | `	/* Extract visibility level */` |
|  274447 |  7988 | `	iProtection = GetProtectionLevel(iProtection);` |
|  274447 |  7989 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  274447 |  7990 | `	iFuncFlags = 0;` |
|  274447 |  7991 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7992 | `		/* Invalid method name */` |
|     ! 0 |  7993 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7994 | `		if( rc == SXERR_ABORT ){` |
|       - |  7995 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7996 | `			return SXERR_ABORT;` |
|       - |  7997 | `		}` |
|     ! 0 |  7998 | `		goto Synchronize;` |
|       - |  7999 | `	}` |
|  274447 |  8000 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  8001 | `		/* Return by reference,remember that */` |
|     ! 0 |  8002 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  8003 | `		/* Jump the '&' token */` |
|     ! 0 |  8004 | `		pGen->pIn++;` |
|     ! 0 |  8005 | `	}` |
|  274447 |  8006 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  8007 | `		/* Invalid method name */` |
|     ! 0 |  8008 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  8009 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8010 | `			return SXERR_ABORT;` |
|       - |  8011 | `		}` |
|     ! 0 |  8012 | `		goto Synchronize;` |
|       - |  8013 | `	}` |
|       - |  8014 | `	/* Peek method name */` |
|  274447 |  8015 | `	pName = &pGen->pIn->sData;` |
|  274447 |  8016 | `	nLine = pGen->pIn->nLine;` |
|       - |  8017 | `	/* Jump the method name */` |
|  274447 |  8018 | `	pGen->pIn++;` |
|  274447 |  8019 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  8020 | `		/* Abstract method */` |
|   94807 |  8021 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  8022 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8023 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  8024 | `				&pClass->sName,pName);` |
|     ! 0 |  8025 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8026 | `				return SXERR_ABORT;` |
|       - |  8027 | `			}` |
|     ! 0 |  8028 | `		}` |
|       - |  8029 | `		/* Assemble method signature only */` |
|   94807 |  8030 | `		doBody = FALSE;` |
|   47401 |  8031 | `	}` |
|  274447 |  8032 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  8033 | `		/* Syntax error */` |
|     ! 0 |  8034 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  8035 | `		if( rc == SXERR_ABORT ){` |
|       - |  8036 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8037 | `			return SXERR_ABORT;` |
|       - |  8038 | `		}` |
|     ! 0 |  8039 | `		goto Synchronize;` |
|       - |  8040 | `	}` |
|       - |  8041 | `	/* Allocate a new class_method instance */` |
|  274447 |  8042 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  274447 |  8043 | `	if( pMeth == 0 ){` |
|     ! 0 |  8044 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8045 | `		return SXERR_ABORT;` |
|       - |  8046 | `	}` |
|       - |  8047 | `	/* Jump the left parenthesis '(' */` |
|  274447 |  8048 | `	pGen->pIn++;` |
|  274447 |  8049 | `	pEnd = 0; /* cc warning */` |
|       - |  8050 | `	/* Delimit the method signature */` |
|  274447 |  8051 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  274447 |  8052 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8053 | `		/* Syntax error */` |
|       3 |  8054 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  8055 | `		if( rc == SXERR_ABORT ){` |
|       - |  8056 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8057 | `			return SXERR_ABORT;` |
|       - |  8058 | `		}` |
|       3 |  8059 | `		goto Synchronize;` |
|       - |  8060 | `	}` |
|       - |  8061 | `	{` |
|  274445 |  8062 | `		int bIsCtor = 0;` |
|  274445 |  8063 | `		int bAbstractCtor = 0;` |
|  274440 |  8064 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  162854 |  8065 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  263431 |  8066 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   22033 |  8067 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  8068 | `				bAbstractCtor = 1;` |
|       2 |  8069 | `			}else{` |
|   22031 |  8070 | `				bIsCtor = 1;` |
|       - |  8071 | `			}` |
|   11014 |  8072 | `		}` |
|  274445 |  8073 | `		if( pGen->pIn < pEnd ){` |
|       - |  8074 | `			/* Collect method arguments */` |
|   73355 |  8075 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   73355 |  8076 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8077 | `				return SXERR_ABORT;` |
|       - |  8078 | `			}` |
|   36675 |  8079 | `		}` |
|       - |  8080 | `	}` |
|       - |  8081 | `	/* Point past ')' and parse optional return type ': type' */` |
|  274445 |  8082 | `	pGen->pIn = &pEnd[1];` |
|       - |  8083 | `	{` |
|  274445 |  8084 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  274445 |  8085 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  8086 | `			return SXERR_ABORT;` |
|  274445 |  8087 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  8088 | `			goto Synchronize;` |
|       - |  8089 | `		}` |
|       - |  8090 | `	}` |
|       - |  8091 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  8092 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  8093 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  8094 | `	{` |
|  274445 |  8095 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  8096 | `		sxu32 i;` |
|  398921 |  8097 | `		for( i = 0; i < nArg; i++ ){` |
|  124491 |  8098 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  8099 | `			ph7_class_attr *pAttr;` |
|  124491 |  8100 | `			sxi32 iAttrFlags = 0;` |
|       - |  8101 | `			int bArgTyped;` |
|  124491 |  8102 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  124427 |  8103 | `				continue;` |
|       - |  8104 | `			}` |
|       - |  8105 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - |  8106 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - |  8107 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      49 |  8108 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      70 |  8109 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|      69 |  8110 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  8111 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8112 | `					"Cannot declare variadic promoted property");` |
|       3 |  8113 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8114 | `					return SXERR_ABORT;` |
|       - |  8115 | `				}` |
|       3 |  8116 | `				goto Synchronize;` |
|       - |  8117 | `			}` |
|       - |  8118 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  8119 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  8120 | `			 * appear as an alternative of a union type. */` |
|      67 |  8121 | `			if( bArgTyped ){` |
|      92 |  8122 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      58 |  8123 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      58 |  8124 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      29 |  8125 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      63 |  8126 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8127 | `					return SXERR_ABORT;` |
|      63 |  8128 | `				}else if( rc != SXRET_OK ){` |
|       6 |  8129 | `					goto Synchronize;` |
|       - |  8130 | `				}` |
|      27 |  8131 | `			}` |
|       - |  8132 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      63 |  8133 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  8134 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8135 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  8136 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8137 | `					return SXERR_ABORT;` |
|       - |  8138 | `				}` |
|       3 |  8139 | `				goto Synchronize;` |
|       - |  8140 | `			}` |
|      61 |  8141 | `			if( bArgTyped ){` |
|      57 |  8142 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      26 |  8143 | `			}` |
|      61 |  8144 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  8145 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  8146 | `			}` |
|      61 |  8147 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 |  8148 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 |  8149 | `			}` |
|      61 |  8150 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  8151 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  8152 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      24 |  8153 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  8154 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8155 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  8156 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8157 | `						return SXERR_ABORT;` |
|       - |  8158 | `					}` |
|       3 |  8159 | `					goto Synchronize;` |
|       - |  8160 | `				}` |
|      22 |  8161 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  8162 | `			}` |
|      59 |  8163 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      59 |  8164 | `			if( pAttr == 0 ){` |
|     ! 0 |  8165 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8166 | `				return SXERR_ABORT;` |
|       - |  8167 | `			}` |
|      59 |  8168 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      57 |  8169 | `				pAttr->nType = pArg->nType;` |
|      57 |  8170 | `				pAttr->sClass = pArg->sClass;` |
|      57 |  8171 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      57 |  8172 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  8173 | `					sxu32 k;` |
|      20 |  8174 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 |  8175 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 |  8176 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 |  8177 | `					}` |
|       3 |  8178 | `				}` |
|      26 |  8179 | `			}` |
|      59 |  8180 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      59 |  8181 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8182 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8183 | `				return SXERR_ABORT;` |
|       - |  8184 | `			}` |
|      32 |  8185 | `		}` |
|       - |  8186 | `	}` |
|  274435 |  8187 | `	if( doBody ){` |
|       - |  8188 | `		/* Compile method body */` |
|  179633 |  8189 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  179633 |  8190 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8191 | `			return SXERR_ABORT;` |
|       - |  8192 | `		}` |
|   89819 |  8193 | `	}else{` |
|       - |  8194 | `		/* Only method signature is allowed */` |
|   94807 |  8195 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  8196 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8197 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  8198 | `				if( rc == SXERR_ABORT ){` |
|       - |  8199 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8200 | `					return SXERR_ABORT;` |
|       - |  8201 | `				}` |
|     ! 0 |  8202 | `				return SXERR_CORRUPT;` |
|       - |  8203 | `			}` |
|       - |  8204 | `	}` |
|       - |  8205 | `	/* All done,install the method */` |
|  274435 |  8206 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  274435 |  8207 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8208 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8209 | `		return SXERR_ABORT;` |
|       - |  8210 | `	}` |
|  274435 |  8211 | `	return SXRET_OK;` |
|       6 |  8212 | `Synchronize:` |
|       - |  8213 | `	/* Synchronize with the first semi-colon */` |
|      40 |  8214 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  8215 | `		pGen->pIn++;` |
|       4 |  8216 | `	}` |
|      16 |  8217 | `	return SXERR_CORRUPT;` |
|  137226 |  8218 | `}` |
|       - |  8219 | `/*` |
|       - |  8220 | ` * Compile an object interface.` |
|       - |  8221 | ` *  According to the PHP language reference manual` |
|       - |  8222 | ` *   Object Interfaces:` |
|       - |  8223 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  8224 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  8225 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  8226 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  8227 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  8228 | ` */` |
|   40168 |  8229 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  8230 | `{` |
|   40173 |  8231 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8232 | `	ph7_class *pClass,*pBase;` |
|       - |  8233 | `	SyToken *pEnd,*pTmp;` |
|       - |  8234 | `	SyString *pName;` |
|       - |  8235 | `	sxi32 nKwrd;` |
|       - |  8236 | `	sxi32 rc;` |
|       - |  8237 | `	/* Jump the 'interface' keyword */` |
|   40173 |  8238 | `	pGen->pIn++;` |
|       - |  8239 | `	/* Extract interface name */` |
|   40173 |  8240 | `	pName = &pGen->pIn->sData;` |
|       - |  8241 | `	/* Advance the stream cursor */` |
|   40173 |  8242 | `	pGen->pIn++;` |
|       - |  8243 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8244 | `		SyBlob sFQN;` |
|       - |  8245 | `		SyString sFQNStr;` |
|   40173 |  8246 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   40173 |  8247 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   40173 |  8248 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   40173 |  8249 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   40173 |  8250 | `		SyBlobRelease(&sFQN);` |
|       - |  8251 | `	}` |
|   40173 |  8252 | `	if( pClass == 0 ){` |
|     ! 0 |  8253 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8254 | `		return SXERR_ABORT;` |
|       - |  8255 | `	}` |
|       - |  8256 | `	/* Mark as an interface */` |
|   40173 |  8257 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  8258 | `	/* Assume no base class is given */` |
|   40173 |  8259 | `	pBase = 0;` |
|   40173 |  8260 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   10945 |  8261 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   10945 |  8262 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  8263 | `			SyBlob sResolved;` |
|       - |  8264 | `			SyString sBaseName;` |
|       - |  8265 | `			sxu32 nRefLine;` |
|       - |  8266 | `			/* Extract base interface */` |
|   10945 |  8267 | `			pGen->pIn++;` |
|   10945 |  8268 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10945 |  8269 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10945 |  8270 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8271 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  8272 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8273 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  8274 | `					pName);` |
|     ! 0 |  8275 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8276 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8277 | `					return SXERR_ABORT;` |
|       - |  8278 | `				}` |
|     ! 0 |  8279 | `				return SXRET_OK;` |
|       - |  8280 | `			}` |
|   16415 |  8281 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   10940 |  8282 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10945 |  8283 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8284 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8285 | `			/* Only interfaces is allowed */` |
|   10945 |  8286 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8287 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8288 | `			}` |
|   10945 |  8289 | `			if( pBase == 0 ){` |
|     ! 0 |  8290 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8291 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  8292 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8293 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8294 | `					return SXERR_ABORT;` |
|       - |  8295 | `				}` |
|     ! 0 |  8296 | `			}` |
|   10945 |  8297 | `			SyBlobRelease(&sResolved);` |
|    5470 |  8298 | `		}` |
|    5470 |  8299 | `	}` |
|   40173 |  8300 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8301 | `		/* Syntax error */` |
|     ! 0 |  8302 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  8303 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8304 | `		if( rc == SXERR_ABORT ){` |
|       - |  8305 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8306 | `			return SXERR_ABORT;` |
|       - |  8307 | `		}` |
|     ! 0 |  8308 | `		return SXRET_OK;` |
|       - |  8309 | `	}` |
|   40173 |  8310 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   40173 |  8311 | `	pEnd = 0; /* cc warning */` |
|       - |  8312 | `	/* Delimit the interface body */` |
|   40173 |  8313 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   40173 |  8314 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8315 | `		/* Syntax error */` |
|     ! 0 |  8316 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  8317 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8318 | `		if( rc == SXERR_ABORT ){` |
|       - |  8319 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8320 | `			return SXERR_ABORT;` |
|       - |  8321 | `		}` |
|     ! 0 |  8322 | `		return SXRET_OK;` |
|       - |  8323 | `	}` |
|       - |  8324 | `	/* Swap token stream */` |
|   40173 |  8325 | `	pTmp = pGen->pEnd;` |
|   40173 |  8326 | `	pGen->pEnd = pEnd;` |
|       - |  8327 | `	/* Start the parse process` |
|       - |  8328 | `	 * Note (According to the PHP reference manual):` |
|       - |  8329 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  8330 | `	 *  Only 'public' visibility is allowed.` |
|       - |  8331 | `	 */` |
|   67480 |  8332 | `	for(;;){` |
|       - |  8333 | `		/* Jump leading/trailing semi-colons */` |
|  229757 |  8334 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   94797 |  8335 | `			pGen->pIn++;` |
|       5 |  8336 | `		}` |
|  134965 |  8337 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8338 | `			/* End of interface body */` |
|   40169 |  8339 | `			break;` |
|       - |  8340 | `		}` |
|   94801 |  8341 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8342 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8343 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  8344 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8345 | `			if( rc == SXERR_ABORT ){` |
|       - |  8346 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8347 | `				return SXERR_ABORT;` |
|       - |  8348 | `			}` |
|     ! 0 |  8349 | `			goto done;` |
|       - |  8350 | `		}` |
|       - |  8351 | `		/* Extract the current keyword */` |
|   94801 |  8352 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   94801 |  8353 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  8354 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  8355 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  8356 | `			const char *zKind = "member";` |
|       3 |  8357 | `			SyString *pMemberName = 0;` |
|       3 |  8358 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  8359 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  8360 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  8361 | `					zKind = "constant";` |
|       3 |  8362 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  8363 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  8364 | `					}` |
|       1 |  8365 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8366 | `					zKind = "method";` |
|     ! 0 |  8367 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  8368 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  8369 | `					}` |
|     ! 0 |  8370 | `				}` |
|       1 |  8371 | `			}` |
|       3 |  8372 | `			if( pMemberName ){` |
|       4 |  8373 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  8374 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  8375 | `			}else{` |
|     ! 0 |  8376 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8377 | `					"Access type for interface %s must be public",zKind);` |
|       - |  8378 | `			}` |
|       3 |  8379 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8380 | `				return SXERR_ABORT;` |
|       - |  8381 | `			}` |
|       3 |  8382 | `			goto done;` |
|       - |  8383 | `		}` |
|   94799 |  8384 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8385 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8386 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8387 | `			if( rc == SXERR_ABORT ){` |
|       - |  8388 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8389 | `				return SXERR_ABORT;` |
|       - |  8390 | `			}` |
|     ! 0 |  8391 | `			goto done;` |
|       - |  8392 | `		}` |
|   94799 |  8393 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  8394 | `			/* Advance the stream cursor */` |
|   94787 |  8395 | `			pGen->pIn++;` |
|   94787 |  8396 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8397 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8398 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8399 | `				if( rc == SXERR_ABORT ){` |
|       - |  8400 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8401 | `					return SXERR_ABORT;` |
|       - |  8402 | `				}` |
|     ! 0 |  8403 | `				goto done;` |
|       - |  8404 | `			}` |
|   94787 |  8405 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   94787 |  8406 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8407 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8408 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8409 | `				if( rc == SXERR_ABORT ){` |
|       - |  8410 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8411 | `					return SXERR_ABORT;` |
|       - |  8412 | `				}` |
|     ! 0 |  8413 | `				goto done;` |
|       - |  8414 | `			}` |
|   47391 |  8415 | `		}` |
|   94799 |  8416 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8417 | `			/* Parse constant */` |
|      10 |  8418 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|      10 |  8419 | `			if( rc != SXRET_OK ){` |
|       3 |  8420 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8421 | `					return SXERR_ABORT;` |
|       - |  8422 | `				}` |
|       3 |  8423 | `				goto done;` |
|       - |  8424 | `			}` |
|       4 |  8425 | `		}else{` |
|   94791 |  8426 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   94791 |  8427 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8428 | `				/* Static method,record that */` |
|   10937 |  8429 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  8430 | `				/* Advance the stream cursor */` |
|   10937 |  8431 | `				pGen->pIn++;` |
|   10932 |  8432 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|   10937 |  8433 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8434 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8435 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8436 | `						if( rc == SXERR_ABORT ){` |
|       - |  8437 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8438 | `							return SXERR_ABORT;` |
|       - |  8439 | `						}` |
|     ! 0 |  8440 | `						goto done;` |
|       - |  8441 | `				}` |
|    5466 |  8442 | `			}` |
|       - |  8443 | `			/* Process method signature (no body for interface methods) */` |
|   94791 |  8444 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   94791 |  8445 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8446 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8447 | `					return SXERR_ABORT;` |
|       - |  8448 | `				}` |
|     ! 0 |  8449 | `				goto done;` |
|       - |  8450 | `			}` |
|       - |  8451 | `		}` |
|       5 |  8452 | `	}` |
|       - |  8453 | `	/* Install the interface */` |
|   40169 |  8454 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   40169 |  8455 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  8456 | `		/* Inherit from the base interface */` |
|   10945 |  8457 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    5470 |  8458 | `	}` |
|   40169 |  8459 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8460 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8461 | `		return SXERR_ABORT;` |
|       - |  8462 | `	}` |
|   20082 |  8463 | `done:` |
|       - |  8464 | `	/* Point beyond the interface body */` |
|   40173 |  8465 | `	pGen->pIn  = &pEnd[1];` |
|   40173 |  8466 | `	pGen->pEnd = pTmp;` |
|   40173 |  8467 | `	return PH7_OK;` |
|   20089 |  8468 | `}` |
|       - |  8469 | `/*` |
|       - |  8470 | ` * Compile a user-defined class.` |
|       - |  8471 | ` * According to the PHP language reference manual` |
|       - |  8472 | ` *  class` |
|       - |  8473 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  8474 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  8475 | ` *  of the properties and methods belonging to the class.` |
|       - |  8476 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  8477 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  8478 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  8479 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  8480 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  8481 | ` *  (called "methods").` |
|       - |  8482 | ` */` |
|       - |  8483 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  8484 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  8485 | `struct TraitUseEntry {` |
|       - |  8486 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  8487 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  8488 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  8489 | `};` |
|       - |  8490 | `/*` |
|       - |  8491 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  8492 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  8493 | ` */` |
|  103292 |  8494 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8495 | `{` |
|       - |  8496 | `	ph7_class **apIface;` |
|       - |  8497 | `	sxu32 nIface,i;` |
|       - |  8498 | `	sxi32 rc;` |
|  103297 |  8499 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  8500 | `		return SXRET_OK;` |
|       - |  8501 | `	}` |
|  103297 |  8502 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  103297 |  8503 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  198295 |  8504 | `	for(i = 0; i < nIface; i++){` |
|   95003 |  8505 | `		ph7_class *pIface = apIface[i];` |
|       - |  8506 | `		SyHashEntry *pEntry;` |
|   95003 |  8507 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  255793 |  8508 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  160795 |  8509 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  8510 | `			ph7_class_method *pImplMeth;` |
|  160795 |  8511 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  8512 | `			/* Find the implementing method in the class */` |
|  160795 |  8513 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  160795 |  8514 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  8515 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  8516 | `			}` |
|       - |  8517 | `			/* Check visibility: interface methods must be implemented as public */` |
|  160781 |  8518 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  8519 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8520 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  8521 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  8522 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8523 | `					return SXERR_ABORT;` |
|       - |  8524 | `				}` |
|       1 |  8525 | `			}` |
|       - |  8526 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  8527 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  8528 | `			 */` |
|       - |  8529 | `			{` |
|  160781 |  8530 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  160781 |  8531 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  160781 |  8532 | `				int sigError = 0;` |
|  160781 |  8533 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  8534 | `					sigError = 1;` |
|  160780 |  8535 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  8536 | `					/* Extra parameters must all have default values */` |
|       6 |  8537 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  8538 | `					sxu32 k;` |
|       8 |  8539 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  8540 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  8541 | `							sigError = 1;` |
|       3 |  8542 | `							break;` |
|       - |  8543 | `						}` |
|       2 |  8544 | `					}` |
|       2 |  8545 | `				}` |
|  160781 |  8546 | `				if( sigError ){` |
|       - |  8547 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  8548 | `					ph7_vm_func_arg *aArgs;` |
|       - |  8549 | `					sxu32 j;` |
|       6 |  8550 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  8551 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  8552 | `					/* Build implementing method signature */` |
|       6 |  8553 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  8554 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  8555 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  8556 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  8557 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8558 | `					}` |
|       - |  8559 | `					/* Build interface method signature */` |
|       6 |  8560 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  8561 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  8562 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  8563 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  8564 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8565 | `					}` |
|       8 |  8566 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8567 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  8568 | `						&pClass->sName,pMName,` |
|       4 |  8569 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  8570 | `						&pIface->sName,pMName,` |
|       4 |  8571 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  8572 | `					SyBlobRelease(&sImplSig);` |
|       6 |  8573 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  8574 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8575 | `						return SXERR_ABORT;` |
|       - |  8576 | `					}` |
|       2 |  8577 | `				}` |
|       - |  8578 | `			}` |
|       5 |  8579 | `		}` |
|   47504 |  8580 | `	}` |
|  103297 |  8581 | `	return SXRET_OK;` |
|   51651 |  8582 | `}` |
|       - |  8583 | `/*` |
|       - |  8584 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  8585 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  8586 | ` */` |
|  103292 |  8587 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8588 | `{` |
|       - |  8589 | `	ph7_class_method *pMeth;` |
|       - |  8590 | `	SyHashEntry *pEntry;` |
|       - |  8591 | `	sxu32 nAbstract;` |
|       - |  8592 | `	SyBlob sMsg;` |
|       - |  8593 | `	sxi32 rc;` |
|       - |  8594 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|  103297 |  8595 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      37 |  8596 | `		return SXRET_OK;` |
|       - |  8597 | `	}` |
|       - |  8598 | `	/* Count abstract methods */` |
|  103265 |  8599 | `	nAbstract = 0;` |
|  103265 |  8600 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  968625 |  8601 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  865365 |  8602 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  865365 |  8603 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  8604 | `			nAbstract++;` |
|       8 |  8605 | `		}` |
|       5 |  8606 | `	}` |
|  103265 |  8607 | `	if( nAbstract == 0 ){` |
|  103251 |  8608 | `		return SXRET_OK;` |
|       - |  8609 | `	}` |
|       - |  8610 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  8611 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  8612 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  8613 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  8614 | `		&pClass->sName,nAbstract,` |
|       7 |  8615 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  8616 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  8617 | `	/* Second pass: list methods with origins */` |
|       - |  8618 | `	{` |
|      18 |  8619 | `		sxu32 nListed = 0;` |
|      18 |  8620 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  8621 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  8622 | `			ph7_class *pOrigin = 0;` |
|       - |  8623 | `			SyString *pMName;` |
|      22 |  8624 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  8625 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  8626 | `				continue;` |
|       - |  8627 | `			}` |
|      20 |  8628 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  8629 | `			if( nListed > 0 ){` |
|       3 |  8630 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  8631 | `			}` |
|       - |  8632 | `			/* Find the origin of this abstract method.` |
|       - |  8633 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  8634 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  8635 | `			 * methods. Abstract class methods only win when the class` |
|       - |  8636 | `			 * itself declared the abstract method (not inherited from` |
|       - |  8637 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  8638 | `			 * class's namespace.` |
|       - |  8639 | `			 */` |
|       - |  8640 | `			{` |
|       - |  8641 | `				ph7_class **apIface;` |
|       - |  8642 | `				ph7_class **apTrait;` |
|       - |  8643 | `				ph7_class *pWalk;` |
|       - |  8644 | `				sxu32 i;` |
|       - |  8645 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  8646 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  8647 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  8648 | `				 */` |
|      20 |  8649 | `				if( pClass->pBase ){` |
|      11 |  8650 | `					pWalk = pClass->pBase;` |
|      19 |  8651 | `					while( pWalk ){` |
|       - |  8652 | `						ph7_class_method *pParentMeth;` |
|      13 |  8653 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  8654 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  8655 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  8656 | `							 * in this class's ancestor chain.` |
|       - |  8657 | `							 */` |
|      13 |  8658 | `							int fromIface = 0;` |
|      13 |  8659 | `							ph7_class *pAnc = pWalk;` |
|      17 |  8660 | `							while( pAnc ){` |
|       - |  8661 | `								ph7_class **apPI;` |
|       - |  8662 | `								sxu32 j;` |
|      15 |  8663 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  8664 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  8665 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  8666 | `										fromIface = 1;` |
|      10 |  8667 | `										break;` |
|       - |  8668 | `									}` |
|     ! 0 |  8669 | `								}` |
|      15 |  8670 | `								if( fromIface ) break;` |
|       6 |  8671 | `								pAnc = pAnc->pBase;` |
|       2 |  8672 | `							}` |
|      13 |  8673 | `							if( !fromIface ){` |
|       3 |  8674 | `								pOrigin = pWalk;` |
|       3 |  8675 | `								break;` |
|       - |  8676 | `							}` |
|       4 |  8677 | `						}` |
|      10 |  8678 | `						pWalk = pWalk->pBase;` |
|       2 |  8679 | `					}` |
|       4 |  8680 | `				}` |
|       - |  8681 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  8682 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  8683 | `				 */` |
|      20 |  8684 | `				if( !pOrigin ){` |
|      18 |  8685 | `					pWalk = pClass;` |
|      40 |  8686 | `					while( pWalk && !pOrigin ){` |
|      26 |  8687 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  8688 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  8689 | `							ph7_class *pIface = apIface[i];` |
|      16 |  8690 | `							ph7_class *pDeepest = 0;` |
|      28 |  8691 | `							while( pIface ){` |
|      16 |  8692 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  8693 | `									pDeepest = pIface;` |
|       6 |  8694 | `								}` |
|      16 |  8695 | `								pIface = pIface->pBase;` |
|       4 |  8696 | `							}` |
|      16 |  8697 | `							if( pDeepest ){` |
|      16 |  8698 | `								pOrigin = pDeepest;` |
|      16 |  8699 | `								break;` |
|       - |  8700 | `							}` |
|     ! 0 |  8701 | `						}` |
|      26 |  8702 | `						pWalk = pWalk->pBase;` |
|       4 |  8703 | `					}` |
|       7 |  8704 | `				}` |
|       - |  8705 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  8706 | `				if( !pOrigin ){` |
|       3 |  8707 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  8708 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  8709 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  8710 | `							pOrigin = pClass;` |
|       3 |  8711 | `							break;` |
|       - |  8712 | `						}` |
|     ! 0 |  8713 | `					}` |
|       1 |  8714 | `				}` |
|       - |  8715 | `			}` |
|      20 |  8716 | `			if( pOrigin ){` |
|      20 |  8717 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  8718 | `			}else{` |
|       - |  8719 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  8720 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  8721 | `			}` |
|      20 |  8722 | `			nListed++;` |
|       4 |  8723 | `		}` |
|       - |  8724 | `	}` |
|      18 |  8725 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  8726 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  8727 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  8728 | `	SyBlobRelease(&sMsg);` |
|      18 |  8729 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8730 | `		return SXERR_ABORT;` |
|       - |  8731 | `	}` |
|      18 |  8732 | `	return SXRET_OK;` |
|   51651 |  8733 | `}` |
|       - |  8734 | `/*` |
|       - |  8735 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  8736 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  8737 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  8738 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  8739 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  8740 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  8741 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  8742 | ` */` |
|   99422 |  8743 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  8744 | `{` |
|   99427 |  8745 | `	int isAbsolute = 0;` |
|   99427 |  8746 | `	SyToken *pStart = pGen->pIn;` |
|       - |  8747 | `	SyBlob sName;` |
|   99427 |  8748 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     101 |  8749 | `		isAbsolute = 1;` |
|     101 |  8750 | `		pGen->pIn++;` |
|      48 |  8751 | `	}` |
|   99427 |  8752 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       9 |  8753 | `		pGen->pIn = pStart;` |
|       9 |  8754 | `		return SXERR_INVALID;` |
|       - |  8755 | `	}` |
|   99421 |  8756 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   99421 |  8757 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   99421 |  8758 | `	pGen->pIn++;` |
|  149142 |  8759 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   49731 |  8760 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  8761 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  8762 | `		pGen->pIn++;` |
|      13 |  8763 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  8764 | `		pGen->pIn++;` |
|       1 |  8765 | `	}` |
|   99421 |  8766 | `	if( isAbsolute ){` |
|      99 |  8767 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      52 |  8768 | `	}else{` |
|       - |  8769 | `		SyString sRaw;` |
|   99327 |  8770 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   99327 |  8771 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  8772 | `	}` |
|   99421 |  8773 | `	SyBlobRelease(&sName);` |
|   99421 |  8774 | `	return SXRET_OK;` |
|   49716 |  8775 | `}` |
|       - |  8776 | `/*` |
|       - |  8777 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  8778 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  8779 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  8780 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  8781 | ` * either direction cannot run unbounded.` |
|       - |  8782 | ` */` |
|       - |  8783 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   11104 |  8784 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  8785 | `{` |
|       - |  8786 | `	ph7_class **apParent;` |
|       - |  8787 | `	sxu32 n;` |
|   18601 |  8788 | `	while( pInterface ){` |
|   14795 |  8789 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  8790 | `			return FALSE;` |
|       - |  8791 | `		}` |
|   18453 |  8792 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    7316 |  8793 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    7303 |  8794 | `			return TRUE;` |
|       - |  8795 | `		}` |
|    7497 |  8796 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    7497 |  8797 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  8798 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  8799 | `				return TRUE;` |
|       - |  8800 | `			}` |
|     ! 0 |  8801 | `		}` |
|    7497 |  8802 | `		pInterface = pInterface->pBase;` |
|    7497 |  8803 | `		iDepth++;` |
|       5 |  8804 | `	}` |
|    3811 |  8805 | `	return FALSE;` |
|    5557 |  8806 | `}` |
|   11104 |  8807 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  8808 | `{` |
|   11109 |  8809 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  8810 | `}` |
|       - |  8811 | `/*` |
|       - |  8812 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  8813 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  8814 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  8815 | ` */` |
|    7298 |  8816 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  8817 | `{` |
|    7307 |  8818 | `	while( pBase ){` |
|      10 |  8819 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  8820 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  8821 | `			return TRUE;` |
|       - |  8822 | `		}` |
|      10 |  8823 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8824 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8825 | `			return TRUE;` |
|       - |  8826 | `		}` |
|       5 |  8827 | `		pBase = pBase->pBase;` |
|       1 |  8828 | `	}` |
|    7299 |  8829 | `	return FALSE;` |
|    3654 |  8830 | `}` |
|       - |  8831 | `/*` |
|       - |  8832 | ` * Compile a class declaration, named or anonymous.` |
|       - |  8833 | ` *` |
|       - |  8834 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - |  8835 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - |  8836 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - |  8837 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - |  8838 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - |  8839 | ` * implements, body, install) is shared by both paths.` |
|       - |  8840 | ` */` |
|  103332 |  8841 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - |  8842 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 |  8843 | `{` |
|  103337 |  8844 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8845 | `	ph7_class *pClass,*pBase;` |
|       - |  8846 | `	SyToken *pEnd,*pTmp;` |
|       - |  8847 | `	sxi32 iProtection;` |
|       - |  8848 | `	SySet aInterfaces;` |
|       - |  8849 | `	SySet aUseEntries;` |
|       - |  8850 | `	sxi32 iAttrflags;` |
|       - |  8851 | `	SyString *pName;` |
|       - |  8852 | `	sxi32 nKwrd;` |
|       - |  8853 | `	sxi32 rc;` |
|       - |  8854 | `	/* Jump the 'class' keyword */` |
|  103337 |  8855 | `	pGen->pIn++;` |
|  103337 |  8856 | `	if( pAnonName ){` |
|       - |  8857 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - |  8858 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - |  8859 | `		 * then use the synthesized name. */` |
|      30 |  8860 | `		*ppArgStart = *ppArgEnd = 0;` |
|      30 |  8861 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       7 |  8862 | `			pGen->pIn++; /* Jump '(' */` |
|       7 |  8863 | `			*ppArgStart = pGen->pIn;` |
|      10 |  8864 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       3 |  8865 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       7 |  8866 | `			pGen->pIn = *ppArgEnd;` |
|       7 |  8867 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       3 |  8868 | `		}` |
|      30 |  8869 | `		pName = pAnonName;` |
|      30 |  8870 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      17 |  8871 | `	}else{` |
|  103311 |  8872 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8873 | `			/* Syntax error */` |
|     ! 0 |  8874 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8875 | `			if( rc == SXERR_ABORT ){` |
|       - |  8876 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8877 | `				return SXERR_ABORT;` |
|       - |  8878 | `			}` |
|       - |  8879 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8880 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8881 | `				pGen->pIn++;` |
|     ! 0 |  8882 | `			}` |
|     ! 0 |  8883 | `			return SXRET_OK;` |
|       - |  8884 | `		}` |
|       - |  8885 | `		/* Extract class name */` |
|  103311 |  8886 | `		pName = &pGen->pIn->sData;` |
|       - |  8887 | `		/* Advance the stream cursor */` |
|  103311 |  8888 | `		pGen->pIn++;` |
|       - |  8889 | `		/* Build FQN and obtain a raw class */ {` |
|       - |  8890 | `			SyBlob sFQN;` |
|       - |  8891 | `			SyString sFQNStr;` |
|  103311 |  8892 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|  103311 |  8893 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|  103311 |  8894 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|  103311 |  8895 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|  103311 |  8896 | `			SyBlobRelease(&sFQN);` |
|       - |  8897 | `		}` |
|       - |  8898 | `	}` |
|  103337 |  8899 | `	if( pClass == 0 ){` |
|     ! 0 |  8900 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8901 | `		return SXERR_ABORT;` |
|       - |  8902 | `	}` |
|       - |  8903 | `	/* implemented interfaces and per-use-statement trait containers */` |
|  103337 |  8904 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|  103337 |  8905 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8906 | `	/* Assume a standalone class */` |
|  103337 |  8907 | `	pBase = 0;` |
|  103337 |  8908 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   87805 |  8909 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   87805 |  8910 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8911 | `			SyBlob sResolved;` |
|       - |  8912 | `			SyString sBaseName;` |
|       - |  8913 | `			sxu32 nRefLine;` |
|   76719 |  8914 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   76719 |  8915 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   76719 |  8916 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   76719 |  8917 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8918 | `				SyBlobRelease(&sResolved);` |
|       4 |  8919 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8920 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8921 | `					pName);` |
|       3 |  8922 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8923 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8924 | `					return SXERR_ABORT;` |
|       - |  8925 | `				}` |
|       3 |  8926 | `				return SXRET_OK;` |
|       - |  8927 | `			}` |
|  115073 |  8928 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   76712 |  8929 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   76717 |  8930 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8931 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8932 | `			/* Interfaces are not allowed */` |
|   76717 |  8933 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8934 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8935 | `			}` |
|   76717 |  8936 | `			if( pBase == 0 ){` |
|     ! 0 |  8937 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8938 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8939 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8940 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8941 | `					return SXERR_ABORT;` |
|       - |  8942 | `				}` |
|     ! 0 |  8943 | `			}else{` |
|   76717 |  8944 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8945 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8946 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8947 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8948 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8949 | `						return SXERR_ABORT;` |
|       - |  8950 | `					}` |
|     ! 0 |  8951 | `				}` |
|       - |  8952 | `			}` |
|   76717 |  8953 | `			SyBlobRelease(&sResolved);` |
|   38356 |  8954 | `		}` |
|   87803 |  8955 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8956 | `			ph7_class *pInterface;` |
|       - |  8957 | `			/* Interface implementation */` |
|   11099 |  8958 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5557 |  8959 | `			for(;;){` |
|       - |  8960 | `				SyBlob sResolved;` |
|       - |  8961 | `				SyString sIntName;` |
|       - |  8962 | `				sxu32 nRefLine;` |
|   11109 |  8963 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   11109 |  8964 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   11109 |  8965 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8966 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8967 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8968 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8969 | `						pName);` |
|     ! 0 |  8970 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8971 | `						return SXERR_ABORT;` |
|       - |  8972 | `					}` |
|     ! 0 |  8973 | `					break;` |
|       - |  8974 | `				}` |
|   22213 |  8975 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   11104 |  8976 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   11109 |  8977 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8978 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8979 | `				/* Only interfaces are allowed */` |
|   11109 |  8980 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8981 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8982 | `				}` |
|   11109 |  8983 | `				if( pInterface == 0 ){` |
|     ! 0 |  8984 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8985 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8986 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8987 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8988 | `						return SXERR_ABORT;` |
|       - |  8989 | `					}` |
|     ! 0 |  8990 | `				}else{` |
|       - |  8991 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8992 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8993 | `					 * unless they already extend Exception or Error.` |
|       - |  8994 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8995 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8996 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   11109 |  8997 | `					SyString *pFqn = &pClass->sName;` |
|   11109 |  8998 | `					int bIsExceptionOrError =` |
|    9200 |  8999 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   18482 |  9000 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    9289 |  9001 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3658 |  9002 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   14753 |  9003 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|   10950 |  9004 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3647 |  9005 | `						!bIsExceptionOrError ){` |
|      12 |  9006 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9007 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  9008 | `							&pClass->sName);` |
|       9 |  9009 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9010 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  9011 | `							return SXERR_ABORT;` |
|       - |  9012 | `						}` |
|       - |  9013 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  9014 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  9015 | `					}else{` |
|   11103 |  9016 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  9017 | `					}` |
|       - |  9018 | `				}` |
|   11109 |  9019 | `				SyBlobRelease(&sResolved);` |
|   11109 |  9020 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5552 |  9021 | `					break;` |
|       - |  9022 | `				}` |
|      14 |  9023 | `				pGen->pIn++;/* Jump the comma */` |
|       4 |  9024 | `			}` |
|    5547 |  9025 | `		}` |
|   43899 |  9026 | `	}` |
|  103335 |  9027 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  9028 | `		/* Syntax error */` |
|     ! 0 |  9029 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  9030 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9031 | `		if( rc == SXERR_ABORT ){` |
|       - |  9032 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9033 | `			return SXERR_ABORT;` |
|       - |  9034 | `		}` |
|     ! 0 |  9035 | `		return SXRET_OK;` |
|       - |  9036 | `	}` |
|  103335 |  9037 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|  103335 |  9038 | `	pEnd = 0; /* cc warning */` |
|       - |  9039 | `	/* Delimit the class body */` |
|  103335 |  9040 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|  103335 |  9041 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  9042 | `		/* Syntax error */` |
|     ! 0 |  9043 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  9044 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9045 | `		if( rc == SXERR_ABORT ){` |
|       - |  9046 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9047 | `			return SXERR_ABORT;` |
|       - |  9048 | `		}` |
|     ! 0 |  9049 | `		return SXRET_OK;` |
|       - |  9050 | `	}` |
|       - |  9051 | `	/* Swap token stream */` |
|  103335 |  9052 | `	pTmp = pGen->pEnd;` |
|  103335 |  9053 | `	pGen->pEnd = pEnd;` |
|       - |  9054 | `	/* Set the inherited flags */` |
|  103335 |  9055 | `	pClass->iFlags = iFlags;` |
|       - |  9056 | `	/* Start the parse process */` |
|  141491 |  9057 | `	for(;;){` |
|       - |  9058 | `		/* Jump leading/trailing semi-colons */` |
|  437549 |  9059 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   77323 |  9060 | `			pGen->pIn++;` |
|       5 |  9061 | `		}` |
|  360231 |  9062 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9063 | `			/* End of class body */` |
|  103297 |  9064 | `			break;` |
|       - |  9065 | `		}` |
|  256934 |  9066 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  128472 |  9067 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  9068 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9069 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  9070 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9071 | `			if( rc == SXERR_ABORT ){` |
|       - |  9072 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  9073 | `				return SXERR_ABORT;` |
|       - |  9074 | `			}` |
|     ! 0 |  9075 | `			goto done;` |
|       - |  9076 | `		}` |
|       - |  9077 | `		/* Assume public visibility */` |
|  256939 |  9078 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  256939 |  9079 | `		iAttrflags = 0;` |
|       - |  9080 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  9081 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  9082 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  9083 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  256939 |  9084 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9085 | `			int bMod = 0;` |
|     ! 0 |  9086 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9087 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  9088 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  9089 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  9090 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  9091 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  9092 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  9093 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  9094 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  9095 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  9096 | `			}` |
|     ! 0 |  9097 | `			if( !bMod ){` |
|     ! 0 |  9098 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9099 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9100 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9101 | `						return SXERR_ABORT;` |
|       - |  9102 | `					}` |
|     ! 0 |  9103 | `					goto done;` |
|       - |  9104 | `				}` |
|     ! 0 |  9105 | `				continue;` |
|       - |  9106 | `			}` |
|     ! 0 |  9107 | `		}` |
|  256939 |  9108 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9109 | `			/* Extract the current keyword */` |
|  256939 |  9110 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  256939 |  9111 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9112 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  9113 | `				TraitUseEntry sUse;` |
|      57 |  9114 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      57 |  9115 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      57 |  9116 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      34 |  9117 | `				for(;;){` |
|       - |  9118 | `					ph7_class *pTrait;` |
|       - |  9119 | `					SyString *pTraitName;` |
|      65 |  9120 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9121 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9122 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  9123 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9124 | `							return SXERR_ABORT;` |
|       - |  9125 | `						}` |
|     ! 0 |  9126 | `						break;` |
|       - |  9127 | `					}` |
|      65 |  9128 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  9129 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  9130 | `						SyBlob sResolved;` |
|      65 |  9131 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      65 |  9132 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     125 |  9133 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      60 |  9134 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      65 |  9135 | `						SyBlobRelease(&sResolved);` |
|       - |  9136 | `					}` |
|       - |  9137 | `					/* Only traits are allowed */` |
|      65 |  9138 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9139 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  9140 | `					}` |
|      65 |  9141 | `					if( pTrait == 0 ){` |
|     ! 0 |  9142 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9143 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  9144 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9145 | `							return SXERR_ABORT;` |
|       - |  9146 | `						}` |
|     ! 0 |  9147 | `					}else{` |
|      65 |  9148 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  9149 | `					}` |
|      65 |  9150 | `					pGen->pIn++; /* Advance past trait name */` |
|      65 |  9151 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      31 |  9152 | `						break;` |
|       - |  9153 | `					}` |
|      10 |  9154 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  9155 | `				}` |
|       - |  9156 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      57 |  9157 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  9158 | `					SyToken *pBlock;` |
|      13 |  9159 | `					pGen->pIn++; /* Jump '{' */` |
|      13 |  9160 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      13 |  9161 | `					sUse.pResolvStart = pGen->pIn;` |
|      13 |  9162 | `					sUse.pResolvEnd = pBlock;` |
|      13 |  9163 | `					if( pBlock < pGen->pEnd ){` |
|      13 |  9164 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       8 |  9165 | `					}else{` |
|     ! 0 |  9166 | `						pGen->pIn = pGen->pEnd;` |
|       - |  9167 | `					}` |
|       5 |  9168 | `				}` |
|      57 |  9169 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  9170 | `				/* The semicolon will be consumed by the outer loop */` |
|      57 |  9171 | `				continue;` |
|       - |  9172 | `			}` |
|  256887 |  9173 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  256581 |  9174 | `				iProtection = nKwrd;` |
|  256581 |  9175 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  9176 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  256581 |  9177 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  9178 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  9179 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  9180 | `				}` |
|  256576 |  9181 | `				if( pGen->pIn >= pGen->pEnd` |
|  256581 |  9182 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9183 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9184 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  9185 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9186 | `					if( rc == SXERR_ABORT ){` |
|       - |  9187 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  9188 | `						return SXERR_ABORT;` |
|       - |  9189 | `					}` |
|     ! 0 |  9190 | `					goto done;` |
|       - |  9191 | `				}` |
|  256581 |  9192 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9193 | `					/* Attribute declaration (untyped) */` |
|   77017 |  9194 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   77017 |  9195 | `					if( rc != SXRET_OK ){` |
|      11 |  9196 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9197 | `							return SXERR_ABORT;` |
|       - |  9198 | `						}` |
|      11 |  9199 | `						goto done;` |
|       - |  9200 | `					}` |
|   77009 |  9201 | `					continue;` |
|       - |  9202 | `				}` |
|  179569 |  9203 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9204 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     173 |  9205 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     173 |  9206 | `					if( rc != SXRET_OK ){` |
|       8 |  9207 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9208 | `							return SXERR_ABORT;` |
|       - |  9209 | `						}` |
|       8 |  9210 | `						goto done;` |
|       - |  9211 | `					}` |
|     167 |  9212 | `					continue;` |
|       - |  9213 | `				}` |
|       - |  9214 | `				/* Extract the keyword */` |
|  179401 |  9215 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   89698 |  9216 | `			}` |
|  179707 |  9217 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  9218 | `				/* Process constant declaration */` |
|      79 |  9219 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      79 |  9220 | `				if( rc != SXRET_OK ){` |
|      11 |  9221 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9222 | `						return SXERR_ABORT;` |
|       - |  9223 | `					}` |
|      11 |  9224 | `					goto done;` |
|       - |  9225 | `				}` |
|      38 |  9226 | `			}else{` |
|  179633 |  9227 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  9228 | `					/* Static method or attribute,record that */` |
|   10999 |  9229 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|   10999 |  9230 | `					pGen->pIn++; /* Jump the static keyword */` |
|   10999 |  9231 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9232 | `						/* Extract the keyword */` |
|   10989 |  9233 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   10989 |  9234 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9235 | `							iProtection = nKwrd;` |
|     ! 0 |  9236 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  9237 | `						}` |
|    5492 |  9238 | `					}` |
|       - |  9239 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  9240 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  9241 | `					 * than a generic "expecting method" parse error. */` |
|   10999 |  9242 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9243 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9244 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  9245 | `					}` |
|   10994 |  9246 | `					if( pGen->pIn >= pGen->pEnd` |
|   10999 |  9247 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9248 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9249 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  9250 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9251 | `						if( rc == SXERR_ABORT ){` |
|       - |  9252 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9253 | `							return SXERR_ABORT;` |
|       - |  9254 | `						}` |
|     ! 0 |  9255 | `						goto done;` |
|       - |  9256 | `					}` |
|   10999 |  9257 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9258 | `						/* Attribute declaration */` |
|      11 |  9259 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 |  9260 | `						if( rc != SXRET_OK ){` |
|       3 |  9261 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9262 | `								return SXERR_ABORT;` |
|       - |  9263 | `							}` |
|       3 |  9264 | `							goto done;` |
|       - |  9265 | `						}` |
|       8 |  9266 | `						continue;` |
|       - |  9267 | `					}` |
|   10991 |  9268 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9269 | `						/* Typed static attribute declaration */` |
|      15 |  9270 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  9271 | `						if( rc != SXRET_OK ){` |
|       3 |  9272 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9273 | `								return SXERR_ABORT;` |
|       - |  9274 | `							}` |
|       3 |  9275 | `							goto done;` |
|       - |  9276 | `						}` |
|      13 |  9277 | `						continue;` |
|       - |  9278 | `					}` |
|       - |  9279 | `					/* Extract the keyword */` |
|   10979 |  9280 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  174126 |  9281 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  9282 | `					/* Abstract method,record that */` |
|      15 |  9283 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  9284 | `					/* Mark the whole class as abstract */` |
|      15 |  9285 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  9286 | `					/* Advance the stream cursor */` |
|      15 |  9287 | `					pGen->pIn++;` |
|      15 |  9288 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      15 |  9289 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      15 |  9290 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      13 |  9291 | `							iProtection = nKwrd;` |
|      13 |  9292 | `							pGen->pIn++; /* Jump the visibility token */` |
|       5 |  9293 | `						}` |
|       6 |  9294 | `					}` |
|      15 |  9295 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      12 |  9296 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9297 | `							/* Static method */` |
|     ! 0 |  9298 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9299 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9300 | `					}` |
|      15 |  9301 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      12 |  9302 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9303 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9304 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  9305 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9306 | `							if( rc == SXERR_ABORT ){` |
|       - |  9307 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9308 | `								return SXERR_ABORT;` |
|       - |  9309 | `							}` |
|     ! 0 |  9310 | `							goto done;` |
|       - |  9311 | `					}` |
|      15 |  9312 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  168633 |  9313 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  9314 | `					/* final method ,record that */` |
|      17 |  9315 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      17 |  9316 | `					pGen->pIn++; /* Jump the final keyword */` |
|      17 |  9317 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9318 | `						/* Extract the keyword */` |
|      17 |  9319 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      17 |  9320 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       9 |  9321 | `							iProtection = nKwrd;` |
|       9 |  9322 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  9323 | `						}` |
|       7 |  9324 | `					}` |
|      17 |  9325 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  9326 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  9327 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  9328 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  9329 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  9330 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9331 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  9332 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  9333 | `									return SXERR_ABORT;` |
|       - |  9334 | `								}` |
|     ! 0 |  9335 | `								goto done;` |
|       - |  9336 | `							}` |
|      12 |  9337 | `							continue;` |
|       - |  9338 | `					}` |
|       6 |  9339 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  9340 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9341 | `							/* Static method */` |
|     ! 0 |  9342 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9343 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9344 | `					}` |
|       6 |  9345 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9346 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9347 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9348 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  9349 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9350 | `							if( rc == SXERR_ABORT ){` |
|       - |  9351 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9352 | `								return SXERR_ABORT;` |
|       - |  9353 | `							}` |
|     ! 0 |  9354 | `							goto done;` |
|       - |  9355 | `					}` |
|       6 |  9356 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9357 | `				}` |
|  179603 |  9358 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9359 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9360 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  9361 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9362 | `						if( rc == SXERR_ABORT ){` |
|       - |  9363 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9364 | `							return SXERR_ABORT;` |
|       - |  9365 | `						}` |
|     ! 0 |  9366 | `						goto done;` |
|       - |  9367 | `				}` |
|  179603 |  9368 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  9369 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  9370 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  9371 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9372 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9373 | `						if( rc == SXERR_ABORT ){` |
|       - |  9374 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9375 | `							return SXERR_ABORT;` |
|       - |  9376 | `						}` |
|     ! 0 |  9377 | `						goto done;` |
|       - |  9378 | `					}` |
|       - |  9379 | `					/* Attribute declaration */` |
|       7 |  9380 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  9381 | `				}else{` |
|       - |  9382 | `					/* Process method declaration */` |
|  179597 |  9383 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9384 | `				}` |
|  179603 |  9385 | `				if( rc != SXRET_OK ){` |
|      16 |  9386 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9387 | `						return SXERR_ABORT;` |
|       - |  9388 | `					}` |
|      16 |  9389 | `					goto done;` |
|       - |  9390 | `				}` |
|       - |  9391 | `			}` |
|   89831 |  9392 | `		}else{` |
|       - |  9393 | `			/* Attribute declaration */` |
|     ! 0 |  9394 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9395 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9396 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9397 | `					return SXERR_ABORT;` |
|       - |  9398 | `				}` |
|     ! 0 |  9399 | `				goto done;` |
|       - |  9400 | `			}` |
|       - |  9401 | `		}` |
|       5 |  9402 | `	}` |
|       - |  9403 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  9404 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  9405 | `	 */` |
|       - |  9406 | `	{` |
|       - |  9407 | `		TraitUseEntry *apUse;` |
|       - |  9408 | `		sxu32 nU;` |
|  103297 |  9409 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|  103349 |  9410 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      57 |  9411 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      57 |  9412 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      57 |  9413 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      57 |  9414 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  9415 | `			sxu32 nT;` |
|      57 |  9416 | `			if( !hasResolution ){` |
|       - |  9417 | `				/* No conflict resolution block: use standard trait application */` |
|      95 |  9418 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      53 |  9419 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      53 |  9420 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9421 | `						break;` |
|       - |  9422 | `					}` |
|      29 |  9423 | `				}` |
|      26 |  9424 | `			}else{` |
|       - |  9425 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  9426 | `				 * then use the block to resolve method conflicts.` |
|       - |  9427 | `				 */` |
|       - |  9428 | `				SyToken *pR;` |
|      25 |  9429 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      15 |  9430 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  9431 | `					ph7_class_attr *pAR;` |
|       - |  9432 | `					SyHashEntry *pER;` |
|       - |  9433 | `					SyString *pNR;` |
|      15 |  9434 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      21 |  9435 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  9436 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  9437 | `						pNR = &pAR->sName;` |
|     ! 0 |  9438 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  9439 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  9440 | `						}` |
|     ! 0 |  9441 | `					}` |
|      15 |  9442 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       9 |  9443 | `				}` |
|       - |  9444 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      13 |  9445 | `				pR = pUse->pResolvStart;` |
|      27 |  9446 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9447 | `					SyString sTrait,sMethod;` |
|       - |  9448 | `					ph7_class *pSrcTrait;` |
|       - |  9449 | `					ph7_class_method *pMeth;` |
|       - |  9450 | `					sxi32 nRKwrd;` |
|      41 |  9451 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9452 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9453 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9454 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9455 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9456 | `					sMethod = pR->sData;` |
|      17 |  9457 | `					pR++;` |
|      17 |  9458 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9459 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9460 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9461 | `							sTrait = sMethod;` |
|       7 |  9462 | `							pR++;` |
|       7 |  9463 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9464 | `							sMethod = pR->sData;` |
|       7 |  9465 | `							pR++;` |
|       3 |  9466 | `						}` |
|       3 |  9467 | `					}` |
|      17 |  9468 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9469 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9470 | `						continue;` |
|       - |  9471 | `					}` |
|      17 |  9472 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9473 | `					pR++;` |
|      17 |  9474 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  9475 | `						pSrcTrait = 0;` |
|       7 |  9476 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  9477 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  9478 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  9479 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  9480 | `								pSrcTrait = apTrait[nT];` |
|       5 |  9481 | `								break;` |
|       - |  9482 | `							}` |
|       2 |  9483 | `						}` |
|       5 |  9484 | `						if( pSrcTrait ){` |
|       5 |  9485 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  9486 | `							if( pMeth ){` |
|       5 |  9487 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  9488 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  9489 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  9490 | `								}` |
|       2 |  9491 | `							}` |
|       2 |  9492 | `						}` |
|       2 |  9493 | `					}` |
|      35 |  9494 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9495 | `				}` |
|       - |  9496 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      25 |  9497 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  9498 | `					ph7_class_method *pMR;` |
|       - |  9499 | `					SyHashEntry *pER;` |
|       - |  9500 | `					SyString *pNR;` |
|      15 |  9501 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      41 |  9502 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      23 |  9503 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      23 |  9504 | `						pNR = &pMR->sFunc.sName;` |
|      23 |  9505 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      14 |  9506 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       6 |  9507 | `						}` |
|       3 |  9508 | `					}` |
|       9 |  9509 | `				}` |
|       - |  9510 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      13 |  9511 | `				pR = pUse->pResolvStart;` |
|      27 |  9512 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9513 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  9514 | `					ph7_class *pSrcTrait;` |
|       - |  9515 | `					ph7_class_method *pMeth;` |
|      27 |  9516 | `					int hasQual = 0;` |
|       - |  9517 | `					sxi32 nRKwrd;` |
|      41 |  9518 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9519 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9520 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9521 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9522 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      17 |  9523 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9524 | `					sMethod = pR->sData;` |
|      17 |  9525 | `					pR++;` |
|      17 |  9526 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9527 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9528 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9529 | `							sTrait = sMethod;` |
|       7 |  9530 | `							hasQual = 1;` |
|       7 |  9531 | `							pR++;` |
|       7 |  9532 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9533 | `							sMethod = pR->sData;` |
|       7 |  9534 | `							pR++;` |
|       3 |  9535 | `						}` |
|       3 |  9536 | `					}` |
|      17 |  9537 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9538 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9539 | `						continue;` |
|       - |  9540 | `					}` |
|      17 |  9541 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9542 | `					pR++;` |
|      17 |  9543 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      13 |  9544 | `						sxi32 iNewVis = -1;` |
|      13 |  9545 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  9546 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  9547 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  9548 | `								iNewVis = nAK;` |
|       7 |  9549 | `								pR++;` |
|       3 |  9550 | `							}` |
|       3 |  9551 | `						}` |
|      13 |  9552 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      11 |  9553 | `							sAlias = pR->sData;` |
|      11 |  9554 | `							pR++;` |
|       4 |  9555 | `						}` |
|      13 |  9556 | `						pMeth = 0;` |
|      13 |  9557 | `						if( hasQual ){` |
|       3 |  9558 | `							pSrcTrait = 0;` |
|       5 |  9559 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  9560 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  9561 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  9562 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  9563 | `									pSrcTrait = apTrait[nT];` |
|       3 |  9564 | `									break;` |
|       - |  9565 | `								}` |
|       2 |  9566 | `							}` |
|       3 |  9567 | `							if( pSrcTrait ){` |
|       3 |  9568 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  9569 | `							}` |
|       2 |  9570 | `						}else{` |
|      10 |  9571 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  9572 | `						}` |
|      13 |  9573 | `						if( pMeth ){` |
|      13 |  9574 | `							if( sAlias.nByte > 0 ){` |
|       - |  9575 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  9576 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  9577 | `								 */` |
|       - |  9578 | `								ph7_class_method *pAlias;` |
|       - |  9579 | `								char *zAliasDup;` |
|      11 |  9580 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      11 |  9581 | `								if( pAlias ){` |
|      11 |  9582 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      11 |  9583 | `									if( iNewVis >= 0 ){` |
|       5 |  9584 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9585 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9586 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  9587 | `									}` |
|      11 |  9588 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      11 |  9589 | `									if( zAliasDup ){` |
|      11 |  9590 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       4 |  9591 | `									}` |
|       7 |  9592 | `								}` |
|       7 |  9593 | `							}else if( iNewVis >= 0 ){` |
|       - |  9594 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  9595 | `								ph7_class_method *pCopy;` |
|       3 |  9596 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  9597 | `								if( pCopy ){` |
|       3 |  9598 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  9599 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  9600 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9601 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9602 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  9603 | `									/* Replace the method in the class hash */` |
|       3 |  9604 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  9605 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  9606 | `								}` |
|       1 |  9607 | `							}` |
|       5 |  9608 | `						}` |
|       5 |  9609 | `						SXUNUSED(hasQual);` |
|       5 |  9610 | `					}` |
|      21 |  9611 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9612 | `				}` |
|       - |  9613 | `			}` |
|      57 |  9614 | `			SySetRelease(&pUse->aTraits);` |
|      31 |  9615 | `		}` |
|       - |  9616 | `	}` |
|       - |  9617 | `	/* Install the class */` |
|  103297 |  9618 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|  103297 |  9619 | `	if( rc == SXRET_OK ){` |
|       - |  9620 | `		ph7_class **apInterface;` |
|       - |  9621 | `		sxu32 n;` |
|  103297 |  9622 | `		if( pBase ){` |
|       - |  9623 | `			/* Inherit from base class and mark as a subclass */` |
|   76717 |  9624 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   38356 |  9625 | `		}` |
|  103297 |  9626 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  114395 |  9627 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  9628 | `			/* Implements one or more interface */` |
|   11103 |  9629 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   11103 |  9630 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9631 | `				break;` |
|       - |  9632 | `			}` |
|    5554 |  9633 | `		}` |
|       - |  9634 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  9635 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  103292 |  9636 | `		if( rc == SXRET_OK` |
|  103292 |  9637 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|  103297 |  9638 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   83907 |  9639 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  9640 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   83907 |  9641 | `			if( pStringable ){` |
|   83907 |  9642 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   83907 |  9643 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  9644 | `				sxu32 i;` |
|   83907 |  9645 | `				int bAlready = 0;` |
|   91199 |  9646 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    7299 |  9647 | `					if( apImpl[i] == pStringable ){` |
|       3 |  9648 | `						bAlready = 1;` |
|       3 |  9649 | `						break;` |
|       - |  9650 | `					}` |
|    3651 |  9651 | `				}` |
|   83907 |  9652 | `				if( !bAlready ){` |
|   83905 |  9653 | `					PH7_ClassImplement(pClass,pStringable);` |
|   41950 |  9654 | `				}` |
|   41951 |  9655 | `			}` |
|   41951 |  9656 | `		}` |
|       - |  9657 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|  103297 |  9658 | `		if( rc == SXRET_OK ){` |
|  103297 |  9659 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|  103297 |  9660 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9661 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9662 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9663 | `				return SXERR_ABORT;` |
|       - |  9664 | `			}` |
|   51646 |  9665 | `		}` |
|       - |  9666 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|  103297 |  9667 | `		if( rc == SXRET_OK ){` |
|  103297 |  9668 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|  103297 |  9669 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9670 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9671 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9672 | `				return SXERR_ABORT;` |
|       - |  9673 | `			}` |
|   51646 |  9674 | `		}` |
|   51646 |  9675 | `	}` |
|  103297 |  9676 | `	SySetRelease(&aUseEntries);` |
|  103297 |  9677 | `	SySetRelease(&aInterfaces);` |
|  103297 |  9678 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9679 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9680 | `		return SXERR_ABORT;` |
|       - |  9681 | `	}` |
|   51646 |  9682 | `done:` |
|       - |  9683 | `	/* Point beyond the class body */` |
|  103335 |  9684 | `	pGen->pIn = &pEnd[1];` |
|  103335 |  9685 | `	pGen->pEnd = pTmp;` |
|  103335 |  9686 | `	return PH7_OK;` |
|   51671 |  9687 | `}` |
|       - |  9688 | `/* Compile a named class declaration (the common case). */` |
|  103306 |  9689 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  9690 | `{` |
|  103311 |  9691 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 |  9692 | `}` |
|       - |  9693 | `/*` |
|       - |  9694 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - |  9695 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - |  9696 | ` * compile + install the class body once (at compile time, like every other` |
|       - |  9697 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - |  9698 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - |  9699 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - |  9700 | ` */` |
|      26 |  9701 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  9702 | `{` |
|       - |  9703 | `	char zName[128];         /* Synthesized class name */` |
|       - |  9704 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - |  9705 | `	SyString sName;` |
|       - |  9706 | `	SyToken *pArgStart,*pArgEnd;` |
|       - |  9707 | `	ph7_value *pObj;` |
|      30 |  9708 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9709 | `	sxu32 nIdx,nLen;` |
|       - |  9710 | `	sxi32 nArg,rc;` |
|      13 |  9711 | `	SXUNUSED(iCompileFlag);` |
|       - |  9712 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|      30 |  9713 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      30 |  9714 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  9715 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 |  9716 | `	}` |
|      30 |  9717 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  9718 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - |  9719 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - |  9720 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|      30 |  9721 | `	pArgStart = pArgEnd = 0;` |
|      30 |  9722 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      30 |  9723 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9724 | `		return rc;` |
|       - |  9725 | `	}` |
|       - |  9726 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - |  9727 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      30 |  9728 | `	nArg = 0;` |
|      30 |  9729 | `	if( pArgStart < pArgEnd ){` |
|       7 |  9730 | `		SyToken *pSavedIn = pGen->pIn;` |
|       7 |  9731 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  9732 | `		SyToken *pArgNext;` |
|       7 |  9733 | `		pGen->pIn = pArgStart;` |
|       7 |  9734 | `		pGen->pEnd = pArgEnd;` |
|      13 |  9735 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       7 |  9736 | `			if( pGen->pIn < pArgNext ){` |
|       7 |  9737 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       7 |  9738 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9739 | `					pGen->pIn = pSavedIn;` |
|     ! 0 |  9740 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 |  9741 | `					return SXERR_ABORT;` |
|       - |  9742 | `				}` |
|       7 |  9743 | `				nArg++;` |
|       3 |  9744 | `			}` |
|       7 |  9745 | `			pGen->pIn = &pArgNext[1];` |
|       1 |  9746 | `		}` |
|       7 |  9747 | `		pGen->pIn = pSavedIn;` |
|       7 |  9748 | `		pGen->pEnd = pSavedEnd;` |
|       3 |  9749 | `	}` |
|       - |  9750 | `	/* Load the synthesized class name */` |
|      30 |  9751 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      30 |  9752 | `	if( pObj == 0 ){` |
|     ! 0 |  9753 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9754 | `		return SXERR_ABORT;` |
|       - |  9755 | `	}` |
|      30 |  9756 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      30 |  9757 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  9758 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      30 |  9759 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      30 |  9760 | `	return SXRET_OK;` |
|      17 |  9761 | `}` |
|       - |  9762 | `/*` |
|       - |  9763 | ` * Compile a user-defined abstract class.` |
|       - |  9764 | ` *  According to the PHP language reference manual` |
|       - |  9765 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  9766 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  9767 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  9768 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  9769 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  9770 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  9771 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  9772 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  9773 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  9774 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  9775 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  9776 | ` *   could differ.` |
|       - |  9777 | ` */` |
|       - |  9778 | `/*` |
|       - |  9779 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - |  9780 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - |  9781 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - |  9782 | ` */` |
| 1000350 |  9783 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 |  9784 | `{` |
| 1000355 |  9785 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  669377 |  9786 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  669377 |  9787 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  662071 |  9788 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  331002 |  9789 | `	}` |
|  992987 |  9790 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  992927 |  9791 | `	return FALSE;` |
|  500180 |  9792 | `}` |
|       - |  9793 | `/*` |
|       - |  9794 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - |  9795 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - |  9796 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - |  9797 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - |  9798 | ` */` |
|  992922 |  9799 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 |  9800 | `{` |
|  992927 |  9801 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  992927 |  9802 | `	sxi32 iFlags = 0,iFlag;` |
| 1000355 |  9803 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    7433 |  9804 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 |  9805 | `			pDup = pIn;` |
|       2 |  9806 | `		}` |
|    7433 |  9807 | `		iFlags \|= iFlag;` |
|    7433 |  9808 | `		pIn++;` |
|       5 |  9809 | `	}` |
|  992927 |  9810 | `	*ppIn = pIn;` |
|  992927 |  9811 | `	if( ppDup ){ *ppDup = pDup; }` |
|  992927 |  9812 | `	return iFlags;` |
|       5 |  9813 | `}` |
|       - |  9814 | `/*` |
|       - |  9815 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - |  9816 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - |  9817 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - |  9818 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - |  9819 | `` * `readonly`) to their existing handlers.`` |
|       - |  9820 | ` */` |
|  989218 |  9821 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 |  9822 | `{` |
|  989223 |  9823 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  498320 |  9824 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  991072 |  9825 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 |  9826 | `}` |
|       - |  9827 | `/*` |
|       - |  9828 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - |  9829 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - |  9830 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - |  9831 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - |  9832 | `` * `abstract`+`final` pair, like PHP.`` |
|       - |  9833 | ` */` |
|    3704 |  9834 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 |  9835 | `{` |
|       - |  9836 | `	SyToken *pDup;` |
|    3709 |  9837 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - |  9838 | `	sxi32 rc;` |
|    3709 |  9839 | `	if( pDup ){` |
|       4 |  9840 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 |  9841 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 |  9842 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9843 | `			return SXERR_ABORT;` |
|       - |  9844 | `		}` |
|       1 |  9845 | `	}` |
|    3704 |  9846 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    1857 |  9847 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 |  9848 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9849 | `			"Cannot use the final modifier on an abstract class");` |
|       3 |  9850 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9851 | `			return SXERR_ABORT;` |
|       - |  9852 | `		}` |
|       1 |  9853 | `	}` |
|    3709 |  9854 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    1857 |  9855 | `}` |
|       - |  9856 | `/*` |
|       - |  9857 | ` * Compile a user-defined trait.` |
|       - |  9858 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  9859 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  9860 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  9861 | ` */` |
|      64 |  9862 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 |  9863 | `{` |
|      69 |  9864 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9865 | `	ph7_class *pClass;` |
|       - |  9866 | `	SyToken *pEnd,*pTmp;` |
|       - |  9867 | `	sxi32 iProtection;` |
|       - |  9868 | `	sxi32 iAttrflags;` |
|       - |  9869 | `	SyString *pName;` |
|       - |  9870 | `	sxi32 nKwrd;` |
|       - |  9871 | `	sxi32 rc;` |
|       - |  9872 | `	/* Jump the 'trait' keyword */` |
|      69 |  9873 | `	pGen->pIn++;` |
|      69 |  9874 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9875 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  9876 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9877 | `			return SXERR_ABORT;` |
|       - |  9878 | `		}` |
|     ! 0 |  9879 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  9880 | `			pGen->pIn++;` |
|     ! 0 |  9881 | `		}` |
|     ! 0 |  9882 | `		return SXRET_OK;` |
|       - |  9883 | `	}` |
|       - |  9884 | `	/* Extract trait name */` |
|      69 |  9885 | `	pName = &pGen->pIn->sData;` |
|      69 |  9886 | `	pGen->pIn++;` |
|       - |  9887 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  9888 | `		SyBlob sFQN;` |
|       - |  9889 | `		SyString sFQNStr;` |
|      69 |  9890 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      69 |  9891 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      69 |  9892 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      69 |  9893 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      69 |  9894 | `		SyBlobRelease(&sFQN);` |
|       - |  9895 | `	}` |
|      69 |  9896 | `	if( pClass == 0 ){` |
|     ! 0 |  9897 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9898 | `		return SXERR_ABORT;` |
|       - |  9899 | `	}` |
|       - |  9900 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      69 |  9901 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  9902 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  9903 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9904 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9905 | `			return SXERR_ABORT;` |
|       - |  9906 | `		}` |
|     ! 0 |  9907 | `		return SXRET_OK;` |
|       - |  9908 | `	}` |
|      69 |  9909 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      69 |  9910 | `	pEnd = 0;` |
|      69 |  9911 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      69 |  9912 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  9913 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  9914 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9915 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9916 | `			return SXERR_ABORT;` |
|       - |  9917 | `		}` |
|     ! 0 |  9918 | `		return SXRET_OK;` |
|       - |  9919 | `	}` |
|       - |  9920 | `	/* Swap token stream */` |
|      69 |  9921 | `	pTmp = pGen->pEnd;` |
|      69 |  9922 | `	pGen->pEnd = pEnd;` |
|       - |  9923 | `	/* Mark as trait */` |
|      69 |  9924 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  9925 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      64 |  9926 | `	for(;;){` |
|     177 |  9927 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 |  9928 | `			pGen->pIn++;` |
|       4 |  9929 | `		}` |
|     153 |  9930 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      69 |  9931 | `			break;` |
|       - |  9932 | `		}` |
|      89 |  9933 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  9934 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9935 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9936 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9937 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9938 | `				return SXERR_ABORT;` |
|       - |  9939 | `			}` |
|     ! 0 |  9940 | `			goto done;` |
|       - |  9941 | `		}` |
|      89 |  9942 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      89 |  9943 | `		iAttrflags = 0;` |
|      89 |  9944 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      89 |  9945 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      89 |  9946 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9947 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  9948 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  9949 | `				for(;;){` |
|       - |  9950 | `					ph7_class *pUsedTrait;` |
|       - |  9951 | `					SyString *pUsedName;` |
|       5 |  9952 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9953 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9954 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  9955 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9956 | `							return SXERR_ABORT;` |
|       - |  9957 | `						}` |
|     ! 0 |  9958 | `						break;` |
|       - |  9959 | `					}` |
|       5 |  9960 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  9961 | `					{` |
|       - |  9962 | `						SyBlob sResolved;` |
|       5 |  9963 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  9964 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  9965 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  9966 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  9967 | `						SyBlobRelease(&sResolved);` |
|       - |  9968 | `					}` |
|       5 |  9969 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9970 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  9971 | `					}` |
|       5 |  9972 | `					if( pUsedTrait == 0 ){` |
|       4 |  9973 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  9974 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  9975 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9976 | `							return SXERR_ABORT;` |
|       - |  9977 | `						}` |
|       2 |  9978 | `					}else{` |
|       3 |  9979 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  9980 | `					}` |
|       5 |  9981 | `					pGen->pIn++;` |
|       5 |  9982 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  9983 | `						break;` |
|       - |  9984 | `					}` |
|     ! 0 |  9985 | `					pGen->pIn++;` |
|     ! 0 |  9986 | `				}` |
|       5 |  9987 | `				continue;` |
|       - |  9988 | `			}` |
|      85 |  9989 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 |  9990 | `				iProtection = nKwrd;` |
|      73 |  9991 | `				pGen->pIn++;` |
|      68 |  9992 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 |  9993 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9994 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9995 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9996 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9997 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9998 | `						return SXERR_ABORT;` |
|       - |  9999 | `					}` |
|     ! 0 | 10000 | `					goto done;` |
|       - | 10001 | `				}` |
|      73 | 10002 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 | 10003 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 | 10004 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 10005 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10006 | `							return SXERR_ABORT;` |
|       - | 10007 | `						}` |
|     ! 0 | 10008 | `						goto done;` |
|       - | 10009 | `					}` |
|      12 | 10010 | `					continue;` |
|       - | 10011 | `				}` |
|      63 | 10012 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 | 10013 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 10014 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 10015 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10016 | `							return SXERR_ABORT;` |
|       - | 10017 | `						}` |
|     ! 0 | 10018 | `						goto done;` |
|       - | 10019 | `					}` |
|       5 | 10020 | `					continue;` |
|       - | 10021 | `				}` |
|      58 | 10022 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 | 10023 | `			}` |
|      71 | 10024 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 10025 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10026 | `					"Traits cannot have constants");` |
|     ! 0 | 10027 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10028 | `					return SXERR_ABORT;` |
|       - | 10029 | `				}` |
|     ! 0 | 10030 | `				goto done;` |
|     ! 0 | 10031 | `			}else{` |
|      71 | 10032 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 10033 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 10034 | `					pGen->pIn++;` |
|       5 | 10035 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 10036 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 10037 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 10038 | `							iProtection = nKwrd;` |
|     ! 0 | 10039 | `							pGen->pIn++;` |
|     ! 0 | 10040 | `						}` |
|       1 | 10041 | `					}` |
|       4 | 10042 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 | 10043 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 10044 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10045 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 10046 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 10047 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10048 | `							return SXERR_ABORT;` |
|       - | 10049 | `						}` |
|     ! 0 | 10050 | `						goto done;` |
|       - | 10051 | `					}` |
|       5 | 10052 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 10053 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 10054 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 10055 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 10056 | `								return SXERR_ABORT;` |
|       - | 10057 | `							}` |
|     ! 0 | 10058 | `							goto done;` |
|       - | 10059 | `						}` |
|       3 | 10060 | `						continue;` |
|       - | 10061 | `					}` |
|       3 | 10062 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 | 10063 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10064 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 10065 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 10066 | `								return SXERR_ABORT;` |
|       - | 10067 | `							}` |
|     ! 0 | 10068 | `							goto done;` |
|       - | 10069 | `						}` |
|     ! 0 | 10070 | `						continue;` |
|       - | 10071 | `					}` |
|       3 | 10072 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      68 | 10073 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 | 10074 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 | 10075 | `					pGen->pIn++;` |
|       6 | 10076 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 | 10077 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 | 10078 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 | 10079 | `							iProtection = nKwrd;` |
|       6 | 10080 | `							pGen->pIn++;` |
|       2 | 10081 | `						}` |
|       2 | 10082 | `					}` |
|       6 | 10083 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 10084 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 10085 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10086 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 10087 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 10088 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10089 | `							return SXERR_ABORT;` |
|       - | 10090 | `						}` |
|     ! 0 | 10091 | `						goto done;` |
|       - | 10092 | `					}` |
|       6 | 10093 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 10094 | `				}` |
|      69 | 10095 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 10096 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10097 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 10098 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 10099 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10100 | `						return SXERR_ABORT;` |
|       - | 10101 | `					}` |
|     ! 0 | 10102 | `					goto done;` |
|       - | 10103 | `				}` |
|      69 | 10104 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 10105 | `					pGen->pIn++;` |
|     ! 0 | 10106 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 10107 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10108 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 10109 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10110 | `							return SXERR_ABORT;` |
|       - | 10111 | `						}` |
|     ! 0 | 10112 | `						goto done;` |
|       - | 10113 | `					}` |
|     ! 0 | 10114 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10115 | `				}else{` |
|      69 | 10116 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 10117 | `				}` |
|      69 | 10118 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10119 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10120 | `						return SXERR_ABORT;` |
|       - | 10121 | `					}` |
|     ! 0 | 10122 | `					goto done;` |
|       - | 10123 | `				}` |
|       - | 10124 | `			}` |
|      37 | 10125 | `		}else{` |
|     ! 0 | 10126 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10127 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10128 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10129 | `					return SXERR_ABORT;` |
|       - | 10130 | `				}` |
|     ! 0 | 10131 | `				goto done;` |
|       - | 10132 | `			}` |
|       - | 10133 | `		}` |
|       5 | 10134 | `	}` |
|       - | 10135 | `	/* Install the trait */` |
|      69 | 10136 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      69 | 10137 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10138 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10139 | `		return SXERR_ABORT;` |
|       - | 10140 | `	}` |
|      32 | 10141 | `done:` |
|       - | 10142 | `	/* Point beyond the trait body */` |
|      69 | 10143 | `	pGen->pIn = &pEnd[1];` |
|      69 | 10144 | `	pGen->pEnd = pTmp;` |
|      69 | 10145 | `	return PH7_OK;` |
|      37 | 10146 | `}` |
|       - | 10147 | `/*` |
|       - | 10148 | ` * Compile a user-defined class.` |
|       - | 10149 | ` *  According to the PHP language reference manual` |
|       - | 10150 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 10151 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 10152 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 10153 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 10154 | ` *   and functions (called "methods").` |
|       - | 10155 | ` */` |
|   99602 | 10156 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 | 10157 | `{` |
|       - | 10158 | `	sxi32 rc;` |
|   99607 | 10159 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   99607 | 10160 | `	return rc;` |
|       5 | 10161 | `}` |
|       - | 10162 | `/*` |
|       - | 10163 | ` * Exception handling.` |
|       - | 10164 | ` *  According to the PHP language reference manual` |
|       - | 10165 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 10166 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 10167 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 10168 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 10169 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 10170 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 10171 | ` *    (or re-thrown) within a catch block.` |
|       - | 10172 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 10173 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 10174 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 10175 | ` *    been defined with set_exception_handler().` |
|       - | 10176 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 10177 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 10178 | ` */` |
|       - | 10179 | `/*` |
|       - | 10180 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 10181 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 10182 | ` * indicates failure.` |
|       - | 10183 | ` */` |
|   14920 | 10184 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 10185 | `{` |
|   14925 | 10186 | `	sxi32 rc = SXRET_OK;` |
|   14925 | 10187 | `	if( pRoot->pOp ){` |
|   14915 | 10188 | `		switch( pRoot->pOp->iOp ){` |
|    7455 | 10189 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - | 10190 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - | 10191 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - | 10192 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - | 10193 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - | 10194 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   14915 | 10195 | `			break;` |
|     ! 0 | 10196 | `		default:` |
|       - | 10197 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - | 10198 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - | 10199 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 | 10200 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 10201 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 10202 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 10203 | `				rc = SXERR_INVALID;` |
|     ! 0 | 10204 | `			}` |
|     ! 0 | 10205 | `			break;` |
|       - | 10206 | `		}` |
|    7470 | 10207 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 10208 | `		/* Unexpected expression */` |
|     ! 0 | 10209 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 10210 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10211 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 10212 | `			rc = SXERR_INVALID;` |
|     ! 0 | 10213 | `		}` |
|     ! 0 | 10214 | `	}` |
|   14925 | 10215 | `	return rc;` |
|       5 | 10216 | `}` |
|       - | 10217 | `/*` |
|       - | 10218 | ` * Compile a 'throw' statement.` |
|       - | 10219 | ` * throw: This is how you trigger an exception.` |
|       - | 10220 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 10221 | ` */` |
|   14884 | 10222 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 | 10223 | `{` |
|   14889 | 10224 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10225 | `	GenBlock *pBlock;` |
|       - | 10226 | `	sxu32 nIdx;` |
|       - | 10227 | `	sxi32 rc;` |
|   14889 | 10228 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 10229 | `	/* Compile the expression */` |
|   14889 | 10230 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   14889 | 10231 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10232 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 10233 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10234 | `			return SXERR_ABORT;` |
|       - | 10235 | `		}` |
|     ! 0 | 10236 | `		return SXRET_OK;` |
|       - | 10237 | `	}` |
|   14889 | 10238 | `	pBlock = pGen->pCurrent;` |
|       - | 10239 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   58925 | 10240 | `	while(pBlock->pParent){` |
|   58921 | 10241 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   14885 | 10242 | `			break;` |
|       - | 10243 | `		}` |
|       - | 10244 | `		/* Point to the parent block */` |
|   44041 | 10245 | `		pBlock = pBlock->pParent;` |
|       5 | 10246 | `	}` |
|       - | 10247 | `	/* Emit the throw instruction */` |
|   14889 | 10248 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 10249 | `	/* Emit the jump */` |
|   14889 | 10250 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   14889 | 10251 | `	return SXRET_OK;` |
|    7447 | 10252 | `}` |
|       - | 10253 | `/*` |
|       - | 10254 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - | 10255 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - | 10256 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - | 10257 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - | 10258 | ` * the validator guarantees the operand is a valid exception target.` |
|       - | 10259 | ` */` |
|      36 | 10260 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 10261 | `{` |
|      38 | 10262 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10263 | `	GenBlock *pBlock;` |
|       - | 10264 | `	sxu32 nIdx;` |
|       - | 10265 | `	sxi32 rc;` |
|      18 | 10266 | `	(void)iCompileFlag;` |
|      38 | 10267 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 | 10268 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 10269 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10270 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10271 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10272 | `			return SXERR_ABORT;` |
|       - | 10273 | `		}` |
|     ! 0 | 10274 | `		return SXRET_OK;` |
|       - | 10275 | `	}` |
|      38 | 10276 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 | 10277 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10278 | `		return SXERR_ABORT;` |
|       - | 10279 | `	}` |
|      38 | 10280 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10281 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10282 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10283 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10284 | `			return SXERR_ABORT;` |
|       - | 10285 | `		}` |
|     ! 0 | 10286 | `		return SXRET_OK;` |
|       - | 10287 | `	}` |
|       - | 10288 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 | 10289 | `	pBlock = pGen->pCurrent;` |
|      60 | 10290 | `	while( pBlock->pParent ){` |
|      49 | 10291 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 | 10292 | `			break;` |
|       - | 10293 | `		}` |
|      23 | 10294 | `		pBlock = pBlock->pParent;` |
|       1 | 10295 | `	}` |
|      38 | 10296 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 | 10297 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 | 10298 | `	return SXRET_OK;` |
|      20 | 10299 | `}` |
|       - | 10300 | `/*` |
|       - | 10301 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|       - | 10302 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|       - | 10303 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|       - | 10304 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|       - | 10305 | ` * compile error propagated from the parser.` |
|       - | 10306 | ` */` |
|     ! 0 | 10307 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|     ! 0 | 10308 | `{` |
|       - | 10309 | `	SyString sClassName;` |
|       - | 10310 | `	SyToken *pToken;` |
|       - | 10311 | `	SyString *pName;` |
|       - | 10312 | `	char *zDup;` |
|       - | 10313 | `	sxi32 rc;` |
|     ! 0 | 10314 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|     ! 0 | 10315 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|     ! 0 | 10316 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|     ! 0 | 10317 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 | 10318 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 10319 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10320 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10321 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10322 | `		return SXERR_INVALID;` |
|       - | 10323 | `	}` |
|     ! 0 | 10324 | `	pGen->pIn++; /* '(' */` |
|     ! 0 | 10325 | `	for(;;){` |
|       - | 10326 | `		SyBlob sResolved;` |
|     ! 0 | 10327 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     ! 0 | 10328 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 10329 | `			SyBlobRelease(&sResolved);` |
|     ! 0 | 10330 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10331 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10332 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10333 | `			return SXERR_INVALID;` |
|       - | 10334 | `		}` |
|     ! 0 | 10335 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     ! 0 | 10336 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     ! 0 | 10337 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     ! 0 | 10338 | `		SyBlobRelease(&sResolved);` |
|     ! 0 | 10339 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|     ! 0 | 10340 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|     ! 0 | 10341 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|     ! 0 | 10342 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|     ! 0 | 10343 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     ! 0 | 10344 | `			pGen->pIn++; continue;` |
|       - | 10345 | `		}` |
|     ! 0 | 10346 | `		break;` |
|     ! 0 | 10347 | `	}` |
|     ! 0 | 10348 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|     ! 0 | 10349 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 10350 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10351 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10352 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10353 | `		return SXERR_INVALID;` |
|       - | 10354 | `	}` |
|     ! 0 | 10355 | `	pGen->pIn++; /* '$' */` |
|     ! 0 | 10356 | `	pName = &pGen->pIn->sData;` |
|     ! 0 | 10357 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 | 10358 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|     ! 0 | 10359 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|     ! 0 | 10360 | `	pGen->pIn++;` |
|     ! 0 | 10361 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 10362 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10363 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10364 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10365 | `		return SXERR_INVALID;` |
|       - | 10366 | `	}` |
|     ! 0 | 10367 | `	pGen->pIn++; /* ')' */` |
|     ! 0 | 10368 | `	return SXRET_OK;` |
|     ! 0 | 10369 | `}` |
|       - | 10370 | `/*` |
|       - | 10371 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|       - | 10372 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|       - | 10373 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|       - | 10374 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|       - | 10375 | ` * VmThrowException):` |
|       - | 10376 | ` *` |
|       - | 10377 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|       - | 10378 | ` *    <try body>` |
|       - | 10379 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|       - | 10380 | ` *    JMP  -> finally\|end` |
|       - | 10381 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|       - | 10382 | ` *    <catch body>` |
|       - | 10383 | ` *    JMP  -> finally\|end` |
|       - | 10384 | ` *    ... more catches ...` |
|       - | 10385 | ` *  Lfin: <finally body>` |
|       - | 10386 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|       - | 10387 | ` *  Lend:` |
|       - | 10388 | ` */` |
|     ! 0 | 10389 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|     ! 0 | 10390 | `{` |
|     ! 0 | 10391 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10392 | `	GenBlock *pTry;` |
|       - | 10393 | `	VmInstr *pInstr;` |
|     ! 0 | 10394 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|       - | 10395 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|       - | 10396 | `	sxi32 rc;` |
|     ! 0 | 10397 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|       - | 10398 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|     ! 0 | 10399 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     ! 0 | 10400 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|     ! 0 | 10401 | `	pTry->pUserData = pException;` |
|     ! 0 | 10402 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|     ! 0 | 10403 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|     ! 0 | 10404 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     ! 0 | 10405 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|     ! 0 | 10406 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|     ! 0 | 10407 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10408 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|     ! 0 | 10409 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|     ! 0 | 10410 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|     ! 0 | 10411 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|     ! 0 | 10412 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10413 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|     ! 0 | 10414 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|       - | 10415 | `	/* Catch clauses (inline) */` |
|     ! 0 | 10416 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     ! 0 | 10417 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|     ! 0 | 10418 | `		sxu32 k = 0;` |
|     ! 0 | 10419 | `		for(;;){` |
|       - | 10420 | `			ph7_exception_block sCatch;` |
|       - | 10421 | `			GenBlock *pCatchBlk;` |
|     ! 0 | 10422 | `			sxu32 idxJmp = 0;` |
|     ! 0 | 10423 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 10424 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     ! 0 | 10425 | `				break;` |
|       - | 10426 | `			}` |
|     ! 0 | 10427 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|     ! 0 | 10428 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|     ! 0 | 10429 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|     ! 0 | 10430 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 | 10431 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|     ! 0 | 10432 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|     ! 0 | 10433 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|     ! 0 | 10434 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|     ! 0 | 10435 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|     ! 0 | 10436 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|     ! 0 | 10437 | `			GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 | 10438 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|     ! 0 | 10439 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|     ! 0 | 10440 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     ! 0 | 10441 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|     ! 0 | 10442 | `			k++;` |
|     ! 0 | 10443 | `		}` |
|     ! 0 | 10444 | `	}` |
|       - | 10445 | `	/* Finally (inline) */` |
|     ! 0 | 10446 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     ! 0 | 10447 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10448 | `		GenBlock *pFinBlk;` |
|     ! 0 | 10449 | `		pGen->pIn++; /* Jump 'finally' */` |
|     ! 0 | 10450 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 | 10451 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|     ! 0 | 10452 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|     ! 0 | 10453 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     ! 0 | 10454 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|     ! 0 | 10455 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|     ! 0 | 10456 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 | 10457 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|     ! 0 | 10458 | `		pException->iHasFinally = 1;` |
|     ! 0 | 10459 | `	}` |
|     ! 0 | 10460 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 | 10461 | `	pException->iInlined = 1;` |
|       - | 10462 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|       - | 10463 | `	{` |
|     ! 0 | 10464 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|       - | 10465 | `		sxu32 *aJ; sxu32 n;` |
|     ! 0 | 10466 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|     ! 0 | 10467 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|     ! 0 | 10468 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|     ! 0 | 10469 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|     ! 0 | 10470 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|     ! 0 | 10471 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|     ! 0 | 10472 | `		}` |
|       - | 10473 | `	}` |
|     ! 0 | 10474 | `	SySetRelease(&aCatchJmp);` |
|     ! 0 | 10475 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|     ! 0 | 10476 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|     ! 0 | 10477 | `	}` |
|     ! 0 | 10478 | `	return SXRET_OK;` |
|     ! 0 | 10479 | `}` |
|       - | 10480 | `/*` |
|       - | 10481 | ` * Compile a 'catch' block.` |
|       - | 10482 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 10483 | ` * an object containing the exception information.` |
|       - | 10484 | ` */` |
|     636 | 10485 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 10486 | `{` |
|     641 | 10487 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10488 | `	ph7_exception_block sCatch;` |
|       - | 10489 | `	SySet *pInstrContainer;` |
|       - | 10490 | `	SyString sClassName;` |
|       - | 10491 | `	GenBlock *pCatch;` |
|       - | 10492 | `	SyToken *pToken;` |
|       - | 10493 | `	SyString *pName;` |
|       - | 10494 | `	char *zDup;` |
|       - | 10495 | `	sxi32 rc;` |
|     641 | 10496 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 10497 | `	/* Zero the structure */` |
|     641 | 10498 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 10499 | `	/* Initialize fields */` |
|     641 | 10500 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     641 | 10501 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     641 | 10502 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 10503 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10504 | `			pToken = pGen->pIn;` |
|     ! 0 | 10505 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10506 | `				pToken--;` |
|     ! 0 | 10507 | `			}` |
|     ! 0 | 10508 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10509 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10510 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10511 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10512 | `				return SXERR_ABORT;` |
|       - | 10513 | `			}` |
|     ! 0 | 10514 | `			return SXERR_INVALID;` |
|       - | 10515 | `	}` |
|       - | 10516 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     641 | 10517 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     332 | 10518 | `	for(;;){` |
|       - | 10519 | `		SyBlob sResolved;` |
|     669 | 10520 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     669 | 10521 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 10522 | `			SyBlobRelease(&sResolved);` |
|       6 | 10523 | `			pToken = pGen->pIn;` |
|       6 | 10524 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10525 | `				pToken--;` |
|     ! 0 | 10526 | `			}` |
|       8 | 10527 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10528 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 10529 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 10530 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10531 | `				return SXERR_ABORT;` |
|       - | 10532 | `			}` |
|       6 | 10533 | `			return SXERR_INVALID;` |
|       - | 10534 | `		}` |
|       - | 10535 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 10536 | `		 * transient SyBlob allocation. */` |
|     995 | 10537 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     660 | 10538 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     665 | 10539 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     665 | 10540 | `		SyBlobRelease(&sResolved);` |
|     665 | 10541 | `		if( zDup == 0 ){` |
|     ! 0 | 10542 | `			goto Mem;` |
|       - | 10543 | `		}` |
|     665 | 10544 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     665 | 10545 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10546 | `			goto Mem;` |
|       - | 10547 | `		}` |
|       - | 10548 | `		/* Check for '\|' (multi-catch separator) */` |
|     660 | 10549 | `		if( pGen->pIn < pGen->pEnd &&` |
|     660 | 10550 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      33 | 10551 | `			pGen->pIn->sData.nByte == 1 &&` |
|      28 | 10552 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      30 | 10553 | `			pGen->pIn++; /* Consume the '\|' */` |
|      30 | 10554 | `			continue;` |
|       - | 10555 | `		}` |
|     637 | 10556 | `		break;` |
|     ! 0 | 10557 | `	}` |
|     632 | 10558 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     637 | 10559 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 10560 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10561 | `			pToken = pGen->pIn;` |
|     ! 0 | 10562 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10563 | `				pToken--;` |
|     ! 0 | 10564 | `			}` |
|     ! 0 | 10565 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10566 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10567 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10568 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10569 | `				return SXERR_ABORT;` |
|       - | 10570 | `			}` |
|     ! 0 | 10571 | `			return SXERR_INVALID;` |
|       - | 10572 | `	}` |
|     637 | 10573 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 10574 | `	/* Duplicate instance name */` |
|     637 | 10575 | `	pName = &pGen->pIn->sData;` |
|     637 | 10576 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     637 | 10577 | `	if( zDup == 0 ){` |
|     ! 0 | 10578 | `		goto Mem;` |
|       - | 10579 | `	}` |
|     637 | 10580 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     637 | 10581 | `	pGen->pIn++;` |
|     637 | 10582 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 10583 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 10584 | `		pToken = pGen->pIn;` |
|     ! 0 | 10585 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10586 | `			pToken--;` |
|     ! 0 | 10587 | `		}` |
|     ! 0 | 10588 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10589 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10590 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10591 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10592 | `			return SXERR_ABORT;` |
|       - | 10593 | `		}` |
|     ! 0 | 10594 | `		return SXERR_INVALID;` |
|       - | 10595 | `	}` |
|       - | 10596 | `	/* Compile the block */` |
|     637 | 10597 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 10598 | `	/* Create the catch block */` |
|     637 | 10599 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     637 | 10600 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10601 | `		return SXERR_ABORT;` |
|       - | 10602 | `	}` |
|       - | 10603 | `	/* Swap bytecode container */` |
|     637 | 10604 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     637 | 10605 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 10606 | `	/* Compile the block */` |
|     637 | 10607 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 10608 | `	/* Fix forward jumps now the destination is resolved  */` |
|     637 | 10609 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10610 | `	/* Emit the DONE instruction */` |
|     637 | 10611 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10612 | `	/* Leave the block */` |
|     637 | 10613 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10614 | `	/* Restore the default container */` |
|     637 | 10615 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10616 | `	/* Install the catch block */` |
|     637 | 10617 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     637 | 10618 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10619 | `		goto Mem;` |
|       - | 10620 | `	}` |
|     637 | 10621 | `	return SXRET_OK;` |
|     ! 0 | 10622 | `Mem:` |
|     ! 0 | 10623 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10624 | `	return SXERR_ABORT;` |
|     323 | 10625 | `}` |
|       - | 10626 | `/*` |
|       - | 10627 | ` * Compile a 'try' block.` |
|       - | 10628 | ` * A function using an exception should be in a "try" block.` |
|       - | 10629 | ` * If the exception does not trigger, the code will continue` |
|       - | 10630 | ` * as normal. However if the exception triggers, an exception` |
|       - | 10631 | ` * is "thrown".` |
|       - | 10632 | ` */` |
|     694 | 10633 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 10634 | `{` |
|       - | 10635 | `	ph7_exception *pException;` |
|     699 | 10636 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10637 | `	GenBlock *pTry;` |
|       - | 10638 | `	sxu32 nJmpIdx;` |
|       - | 10639 | `	sxi32 rc;` |
|       - | 10640 | `	/* Create the exception container */` |
|     699 | 10641 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     699 | 10642 | `	if( pException == 0 ){` |
|     ! 0 | 10643 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 10644 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10645 | `		return SXERR_ABORT;` |
|       - | 10646 | `	}` |
|       - | 10647 | `	/* Zero the structure */` |
|     699 | 10648 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 10649 | `	/* Initialize fields */` |
|     699 | 10650 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     699 | 10651 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     699 | 10652 | `	pException->iHasFinally = 0;` |
|     699 | 10653 | `	pException->iFinallyDone = 0;` |
|     699 | 10654 | `	pException->pVm = pGen->pVm;` |
|       - | 10655 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|       - | 10656 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|       - | 10657 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|       - | 10658 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|       - | 10659 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|       - | 10660 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     699 | 10661 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|     ! 0 | 10662 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|       - | 10663 | `	}` |
|       - | 10664 | `	/* Create the try block */` |
|     699 | 10665 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     699 | 10666 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10667 | `		return SXERR_ABORT;` |
|       - | 10668 | `	}` |
|       - | 10669 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     699 | 10670 | `	pTry->pUserData = pException;` |
|       - | 10671 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     699 | 10672 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 10673 | `	/* Fix the jump later when the destination is resolved */` |
|     699 | 10674 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     699 | 10675 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 10676 | `	/* Compile the block */` |
|     699 | 10677 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     699 | 10678 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10679 | `		return SXERR_ABORT;` |
|       - | 10680 | `	}` |
|       - | 10681 | `	/* Fix forward jumps now the destination is resolved */` |
|     699 | 10682 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10683 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     699 | 10684 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 10685 | `	/* Leave the block */` |
|     699 | 10686 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10687 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     699 | 10688 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     692 | 10689 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 10690 | `		/* Compile one or more catch blocks */` |
|     632 | 10691 | `		for(;;){` |
|    1264 | 10692 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    1034 | 10693 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     319 | 10694 | `					break;` |
|       - | 10695 | `			}` |
|     641 | 10696 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     641 | 10697 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10698 | `				return SXERR_ABORT;` |
|       - | 10699 | `			}` |
|       5 | 10700 | `		}` |
|     314 | 10701 | `	}` |
|       - | 10702 | `	/* Compile optional finally block */` |
|     699 | 10703 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     396 | 10704 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10705 | `		SySet *pInstrContainer;` |
|       - | 10706 | `		GenBlock *pFinBlock;` |
|     127 | 10707 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 10708 | `		/* Create the finally block for jump fixup bookkeeping */` |
|     127 | 10709 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     127 | 10710 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10711 | `			return SXERR_ABORT;` |
|       - | 10712 | `		}` |
|       - | 10713 | `		/* Swap bytecode container */` |
|     127 | 10714 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     127 | 10715 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 10716 | `		/* Compile the finally body */` |
|     127 | 10717 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     127 | 10718 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10719 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 10720 | `			return SXERR_ABORT;` |
|       - | 10721 | `		}` |
|       - | 10722 | `		/* Fix forward jumps now the destination is resolved */` |
|     127 | 10723 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10724 | `		/* Emit DONE to terminate the finally block */` |
|     127 | 10725 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10726 | `		/* Leave the block */` |
|     127 | 10727 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10728 | `		/* Restore the default container */` |
|     127 | 10729 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     127 | 10730 | `		pException->iHasFinally = 1;` |
|      61 | 10731 | `	}` |
|       - | 10732 | `	/* Must have at least one catch or finally */` |
|     699 | 10733 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       9 | 10734 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10735 | `			"Cannot use try without catch or finally");` |
|       9 | 10736 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10737 | `			return SXERR_ABORT;` |
|       - | 10738 | `		}` |
|       3 | 10739 | `	}` |
|     699 | 10740 | `	return SXRET_OK;` |
|     352 | 10741 | `}` |
|       - | 10742 | `/*` |
|       - | 10743 | ` * Compile a switch block.` |
|       - | 10744 | ` *  (See block-comment below for more information)` |
|       - | 10745 | ` */` |
|     112 | 10746 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 10747 | `{` |
|     117 | 10748 | `	sxi32 rc = SXRET_OK;` |
|     117 | 10749 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 10750 | `		/* Unexpected token */` |
|     ! 0 | 10751 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10752 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10753 | `			return SXERR_ABORT;` |
|       - | 10754 | `		}` |
|     ! 0 | 10755 | `		pGen->pIn++;` |
|     ! 0 | 10756 | `	}` |
|     117 | 10757 | `	pGen->pIn++;` |
|       - | 10758 | `	/* First instruction to execute in this block. */` |
|     117 | 10759 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10760 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 10761 | `	 * or the '}' token */` |
|     206 | 10762 | `	for(;;){` |
|     417 | 10763 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10764 | `			/* No more input to process */` |
|     ! 0 | 10765 | `			break;` |
|       - | 10766 | `		}` |
|     417 | 10767 | `		rc = SXRET_OK;` |
|     417 | 10768 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 | 10769 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 | 10770 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 10771 | `					/* Unexpected token */` |
|     ! 0 | 10772 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10773 | `						&pGen->pIn->sData);` |
|     ! 0 | 10774 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10775 | `						return SXERR_ABORT;` |
|       - | 10776 | `					}` |
|       - | 10777 | `					/* FALL THROUGH */` |
|     ! 0 | 10778 | `				}` |
|      31 | 10779 | `				rc = SXERR_EOF;` |
|      31 | 10780 | `				break;` |
|       - | 10781 | `			}` |
|      32 | 10782 | `		}else{` |
|       - | 10783 | `			sxi32 nKwrd;` |
|       - | 10784 | `			/* Extract the keyword */` |
|     337 | 10785 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 | 10786 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 | 10787 | `				break;` |
|       - | 10788 | `			}` |
|     253 | 10789 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10790 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 10791 | `					/* Unexpected token */` |
|     ! 0 | 10792 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10793 | `						&pGen->pIn->sData);` |
|     ! 0 | 10794 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10795 | `						return SXERR_ABORT;` |
|       - | 10796 | `					}` |
|       - | 10797 | `					/* FALL THROUGH */` |
|     ! 0 | 10798 | `				}` |
|       - | 10799 | `				/* Block compiled */` |
|       3 | 10800 | `				break;` |
|       - | 10801 | `			}` |
|       - | 10802 | `		}` |
|       - | 10803 | `		/* Compile block */` |
|     305 | 10804 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 | 10805 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10806 | `			return SXERR_ABORT;` |
|       - | 10807 | `		}` |
|       5 | 10808 | `	}` |
|     117 | 10809 | `	return rc;` |
|      61 | 10810 | `}` |
|       - | 10811 | `/*` |
|       - | 10812 | ` * Compile a case eXpression.` |
|       - | 10813 | ` *  (See block-comment below for more information)` |
|       - | 10814 | ` */` |
|      92 | 10815 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 10816 | `{` |
|       - | 10817 | `	SySet *pInstrContainer;` |
|       - | 10818 | `	SyToken *pEnd,*pTmp;` |
|      97 | 10819 | `	sxi32 iNest = 0;` |
|       - | 10820 | `	sxi32 rc;` |
|       - | 10821 | `	/* Delimit the expression */` |
|      97 | 10822 | `	pEnd = pGen->pIn;` |
|     197 | 10823 | `	while( pEnd < pGen->pEnd ){` |
|     197 | 10824 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 10825 | `			/* Increment nesting level */` |
|       3 | 10826 | `			iNest++;` |
|     196 | 10827 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 10828 | `			/* Decrement nesting level */` |
|       3 | 10829 | `			iNest--;` |
|     194 | 10830 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 | 10831 | `			break;` |
|       - | 10832 | `		}` |
|     105 | 10833 | `		pEnd++;` |
|       5 | 10834 | `	}` |
|      97 | 10835 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 10836 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 10837 | `		if( rc == SXERR_ABORT ){` |
|       - | 10838 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10839 | `			return SXERR_ABORT;` |
|       - | 10840 | `		}` |
|     ! 0 | 10841 | `	}` |
|       - | 10842 | `	/* Swap token stream */` |
|      97 | 10843 | `	pTmp = pGen->pEnd;` |
|      97 | 10844 | `	pGen->pEnd = pEnd;` |
|      97 | 10845 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 | 10846 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 | 10847 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 10848 | `	/* Emit the done instruction */` |
|      97 | 10849 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 | 10850 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10851 | `	/* Update token stream */` |
|      97 | 10852 | `	pGen->pIn  = pEnd;` |
|      97 | 10853 | `	pGen->pEnd = pTmp;` |
|      97 | 10854 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10855 | `		return SXERR_ABORT;` |
|       - | 10856 | `	}` |
|      97 | 10857 | `	return SXRET_OK;` |
|      51 | 10858 | `}` |
|       - | 10859 | `/*` |
|       - | 10860 | ` * Compile the smart switch statement.` |
|       - | 10861 | ` * According to the PHP language reference manual` |
|       - | 10862 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 10863 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 10864 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 10865 | ` *  This is exactly what the switch statement is for.` |
|       - | 10866 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 10867 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 10868 | ` *  of the outer loop, use continue 2.` |
|       - | 10869 | ` *  Note that switch/case does loose comparision.` |
|       - | 10870 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 10871 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 10872 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 10873 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 10874 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 10875 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 10876 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 10877 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 10878 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 10879 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 10880 | ` *  list for the next case.` |
|       - | 10881 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 10882 | ` *  or floating-point numbers and strings.` |
|       - | 10883 | ` */` |
|      28 | 10884 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 10885 | `{` |
|       - | 10886 | `	GenBlock *pSwitchBlock;` |
|       - | 10887 | `	SyToken *pTmp,*pEnd;` |
|       - | 10888 | `	ph7_switch *pSwitch;` |
|       - | 10889 | `	sxu32 nToken;` |
|       - | 10890 | `	sxu32 nLine;` |
|       - | 10891 | `	sxi32 rc;` |
|      33 | 10892 | `	nLine = pGen->pIn->nLine;` |
|       - | 10893 | `	/* Jump the 'switch' keyword */` |
|      33 | 10894 | `	pGen->pIn++;` |
|      33 | 10895 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 10896 | `		/* Syntax error */` |
|     ! 0 | 10897 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 10898 | `		if( rc == SXERR_ABORT ){` |
|       - | 10899 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10900 | `			return SXERR_ABORT;` |
|       - | 10901 | `		}` |
|     ! 0 | 10902 | `		goto Synchronize;` |
|       - | 10903 | `	}` |
|       - | 10904 | `	/* Jump the left parenthesis '(' */` |
|      33 | 10905 | `	pGen->pIn++;` |
|      33 | 10906 | `	pEnd = 0; /* cc warning */` |
|       - | 10907 | `	/* Create the loop block */` |
|      47 | 10908 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 10909 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 | 10910 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10911 | `		return SXERR_ABORT;` |
|       - | 10912 | `	}` |
|       - | 10913 | `	/* Delimit the condition */` |
|      33 | 10914 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 | 10915 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 10916 | `		/* Empty expression */` |
|     ! 0 | 10917 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 10918 | `		if( rc == SXERR_ABORT ){` |
|       - | 10919 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10920 | `			return SXERR_ABORT;` |
|       - | 10921 | `		}` |
|     ! 0 | 10922 | `	}` |
|       - | 10923 | `	/* Swap token streams */` |
|      33 | 10924 | `	pTmp = pGen->pEnd;` |
|      33 | 10925 | `	pGen->pEnd = pEnd;` |
|       - | 10926 | `	/* Compile the expression */` |
|      33 | 10927 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 | 10928 | `	if( rc == SXERR_ABORT ){` |
|       - | 10929 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 10930 | `		return SXERR_ABORT;` |
|       - | 10931 | `	}` |
|       - | 10932 | `	/* Update token stream */` |
|      33 | 10933 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 10934 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 10935 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10936 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10937 | `			return SXERR_ABORT;` |
|       - | 10938 | `		}` |
|     ! 0 | 10939 | `		pGen->pIn++;` |
|     ! 0 | 10940 | `	}` |
|      33 | 10941 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 10942 | `	pGen->pEnd = pTmp;` |
|      33 | 10943 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 10944 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 10945 | `			pTmp = pGen->pIn;` |
|     ! 0 | 10946 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 10947 | `				pTmp--;` |
|     ! 0 | 10948 | `			}` |
|       - | 10949 | `			/* Unexpected token */` |
|     ! 0 | 10950 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 10951 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10952 | `				return SXERR_ABORT;` |
|       - | 10953 | `			}` |
|     ! 0 | 10954 | `			goto Synchronize;` |
|       - | 10955 | `	}` |
|       - | 10956 | `	/* Set the delimiter token */` |
|      33 | 10957 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 10958 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 10959 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 10960 | `	}else{` |
|      31 | 10961 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 10962 | `	}` |
|      33 | 10963 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 10964 | `	/* Create the switch blocks container */` |
|      33 | 10965 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 10966 | `	if( pSwitch == 0 ){` |
|       - | 10967 | `		/* Abort compilation */` |
|     ! 0 | 10968 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10969 | `		return SXERR_ABORT;` |
|       - | 10970 | `	}` |
|       - | 10971 | `	/* Zero the structure */` |
|      33 | 10972 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 10973 | `	/* Initialize fields */` |
|      33 | 10974 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 10975 | `	/* Emit the switch instruction */` |
|      33 | 10976 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 10977 | `	/* Compile case blocks */` |
|     100 | 10978 | `	for(;;){` |
|       - | 10979 | `		sxu32 nKwrd;` |
|     119 | 10980 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10981 | `			/* No more input to process */` |
|     ! 0 | 10982 | `			break;` |
|       - | 10983 | `		}` |
|     119 | 10984 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 10985 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 10986 | `				/* Unexpected token */` |
|     ! 0 | 10987 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10988 | `					&pGen->pIn->sData);` |
|     ! 0 | 10989 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10990 | `					return SXERR_ABORT;` |
|       - | 10991 | `				}` |
|       - | 10992 | `				/* FALL THROUGH */` |
|     ! 0 | 10993 | `			}` |
|       - | 10994 | `			/* Block compiled */` |
|     ! 0 | 10995 | `			break;` |
|       - | 10996 | `		}` |
|       - | 10997 | `		/* Extract the keyword */` |
|     119 | 10998 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 10999 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 11000 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 11001 | `				/* Unexpected token */` |
|     ! 0 | 11002 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11003 | `					&pGen->pIn->sData);` |
|     ! 0 | 11004 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11005 | `					return SXERR_ABORT;` |
|       - | 11006 | `				}` |
|       - | 11007 | `				/* FALL THROUGH */` |
|     ! 0 | 11008 | `			}` |
|       - | 11009 | `			/* Block compiled */` |
|       3 | 11010 | `			break;` |
|       - | 11011 | `		}` |
|     117 | 11012 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 11013 | `			/*` |
|       - | 11014 | `			 * Accroding to the PHP language reference manual` |
|       - | 11015 | `			 *  A special case is the default case. This case matches anything` |
|       - | 11016 | `			 *  that wasn't matched by the other cases.` |
|       - | 11017 | `			 */` |
|      25 | 11018 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 11019 | `				/* Default case already compiled */` |
|     ! 0 | 11020 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 11021 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11022 | `					return SXERR_ABORT;` |
|       - | 11023 | `				}` |
|     ! 0 | 11024 | `			}` |
|      25 | 11025 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 11026 | `			/* Compile the default block */` |
|      25 | 11027 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 11028 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 11029 | `				return SXERR_ABORT;` |
|      25 | 11030 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 11031 | `				break;` |
|       1 | 11032 | `			}` |
|      98 | 11033 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 11034 | `			ph7_case_expr sCase;` |
|       - | 11035 | `			/* Standard case block */` |
|      97 | 11036 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 11037 | `			/* initialize the structure */` |
|      97 | 11038 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 11039 | `			/* Compile the case expression */` |
|      97 | 11040 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 11041 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11042 | `				return SXERR_ABORT;` |
|       - | 11043 | `			}` |
|       - | 11044 | `			/* Compile the case block */` |
|      97 | 11045 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 11046 | `			/* Insert in the switch container */` |
|      97 | 11047 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 11048 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 11049 | `				return SXERR_ABORT;` |
|      97 | 11050 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 11051 | `				break;` |
|       - | 11052 | `			}` |
|      47 | 11053 | `		}else{` |
|       - | 11054 | `			/* Unexpected token */` |
|     ! 0 | 11055 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11056 | `				&pGen->pIn->sData);` |
|     ! 0 | 11057 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11058 | `				return SXERR_ABORT;` |
|       - | 11059 | `			}` |
|     ! 0 | 11060 | `			break;` |
|       - | 11061 | `		}` |
|       5 | 11062 | `	}` |
|       - | 11063 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 11064 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 11065 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11066 | `	/* Release the loop block */` |
|      33 | 11067 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 11068 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 11069 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 11070 | `		pGen->pIn++;` |
|      14 | 11071 | `	}` |
|       - | 11072 | `	/* Statement successfully compiled */` |
|      33 | 11073 | `	return SXRET_OK;` |
|     ! 0 | 11074 | `Synchronize:` |
|       - | 11075 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 11076 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 11077 | `		pGen->pIn++;` |
|     ! 0 | 11078 | `	}` |
|     ! 0 | 11079 | `	return SXRET_OK;` |
|      19 | 11080 | `}` |
|       - | 11081 | `/*` |
|       - | 11082 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 11083 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 11084 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 11085 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 11086 | ` */` |
|       - | 11087 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 11088 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 11089 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 11090 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 11091 |  |
|       - | 11092 | `/*` |
|       - | 11093 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 11094 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 11095 | ` * patched entries from the pending set.` |
|       - | 11096 | ` */` |
| 2707886 | 11097 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 11098 | `{` |
| 2707891 | 11099 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 11100 | `	sxu32 nTarget;` |
|       - | 11101 | `	sxu32 *aIdx;` |
|       - | 11102 | `	sxu32 i;` |
| 2707891 | 11103 | `	if( nCur <= nBaseline ){` |
| 2707797 | 11104 | `		return;` |
|       - | 11105 | `	}` |
|      98 | 11106 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      98 | 11107 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     200 | 11108 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     106 | 11109 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     106 | 11110 | `		if( pInstr ){` |
|     106 | 11111 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      51 | 11112 | `		}` |
|      55 | 11113 | `	}` |
|      98 | 11114 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1353948 | 11115 | `}` |
|       - | 11116 |  |
|       - | 11117 | `/*` |
|       - | 11118 | ` * By-reference out-parameters of builtin functions.` |
|       - | 11119 | ` *` |
|       - | 11120 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 11121 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 11122 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 11123 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 11124 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 11125 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 11126 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 11127 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 11128 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 11129 | ` * creates it" behaviour).` |
|       - | 11130 | ` *` |
|       - | 11131 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 11132 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 11133 | ` */` |
|  455120 | 11134 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 11135 | `{` |
|       - | 11136 | `	static const struct {` |
|       - | 11137 | `		const char *zName;` |
|       - | 11138 | `		sxu32 nByte;` |
|       - | 11139 | `		sxu32 mask;` |
|       - | 11140 | `	} aByRef[] = {` |
|       - | 11141 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 11142 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 11143 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 11144 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 11145 | `	};` |
|       - | 11146 | `	sxu32 i;` |
|  455125 | 11147 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1743 | 11148 | `		return 0;` |
|       - | 11149 | `	}` |
| 2266643 | 11150 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1813350 | 11151 | `		if( pName->nByte == aByRef[i].nByte` |
|  929544 | 11152 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      99 | 11153 | `			return aByRef[i].mask;` |
|       - | 11154 | `		}` |
|  906633 | 11155 | `	}` |
|  453293 | 11156 | `	return 0;` |
|  227565 | 11157 | `}` |
|       - | 11158 | `/*` |
|       - | 11159 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 11160 | ` *` |
|       - | 11161 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 11162 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 11163 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 11164 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 11165 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 11166 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 11167 | ` */` |
|  455120 | 11168 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 11169 | `{` |
|       - | 11170 | `	SyToken *p, *pEnd;` |
|  455125 | 11171 | `	pOut->zString = 0;` |
|  455125 | 11172 | `	pOut->nByte = 0;` |
|  455125 | 11173 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 11174 | `		return;` |
|       - | 11175 | `	}` |
|  455125 | 11176 | `	p = pLeft->pStart;` |
|  455125 | 11177 | `	pEnd = pLeft->pEnd;` |
|       - | 11178 | `	/* Optional single leading namespace separator (absolute path). */` |
|  455125 | 11179 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|    3671 | 11180 | `		p++;` |
|    1833 | 11181 | `	}` |
|  455125 | 11182 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1715 | 11183 | `		return;` |
|       - | 11184 | `	}` |
|       - | 11185 | `	/* Must be a single component: nothing follows the name token. */` |
|  453415 | 11186 | `	if( p + 1 != pEnd ){` |
|      32 | 11187 | `		return;` |
|       - | 11188 | `	}` |
|  453387 | 11189 | `	*pOut = p->sData;` |
|  227565 | 11190 | `}` |
|       - | 11191 | `/*` |
|       - | 11192 | ` * Generate bytecode for a given expression tree.` |
|       - | 11193 | ` * If something goes wrong while generating bytecode` |
|       - | 11194 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 11195 | ` * this function takes care of generating the appropriate` |
|       - | 11196 | ` * error message.` |
|       - | 11197 | ` */` |
| 3623990 | 11198 | `static sxi32 GenStateEmitExprCode(` |
|       - | 11199 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11200 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 11201 | `	sxi32 iFlags /* Control flags */` |
|       - | 11202 | `	)` |
|       5 | 11203 | `{` |
|       - | 11204 | `	VmInstr *pInstr;` |
|       - | 11205 | `	sxu32 nJmpIdx;` |
| 3623995 | 11206 | `	sxi32 iP1 = 0;` |
| 3623995 | 11207 | `	sxu32 iP2 = 0;` |
| 3623995 | 11208 | `	void *p3  = 0;` |
|       - | 11209 | `	sxi32 iVmOp;` |
|       - | 11210 | `	sxi32 rc;` |
| 3623995 | 11211 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3623995 | 11212 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 3623995 | 11213 | `	sxu32 nRhsNsBase = 0;` |
| 3623995 | 11214 | `	if( pNode->xCode ){` |
|       - | 11215 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 11216 | `		/* Compile node */` |
| 2262191 | 11217 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2262191 | 11218 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2262191 | 11219 | `		RE_SWAP_DELIMITER(pGen);` |
| 2262191 | 11220 | `		return rc;` |
|       - | 11221 | `	}` |
| 1361809 | 11222 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 11223 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 11224 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 11225 | `		return SXERR_ABORT;` |
|       - | 11226 | `	}` |
| 1361809 | 11227 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1361809 | 11228 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      65 | 11229 | `		sxu32 nJmp = 0;` |
|       - | 11230 | `		sxu32 nNcNsBase;` |
|       - | 11231 | `		VmInstr *pInstrFix;` |
|       - | 11232 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 11233 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 11234 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 11235 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 11236 | `		 * stack slot carries a writable nIdx. */` |
|      65 | 11237 | `		if( pNode->pRight ){` |
|      65 | 11238 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 11239 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|      65 | 11240 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11241 | `				return rc;` |
|       - | 11242 | `			}` |
|      65 | 11243 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 11244 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 11245 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 11246 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 11247 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 11248 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 11249 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 11250 | `			 * cascade for the actual write path stays correct. */` |
|      65 | 11251 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 11252 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      31 | 11253 | `				pInstrFix->iP2 = 3;` |
|      14 | 11254 | `			}` |
|      31 | 11255 | `		}` |
|       - | 11256 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      65 | 11257 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 11258 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      65 | 11259 | `		if( pNode->pLeft ){` |
|      65 | 11260 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 11261 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      65 | 11262 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11263 | `				return rc;` |
|       - | 11264 | `			}` |
|      65 | 11265 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      31 | 11266 | `		}` |
|       - | 11267 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      65 | 11268 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 11269 | `		/* Patch the short-circuit jump to land after the store. */` |
|      65 | 11270 | `		if( nJmp > 0 ){` |
|      65 | 11271 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      65 | 11272 | `			if( pInstrFix ){` |
|      65 | 11273 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      31 | 11274 | `			}` |
|      31 | 11275 | `		}` |
|      65 | 11276 | `		return SXRET_OK;` |
|       - | 11277 | `	}` |
| 1361747 | 11278 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 11279 | `		sxu32 nJz,nJmp;` |
|       - | 11280 | `		sxu32 nTernaryNsBase;` |
|       - | 11281 | `		/* Ternary operator require special handling */` |
|       - | 11282 | `		/* Phase#1: Compile the condition */` |
|    2669 | 11283 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2669 | 11284 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2669 | 11285 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11286 | `			return rc;` |
|       - | 11287 | `		}` |
|       - | 11288 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 11289 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 11290 | `		 * condition expression, not leak past the ternary. */` |
|    2669 | 11291 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2669 | 11292 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2669 | 11293 | `		if( pNode->pLeft ){` |
|       - | 11294 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 11295 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2601 | 11296 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 11297 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2601 | 11298 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2601 | 11299 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2601 | 11300 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11301 | `				return rc;` |
|       - | 11302 | `			}` |
|    2601 | 11303 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1303 | 11304 | `		}else{` |
|       - | 11305 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 11306 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 11307 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 11308 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 11309 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 11310 | `		}` |
|       - | 11311 | `		/* Phase#4: Emit the unconditional jump */` |
|    2669 | 11312 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 11313 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2669 | 11314 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2669 | 11315 | `		if( pInstr ){` |
|    2669 | 11316 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1332 | 11317 | `		}` |
|    2669 | 11318 | `		if( !pNode->pLeft ){` |
|       - | 11319 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 11320 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 11321 | `		}` |
|       - | 11322 | `		/* Phase#6: Compile the 'else' expression */` |
|    2669 | 11323 | `		if( pNode->pRight ){` |
|    2669 | 11324 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2669 | 11325 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2669 | 11326 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11327 | `				return rc;` |
|       - | 11328 | `			}` |
|    2669 | 11329 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1332 | 11330 | `		}` |
|    2669 | 11331 | `		if( nJmp > 0 ){` |
|       - | 11332 | `			/* Phase#7: Fix the unconditional jump */` |
|    2669 | 11333 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2669 | 11334 | `			if( pInstr ){` |
|    2669 | 11335 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1332 | 11336 | `			}` |
|    1332 | 11337 | `		}` |
|       - | 11338 | `		/* All done */` |
|    2669 | 11339 | `		return SXRET_OK;` |
|       - | 11340 | `	}` |
| 1359083 | 11341 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 11342 | `	/* Generate code for the left tree */` |
| 1359083 | 11343 | `	if( pNode->pLeft ){` |
| 1359043 | 11344 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1359043 | 11345 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 11346 | `			ph7_expr_node **apNode;` |
|  458911 | 11347 | `			int hasSpread = 0;` |
|  458911 | 11348 | `			int hasNamed = 0;` |
|  458911 | 11349 | `			int bAnySpread = 0;` |
|  458911 | 11350 | `			sxu32 byRefMask = 0;` |
|       - | 11351 | `			sxi32 nArgs;` |
|       - | 11352 | `			sxi32 n;` |
|       - | 11353 | `			/* Recurse and generate bytecodes for function arguments */` |
|  458911 | 11354 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  458911 | 11355 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 11356 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|       - | 11357 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|       - | 11358 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  458911 | 11359 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|      65 | 11360 | `				bFcc = 1;` |
|      65 | 11361 | `				nArgs = 0;` |
|      32 | 11362 | `			}` |
|       - | 11363 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 11364 | `			{` |
|  458911 | 11365 | `				int seenNamed = 0;` |
|  931085 | 11366 | `				for( n = 0; n < nArgs; ++n ){` |
|  472181 | 11367 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     216 | 11368 | `						seenNamed = 1;` |
|     216 | 11369 | `						hasNamed = 1;` |
|  472075 | 11370 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|    3673 | 11371 | `						bAnySpread = 1;` |
|  470135 | 11372 | `					}else if( seenNamed ){` |
|       3 | 11373 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 11374 | `							"Cannot use positional argument after named argument");` |
|       3 | 11375 | `						return SXERR_SYNTAX;` |
|       - | 11376 | `					}` |
|  236092 | 11377 | `				}` |
|       - | 11378 | `			}` |
|       - | 11379 | `			/* Read-only load */` |
|  458909 | 11380 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 11381 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 11382 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 11383 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 11384 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  458909 | 11385 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  458909 | 11386 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  458904 | 11387 | `				if( pCallName->nByte == 5` |
|  250535 | 11388 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   22197 | 11389 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  447813 | 11390 | `				}else if( pCallName->nByte == 5` |
|  228343 | 11391 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      91 | 11392 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      43 | 11393 | `				}` |
|       - | 11394 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 11395 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 11396 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 11397 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 11398 | `				 * the compile-time positional index no longer maps to the` |
|       - | 11399 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  458909 | 11400 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 11401 | `					SyString sBuiltin;` |
|  455125 | 11402 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  455125 | 11403 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  227560 | 11404 | `				}` |
|  229452 | 11405 | `			}` |
|  931081 | 11406 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  472177 | 11407 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  472177 | 11408 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11409 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 11410 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|       - | 11411 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|       - | 11412 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|       - | 11413 | `				 * builtin to write back through. A plain $var target is unaffected` |
|       - | 11414 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  472177 | 11415 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      55 | 11416 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      55 | 11417 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      25 | 11418 | `				}` |
|  472177 | 11419 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  472177 | 11420 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11421 | `					return rc;` |
|       - | 11422 | `				}` |
|       - | 11423 | `				/* Each argument is an independent nullsafe scope. */` |
|  472177 | 11424 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  472177 | 11425 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 11426 | `					/* Emit spread opcode to unpack this array argument */` |
|    3673 | 11427 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|    3673 | 11428 | `					hasSpread = 1;` |
|    1834 | 11429 | `				}` |
|  236091 | 11430 | `			}` |
|       - | 11431 | `			/* Total number of given arguments */` |
|  458909 | 11432 | `			iP1 = nArgs;` |
|  458909 | 11433 | `			iP2 = hasSpread;` |
|       - | 11434 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 11435 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  458909 | 11436 | `			if( hasNamed ){` |
|     119 | 11437 | `				sxu32 nStrBytes = 0;` |
|       - | 11438 | `				char *zBuf;` |
|     347 | 11439 | `				for( n = 0; n < nArgs; ++n ){` |
|     231 | 11440 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     213 | 11441 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|     105 | 11442 | `					}` |
|     117 | 11443 | `				}` |
|       - | 11444 | `				{` |
|     119 | 11445 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     119 | 11446 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|     116 | 11447 | `					&pGen->pVm->sAllocator, mapSize);` |
|     119 | 11448 | `				if( pMap ){` |
|     119 | 11449 | `					SyZero(pMap, mapSize);` |
|     119 | 11450 | `					pMap->bHasNamed = 1;` |
|     119 | 11451 | `					pMap->nTotal = (sxu32)nArgs;` |
|     119 | 11452 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     119 | 11453 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     347 | 11454 | `					for( n = 0; n < nArgs; ++n ){` |
|     231 | 11455 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     213 | 11456 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     213 | 11457 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     213 | 11458 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     213 | 11459 | `							zBuf += nb;` |
|     105 | 11460 | `						}` |
|       - | 11461 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     117 | 11462 | `					}` |
|     119 | 11463 | `					p3 = (void *)pMap;` |
|      58 | 11464 | `				}` |
|       - | 11465 | `				}` |
|      58 | 11466 | `			}` |
|       - | 11467 | `			/* Remove stale flags now */` |
|  458909 | 11468 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  229452 | 11469 | `		}` |
|       - | 11470 | `		{` |
|       - | 11471 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|       - | 11472 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|       - | 11473 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|       - | 11474 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|       - | 11475 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|       - | 11476 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|       - | 11477 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|       - | 11478 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 1359041 | 11479 | `			sxi32 iLeftFlags = iFlags;` |
| 1359036 | 11480 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
| 1038007 | 11481 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  358514 | 11482 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  350416 | 11483 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   16391 | 11484 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|    8193 | 11485 | `			}` |
|       - | 11486 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|       - | 11487 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|       - | 11488 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|       - | 11489 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|       - | 11490 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|       - | 11491 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|       - | 11492 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 1359036 | 11493 | `			if( pNode->pOp` |
| 1947860 | 11494 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 1268388 | 11495 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 1177689 | 11496 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  181737 | 11497 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   90866 | 11498 | `			}` |
| 1359041 | 11499 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|       - | 11500 | `		}` |
| 1359041 | 11501 | `		if( rc != SXRET_OK ){` |
|      34 | 11502 | `			return rc;` |
|       - | 11503 | `		}` |
| 1359011 | 11504 | `		if( !bIsChainOp ){` |
|       - | 11505 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 11506 | `			 * target the end of that LHS chain, which is right here. */` |
|  624665 | 11507 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  312330 | 11508 | `		}` |
| 1359011 | 11509 | `		if( iVmOp == PH7_OP_CALL ){` |
|  458909 | 11510 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  458909 | 11511 | `			if( pInstr ){` |
|  458909 | 11512 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  453509 | 11513 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 11514 | `					sxu32 nQual;` |
|  453509 | 11515 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11516 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 11517 | `					 * so the later NEW handler (if any) can see it. */` |
|  453509 | 11518 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 11519 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 11520 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 11521 | `					 * imports — class imports must NOT affect function` |
|       - | 11522 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 11523 | `					 * before NEW; we store the original literal index in the` |
|       - | 11524 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 11525 | `					 * the unqualified name and re-qualify with class imports. */` |
|  453509 | 11526 | `					if( bAbsolute ){` |
|    3671 | 11527 | `						pInstr->iP2 = (sxi32)nOrig;` |
|    1838 | 11528 | `					}else{` |
|  449843 | 11529 | `						int fromImport = 0;` |
|  449843 | 11530 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  449843 | 11531 | `						pInstr->iP2 = (sxi32)nQual;` |
|  449843 | 11532 | `						if( nQual != nOrig ){` |
|       - | 11533 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 11534 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 11535 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 11536 | `							if( !fromImport ){` |
|       - | 11537 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 11538 | `								if( p3 == 0 ){` |
|      67 | 11539 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 11540 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 11541 | `									if( pMap ){` |
|      67 | 11542 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 11543 | `										p3 = (void *)pMap;` |
|      31 | 11544 | `									}` |
|      31 | 11545 | `								}` |
|      67 | 11546 | `								if( p3 ){` |
|      67 | 11547 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 11548 | `								}` |
|      31 | 11549 | `							}` |
|      36 | 11550 | `						}` |
|       5 | 11551 | `					}` |
|  232157 | 11552 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 11553 | `					/* Method call,flag that */` |
|    1323 | 11554 | `					pInstr->iP2 = 1;` |
|     659 | 11555 | `				}` |
|  229457 | 11556 | `			}` |
| 1129559 | 11557 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 11558 | `			ph7_expr_node **apNode;` |
|       - | 11559 | `			sxi32 n;` |
|   93715 | 11560 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 11561 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 11562 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11563 | `			/* Recurse and generate bytecodes for array index */` |
|   93715 | 11564 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  169105 | 11565 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   75395 | 11566 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   75395 | 11567 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   75395 | 11568 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11569 | `					return rc;` |
|       - | 11570 | `				}` |
|       - | 11571 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   75395 | 11572 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   37700 | 11573 | `			}` |
|   93715 | 11574 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   75395 | 11575 | `				iP1 = 1; /* Node have an index associated with it */` |
|   37695 | 11576 | `			}` |
|   93715 | 11577 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 11578 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     243 | 11579 | `				iP2 = 4;` |
|   93596 | 11580 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 11581 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 11582 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      56 | 11583 | `				iP2 = 5;` |
|   93451 | 11584 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 11585 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 11586 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 11587 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 11588 | `				iP2 = 6;` |
|   93413 | 11589 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 11590 | `				/* Create an empty entry when the desired index is not found */` |
|   36947 | 11591 | `				iP2 = 1;` |
|   18476 | 11592 | `			}` |
|  853252 | 11593 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 11594 | `			/* POP the left node */` |
|      32 | 11595 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 11596 | `		}` |
|  679503 | 11597 | `	}` |
| 1359051 | 11598 | `	rc = SXRET_OK;` |
| 1359051 | 11599 | `	nJmpIdx = 0;` |
|       - | 11600 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 11601 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 11602 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1359051 | 11603 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     377 | 11604 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     377 | 11605 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     377 | 11606 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     377 | 11607 | `			int isSpecial = 0;` |
|     377 | 11608 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     281 | 11609 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     281 | 11610 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     276 | 11611 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     276 | 11612 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     132 | 11613 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      98 | 11614 | `					isSpecial = 1;` |
|      47 | 11615 | `				}` |
|     162 | 11616 | `			}` |
|     425 | 11617 | `			pInstr->iP1 = 0;` |
|     425 | 11618 | `			if( !isSpecial ){` |
|     235 | 11619 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     115 | 11620 | `			}` |
|       - | 11621 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 11622 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     329 | 11623 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     235 | 11624 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     235 | 11625 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      46 | 11626 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      48 | 11627 | `					return SXRET_OK;` |
|       - | 11628 | `				}` |
|      93 | 11629 | `			}` |
|     140 | 11630 | `		}` |
|     221 | 11631 | `	}` |
|       - | 11632 | `	/* Generate code for the right tree */` |
| 1358969 | 11633 | `	if( pNode->pRight ){` |
|  733413 | 11634 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 11635 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   11439 | 11636 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  727696 | 11637 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 11638 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3827 | 11639 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  720068 | 11640 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 11641 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 11642 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 11643 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  718146 | 11644 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 11645 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 11646 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 11647 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 11648 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 11649 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 11650 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     106 | 11651 | `			sxu32 nNsJmp = 0;` |
|     106 | 11652 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     106 | 11653 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  717982 | 11654 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|       - | 11655 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|       - | 11656 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|       - | 11657 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  305123 | 11658 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  152559 | 11659 | `		}` |
|  733413 | 11660 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  733413 | 11661 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  733413 | 11662 | `		if( !bIsChainOp ){` |
|       - | 11663 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 11664 | `			 * operator instruction is emitted. */` |
|  551725 | 11665 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  275860 | 11666 | `		}` |
|  733413 | 11667 | `		if( iVmOp == PH7_OP_STORE ){` |
|  301217 | 11668 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  301186 | 11669 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 11670 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 11671 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 11672 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 11673 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 11674 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 11675 | `				 */` |
|      80 | 11676 | `				iVmOp = 0;` |
|  301179 | 11677 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  301141 | 11678 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11679 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   80675 | 11680 | `					iP2 = 1;` |
|   40340 | 11681 | `				}else{` |
|  220471 | 11682 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11683 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   36871 | 11684 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   36871 | 11685 | `						iP1 = pInstr->iP1;` |
|   18438 | 11686 | `					}else{` |
|  183605 | 11687 | `						p3 = pInstr->p3;` |
|       - | 11688 | `					}` |
|       - | 11689 | `					/* POP the last dynamic load instruction */` |
|  220471 | 11690 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 11691 | `				}` |
|  150573 | 11692 | `			}` |
|  582807 | 11693 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      54 | 11694 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      54 | 11695 | `			if( pInstr ){` |
|      54 | 11696 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11697 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 11698 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 11699 | `					 */` |
|      17 | 11700 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      17 | 11701 | `					iP1 = pInstr->iP1;` |
|      17 | 11702 | `					iP2 = pInstr->iP2;` |
|      17 | 11703 | `					p3  = pInstr->p3;` |
|       9 | 11704 | `				}else{` |
|      38 | 11705 | `					p3 = pInstr->p3;` |
|       - | 11706 | `				}` |
|      26 | 11707 | `			}` |
|      26 | 11708 | `		}` |
|  366704 | 11709 | `	}` |
| 1358964 | 11710 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   11868 | 11711 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|       - | 11712 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|       - | 11713 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|      30 | 11714 | `		iVmOp = 0;` |
|      13 | 11715 | `	}` |
| 1358969 | 11716 | `	if( iVmOp > 0 ){` |
| 1358713 | 11717 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   14975 | 11718 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 11719 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|   10957 | 11720 | `				iP1 = 1;` |
|    5481 | 11721 | `			}` |
| 1351228 | 11722 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 11723 | `			/* Namespace-qualify the class name for NEW */ {` |
|   23487 | 11724 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   23487 | 11725 | `				VmInstr *pCallInstr = 0;` |
|   23487 | 11726 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   23295 | 11727 | `					pCallInstr = pPeek;` |
|   23295 | 11728 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   11645 | 11729 | `				}` |
|   23487 | 11730 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   23485 | 11731 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11732 | `					sxu32 nLitForClass;` |
|       - | 11733 | `					/* If the CALL handler already qualified the name using` |
|       - | 11734 | `					 * function imports, recover the original unqualified` |
|       - | 11735 | `					 * literal so we can re-qualify with class imports. */` |
|   23485 | 11736 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 11737 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 11738 | `					}else{` |
|   23453 | 11739 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 11740 | `					}` |
|   23485 | 11741 | `					pPeek->iP1 = 0;` |
|   23485 | 11742 | `					if( !bAbsolute ){` |
|   19823 | 11743 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    9914 | 11744 | `					}else{` |
|    3667 | 11745 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 11746 | `					}` |
|   11740 | 11747 | `				}` |
|       - | 11748 | `			}` |
|   23487 | 11749 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   23487 | 11750 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 11751 | `				VmInstr *pPrev;` |
|   23295 | 11752 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   23295 | 11753 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 11754 | `					/* Pop the call instruction, preserve named-arg map */` |
|   23295 | 11755 | `					iP1 = pInstr->iP1;` |
|   23295 | 11756 | `					if( pInstr->p3 ){` |
|      43 | 11757 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 11758 | `					}` |
|   23295 | 11759 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   11645 | 11760 | `				}` |
|   11650 | 11761 | `			}` |
| 1332002 | 11762 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 11763 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 11764 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     201 | 11765 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     201 | 11766 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     201 | 11767 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     201 | 11768 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     201 | 11769 | `				int isSpecialIs = 0;` |
|     201 | 11770 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     197 | 11771 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     197 | 11772 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     192 | 11773 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     197 | 11774 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      97 | 11775 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 11776 | `						isSpecialIs = 1;` |
|       5 | 11777 | `					}` |
|      97 | 11778 | `				}` |
|     203 | 11779 | `				pInstr->iP1 = 0;` |
|     203 | 11780 | `				if( !isSpecialIs && !bAbsolute ){` |
|     181 | 11781 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      88 | 11782 | `				}` |
|     102 | 11783 | `			}` |
| 1320166 | 11784 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 11785 | `			/* Prevent constant expansion for member/property names.` |
|       - | 11786 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 11787 | `			 * should not trigger constant lookup. */` |
|  181693 | 11788 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  181693 | 11789 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  181645 | 11790 | `				pInstr->iP1 = 0;` |
|   90820 | 11791 | `			}` |
|  181693 | 11792 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 11793 | `				/* Static member access,remember that */` |
|     295 | 11794 | `				iP1 = 1;` |
|     295 | 11795 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     295 | 11796 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      42 | 11797 | `					p3 = pInstr->p3;` |
|      42 | 11798 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      19 | 11799 | `				}` |
|     145 | 11800 | `			}` |
|       - | 11801 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|       - | 11802 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|       - | 11803 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|       - | 11804 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  181693 | 11805 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  181693 | 11806 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|      30 | 11807 | `					iP2 = PH7_MEMBER_UNSET;` |
|  181679 | 11808 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|      77 | 11809 | `					iP2 = PH7_MEMBER_ISSET;` |
|  181629 | 11810 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|      13 | 11811 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  181587 | 11812 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|       - | 11813 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   80755 | 11814 | `					iP2 = PH7_MEMBER_WRITE;` |
|   40375 | 11815 | `				}` |
|   90844 | 11816 | `			}` |
|   90844 | 11817 | `		}` |
|       - | 11818 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|       - | 11819 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|       - | 11820 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|       - | 11821 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|       - | 11822 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 1358711 | 11823 | `		if( bFcc ){` |
|      65 | 11824 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|      65 | 11825 | `			iP2 = 0;` |
|      65 | 11826 | `			p3 = 0;` |
|      65 | 11827 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 11828 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11829 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|       - | 11830 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|       - | 11831 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|       - | 11832 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|      31 | 11833 | `				void *pMemberName = pInstr->p3;` |
|      31 | 11834 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|      31 | 11835 | `				if( pMemberName ){` |
|       3 | 11836 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       1 | 11837 | `				}` |
|      31 | 11838 | `				iP1 = 2;` |
|      16 | 11839 | `			}else{` |
|      35 | 11840 | `				iP1 = 1;` |
|       - | 11841 | `			}` |
|      32 | 11842 | `		}` |
|       - | 11843 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 11844 | `		 * This is the primary emit path for user-visible calls. */` |
| 1358711 | 11845 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  482327 | 11846 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  241161 | 11847 | `		}` |
|       - | 11848 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1358711 | 11849 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  679353 | 11850 | `	}` |
| 1358967 | 11851 | `	if( nJmpIdx > 0 ){` |
|       - | 11852 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   15385 | 11853 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   15385 | 11854 | `		if( pInstr ){` |
|   15385 | 11855 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    7690 | 11856 | `		}` |
|    7690 | 11857 | `	}` |
| 1358967 | 11858 | `	return rc;` |
| 1811980 | 11859 | `}` |
|       - | 11860 | `/*` |
|       - | 11861 | ` * Compile a PHP expression.` |
|       - | 11862 | ` * According to the PHP language reference manual:` |
|       - | 11863 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 11864 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 11865 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 11866 | ` *  is "anything that has a value".` |
|       - | 11867 | ` * If something goes wrong while compiling the expression,this` |
|       - | 11868 | ` * function takes care of generating the appropriate error` |
|       - | 11869 | ` * message.` |
|       - | 11870 | ` */` |
|  976088 | 11871 | `static sxi32 PH7_CompileExpr(` |
|       - | 11872 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11873 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 11874 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 11875 | `	)` |
|       5 | 11876 | `{` |
|       - | 11877 | `	ph7_expr_node *pRoot;` |
|       - | 11878 | `	SySet sExprNode;` |
|       - | 11879 | `	SyToken *pEnd;` |
|       - | 11880 | `	sxi32 nExpr;` |
|       - | 11881 | `	sxi32 iNest;` |
|       - | 11882 | `	sxi32 rc;` |
|       - | 11883 | `	sxu32 nNullsafeBase;` |
|       - | 11884 | `	/* Initialize worker variables */` |
|  976093 | 11885 | `	nExpr = 0;` |
|  976093 | 11886 | `	pRoot = 0;` |
|       - | 11887 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 11888 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  976093 | 11889 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  976093 | 11890 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  976093 | 11891 | `	SySetAlloc(&sExprNode,0x10);` |
|  976093 | 11892 | `	rc = SXRET_OK;` |
|       - | 11893 | `	/* Delimit the expression */` |
|  976093 | 11894 | `	pEnd = pGen->pIn;` |
|  976093 | 11895 | `	iNest = 0;` |
| 6585183 | 11896 | `	while( pEnd < pGen->pEnd ){` |
| 6248989 | 11897 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11898 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     517 | 11899 | `			iNest++;` |
| 6248733 | 11900 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     525 | 11901 | `			iNest--;` |
| 6248217 | 11902 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  640271 | 11903 | `			if( iNest <= 0 ){` |
|  639899 | 11904 | `				break;` |
|       - | 11905 | `			}` |
|     186 | 11906 | `		}` |
| 5609095 | 11907 | `		pEnd++;` |
|       5 | 11908 | `	}` |
|  976093 | 11909 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   22449 | 11910 | `		SyToken *pEnd2 = pGen->pIn;` |
|   22449 | 11911 | `		iNest = 0;` |
|       - | 11912 | `		/* Stop at the first comma */` |
|   45211 | 11913 | `		while( pEnd2 < pEnd ){` |
|   22773 | 11914 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      71 | 11915 | `				iNest++;` |
|   22740 | 11916 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      71 | 11917 | `				iNest--;` |
|   22674 | 11918 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      57 | 11919 | `				if( iNest <= 0 ){` |
|       7 | 11920 | `					break;` |
|       - | 11921 | `				}` |
|      23 | 11922 | `			}` |
|   22767 | 11923 | `			pEnd2++;` |
|       5 | 11924 | `		}` |
|   22449 | 11925 | `		if( pEnd2 <pEnd ){` |
|       7 | 11926 | `			pEnd = pEnd2;` |
|       3 | 11927 | `		}` |
|   11222 | 11928 | `	}` |
|  976093 | 11929 | `	if( pEnd > pGen->pIn ){` |
|  976083 | 11930 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 11931 | `		/* Swap delimiter */` |
|  976083 | 11932 | `		pGen->pEnd = pEnd;` |
|       - | 11933 | `		/* Try to get an expression tree */` |
|  976083 | 11934 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  976083 | 11935 | `		if( rc == SXRET_OK && pRoot ){` |
|  975901 | 11936 | `			rc = SXRET_OK;` |
|  975901 | 11937 | `			if( xTreeValidator ){` |
|       - | 11938 | `				/* Call the upper layer validator callback */` |
|   29877 | 11939 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   14936 | 11940 | `			}` |
|  975901 | 11941 | `			if( rc != SXERR_ABORT ){` |
|       - | 11942 | `				/* Generate code for the given tree */` |
|  975901 | 11943 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 11944 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 11945 | `				 * expression so they short-circuit to its end. */` |
|  975901 | 11946 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  487948 | 11947 | `			}` |
|  975901 | 11948 | `			nExpr = 1;` |
|  487948 | 11949 | `		}` |
|       - | 11950 | `		/* Release the whole tree */` |
|  976083 | 11951 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 11952 | `		/* Synchronize token stream */` |
|  976083 | 11953 | `		pGen->pEnd = pTmp;` |
|  976083 | 11954 | `		pGen->pIn  = pEnd;` |
|  976083 | 11955 | `		if( rc == SXERR_ABORT ){` |
|      13 | 11956 | `			SySetRelease(&sExprNode);` |
|      13 | 11957 | `			return SXERR_ABORT;` |
|       - | 11958 | `		}` |
|  488034 | 11959 | `	}` |
|  976083 | 11960 | `	SySetRelease(&sExprNode);` |
|  976083 | 11961 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  488049 | 11962 | `}` |
|       - | 11963 | `/*` |
|       - | 11964 | ` * Return a pointer to the node construct handler associated` |
|       - | 11965 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 11966 | ` */` |
|  255512 | 11967 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 11968 | `{` |
|  255517 | 11969 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 11970 | `		/* Numeric literal: Either real or integer */` |
|  128667 | 11971 | `		return PH7_CompileNumLiteral;` |
|  126855 | 11972 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 11973 | `		/* Double quoted string */` |
|   24121 | 11974 | `		return PH7_CompileString;` |
|  102739 | 11975 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 11976 | `		/* Single quoted string */` |
|  102623 | 11977 | `		return PH7_CompileSimpleString;` |
|     121 | 11978 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 11979 | `		/* Heredoc */` |
|      68 | 11980 | `		return PH7_CompileHereDoc;` |
|      57 | 11981 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 11982 | `		/* Nowdoc */` |
|      50 | 11983 | `		return PH7_CompileNowDoc;` |
|       8 | 11984 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 11985 | `		/* Backtick quoted string */` |
|       6 | 11986 | `		return PH7_CompileBacktic;` |
|       - | 11987 | `	}` |
|       3 | 11988 | `	return 0;` |
|  127761 | 11989 | `}` |
|       - | 11990 | `/*` |
|       - | 11991 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 11992 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 11993 | ` * in write context" parse error.` |
|       - | 11994 | ` */` |
|    6866 | 11995 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 11996 | `{` |
|       - | 11997 | `	sxi32 rc;` |
|    6871 | 11998 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6869 | 11999 | `		return SXRET_OK;` |
|       - | 12000 | `	}` |
|       5 | 12001 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 12002 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 12003 | `		"Can't use nullsafe operator in write context");` |
|       3 | 12004 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3438 | 12005 | `}` |
|       - | 12006 | `/*` |
|       - | 12007 | ` * Compile an unset() statement.` |
|       - | 12008 | ` * unset($var, $arr[$key], ...);` |
|       - | 12009 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 12010 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 12011 | ` * parent array before extracting the element to unset.` |
|       - | 12012 | ` */` |
|    2978 | 12013 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 12014 | `{` |
|    2983 | 12015 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2983 | 12016 | `	sxu32 nIdx = 0;` |
|       - | 12017 | `	SyString sName;` |
|       - | 12018 | `	sxi32 rc;` |
|       - | 12019 | `	/* Jump the 'unset' keyword */` |
|    2983 | 12020 | `	pGen->pIn++;` |
|       - | 12021 | `	/* Save delimiter */` |
|    2983 | 12022 | `	pTmp = pGen->pEnd;` |
|       - | 12023 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2983 | 12024 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2983 | 12025 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 12026 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 12027 | `		SyToken *pClose;` |
|    2983 | 12028 | `		pGen->pIn++;   /* Skip '(' */` |
|    2983 | 12029 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2983 | 12030 | `		pEnd = pClose; /* Stop at ')' */` |
|    1489 | 12031 | `	}` |
|    2983 | 12032 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 12033 | `	/* Resolve the 'unset' builtin name once */` |
|    2983 | 12034 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     365 | 12035 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     365 | 12036 | `		if( pObj == 0 ){` |
|     ! 0 | 12037 | `			return SXERR_ABORT;` |
|       - | 12038 | `		}` |
|     365 | 12039 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     365 | 12040 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     180 | 12041 | `	}` |
|       - | 12042 | `	/* Compile each comma-separated argument */` |
|    9851 | 12043 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6873 | 12044 | `		if( pGen->pIn < pNext ){` |
|    6873 | 12045 | `			pGen->pEnd = pNext;` |
|    6873 | 12046 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 12047 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 12048 | `				GenStateUnsetValidator);` |
|    6873 | 12049 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 12050 | `				return SXERR_ABORT;` |
|       - | 12051 | `			}` |
|    6873 | 12052 | `			if( rc != SXERR_EMPTY ){` |
|       - | 12053 | `				/* Emit call for this single argument */` |
|    6871 | 12054 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6871 | 12055 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6871 | 12056 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3433 | 12057 | `			}` |
|    3434 | 12058 | `		}` |
|       - | 12059 | `		/* Jump trailing commas */` |
|   10765 | 12060 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3897 | 12061 | `			pNext++;` |
|       5 | 12062 | `		}` |
|    6873 | 12063 | `		pGen->pIn = pNext;` |
|       5 | 12064 | `	}` |
|       - | 12065 | `	/* Skip past the closing ')' if present */` |
|    2983 | 12066 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2983 | 12067 | `		pGen->pIn++;` |
|    1489 | 12068 | `	}` |
|       - | 12069 | `	/* Restore token stream */` |
|    2983 | 12070 | `	pGen->pEnd = pTmp;` |
|    2983 | 12071 | `	return SXRET_OK;` |
|    1494 | 12072 | `}` |
|       - | 12073 | `/*` |
|       - | 12074 | ` * PHP Language construct table.` |
|       - | 12075 | ` */` |
|       - | 12076 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 12077 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 12078 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 12079 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 12080 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 12081 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 12082 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 12083 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 12084 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 12085 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 12086 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 12087 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 12088 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 12089 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 12090 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 12091 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 12092 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 12093 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 12094 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 12095 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 12096 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 12097 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 12098 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 12099 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 12100 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 12101 | `};` |
|       - | 12102 | `/*` |
|       - | 12103 | ` * Return a pointer to the statement handler routine associated` |
|       - | 12104 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 12105 | ` */` |
|  654600 | 12106 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 12107 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 12108 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 12109 | `	)` |
|       5 | 12110 | `{` |
|  654605 | 12111 | `	sxu32 n = 0;` |
| 3394390 | 12112 | `	for(;;){` |
| 6788785 | 12113 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  140153 | 12114 | `			break;` |
|       - | 12115 | `		}` |
| 6648637 | 12116 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  514457 | 12117 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 12118 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 12119 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 12120 | `					/* 'static' (class context),return null */` |
|     ! 0 | 12121 | `					return 0;` |
|       - | 12122 | `				}` |
|     ! 0 | 12123 | `			}` |
|  514452 | 12124 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       8 | 12125 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       9 | 12126 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 12127 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 12128 | `				return 0;` |
|       - | 12129 | `			}` |
|       - | 12130 | `			/* Return a pointer to the handler.` |
|       - | 12131 | `			*/` |
|  514457 | 12132 | `			return aLangConstruct[n].xConstruct;` |
|       - | 12133 | `		}` |
| 6134185 | 12134 | `		n++;` |
|       5 | 12135 | `	}` |
|  140153 | 12136 | `	if( pLookahed ){` |
|  140153 | 12137 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   40173 | 12138 | `			return PH7_CompileClassInterface;` |
|   99985 | 12139 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   99607 | 12140 | `			return PH7_CompileClass;` |
|     383 | 12141 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      69 | 12142 | `			return PH7_CompileTrait;` |
|       - | 12143 | `		}` |
|       - | 12144 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 12145 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 12146 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 12147 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     157 | 12148 | `	}` |
|       - | 12149 | `	/* Not a language construct */` |
|     319 | 12150 | `	return 0;` |
|  327305 | 12151 | `}` |
|       - | 12152 | `/*` |
|       - | 12153 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 12154 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 12155 | ` */` |
|     314 | 12156 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 12157 | `{` |
|       - | 12158 | `	int rc;` |
|     319 | 12159 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     319 | 12160 | `	if( rc == FALSE ){` |
|     204 | 12161 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     203 | 12162 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 12163 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 12164 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 12165 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 12166 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 12167 | `			*/` |
|       - | 12168 | `			){` |
|     201 | 12169 | `				rc = TRUE;` |
|      98 | 12170 | `		}` |
|     102 | 12171 | `	}` |
|     319 | 12172 | `	return rc;` |
|       5 | 12173 | `}` |
|       - | 12174 | `/*` |
|       - | 12175 | ` * Compile a PHP chunk.` |
|       - | 12176 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 12177 | ` * takes care of generating the appropriate error message.` |
|       - | 12178 | ` */` |
|  783012 | 12179 | `static sxi32 GenStateCompileChunk(` |
|       - | 12180 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 12181 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 12182 | `	)` |
|       5 | 12183 | `{` |
|       - | 12184 | `	ProcLangConstruct xCons;` |
|       - | 12185 | `	sxi32 rc;` |
|  783017 | 12186 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  612160 | 12187 | `	for(;;){` |
| 1003671 | 12188 | `		int bStmtIsDeclare = 0;` |
| 1003671 | 12189 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 12190 | `			/* No more input to process */` |
|   14435 | 12191 | `			break;` |
|       - | 12192 | `		}` |
|       - | 12193 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 12194 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  989241 | 12195 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  658283 | 12196 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  658283 | 12197 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      45 | 12198 | `				bStmtIsDeclare = 1;` |
|      20 | 12199 | `			}` |
|  329139 | 12200 | `		}` |
|  989241 | 12201 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 12202 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 12203 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  220629 | 12204 | `			pGen->bStrictTypesLocked = 1;` |
|  110312 | 12205 | `		}` |
|  989241 | 12206 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 12207 | `			/* Compile block */` |
|      23 | 12208 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      23 | 12209 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 12210 | `				break;` |
|       - | 12211 | `			}` |
|      14 | 12212 | `		}else{` |
|  989223 | 12213 | `			xCons = 0;` |
|  989223 | 12214 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 12215 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 12216 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 12217 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    3709 | 12218 | `				xCons = PH7_CompileClassModifiers;` |
|  987371 | 12219 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  654605 | 12220 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 12221 | `				/* Try to extract a language construct handler */` |
|  654605 | 12222 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  654605 | 12223 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 12224 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 12225 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 12226 | `						&pGen->pIn->sData);` |
|       9 | 12227 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 12228 | `						break;` |
|       - | 12229 | `					}` |
|       - | 12230 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 12231 | `					 * this erroneous statement.` |
|       - | 12232 | `					 */` |
|       9 | 12233 | `					xCons = PH7_ErrorRecover;` |
|       4 | 12234 | `				}` |
|  658219 | 12235 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   54225 | 12236 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 12237 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 12238 | `				xCons = PH7_CompileLabel;` |
|      56 | 12239 | `			}` |
|  989223 | 12240 | `			if( xCons == 0 ){` |
|       - | 12241 | `				/* Assume an expression an try to compile it */` |
|  331113 | 12242 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  331113 | 12243 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 12244 | `					/* Pop l-value */` |
|  330963 | 12245 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  165479 | 12246 | `				}` |
|  165559 | 12247 | `			}else{` |
|       - | 12248 | `				/* Go compile the sucker */` |
|  658115 | 12249 | `				rc = xCons(&(*pGen));` |
|       - | 12250 | `			}` |
|  989223 | 12251 | `			if( rc == SXERR_ABORT ){` |
|       - | 12252 | `				/* Request to abort compilation */` |
|      13 | 12253 | `				break;` |
|       - | 12254 | `			}` |
|       - | 12255 | `		}` |
|       - | 12256 | `		/* Ignore trailing semi-colons ';' */` |
| 1599401 | 12257 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  610175 | 12258 | `			pGen->pIn++;` |
|       5 | 12259 | `		}` |
|  989231 | 12260 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 12261 | `			/* Compile a single statement and return */` |
|  768577 | 12262 | `			break;` |
|       - | 12263 | `		}` |
|       - | 12264 | `		/* LOOP ONE */` |
|       - | 12265 | `		/* LOOP TWO */` |
|       - | 12266 | `		/* LOOP THREE */` |
|       - | 12267 | `		/* LOOP FOUR */` |
|       5 | 12268 | `	}` |
|       - | 12269 | `	/* Return compilation status */` |
|  783017 | 12270 | `	return rc;` |
|       5 | 12271 | `}` |
|       - | 12272 | `/*` |
|       - | 12273 | ` * Compile a Raw PHP chunk.` |
|       - | 12274 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 12275 | ` * takes care of generating the appropriate error message.` |
|       - | 12276 | ` */` |
|   14442 | 12277 | `static sxi32 PH7_CompilePHP(` |
|       - | 12278 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 12279 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 12280 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 12281 | `	)` |
|       5 | 12282 | `{` |
|   14447 | 12283 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 12284 | `	sxi32 rc;` |
|       - | 12285 | `	/* Reset the token set */` |
|   14447 | 12286 | `	SySetReset(&(*pTokenSet));` |
|       - | 12287 | `	/* Mark as the default token set */` |
|   14447 | 12288 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 12289 | `	/* Advance the stream cursor */` |
|   14447 | 12290 | `	pGen->pRawIn++;` |
|       - | 12291 | `	/* Tokenize the PHP chunk first */` |
|   14447 | 12292 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 12293 | `	/* Point to the head and tail of the token stream. */` |
|   14447 | 12294 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   14447 | 12295 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   14447 | 12296 | `	if( is_expr ){` |
|     ! 0 | 12297 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 12298 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 12299 | `			/* A simple expression,compile it */` |
|     ! 0 | 12300 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 12301 | `		}` |
|       - | 12302 | `		/* Emit the DONE instruction */` |
|     ! 0 | 12303 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 12304 | `		return SXRET_OK;` |
|       - | 12305 | `	}` |
|   14447 | 12306 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 12307 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 12308 | `		/*` |
|       - | 12309 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 12310 | `		 * According to the PHP reference manual:` |
|       - | 12311 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 12312 | `		 *  immediately follow` |
|       - | 12313 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 12314 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 12315 | `		 * Symisc extension:` |
|       - | 12316 | `		 *   This short syntax works with all PHP opening` |
|       - | 12317 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 12318 | `		 *   only short tag.` |
|       - | 12319 | `		 */` |
|       - | 12320 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 12321 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 12322 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 12323 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 12324 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 12325 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 12326 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 12327 | `		}` |
|       3 | 12328 | `		return SXRET_OK;` |
|       - | 12329 | `	}` |
|       - | 12330 | `	/* Compile the PHP chunk */` |
|   14445 | 12331 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 12332 | `	/* Fix exceptions jumps */` |
|   14445 | 12333 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 12334 | `	/* Fix gotos now, the jump destination is resolved */` |
|   14445 | 12335 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 12336 | `		rc = SXERR_ABORT;` |
|       1 | 12337 | `	}` |
|       - | 12338 | `	/* Reset container */` |
|   14445 | 12339 | `	SySetReset(&pGen->aGoto);` |
|   14445 | 12340 | `	SySetReset(&pGen->aLabel);` |
|   14445 | 12341 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 12342 | `	/* Compilation result */` |
|   14445 | 12343 | `	return rc;` |
|    7226 | 12344 | `}` |
|       - | 12345 | `/*` |
|       - | 12346 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 12347 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 12348 | ` * This is the only compile interface exported from this file.` |
|       - | 12349 | ` */` |
|   17448 | 12350 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 12351 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 12352 | `	SyString *pScript,  /* Script to compile */` |
|       - | 12353 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 12354 | `	)` |
|       5 | 12355 | `{` |
|       - | 12356 | `	SySet aPhpToken,aRawToken;` |
|       - | 12357 | `	ph7_gen_state *pCodeGen;` |
|       - | 12358 | `	ph7_value *pRawObj;` |
|       - | 12359 | `	sxu32 nObjIdx;` |
|       - | 12360 | `	sxi32 nRawObj;` |
|       - | 12361 | `	int is_expr;` |
|       - | 12362 | `	sxi8 bSavedStrict;` |
|       - | 12363 | `	sxi8 bSavedStrictLocked;` |
|       - | 12364 | `	sxi32 rc;` |
|   17453 | 12365 | `	if( pScript->nByte < 1 ){` |
|       - | 12366 | `		/* Nothing to compile */` |
|     ! 0 | 12367 | `		return PH7_OK;` |
|       - | 12368 | `	}` |
|       - | 12369 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 12370 | `	 * file's flags so include/require restore them on return. */` |
|   17453 | 12371 | `	pCodeGen = &pVm->sCodeGen;` |
|   17453 | 12372 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   17453 | 12373 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   17453 | 12374 | `	pCodeGen->bStrictTypes = 0;` |
|   17453 | 12375 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 12376 | `	/* Initialize the tokens containers */` |
|   17453 | 12377 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17453 | 12378 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17453 | 12379 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   17453 | 12380 | `	is_expr = 0;` |
|   17453 | 12381 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 12382 | `		SyToken sTmp;` |
|       - | 12383 | `		/* PHP only: -*/` |
|    3717 | 12384 | `		sTmp.nLine = 1;` |
|    3717 | 12385 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3717 | 12386 | `		sTmp.pUserData = 0;` |
|    3717 | 12387 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3717 | 12388 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3717 | 12389 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 12390 | `			/* A simple PHP expression */` |
|     ! 0 | 12391 | `			is_expr = 1;` |
|     ! 0 | 12392 | `		}` |
|    1861 | 12393 | `	}else{` |
|       - | 12394 | `		/* Tokenize raw text */` |
|   13741 | 12395 | `		SySetAlloc(&aRawToken,32);` |
|   13741 | 12396 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 12397 | `	}` |
|       - | 12398 | `	/* Process high-level tokens */` |
|   17453 | 12399 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   17453 | 12400 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   17453 | 12401 | `	rc = PH7_OK;` |
|   17453 | 12402 | `	if( is_expr ){` |
|       - | 12403 | `		/* Compile the expression */` |
|     ! 0 | 12404 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 12405 | `		goto cleanup;` |
|       - | 12406 | `	}` |
|   17453 | 12407 | `	nObjIdx = 0;` |
|       - | 12408 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 12409 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 12410 | `	 * preventing namespace bleeding across include()d files. */` |
|   17453 | 12411 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 12412 | `	/* Start the compilation process */` |
|   15598 | 12413 | `	for(;;){` |
|   45631 | 12414 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   17441 | 12415 | `			break; /* No more tokens to process */` |
|       - | 12416 | `		}` |
|   28195 | 12417 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 12418 | `			/* Compile the PHP chunk */` |
|   14447 | 12419 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   14447 | 12420 | `			if( rc == SXERR_ABORT ){` |
|      15 | 12421 | `				break;` |
|       - | 12422 | `			}` |
|   14435 | 12423 | `			continue;` |
|       - | 12424 | `		}` |
|       - | 12425 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13753 | 12426 | `		nRawObj = 0;` |
|   27543 | 12427 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 12428 | `			/* Consume the raw chunk without any processing */` |
|   13795 | 12429 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13795 | 12430 | `			if( pRawObj == 0 ){` |
|     ! 0 | 12431 | `				rc = SXERR_MEM;` |
|     ! 0 | 12432 | `				break;` |
|       - | 12433 | `			}` |
|       - | 12434 | `			/* Mark as constant and emit the load constant instruction */` |
|   13795 | 12435 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13795 | 12436 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13795 | 12437 | `			++nRawObj;` |
|   13795 | 12438 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 12439 | `		}` |
|   13753 | 12440 | `		if( nRawObj > 0 ){` |
|       - | 12441 | `			/* Emit the consume instruction */` |
|   13753 | 12442 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6874 | 12443 | `		}` |
|    8729 | 12444 | `	}` |
|    8724 | 12445 | `cleanup:` |
|   17453 | 12446 | `	SySetRelease(&aRawToken);` |
|   17453 | 12447 | `	SySetRelease(&aPhpToken);` |
|       - | 12448 | `	/* Restore outer file's strict_types scope */` |
|   17453 | 12449 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   17453 | 12450 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   17453 | 12451 | `	return rc;` |
|    8729 | 12452 | `}` |
|       - | 12453 | `/*` |
|       - | 12454 | ` * Utility routines.Initialize the code generator.` |
|       - | 12455 | ` */` |
|    3644 | 12456 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 12457 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12458 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12459 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12460 | `	)` |
|       5 | 12461 | `{` |
|    3649 | 12462 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12463 | `	/* Zero the structure */` |
|    3649 | 12464 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 12465 | `	/* Initial state */` |
|    3649 | 12466 | `	pGen->pVm  = &(*pVm);` |
|    3649 | 12467 | `	pGen->xErr = xErr;` |
|    3649 | 12468 | `	pGen->pErrData = pErrData;` |
|    3649 | 12469 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3649 | 12470 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3649 | 12471 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3649 | 12472 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3649 | 12473 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 12474 | `	/* Error log buffer */` |
|    3649 | 12475 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 12476 | `	/* General purpose working buffer */` |
|    3649 | 12477 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 12478 | `	/* Namespace state */` |
|    3649 | 12479 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3649 | 12480 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3649 | 12481 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3649 | 12482 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12483 | `	/* Create the global scope */` |
|    3649 | 12484 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 12485 | `	/* Point to the global scope */` |
|    3649 | 12486 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3649 | 12487 | `	return SXRET_OK;` |
|       5 | 12488 | `}` |
|       - | 12489 | `/*` |
|       - | 12490 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 12491 | ` */` |
|   20728 | 12492 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 12493 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12494 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12495 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12496 | `	)` |
|       5 | 12497 | `{` |
|   20733 | 12498 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12499 | `	GenBlock *pBlock,*pParent;` |
|       - | 12500 | `	/* Reset state */` |
|   20733 | 12501 | `	SySetReset(&pGen->aLabel);` |
|   20733 | 12502 | `	SySetReset(&pGen->aGoto);` |
|   20733 | 12503 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   20733 | 12504 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   20733 | 12505 | `	SyBlobRelease(&pGen->sWorker);` |
|   20733 | 12506 | `	SyBlobRelease(&pGen->sNamespace);` |
|   20733 | 12507 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   20733 | 12508 | `	SyHashRelease(&pGen->hUseImports);` |
|   20733 | 12509 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   20733 | 12510 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   20733 | 12511 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   20733 | 12512 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   20733 | 12513 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12514 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 12515 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 12516 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 12517 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 12518 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 12519 | `	 * number of unique names, which is acceptable. */` |
|       - | 12520 | `	/* Point to the global scope */` |
|   20733 | 12521 | `	pBlock = pGen->pCurrent;` |
|   20733 | 12522 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 12523 | `		pParent = pBlock->pParent;` |
|     ! 0 | 12524 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 12525 | `		pBlock = pParent;` |
|     ! 0 | 12526 | `	}` |
|   20733 | 12527 | `	pGen->xErr = xErr;` |
|   20733 | 12528 | `	pGen->pErrData = pErrData;` |
|   20733 | 12529 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   20733 | 12530 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   20733 | 12531 | `	pGen->pIn = pGen->pEnd = 0;` |
|   20733 | 12532 | `	pGen->nErr = 0;` |
|   20733 | 12533 | `	return SXRET_OK;` |
|       5 | 12534 | `}` |
|       - | 12535 | `/*` |
|       - | 12536 | ` * Generate a compile-time error message.` |
|       - | 12537 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 12538 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 12539 | ` * abort compilation immediately.` |
|       - | 12540 | ` */` |
|     632 | 12541 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 12542 | `{` |
|     637 | 12543 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     637 | 12544 | `	const char *zErr = "Error";` |
|       - | 12545 | `	SyString *pFile;` |
|       - | 12546 | `	va_list ap;` |
|       - | 12547 | `	sxi32 rc;` |
|       - | 12548 | `	/* Reset the working buffer */` |
|     637 | 12549 | `	SyBlobReset(pWorker);` |
|       - | 12550 | `	/* Peek the processed file path if available */` |
|     637 | 12551 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     637 | 12552 | `	if( nErrType == E_ERROR ){` |
|       - | 12553 | `		/* Increment the error counter */` |
|     525 | 12554 | `		pGen->nErr++;` |
|     525 | 12555 | `		if( pGen->nErr > 15 ){` |
|       - | 12556 | `			/* Error count limit reached */` |
|       5 | 12557 | `			if( pGen->xErr ){` |
|       5 | 12558 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 12559 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 12560 | `				if( pFile ){` |
|       5 | 12561 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 12562 | `				}` |
|       5 | 12563 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 12564 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 12565 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 12566 | `				}` |
|       2 | 12567 | `			}` |
|       - | 12568 | `			/* Abort immediately */` |
|       5 | 12569 | `			return SXERR_ABORT;` |
|       - | 12570 | `		}` |
|     258 | 12571 | `	}` |
|     633 | 12572 | `	if( pGen->xErr == 0 ){` |
|       - | 12573 | `		/* No available error consumer,return immediately */` |
|       3 | 12574 | `		return SXRET_OK;` |
|       - | 12575 | `	}` |
|     630 | 12576 | `	switch(nErrType){` |
|     518 | 12577 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      30 | 12578 | `	case E_WARNING: zErr = "Warning";     break;` |
|      82 | 12579 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      11 | 12580 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 12581 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 12582 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 12583 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 12584 | `	default:` |
|     ! 0 | 12585 | `		break;` |
|       - | 12586 | `	}` |
|     630 | 12587 | `	rc = SXRET_OK;` |
|       - | 12588 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     630 | 12589 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     630 | 12590 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     630 | 12591 | `	va_start(ap,zFormat);` |
|     630 | 12592 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     630 | 12593 | `	va_end(ap);` |
|     630 | 12594 | `	if( pFile ){` |
|     630 | 12595 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     313 | 12596 | `	}` |
|       - | 12597 | `	/* Append a new line */` |
|     630 | 12598 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     630 | 12599 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 12600 | `		/* Consume the generated error message */` |
|     630 | 12601 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     313 | 12602 | `	}` |
|     630 | 12603 | `	return rc;` |
|     321 | 12604 | `}` |
|       - | 12605 |  |
