# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3183/4253 lines (74.84%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/*` |
|       - |    8 | ` * This file implement a thread-safe and full-reentrant compiler for the PH7 engine.` |
|       - |    9 | ` * That is, routines defined in this file takes a stream of tokens and output` |
|       - |   10 | ` * PH7 bytecode instructions.` |
|       - |   11 | ` */` |
|       - |   12 | `/* Forward declaration */` |
|       - |   13 | `typedef struct LangConstruct LangConstruct;` |
|       - |   14 | `typedef struct JumpFixup     JumpFixup;` |
|       - |   15 | `typedef struct Label         Label;` |
|       - |   16 | `/* Block [i.e: set of statements] control flags */` |
|       - |   17 | `#define GEN_BLOCK_LOOP        0x001    /* Loop block [i.e: for,while,...] */` |
|       - |   18 | `#define GEN_BLOCK_PROTECTED   0x002    /* Protected block */` |
|       - |   19 | `#define GEN_BLOCK_COND        0x004    /* Conditional block [i.e: if(condition){} ]*/` |
|       - |   20 | `#define GEN_BLOCK_FUNC        0x008    /* Function body */` |
|       - |   21 | `#define GEN_BLOCK_GLOBAL      0x010    /* Global block (always set)*/` |
|       - |   22 | `#define GEN_BLOC_NESTED_FUNC  0x020    /* Nested function body */` |
|       - |   23 | `#define GEN_BLOCK_EXPR        0x040    /* Expression */` |
|       - |   24 | `#define GEN_BLOCK_STD         0x080    /* Standard block */` |
|       - |   25 | `#define GEN_BLOCK_EXCEPTION   0x100    /* Exception block [i.e: try{ } }*/` |
|       - |   26 | `#define GEN_BLOCK_SWITCH      0x200    /* Switch statement */` |
|       - |   27 | `/*` |
|       - |   28 | ` * Each label seen in the input is recorded in an instance` |
|       - |   29 | ` * of the following structure.` |
|       - |   30 | ` * A label is a target point [i.e: a jump destination] that is specified` |
|       - |   31 | ` * by an identifier followed by a colon.` |
|       - |   32 | ` * Example` |
|       - |   33 | ` *  LABEL:` |
|       - |   34 | ` *		echo "hello\n";` |
|       - |   35 | ` */` |
|       - |   36 | `struct Label` |
|       - |   37 |  |
|       - |   38 | `	ph7_vm_func *pFunc;  /* Compiled function where the label was declared.NULL otherwise */` |
|       - |   39 | `	sxu32 nJumpDest;     /* Jump destination */` |
|       - |   40 | `	SyString sName;      /* Label name */` |
|       - |   41 | `	sxu32 nLine;         /* Line number this label occurs */` |
|       - |   42 | `	sxu8 bRef;           /* True if the label was referenced */` |
|       - |   43 | `};` |
|       - |   44 | `/*` |
|       - |   45 | ` * Compilation of some PHP constructs such as if, for, while, the logical or` |
|       - |   46 | ` * (\|\|) and logical and (&&) operators in expressions requires the` |
|       - |   47 | ` * generation of forward jumps.` |
|       - |   48 | ` * Since the destination PC target of these jumps isn't known when the jumps` |
|       - |   49 | ` * are emitted, we record each forward jump in an instance of the following` |
|       - |   50 | ` * structure. Those jumps are fixed later when the jump destination is resolved.` |
|       - |   51 | ` */` |
|       - |   52 | `struct JumpFixup` |
|       - |   53 |  |
|       - |   54 | `	sxi32 nJumpType;     /* Jump type. Either TRUE jump, FALSE jump or Unconditional jump */` |
|       - |   55 | `	sxu32 nInstrIdx;     /* Instruction index to fix later when the jump destination is resolved. */` |
|       - |   56 | `	/* The following fields are only used by the goto statement */` |
|       - |   57 | `	SyString sLabel;    /* Label name */` |
|       - |   58 | `	ph7_vm_func *pFunc; /* Compiled function inside which the goto was emitted. NULL otherwise */` |
|       - |   59 | `	sxu32 nLine;        /* Track line number */` |
|       - |   60 | `};` |
|       - |   61 | `/*` |
|       - |   62 | ` * Each language construct is represented by an instance` |
|       - |   63 | ` * of the following structure.` |
|       - |   64 | ` */` |
|       - |   65 | `struct LangConstruct` |
|       - |   66 |  |
|       - |   67 | `	sxu32 nID;                     /* Language construct ID [i.e: PH7_TKWRD_WHILE,PH7_TKWRD_FOR,PH7_TKWRD_IF...] */` |
|       - |   68 | `	ProcLangConstruct xConstruct;  /* C function implementing the language construct */` |
|       - |   69 | `};` |
|       - |   70 | `/* Compilation flags */` |
|       - |   71 | `#define PH7_COMPILE_SINGLE_STMT 0x001 /* Compile a single statement */` |
|       - |   72 | `/* Token stream synchronization macros */` |
|       - |   73 | `#define SWAP_TOKEN_STREAM(GEN,START,END)\` |
|       - |   74 | `	pTmp  = GEN->pEnd;\` |
|       - |   75 | `	pGen->pIn  = START;\` |
|       - |   76 | `	pGen->pEnd = END` |
|       - |   77 | `#define UPDATE_TOKEN_STREAM(GEN)\` |
|       - |   78 | `	if( GEN->pIn < pTmp ){\` |
|       - |   79 | `	    GEN->pIn++;\` |
|       - |   80 | `	}\` |
|       - |   81 | `	GEN->pEnd = pTmp` |
|       - |   82 | `#define SWAP_DELIMITER(GEN,START,END)\` |
|       - |   83 | `	pTmpIn  = GEN->pIn;\` |
|       - |   84 | `	pTmpEnd = GEN->pEnd;\` |
|       - |   85 | `	GEN->pIn = START;\` |
|       - |   86 | `	GEN->pEnd = END` |
|       - |   87 | `#define RE_SWAP_DELIMITER(GEN)\` |
|       - |   88 | `	GEN->pIn  = pTmpIn;\` |
|       - |   89 | `	GEN->pEnd = pTmpEnd` |
|       - |   90 | `/* Flags related to expression compilation */` |
|       - |   91 | `#define EXPR_FLAG_LOAD_IDX_STORE    0x001 /* Set the iP2 flag when dealing with the LOAD_IDX instruction */` |
|       - |   92 | `#define EXPR_FLAG_RDONLY_LOAD       0x002 /* Read-only load, refer to the 'PH7_OP_LOAD' VM instruction for more information */` |
|       - |   93 | `#define EXPR_FLAG_COMMA_STATEMENT   0x004 /* Treat comma expression as a single statement (used by class attributes) */` |
|       - |   94 | `/* Forward declaration */` |
|       - |   95 | `static sxi32 PH7_CompileExpr(ph7_gen_state *pGen,sxi32 iFlags,sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *));` |
|       - |   96 | `/*` |
|       - |   97 | ` * Local utility routines used in the code generation phase.` |
|       - |   98 | ` */` |
|       - |   99 | `/*` |
|       - |  100 | ` * Check if the given name refer to a valid label.` |
|       - |  101 | ` * Return SXRET_OK and write a pointer to that label on success.` |
|       - |  102 | ` * Any other return value indicates no such label.` |
|       - |  103 | ` */` |
|     148 |  104 | `static sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,Label **ppOut)` |
|       2 |  105 |  |
|       - |  106 | `	Label *aLabel;` |
|       - |  107 | `	sxu32 n;` |
|       - |  108 | `	/* Perform a linear scan on the label table */` |
|     150 |  109 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|     330 |  110 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     274 |  111 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|       - |  112 | `			/* Jump destination found */` |
|      94 |  113 | `			aLabel[n].bRef = TRUE;` |
|      94 |  114 | `			if( ppOut ){` |
|      94 |  115 | `				*ppOut = &aLabel[n];` |
|      46 |  116 | `			}` |
|      94 |  117 | `			return SXRET_OK;` |
|       - |  118 | `		}` |
|      92 |  119 | `	}` |
|       - |  120 | `	/* No such destination */` |
|      57 |  121 | `	return SXERR_NOTFOUND;` |
|      76 |  122 |  |
|       - |  123 | `/*` |
|       - |  124 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|       - |  125 | ` * compiled blocks.` |
|       - |  126 | ` * Return a pointer to that block on success. NULL otherwise.` |
|       - |  127 | ` */` |
|    2672 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    2674 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    7501 |  131 | `	for(;;){` |
|   15004 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2562 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2562 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2540 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      11 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   12466 |  140 | `		pBlock = pBlock->pParent;` |
|   12466 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1338 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  426766 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  426768 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  426768 |  162 | `	pBlock->pUserData   = pUserData;` |
|  426768 |  163 | `	pBlock->pGen        = pGen;` |
|  426768 |  164 | `	pBlock->iFlags      = iType;` |
|  426768 |  165 | `	pBlock->pParent     = 0;` |
|  426768 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  426768 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  426768 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  424332 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  424334 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  424334 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  424334 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  424334 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  424334 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  424334 |  200 | `	pGen->pCurrent = pBlock;` |
|  424334 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  203822 |  203 | `		*ppBlock = pBlock;` |
|  101910 |  204 | `	}` |
|  424334 |  205 | `	return SXRET_OK;` |
|  212168 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  424326 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  424328 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  424328 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  424328 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  424326 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  424328 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  424328 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  424328 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  424328 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  424326 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  424328 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  424328 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  424328 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  424328 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  424328 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  424328 |  244 | `	return SXRET_OK;` |
|  212165 |  245 |  |
|       - |  246 | `/*` |
|       - |  247 | ` * Emit a forward jump.` |
|       - |  248 | ` * Notes on forward jumps` |
|       - |  249 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|       - |  250 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|       - |  251 | ` *  generation of forward jumps.` |
|       - |  252 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|       - |  253 | ` *  are emitted, we record each forward jump in an instance of the following` |
|       - |  254 | ` *  structure. Those jumps are fixed later when the jump destination is resolved.` |
|       - |  255 | ` */` |
|  157736 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  157738 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  157738 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  157738 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  157738 |  265 | `	return rc;` |
|       2 |  266 |  |
|       - |  267 | `/*` |
|       - |  268 | ` * Fix a forward jump now the jump destination is resolved.` |
|       - |  269 | ` * Return the total number of fixed jumps.` |
|       - |  270 | ` * Notes on forward jumps:` |
|       - |  271 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|       - |  272 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|       - |  273 | ` *  generation of forward jumps.` |
|       - |  274 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|       - |  275 | ` *  are emitted, we record each forward jump in an instance of the following` |
|       - |  276 | ` *  structure.Those jumps are fixed later when the jump destination is resolved.` |
|       - |  277 | ` */` |
|  322036 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  322038 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  629584 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  307548 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  119824 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  187726 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   29992 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  157736 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  157736 |  298 | `		if( pInstr ){` |
|  157736 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  157736 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  157736 |  302 | `			aFix[n].nJumpType = -1;` |
|   78867 |  303 | `		}` |
|   78869 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  322038 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|   94172 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|   94174 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|   94320 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|     150 |  326 | `		pJump = &aJumps[n];` |
|       - |  327 | `		/* Extract the target label */` |
|     150 |  328 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|     150 |  329 | `		if( rc != SXRET_OK ){` |
|       - |  330 | `			/* No such label */` |
|      57 |  331 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);` |
|      57 |  332 | `			if( rc == SXERR_ABORT ){` |
|       3 |  333 | `				return SXERR_ABORT;` |
|       - |  334 | `			}` |
|      55 |  335 | `			continue;` |
|       - |  336 | `		}` |
|       - |  337 | `		/* Make sure the target label is reachable */` |
|      94 |  338 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|       9 |  339 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|       9 |  340 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  341 | `				return SXERR_ABORT;` |
|       - |  342 | `			}` |
|       4 |  343 | `		}` |
|       - |  344 | `		/* Fix the jump now the destination is resolved */` |
|      94 |  345 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|      94 |  346 | `		if( pInstr ){` |
|      94 |  347 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      46 |  348 | `		}` |
|      48 |  349 | `	}` |
|   94172 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|   94304 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|   94172 |  358 | `	return SXRET_OK;` |
|   47088 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  408388 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  408390 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  408390 |  367 | `	if( pEntry == 0 ){` |
|  178842 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  229550 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  229550 |  371 | `	return SXRET_OK;` |
|  204196 |  372 |  |
|       - |  373 | `/*` |
|       - |  374 | ` * Install a given constant index in the literal table.` |
|       - |  375 | ` * In order to be installed, the ph7_value must be of type string.` |
|       - |  376 | ` *` |
|       - |  377 | ` * NOTE: empty strings are deliberately omitted here.  The VM reserves a` |
|       - |  378 | ` * single shared constant for "" during initialization (pVm->nEmptyStringIdx)` |
|       - |  379 | ` * and the compiler emits a LOADC referencing that slot whenever an empty` |
|       - |  380 | ` * literal is encountered.  This keeps the literal hash from growing when` |
|       - |  381 | ` * many "" literals appear in user code.` |
|       - |  382 | ` */` |
|  178840 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  178842 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  178842 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   89420 |  387 | `	}` |
|  178842 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   79434 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   79436 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   79436 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   79436 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   79436 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   79436 |  408 | `	return pObj;` |
|   39719 |  409 |  |
|       - |  410 | `/*` |
|       - |  411 | ` * Implementation of the PHP language constructs.` |
|       - |  412 | ` */` |
|       - |  413 | `/* Forward declaration */` |
|       - |  414 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|       - |  415 | `/*` |
|       - |  416 | ` * Compile a numeric [i.e: integer or real] literal.` |
|       - |  417 | ` * Notes on the integer type.` |
|       - |  418 | ` *  According to the PHP language reference manual` |
|       - |  419 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|       - |  420 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|       - |  421 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|       - |  422 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|       - |  423 | ` * Symisc eXtension to the integer type.` |
|       - |  424 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|       - |  425 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|       - |  426 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|       - |  427 | ` *  [i.e: either 32bit or 64bit].` |
|       - |  428 | ` *  For more information on this powerfull extension please refer to the official` |
|       - |  429 | ` *  documentation.` |
|       - |  430 | ` */` |
|   79834 |  431 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  432 |  |
|   79836 |  433 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   79836 |  434 | `	sxu32 nIdx = 0;` |
|   79836 |  435 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  436 | `		ph7_value *pObj;` |
|       - |  437 | `		sxi64 iValue;` |
|   79436 |  438 | `		iValue = PH7_TokenValueToInt64(&pToken->sData);` |
|   79436 |  439 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   79436 |  440 | `		if( pObj == 0 ){` |
|     ! 0 |  441 | `			SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  442 | `			return SXERR_ABORT;` |
|       - |  443 | `		}` |
|   79436 |  444 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   39719 |  445 | `	}else{` |
|       - |  446 | `		/* Real number */` |
|       - |  447 | `		ph7_value *pObj;` |
|       - |  448 | `		/* Reserve a new constant */` |
|     402 |  449 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     402 |  450 | `		if( pObj == 0 ){` |
|     ! 0 |  451 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  452 | `			return SXERR_ABORT;` |
|       - |  453 | `		}` |
|     402 |  454 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&pToken->sData);` |
|     402 |  455 | `		PH7_MemObjToReal(pObj);` |
|       - |  456 | `	}` |
|       - |  457 | `	/* Emit the load constant instruction */` |
|   79836 |  458 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  459 | `	/* Node successfully compiled */` |
|   79836 |  460 | `	return SXRET_OK;` |
|   39919 |  461 |  |
|       - |  462 | `/*` |
|       - |  463 | ` * Compile a single quoted string.` |
|       - |  464 | ` * According to the PHP language reference manual:` |
|       - |  465 | ` *` |
|       - |  466 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|       - |  467 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|       - |  468 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|       - |  469 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|       - |  470 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|       - |  471 | ` *` |
|       - |  472 | ` */` |
|   52486 |  473 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  474 |  |
|   52488 |  475 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  476 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  477 | `	ph7_value *pObj;` |
|       - |  478 | `	sxu32 nIdx;` |
|   52488 |  479 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  480 | `	/* Delimit the string */` |
|   52488 |  481 | `	zIn  = pStr->zString;` |
|   52488 |  482 | `	zEnd = &zIn[pStr->nByte];` |
|   52488 |  483 | `	if( zIn >= zEnd ){` |
|       - |  484 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  485 | `		 * rather than reserving a new object each time. */` |
|     112 |  486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     112 |  487 | `		return SXRET_OK;` |
|       - |  488 | `	}` |
|   52378 |  489 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  490 | `		/* Already processed,emit the load constant instruction` |
|       - |  491 | `		 * and return.` |
|       - |  492 | `		 */` |
|   15620 |  493 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   15620 |  494 | `		return SXRET_OK;` |
|       - |  495 | `	}` |
|       - |  496 | `	/* Reserve a new constant */` |
|   36760 |  497 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   36760 |  498 | `	if( pObj == 0 ){` |
|     ! 0 |  499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  500 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  501 | `		return SXERR_ABORT;` |
|       - |  502 | `	}` |
|   36760 |  503 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  504 | `	/* Compile the node */` |
|   36775 |  505 | `	for(;;){` |
|   73552 |  506 | `		if( zIn >= zEnd ){` |
|       - |  507 | `			/* End of input */` |
|   36760 |  508 | `			break;` |
|       - |  509 | `		}` |
|   36794 |  510 | `		zCur = zIn;` |
|  581100 |  511 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  544308 |  512 | `			zIn++;` |
|       2 |  513 | `		}` |
|   36794 |  514 | `		if( zIn > zCur ){` |
|       - |  515 | `			/* Append raw contents*/` |
|   36776 |  516 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   18387 |  517 | `		}` |
|   36794 |  518 | `		zIn++;` |
|   36794 |  519 | `		if( zIn < zEnd ){` |
|      55 |  520 | `			if( zIn[0] == '\\' ){` |
|       - |  521 | `				/* A literal backslash */` |
|      23 |  522 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      44 |  523 | `			}else if( zIn[0] == '\'' ){` |
|       - |  524 | `				/* A single quote */` |
|      11 |  525 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |  526 | `			}else{` |
|       - |  527 | `				/* verbatim copy */` |
|      23 |  528 | `				zIn--;` |
|      23 |  529 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      23 |  530 | `				zIn++;` |
|       - |  531 | `			}` |
|      27 |  532 | `		}` |
|       - |  533 | `		/* Advance the stream cursor */` |
|   36794 |  534 | `		zIn++;` |
|       2 |  535 | `	}` |
|       - |  536 | `	/* Emit the load constant instruction */` |
|   36760 |  537 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   36760 |  538 | `	if( pStr->nByte < 1024 ){` |
|       - |  539 | `		/* Install in the literal table */` |
|   36760 |  540 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   18379 |  541 | `	}` |
|       - |  542 | `	/* Node successfully compiled */` |
|   36760 |  543 | `	return SXRET_OK;` |
|   26245 |  544 |  |
|       - |  545 | `/*` |
|       - |  546 | ` * Compile a nowdoc string.` |
|       - |  547 | ` * According to the PHP language reference manual:` |
|       - |  548 | ` *` |
|       - |  549 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |  550 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |  551 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |  552 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |  553 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |  554 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |  555 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |  556 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |  557 | ` *  of the closing identifier.` |
|       - |  558 | ` */` |
|      28 |  559 | `static sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  560 |  |
|      29 |  561 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  562 | `	ph7_value *pObj;` |
|       - |  563 | `	sxu32 nIdx;` |
|      29 |  564 | `	nIdx = 0; /* Prevent compiler warning */` |
|      29 |  565 | `	if( pStr->nByte <= 0 ){` |
|       - |  566 | `		/* Empty string,load NULL */` |
|     ! 0 |  567 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     ! 0 |  568 | `		return SXRET_OK;` |
|       - |  569 | `	}` |
|       - |  570 | `	/* Reserve a new constant */` |
|      29 |  571 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  572 | `	if( pObj == 0 ){` |
|     ! 0 |  573 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  574 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  575 | `		return SXERR_ABORT;` |
|       - |  576 | `	}` |
|       - |  577 | `	/* No processing is done here, simply a memcpy() operation */` |
|      29 |  578 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |  579 | `	/* Emit the load constant instruction */` |
|      29 |  580 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  581 | `	/* Node successfully compiled */` |
|      29 |  582 | `	return SXRET_OK;` |
|      15 |  583 |  |
|       - |  584 | `/*` |
|       - |  585 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |  586 | ` * According to the PHP language reference manual` |
|       - |  587 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |  588 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |  589 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |  590 | ` *  property in a string with a minimum of effort.` |
|       - |  591 | ` *  Simple syntax` |
|       - |  592 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |  593 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |  594 | ` *   the end of the name.` |
|       - |  595 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |  596 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |  597 | ` *   as to simple variables.` |
|       - |  598 | ` *  Complex (curly) syntax` |
|       - |  599 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |  600 | ` *   of complex expressions.` |
|       - |  601 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |  602 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |  603 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |  604 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |  605 | ` */` |
|    1522 |  606 | `static sxi32 GenStateProcessStringExpression(` |
|       - |  607 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  608 | `	sxu32 nLine,         /* Line number */` |
|       - |  609 | `	const char *zIn,     /* Raw expression */` |
|       - |  610 | `	const char *zEnd     /* End of the expression */` |
|       - |  611 | `	)` |
|       2 |  612 |  |
|       - |  613 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  614 | `	SySet sToken;` |
|       - |  615 | `	sxi32 rc;` |
|       - |  616 | `	/* Initialize the token set */` |
|    1524 |  617 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  618 | `	/* Preallocate some slots */` |
|    1524 |  619 | `	SySetAlloc(&sToken,0x08);` |
|       - |  620 | `	/* Tokenize the text */` |
|    1524 |  621 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  622 | `	/* Swap delimiter */` |
|    1524 |  623 | `	pTmpIn  = pGen->pIn;` |
|    1524 |  624 | `	pTmpEnd = pGen->pEnd;` |
|    1524 |  625 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1524 |  626 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  627 | `	/* Compile the expression */` |
|    1524 |  628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  629 | `	/* Restore token stream */` |
|    1524 |  630 | `	pGen->pIn  = pTmpIn;` |
|    1524 |  631 | `	pGen->pEnd = pTmpEnd;` |
|       - |  632 | `	/* Release the token set */` |
|    1524 |  633 | `	SySetRelease(&sToken);` |
|       - |  634 | `	/* Compilation result */` |
|    1524 |  635 | `	return rc;` |
|       2 |  636 |  |
|       - |  637 | `/*` |
|       - |  638 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  639 | ` */` |
|   14588 |  640 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  641 |  |
|       - |  642 | `	ph7_value *pConstObj;` |
|   14590 |  643 | `	sxu32 nIdx = 0;` |
|       - |  644 | `	/* Reserve a new constant */` |
|   14590 |  645 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   14590 |  646 | `	if( pConstObj == 0 ){` |
|     ! 0 |  647 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  648 | `		return 0;` |
|       - |  649 | `	}` |
|   14590 |  650 | `	(*pCount)++;` |
|   14590 |  651 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  652 | `	/* Emit the load constant instruction */` |
|   14590 |  653 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   14590 |  654 | `	return pConstObj;` |
|    7296 |  655 |  |
|       - |  656 | `/*` |
|       - |  657 | ` * Compile a double quoted/heredoc string.` |
|       - |  658 | ` * According to the PHP language reference manual` |
|       - |  659 | ` * Heredoc` |
|       - |  660 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |  661 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |  662 | ` *  to close the quotation.` |
|       - |  663 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |  664 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |  665 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |  666 | ` *  Warning` |
|       - |  667 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |  668 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |  669 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |  670 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |  671 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |  672 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |  673 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |  674 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |  675 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |  676 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |  677 | ` * Double quoted` |
|       - |  678 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |  679 | ` *  Escaped characters Sequence 	Meaning` |
|       - |  680 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |  681 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |  682 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |  683 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |  684 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |  685 | ` *  \\ backslash` |
|       - |  686 | ` *  \$ dollar sign` |
|       - |  687 | ` *  \" double-quote` |
|       - |  688 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation` |
|       - |  689 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |  690 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |  691 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |  692 | ` * See string parsing for details.` |
|       - |  693 | ` */` |
|   13492 |  694 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  695 |  |
|   13494 |  696 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  697 | `	const char *zIn,*zCur,*zEnd;` |
|   13494 |  698 | `	ph7_value *pObj = 0;` |
|       - |  699 | `	sxi32 iCons;` |
|       - |  700 | `	sxi32 rc;` |
|       - |  701 | `	/* Delimit the string */` |
|   13494 |  702 | `	zIn  = pStr->zString;` |
|   13494 |  703 | `	zEnd = &zIn[pStr->nByte];` |
|   13494 |  704 | `	if( zIn >= zEnd ){` |
|       - |  705 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  706 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  707 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  708 | `		 */` |
|     224 |  709 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     224 |  710 | `		return SXRET_OK;` |
|       - |  711 | `	}` |
|   13272 |  712 | `	zCur = 0;` |
|       - |  713 | `	/* Compile the node */` |
|   13272 |  714 | `	iCons = 0;` |
|    7396 |  715 | `	for(;;){` |
|   22276 |  716 | `		zCur = zIn;` |
|  128258 |  717 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  107506 |  718 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      44 |  719 | `				break;` |
|  107422 |  720 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1440 |  721 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     720 |  722 | `					break;` |
|       - |  723 | `			}` |
|  105984 |  724 | `			zIn++;` |
|       2 |  725 | `		}` |
|   22276 |  726 | `		if( zIn > zCur ){` |
|   10856 |  727 | `			if( pObj == 0 ){` |
|   10586 |  728 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   10586 |  729 | `				if( pObj == 0 ){` |
|     ! 0 |  730 | `					return SXERR_ABORT;` |
|       - |  731 | `				}` |
|    5292 |  732 | `			}` |
|   10856 |  733 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    5427 |  734 | `		}` |
|   22276 |  735 | `		if( zIn >= zEnd ){` |
|   13272 |  736 | `			break;` |
|       - |  737 | `		}` |
|    9006 |  738 | `		if( zIn[0] == '\\' ){` |
|    7484 |  739 | `			const char *zPtr = 0;` |
|       - |  740 | `			sxu32 n;` |
|    7484 |  741 | `			zIn++;` |
|    7484 |  742 | `			if( zIn >= zEnd ){` |
|     ! 0 |  743 | `				break;` |
|       - |  744 | `			}` |
|    7484 |  745 | `			if( pObj == 0 ){` |
|    4006 |  746 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    4006 |  747 | `				if( pObj == 0 ){` |
|     ! 0 |  748 | `					return SXERR_ABORT;` |
|       - |  749 | `				}` |
|    2002 |  750 | `			}` |
|    7484 |  751 | `			n = sizeof(char); /* size of conversion */` |
|    7484 |  752 | `			switch( zIn[0] ){` |
|       3 |  753 | `			case '$':` |
|       - |  754 | `				/* Dollar sign */` |
|       7 |  755 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       7 |  756 | `				break;` |
|      38 |  757 | `			case '\\':` |
|       - |  758 | `				/* A literal backslash */` |
|      78 |  759 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      78 |  760 | `				break;` |
|       2 |  761 | `			case 'a':` |
|       - |  762 | `				/* The "alert" character (BEL)[ctrl+g] ASCII code 7 */` |
|       5 |  763 | `				PH7_MemObjStringAppend(pObj,"\a",sizeof(char));` |
|       5 |  764 | `				break;` |
|       2 |  765 | `			case 'b':` |
|       - |  766 | `				/* Backspace (BS)[ctrl+h] ASCII code 8 */` |
|       5 |  767 | `				PH7_MemObjStringAppend(pObj,"\b",sizeof(char));` |
|       5 |  768 | `				break;` |
|       4 |  769 | `			case 'f':` |
|       - |  770 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       9 |  771 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       9 |  772 | `				break;` |
|    3379 |  773 | `			case 'n':` |
|       - |  774 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    6760 |  775 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    6760 |  776 | `				break;` |
|      16 |  777 | `			case 'r':` |
|       - |  778 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      34 |  779 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      34 |  780 | `				break;` |
|      24 |  781 | `			case 't':` |
|       - |  782 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      50 |  783 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      50 |  784 | `				break;` |
|       3 |  785 | `			case 'v':` |
|       - |  786 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 |  787 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 |  788 | `				break;` |
|       1 |  789 | `			case '\'':` |
|       - |  790 | `				/* Single quote */` |
|       3 |  791 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       3 |  792 | `				break;` |
|      50 |  793 | `			case '"':` |
|       - |  794 | `				/* Double quote */` |
|     102 |  795 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|     102 |  796 | `				break;` |
|       5 |  797 | `			case '0':` |
|       - |  798 | `				/* NUL byte */` |
|      11 |  799 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      11 |  800 | `				break;` |
|     188 |  801 | `			case 'x':` |
|     377 |  802 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - |  803 | `					int c;` |
|       - |  804 | `					/* Hex digit */` |
|     363 |  805 | `					c = SyHexToint(zIn[1]) << 4;` |
|     363 |  806 | `					if( &zIn[2] < zEnd ){` |
|     363 |  807 | `						c +=  SyHexToint(zIn[2]);` |
|     181 |  808 | `					}` |
|       - |  809 | `					/* Output char */` |
|     363 |  810 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     363 |  811 | `					n += sizeof(char) * 2;` |
|     182 |  812 | `				}else{` |
|       - |  813 | `					/* Output literal character  */` |
|      15 |  814 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - |  815 | `				}` |
|     377 |  816 | `				break;` |
|      15 |  817 | `			case 'o':` |
|      31 |  818 | `				if( &zIn[1] < zEnd && (unsigned char)zIn[1] < 0xc0 && SyisDigit(zIn[1]) && (zIn[1] - '0') < 8 ){` |
|       - |  819 | `					/* Octal digit stream */` |
|       - |  820 | `					int c;` |
|      21 |  821 | `					c = 0;` |
|      21 |  822 | `					zIn++;` |
|      61 |  823 | `					for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      55 |  824 | `						if( zPtr >= zEnd \|\| (unsigned char)zPtr[0] >= 0xc0 \|\| !SyisDigit(zPtr[0]) \|\| (zPtr[0] - '0') > 7 ){` |
|       8 |  825 | `							break;` |
|       - |  826 | `						}` |
|      41 |  827 | `						c = c * 8 + (zPtr[0] - '0');` |
|      21 |  828 | `					}` |
|      21 |  829 | `					if ( c > 0 ){` |
|      15 |  830 | `						PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|       7 |  831 | `					}` |
|      21 |  832 | `					n = (sxu32)(zPtr-zIn);` |
|      11 |  833 | `				}else{` |
|       - |  834 | `					/* Output literal character  */` |
|      11 |  835 | `					PH7_MemObjStringAppend(pObj,"o",sizeof(char));` |
|       - |  836 | `				}` |
|      31 |  837 | `				break;` |
|      11 |  838 | `			default:` |
|       - |  839 | `				/* Output without a slash */` |
|      23 |  840 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char));` |
|      22 |  841 | `				break;` |
|       - |  842 | `			}` |
|       - |  843 | `			/* Advance the stream cursor */` |
|    7484 |  844 | `			zIn += n;` |
|    7484 |  845 | `			continue;` |
|       - |  846 | `		}` |
|    1524 |  847 | `		if( zIn[0] == '{' ){` |
|       - |  848 | `			/* Curly syntax */` |
|       - |  849 | `			const char *zExpr;` |
|      87 |  850 | `			sxi32 iNest = 1;` |
|      87 |  851 | `			zIn++;` |
|      87 |  852 | `			zExpr = zIn;` |
|       - |  853 | `			/* Synchronize with the next closing curly braces */` |
|     985 |  854 | `			while( zIn < zEnd ){` |
|     985 |  855 | `				if( zIn[0] == '{' ){` |
|       - |  856 | `					/* Increment nesting level */` |
|       9 |  857 | `					iNest++;` |
|     981 |  858 | `				}else if(zIn[0] == '}' ){` |
|       - |  859 | `					/* Decrement nesting level */` |
|      95 |  860 | `					iNest--;` |
|      95 |  861 | `					if( iNest <= 0 ){` |
|      87 |  862 | `						break;` |
|       - |  863 | `					}` |
|       4 |  864 | `				}` |
|     899 |  865 | `				zIn++;` |
|       1 |  866 | `			}` |
|       - |  867 | `			/* Process the expression */` |
|      87 |  868 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      87 |  869 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  870 | `				return SXERR_ABORT;` |
|       - |  871 | `			}` |
|      87 |  872 | `			if( rc != SXERR_EMPTY ){` |
|      87 |  873 | `				++iCons;` |
|      43 |  874 | `			}` |
|      87 |  875 | `			if( zIn < zEnd ){` |
|       - |  876 | `				/* Jump the trailing curly */` |
|      87 |  877 | `				zIn++;` |
|      43 |  878 | `			}` |
|      44 |  879 | `		}else{` |
|       - |  880 | `			/* Simple syntax */` |
|    1438 |  881 | `			const char *zExpr = zIn;` |
|       - |  882 | `			/* Assemble variable name */` |
|     718 |  883 | `			for(;;){` |
|       - |  884 | `				/* Jump leading dollars */` |
|    2874 |  885 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1438 |  886 | `					zIn++;` |
|       2 |  887 | `				}` |
|     718 |  888 | `				for(;;){` |
|    9340 |  889 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7186 |  890 | `						zIn++;` |
|       2 |  891 | `					}` |
|    1438 |  892 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  893 | `						/* UTF-8 stream */` |
|     ! 0 |  894 | `						zIn++;` |
|     ! 0 |  895 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  896 | `							zIn++;` |
|     ! 0 |  897 | `						}` |
|     ! 0 |  898 | `						continue;` |
|       - |  899 | `					}` |
|    1438 |  900 | `					break;` |
|     ! 0 |  901 | `				}` |
|    1438 |  902 | `				if( zIn >= zEnd ){` |
|      79 |  903 | `					break;` |
|       - |  904 | `				}` |
|    1360 |  905 | `				if( zIn[0] == '[' ){` |
|       9 |  906 | `					sxi32 iSquare = 1;` |
|       9 |  907 | `					zIn++;` |
|      17 |  908 | `					while( zIn < zEnd ){` |
|      17 |  909 | `						if( zIn[0] == '[' ){` |
|     ! 0 |  910 | `							iSquare++;` |
|      17 |  911 | `						}else if (zIn[0] == ']' ){` |
|       9 |  912 | `							iSquare--;` |
|       9 |  913 | `							if( iSquare <= 0 ){` |
|       9 |  914 | `								break;` |
|       - |  915 | `							}` |
|     ! 0 |  916 | `						}` |
|       9 |  917 | `						zIn++;` |
|       1 |  918 | `					}` |
|       9 |  919 | `					if( zIn < zEnd ){` |
|       9 |  920 | `						zIn++;` |
|       4 |  921 | `					}` |
|       9 |  922 | `					break;` |
|    1352 |  923 | `				}else if(zIn[0] == '{' ){` |
|       6 |  924 | `					sxi32 iCurly = 1;` |
|       6 |  925 | `					zIn++;` |
|      18 |  926 | `					while( zIn < zEnd ){` |
|      16 |  927 | `						if( zIn[0] == '{' ){` |
|     ! 0 |  928 | `							iCurly++;` |
|      16 |  929 | `						}else if (zIn[0] == '}' ){` |
|       3 |  930 | `							iCurly--;` |
|       3 |  931 | `							if( iCurly <= 0 ){` |
|       3 |  932 | `								break;` |
|       - |  933 | `							}` |
|     ! 0 |  934 | `						}` |
|      14 |  935 | `						zIn++;` |
|       2 |  936 | `					}` |
|       6 |  937 | `					if( zIn < zEnd ){` |
|       3 |  938 | `						zIn++;` |
|       1 |  939 | `					}` |
|       6 |  940 | `					break;` |
|    1348 |  941 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  942 | `					/* Member access operator '->' */` |
|     ! 0 |  943 | `					zIn += 2;` |
|    1348 |  944 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  945 | `					/* Static member access operator '::' */` |
|     ! 0 |  946 | `					zIn += 2;` |
|     ! 0 |  947 | `				}else{` |
|     675 |  948 | `					break;` |
|       - |  949 | `				}` |
|     ! 0 |  950 | `			}` |
|       - |  951 | `			/* Process the expression */` |
|    1438 |  952 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1438 |  953 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  954 | `				return SXERR_ABORT;` |
|       - |  955 | `			}` |
|    1438 |  956 | `			if( rc != SXERR_EMPTY ){` |
|    1436 |  957 | `				++iCons;` |
|     717 |  958 | `			}` |
|       - |  959 | `		}` |
|       - |  960 | `		/* Invalidate the previously used constant */` |
|    1524 |  961 | `		pObj = 0;` |
|       2 |  962 | `	}/*for(;;)*/` |
|   13272 |  963 | `	if( iCons > 1 ){` |
|       - |  964 | `		/* Concatenate all compiled constants */` |
|    1170 |  965 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     584 |  966 | `	}` |
|       - |  967 | `	/* Node successfully compiled */` |
|   13272 |  968 | `	return SXRET_OK;` |
|    6748 |  969 |  |
|       - |  970 | `/*` |
|       - |  971 | ` * Compile a double quoted string.` |
|       - |  972 | ` *  See the block-comment above for more information.` |
|       - |  973 | ` */` |
|   13466 |  974 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  975 |  |
|       - |  976 | `	sxi32 rc;` |
|   13468 |  977 | `	rc = GenStateCompileString(&(*pGen));` |
|    6733 |  978 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  979 | `	/* Compilation result */` |
|   13468 |  980 | `	return rc;` |
|       2 |  981 |  |
|       - |  982 | `/*` |
|       - |  983 | ` * Compile a Heredoc string.` |
|       - |  984 | ` *  See the block-comment above for more information.` |
|       - |  985 | ` */` |
|      26 |  986 | `static sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  987 |  |
|      28 |  988 | `	GenStateCompileString(&(*pGen));` |
|      13 |  989 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  990 | `	/* Compilation result */` |
|      28 |  991 | `	return SXRET_OK;` |
|       2 |  992 |  |
|       - |  993 | `/*` |
|       - |  994 | ` * Compile an array entry whether it is a key or a value.` |
|       - |  995 | ` *  Notes on array entries.` |
|       - |  996 | ` *  According to the PHP language reference manual` |
|       - |  997 | ` *  An array can be created by the array() language construct.` |
|       - |  998 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - |  999 | ` *  array(  key =>  value` |
|       - | 1000 | ` *    , ...` |
|       - | 1001 | ` *    )` |
|       - | 1002 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - | 1003 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - | 1004 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - | 1005 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - | 1006 | ` *  contain integer and string indices.` |
|       - | 1007 | ` *  A value can be any PHP type.` |
|       - | 1008 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - | 1009 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - | 1010 | ` *  is specified, that value will be overwritten.` |
|       - | 1011 | ` */` |
|   14490 | 1012 | `static sxi32 GenStateCompileArrayEntry(` |
|       - | 1013 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 1014 | `	SyToken *pIn,        /* Token stream */` |
|       - | 1015 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - | 1016 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - | 1017 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - | 1018 | `	)` |
|       2 | 1019 |  |
|       - | 1020 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 1021 | `	sxi32 rc;` |
|       - | 1022 | `	/* Swap token stream */` |
|   14492 | 1023 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1024 | `	/* Compile the expression*/` |
|   14492 | 1025 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1026 | `	/* Restore token stream */` |
|   14492 | 1027 | `	RE_SWAP_DELIMITER(pGen);` |
|   14492 | 1028 | `	return rc;` |
|       2 | 1029 |  |
|       - | 1030 | `/*` |
|       - | 1031 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - | 1032 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1033 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1034 | ` * error message.` |
|       - | 1035 | ` * See the routine responible of compiling the array language construct` |
|       - | 1036 | ` * for more inforation.` |
|       - | 1037 | ` */` |
|      30 | 1038 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1039 |  |
|      32 | 1040 | `	sxi32 rc = SXRET_OK;` |
|      32 | 1041 | `	if( pRoot->pOp ){` |
|      19 | 1042 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 | 1043 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      14 | 1044 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 1045 | `			/* Unexpected expression */` |
|      11 | 1046 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1047 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      11 | 1048 | `			if( rc != SXERR_ABORT ){` |
|      11 | 1049 | `				rc = SXERR_INVALID;` |
|       5 | 1050 | `			}` |
|       7 | 1051 | `		}` |
|      25 | 1052 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1053 | `		/* Unexpected expression */` |
|       3 | 1054 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1055 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 | 1056 | `		if( rc != SXERR_ABORT ){` |
|       3 | 1057 | `			rc = SXERR_INVALID;` |
|       1 | 1058 | `		}` |
|       1 | 1059 | `	}` |
|      32 | 1060 | `	return rc;` |
|       2 | 1061 |  |
|       - | 1062 | `/*` |
|       - | 1063 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - | 1064 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - | 1065 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - | 1066 | ` */` |
|   21280 | 1067 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 | 1068 |  |
|       - | 1069 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1070 | `	SyToken *pKey,*pCur;` |
|   21282 | 1071 | `	sxi32 iEmitRef = 0;` |
|   21282 | 1072 | `	sxi32 nPair = 0;` |
|       - | 1073 | `	sxi32 iNest;` |
|       - | 1074 | `	sxi32 rc;` |
|   21282 | 1075 | `	xValidator = 0;` |
|   17284 | 1076 | `	for(;;){` |
|       - | 1077 | `		/* Jump leading commas */` |
|   39074 | 1078 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    4506 | 1079 | `			pGen->pIn++;` |
|       2 | 1080 | `		}` |
|   34570 | 1081 | `		pCur = pGen->pIn;` |
|   34570 | 1082 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1083 | `			/* No more entry to process */` |
|   21270 | 1084 | `			break;` |
|       - | 1085 | `		}` |
|   13302 | 1086 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1087 | `			continue;` |
|       - | 1088 | `		}` |
|       - | 1089 | `		/* Compile the key if available */` |
|   13302 | 1090 | `		pKey = pCur;` |
|   13302 | 1091 | `		iNest = 0;` |
|   36822 | 1092 | `		while( pCur < pGen->pIn ){` |
|   24684 | 1093 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1164 | 1094 | `				break;` |
|       - | 1095 | `			}` |
|   23522 | 1096 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      66 | 1097 | `				iNest++;` |
|   23490 | 1098 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1099 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1100 | `				 * parser will shortly detect any syntax error.` |
|       - | 1101 | `				 */` |
|      66 | 1102 | `				iNest--;` |
|      32 | 1103 | `			}` |
|   23522 | 1104 | `			pCur++;` |
|       2 | 1105 | `		}` |
|   13302 | 1106 | `		rc = SXERR_EMPTY;` |
|   13302 | 1107 | `		if( pCur < pGen->pIn ){` |
|    1164 | 1108 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - | 1109 | `				/* Missing value */` |
|      11 | 1110 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 | 1111 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1112 | `					return SXERR_ABORT;` |
|       - | 1113 | `				}` |
|      11 | 1114 | `				return SXRET_OK;` |
|       - | 1115 | `			}` |
|       - | 1116 | `			/* Compile the expression holding the key */` |
|    1154 | 1117 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - | 1118 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1154 | 1119 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1120 | `				return SXERR_ABORT;` |
|       - | 1121 | `			}` |
|    1154 | 1122 | `			pCur++; /* Jump the '=>' operator */` |
|   12716 | 1123 | `		}else if( pKey == pCur ){` |
|       - | 1124 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1125 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1126 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1127 | `		}else{` |
|       - | 1128 | `			/* Reset back the cursor and point to the entry value */` |
|   12140 | 1129 | `			pCur = pKey;` |
|       - | 1130 | `		}` |
|   13292 | 1131 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1132 | `			/* No available key,load NULL */` |
|   12142 | 1133 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    6070 | 1134 | `		}` |
|   13292 | 1135 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - | 1136 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      34 | 1137 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      34 | 1138 | `			iEmitRef = 1;` |
|      34 | 1139 | `			pCur++; /* Jump the '&' token */` |
|      34 | 1140 | `			if( pCur >= pGen->pIn ){` |
|       - | 1141 | `				/* Missing value */` |
|       3 | 1142 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 | 1143 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1144 | `					return SXERR_ABORT;` |
|       - | 1145 | `				}` |
|       3 | 1146 | `				return SXRET_OK;` |
|       - | 1147 | `			}` |
|      15 | 1148 | `		}` |
|       - | 1149 | `		/* Compile indice value */` |
|   13290 | 1150 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   13290 | 1151 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1152 | `			return SXERR_ABORT;` |
|       - | 1153 | `		}` |
|   13290 | 1154 | `		if( iEmitRef ){` |
|       - | 1155 | `			/* Emit the load reference instruction */` |
|      32 | 1156 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1157 | `		}` |
|   13290 | 1158 | `		xValidator = 0;` |
|   13290 | 1159 | `		iEmitRef = 0;` |
|   13290 | 1160 | `		nPair++;` |
|       2 | 1161 | `	}` |
|       - | 1162 | `	/* Emit the load map instruction */` |
|   21270 | 1163 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1164 | `	/* Node successfully compiled */` |
|   21270 | 1165 | `	return SXRET_OK;` |
|   10642 | 1166 |  |
|       - | 1167 | `/*` |
|       - | 1168 | ` * Compile the 'array' language construct.` |
|       - | 1169 | ` *	 According to the PHP language reference manual` |
|       - | 1170 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1171 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1172 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1173 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1174 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1175 | ` */` |
|   21210 | 1176 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1177 |  |
|       - | 1178 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   21212 | 1179 | `	pGen->pIn += 2;` |
|   21212 | 1180 | `	pGen->pEnd--;` |
|   10605 | 1181 | `	SXUNUSED(iCompileFlag);` |
|   21212 | 1182 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1183 |  |
|       - | 1184 | `/*` |
|       - | 1185 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - | 1186 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - | 1187 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - | 1188 | ` */` |
|      70 | 1189 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1190 |  |
|       - | 1191 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      71 | 1192 | `	pGen->pIn++;` |
|      71 | 1193 | `	pGen->pEnd--;` |
|      35 | 1194 | `	SXUNUSED(iCompileFlag);` |
|      71 | 1195 | `	return GenStateCompileArrayBody(pGen);` |
|       1 | 1196 |  |
|       - | 1197 | `/*` |
|       - | 1198 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - | 1199 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1200 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1201 | ` * error message.` |
|       - | 1202 | ` * See the routine responible of compiling the list language construct` |
|       - | 1203 | ` * for more inforation.` |
|       - | 1204 | ` */` |
|      50 | 1205 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1206 |  |
|      52 | 1207 | `	sxi32 rc = SXRET_OK;` |
|      52 | 1208 | `	if( pRoot->pOp ){` |
|     ! 0 | 1209 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 | 1210 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - | 1211 | `				/* Unexpected expression */` |
|     ! 0 | 1212 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1213 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 | 1214 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 | 1215 | `					rc = SXERR_INVALID;` |
|     ! 0 | 1216 | `				}` |
|     ! 0 | 1217 | `		}` |
|      52 | 1218 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1219 | `		/* Unexpected expression */` |
|       3 | 1220 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1221 | `			"list(): Expecting a variable not an expression");` |
|       3 | 1222 | `		if( rc != SXERR_ABORT ){` |
|       3 | 1223 | `			rc = SXERR_INVALID;` |
|       1 | 1224 | `		}` |
|       1 | 1225 | `	}` |
|      52 | 1226 | `	return rc;` |
|       2 | 1227 |  |
|       - | 1228 | `/*` |
|       - | 1229 | ` * Compile the 'list' language construct.` |
|       - | 1230 | ` *  According to the PHP language reference` |
|       - | 1231 | ` *  list(): Assign variables as if they were an array.` |
|       - | 1232 | ` *  list() is used to assign a list of variables in one operation.` |
|       - | 1233 | ` *  Description` |
|       - | 1234 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - | 1235 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - | 1236 | ` *   list() is used to assign a list of variables in one operation.` |
|       - | 1237 | ` *  Parameters` |
|       - | 1238 | ` *   $varname: A variable.` |
|       - | 1239 | ` *  Return Values` |
|       - | 1240 | ` *   The assigned array.` |
|       - | 1241 | ` */` |
|      24 | 1242 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1243 |  |
|       - | 1244 | `	SyToken *pNext;` |
|       - | 1245 | `	sxi32 nExpr;` |
|       - | 1246 | `	sxi32 rc;` |
|      26 | 1247 | `	nExpr = 0;` |
|       - | 1248 | `	/* Jump the 'list' keyword,the leading left parenthesis and the trailing parenthesis */` |
|      26 | 1249 | `	pGen->pIn += 2;` |
|      26 | 1250 | `	pGen->pEnd--;` |
|      12 | 1251 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      80 | 1252 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      56 | 1253 | `		if( pGen->pIn < pNext ){` |
|       - | 1254 | `			/* Compile the expression holding the variable */` |
|      52 | 1255 | `			rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      52 | 1256 | `			if( rc != SXRET_OK ){` |
|       - | 1257 | `				/* Do not bother compiling this expression, it's broken anyway */` |
|     ! 0 | 1258 | `				return SXRET_OK;` |
|       - | 1259 | `			}` |
|      27 | 1260 | `		}else{` |
|       - | 1261 | `			/* Empty entry,load NULL */` |
|       5 | 1262 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - | 1263 | `		}` |
|      56 | 1264 | `		nExpr++;` |
|       - | 1265 | `		/* Advance the stream cursor */` |
|      56 | 1266 | `		pGen->pIn = &pNext[1];` |
|       2 | 1267 | `	}` |
|       - | 1268 | `	/* Emit the LOAD_LIST instruction */` |
|      26 | 1269 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - | 1270 | `	/* Node successfully compiled */` |
|      26 | 1271 | `	return SXRET_OK;` |
|      14 | 1272 |  |
|       - | 1273 | `/* Forward declarations */` |
|       - | 1274 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - | 1275 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - | 1276 | `/*` |
|       - | 1277 | ` * Compile an annoynmous function or a closure.` |
|       - | 1278 | ` * According to the PHP language reference` |
|       - | 1279 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - | 1280 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - | 1281 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - | 1282 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - | 1283 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - | 1284 | ` *  Example Anonymous function variable assignment example` |
|       - | 1285 | ` * <?php` |
|       - | 1286 | ` * $greet = function($name)` |
|       - | 1287 | ` * {` |
|       - | 1288 | ` *    printf("Hello %s\r\n", $name);` |
|       - | 1289 | ` * };` |
|       - | 1290 | ` * $greet('World');` |
|       - | 1291 | ` * $greet('PHP');` |
|       - | 1292 | ` * ?>` |
|       - | 1293 | ` * Note that the implementation of annoynmous function and closure under` |
|       - | 1294 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - | 1295 | ` */` |
|     128 | 1296 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1297 |  |
|       - | 1298 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - | 1299 | `	char zName[512];         /* Unique lambda name */` |
|       - | 1300 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - | 1301 | `							  * one thread is allowed to compile the script.` |
|       - | 1302 | `						      */` |
|       - | 1303 | `	ph7_value *pObj;` |
|       - | 1304 | `	SyString sName;` |
|       - | 1305 | `	sxu32 nIdx;` |
|       - | 1306 | `	sxu32 nLen;` |
|       - | 1307 | `	sxi32 rc;` |
|       - | 1308 |  |
|     130 | 1309 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     130 | 1310 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 | 1311 | `		pGen->pIn++;` |
|     ! 0 | 1312 | `	}` |
|       - | 1313 | `	/* Reserve a constant for the lambda */` |
|     130 | 1314 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     130 | 1315 | `	if( pObj == 0 ){` |
|     ! 0 | 1316 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1317 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1318 | `		return SXERR_ABORT;` |
|       - | 1319 | `	}` |
|       - | 1320 | `	/* Generate a unique name */` |
|     130 | 1321 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - | 1322 | `	/* Make sure the generated name is unique */` |
|     130 | 1323 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 1324 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 | 1325 | `	}` |
|     130 | 1326 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     130 | 1327 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - | 1328 | `	/* Compile the lambda body */` |
|     130 | 1329 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     130 | 1330 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1331 | `		return SXERR_ABORT;` |
|       - | 1332 | `	}` |
|     130 | 1333 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1334 | `		/* Emit the load closure instruction */` |
|      10 | 1335 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       6 | 1336 | `	}else{` |
|       - | 1337 | `		/* Emit the load constant instruction */` |
|     122 | 1338 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1339 | `	}` |
|       - | 1340 | `	/* Node successfully compiled */` |
|     130 | 1341 | `	return SXRET_OK;` |
|      66 | 1342 |  |
|       - | 1343 | `/*` |
|       - | 1344 | ` * Compile a backtick quoted string.` |
|       - | 1345 | ` */` |
|       4 | 1346 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1347 |  |
|       - | 1348 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - | 1349 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - | 1350 | `	 */` |
|       7 | 1351 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - | 1352 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 | 1353 | `		ph7_lib_version()` |
|       - | 1354 | `		);` |
|       - | 1355 | `	/* Load NULL */` |
|       5 | 1356 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1357 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1358 | `	/* Node successfully compiled */` |
|       5 | 1359 | `	return SXRET_OK;` |
|       1 | 1360 |  |
|       - | 1361 | `/*` |
|       - | 1362 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - | 1363 | ` * construct.` |
|       - | 1364 | ` */` |
|      70 | 1365 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1366 |  |
|       - | 1367 | `	SyString *pName;` |
|       - | 1368 | `	sxu32 nKeyID;` |
|       - | 1369 | `	sxi32 rc;` |
|       - | 1370 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      72 | 1371 | `	pName = &pGen->pIn->sData;` |
|      72 | 1372 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      72 | 1373 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      72 | 1374 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 | 1375 | `		SyToken *pTmp,*pNext = 0;` |
|       - | 1376 | `		/* Compile arguments one after one */` |
|       9 | 1377 | `		pTmp = pGen->pEnd;` |
|       - | 1378 | `		/* Symisc eXtension to the PHP programming language:` |
|       - | 1379 | `		 * 'echo' can be used in the context of a function which` |
|       - | 1380 | `		 *  mean that the following expression is valid:` |
|       - | 1381 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - | 1382 | `		 */` |
|       9 | 1383 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 | 1384 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 | 1385 | `			if( pGen->pIn < pNext ){` |
|       9 | 1386 | `				pGen->pEnd = pNext;` |
|       9 | 1387 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 | 1388 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1389 | `					return SXERR_ABORT;` |
|       - | 1390 | `				}` |
|       9 | 1391 | `				if( rc != SXERR_EMPTY ){` |
|       - | 1392 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - | 1393 | `					 * without the overhead of a function call.` |
|       - | 1394 | `					 * This is a very powerful optimization that improve` |
|       - | 1395 | `					 * performance greatly.` |
|       - | 1396 | `					 */` |
|       9 | 1397 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 | 1398 | `				}` |
|       4 | 1399 | `			}` |
|       - | 1400 | `			/* Jump trailing commas */` |
|       9 | 1401 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 1402 | `				pNext++;` |
|     ! 0 | 1403 | `			}` |
|       9 | 1404 | `			pGen->pIn = pNext;` |
|       1 | 1405 | `		}` |
|       - | 1406 | `		/* Restore token stream */` |
|       9 | 1407 | `		pGen->pEnd = pTmp;` |
|       5 | 1408 | `	}else{` |
|      64 | 1409 | `		sxi32 nArg = 0;` |
|      64 | 1410 | `		sxu32 nIdx = 0;` |
|      64 | 1411 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|      64 | 1412 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1413 | `			return SXERR_ABORT;` |
|      64 | 1414 | `		}else if(rc != SXERR_EMPTY ){` |
|      64 | 1415 | `			nArg = 1;` |
|      31 | 1416 | `		}` |
|      64 | 1417 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1418 | `			ph7_value *pObj;` |
|       - | 1419 | `			/* Emit the call instruction */` |
|      18 | 1420 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      18 | 1421 | `			if( pObj == 0 ){` |
|     ! 0 | 1422 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1423 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1424 | `				return SXERR_ABORT;` |
|       - | 1425 | `			}` |
|      18 | 1426 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1427 | `			/* Install in the literal table */` |
|      18 | 1428 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       8 | 1429 | `		}` |
|       - | 1430 | `		/* Emit the call instruction */` |
|      64 | 1431 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      64 | 1432 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - | 1433 | `	}` |
|       - | 1434 | `	/* Node successfully compiled */` |
|      72 | 1435 | `	return SXRET_OK;` |
|      37 | 1436 |  |
|       - | 1437 | `/*` |
|       - | 1438 | ` * Compile a node holding a variable declaration.` |
|       - | 1439 | ` * According to the PHP language reference` |
|       - | 1440 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 1441 | ` *  The variable name is case-sensitive.` |
|       - | 1442 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 1443 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1444 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 1445 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 1446 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 1447 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 1448 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 1449 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 1450 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 1451 | ` *  the chapter on Expressions.` |
|       - | 1452 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 1453 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 1454 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 1455 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 1456 | ` *  is being assigned (the source variable).` |
|       - | 1457 | ` */` |
|  654572 | 1458 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1459 |  |
|  654574 | 1460 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1461 | `	sxi32 iVv;` |
|       - | 1462 | `	sxi32 iP1;` |
|       - | 1463 | `	void *p3;` |
|       - | 1464 | `	sxi32 rc;` |
|  654574 | 1465 | `	iVv = -1; /* Variable variable counter */` |
| 1309158 | 1466 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  654586 | 1467 | `		pGen->pIn++;` |
|  654586 | 1468 | `		iVv++;` |
|       2 | 1469 | `	}` |
|  654574 | 1470 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1471 | `		/* Invalid variable name */` |
|       3 | 1472 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1473 | `		if( rc == SXERR_ABORT ){` |
|       - | 1474 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1475 | `			return SXERR_ABORT;` |
|       - | 1476 | `		}` |
|       3 | 1477 | `		return SXRET_OK;` |
|       - | 1478 | `	}` |
|  654572 | 1479 | `	p3  = 0;` |
|  654572 | 1480 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 1481 | `		/* Dynamic variable creation */` |
|      18 | 1482 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 | 1483 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 | 1484 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1485 | `			/* Empty expression */` |
|       3 | 1486 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1487 | `			return SXRET_OK;` |
|       - | 1488 | `		}` |
|       - | 1489 | `		/* Compile the expression holding the variable name */` |
|      16 | 1490 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 | 1491 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1492 | `			return SXERR_ABORT;` |
|      16 | 1493 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 1494 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 | 1495 | `			return SXRET_OK;` |
|       - | 1496 | `		}` |
|       7 | 1497 | `	}else{` |
|       - | 1498 | `		SyHashEntry *pEntry;` |
|       - | 1499 | `		SyString *pName;` |
|  654556 | 1500 | `		char *zName = 0;` |
|       - | 1501 | `		/* Extract variable name */` |
|  654556 | 1502 | `		pName = &pGen->pIn->sData;` |
|       - | 1503 | `		/* Advance the stream cursor */` |
|  654556 | 1504 | `		pGen->pIn++;` |
|  654556 | 1505 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  654556 | 1506 | `		if( pEntry == 0 ){` |
|       - | 1507 | `			/* Duplicate name */` |
|   97052 | 1508 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   97052 | 1509 | `			if( zName == 0 ){` |
|     ! 0 | 1510 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1511 | `				return SXERR_ABORT;` |
|       - | 1512 | `			}` |
|       - | 1513 | `			/* Install in the hashtable */` |
|   97052 | 1514 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   48527 | 1515 | `		}else{` |
|       - | 1516 | `			/* Name already available */` |
|  557506 | 1517 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1518 | `		}` |
|  654556 | 1519 | `		p3 = (void *)zName;` |
|       - | 1520 | `	}` |
|  654568 | 1521 | `	iP1 = 0;` |
|  654568 | 1522 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  217346 | 1523 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1524 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  217342 | 1525 | `			iP1 = 1;` |
|  108670 | 1526 | `		}` |
|  108672 | 1527 | `	}` |
|       - | 1528 | `	/* Emit the load instruction */` |
|  654568 | 1529 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  654580 | 1530 | `	while( iVv > 0 ){` |
|      13 | 1531 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1532 | `		iVv--;` |
|       1 | 1533 | `	}` |
|       - | 1534 | `	/* Node successfully compiled */` |
|  654568 | 1535 | `	return SXRET_OK;` |
|  327288 | 1536 |  |
|       - | 1537 | `/*` |
|       - | 1538 | ` * Load a literal.` |
|       - | 1539 | ` */` |
|  422884 | 1540 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 1541 |  |
|  422886 | 1542 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1543 | `	ph7_value *pObj;` |
|       - | 1544 | `	SyString *pStr;` |
|       - | 1545 | `	sxu32 nIdx;` |
|       - | 1546 | `	/* Extract token value */` |
|  422886 | 1547 | `	pStr = &pToken->sData;` |
|       - | 1548 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  422886 | 1549 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   79238 | 1550 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1551 | `			/* NULL constant are always indexed at 0 */` |
|   29498 | 1552 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   29498 | 1553 | `			return SXRET_OK;` |
|   49742 | 1554 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1555 | `			/* TRUE constant are always indexed at 1 */` |
|     464 | 1556 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     464 | 1557 | `			return SXRET_OK;` |
|       2 | 1558 | `		}` |
|  407028 | 1559 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   77478 | 1560 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1561 | `			/* FALSE constant are always indexed at 2 */` |
|   32170 | 1562 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   32170 | 1563 | `			return SXRET_OK;` |
|  344717 | 1564 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   66470 | 1565 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1566 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    4872 | 1567 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    4872 | 1568 | `			if( pObj == 0 ){` |
|     ! 0 | 1569 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1570 | `				return SXERR_ABORT;` |
|       - | 1571 | `			}` |
|    4872 | 1572 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1573 | `			/* Emit the load constant instruction */` |
|    4872 | 1574 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    4872 | 1575 | `			return SXRET_OK;` |
|  315241 | 1576 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   17258 | 1577 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - | 1578 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       5 | 1579 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 | 1580 | `			if( pObj == 0 ){` |
|     ! 0 | 1581 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1582 | `				return SXERR_ABORT;` |
|       - | 1583 | `			}` |
|       5 | 1584 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1585 | `				SyString sNs;` |
|       5 | 1586 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       5 | 1587 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       3 | 1588 | `			}else{` |
|     ! 0 | 1589 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - | 1590 | `			}` |
|       5 | 1591 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       5 | 1592 | `			return SXRET_OK;` |
|  314454 | 1593 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    7822 | 1594 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  310537 | 1595 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    7876 | 1596 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 | 1597 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 1598 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 | 1599 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - | 1600 | `				/* Point to the upper block */` |
|      11 | 1601 | `				pBlock = pBlock->pParent;` |
|       1 | 1602 | `			}` |
|      11 | 1603 | `			if( pBlock == 0 ){` |
|       - | 1604 | `				/* Called in the global scope,load NULL */` |
|       5 | 1605 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1606 | `			}else{` |
|       - | 1607 | `				/* Extract the target function/method */` |
|       7 | 1608 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 | 1609 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - | 1610 | `					/* Not a class method,Load null */` |
|       3 | 1611 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1612 | `				}else{` |
|       5 | 1613 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 | 1614 | `					if( pObj == 0 ){` |
|     ! 0 | 1615 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1616 | `						return SXERR_ABORT;` |
|       - | 1617 | `					}` |
|       5 | 1618 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - | 1619 | `					/* Emit the load constant instruction */` |
|       5 | 1620 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1621 | `				}` |
|       - | 1622 | `			}` |
|      11 | 1623 | `			return SXRET_OK;` |
|       - | 1624 | `	}` |
|       - | 1625 | `	/* Query literal table */` |
|  355876 | 1626 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 1627 | `		ph7_value *pLitObj;` |
|       - | 1628 | `		/* Unknown literal,install it in the literal table */` |
|  142018 | 1629 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  142018 | 1630 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1631 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1632 | `			return SXERR_ABORT;` |
|       - | 1633 | `		}` |
|  142018 | 1634 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  142018 | 1635 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   71008 | 1636 | `	}` |
|       - | 1637 | `	/* Emit the load constant instruction */` |
|  355876 | 1638 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  355876 | 1639 | `	return SXRET_OK;` |
|  211444 | 1640 |  |
|       - | 1641 | `/*` |
|       - | 1642 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1643 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1644 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1645 | ` * Otherwise, load the simple literal directly.` |
|       - | 1646 | ` */` |
|  422904 | 1647 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 1648 |  |
|       - | 1649 | `	sxi32 rc;` |
|  422906 | 1650 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1651 | `		return SXRET_OK;` |
|       - | 1652 | `	}` |
|       - | 1653 | `	/* Check if this is a multi-token namespace path */` |
|  422906 | 1654 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 1655 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      21 | 1656 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      21 | 1657 | `		int isAbsolute = 0;` |
|      21 | 1658 | `		SyBlobReset(pWorker);` |
|       - | 1659 | `		/* Check for leading backslash (absolute path) */` |
|      21 | 1660 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      19 | 1661 | `			isAbsolute = 1;` |
|      19 | 1662 | `			pGen->pIn++; /* Skip leading backslash */` |
|       9 | 1663 | `		}` |
|       - | 1664 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      21 | 1665 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 1666 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 1667 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 | 1668 | `		}` |
|       - | 1669 | `		/* Collect all path components */` |
|      81 | 1670 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      81 | 1671 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      31 | 1672 | `				SyBlobAppend(pWorker,"\\",1);` |
|      16 | 1673 | `			}else{` |
|      51 | 1674 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 1675 | `			}` |
|      81 | 1676 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      21 | 1677 | `				pGen->pIn++;` |
|      21 | 1678 | `				break;` |
|       - | 1679 | `			}` |
|      61 | 1680 | `			pGen->pIn++;` |
|       1 | 1681 | `		}` |
|      21 | 1682 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 1683 | `			ph7_value *pObj;` |
|       - | 1684 | `			SyString sPath;` |
|       - | 1685 | `			sxu32 nIdx;` |
|      21 | 1686 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 1687 | `			/* Install in the literal table */` |
|      21 | 1688 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      11 | 1689 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      11 | 1690 | `				if( pObj == 0 ){` |
|     ! 0 | 1691 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1692 | `					return SXERR_ABORT;` |
|       - | 1693 | `				}` |
|      11 | 1694 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      11 | 1695 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       5 | 1696 | `			}` |
|       - | 1697 | `			/* Emit the load constant instruction.` |
|       - | 1698 | `			 * P1=1 means candidate for constant/function/class expansion. */` |
|      21 | 1699 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      21 | 1700 | `			return SXRET_OK;` |
|       - | 1701 | `		}` |
|     ! 0 | 1702 | `	}` |
|       - | 1703 | `	/* Single-token literal: load directly */` |
|  422886 | 1704 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  422886 | 1705 | `	return rc;` |
|  211454 | 1706 |  |
|       - | 1707 | `/*` |
|       - | 1708 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1709 | ` */` |
|  422904 | 1710 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1711 |  |
|       - | 1712 | `	sxi32 rc;` |
|  422906 | 1713 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  422906 | 1714 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1715 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1716 | `		return rc;` |
|       - | 1717 | `	}` |
|       - | 1718 | `	/* Node successfully compiled */` |
|  422906 | 1719 | `	return SXRET_OK;` |
|  211454 | 1720 |  |
|       - | 1721 | `/*` |
|       - | 1722 | ` * Recover from a compile-time error. In other words synchronize` |
|       - | 1723 | ` * the token stream cursor with the first semi-colon seen.` |
|       - | 1724 | ` */` |
|       8 | 1725 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 | 1726 |  |
|       - | 1727 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 | 1728 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 | 1729 | `		pGen->pIn++;` |
|       1 | 1730 | `	}` |
|       9 | 1731 | `	return SXRET_OK;` |
|       1 | 1732 |  |
|       - | 1733 | `/*` |
|       - | 1734 | ` * Check if the given identifier name is reserved or not.` |
|       - | 1735 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - | 1736 | ` */` |
|      30 | 1737 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 | 1738 |  |
|      32 | 1739 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      12 | 1740 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 | 1741 | `			return TRUE;` |
|      10 | 1742 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 | 1743 | `			return TRUE;` |
|       1 | 1744 | `		}` |
|      24 | 1745 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 | 1746 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 | 1747 | `			return TRUE;` |
|       - | 1748 | `		}` |
|     ! 0 | 1749 | `	}` |
|       - | 1750 | `	/* Not a reserved constant */` |
|      24 | 1751 | `	return FALSE;` |
|      17 | 1752 |  |
|       - | 1753 | `/*` |
|       - | 1754 | ` * Compile the 'const' statement.` |
|       - | 1755 | ` * According to the PHP language reference` |
|       - | 1756 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - | 1757 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - | 1758 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - | 1759 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - | 1760 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1761 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - | 1762 | ` *  Syntax` |
|       - | 1763 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - | 1764 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - | 1765 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - | 1766 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - | 1767 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - | 1768 | ` *  to get a list of all defined constants.` |
|       - | 1769 | ` *` |
|       - | 1770 | ` * Symisc eXtension.` |
|       - | 1771 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - | 1772 | ` *  would allow only simple scalar value.` |
|       - | 1773 | ` *  Example` |
|       - | 1774 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 1775 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 1776 | ` */` |
|      26 | 1777 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 | 1778 |  |
|       - | 1779 | `	SySet *pConsCode,*pInstrContainer;` |
|      28 | 1780 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1781 | `	SyString *pName;` |
|       - | 1782 | `	sxi32 rc;` |
|      28 | 1783 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      28 | 1784 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 1785 | `		/* Invalid constant name */` |
|       7 | 1786 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 | 1787 | `		if( rc == SXERR_ABORT ){` |
|       - | 1788 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1789 | `			return SXERR_ABORT;` |
|       - | 1790 | `		}` |
|       7 | 1791 | `		goto Synchronize;` |
|       - | 1792 | `	}` |
|       - | 1793 | `	/* Peek constant name */` |
|      22 | 1794 | `	pName = &pGen->pIn->sData;` |
|       - | 1795 | `	/* Make sure the constant name isn't reserved */` |
|      22 | 1796 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 1797 | `		/* Reserved constant */` |
|       9 | 1798 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 | 1799 | `		if( rc == SXERR_ABORT ){` |
|       - | 1800 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1801 | `			return SXERR_ABORT;` |
|       - | 1802 | `		}` |
|       9 | 1803 | `		goto Synchronize;` |
|       - | 1804 | `	}` |
|      14 | 1805 | `	pGen->pIn++;` |
|      14 | 1806 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 1807 | `		/* Invalid statement*/` |
|       5 | 1808 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 | 1809 | `		if( rc == SXERR_ABORT ){` |
|       - | 1810 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1811 | `			return SXERR_ABORT;` |
|       - | 1812 | `		}` |
|       5 | 1813 | `		goto Synchronize;` |
|       - | 1814 | `	}` |
|       9 | 1815 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - | 1816 | `	/* Allocate a new constant value container */` |
|       9 | 1817 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       9 | 1818 | `	if( pConsCode == 0 ){` |
|     ! 0 | 1819 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1820 | `		return SXERR_ABORT;` |
|       - | 1821 | `	}` |
|       9 | 1822 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 1823 | `	/* Swap bytecode container */` |
|       9 | 1824 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       9 | 1825 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - | 1826 | `	/* Compile constant value */` |
|       9 | 1827 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 1828 | `	/* Emit the done instruction */` |
|       9 | 1829 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       9 | 1830 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       9 | 1831 | `	if( rc == SXERR_ABORT ){` |
|       - | 1832 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1833 | `		return SXERR_ABORT;` |
|       - | 1834 | `	}` |
|       9 | 1835 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - | 1836 | `	/* Register the constant */` |
|       9 | 1837 | `	rc = PH7_VmRegisterConstant(pGen->pVm,pName,PH7_VmExpandConstantValue,pConsCode);` |
|       9 | 1838 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1839 | `		SySetRelease(pConsCode);` |
|     ! 0 | 1840 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 | 1841 | `	}` |
|       9 | 1842 | `	return SXRET_OK;` |
|       9 | 1843 | `Synchronize:` |
|       - | 1844 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 | 1845 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 | 1846 | `		pGen->pIn++;` |
|       1 | 1847 | `	}` |
|      19 | 1848 | `	return SXRET_OK;` |
|      15 | 1849 |  |
|       - | 1850 | `/*` |
|       - | 1851 | ` * Compile the 'continue' statement.` |
|       - | 1852 | ` * According to the PHP language reference` |
|       - | 1853 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - | 1854 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - | 1855 | ` *  iteration.` |
|       - | 1856 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - | 1857 | ` *  the purposes of continue.` |
|       - | 1858 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - | 1859 | ` *  of enclosing loops it should skip to the end of.` |
|       - | 1860 | ` *  Note:` |
|       - | 1861 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - | 1862 | ` */` |
|    2472 | 1863 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 1864 |  |
|       - | 1865 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1866 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1867 | `	sxu32 nLineLocal;` |
|       - | 1868 | `	sxi32 rc;` |
|    2474 | 1869 | `	nLineLocal = pGen->pIn->nLine;` |
|    2474 | 1870 | `	iLevel = 0;` |
|       - | 1871 | `	/* Jump the 'continue' keyword */` |
|    2474 | 1872 | `	pGen->pIn++;` |
|    2474 | 1873 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 1874 | `		/* optional numeric argument which tells us how many levels` |
|       - | 1875 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 1876 | `		 */` |
|      12 | 1877 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      12 | 1878 | `		if( iLevel < 2 ){` |
|     ! 0 | 1879 | `			iLevel = 0;` |
|     ! 0 | 1880 | `		}` |
|      12 | 1881 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 1882 | `	}` |
|       - | 1883 | `	/* Point to the target loop */` |
|    2474 | 1884 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2474 | 1885 | `	if( pLoop == 0 ){` |
|       - | 1886 | `		/* Illegal continue */` |
|      11 | 1887 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 1888 | `		if( rc == SXERR_ABORT ){` |
|       - | 1889 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1890 | `			return SXERR_ABORT;` |
|       - | 1891 | `		}` |
|       6 | 1892 | `	}else{` |
|    2464 | 1893 | `		sxu32 nInstrIdx = 0;` |
|    2464 | 1894 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - | 1895 | `			/* According to the PHP language reference manual` |
|       - | 1896 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - | 1897 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - | 1898 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - | 1899 | `			 */` |
|       5 | 1900 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 | 1901 | `			if( rc == SXRET_OK ){` |
|       5 | 1902 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 | 1903 | `			}` |
|       3 | 1904 | `		}else{` |
|       - | 1905 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    2460 | 1906 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2460 | 1907 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 1908 | `				JumpFixup sJumpFix;` |
|       - | 1909 | `				/* Post-continue */` |
|       8 | 1910 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       8 | 1911 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       8 | 1912 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       3 | 1913 | `			}` |
|       - | 1914 | `		}` |
|       - | 1915 | `	}` |
|    2474 | 1916 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1917 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 1918 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 1919 | `	}` |
|       - | 1920 | `	/* Statement successfully compiled */` |
|    2474 | 1921 | `	return SXRET_OK;` |
|    1238 | 1922 |  |
|       - | 1923 | `/*` |
|       - | 1924 | ` * Compile the 'break' statement.` |
|       - | 1925 | ` * According to the PHP language reference` |
|       - | 1926 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - | 1927 | ` *  structure.` |
|       - | 1928 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - | 1929 | ` *  enclosing structures are to be broken out of.` |
|       - | 1930 | ` */` |
|      88 | 1931 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 | 1932 |  |
|       - | 1933 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1934 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1935 | `	sxi32 rc;` |
|      90 | 1936 | `	iLevel = 0;` |
|       - | 1937 | `	/* Jump the 'break' keyword */` |
|      90 | 1938 | `	pGen->pIn++;` |
|      90 | 1939 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 1940 | `		/* optional numeric argument which tells us how many levels` |
|       - | 1941 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 1942 | `		 */` |
|      12 | 1943 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      12 | 1944 | `		if( iLevel < 2 ){` |
|     ! 0 | 1945 | `			iLevel = 0;` |
|     ! 0 | 1946 | `		}` |
|      12 | 1947 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 1948 | `	}` |
|       - | 1949 | `	/* Extract the target loop */` |
|      90 | 1950 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|      90 | 1951 | `	if( pLoop == 0 ){` |
|       - | 1952 | `		/* Illegal break */` |
|      17 | 1953 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 | 1954 | `		if( rc == SXERR_ABORT ){` |
|       - | 1955 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1956 | `			return SXERR_ABORT;` |
|       - | 1957 | `		}` |
|       9 | 1958 | `	}else{` |
|       - | 1959 | `		sxu32 nInstrIdx;` |
|      74 | 1960 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      74 | 1961 | `		if( rc == SXRET_OK ){` |
|       - | 1962 | `			/* Fix the jump later when the jump destination is resolved */` |
|      74 | 1963 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      36 | 1964 | `		}` |
|       - | 1965 | `	}` |
|      90 | 1966 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1967 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 1968 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 | 1969 | `	}` |
|       - | 1970 | `	/* Statement successfully compiled */` |
|      90 | 1971 | `	return SXRET_OK;` |
|      46 | 1972 |  |
|       - | 1973 | `/*` |
|       - | 1974 | ` * Compile or record a label.` |
|       - | 1975 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - | 1976 | ` * Example` |
|       - | 1977 | ` *  goto LABEL;` |
|       - | 1978 | ` *   echo 'Foo';` |
|       - | 1979 | ` *  LABEL:` |
|       - | 1980 | ` *   echo 'Bar';` |
|       - | 1981 | ` */` |
|     112 | 1982 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 | 1983 |  |
|       - | 1984 | `	GenBlock *pBlock;` |
|       - | 1985 | `	Label sLabel;` |
|       - | 1986 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 | 1987 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 | 1988 | `	if( pBlock ){` |
|       - | 1989 | `		sxi32 rc;` |
|       7 | 1990 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 | 1991 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 | 1992 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1993 | `			return SXERR_ABORT;` |
|       - | 1994 | `		}` |
|       3 | 1995 | `	}else{` |
|     110 | 1996 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 1997 | `		char *zDup;` |
|       - | 1998 | `		/* Initialize label fields */` |
|     110 | 1999 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2000 | `		/* Duplicate label name */` |
|     110 | 2001 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 | 2002 | `		if( zDup == 0 ){` |
|     ! 0 | 2003 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2004 | `			return SXERR_ABORT;` |
|       - | 2005 | `		}` |
|     110 | 2006 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 | 2007 | `		sLabel.bRef  = FALSE;` |
|     110 | 2008 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 | 2009 | `		pBlock = pGen->pCurrent;` |
|     218 | 2010 | `		while( pBlock ){` |
|     130 | 2011 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 | 2012 | `				break;` |
|       - | 2013 | `			}` |
|       - | 2014 | `			/* Point to the upper block */` |
|     110 | 2015 | `			pBlock = pBlock->pParent;` |
|       2 | 2016 | `		}` |
|     110 | 2017 | `		if( pBlock ){` |
|      22 | 2018 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 | 2019 | `		}else{` |
|      90 | 2020 | `			sLabel.pFunc = 0;` |
|       - | 2021 | `		}` |
|       - | 2022 | `		/* Insert in label set */` |
|     110 | 2023 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - | 2024 | `	}` |
|     114 | 2025 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 | 2026 | `	return SXRET_OK;` |
|      58 | 2027 |  |
|       - | 2028 | `/*` |
|       - | 2029 | ` * Compile the so hated 'goto' statement.` |
|       - | 2030 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - | 2031 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - | 2032 | ` * a compiler it has to do this.` |
|       - | 2033 | ` * According to the PHP language reference manual` |
|       - | 2034 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - | 2035 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - | 2036 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - | 2037 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - | 2038 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - | 2039 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - | 2040 | ` *   of a multi-level break` |
|       - | 2041 | ` */` |
|     152 | 2042 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 | 2043 |  |
|       - | 2044 | `	JumpFixup sJump;` |
|       - | 2045 | `	sxi32 rc;` |
|     154 | 2046 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 | 2047 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2048 | `		/* Missing label */` |
|     ! 0 | 2049 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 | 2050 | `		if( rc == SXERR_ABORT ){` |
|       - | 2051 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2052 | `			return SXERR_ABORT;` |
|       - | 2053 | `		}` |
|     ! 0 | 2054 | `		return SXRET_OK;` |
|       - | 2055 | `	}` |
|     154 | 2056 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 | 2057 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 | 2058 | `		if( rc == SXERR_ABORT ){` |
|       - | 2059 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2060 | `			return SXERR_ABORT;` |
|       - | 2061 | `		}` |
|       3 | 2062 | `	}else{` |
|     150 | 2063 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 2064 | `		GenBlock *pBlock;` |
|       - | 2065 | `		char *zDup;` |
|       - | 2066 | `		/* Prepare the jump destination */` |
|     150 | 2067 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 | 2068 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - | 2069 | `		/* Duplicate label name */` |
|     150 | 2070 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 | 2071 | `		if( zDup == 0 ){` |
|     ! 0 | 2072 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2073 | `			return SXERR_ABORT;` |
|       - | 2074 | `		}` |
|     150 | 2075 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 | 2076 | `		pBlock = pGen->pCurrent;` |
|     312 | 2077 | `		while( pBlock ){` |
|     196 | 2078 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 | 2079 | `				break;` |
|       - | 2080 | `			}` |
|       - | 2081 | `			/* Point to the upper block */` |
|     164 | 2082 | `			pBlock = pBlock->pParent;` |
|       2 | 2083 | `		}` |
|     150 | 2084 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 | 2085 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 | 2086 | `			if( rc == SXERR_ABORT ){` |
|       - | 2087 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2088 | `				return SXERR_ABORT;` |
|       - | 2089 | `			}` |
|       3 | 2090 | `		}` |
|     150 | 2091 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 | 2092 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 | 2093 | `		}else{` |
|     124 | 2094 | `			sJump.pFunc = 0;` |
|       - | 2095 | `		}` |
|       - | 2096 | `		/* Emit the unconditional jump */` |
|     150 | 2097 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 | 2098 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 | 2099 | `		}` |
|       - | 2100 | `	}` |
|     154 | 2101 | `	pGen->pIn++; /* Jump the label name */` |
|     154 | 2102 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 | 2103 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 | 2104 | `	}` |
|       - | 2105 | `	/* Statement successfully compiled */` |
|     154 | 2106 | `	return SXRET_OK;` |
|      78 | 2107 |  |
|       - | 2108 | `/*` |
|       - | 2109 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - | 2110 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - | 2111 | ` * failure.` |
|       - | 2112 | ` */` |
|      20 | 2113 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 | 2114 |  |
|       - | 2115 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - | 2116 | `	sxu32 nRawObj;` |
|      10 | 2117 | `	sxu32 nObjIdx;` |
|       - | 2118 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - | 2119 | `	 * a PHP block.` |
|       - | 2120 | `	 */` |
|      10 | 2121 | `Consume:` |
|      21 | 2122 | `	nRawObj = nObjIdx = 0;` |
|      21 | 2123 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 | 2124 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 | 2125 | `		if( pRawObj == 0 ){` |
|     ! 0 | 2126 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2127 | `			return SXERR_ABORT;` |
|       - | 2128 | `		}` |
|       - | 2129 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 | 2130 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 | 2131 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 | 2132 | `		++nRawObj;` |
|     ! 0 | 2133 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 | 2134 | `	}` |
|      21 | 2135 | `	if( nRawObj > 0 ){` |
|       - | 2136 | `		/* Emit the consume instruction */` |
|     ! 0 | 2137 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 | 2138 | `	}` |
|      21 | 2139 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 | 2140 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - | 2141 | `		/* Reset the token set */` |
|     ! 0 | 2142 | `		SySetReset(pTokenSet);` |
|       - | 2143 | `		/* Tokenize input */` |
|     ! 0 | 2144 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 | 2145 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - | 2146 | `		/* Point to the fresh token stream */` |
|     ! 0 | 2147 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 | 2148 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - | 2149 | `		/* Advance the stream cursor */` |
|     ! 0 | 2150 | `		pGen->pRawIn++;` |
|       - | 2151 | `		/* TICKET 1433-011 */` |
|     ! 0 | 2152 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 2153 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 2154 | `			sxi32 rc;` |
|       - | 2155 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 | 2156 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 | 2157 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 | 2158 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 | 2159 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 2160 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2161 | `				return SXERR_ABORT;` |
|     ! 0 | 2162 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 | 2163 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 2164 | `			}` |
|     ! 0 | 2165 | `			goto Consume;` |
|       - | 2166 | `		}` |
|     ! 0 | 2167 | `	}else{` |
|       - | 2168 | `		/* No more chunks to process */` |
|      21 | 2169 | `		pGen->pIn = pGen->pEnd;` |
|      21 | 2170 | `		return SXERR_EOF;` |
|       - | 2171 | `	}` |
|     ! 0 | 2172 | `	return SXRET_OK;` |
|      11 | 2173 |  |
|       - | 2174 | `/*` |
|       - | 2175 | ` * Compile a PHP block.` |
|       - | 2176 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - | 2177 | ` * optionally delimited by braces {}.` |
|       - | 2178 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 2179 | ` * and this function takes care of generating the appropriate error` |
|       - | 2180 | ` * message.` |
|       - | 2181 | ` */` |
|  221806 | 2182 | `static sxi32 PH7_CompileBlock(` |
|       - | 2183 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2184 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2185 | `	)` |
|       2 | 2186 |  |
|       - | 2187 | `	sxi32 rc;` |
|       - | 2188 | `	sxu32 nLine;` |
|  221808 | 2189 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  220514 | 2190 | `		nLine = pGen->pIn->nLine;` |
|  220514 | 2191 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  220514 | 2192 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2193 | `			return SXERR_ABORT;` |
|       - | 2194 | `		}` |
|  220514 | 2195 | `		pGen->pIn++;` |
|       - | 2196 | `		/* Compile until we hit the closing braces '}' */` |
|  322051 | 2197 | `		for(;;){` |
|  644104 | 2198 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 | 2199 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 | 2200 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2201 | `			 	   return SXERR_ABORT;` |
|       - | 2202 | `				}` |
|      21 | 2203 | `				if( rc == SXERR_EOF ){` |
|       - | 2204 | `					/* No more token to process. Missing closing braces */` |
|      21 | 2205 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 | 2206 | `					break;` |
|       - | 2207 | `				}` |
|     ! 0 | 2208 | `			}` |
|  644084 | 2209 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2210 | `				/* Closing braces found,break immediately*/` |
|  220494 | 2211 | `				pGen->pIn++;` |
|  220494 | 2212 | `				break;` |
|       - | 2213 | `			}` |
|       - | 2214 | `			/* Compile a single statement */` |
|  423592 | 2215 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  423592 | 2216 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2217 | `				return SXERR_ABORT;` |
|       - | 2218 | `			}` |
|       2 | 2219 | `		}` |
|  220514 | 2220 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  111552 | 2221 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 | 2222 | `		pGen->pIn++;` |
|     ! 0 | 2223 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 | 2224 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2225 | `			return SXERR_ABORT;` |
|       - | 2226 | `		}` |
|       - | 2227 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 | 2228 | `		for(;;){` |
|     ! 0 | 2229 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2230 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 | 2231 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2232 | `			 	   return SXERR_ABORT;` |
|       - | 2233 | `				}` |
|     ! 0 | 2234 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - | 2235 | `					/* No more token to process */` |
|     ! 0 | 2236 | `					if( rc == SXERR_EOF ){` |
|     ! 0 | 2237 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - | 2238 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 | 2239 | `					}` |
|     ! 0 | 2240 | `					break;` |
|       - | 2241 | `				}` |
|     ! 0 | 2242 | `			}` |
|     ! 0 | 2243 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 2244 | `				sxi32 nKwrd;` |
|       - | 2245 | `				/* Keyword found */` |
|     ! 0 | 2246 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 | 2247 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 | 2248 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - | 2249 | `						/* Delimiter keyword found,break */` |
|     ! 0 | 2250 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 | 2251 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 | 2252 | `						}` |
|     ! 0 | 2253 | `						break;` |
|       - | 2254 | `				}` |
|     ! 0 | 2255 | `			}` |
|       - | 2256 | `			/* Compile a single statement */` |
|     ! 0 | 2257 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 | 2258 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2259 | `				return SXERR_ABORT;` |
|       - | 2260 | `			}` |
|     ! 0 | 2261 | `		}` |
|     ! 0 | 2262 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 | 2263 | `	}else{` |
|       - | 2264 | `		/* Compile a single statement */` |
|    1296 | 2265 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1296 | 2266 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2267 | `			return SXERR_ABORT;` |
|       - | 2268 | `		}` |
|       - | 2269 | `	}` |
|       - | 2270 | `	/* Jump trailing semi-colons ';' */` |
|  221808 | 2271 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2272 | `		pGen->pIn++;` |
|     ! 0 | 2273 | `	}` |
|  221808 | 2274 | `	return SXRET_OK;` |
|  110905 | 2275 |  |
|       - | 2276 | `/*` |
|       - | 2277 | ` * Compile the gentle 'while' statement.` |
|       - | 2278 | ` * According to the PHP language reference` |
|       - | 2279 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - | 2280 | ` *  The basic form of a while statement is:` |
|       - | 2281 | ` *  while (expr)` |
|       - | 2282 | ` *   statement` |
|       - | 2283 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - | 2284 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - | 2285 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - | 2286 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - | 2287 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - | 2288 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - | 2289 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - | 2290 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - | 2291 | ` *  while (expr):` |
|       - | 2292 | ` *    statement` |
|       - | 2293 | ` *   endwhile;` |
|       - | 2294 | ` */` |
|    9812 | 2295 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 2296 |  |
|    9814 | 2297 | `	GenBlock *pWhileBlock = 0;` |
|    9814 | 2298 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2299 | `	sxu32 nFalseJump;` |
|       - | 2300 | `	sxu32 nLine;` |
|       - | 2301 | `	sxi32 rc;` |
|    9814 | 2302 | `	nLine = pGen->pIn->nLine;` |
|       - | 2303 | `	/* Jump the 'while' keyword */` |
|    9814 | 2304 | `	pGen->pIn++;` |
|    9814 | 2305 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2306 | `		/* Syntax error */` |
|     ! 0 | 2307 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2308 | `		if( rc == SXERR_ABORT ){` |
|       - | 2309 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2310 | `			return SXERR_ABORT;` |
|       - | 2311 | `		}` |
|     ! 0 | 2312 | `		goto Synchronize;` |
|       - | 2313 | `	}` |
|       - | 2314 | `	/* Jump the left parenthesis '(' */` |
|    9814 | 2315 | `	pGen->pIn++;` |
|       - | 2316 | `	/* Create the loop block */` |
|    9814 | 2317 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    9814 | 2318 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2319 | `		return SXERR_ABORT;` |
|       - | 2320 | `	}` |
|       - | 2321 | `	/* Delimit the condition */` |
|    9814 | 2322 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    9814 | 2323 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2324 | `		/* Empty expression */` |
|       3 | 2325 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2326 | `		if( rc == SXERR_ABORT ){` |
|       - | 2327 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2328 | `			return SXERR_ABORT;` |
|       - | 2329 | `		}` |
|       1 | 2330 | `	}` |
|       - | 2331 | `	/* Swap token streams */` |
|    9814 | 2332 | `	pTmp = pGen->pEnd;` |
|    9814 | 2333 | `	pGen->pEnd = pEnd;` |
|       - | 2334 | `	/* Compile the expression */` |
|    9814 | 2335 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    9814 | 2336 | `	if( rc == SXERR_ABORT ){` |
|       - | 2337 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2338 | `		return SXERR_ABORT;` |
|       - | 2339 | `	}` |
|       - | 2340 | `	/* Update token stream */` |
|    9814 | 2341 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2342 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2343 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2344 | `			return SXERR_ABORT;` |
|       - | 2345 | `		}` |
|     ! 0 | 2346 | `		pGen->pIn++;` |
|     ! 0 | 2347 | `	}` |
|       - | 2348 | `	/* Synchronize pointers */` |
|    9814 | 2349 | `	pGen->pIn  = &pEnd[1];` |
|    9814 | 2350 | `	pGen->pEnd = pTmp;` |
|       - | 2351 | `	/* Emit the false jump */` |
|    9814 | 2352 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2353 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    9814 | 2354 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2355 | `	/* Compile the loop body */` |
|    9814 | 2356 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    9814 | 2357 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2358 | `		return SXERR_ABORT;` |
|       - | 2359 | `	}` |
|       - | 2360 | `	/* Emit the unconditional jump to the start of the loop */` |
|    9814 | 2361 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2362 | `	/* Fix all jumps now the destination is resolved */` |
|    9814 | 2363 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2364 | `	/* Release the loop block */` |
|    9814 | 2365 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2366 | `	/* Statement successfully compiled */` |
|    9814 | 2367 | `	return SXRET_OK;` |
|     ! 0 | 2368 | `Synchronize:` |
|       - | 2369 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2370 | `	 * compiling this erroneous block.` |
|       - | 2371 | `	 */` |
|     ! 0 | 2372 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2373 | `		pGen->pIn++;` |
|     ! 0 | 2374 | `	}` |
|     ! 0 | 2375 | `	return SXRET_OK;` |
|    4908 | 2376 |  |
|       - | 2377 | `/*` |
|       - | 2378 | ` * Compile the ugly do..while() statement.` |
|       - | 2379 | ` * According to the PHP language reference` |
|       - | 2380 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - | 2381 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - | 2382 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - | 2383 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - | 2384 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - | 2385 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - | 2386 | ` *  would end immediately).` |
|       - | 2387 | ` *  There is just one syntax for do-while loops:` |
|       - | 2388 | ` *  <?php` |
|       - | 2389 | ` *  $i = 0;` |
|       - | 2390 | ` *  do {` |
|       - | 2391 | ` *   echo $i;` |
|       - | 2392 | ` *  } while ($i > 0);` |
|       - | 2393 | ` * ?>` |
|       - | 2394 | ` */` |
|       2 | 2395 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 | 2396 |  |
|       3 | 2397 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 | 2398 | `	GenBlock *pDoBlock = 0;` |
|       - | 2399 | `	sxu32 nLine;` |
|       - | 2400 | `	sxi32 rc;` |
|       3 | 2401 | `	nLine = pGen->pIn->nLine;` |
|       - | 2402 | `	/* Jump the 'do' keyword */` |
|       3 | 2403 | `	pGen->pIn++;` |
|       - | 2404 | `	/* Create the loop block */` |
|       3 | 2405 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 | 2406 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2407 | `		return SXERR_ABORT;` |
|       - | 2408 | `	}` |
|       - | 2409 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 | 2410 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 | 2411 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 | 2412 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2413 | `		return SXERR_ABORT;` |
|       - | 2414 | `	}` |
|       3 | 2415 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2416 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 | 2417 | `	}` |
|       3 | 2418 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 | 2419 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - | 2420 | `			/* Missing 'while' statement */` |
|       3 | 2421 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 | 2422 | `			if( rc == SXERR_ABORT ){` |
|       - | 2423 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2424 | `				return SXERR_ABORT;` |
|       - | 2425 | `			}` |
|       3 | 2426 | `			goto Synchronize;` |
|       - | 2427 | `	}` |
|       - | 2428 | `	/* Jump the 'while' keyword */` |
|     ! 0 | 2429 | `	pGen->pIn++;` |
|     ! 0 | 2430 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2431 | `		/* Syntax error */` |
|     ! 0 | 2432 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2433 | `		if( rc == SXERR_ABORT ){` |
|       - | 2434 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2435 | `			return SXERR_ABORT;` |
|       - | 2436 | `		}` |
|     ! 0 | 2437 | `		goto Synchronize;` |
|       - | 2438 | `	}` |
|       - | 2439 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 | 2440 | `	pGen->pIn++;` |
|       - | 2441 | `	/* Delimit the condition */` |
|     ! 0 | 2442 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 | 2443 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2444 | `		/* Empty expression */` |
|     ! 0 | 2445 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 | 2446 | `		if( rc == SXERR_ABORT ){` |
|       - | 2447 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2448 | `			return SXERR_ABORT;` |
|       - | 2449 | `		}` |
|     ! 0 | 2450 | `		goto Synchronize;` |
|       - | 2451 | `	}` |
|       - | 2452 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 | 2453 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - | 2454 | `		JumpFixup *aPost;` |
|       - | 2455 | `		VmInstr *pInstr;` |
|       - | 2456 | `		sxu32 nJumpDest;` |
|       - | 2457 | `		sxu32 n;` |
|     ! 0 | 2458 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 | 2459 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 | 2460 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 | 2461 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 | 2462 | `			if( pInstr ){` |
|       - | 2463 | `				/* Fix */` |
|     ! 0 | 2464 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 | 2465 | `			}` |
|     ! 0 | 2466 | `		}` |
|     ! 0 | 2467 | `	}` |
|       - | 2468 | `	/* Swap token streams */` |
|     ! 0 | 2469 | `	pTmp = pGen->pEnd;` |
|     ! 0 | 2470 | `	pGen->pEnd = pEnd;` |
|       - | 2471 | `	/* Compile the expression */` |
|     ! 0 | 2472 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 2473 | `	if( rc == SXERR_ABORT ){` |
|       - | 2474 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2475 | `		return SXERR_ABORT;` |
|       - | 2476 | `	}` |
|       - | 2477 | `	/* Update token stream */` |
|     ! 0 | 2478 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2479 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2480 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2481 | `			return SXERR_ABORT;` |
|       - | 2482 | `		}` |
|     ! 0 | 2483 | `		pGen->pIn++;` |
|     ! 0 | 2484 | `	}` |
|     ! 0 | 2485 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 | 2486 | `	pGen->pEnd = pTmp;` |
|       - | 2487 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 | 2488 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - | 2489 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 | 2490 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2491 | `	/* Release the loop block */` |
|     ! 0 | 2492 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2493 | `	/* Statement successfully compiled */` |
|     ! 0 | 2494 | `	return SXRET_OK;` |
|       1 | 2495 | `Synchronize:` |
|       - | 2496 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2497 | `	 * compiling this erroneous block.` |
|       - | 2498 | `	 */` |
|       3 | 2499 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2500 | `		pGen->pIn++;` |
|     ! 0 | 2501 | `	}` |
|       3 | 2502 | `	return SXRET_OK;` |
|       2 | 2503 |  |
|       - | 2504 | `/*` |
|       - | 2505 | ` * Compile the complex and powerful 'for' statement.` |
|       - | 2506 | ` * According to the PHP language reference` |
|       - | 2507 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - | 2508 | ` *  The syntax of a for loop is:` |
|       - | 2509 | ` *  for (expr1; expr2; expr3)` |
|       - | 2510 | ` *   statement` |
|       - | 2511 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - | 2512 | ` *  the beginning of the loop.` |
|       - | 2513 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - | 2514 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - | 2515 | ` *  to FALSE, the execution of the loop ends.` |
|       - | 2516 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - | 2517 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - | 2518 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - | 2519 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - | 2520 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - | 2521 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - | 2522 | ` *  of using the for truth expression.` |
|       - | 2523 | ` */` |
|    9814 | 2524 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 2525 |  |
|    9816 | 2526 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    9816 | 2527 | `	GenBlock *pForBlock = 0;` |
|       - | 2528 | `	sxu32 nFalseJump;` |
|       - | 2529 | `	sxu32 nLine;` |
|       - | 2530 | `	sxi32 rc;` |
|    9816 | 2531 | `	nLine = pGen->pIn->nLine;` |
|       - | 2532 | `	/* Jump the 'for' keyword */` |
|    9816 | 2533 | `	pGen->pIn++;` |
|    9816 | 2534 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2535 | `		/* Syntax error */` |
|     ! 0 | 2536 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2537 | `		if( rc == SXERR_ABORT ){` |
|       - | 2538 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2539 | `			return SXERR_ABORT;` |
|       - | 2540 | `		}` |
|     ! 0 | 2541 | `		return SXRET_OK;` |
|       - | 2542 | `	}` |
|       - | 2543 | `	/* Jump the left parenthesis '(' */` |
|    9816 | 2544 | `	pGen->pIn++;` |
|       - | 2545 | `	/* Delimit the init-expr;condition;post-expr */` |
|    9816 | 2546 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    9816 | 2547 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2548 | `		/* Empty expression */` |
|     ! 0 | 2549 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 | 2550 | `		if( rc == SXERR_ABORT ){` |
|       - | 2551 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2552 | `			return SXERR_ABORT;` |
|       - | 2553 | `		}` |
|       - | 2554 | `		/* Synchronize */` |
|     ! 0 | 2555 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2556 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2557 | `			pGen->pIn++;` |
|     ! 0 | 2558 | `		}` |
|     ! 0 | 2559 | `		return SXRET_OK;` |
|       - | 2560 | `	}` |
|       - | 2561 | `	/* Swap token streams */` |
|    9816 | 2562 | `	pTmp = pGen->pEnd;` |
|    9816 | 2563 | `	pGen->pEnd = pEnd;` |
|       - | 2564 | `	/* Compile initialization expressions if available */` |
|    9816 | 2565 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2566 | `	/* Pop operand lvalues */` |
|    9816 | 2567 | `	if( rc == SXERR_ABORT ){` |
|       - | 2568 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2569 | `		return SXERR_ABORT;` |
|    9816 | 2570 | `	}else if( rc != SXERR_EMPTY ){` |
|    9814 | 2571 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    4906 | 2572 | `	}` |
|    9816 | 2573 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2574 | `		/* Syntax error */` |
|     ! 0 | 2575 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2576 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 | 2577 | `		if( rc == SXERR_ABORT ){` |
|       - | 2578 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2579 | `			return SXERR_ABORT;` |
|       - | 2580 | `		}` |
|     ! 0 | 2581 | `		return SXRET_OK;` |
|       - | 2582 | `	}` |
|       - | 2583 | `	/* Jump the trailing ';' */` |
|    9816 | 2584 | `	pGen->pIn++;` |
|       - | 2585 | `	/* Create the loop block */` |
|    9816 | 2586 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    9816 | 2587 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2588 | `		return SXERR_ABORT;` |
|       - | 2589 | `	}` |
|       - | 2590 | `	/* Deffer continue jumps */` |
|    9816 | 2591 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 2592 | `	/* Compile the condition */` |
|    9816 | 2593 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    9816 | 2594 | `	if( rc == SXERR_ABORT ){` |
|       - | 2595 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2596 | `		return SXERR_ABORT;` |
|    9816 | 2597 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 2598 | `		/* Emit the false jump */` |
|    9814 | 2599 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2600 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    9814 | 2601 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    4906 | 2602 | `	}` |
|    9816 | 2603 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2604 | `		/* Syntax error */` |
|       5 | 2605 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2606 | `			"for: Expected ';' after conditionals expressions");` |
|       5 | 2607 | `		if( rc == SXERR_ABORT ){` |
|       - | 2608 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2609 | `			return SXERR_ABORT;` |
|       - | 2610 | `		}` |
|       5 | 2611 | `		return SXRET_OK;` |
|       - | 2612 | `	}` |
|       - | 2613 | `	/* Jump the trailing ';' */` |
|    9812 | 2614 | `	pGen->pIn++;` |
|       - | 2615 | `	/* Save the post condition stream */` |
|    9812 | 2616 | `	pPostStart = pGen->pIn;` |
|       - | 2617 | `	/* Compile the loop body */` |
|    9812 | 2618 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    9812 | 2619 | `	pGen->pEnd = pTmp;` |
|    9812 | 2620 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    9812 | 2621 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2622 | `		return SXERR_ABORT;` |
|       - | 2623 | `	}` |
|       - | 2624 | `	/* Fix post-continue jumps */` |
|    9812 | 2625 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 2626 | `		JumpFixup *aPost;` |
|       - | 2627 | `		VmInstr *pInstr;` |
|       - | 2628 | `		sxu32 nJumpDest;` |
|       - | 2629 | `		sxu32 n;` |
|       8 | 2630 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|       8 | 2631 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      14 | 2632 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|       8 | 2633 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       8 | 2634 | `			if( pInstr ){` |
|       - | 2635 | `				/* Fix jump */` |
|       8 | 2636 | `				pInstr->iP2 = nJumpDest;` |
|       3 | 2637 | `			}` |
|       5 | 2638 | `		}` |
|       3 | 2639 | `	}` |
|       - | 2640 | `	/* compile the post-expressions if available */` |
|    9812 | 2641 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2642 | `		pPostStart++;` |
|     ! 0 | 2643 | `	}` |
|    9812 | 2644 | `	if( pPostStart < pEnd ){` |
|       - | 2645 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    9812 | 2646 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    9812 | 2647 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    9812 | 2648 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 2649 | `			/* Syntax error */` |
|     ! 0 | 2650 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 2651 | `			if( rc == SXERR_ABORT ){` |
|       - | 2652 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2653 | `				return SXERR_ABORT;` |
|       - | 2654 | `			}` |
|     ! 0 | 2655 | `			return SXRET_OK;` |
|       - | 2656 | `		}` |
|    9812 | 2657 | `		RE_SWAP_DELIMITER(pGen);` |
|    9812 | 2658 | `		if( rc == SXERR_ABORT ){` |
|       - | 2659 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2660 | `			return SXERR_ABORT;` |
|    9812 | 2661 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 2662 | `			/* Pop operand lvalue */` |
|    9812 | 2663 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    4905 | 2664 | `		}` |
|    4905 | 2665 | `	}` |
|       - | 2666 | `	/* Emit the unconditional jump to the start of the loop */` |
|    9812 | 2667 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 2668 | `	/* Fix all jumps now the destination is resolved */` |
|    9812 | 2669 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2670 | `	/* Release the loop block */` |
|    9812 | 2671 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2672 | `	/* Statement successfully compiled */` |
|    9812 | 2673 | `	return SXRET_OK;` |
|    4909 | 2674 |  |
|       - | 2675 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 2676 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 2677 | ` * are allowed.` |
|       - | 2678 | ` */` |
|    5206 | 2679 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 2680 |  |
|    5208 | 2681 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    5208 | 2682 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 2683 | `		/* Unexpected expression */` |
|     ! 0 | 2684 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 2685 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 2686 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 2687 | `			rc = SXERR_INVALID;` |
|     ! 0 | 2688 | `		}` |
|     ! 0 | 2689 | `	}` |
|    5208 | 2690 | `	return rc;` |
|       2 | 2691 |  |
|       - | 2692 | `/*` |
|       - | 2693 | ` * Compile the 'foreach' statement.` |
|       - | 2694 | ` * According to the PHP language reference` |
|       - | 2695 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - | 2696 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - | 2697 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - | 2698 | ` *  is a minor but useful extension of the first:` |
|       - | 2699 | ` *  foreach (array_expression as $value)` |
|       - | 2700 | ` *    statement` |
|       - | 2701 | ` *  foreach (array_expression as $key => $value)` |
|       - | 2702 | ` *   statement` |
|       - | 2703 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - | 2704 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - | 2705 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - | 2706 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - | 2707 | ` *  to the variable $key on each loop.` |
|       - | 2708 | ` *  Note:` |
|       - | 2709 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - | 2710 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - | 2711 | ` *  Note:` |
|       - | 2712 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - | 2713 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - | 2714 | ` *  or after the foreach without resetting it.` |
|       - | 2715 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - | 2716 | ` *  of copying the value.` |
|       - | 2717 | ` */` |
|    2626 | 2718 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 2719 |  |
|    2628 | 2720 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    2628 | 2721 | `	GenBlock *pForeachBlock = 0;` |
|       - | 2722 | `	ph7_foreach_info *pInfo;` |
|       - | 2723 | `	sxu32 nFalseJump;` |
|       - | 2724 | `	VmInstr *pInstr;` |
|       - | 2725 | `	sxu32 nLine;` |
|       - | 2726 | `	sxi32 rc;` |
|    2628 | 2727 | `	nLine = pGen->pIn->nLine;` |
|       - | 2728 | `	/* Jump the 'foreach' keyword */` |
|    2628 | 2729 | `	pGen->pIn++;` |
|    2628 | 2730 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2731 | `		/* Syntax error */` |
|     ! 0 | 2732 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 2733 | `		if( rc == SXERR_ABORT ){` |
|       - | 2734 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2735 | `			return SXERR_ABORT;` |
|       - | 2736 | `		}` |
|     ! 0 | 2737 | `		goto Synchronize;` |
|       - | 2738 | `	}` |
|       - | 2739 | `	/* Jump the left parenthesis '(' */` |
|    2628 | 2740 | `	pGen->pIn++;` |
|       - | 2741 | `	/* Create the loop block */` |
|    2628 | 2742 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    2628 | 2743 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2744 | `		return SXERR_ABORT;` |
|       - | 2745 | `	}` |
|       - | 2746 | `	/* Delimit the expression */` |
|    2628 | 2747 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    2628 | 2748 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2749 | `		/* Empty expression */` |
|     ! 0 | 2750 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 2751 | `		if( rc == SXERR_ABORT ){` |
|       - | 2752 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2753 | `			return SXERR_ABORT;` |
|       - | 2754 | `		}` |
|       - | 2755 | `		/* Synchronize */` |
|     ! 0 | 2756 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2757 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2758 | `			pGen->pIn++;` |
|     ! 0 | 2759 | `		}` |
|     ! 0 | 2760 | `		return SXRET_OK;` |
|       - | 2761 | `	}` |
|       - | 2762 | `	/* Compile the array expression */` |
|    2628 | 2763 | `	pCur = pGen->pIn;` |
|   17672 | 2764 | `	while( pCur < pEnd ){` |
|   17672 | 2765 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    2638 | 2766 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    2638 | 2767 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 2768 | `				/* Break with the first 'as' found */` |
|    2628 | 2769 | `				break;` |
|       - | 2770 | `			}` |
|       5 | 2771 | `		}` |
|       - | 2772 | `		/* Advance the stream cursor */` |
|   15046 | 2773 | `		pCur++;` |
|       2 | 2774 | `	}` |
|    2628 | 2775 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 2776 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2777 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 2778 | `		if( rc == SXERR_ABORT ){` |
|       - | 2779 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2780 | `			return SXERR_ABORT;` |
|       - | 2781 | `		}` |
|     ! 0 | 2782 | `		goto Synchronize;` |
|       - | 2783 | `	}` |
|       - | 2784 | `	/* Swap token streams */` |
|    2628 | 2785 | `	pTmp = pGen->pEnd;` |
|    2628 | 2786 | `	pGen->pEnd = pCur;` |
|    2628 | 2787 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    2628 | 2788 | `	if( rc == SXERR_ABORT ){` |
|       - | 2789 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2790 | `		return SXERR_ABORT;` |
|       - | 2791 | `	}` |
|       - | 2792 | `	/* Update token stream */` |
|    2628 | 2793 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 2794 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2795 | `		if( rc == SXERR_ABORT ){` |
|       - | 2796 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2797 | `			return SXERR_ABORT;` |
|       - | 2798 | `		}` |
|     ! 0 | 2799 | `		pGen->pIn++;` |
|     ! 0 | 2800 | `	}` |
|    2628 | 2801 | `	pCur++; /* Jump the 'as' keyword */` |
|    2628 | 2802 | `	pGen->pIn = pCur;` |
|    2628 | 2803 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2804 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 2805 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2806 | `			return SXERR_ABORT;` |
|       - | 2807 | `		}` |
|     ! 0 | 2808 | `	}` |
|       - | 2809 | `	/* Create the foreach context */` |
|    2628 | 2810 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    2628 | 2811 | `	if( pInfo == 0 ){` |
|     ! 0 | 2812 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 2813 | `		return SXERR_ABORT;` |
|       - | 2814 | `	}` |
|       - | 2815 | `	/* Zero the structure */` |
|    2628 | 2816 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 2817 | `	/* Initialize structure fields */` |
|    2628 | 2818 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 2819 | `	/* Check if we have a key field */` |
|    7882 | 2820 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    5256 | 2821 | `		pCur++;` |
|       2 | 2822 | `	}` |
|    2628 | 2823 | `	if( pCur < pEnd ){` |
|       - | 2824 | `		/* Compile the expression holding the key name */` |
|    2582 | 2825 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 2826 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 2827 | `			if( rc == SXERR_ABORT ){` |
|       - | 2828 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2829 | `				return SXERR_ABORT;` |
|       - | 2830 | `			}` |
|     ! 0 | 2831 | `		}else{` |
|    2582 | 2832 | `			pGen->pEnd = pCur;` |
|    2582 | 2833 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2582 | 2834 | `			if( rc == SXERR_ABORT ){` |
|       - | 2835 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2836 | `				return SXERR_ABORT;` |
|       - | 2837 | `			}` |
|    2582 | 2838 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2582 | 2839 | `			if( pInstr->p3 ){` |
|       - | 2840 | `				/* Record key name */` |
|    2582 | 2841 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1290 | 2842 | `			}` |
|    2582 | 2843 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 2844 | `		}` |
|    2582 | 2845 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1290 | 2846 | `	}` |
|    2628 | 2847 | `	pGen->pEnd = pEnd;` |
|    2628 | 2848 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2849 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 2850 | `		if( rc == SXERR_ABORT ){` |
|       - | 2851 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2852 | `			return SXERR_ABORT;` |
|       - | 2853 | `		}` |
|     ! 0 | 2854 | `		goto Synchronize;` |
|       - | 2855 | `	}` |
|    2628 | 2856 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       7 | 2857 | `		pGen->pIn++;` |
|       - | 2858 | `		/* Pass by reference  */` |
|       7 | 2859 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       3 | 2860 | `	}` |
|       - | 2861 | `	/* Compile the expression holding the value name */` |
|    2628 | 2862 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2628 | 2863 | `	if( rc == SXERR_ABORT ){` |
|       - | 2864 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2865 | `		return SXERR_ABORT;` |
|       - | 2866 | `	}` |
|    2628 | 2867 | `	pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2628 | 2868 | `	if( pInstr->p3 ){` |
|       - | 2869 | `		/* Record value name */` |
|    2628 | 2870 | `		SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1313 | 2871 | `	}` |
|       - | 2872 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    2628 | 2873 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 2874 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2628 | 2875 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 2876 | `	/* Record the first instruction to execute */` |
|    2628 | 2877 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2878 | `	/* Emit the FOREACH_STEP instruction */` |
|    2628 | 2879 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 2880 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2628 | 2881 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 2882 | `	/* Compile the loop body */` |
|    2628 | 2883 | `	pGen->pIn = &pEnd[1];` |
|    2628 | 2884 | `	pGen->pEnd = pTmp;` |
|    2628 | 2885 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    2628 | 2886 | `	if( rc == SXERR_ABORT ){` |
|       - | 2887 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2888 | `		return SXERR_ABORT;` |
|       - | 2889 | `	}` |
|       - | 2890 | `	/* Emit the unconditional jump to the start of the loop */` |
|    2628 | 2891 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 2892 | `	/* Fix all jumps now the destination is resolved */` |
|    2628 | 2893 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2894 | `	/* Release the loop block */` |
|    2628 | 2895 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2896 | `	/* Statement successfully compiled */` |
|    2628 | 2897 | `	return SXRET_OK;` |
|     ! 0 | 2898 | `Synchronize:` |
|       - | 2899 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2900 | `	 * compiling this erroneous block.` |
|       - | 2901 | `	 */` |
|     ! 0 | 2902 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2903 | `		pGen->pIn++;` |
|     ! 0 | 2904 | `	}` |
|     ! 0 | 2905 | `	return SXRET_OK;` |
|    1315 | 2906 |  |
|       - | 2907 | `/*` |
|       - | 2908 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 2909 | ` * According to the PHP language reference` |
|       - | 2910 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 2911 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 2912 | ` *  that is similar to that of C:` |
|       - | 2913 | ` *  if (expr)` |
|       - | 2914 | ` *   statement` |
|       - | 2915 | ` *  else construct:` |
|       - | 2916 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 2917 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 2918 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 2919 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 2920 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 2921 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 2922 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 2923 | ` *  elseif` |
|       - | 2924 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 2925 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 2926 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 2927 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 2928 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 2929 | ` *   <?php` |
|       - | 2930 | ` *    if ($a > $b) {` |
|       - | 2931 | ` *     echo "a is bigger than b";` |
|       - | 2932 | ` *    } elseif ($a == $b) {` |
|       - | 2933 | ` *     echo "a is equal to b";` |
|       - | 2934 | ` *    } else {` |
|       - | 2935 | ` *     echo "a is smaller than b";` |
|       - | 2936 | ` *    }` |
|       - | 2937 | ` *    ?>` |
|       - | 2938 | ` */` |
|   97856 | 2939 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 2940 |  |
|   97858 | 2941 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   97858 | 2942 | `	GenBlock *pCondBlock = 0;` |
|       - | 2943 | `	sxu32 nJumpIdx;` |
|       - | 2944 | `	sxu32 nKeyID;` |
|       - | 2945 | `	sxi32 rc;` |
|       - | 2946 | `	/* Jump the 'if' keyword */` |
|   97858 | 2947 | `	pGen->pIn++;` |
|   97858 | 2948 | `	pToken = pGen->pIn;` |
|       - | 2949 | `	/* Create the conditional block */` |
|   97858 | 2950 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   97858 | 2951 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2952 | `		return SXERR_ABORT;` |
|       - | 2953 | `	}` |
|       - | 2954 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   53808 | 2955 | `	for(;;){` |
|  107618 | 2956 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2957 | `			/* Syntax error */` |
|     ! 0 | 2958 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 2959 | `				pToken--;` |
|     ! 0 | 2960 | `			}` |
|     ! 0 | 2961 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 2962 | `			if( rc == SXERR_ABORT ){` |
|       - | 2963 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2964 | `				return SXERR_ABORT;` |
|       - | 2965 | `			}` |
|     ! 0 | 2966 | `			goto Synchronize;` |
|       - | 2967 | `		}` |
|       - | 2968 | `		/* Jump the left parenthesis '(' */` |
|  107618 | 2969 | `		pToken++;` |
|       - | 2970 | `		/* Delimit the condition */` |
|  107618 | 2971 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  107618 | 2972 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 2973 | `			/* Syntax error */` |
|     ! 0 | 2974 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 2975 | `				pToken--;` |
|     ! 0 | 2976 | `			}` |
|     ! 0 | 2977 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 2978 | `			if( rc == SXERR_ABORT ){` |
|       - | 2979 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2980 | `				return SXERR_ABORT;` |
|       - | 2981 | `			}` |
|     ! 0 | 2982 | `			goto Synchronize;` |
|       - | 2983 | `		}` |
|       - | 2984 | `		/* Swap token streams */` |
|  107618 | 2985 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 2986 | `		/* Compile the condition */` |
|  107618 | 2987 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2988 | `		/* Update token stream */` |
|  107618 | 2989 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 2990 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2991 | `			pGen->pIn++;` |
|     ! 0 | 2992 | `		}` |
|  107618 | 2993 | `		pGen->pIn  = &pEnd[1];` |
|  107618 | 2994 | `		pGen->pEnd = pTmp;` |
|  107618 | 2995 | `		if( rc == SXERR_ABORT ){` |
|       - | 2996 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2997 | `			return SXERR_ABORT;` |
|       - | 2998 | `		}` |
|       - | 2999 | `		/* Emit the false jump */` |
|  107618 | 3000 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 3001 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  107618 | 3002 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 3003 | `		/* Compile the body */` |
|  107618 | 3004 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  107618 | 3005 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3006 | `			return SXERR_ABORT;` |
|       - | 3007 | `		}` |
|  107618 | 3008 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   28973 | 3009 | `			break;` |
|       - | 3010 | `		}` |
|       - | 3011 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   49676 | 3012 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   49676 | 3013 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   31894 | 3014 | `			break;` |
|       - | 3015 | `		}` |
|       - | 3016 | `		/* Emit the unconditional jump */` |
|   17784 | 3017 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 3018 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   17784 | 3019 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   17784 | 3020 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   12892 | 3021 | `			pToken = &pGen->pIn[1];` |
|   12892 | 3022 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    4898 | 3023 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4013 | 3024 | `					break;` |
|       - | 3025 | `			}` |
|    4870 | 3026 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2434 | 3027 | `		}` |
|    9762 | 3028 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 3029 | `		/* Synchronize cursors */` |
|    9762 | 3030 | `		pToken = pGen->pIn;` |
|       - | 3031 | `		/* Fix the false jump */` |
|    9762 | 3032 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 3033 | `	} /* For(;;) */` |
|       - | 3034 | `	/* Fix the false jump */` |
|   97858 | 3035 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   97858 | 3036 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   39914 | 3037 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 3038 | `			/* Compile the else block */` |
|    8024 | 3039 | `			pGen->pIn++;` |
|    8024 | 3040 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    8024 | 3041 | `			if( rc == SXERR_ABORT ){` |
|       - | 3042 |  |
|     ! 0 | 3043 | `				return SXERR_ABORT;` |
|       - | 3044 | `			}` |
|    4011 | 3045 | `	}` |
|   97858 | 3046 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3047 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   97858 | 3048 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 3049 | `	/* Release the conditional block */` |
|   97858 | 3050 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3051 | `	/* Statement successfully compiled */` |
|   97858 | 3052 | `	return SXRET_OK;` |
|     ! 0 | 3053 | `Synchronize:` |
|       - | 3054 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 3055 | `	 */` |
|     ! 0 | 3056 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3057 | `		pGen->pIn++;` |
|     ! 0 | 3058 | `	}` |
|     ! 0 | 3059 | `	return SXRET_OK;` |
|   48930 | 3060 |  |
|       - | 3061 | `/*` |
|       - | 3062 | ` * Compile the global construct.` |
|       - | 3063 | ` * According to the PHP language reference` |
|       - | 3064 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 3065 | ` *  to be used in that function.` |
|       - | 3066 | ` *  Example #1 Using global` |
|       - | 3067 | ` *  <?php` |
|       - | 3068 | ` *   $a = 1;` |
|       - | 3069 | ` *   $b = 2;` |
|       - | 3070 | ` *   function Sum()` |
|       - | 3071 | ` *   {` |
|       - | 3072 | ` *    global $a, $b;` |
|       - | 3073 | ` *    $b = $a + $b;` |
|       - | 3074 | ` *   }` |
|       - | 3075 | ` *   Sum();` |
|       - | 3076 | ` *   echo $b;` |
|       - | 3077 | ` *  ?>` |
|       - | 3078 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 3079 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 3080 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 3081 | ` */` |
|      26 | 3082 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 | 3083 |  |
|      28 | 3084 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3085 | `	sxi32 nExpr;` |
|       - | 3086 | `	sxi32 rc;` |
|       - | 3087 | `	/* Jump the 'global' keyword */` |
|      28 | 3088 | `	pGen->pIn++;` |
|      28 | 3089 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 3090 | `		/* Nothing to process */` |
|     ! 0 | 3091 | `		return SXRET_OK;` |
|       - | 3092 | `	}` |
|      28 | 3093 | `	pTmp = pGen->pEnd;` |
|      28 | 3094 | `	nExpr = 0;` |
|      56 | 3095 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      30 | 3096 | `		if( pGen->pIn < pNext ){` |
|      30 | 3097 | `			pGen->pEnd = pNext;` |
|      30 | 3098 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3099 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 3100 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 3101 | `					return SXERR_ABORT;` |
|       - | 3102 | `				}` |
|     ! 0 | 3103 | `			}else{` |
|      30 | 3104 | `				pGen->pIn++;` |
|      30 | 3105 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3106 | `					/* Emit a warning */` |
|     ! 0 | 3107 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 3108 | `				}else{` |
|      30 | 3109 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 | 3110 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3111 | `						return SXERR_ABORT;` |
|      30 | 3112 | `					}else if(rc != SXERR_EMPTY ){` |
|      30 | 3113 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      30 | 3114 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - | 3115 | `							/* Variable name, not a constant */` |
|      30 | 3116 | `							pLast->iP1 = 0;` |
|      14 | 3117 | `						}` |
|      30 | 3118 | `						nExpr++;` |
|      14 | 3119 | `					}` |
|       - | 3120 | `				}` |
|       - | 3121 | `			}` |
|      14 | 3122 | `		}` |
|       - | 3123 | `		/* Next expression in the stream */` |
|      30 | 3124 | `		pGen->pIn = pNext;` |
|       - | 3125 | `		/* Jump trailing commas */` |
|      32 | 3126 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3127 | `			pGen->pIn++;` |
|       1 | 3128 | `		}` |
|       2 | 3129 | `	}` |
|       - | 3130 | `	/* Restore token stream */` |
|      28 | 3131 | `	pGen->pEnd = pTmp;` |
|      28 | 3132 | `	if( nExpr > 0 ){` |
|       - | 3133 | `		/* Emit the uplink instruction */` |
|      28 | 3134 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      13 | 3135 | `	}` |
|      28 | 3136 | `	return SXRET_OK;` |
|      15 | 3137 |  |
|       - | 3138 | `/*` |
|       - | 3139 | ` * Compile the return statement.` |
|       - | 3140 | ` * According to the PHP language reference` |
|       - | 3141 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 3142 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 3143 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 3144 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 3145 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 3146 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 3147 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 3148 | ` *  from within the main script file, then script execution end.` |
|       - | 3149 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 3150 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 3151 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 3152 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 3153 | ` */` |
|  102696 | 3154 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 3155 |  |
|  102698 | 3156 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3157 | `	sxi32 rc;` |
|       - | 3158 | `	/* Jump the 'return' keyword */` |
|  102698 | 3159 | `	pGen->pIn++;` |
|  102698 | 3160 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3161 | `		/* Compile the expression */` |
|  102676 | 3162 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  102676 | 3163 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3164 | `			return SXERR_ABORT;` |
|  102676 | 3165 | `		}else if(rc != SXERR_EMPTY ){` |
|  102676 | 3166 | `			nRet = 1;` |
|   51337 | 3167 | `		}` |
|   51337 | 3168 | `	}` |
|       - | 3169 | `	/* Emit the done instruction */` |
|  102698 | 3170 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  102698 | 3171 | `	return SXRET_OK;` |
|   51350 | 3172 |  |
|       - | 3173 | `/*` |
|       - | 3174 | ` * Compile the die/exit language construct.` |
|       - | 3175 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 3176 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 3177 | ` */` |
|      88 | 3178 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 | 3179 |  |
|      90 | 3180 | `	sxi32 nExpr = 0;` |
|       - | 3181 | `	sxi32 rc;` |
|       - | 3182 | `	/* Jump the die/exit keyword */` |
|      90 | 3183 | `	pGen->pIn++;` |
|      90 | 3184 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3185 | `		/* Compile the expression */` |
|      90 | 3186 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 | 3187 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3188 | `			return SXERR_ABORT;` |
|      90 | 3189 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 | 3190 | `			nExpr = 1;` |
|      44 | 3191 | `		}` |
|      44 | 3192 | `	}` |
|       - | 3193 | `	/* Emit the HALT instruction */` |
|      90 | 3194 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 | 3195 | `	return SXRET_OK;` |
|      46 | 3196 |  |
|       - | 3197 | `/*` |
|       - | 3198 | ` * Compile the 'echo' language construct.` |
|       - | 3199 | ` */` |
|    9546 | 3200 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 3201 |  |
|    9548 | 3202 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3203 | `	sxi32 rc;` |
|       - | 3204 | `	/* Jump the 'echo' keyword */` |
|    9548 | 3205 | `	pGen->pIn++;` |
|       - | 3206 | `	/* Compile arguments one after one */` |
|    9548 | 3207 | `	pTmp = pGen->pEnd;` |
|   19478 | 3208 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    9932 | 3209 | `		if( pGen->pIn < pNext ){` |
|    9932 | 3210 | `			pGen->pEnd = pNext;` |
|    9932 | 3211 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    9932 | 3212 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3213 | `				return SXERR_ABORT;` |
|    9932 | 3214 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3215 | `				/* Emit the consume instruction */` |
|    9908 | 3216 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    4953 | 3217 | `			}` |
|    4965 | 3218 | `		}` |
|       - | 3219 | `		/* Jump trailing commas */` |
|   10316 | 3220 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     386 | 3221 | `			pNext++;` |
|       2 | 3222 | `		}` |
|    9932 | 3223 | `		pGen->pIn = pNext;` |
|       2 | 3224 | `	}` |
|       - | 3225 | `	/* Restore token stream */` |
|    9548 | 3226 | `	pGen->pEnd = pTmp;` |
|    9548 | 3227 | `	return SXRET_OK;` |
|    4775 | 3228 |  |
|       - | 3229 | `/*` |
|       - | 3230 | ` * Compile the static statement.` |
|       - | 3231 | ` * According to the PHP language reference` |
|       - | 3232 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 3233 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 3234 | ` *  when program execution leaves this scope.` |
|       - | 3235 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 3236 | ` * Symisc eXtension.` |
|       - | 3237 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 3238 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3239 | ` *  Example` |
|       - | 3240 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 3241 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 3242 | ` */` |
|       2 | 3243 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 | 3244 |  |
|       - | 3245 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 3246 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 3247 | `	GenBlock *pBlock;` |
|       - | 3248 | `	SyString *pName;` |
|       - | 3249 | `	char *zDup;` |
|       - | 3250 | `	sxu32 nLine;` |
|       - | 3251 | `	sxi32 rc;` |
|       - | 3252 | `	/* Jump the static keyword */` |
|       3 | 3253 | `	nLine = pGen->pIn->nLine;` |
|       3 | 3254 | `	pGen->pIn++;` |
|       - | 3255 | `	/* Extract the enclosing function if any */` |
|       3 | 3256 | `	pBlock = pGen->pCurrent;` |
|       5 | 3257 | `	while( pBlock ){` |
|       5 | 3258 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 | 3259 | `			break;` |
|       - | 3260 | `		}` |
|       - | 3261 | `		/* Point to the upper block */` |
|       3 | 3262 | `		pBlock = pBlock->pParent;` |
|       1 | 3263 | `	}` |
|       3 | 3264 | `	if( pBlock == 0 ){` |
|       - | 3265 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 3266 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3267 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 | 3268 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3269 | `				return SXERR_ABORT;` |
|       - | 3270 | `			}` |
|     ! 0 | 3271 | `			goto Synchronize;` |
|       - | 3272 | `		}` |
|       - | 3273 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 3274 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 3275 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3276 | `			return SXERR_ABORT;` |
|     ! 0 | 3277 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 3278 | `			/* Emit the POP instruction */` |
|     ! 0 | 3279 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 3280 | `		}` |
|     ! 0 | 3281 | `		return SXRET_OK;` |
|       - | 3282 | `	}` |
|       3 | 3283 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 3284 | `	/* Make sure we are dealing with a valid statement */` |
|       3 | 3285 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 | 3286 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 | 3287 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 | 3288 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3289 | `				return SXERR_ABORT;` |
|       - | 3290 | `			}` |
|       3 | 3291 | `			goto Synchronize;` |
|       - | 3292 | `	}` |
|     ! 0 | 3293 | `	pGen->pIn++;` |
|       - | 3294 | `	/* Extract variable name */` |
|     ! 0 | 3295 | `	pName = &pGen->pIn->sData;` |
|     ! 0 | 3296 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 | 3297 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 3298 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3299 | `		goto Synchronize;` |
|       - | 3300 | `	}` |
|       - | 3301 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 | 3302 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 | 3303 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 3304 | `	/* Duplicate variable name */` |
|     ! 0 | 3305 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 | 3306 | `	if( zDup == 0 ){` |
|     ! 0 | 3307 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3308 | `		return SXERR_ABORT;` |
|       - | 3309 | `	}` |
|     ! 0 | 3310 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 3311 | `	/* Check if we have an expression to compile */` |
|     ! 0 | 3312 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 3313 | `		SySet *pInstrContainer;` |
|       - | 3314 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 3315 | `		 * Static variable can take any complex expression including function` |
|       - | 3316 | `		 * call as their initialization value.` |
|       - | 3317 | `		 * Example:` |
|       - | 3318 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 3319 | `		 */` |
|     ! 0 | 3320 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 3321 | `		/* Swap bytecode container */` |
|     ! 0 | 3322 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 | 3323 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 3324 | `		/* Compile the expression */` |
|     ! 0 | 3325 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3326 | `		/* Emit the done instruction */` |
|     ! 0 | 3327 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 3328 | `		/* Restore default bytecode container */` |
|     ! 0 | 3329 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 3330 | `	}` |
|       - | 3331 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 | 3332 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 | 3333 | `	return SXRET_OK;` |
|       1 | 3334 | `Synchronize:` |
|       - | 3335 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 3336 | `	 * statement.` |
|       - | 3337 | `	 */` |
|       5 | 3338 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 | 3339 | `		pGen->pIn++;` |
|       1 | 3340 | `	}` |
|       3 | 3341 | `	return SXRET_OK;` |
|       2 | 3342 |  |
|       - | 3343 | `/*` |
|       - | 3344 | ` * Compile the var statement.` |
|       - | 3345 | ` * Symisc Extension:` |
|       - | 3346 | ` *      var statement can be used outside of a class definition.` |
|       - | 3347 | ` */` |
|       4 | 3348 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 3349 |  |
|       - | 3350 | `	sxu32 nLine;` |
|       - | 3351 | `	sxi32 rc;` |
|       5 | 3352 | `	nLine = pGen->pIn->nLine;` |
|       - | 3353 | `	/* Jump the 'var' keyword */` |
|       5 | 3354 | `	pGen->pIn++;` |
|       5 | 3355 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 3356 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - | 3357 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 | 3358 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 | 3359 | `			pGen->pIn++;` |
|     ! 0 | 3360 | `		}` |
|     ! 0 | 3361 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3362 | `			return SXERR_ABORT;` |
|       - | 3363 | `		}` |
|     ! 0 | 3364 | `	}else{` |
|       - | 3365 | `		/* Compile the expression */` |
|       5 | 3366 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 | 3367 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3368 | `			return SXERR_ABORT;` |
|       5 | 3369 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 | 3370 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 3371 | `		}` |
|       - | 3372 | `	}` |
|       5 | 3373 | `	return SXRET_OK;` |
|       3 | 3374 |  |
|       - | 3375 | `/*` |
|       - | 3376 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 3377 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 3378 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 3379 | ` */` |
|       - | 3380 | `/*` |
|       - | 3381 | ` * Namespace-qualify a name for CALL/NEW instructions.` |
|       - | 3382 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 3383 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 3384 | ` * qualified name and updates the instruction's operand index.` |
|       - | 3385 | ` *` |
|       - | 3386 | ` * Resolution: use imports -> current NS prefix.` |
|       - | 3387 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 3388 | ` * Returns the (possibly new) literal index.` |
|       - | 3389 | ` */` |
|  244392 | 3390 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx)` |
|       2 | 3391 |  |
|       - | 3392 | `	ph7_value *pLit;` |
|       - | 3393 | `	const char *zLit;` |
|       - | 3394 | `	SyString sQualified;` |
|       - | 3395 | `	sxu32 nLit;` |
|       - | 3396 | `	sxu32 k;` |
|       - | 3397 | `	sxu32 nNewIdx;` |
|       - | 3398 | `	int hasNsSep;` |
|       - | 3399 | `	SyHashEntry *pImport;` |
|       - | 3400 | `	ph7_value *pNew;` |
|  244394 | 3401 | `	if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  244288 | 3402 | `		return nOrigIdx; /* Not in a namespace */` |
|       - | 3403 | `	}` |
|     107 | 3404 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|     107 | 3405 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 3406 | `		return nOrigIdx;` |
|       - | 3407 | `	}` |
|     107 | 3408 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|     107 | 3409 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 3410 | `	/* Skip if already qualified (contains backslash) */` |
|     107 | 3411 | `	hasNsSep = 0;` |
|     521 | 3412 | `	for( k = 0; k < nLit; k++ ){` |
|     465 | 3413 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|     208 | 3414 | `	}` |
|     107 | 3415 | `	if( hasNsSep ){` |
|      51 | 3416 | `		return nOrigIdx;` |
|       - | 3417 | `	}` |
|       - | 3418 | `	/* Build the qualified name into sWorker */` |
|      57 | 3419 | `	SyBlobReset(&pGen->sWorker);` |
|       - | 3420 | `	/* Check use imports first */` |
|      57 | 3421 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zLit,nLit);` |
|      57 | 3422 | `	if( pImport ){` |
|      15 | 3423 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      15 | 3424 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       8 | 3425 | `	}else{` |
|       - | 3426 | `		/* Prepend current namespace */` |
|      43 | 3427 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      43 | 3428 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      43 | 3429 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 3430 | `	}` |
|       - | 3431 | `	/* Look up or create a new literal for the qualified name */` |
|      57 | 3432 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      57 | 3433 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      17 | 3434 | `		return nNewIdx; /* Already interned */` |
|       - | 3435 | `	}` |
|      41 | 3436 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      41 | 3437 | `	if( pNew == 0 ){` |
|     ! 0 | 3438 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 3439 | `	}` |
|      41 | 3440 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      41 | 3441 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      41 | 3442 | `	return nNewIdx;` |
|  122198 | 3443 |  |
|       - | 3444 | `/*` |
|       - | 3445 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 3446 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 3447 | ` */` |
|   14702 | 3448 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3449 |  |
|       - | 3450 | `	SyHashEntry *pImport;` |
|       - | 3451 | `	/* Check use imports first */` |
|   14704 | 3452 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   14704 | 3453 | `	if( pImport ){` |
|       7 | 3454 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       7 | 3455 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       7 | 3456 | `		return;` |
|       - | 3457 | `	}` |
|       - | 3458 | `	/* Prepend current namespace if active */` |
|   14698 | 3459 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 3460 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 3461 | `		SyBlobAppend(pOut,"\\",1);` |
|       1 | 3462 | `	}` |
|   14698 | 3463 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    7353 | 3464 |  |
|       - | 3465 | `/*` |
|       - | 3466 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 3467 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 3468 | ` * The caller must release pOut when done.` |
|       - | 3469 | ` */` |
|   29590 | 3470 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3471 |  |
|   29592 | 3472 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      33 | 3473 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      33 | 3474 | `		SyBlobAppend(pOut,"\\",1);` |
|      16 | 3475 | `	}` |
|   29592 | 3476 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   29592 | 3477 |  |
|       - | 3478 | `/*` |
|       - | 3479 | ` * Compile a namespace statement` |
|       - | 3480 | ` * According to the PHP language reference manual` |
|       - | 3481 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 3482 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 3483 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 3484 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 3485 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 3486 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 3487 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 3488 | ` *  programming world.` |
|       - | 3489 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 3490 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 3491 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 3492 | ` *  classes/functions/constants.` |
|       - | 3493 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 3494 | ` *  readability of source code.` |
|       - | 3495 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 3496 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 3497 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 3498 | ` *       class MyClass {}` |
|       - | 3499 | ` *       function myfunction() {}` |
|       - | 3500 | ` *       const MYCONST = 1;` |
|       - | 3501 | ` *       $a = new MyClass;` |
|       - | 3502 | ` *       $c = new \my\name\MyClass;` |
|       - | 3503 | ` *       $a = strlen('hi');` |
|       - | 3504 | ` *       $d = namespace\MYCONST;` |
|       - | 3505 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 3506 | ` *       echo constant($d);` |
|       - | 3507 | ` * NOTE` |
|       - | 3508 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3509 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3510 | ` */` |
|       - | 3511 | `/*` |
|       - | 3512 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 3513 | ` */` |
|       6 | 3514 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 | 3515 |  |
|       7 | 3516 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|     ! 0 | 3517 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|     ! 0 | 3518 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|     ! 0 | 3519 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|     ! 0 | 3520 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|     ! 0 | 3521 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|     ! 0 | 3522 | `	return "token";` |
|       4 | 3523 |  |
|      50 | 3524 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       1 | 3525 |  |
|       - | 3526 | `	sxu32 nLine;` |
|       - | 3527 | `	sxi32 rc;` |
|      51 | 3528 | `	nLine = pGen->pIn->nLine;` |
|      51 | 3529 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 3530 | `	/* Reset namespace and clear previous use imports */` |
|      51 | 3531 | `	SyBlobReset(&pGen->sNamespace);` |
|      51 | 3532 | `	SyHashRelease(&pGen->hUseImports);` |
|      51 | 3533 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      51 | 3534 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3535 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 | 3536 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3537 | `		return SXRET_OK;` |
|       - | 3538 | `	}` |
|      51 | 3539 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - | 3540 | `		/* namespace; — switch to global namespace */` |
|     ! 0 | 3541 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3542 | `		return SXRET_OK;` |
|       - | 3543 | `	}` |
|      51 | 3544 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - | 3545 | `		/* namespace { } — global namespace block */` |
|     ! 0 | 3546 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3547 | `		return SXRET_OK;` |
|       - | 3548 | `	}` |
|       - | 3549 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     131 | 3550 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      81 | 3551 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 3552 | `			/* Append backslash separator */` |
|      17 | 3553 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      17 | 3554 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       8 | 3555 | `			}` |
|       9 | 3556 | `		}else{` |
|       - | 3557 | `			/* Append identifier */` |
|      65 | 3558 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 3559 | `		}` |
|      81 | 3560 | `		pGen->pIn++;` |
|       1 | 3561 | `	}` |
|       - | 3562 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - | 3563 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - | 3564 | `	{` |
|      51 | 3565 | `		char *zNsDup = 0;` |
|      51 | 3566 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      73 | 3567 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      48 | 3568 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      24 | 3569 | `		}` |
|      51 | 3570 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - | 3571 | `	}` |
|      51 | 3572 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 | 3573 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3574 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 3575 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 | 3576 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3577 | `			return SXERR_ABORT;` |
|       - | 3578 | `		}` |
|       2 | 3579 | `	}` |
|      51 | 3580 | `	return SXRET_OK;` |
|      26 | 3581 |  |
|       - | 3582 | `/*` |
|       - | 3583 | ` * Compile the 'use' statement` |
|       - | 3584 | ` * According to the PHP language reference manual` |
|       - | 3585 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 3586 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 3587 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 3588 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 3589 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 3590 | ` *  a function or constant is not supported.` |
|       - | 3591 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 3592 | ` * NOTE` |
|       - | 3593 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3594 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3595 | ` */` |
|      22 | 3596 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       1 | 3597 |  |
|       - | 3598 | `	sxu32 nLine;` |
|       - | 3599 | `	sxi32 rc;` |
|       - | 3600 | `	SyBlob sPath;` |
|       - | 3601 | `	SyString sAlias;` |
|       - | 3602 | `	SyToken *pLast;` |
|       - | 3603 | `	char *zDup;` |
|      23 | 3604 | `	nLine = pGen->pIn->nLine;` |
|      23 | 3605 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|      23 | 3606 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 3607 | `	/* Process one or more use declarations separated by commas */` |
|      12 | 3608 | `	for(;;){` |
|      25 | 3609 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3610 | `			break;` |
|       - | 3611 | `		}` |
|      25 | 3612 | `		SyBlobReset(&sPath);` |
|      25 | 3613 | `		pLast = 0;` |
|       - | 3614 | `		/* Collect the full namespace path */` |
|     101 | 3615 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      77 | 3616 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      49 | 3617 | `				pLast = pGen->pIn;` |
|      49 | 3618 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      29 | 3619 | `					SyBlobAppend(&sPath,"\\",1);` |
|      14 | 3620 | `				}` |
|      49 | 3621 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      24 | 3622 | `			}` |
|      77 | 3623 | `			pGen->pIn++;` |
|       1 | 3624 | `		}` |
|      25 | 3625 | `		if( pLast == 0 ){` |
|       - | 3626 | `			/* Empty path */` |
|       5 | 3627 | `			break;` |
|       - | 3628 | `		}` |
|       - | 3629 | `		/* Default alias is the last component of the path */` |
|      21 | 3630 | `		sAlias = pLast->sData;` |
|       - | 3631 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      20 | 3632 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      13 | 3633 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       5 | 3634 | `			pGen->pIn++; /* Jump 'as' */` |
|       5 | 3635 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       5 | 3636 | `				sAlias = pGen->pIn->sData;` |
|       5 | 3637 | `				pGen->pIn++;` |
|       2 | 3638 | `			}` |
|       2 | 3639 | `		}` |
|       - | 3640 | `		/* Register the import: alias -> FQN.` |
|       - | 3641 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - | 3642 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - | 3643 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      31 | 3644 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      20 | 3645 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      21 | 3646 | `		if( zDup ){` |
|       - | 3647 | `			char *zAliasDup;` |
|      21 | 3648 | `			SyHashInsert(&pGen->hUseImports,sAlias.zString,sAlias.nByte,zDup);` |
|       - | 3649 | `			/* Duplicate the alias key for the VM hash (token pointers may not survive to runtime) */` |
|      21 | 3650 | `			zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      21 | 3651 | `			if( zAliasDup ){` |
|      21 | 3652 | `				SyHashInsert(&pGen->pVm->hUseImports,zAliasDup,sAlias.nByte,zDup);` |
|      10 | 3653 | `			}` |
|      10 | 3654 | `		}` |
|       - | 3655 | `		/* Check for comma (multiple use declarations) */` |
|      21 | 3656 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3657 | `			pGen->pIn++;` |
|       2 | 3658 | `		}else{` |
|      10 | 3659 | `			break;` |
|       - | 3660 | `		}` |
|       1 | 3661 | `	}` |
|      23 | 3662 | `	SyBlobRelease(&sPath);` |
|      23 | 3663 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 3664 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 3665 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 3666 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3667 | `			return SXERR_ABORT;` |
|       - | 3668 | `		}` |
|       1 | 3669 | `	}` |
|      23 | 3670 | `	return SXRET_OK;` |
|      12 | 3671 |  |
|       - | 3672 | `/*` |
|       - | 3673 | ` * Compile the stupid 'declare' language construct.` |
|       - | 3674 | ` *` |
|       - | 3675 | ` * According to the PHP language reference manual.` |
|       - | 3676 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 3677 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 3678 | ` *  declare (directive)` |
|       - | 3679 | ` *   statement` |
|       - | 3680 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 3681 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 3682 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 3683 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 3684 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 3685 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 3686 | ` * <?php` |
|       - | 3687 | ` * // these are the same:` |
|       - | 3688 | ` * // you can use this:` |
|       - | 3689 | ` * declare(ticks=1) {` |
|       - | 3690 | ` *   // entire script here` |
|       - | 3691 | ` * }` |
|       - | 3692 | ` * // or you can use this:` |
|       - | 3693 | ` * declare(ticks=1);` |
|       - | 3694 | ` * // entire script here` |
|       - | 3695 | ` * ?>` |
|       - | 3696 | ` *` |
|       - | 3697 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 3698 | ` */` |
|       8 | 3699 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 3700 |  |
|       9 | 3701 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 3702 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 3703 | `	sxi32 rc;` |
|       9 | 3704 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 3705 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 3706 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 3707 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3708 | `			return SXERR_ABORT;` |
|       - | 3709 | `		}` |
|       5 | 3710 | `		goto Synchro;` |
|       - | 3711 | `	}` |
|       5 | 3712 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 3713 | `	/* Delimit the directive */` |
|       5 | 3714 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 3715 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 3716 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 3717 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3718 | `			return SXERR_ABORT;` |
|       - | 3719 | `		}` |
|     ! 0 | 3720 | `		return SXRET_OK;` |
|       - | 3721 | `	}` |
|       - | 3722 | `	/* Update the cursor */` |
|       5 | 3723 | `	pGen->pIn = &pEnd[1];` |
|       5 | 3724 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 3725 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 3726 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3727 | `			return SXERR_ABORT;` |
|       - | 3728 | `		}` |
|     ! 0 | 3729 | `	}` |
|       - | 3730 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 3731 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 3732 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 3733 | `		ph7_lib_version()` |
|       - | 3734 | `		);` |
|       - | 3735 | `	/*All done */` |
|       5 | 3736 | `	return SXRET_OK;` |
|       2 | 3737 | `Synchro:` |
|       - | 3738 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 3739 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 3740 | `		pGen->pIn++;` |
|       1 | 3741 | `	}` |
|       5 | 3742 | `	return SXRET_OK;` |
|       5 | 3743 |  |
|       - | 3744 | `/*` |
|       - | 3745 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 3746 | ` * as follows:` |
|       - | 3747 | ` * function makecoffee($type = "cappuccino")` |
|       - | 3748 | ` * {` |
|       - | 3749 | ` *   return "Making a cup of $type.\n";` |
|       - | 3750 | ` * }` |
|       - | 3751 | ` * Symisc eXtension.` |
|       - | 3752 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 3753 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 3754 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 3755 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 3756 | ` *      {` |
|       - | 3757 | ` *       var_dump($a);` |
|       - | 3758 | ` *      }` |
|       - | 3759 | ` *     //call test without args` |
|       - | 3760 | ` *      test();` |
|       - | 3761 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 3762 | ` *      Example:` |
|       - | 3763 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 3764 | ` * 3 -) Function overloading!!` |
|       - | 3765 | ` *      Example:` |
|       - | 3766 | ` *      function foo($a) {` |
|       - | 3767 | ` *   	  return $a.PHP_EOL;` |
|       - | 3768 | ` *	    }` |
|       - | 3769 | ` *	    function foo($a, $b) {` |
|       - | 3770 | ` *   	  return $a + $b;` |
|       - | 3771 | ` *	    }` |
|       - | 3772 | ` *	    echo foo(5); // Prints "5"` |
|       - | 3773 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 3774 | ` *      // Same arg` |
|       - | 3775 | ` *	   function foo(string $a)` |
|       - | 3776 | ` *	   {` |
|       - | 3777 | ` *	     echo "a is a string\n";` |
|       - | 3778 | ` *	     var_dump($a);` |
|       - | 3779 | ` *	   }` |
|       - | 3780 | ` *	  function foo(int $a)` |
|       - | 3781 | ` *	  {` |
|       - | 3782 | ` *	    echo "a is integer\n";` |
|       - | 3783 | ` *	    var_dump($a);` |
|       - | 3784 | ` *	  }` |
|       - | 3785 | ` *	  function foo(array $a)` |
|       - | 3786 | ` *	  {` |
|       - | 3787 | ` * 	    echo "a is an array\n";` |
|       - | 3788 | ` * 	    var_dump($a);` |
|       - | 3789 | ` *	  }` |
|       - | 3790 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 3791 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 3792 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 3793 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 3794 | ` * introduced by the PH7 engine.` |
|       - | 3795 | ` */` |
|   31650 | 3796 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 3797 |  |
|       - | 3798 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 3799 | `	SySet *pInstrContainer;` |
|       - | 3800 | `	sxi32 rc;` |
|       - | 3801 | `	/* Swap token stream */` |
|   31652 | 3802 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   31652 | 3803 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   31652 | 3804 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 3805 | `	/* Compile the expression holding the argument value */` |
|   31652 | 3806 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3807 | `	/* Emit the done instruction */` |
|   31652 | 3808 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   31652 | 3809 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   31652 | 3810 | `	RE_SWAP_DELIMITER(pGen);` |
|   31652 | 3811 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3812 | `		return SXERR_ABORT;` |
|       - | 3813 | `	}` |
|   31652 | 3814 | `	return SXRET_OK;` |
|   15827 | 3815 |  |
|       - | 3816 | `/*` |
|       - | 3817 | ` * Collect function arguments one after one.` |
|       - | 3818 | ` * According to the PHP language reference manual.` |
|       - | 3819 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 3820 | ` * list of expressions.` |
|       - | 3821 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 3822 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 3823 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 3824 | ` * for more information.` |
|       - | 3825 | ` * Example #1 Passing arrays to functions` |
|       - | 3826 | ` * <?php` |
|       - | 3827 | ` * function takes_array($input)` |
|       - | 3828 | ` * {` |
|       - | 3829 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 3830 | ` * }` |
|       - | 3831 | ` * ?>` |
|       - | 3832 | ` * Making arguments be passed by reference` |
|       - | 3833 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 3834 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 3835 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 3836 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 3837 | ` * to the argument name in the function definition:` |
|       - | 3838 | ` * Example #2 Passing function parameters by reference` |
|       - | 3839 | ` * <?php` |
|       - | 3840 | ` * function add_some_extra(&$string)` |
|       - | 3841 | ` * {` |
|       - | 3842 | ` *   $string .= 'and something extra.';` |
|       - | 3843 | ` * }` |
|       - | 3844 | ` * $str = 'This is a string, ';` |
|       - | 3845 | ` * add_some_extra($str);` |
|       - | 3846 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 3847 | ` * ?>` |
|       - | 3848 | ` *` |
|       - | 3849 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 3850 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 3851 | ` * on these extension.` |
|       - | 3852 | ` */` |
|   34528 | 3853 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 3854 |  |
|       - | 3855 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 3856 | `	SyToken *pIn;  /* Token stream */` |
|       - | 3857 | `	SyBlob sSig;         /* Function signature */` |
|       - | 3858 | `	char *zDup;          /* Copy of argument name */` |
|       - | 3859 | `	sxi32 rc;` |
|       - | 3860 |  |
|   34530 | 3861 | `	pIn = pGen->pIn;` |
|   34530 | 3862 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 3863 | `	/* Process arguments one after one */` |
|   46929 | 3864 | `	for(;;){` |
|   93860 | 3865 | `		if( pIn >= pEnd ){` |
|       - | 3866 | `			/* No more arguments to process */` |
|   34528 | 3867 | `			break;` |
|       - | 3868 | `		}` |
|   59334 | 3869 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   59334 | 3870 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   59334 | 3871 | `		if( pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   48684 | 3872 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   43816 | 3873 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   43816 | 3874 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 3875 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   43816 | 3876 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 3877 | `					sArg.nType = MEMOBJ_BOOL;` |
|   43816 | 3878 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   12172 | 3879 | `					sArg.nType = MEMOBJ_INT;` |
|   37731 | 3880 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   31644 | 3881 | `					sArg.nType = MEMOBJ_STRING;` |
|   15824 | 3882 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 3883 | `					sArg.nType = MEMOBJ_REAL;` |
|     ! 0 | 3884 | `				}else{` |
|       4 | 3885 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 3886 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 3887 | `						&pIn->sData);` |
|       - | 3888 | `				}` |
|   21909 | 3889 | `			}else{` |
|    4870 | 3890 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 3891 | `				char *zDupLocal;` |
|       - | 3892 | `				/* Argument must be a class instance,record that*/` |
|    4870 | 3893 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    4870 | 3894 | `				if( zDupLocal ){` |
|    4870 | 3895 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    4870 | 3896 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2434 | 3897 | `				}` |
|       - | 3898 | `			}` |
|   48684 | 3899 | `			pIn++;` |
|   24341 | 3900 | `		}` |
|   59334 | 3901 | `		if( pIn >= pEnd ){` |
|     ! 0 | 3902 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 3903 | `			return rc;` |
|       - | 3904 | `		}` |
|   59334 | 3905 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 3906 | `			/* Pass by reference,record that */` |
|    2456 | 3907 | `			sArg.iFlags = VM_FUNC_ARG_BY_REF;` |
|    2456 | 3908 | `			pIn++;` |
|    1227 | 3909 | `		}` |
|   59334 | 3910 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 3911 | `			/* Invalid argument */` |
|     ! 0 | 3912 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 3913 | `			return rc;` |
|       - | 3914 | `		}` |
|   59334 | 3915 | `		pIn++; /* Jump the dollar sign */` |
|       - | 3916 | `		/* Copy argument name */` |
|   59334 | 3917 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   59334 | 3918 | `		if( zDup == 0 ){` |
|     ! 0 | 3919 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 3920 | `			return SXERR_ABORT;` |
|       - | 3921 | `		}` |
|   59334 | 3922 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   59334 | 3923 | `		pIn++;` |
|   59334 | 3924 | `		if( pIn < pEnd ){` |
|   36988 | 3925 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 3926 | `				SyToken *pDefend;` |
|   31654 | 3927 | `				sxi32 iNest = 0;` |
|   31654 | 3928 | `				pIn++; /* Jump the equal sign */` |
|   31654 | 3929 | `				pDefend = pIn;` |
|       - | 3930 | `				/* Process the default value associated with this argument */` |
|   68172 | 3931 | `				while( pDefend < pEnd ){` |
|   55992 | 3932 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   19474 | 3933 | `						break;` |
|       - | 3934 | `					}` |
|   36520 | 3935 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 3936 | `						/* Increment nesting level */` |
|    2436 | 3937 | `						iNest++;` |
|   35303 | 3938 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 3939 | `						/* Decrement nesting level */` |
|    2436 | 3940 | `						iNest--;` |
|    1217 | 3941 | `					}` |
|   36520 | 3942 | `					pDefend++;` |
|       2 | 3943 | `				}` |
|   31654 | 3944 | `				if( pIn >= pDefend ){` |
|       3 | 3945 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 3946 | `					return rc;` |
|       - | 3947 | `				}` |
|       - | 3948 | `				/* Process default value */` |
|   31652 | 3949 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   31652 | 3950 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 3951 | `					return rc;` |
|       - | 3952 | `				}` |
|       - | 3953 | `				/* Point beyond the default value */` |
|   31652 | 3954 | `				pIn = pDefend;` |
|   15825 | 3955 | `			}` |
|   36986 | 3956 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 3957 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 3958 | `				return rc;` |
|       - | 3959 | `			}` |
|   36986 | 3960 | `			pIn++; /* Jump the trailing comma */` |
|   18492 | 3961 | `		}` |
|       - | 3962 | `		/* Append argument signature */` |
|   59332 | 3963 | `		if( sArg.nType > 0 ){` |
|   48682 | 3964 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 3965 | `				/* Class name */` |
|    4870 | 3966 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2436 | 3967 | `			}else{` |
|       - | 3968 | `				int c;` |
|   43814 | 3969 | `				c = 'n'; /* cc warning */` |
|       - | 3970 | `				/* Type leading character */` |
|   43814 | 3971 | `				switch(sArg.nType){` |
|     ! 0 | 3972 | `				case MEMOBJ_HASHMAP:` |
|       - | 3973 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 3974 | `					c = 'h';` |
|     ! 0 | 3975 | `					break;` |
|    6085 | 3976 | `				case MEMOBJ_INT:` |
|       - | 3977 | `					/* Integer */` |
|   12172 | 3978 | `					c = 'i';` |
|   12172 | 3979 | `					break;` |
|     ! 0 | 3980 | `				case MEMOBJ_BOOL:` |
|       - | 3981 | `					/* Bool */` |
|     ! 0 | 3982 | `					c = 'b';` |
|     ! 0 | 3983 | `					break;` |
|     ! 0 | 3984 | `				case MEMOBJ_REAL:` |
|       - | 3985 | `					/* Float */` |
|     ! 0 | 3986 | `					c = 'f';` |
|     ! 0 | 3987 | `					break;` |
|   15821 | 3988 | `				case MEMOBJ_STRING:` |
|       - | 3989 | `					/* String */` |
|   31644 | 3990 | `					c = 's';` |
|   31642 | 3991 | `					break;` |
|     ! 0 | 3992 | `				default:` |
|     ! 0 | 3993 | `					break;` |
|       - | 3994 | `				}` |
|   43814 | 3995 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 3996 | `			}` |
|   24342 | 3997 | `		}else{` |
|       - | 3998 | `			/* No type is associated with this parameter which mean` |
|       - | 3999 | `			 * that this function is not condidate for overloading.` |
|       - | 4000 | `			 */` |
|   10652 | 4001 | `			SyBlobRelease(&sSig);` |
|       - | 4002 | `		}` |
|       - | 4003 | `		/* Save in the argument set */` |
|   59332 | 4004 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 4005 | `	}` |
|   34528 | 4006 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 4007 | `		/* Save function signature */` |
|   29210 | 4008 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   14604 | 4009 | `	}` |
|   34528 | 4010 | `	return SXRET_OK;` |
|   17266 | 4011 |  |
|       - | 4012 | `/*` |
|       - | 4013 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 4014 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 4015 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 4016 | ` */` |
|   83566 | 4017 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 4018 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 4019 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 4020 | `	)` |
|       2 | 4021 |  |
|       - | 4022 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 4023 | `	GenBlock *pBlock;` |
|       - | 4024 | `	sxu32 nGotoOfft;` |
|       - | 4025 | `	sxi32 rc;` |
|       - | 4026 | `	/* Attach the new function */` |
|   83568 | 4027 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   83568 | 4028 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4029 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 4030 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4031 | `		return SXERR_ABORT;` |
|       - | 4032 | `	}` |
|   83568 | 4033 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 4034 | `	/* Swap bytecode containers */` |
|   83568 | 4035 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   83568 | 4036 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 4037 | `	/* Compile the body */` |
|   83568 | 4038 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 4039 | `	/* Fix exception jumps now the destination is resolved */` |
|   83568 | 4040 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4041 | `	/* Emit the final return if not yet done */` |
|   83568 | 4042 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4043 | `	/* Fix gotos jumps now the destination is resolved */` |
|   83568 | 4044 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 4045 | `		rc = SXERR_ABORT;` |
|     ! 0 | 4046 | `	}` |
|   83568 | 4047 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 4048 | `	/* Restore the default container */` |
|   83568 | 4049 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4050 | `	/* Leave function block */` |
|   83568 | 4051 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   83568 | 4052 | `	if( rc == SXERR_ABORT ){` |
|       - | 4053 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4054 | `		return SXERR_ABORT;` |
|       - | 4055 | `	}` |
|       - | 4056 | `	/* All done, function body compiled */` |
|   83568 | 4057 | `	return SXRET_OK;` |
|   41785 | 4058 |  |
|       - | 4059 | `/*` |
|       - | 4060 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 4061 | ` * According to the PHP language reference manual.` |
|       - | 4062 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 4063 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 4064 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 4065 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4066 | ` *  Functions need not be defined before they are referenced.` |
|       - | 4067 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 4068 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 4069 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 4070 | ` *  calls with over 32-64 recursion levels.` |
|       - | 4071 | ` *` |
|       - | 4072 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 4073 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 4074 | ` * on these extension.` |
|       - | 4075 | ` */` |
|   32146 | 4076 | `static sxi32 GenStateCompileFunc(` |
|       - | 4077 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4078 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 4079 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 4080 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 4081 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 4082 | `	)` |
|       2 | 4083 |  |
|       - | 4084 | `	ph7_vm_func *pFunc;` |
|       - | 4085 | `	SyToken *pEnd;` |
|       - | 4086 | `	sxu32 nLine;` |
|       - | 4087 | `	char *zName;` |
|       - | 4088 | `	sxi32 rc;` |
|       - | 4089 | `	/* Extract line number */` |
|   32148 | 4090 | `	nLine = pGen->pIn->nLine;` |
|       - | 4091 | `	/* Jump the left parenthesis '(' */` |
|   32148 | 4092 | `	pGen->pIn++;` |
|       - | 4093 | `	/* Delimit the function signature */` |
|   32148 | 4094 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   32148 | 4095 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4096 | `		/* Syntax error */` |
|       7 | 4097 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 4098 | `		if( rc == SXERR_ABORT ){` |
|       - | 4099 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4100 | `			return SXERR_ABORT;` |
|       - | 4101 | `		}` |
|       7 | 4102 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 4103 | `		return SXRET_OK;` |
|       - | 4104 | `	}` |
|       - | 4105 | `	/* Create the function state */` |
|   32142 | 4106 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   32142 | 4107 | `	if( pFunc == 0 ){` |
|     ! 0 | 4108 | `		goto OutOfMem;` |
|       - | 4109 | `	}` |
|       - | 4110 | `	/* Build the function name, prepending namespace if active */` |
|   32146 | 4111 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 4112 | `		SyBlob sFQN;` |
|       - | 4113 | `		sxu32 nLen;` |
|       9 | 4114 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       9 | 4115 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       9 | 4116 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       9 | 4117 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       9 | 4118 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       9 | 4119 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       9 | 4120 | `		SyBlobRelease(&sFQN);` |
|       9 | 4121 | `		if( zName == 0 ){` |
|     ! 0 | 4122 | `			goto OutOfMem;` |
|       - | 4123 | `		}` |
|       9 | 4124 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       5 | 4125 | `	}else{` |
|   32134 | 4126 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   32134 | 4127 | `		if( zName == 0 ){` |
|     ! 0 | 4128 | `			goto OutOfMem;` |
|       - | 4129 | `		}` |
|   32134 | 4130 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 4131 | `	}` |
|   32142 | 4132 | `	if( pGen->pIn < pEnd ){` |
|       - | 4133 | `		/* Collect function arguments */` |
|   22300 | 4134 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   22300 | 4135 | `		if( rc == SXERR_ABORT ){` |
|       - | 4136 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4137 | `			return SXERR_ABORT;` |
|       - | 4138 | `		}` |
|   11149 | 4139 | `	}` |
|       - | 4140 | `	/* Compile function body */` |
|   32142 | 4141 | `	pGen->pIn = &pEnd[1];` |
|   32142 | 4142 | `	if( bHandleClosure ){` |
|       - | 4143 | `		ph7_vm_func_closure_env sEnv;` |
|     130 | 4144 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     128 | 4145 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      70 | 4146 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      10 | 4147 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 4148 | `				/* Closure,record environment variable */` |
|      10 | 4149 | `				pGen->pIn++;` |
|      10 | 4150 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 4151 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 4152 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4153 | `						return SXERR_ABORT;` |
|       - | 4154 | `					}` |
|     ! 0 | 4155 | `				}` |
|      10 | 4156 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 4157 | `				/* Compile until we hit the first closing parenthesis */` |
|      18 | 4158 | `				while( pGen->pIn < pGen->pEnd ){` |
|      18 | 4159 | `					int iFlagsLocal = 0;` |
|      18 | 4160 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      10 | 4161 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      10 | 4162 | `						break;` |
|       - | 4163 | `					}` |
|      10 | 4164 | `					nLineLocal = pGen->pIn->nLine;` |
|      10 | 4165 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 4166 | `						/* Pass by reference,record that */` |
|     ! 0 | 4167 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 4168 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 4169 | `							);` |
|     ! 0 | 4170 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 4171 | `						pGen->pIn++;` |
|     ! 0 | 4172 | `					}` |
|       8 | 4173 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      10 | 4174 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 4175 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 4176 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 4177 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4178 | `								return SXERR_ABORT;` |
|       - | 4179 | `							}` |
|       - | 4180 | `							/* Find the closing parenthesis */` |
|     ! 0 | 4181 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 4182 | `								pGen->pIn++;` |
|     ! 0 | 4183 | `							}` |
|     ! 0 | 4184 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 4185 | `								pGen->pIn++;` |
|     ! 0 | 4186 | `							}` |
|     ! 0 | 4187 | `							break;` |
|       - | 4188 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 4189 | `					}else{` |
|       - | 4190 | `						SyString *pNameLocal;` |
|       - | 4191 | `						char *zDup;` |
|       - | 4192 | `						/* Duplicate variable name */` |
|      10 | 4193 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      10 | 4194 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      10 | 4195 | `						if( zDup ){` |
|       - | 4196 | `							/* Zero the structure */` |
|      10 | 4197 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      10 | 4198 | `							sEnv.iFlags = iFlagsLocal;` |
|      10 | 4199 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      10 | 4200 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      10 | 4201 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 4202 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 4203 | `									got_this = 1;` |
|     ! 0 | 4204 | `							}` |
|       - | 4205 | `							/* Save imported variable */` |
|      10 | 4206 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       6 | 4207 | `						}else{` |
|     ! 0 | 4208 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4209 | `							 return SXERR_ABORT;` |
|       - | 4210 | `						}` |
|       - | 4211 | `					}` |
|      10 | 4212 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      10 | 4213 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4214 | `						/* Ignore trailing commas */` |
|     ! 0 | 4215 | `						pGen->pIn++;` |
|     ! 0 | 4216 | `					}` |
|       2 | 4217 | `				}` |
|      10 | 4218 | `				if( !got_this ){` |
|       - | 4219 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 4220 | `					 * available to the closure environment.` |
|       - | 4221 | `					 */` |
|      10 | 4222 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      10 | 4223 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      10 | 4224 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      10 | 4225 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      10 | 4226 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       4 | 4227 | `				}` |
|      10 | 4228 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 4229 | `					/* Mark as closure */` |
|      10 | 4230 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       4 | 4231 | `				}` |
|       4 | 4232 | `		}` |
|      64 | 4233 | `	}` |
|       - | 4234 | `	/* Compile the body */` |
|   32142 | 4235 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   32142 | 4236 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4237 | `		return SXERR_ABORT;` |
|       - | 4238 | `	}` |
|   32142 | 4239 | `	if( ppFunc ){` |
|     130 | 4240 | `		*ppFunc = pFunc;` |
|      64 | 4241 | `	}` |
|   32142 | 4242 | `	rc = SXRET_OK;` |
|   32142 | 4243 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 4244 | `		/* Finally register the function */` |
|   32134 | 4245 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   16066 | 4246 | `	}` |
|   32142 | 4247 | `	if( rc == SXRET_OK ){` |
|   32142 | 4248 | `		return SXRET_OK;` |
|       - | 4249 | `	}` |
|       - | 4250 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 4251 | `OutOfMem:` |
|       - | 4252 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 4253 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 4254 | `	 */` |
|     ! 0 | 4255 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 4256 | `	return SXERR_ABORT;` |
|   16075 | 4257 |  |
|       - | 4258 | `/*` |
|       - | 4259 | ` * Compile a standard PHP function.` |
|       - | 4260 | ` *  Refer to the block-comment above for more information.` |
|       - | 4261 | ` */` |
|   32024 | 4262 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 4263 |  |
|       - | 4264 | `	SyString *pName;` |
|       - | 4265 | `	sxi32 iFlags;` |
|       - | 4266 | `	sxu32 nLine;` |
|       - | 4267 | `	sxi32 rc;` |
|       - | 4268 |  |
|   32026 | 4269 | `	nLine = pGen->pIn->nLine;` |
|   32026 | 4270 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   32026 | 4271 | `	iFlags = 0;` |
|   32026 | 4272 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4273 | `		/* Return by reference,remember that */` |
|       7 | 4274 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4275 | `		/* Jump the '&' token */` |
|       7 | 4276 | `		pGen->pIn++;` |
|       3 | 4277 | `	}` |
|   32026 | 4278 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4279 | `		/* Invalid function name */` |
|       5 | 4280 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 4281 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4282 | `			return SXERR_ABORT;` |
|       - | 4283 | `		}` |
|       - | 4284 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 4285 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 4286 | `			pGen->pIn++;` |
|       1 | 4287 | `		}` |
|       5 | 4288 | `		return SXRET_OK;` |
|       - | 4289 | `	}` |
|   32022 | 4290 | `	pName = &pGen->pIn->sData;` |
|   32022 | 4291 | `	nLine = pGen->pIn->nLine;` |
|       - | 4292 | `	/* Jump the function name */` |
|   32022 | 4293 | `	pGen->pIn++;` |
|   32022 | 4294 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4295 | `		/* Syntax error */` |
|       3 | 4296 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 4297 | `		if( rc == SXERR_ABORT ){` |
|       - | 4298 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4299 | `			return SXERR_ABORT;` |
|       - | 4300 | `		}` |
|       - | 4301 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 4302 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4303 | `			pGen->pIn++;` |
|     ! 0 | 4304 | `		}` |
|       3 | 4305 | `		return SXRET_OK;` |
|       - | 4306 | `	}` |
|       - | 4307 | `	/* Compile function body */` |
|   32020 | 4308 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   32020 | 4309 | `	return rc;` |
|   16014 | 4310 |  |
|       - | 4311 | `/*` |
|       - | 4312 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 4313 | ` * According to the PHP language reference manual` |
|       - | 4314 | ` *  Visibility:` |
|       - | 4315 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 4316 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 4317 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 4318 | ` *  Members declared protected can be accessed only within the class` |
|       - | 4319 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 4320 | ` *  may only be accessed by the class that defines the member.` |
|       - | 4321 | ` */` |
|   95458 | 4322 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 4323 |  |
|   95460 | 4324 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|      44 | 4325 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   95418 | 4326 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   17066 | 4327 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 4328 | `	}` |
|       - | 4329 | `	/* Assume public by default */` |
|   78354 | 4330 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   47731 | 4331 |  |
|       - | 4332 | `/*` |
|       - | 4333 | ` * Compile a class constant.` |
|       - | 4334 | ` * According to the PHP language reference manual` |
|       - | 4335 | ` *  Class Constants` |
|       - | 4336 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 4337 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 4338 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 4339 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 4340 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 4341 | ` *   It's also possible for interfaces to have constants.` |
|       - | 4342 | ` * Symisc eXtension.` |
|       - | 4343 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 4344 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4345 | ` *  Example:` |
|       - | 4346 | ` *   class Test{` |
|       - | 4347 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4348 | ` *   };` |
|       - | 4349 | ` *   var_dump(TEST::MyConst);` |
|       - | 4350 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4351 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4352 | ` */` |
|      10 | 4353 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4354 |  |
|      12 | 4355 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4356 | `	SySet *pInstrContainer;` |
|       - | 4357 | `	ph7_class_attr *pCons;` |
|       - | 4358 | `	SyString *pName;` |
|       - | 4359 | `	sxi32 rc;` |
|       - | 4360 | `	/* Extract visibility level */` |
|      12 | 4361 | `	iProtection = GetProtectionLevel(iProtection);` |
|      12 | 4362 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       5 | 4363 | `loop:` |
|       - | 4364 | `	/* Mark as constant */` |
|      12 | 4365 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      12 | 4366 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4367 | `		/* Invalid constant name */` |
|     ! 0 | 4368 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 4369 | `		if( rc == SXERR_ABORT ){` |
|       - | 4370 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4371 | `			return SXERR_ABORT;` |
|       - | 4372 | `		}` |
|     ! 0 | 4373 | `		goto Synchronize;` |
|       - | 4374 | `	}` |
|       - | 4375 | `	/* Peek constant name */` |
|      12 | 4376 | `	pName = &pGen->pIn->sData;` |
|       - | 4377 | `	/* Make sure the constant name isn't reserved */` |
|      12 | 4378 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 4379 | `		/* Reserved constant name */` |
|     ! 0 | 4380 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 4381 | `		if( rc == SXERR_ABORT ){` |
|       - | 4382 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4383 | `			return SXERR_ABORT;` |
|       - | 4384 | `		}` |
|     ! 0 | 4385 | `		goto Synchronize;` |
|       - | 4386 | `	}` |
|       - | 4387 | `	/* Advance the stream cursor */` |
|      12 | 4388 | `	pGen->pIn++;` |
|      12 | 4389 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 4390 | `		/* Invalid declaration */` |
|     ! 0 | 4391 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 4392 | `		if( rc == SXERR_ABORT ){` |
|       - | 4393 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4394 | `			return SXERR_ABORT;` |
|       - | 4395 | `		}` |
|     ! 0 | 4396 | `		goto Synchronize;` |
|       - | 4397 | `	}` |
|      12 | 4398 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 4399 | `	/* Allocate a new class attribute */` |
|      12 | 4400 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      12 | 4401 | `	if( pCons == 0 ){` |
|     ! 0 | 4402 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4403 | `		return SXERR_ABORT;` |
|       - | 4404 | `	}` |
|       - | 4405 | `	/* Swap bytecode container */` |
|      12 | 4406 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      12 | 4407 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 4408 | `	/* Compile constant value.` |
|       - | 4409 | `	 */` |
|      12 | 4410 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      12 | 4411 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 4412 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 4413 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4414 | `			return SXERR_ABORT;` |
|       - | 4415 | `		}` |
|       1 | 4416 | `	}` |
|       - | 4417 | `	/* Emit the done instruction */` |
|      12 | 4418 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      12 | 4419 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      12 | 4420 | `	if( rc == SXERR_ABORT ){` |
|       - | 4421 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4422 | `		return SXERR_ABORT;` |
|       - | 4423 | `	}` |
|       - | 4424 | `	/* All done,install the constant */` |
|      12 | 4425 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      12 | 4426 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4427 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4428 | `		return SXERR_ABORT;` |
|       - | 4429 | `	}` |
|      12 | 4430 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4431 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 4432 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4433 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 4434 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4435 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4436 | `				pTok--;` |
|     ! 0 | 4437 | `			}` |
|     ! 0 | 4438 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4439 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 4440 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4441 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4442 | `				return SXERR_ABORT;` |
|       - | 4443 | `			}` |
|     ! 0 | 4444 | `		}else{` |
|     ! 0 | 4445 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 4446 | `				goto loop;` |
|       - | 4447 | `			}` |
|       - | 4448 | `		}` |
|     ! 0 | 4449 | `	}` |
|      12 | 4450 | `	return SXRET_OK;` |
|     ! 0 | 4451 | `Synchronize:` |
|       - | 4452 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4453 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 4454 | `		pGen->pIn++;` |
|     ! 0 | 4455 | `	}` |
|     ! 0 | 4456 | `	return SXERR_CORRUPT;` |
|       7 | 4457 |  |
|       - | 4458 | `/*` |
|       - | 4459 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 4460 | ` * According to the PHP language reference manual` |
|       - | 4461 | ` *  Properties` |
|       - | 4462 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 4463 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 4464 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 4465 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 4466 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 4467 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 4468 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 4469 | ` * Symisc eXtension.` |
|       - | 4470 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 4471 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4472 | ` *  Example:` |
|       - | 4473 | ` *   class Test{` |
|       - | 4474 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4475 | ` *   };` |
|       - | 4476 | ` *   var_dump(TEST::myVar);` |
|       - | 4477 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4478 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4479 | ` */` |
|   24532 | 4480 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4481 |  |
|   24534 | 4482 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4483 | `	ph7_class_attr *pAttr;` |
|       - | 4484 | `	SyString *pName;` |
|       - | 4485 | `	sxi32 rc;` |
|       - | 4486 | `	/* Extract visibility level */` |
|   24534 | 4487 | `	iProtection = GetProtectionLevel(iProtection);` |
|   12266 | 4488 | `loop:` |
|   24534 | 4489 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   24534 | 4490 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 4491 | `		/* Invalid attribute name */` |
|     ! 0 | 4492 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 4493 | `		if( rc == SXERR_ABORT ){` |
|       - | 4494 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4495 | `			return SXERR_ABORT;` |
|       - | 4496 | `		}` |
|     ! 0 | 4497 | `		goto Synchronize;` |
|       - | 4498 | `	}` |
|       - | 4499 | `	/* Peek attribute name */` |
|   24534 | 4500 | `	pName = &pGen->pIn->sData;` |
|       - | 4501 | `	/* Advance the stream cursor */` |
|   24534 | 4502 | `	pGen->pIn++;` |
|   24534 | 4503 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 4504 | `		/* Invalid declaration */` |
|       3 | 4505 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 4506 | `		if( rc == SXERR_ABORT ){` |
|       - | 4507 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4508 | `			return SXERR_ABORT;` |
|       - | 4509 | `		}` |
|       3 | 4510 | `		goto Synchronize;` |
|       - | 4511 | `	}` |
|       - | 4512 | `	/* Allocate a new class attribute */` |
|   24532 | 4513 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   24532 | 4514 | `	if( pAttr == 0 ){` |
|     ! 0 | 4515 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 4516 | `		return SXERR_ABORT;` |
|       - | 4517 | `	}` |
|   24532 | 4518 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 4519 | `		SySet *pInstrContainer;` |
|    9890 | 4520 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 4521 | `		/* Swap bytecode container */` |
|    9890 | 4522 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    9890 | 4523 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 4524 | `		/* Compile attribute value.` |
|       - | 4525 | `		 */` |
|    9890 | 4526 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    9890 | 4527 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 4528 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 4529 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4530 | `				return SXERR_ABORT;` |
|       - | 4531 | `			}` |
|     ! 0 | 4532 | `		}` |
|       - | 4533 | `		/* Emit the done instruction */` |
|    9890 | 4534 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    9890 | 4535 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    4944 | 4536 | `	}` |
|       - | 4537 | `	/* All done,install the attribute */` |
|   24532 | 4538 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   24532 | 4539 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4540 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4541 | `		return SXERR_ABORT;` |
|       - | 4542 | `	}` |
|   24532 | 4543 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4544 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 4545 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4546 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 4547 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4548 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4549 | `				pTok--;` |
|     ! 0 | 4550 | `			}` |
|     ! 0 | 4551 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4552 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4553 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4554 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4555 | `				return SXERR_ABORT;` |
|       - | 4556 | `			}` |
|     ! 0 | 4557 | `		}else{` |
|     ! 0 | 4558 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 4559 | `				goto loop;` |
|       - | 4560 | `			}` |
|       - | 4561 | `		}` |
|     ! 0 | 4562 | `	}` |
|   24532 | 4563 | `	return SXRET_OK;` |
|       1 | 4564 | `Synchronize:` |
|       - | 4565 | `	/* Synchronize with the first semi-colon */` |
|       5 | 4566 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 4567 | `		pGen->pIn++;` |
|       1 | 4568 | `	}` |
|       3 | 4569 | `	return SXERR_CORRUPT;` |
|   12268 | 4570 |  |
|       - | 4571 | `/*` |
|       - | 4572 | ` * Compile a class method.` |
|       - | 4573 | ` *` |
|       - | 4574 | ` * Refer to the official documentation for more information` |
|       - | 4575 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 4576 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 4577 | ` * overloading and many more.` |
|       - | 4578 | ` */` |
|   70916 | 4579 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 4580 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4581 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 4582 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 4583 | `	int doBody,          /* TRUE to process method body */` |
|       - | 4584 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 4585 | `	)` |
|       2 | 4586 |  |
|   70918 | 4587 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4588 | `	ph7_class_method *pMeth;` |
|       - | 4589 | `	sxi32 iFuncFlags;` |
|       - | 4590 | `	SyString *pName;` |
|       - | 4591 | `	SyToken *pEnd;` |
|       - | 4592 | `	sxi32 rc;` |
|       - | 4593 | `	/* Extract visibility level */` |
|   70918 | 4594 | `	iProtection = GetProtectionLevel(iProtection);` |
|   70918 | 4595 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   70918 | 4596 | `	iFuncFlags = 0;` |
|   70918 | 4597 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4598 | `		/* Invalid method name */` |
|     ! 0 | 4599 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4600 | `		if( rc == SXERR_ABORT ){` |
|       - | 4601 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4602 | `			return SXERR_ABORT;` |
|       - | 4603 | `		}` |
|     ! 0 | 4604 | `		goto Synchronize;` |
|       - | 4605 | `	}` |
|   70918 | 4606 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4607 | `		/* Return by reference,remember that */` |
|     ! 0 | 4608 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4609 | `		/* Jump the '&' token */` |
|     ! 0 | 4610 | `		pGen->pIn++;` |
|     ! 0 | 4611 | `	}` |
|   70918 | 4612 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID)) == 0 ){` |
|       - | 4613 | `		/* Invalid method name */` |
|     ! 0 | 4614 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4615 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4616 | `			return SXERR_ABORT;` |
|       - | 4617 | `		}` |
|     ! 0 | 4618 | `		goto Synchronize;` |
|       - | 4619 | `	}` |
|       - | 4620 | `	/* Peek method name */` |
|   70918 | 4621 | `	pName = &pGen->pIn->sData;` |
|   70918 | 4622 | `	nLine = pGen->pIn->nLine;` |
|       - | 4623 | `	/* Jump the method name */` |
|   70918 | 4624 | `	pGen->pIn++;` |
|   70918 | 4625 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 4626 | `		/* Abstract method */` |
|      10 | 4627 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 4628 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4629 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 4630 | `				&pClass->sName,pName);` |
|     ! 0 | 4631 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4632 | `				return SXERR_ABORT;` |
|       - | 4633 | `			}` |
|     ! 0 | 4634 | `		}` |
|       - | 4635 | `		/* Assemble method signature only */` |
|      10 | 4636 | `		doBody = FALSE;` |
|       4 | 4637 | `	}` |
|   70918 | 4638 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4639 | `		/* Syntax error */` |
|     ! 0 | 4640 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 4641 | `		if( rc == SXERR_ABORT ){` |
|       - | 4642 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4643 | `			return SXERR_ABORT;` |
|       - | 4644 | `		}` |
|     ! 0 | 4645 | `		goto Synchronize;` |
|       - | 4646 | `	}` |
|       - | 4647 | `	/* Allocate a new class_method instance */` |
|   70918 | 4648 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   70918 | 4649 | `	if( pMeth == 0 ){` |
|     ! 0 | 4650 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4651 | `		return SXERR_ABORT;` |
|       - | 4652 | `	}` |
|       - | 4653 | `	/* Jump the left parenthesis '(' */` |
|   70918 | 4654 | `	pGen->pIn++;` |
|   70918 | 4655 | `	pEnd = 0; /* cc warning */` |
|       - | 4656 | `	/* Delimit the method signature */` |
|   70918 | 4657 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   70918 | 4658 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4659 | `		/* Syntax error */` |
|       3 | 4660 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 4661 | `		if( rc == SXERR_ABORT ){` |
|       - | 4662 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4663 | `			return SXERR_ABORT;` |
|       - | 4664 | `		}` |
|       3 | 4665 | `		goto Synchronize;` |
|       - | 4666 | `	}` |
|   70916 | 4667 | `	if( pGen->pIn < pEnd ){` |
|       - | 4668 | `		/* Collect method arguments */` |
|   12232 | 4669 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   12232 | 4670 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4671 | `			return SXERR_ABORT;` |
|       - | 4672 | `		}` |
|    6115 | 4673 | `	}` |
|       - | 4674 | `	/* Point beyond method signature */` |
|   70916 | 4675 | `	pGen->pIn = &pEnd[1];` |
|   70916 | 4676 | `	if( doBody ){` |
|       - | 4677 | `		/* Compile method body */` |
|   51428 | 4678 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   51428 | 4679 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4680 | `			return SXERR_ABORT;` |
|       - | 4681 | `		}` |
|   25715 | 4682 | `	}else{` |
|       - | 4683 | `		/* Only method signature is allowed */` |
|   19490 | 4684 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 4685 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4686 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 4687 | `				if( rc == SXERR_ABORT ){` |
|       - | 4688 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4689 | `					return SXERR_ABORT;` |
|       - | 4690 | `				}` |
|     ! 0 | 4691 | `				return SXERR_CORRUPT;` |
|       - | 4692 | `			}` |
|       - | 4693 | `	}` |
|       - | 4694 | `	/* All done,install the method */` |
|   70916 | 4695 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   70916 | 4696 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4697 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4698 | `		return SXERR_ABORT;` |
|       - | 4699 | `	}` |
|   70916 | 4700 | `	return SXRET_OK;` |
|       1 | 4701 | `Synchronize:` |
|       - | 4702 | `	/* Synchronize with the first semi-colon */` |
|       7 | 4703 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 4704 | `		pGen->pIn++;` |
|       1 | 4705 | `	}` |
|       3 | 4706 | `	return SXERR_CORRUPT;` |
|   35460 | 4707 |  |
|       - | 4708 | `/*` |
|       - | 4709 | ` * Compile an object interface.` |
|       - | 4710 | ` *  According to the PHP language reference manual` |
|       - | 4711 | ` *   Object Interfaces:` |
|       - | 4712 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 4713 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 4714 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 4715 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 4716 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 4717 | ` */` |
|    7314 | 4718 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 4719 |  |
|    7316 | 4720 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4721 | `	ph7_class *pClass,*pBase;` |
|       - | 4722 | `	SyToken *pEnd,*pTmp;` |
|       - | 4723 | `	SyString *pName;` |
|       - | 4724 | `	sxi32 nKwrd;` |
|       - | 4725 | `	sxi32 rc;` |
|       - | 4726 | `	/* Jump the 'interface' keyword */` |
|    7316 | 4727 | `	pGen->pIn++;` |
|       - | 4728 | `	/* Extract interface name */` |
|    7316 | 4729 | `	pName = &pGen->pIn->sData;` |
|       - | 4730 | `	/* Advance the stream cursor */` |
|    7316 | 4731 | `	pGen->pIn++;` |
|       - | 4732 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 4733 | `		SyBlob sFQN;` |
|       - | 4734 | `		SyString sFQNStr;` |
|    7316 | 4735 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    7316 | 4736 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    7316 | 4737 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    7316 | 4738 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    7316 | 4739 | `		SyBlobRelease(&sFQN);` |
|       - | 4740 | `	}` |
|    7316 | 4741 | `	if( pClass == 0 ){` |
|     ! 0 | 4742 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4743 | `		return SXERR_ABORT;` |
|       - | 4744 | `	}` |
|       - | 4745 | `	/* Mark as an interface */` |
|    7316 | 4746 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 4747 | `	/* Assume no base class is given */` |
|    7316 | 4748 | `	pBase = 0;` |
|    7316 | 4749 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 4750 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 4751 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 4752 | `			SyString *pBaseName;` |
|       - | 4753 | `			/* Extract base interface */` |
|       3 | 4754 | `			pGen->pIn++;` |
|       3 | 4755 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4756 | `				/* Syntax error */` |
|     ! 0 | 4757 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4758 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 4759 | `					pName);` |
|     ! 0 | 4760 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4761 | `				if( rc == SXERR_ABORT ){` |
|       - | 4762 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4763 | `					return SXERR_ABORT;` |
|       - | 4764 | `				}` |
|     ! 0 | 4765 | `				return SXRET_OK;` |
|       - | 4766 | `			}` |
|       3 | 4767 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 4768 | `			{` |
|       - | 4769 | `				SyBlob sResolved;` |
|       3 | 4770 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 4771 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 | 4772 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 4773 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 4774 | `				SyBlobRelease(&sResolved);` |
|       - | 4775 | `			}` |
|       - | 4776 | `			/* Only interfaces is allowed */` |
|       3 | 4777 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 4778 | `				pBase = pBase->pNextName;` |
|     ! 0 | 4779 | `			}` |
|       3 | 4780 | `			if( pBase == 0 ){` |
|       - | 4781 | `				/* Inexistant interface */` |
|     ! 0 | 4782 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 4783 | `				if( rc == SXERR_ABORT ){` |
|       - | 4784 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4785 | `					return SXERR_ABORT;` |
|       - | 4786 | `				}` |
|     ! 0 | 4787 | `			}` |
|       - | 4788 | `			/* Advance the stream cursor */` |
|       3 | 4789 | `			pGen->pIn++;` |
|       1 | 4790 | `		}` |
|       1 | 4791 | `	}` |
|    7316 | 4792 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 4793 | `		/* Syntax error */` |
|     ! 0 | 4794 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 4795 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4796 | `		if( rc == SXERR_ABORT ){` |
|       - | 4797 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4798 | `			return SXERR_ABORT;` |
|       - | 4799 | `		}` |
|     ! 0 | 4800 | `		return SXRET_OK;` |
|       - | 4801 | `	}` |
|    7316 | 4802 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    7316 | 4803 | `	pEnd = 0; /* cc warning */` |
|       - | 4804 | `	/* Delimit the interface body */` |
|    7316 | 4805 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    7316 | 4806 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4807 | `		/* Syntax error */` |
|     ! 0 | 4808 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 4809 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4810 | `		if( rc == SXERR_ABORT ){` |
|       - | 4811 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4812 | `			return SXERR_ABORT;` |
|       - | 4813 | `		}` |
|     ! 0 | 4814 | `		return SXRET_OK;` |
|       - | 4815 | `	}` |
|       - | 4816 | `	/* Swap token stream */` |
|    7316 | 4817 | `	pTmp = pGen->pEnd;` |
|    7316 | 4818 | `	pGen->pEnd = pEnd;` |
|       - | 4819 | `	/* Start the parse process` |
|       - | 4820 | `	 * Note (According to the PHP reference manual):` |
|       - | 4821 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 4822 | `	 *  Only 'public' visibility is allowed.` |
|       - | 4823 | `	 */` |
|   13398 | 4824 | `	for(;;){` |
|       - | 4825 | `		/* Jump leading/trailing semi-colons */` |
|   46280 | 4826 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   19484 | 4827 | `			pGen->pIn++;` |
|       2 | 4828 | `		}` |
|   26798 | 4829 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4830 | `			/* End of interface body */` |
|    7316 | 4831 | `			break;` |
|       - | 4832 | `		}` |
|   19484 | 4833 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4834 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4835 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 4836 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 4837 | `			if( rc == SXERR_ABORT ){` |
|       - | 4838 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4839 | `				return SXERR_ABORT;` |
|       - | 4840 | `			}` |
|     ! 0 | 4841 | `			goto done;` |
|       - | 4842 | `		}` |
|       - | 4843 | `		/* Extract the current keyword */` |
|   19484 | 4844 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   19484 | 4845 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 4846 | `			/* Emit a warning and switch to public visibility */` |
|     ! 0 | 4847 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"interface: Access type must be public");` |
|     ! 0 | 4848 | `			nKwrd = PH7_TKWRD_PUBLIC;` |
|     ! 0 | 4849 | `		}` |
|   19484 | 4850 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 4851 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4852 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 4853 | `			if( rc == SXERR_ABORT ){` |
|       - | 4854 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4855 | `				return SXERR_ABORT;` |
|       - | 4856 | `			}` |
|     ! 0 | 4857 | `			goto done;` |
|       - | 4858 | `		}` |
|   19484 | 4859 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 4860 | `			/* Advance the stream cursor */` |
|   19480 | 4861 | `			pGen->pIn++;` |
|   19480 | 4862 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4863 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4864 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 4865 | `				if( rc == SXERR_ABORT ){` |
|       - | 4866 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4867 | `					return SXERR_ABORT;` |
|       - | 4868 | `				}` |
|     ! 0 | 4869 | `				goto done;` |
|       - | 4870 | `			}` |
|   19480 | 4871 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   19480 | 4872 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 4873 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4874 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 4875 | `				if( rc == SXERR_ABORT ){` |
|       - | 4876 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4877 | `					return SXERR_ABORT;` |
|       - | 4878 | `				}` |
|     ! 0 | 4879 | `				goto done;` |
|       - | 4880 | `			}` |
|    9739 | 4881 | `		}` |
|   19484 | 4882 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 4883 | `			/* Parse constant */` |
|       3 | 4884 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 4885 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 4886 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4887 | `					return SXERR_ABORT;` |
|       - | 4888 | `				}` |
|     ! 0 | 4889 | `				goto done;` |
|       - | 4890 | `			}` |
|       2 | 4891 | `		}else{` |
|   19482 | 4892 | `			sxi32 iFlags = 0;` |
|   19482 | 4893 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 4894 | `				/* Static method,record that */` |
|     ! 0 | 4895 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 4896 | `				/* Advance the stream cursor */` |
|     ! 0 | 4897 | `				pGen->pIn++;` |
|     ! 0 | 4898 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 4899 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 4900 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4901 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 4902 | `						if( rc == SXERR_ABORT ){` |
|       - | 4903 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 4904 | `							return SXERR_ABORT;` |
|       - | 4905 | `						}` |
|     ! 0 | 4906 | `						goto done;` |
|       - | 4907 | `				}` |
|     ! 0 | 4908 | `			}` |
|       - | 4909 | `			/* Process method signature */` |
|   19482 | 4910 | `			rc = GenStateCompileClassMethod(&(*pGen),0,FALSE/* Only method signature*/,iFlags,pClass);` |
|   19482 | 4911 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 4912 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4913 | `					return SXERR_ABORT;` |
|       - | 4914 | `				}` |
|     ! 0 | 4915 | `				goto done;` |
|       - | 4916 | `			}` |
|       - | 4917 | `		}` |
|       2 | 4918 | `	}` |
|       - | 4919 | `	/* Install the interface */` |
|    7316 | 4920 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    7316 | 4921 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 4922 | `		/* Inherit from the base interface */` |
|       3 | 4923 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 4924 | `	}` |
|    7316 | 4925 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4926 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4927 | `		return SXERR_ABORT;` |
|       - | 4928 | `	}` |
|    3657 | 4929 | `done:` |
|       - | 4930 | `	/* Point beyond the interface body */` |
|    7316 | 4931 | `	pGen->pIn  = &pEnd[1];` |
|    7316 | 4932 | `	pGen->pEnd = pTmp;` |
|    7316 | 4933 | `	return PH7_OK;` |
|    3659 | 4934 |  |
|       - | 4935 | `/*` |
|       - | 4936 | ` * Compile a user-defined class.` |
|       - | 4937 | ` * According to the PHP language reference manual` |
|       - | 4938 | ` *  class` |
|       - | 4939 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 4940 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 4941 | ` *  of the properties and methods belonging to the class.` |
|       - | 4942 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 4943 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 4944 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 4945 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4946 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 4947 | ` *  (called "methods").` |
|       - | 4948 | ` */` |
|       - | 4949 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 4950 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 4951 | `struct TraitUseEntry {` |
|       - | 4952 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 4953 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 4954 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 4955 | `};` |
|   22234 | 4956 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 4957 |  |
|   22236 | 4958 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4959 | `	ph7_class *pClass,*pBase;` |
|       - | 4960 | `	SyToken *pEnd,*pTmp;` |
|       - | 4961 | `	sxi32 iProtection;` |
|       - | 4962 | `	SySet aInterfaces;` |
|       - | 4963 | `	SySet aUseEntries;` |
|       - | 4964 | `	sxi32 iAttrflags;` |
|       - | 4965 | `	SyString *pName;` |
|       - | 4966 | `	sxi32 nKwrd;` |
|       - | 4967 | `	sxi32 rc;` |
|       - | 4968 | `	/* Jump the 'class' keyword */` |
|   22236 | 4969 | `	pGen->pIn++;` |
|   22236 | 4970 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4971 | `		/* Syntax error */` |
|     ! 0 | 4972 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 4973 | `		if( rc == SXERR_ABORT ){` |
|       - | 4974 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4975 | `			return SXERR_ABORT;` |
|       - | 4976 | `		}` |
|       - | 4977 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 4978 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 4979 | `			pGen->pIn++;` |
|     ! 0 | 4980 | `		}` |
|     ! 0 | 4981 | `		return SXRET_OK;` |
|       - | 4982 | `	}` |
|       - | 4983 | `	/* Extract class name */` |
|   22236 | 4984 | `	pName = &pGen->pIn->sData;` |
|       - | 4985 | `	/* Advance the stream cursor */` |
|   22236 | 4986 | `	pGen->pIn++;` |
|       - | 4987 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 4988 | `		SyBlob sFQN;` |
|       - | 4989 | `		SyString sFQNStr;` |
|   22236 | 4990 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   22236 | 4991 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   22236 | 4992 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   22236 | 4993 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   22236 | 4994 | `		SyBlobRelease(&sFQN);` |
|       - | 4995 | `	}` |
|   22236 | 4996 | `	if( pClass == 0 ){` |
|     ! 0 | 4997 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4998 | `		return SXERR_ABORT;` |
|       - | 4999 | `	}` |
|       - | 5000 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   22236 | 5001 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   22236 | 5002 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 5003 | `	/* Assume a standalone class */` |
|   22236 | 5004 | `	pBase = 0;` |
|   22236 | 5005 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5006 | `		SyString *pBaseName;` |
|   14660 | 5007 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   14660 | 5008 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   14656 | 5009 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   14656 | 5010 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5011 | `				/* Syntax error */` |
|     ! 0 | 5012 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5013 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 5014 | `					pName);` |
|     ! 0 | 5015 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5016 | `				if( rc == SXERR_ABORT ){` |
|       - | 5017 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5018 | `					return SXERR_ABORT;` |
|       - | 5019 | `				}` |
|     ! 0 | 5020 | `				return SXRET_OK;` |
|       - | 5021 | `			}` |
|       - | 5022 | `			/* Extract base class name and resolve through namespace/imports */` |
|   14656 | 5023 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5024 | `			{` |
|       - | 5025 | `				SyBlob sResolved;` |
|   14656 | 5026 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   14656 | 5027 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   21983 | 5028 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   14654 | 5029 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   14656 | 5030 | `				SyBlobRelease(&sResolved);` |
|       - | 5031 | `			}` |
|       - | 5032 | `			/* Interfaces are not allowed */` |
|   14656 | 5033 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 5034 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5035 | `			}` |
|   14656 | 5036 | `			if( pBase == 0 ){` |
|       - | 5037 | `				/* Inexistant base class */` |
|     ! 0 | 5038 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 5039 | `				if( rc == SXERR_ABORT ){` |
|       - | 5040 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5041 | `					return SXERR_ABORT;` |
|       - | 5042 | `				}` |
|     ! 0 | 5043 | `			}else{` |
|   14656 | 5044 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 5045 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 5046 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 5047 | `					if( rc == SXERR_ABORT ){` |
|       - | 5048 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5049 | `						return SXERR_ABORT;` |
|       - | 5050 | `					}` |
|     ! 0 | 5051 | `				}` |
|       - | 5052 | `			}` |
|       - | 5053 | `			/* Advance the stream cursor */` |
|   14656 | 5054 | `			pGen->pIn++;` |
|    7327 | 5055 | `		}` |
|   14660 | 5056 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 5057 | `			ph7_class *pInterface;` |
|       - | 5058 | `			SyString *pIntName;` |
|       - | 5059 | `			/* Interface implementation */` |
|       8 | 5060 | `			pGen->pIn++; /* Advance the stream cursor */` |
|       3 | 5061 | `			for(;;){` |
|       8 | 5062 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5063 | `					/* Syntax error */` |
|     ! 0 | 5064 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5065 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 5066 | `						pName);` |
|     ! 0 | 5067 | `					if( rc == SXERR_ABORT ){` |
|       - | 5068 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5069 | `						return SXERR_ABORT;` |
|       - | 5070 | `					}` |
|     ! 0 | 5071 | `					break;` |
|       - | 5072 | `				}` |
|       - | 5073 | `				/* Extract interface name and resolve through namespace/imports */` |
|       8 | 5074 | `				pIntName = &pGen->pIn->sData;` |
|       - | 5075 | `				{` |
|       - | 5076 | `					SyBlob sResolved;` |
|       8 | 5077 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       8 | 5078 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|      14 | 5079 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|       6 | 5080 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       8 | 5081 | `					SyBlobRelease(&sResolved);` |
|       - | 5082 | `				}` |
|       - | 5083 | `				/* Only interfaces are allowed */` |
|       8 | 5084 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5085 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 5086 | `				}` |
|       8 | 5087 | `				if( pInterface == 0 ){` |
|       - | 5088 | `					/* Inexistant interface */` |
|     ! 0 | 5089 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 5090 | `					if( rc == SXERR_ABORT ){` |
|       - | 5091 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5092 | `						return SXERR_ABORT;` |
|       - | 5093 | `					}` |
|     ! 0 | 5094 | `				}else{` |
|       - | 5095 | `					/* Register interface */` |
|       8 | 5096 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 5097 | `				}` |
|       - | 5098 | `				/* Advance the stream cursor */` |
|       8 | 5099 | `				pGen->pIn++;` |
|       8 | 5100 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       5 | 5101 | `					break;` |
|       - | 5102 | `				}` |
|     ! 0 | 5103 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 5104 | `			}` |
|       3 | 5105 | `		}` |
|    7329 | 5106 | `	}` |
|   22236 | 5107 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5108 | `		/* Syntax error */` |
|     ! 0 | 5109 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 5110 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5111 | `		if( rc == SXERR_ABORT ){` |
|       - | 5112 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5113 | `			return SXERR_ABORT;` |
|       - | 5114 | `		}` |
|     ! 0 | 5115 | `		return SXRET_OK;` |
|       - | 5116 | `	}` |
|   22236 | 5117 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   22236 | 5118 | `	pEnd = 0; /* cc warning */` |
|       - | 5119 | `	/* Delimit the class body */` |
|   22236 | 5120 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   22236 | 5121 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5122 | `		/* Syntax error */` |
|     ! 0 | 5123 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 5124 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5125 | `		if( rc == SXERR_ABORT ){` |
|       - | 5126 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5127 | `			return SXERR_ABORT;` |
|       - | 5128 | `		}` |
|     ! 0 | 5129 | `		return SXRET_OK;` |
|       - | 5130 | `	}` |
|       - | 5131 | `	/* Swap token stream */` |
|   22236 | 5132 | `	pTmp = pGen->pEnd;` |
|   22236 | 5133 | `	pGen->pEnd = pEnd;` |
|       - | 5134 | `	/* Set the inherited flags */` |
|   22236 | 5135 | `	pClass->iFlags = iFlags;` |
|       - | 5136 | `	/* Start the parse process */` |
|   36818 | 5137 | `	for(;;){` |
|       - | 5138 | `		/* Jump leading/trailing semi-colons */` |
|  122734 | 5139 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   24556 | 5140 | `			pGen->pIn++;` |
|       2 | 5141 | `		}` |
|   98180 | 5142 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5143 | `			/* End of class body */` |
|   22232 | 5144 | `			break;` |
|       - | 5145 | `		}` |
|   75950 | 5146 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5147 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5148 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5149 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5150 | `			if( rc == SXERR_ABORT ){` |
|       - | 5151 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5152 | `				return SXERR_ABORT;` |
|       - | 5153 | `			}` |
|     ! 0 | 5154 | `			goto done;` |
|       - | 5155 | `		}` |
|       - | 5156 | `		/* Assume public visibility */` |
|   75950 | 5157 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   75950 | 5158 | `		iAttrflags = 0;` |
|   75950 | 5159 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 5160 | `			/* Extract the current keyword */` |
|   75950 | 5161 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   75950 | 5162 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5163 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 5164 | `				TraitUseEntry sUse;` |
|      31 | 5165 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      31 | 5166 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      31 | 5167 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      23 | 5168 | `				for(;;){` |
|       - | 5169 | `					ph7_class *pTrait;` |
|       - | 5170 | `					SyString *pTraitName;` |
|      39 | 5171 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5172 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5173 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 5174 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5175 | `							return SXERR_ABORT;` |
|       - | 5176 | `						}` |
|     ! 0 | 5177 | `						break;` |
|       - | 5178 | `					}` |
|      39 | 5179 | `					pTraitName = &pGen->pIn->sData;` |
|       - | 5180 | `					/* Resolve trait name through namespace/imports */ {` |
|       - | 5181 | `						SyBlob sResolved;` |
|      39 | 5182 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      39 | 5183 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      77 | 5184 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      38 | 5185 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      39 | 5186 | `						SyBlobRelease(&sResolved);` |
|       - | 5187 | `					}` |
|       - | 5188 | `					/* Only traits are allowed */` |
|      39 | 5189 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5190 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 5191 | `					}` |
|      39 | 5192 | `					if( pTrait == 0 ){` |
|     ! 0 | 5193 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5194 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 | 5195 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5196 | `							return SXERR_ABORT;` |
|       - | 5197 | `						}` |
|     ! 0 | 5198 | `					}else{` |
|      39 | 5199 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 5200 | `					}` |
|      39 | 5201 | `					pGen->pIn++; /* Advance past trait name */` |
|      39 | 5202 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      16 | 5203 | `						break;` |
|       - | 5204 | `					}` |
|       9 | 5205 | `					pGen->pIn++; /* Jump the comma */` |
|       1 | 5206 | `				}` |
|       - | 5207 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      31 | 5208 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 5209 | `					SyToken *pBlock;` |
|       9 | 5210 | `					pGen->pIn++; /* Jump '{' */` |
|       9 | 5211 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 | 5212 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 | 5213 | `					sUse.pResolvEnd = pBlock;` |
|       9 | 5214 | `					if( pBlock < pGen->pEnd ){` |
|       9 | 5215 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 | 5216 | `					}else{` |
|     ! 0 | 5217 | `						pGen->pIn = pGen->pEnd;` |
|       - | 5218 | `					}` |
|       4 | 5219 | `				}` |
|      31 | 5220 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 5221 | `				/* The semicolon will be consumed by the outer loop */` |
|      31 | 5222 | `				continue;` |
|       - | 5223 | `			}` |
|   75920 | 5224 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|   73392 | 5225 | `				iProtection = nKwrd;` |
|   73392 | 5226 | `				pGen->pIn++; /* Jump the visibility token */` |
|   73392 | 5227 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5228 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5229 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5230 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5231 | `					if( rc == SXERR_ABORT ){` |
|       - | 5232 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5233 | `						return SXERR_ABORT;` |
|       - | 5234 | `					}` |
|     ! 0 | 5235 | `					goto done;` |
|       - | 5236 | `				}` |
|   73392 | 5237 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5238 | `					/* Attribute declaration */` |
|   24512 | 5239 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   24512 | 5240 | `					if( rc != SXRET_OK ){` |
|       3 | 5241 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5242 | `							return SXERR_ABORT;` |
|       - | 5243 | `						}` |
|       3 | 5244 | `						goto done;` |
|       - | 5245 | `					}` |
|   24510 | 5246 | `					continue;` |
|       - | 5247 | `				}` |
|       - | 5248 | `				/* Extract the keyword */` |
|   48882 | 5249 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   24440 | 5250 | `			}` |
|   51410 | 5251 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5252 | `				/* Process constant declaration */` |
|      10 | 5253 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 | 5254 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5255 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5256 | `						return SXERR_ABORT;` |
|       - | 5257 | `					}` |
|     ! 0 | 5258 | `					goto done;` |
|       - | 5259 | `				}` |
|       6 | 5260 | `			}else{` |
|   51402 | 5261 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5262 | `					/* Static method or attribute,record that */` |
|      23 | 5263 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      23 | 5264 | `					pGen->pIn++; /* Jump the static keyword */` |
|      23 | 5265 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5266 | `						/* Extract the keyword */` |
|      19 | 5267 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      19 | 5268 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 5269 | `							iProtection = nKwrd;` |
|     ! 0 | 5270 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 5271 | `						}` |
|       9 | 5272 | `					}` |
|      23 | 5273 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5274 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5275 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 5276 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5277 | `						if( rc == SXERR_ABORT ){` |
|       - | 5278 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5279 | `							return SXERR_ABORT;` |
|       - | 5280 | `						}` |
|     ! 0 | 5281 | `						goto done;` |
|       - | 5282 | `					}` |
|      23 | 5283 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5284 | `						/* Attribute declaration */` |
|       5 | 5285 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 5286 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 5287 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5288 | `								return SXERR_ABORT;` |
|       - | 5289 | `							}` |
|     ! 0 | 5290 | `							goto done;` |
|       - | 5291 | `						}` |
|       5 | 5292 | `						continue;` |
|       - | 5293 | `					}` |
|       - | 5294 | `					/* Extract the keyword */` |
|      19 | 5295 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   51389 | 5296 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 5297 | `					/* Abstract method,record that */` |
|       8 | 5298 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 5299 | `					/* Mark the whole class as abstract */` |
|       8 | 5300 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 5301 | `					/* Advance the stream cursor */` |
|       8 | 5302 | `					pGen->pIn++;` |
|       8 | 5303 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       8 | 5304 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       8 | 5305 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 | 5306 | `							iProtection = nKwrd;` |
|       6 | 5307 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 5308 | `						}` |
|       3 | 5309 | `					}` |
|       8 | 5310 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       6 | 5311 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5312 | `							/* Static method */` |
|     ! 0 | 5313 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5314 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5315 | `					}` |
|       8 | 5316 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       6 | 5317 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5318 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5319 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 5320 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5321 | `							if( rc == SXERR_ABORT ){` |
|       - | 5322 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5323 | `								return SXERR_ABORT;` |
|       - | 5324 | `							}` |
|     ! 0 | 5325 | `							goto done;` |
|       - | 5326 | `					}` |
|       8 | 5327 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   51377 | 5328 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 5329 | `					/* final method ,record that */` |
|       5 | 5330 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 5331 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 5332 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5333 | `						/* Extract the keyword */` |
|       5 | 5334 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 5335 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 5336 | `							iProtection = nKwrd;` |
|       5 | 5337 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 5338 | `						}` |
|       2 | 5339 | `					}` |
|       5 | 5340 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 5341 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5342 | `							/* Static method */` |
|     ! 0 | 5343 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5344 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5345 | `					}` |
|       5 | 5346 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 5347 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5348 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5349 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 5350 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5351 | `							if( rc == SXERR_ABORT ){` |
|       - | 5352 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5353 | `								return SXERR_ABORT;` |
|       - | 5354 | `							}` |
|     ! 0 | 5355 | `							goto done;` |
|       - | 5356 | `					}` |
|       5 | 5357 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 5358 | `				}` |
|   51398 | 5359 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 5360 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5361 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 5362 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5363 | `						if( rc == SXERR_ABORT ){` |
|       - | 5364 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5365 | `							return SXERR_ABORT;` |
|       - | 5366 | `						}` |
|     ! 0 | 5367 | `						goto done;` |
|       - | 5368 | `				}` |
|   51398 | 5369 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 5370 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 5371 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 5372 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5373 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 5374 | `						if( rc == SXERR_ABORT ){` |
|       - | 5375 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5376 | `							return SXERR_ABORT;` |
|       - | 5377 | `						}` |
|     ! 0 | 5378 | `						goto done;` |
|       - | 5379 | `					}` |
|       - | 5380 | `					/* Attribute declaration */` |
|       7 | 5381 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 5382 | `				}else{` |
|       - | 5383 | `					/* Process method declaration */` |
|   51392 | 5384 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 5385 | `				}` |
|   51398 | 5386 | `				if( rc != SXRET_OK ){` |
|       3 | 5387 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5388 | `						return SXERR_ABORT;` |
|       - | 5389 | `					}` |
|       3 | 5390 | `					goto done;` |
|       - | 5391 | `				}` |
|       - | 5392 | `			}` |
|   25703 | 5393 | `		}else{` |
|       - | 5394 | `			/* Attribute declaration */` |
|     ! 0 | 5395 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5396 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5397 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5398 | `					return SXERR_ABORT;` |
|       - | 5399 | `				}` |
|     ! 0 | 5400 | `				goto done;` |
|       - | 5401 | `			}` |
|       - | 5402 | `		}` |
|       2 | 5403 | `	}` |
|       - | 5404 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 5405 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 5406 | `	 */` |
|       - | 5407 | `	{` |
|       - | 5408 | `		TraitUseEntry *apUse;` |
|       - | 5409 | `		sxu32 nU;` |
|   22232 | 5410 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   22262 | 5411 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      31 | 5412 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      31 | 5413 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      31 | 5414 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      31 | 5415 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 5416 | `			sxu32 nT;` |
|      31 | 5417 | `			if( !hasResolution ){` |
|       - | 5418 | `				/* No conflict resolution block: use standard trait application */` |
|      51 | 5419 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      29 | 5420 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      29 | 5421 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 5422 | `						break;` |
|       - | 5423 | `					}` |
|      15 | 5424 | `				}` |
|      12 | 5425 | `			}else{` |
|       - | 5426 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 5427 | `				 * then use the block to resolve method conflicts.` |
|       - | 5428 | `				 */` |
|       - | 5429 | `				SyToken *pR;` |
|      19 | 5430 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 | 5431 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 5432 | `					ph7_class_attr *pAR;` |
|       - | 5433 | `					SyHashEntry *pER;` |
|       - | 5434 | `					SyString *pNR;` |
|      11 | 5435 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 | 5436 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 5437 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 5438 | `						pNR = &pAR->sName;` |
|     ! 0 | 5439 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 5440 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 5441 | `						}` |
|     ! 0 | 5442 | `					}` |
|      11 | 5443 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 | 5444 | `				}` |
|       - | 5445 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 | 5446 | `				pR = pUse->pResolvStart;` |
|      21 | 5447 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 5448 | `					SyString sTrait,sMethod;` |
|       - | 5449 | `					ph7_class *pSrcTrait;` |
|       - | 5450 | `					ph7_class_method *pMeth;` |
|       - | 5451 | `					sxi32 nRKwrd;` |
|      33 | 5452 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 5453 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 5454 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 5455 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 5456 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 5457 | `					sMethod = pR->sData;` |
|      13 | 5458 | `					pR++;` |
|      13 | 5459 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 5460 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 5461 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 5462 | `							sTrait = sMethod;` |
|       7 | 5463 | `							pR++;` |
|       7 | 5464 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 5465 | `							sMethod = pR->sData;` |
|       7 | 5466 | `							pR++;` |
|       3 | 5467 | `						}` |
|       3 | 5468 | `					}` |
|      13 | 5469 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5470 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 5471 | `						continue;` |
|       - | 5472 | `					}` |
|      13 | 5473 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 5474 | `					pR++;` |
|      13 | 5475 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 | 5476 | `						pSrcTrait = 0;` |
|       7 | 5477 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 | 5478 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 | 5479 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 | 5480 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 | 5481 | `								pSrcTrait = apTrait[nT];` |
|       5 | 5482 | `								break;` |
|       - | 5483 | `							}` |
|       2 | 5484 | `						}` |
|       5 | 5485 | `						if( pSrcTrait ){` |
|       5 | 5486 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 | 5487 | `							if( pMeth ){` |
|       5 | 5488 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 | 5489 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 | 5490 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 | 5491 | `								}` |
|       2 | 5492 | `							}` |
|       2 | 5493 | `						}` |
|       2 | 5494 | `					}` |
|      29 | 5495 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 5496 | `				}` |
|       - | 5497 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 | 5498 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 5499 | `					ph7_class_method *pMR;` |
|       - | 5500 | `					SyHashEntry *pER;` |
|       - | 5501 | `					SyString *pNR;` |
|      11 | 5502 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 | 5503 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 | 5504 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 | 5505 | `						pNR = &pMR->sFunc.sName;` |
|      19 | 5506 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 | 5507 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 | 5508 | `						}` |
|       1 | 5509 | `					}` |
|       6 | 5510 | `				}` |
|       - | 5511 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 | 5512 | `				pR = pUse->pResolvStart;` |
|      21 | 5513 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 5514 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 5515 | `					ph7_class *pSrcTrait;` |
|       - | 5516 | `					ph7_class_method *pMeth;` |
|      21 | 5517 | `					int hasQual = 0;` |
|       - | 5518 | `					sxi32 nRKwrd;` |
|      33 | 5519 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 5520 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 5521 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 5522 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 5523 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 | 5524 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 5525 | `					sMethod = pR->sData;` |
|      13 | 5526 | `					pR++;` |
|      13 | 5527 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 5528 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 5529 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 5530 | `							sTrait = sMethod;` |
|       7 | 5531 | `							hasQual = 1;` |
|       7 | 5532 | `							pR++;` |
|       7 | 5533 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 5534 | `							sMethod = pR->sData;` |
|       7 | 5535 | `							pR++;` |
|       3 | 5536 | `						}` |
|       3 | 5537 | `					}` |
|      13 | 5538 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5539 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 5540 | `						continue;` |
|       - | 5541 | `					}` |
|      13 | 5542 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 5543 | `					pR++;` |
|      13 | 5544 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 | 5545 | `						sxi32 iNewVis = -1;` |
|       9 | 5546 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 | 5547 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 | 5548 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 | 5549 | `								iNewVis = nAK;` |
|       7 | 5550 | `								pR++;` |
|       3 | 5551 | `							}` |
|       3 | 5552 | `						}` |
|       9 | 5553 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 | 5554 | `							sAlias = pR->sData;` |
|       7 | 5555 | `							pR++;` |
|       3 | 5556 | `						}` |
|       9 | 5557 | `						pMeth = 0;` |
|       9 | 5558 | `						if( hasQual ){` |
|       3 | 5559 | `							pSrcTrait = 0;` |
|       5 | 5560 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 | 5561 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 | 5562 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 | 5563 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 | 5564 | `									pSrcTrait = apTrait[nT];` |
|       3 | 5565 | `									break;` |
|       - | 5566 | `								}` |
|       2 | 5567 | `							}` |
|       3 | 5568 | `							if( pSrcTrait ){` |
|       3 | 5569 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 | 5570 | `							}` |
|       2 | 5571 | `						}else{` |
|       7 | 5572 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 5573 | `						}` |
|       9 | 5574 | `						if( pMeth ){` |
|       9 | 5575 | `							if( sAlias.nByte > 0 ){` |
|       - | 5576 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 5577 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 5578 | `								 */` |
|       - | 5579 | `								ph7_class_method *pAlias;` |
|       - | 5580 | `								char *zAliasDup;` |
|       7 | 5581 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 | 5582 | `								if( pAlias ){` |
|       7 | 5583 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 | 5584 | `									if( iNewVis >= 0 ){` |
|       5 | 5585 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 5586 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 5587 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 | 5588 | `									}` |
|       7 | 5589 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 | 5590 | `									if( zAliasDup ){` |
|       7 | 5591 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 | 5592 | `									}` |
|       4 | 5593 | `								}` |
|       6 | 5594 | `							}else if( iNewVis >= 0 ){` |
|       - | 5595 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 5596 | `								ph7_class_method *pCopy;` |
|       3 | 5597 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 5598 | `								if( pCopy ){` |
|       3 | 5599 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 5600 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 5601 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 5602 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 5603 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 5604 | `									/* Replace the method in the class hash */` |
|       3 | 5605 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 5606 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 5607 | `								}` |
|       1 | 5608 | `							}` |
|       4 | 5609 | `						}` |
|       4 | 5610 | `						SXUNUSED(hasQual);` |
|       4 | 5611 | `					}` |
|      17 | 5612 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 5613 | `				}` |
|       - | 5614 | `			}` |
|      31 | 5615 | `			SySetRelease(&pUse->aTraits);` |
|      16 | 5616 | `		}` |
|       - | 5617 | `	}` |
|       - | 5618 | `	/* Install the class */` |
|   22232 | 5619 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   22232 | 5620 | `	if( rc == SXRET_OK ){` |
|       - | 5621 | `		ph7_class **apInterface;` |
|       - | 5622 | `		sxu32 n;` |
|   22232 | 5623 | `		if( pBase ){` |
|       - | 5624 | `			/* Inherit from base class and mark as a subclass */` |
|   14656 | 5625 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    7327 | 5626 | `		}` |
|   22232 | 5627 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   22238 | 5628 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 5629 | `			/* Implements one or more interface */` |
|       8 | 5630 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|       8 | 5631 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5632 | `				break;` |
|       - | 5633 | `			}` |
|       5 | 5634 | `		}` |
|   11115 | 5635 | `	}` |
|   22232 | 5636 | `	SySetRelease(&aUseEntries);` |
|   22232 | 5637 | `	SySetRelease(&aInterfaces);` |
|   22232 | 5638 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5639 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5640 | `		return SXERR_ABORT;` |
|       - | 5641 | `	}` |
|   11115 | 5642 | `done:` |
|       - | 5643 | `	/* Point beyond the class body */` |
|   22236 | 5644 | `	pGen->pIn = &pEnd[1];` |
|   22236 | 5645 | `	pGen->pEnd = pTmp;` |
|   22236 | 5646 | `	return PH7_OK;` |
|   11119 | 5647 |  |
|       - | 5648 | `/*` |
|       - | 5649 | ` * Compile a user-defined abstract class.` |
|       - | 5650 | ` *  According to the PHP language reference manual` |
|       - | 5651 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 5652 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 5653 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 5654 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 5655 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 5656 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 5657 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 5658 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 5659 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 5660 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 5661 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 5662 | ` *   could differ.` |
|       - | 5663 | ` */` |
|       4 | 5664 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 5665 |  |
|       - | 5666 | `	sxi32 rc;` |
|       6 | 5667 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|       6 | 5668 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|       6 | 5669 | `	return rc;` |
|       2 | 5670 |  |
|       - | 5671 | `/*` |
|       - | 5672 | ` * Compile a user-defined final class.` |
|       - | 5673 | ` *  According to the PHP language reference manual` |
|       - | 5674 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 5675 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 5676 | ` *    final then it cannot be extended.` |
|       - | 5677 | ` */` |
|       2 | 5678 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 5679 |  |
|       - | 5680 | `	sxi32 rc;` |
|       3 | 5681 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 5682 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 5683 | `	return rc;` |
|       1 | 5684 |  |
|       - | 5685 | `/*` |
|       - | 5686 | ` * Compile a user-defined trait.` |
|       - | 5687 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 5688 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 5689 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 5690 | ` */` |
|      42 | 5691 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       1 | 5692 |  |
|      43 | 5693 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5694 | `	ph7_class *pClass;` |
|       - | 5695 | `	SyToken *pEnd,*pTmp;` |
|       - | 5696 | `	sxi32 iProtection;` |
|       - | 5697 | `	sxi32 iAttrflags;` |
|       - | 5698 | `	SyString *pName;` |
|       - | 5699 | `	sxi32 nKwrd;` |
|       - | 5700 | `	sxi32 rc;` |
|       - | 5701 | `	/* Jump the 'trait' keyword */` |
|      43 | 5702 | `	pGen->pIn++;` |
|      43 | 5703 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5704 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 5705 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5706 | `			return SXERR_ABORT;` |
|       - | 5707 | `		}` |
|     ! 0 | 5708 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 5709 | `			pGen->pIn++;` |
|     ! 0 | 5710 | `		}` |
|     ! 0 | 5711 | `		return SXRET_OK;` |
|       - | 5712 | `	}` |
|       - | 5713 | `	/* Extract trait name */` |
|      43 | 5714 | `	pName = &pGen->pIn->sData;` |
|      43 | 5715 | `	pGen->pIn++;` |
|       - | 5716 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5717 | `		SyBlob sFQN;` |
|       - | 5718 | `		SyString sFQNStr;` |
|      43 | 5719 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      43 | 5720 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      43 | 5721 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      43 | 5722 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      43 | 5723 | `		SyBlobRelease(&sFQN);` |
|       - | 5724 | `	}` |
|      43 | 5725 | `	if( pClass == 0 ){` |
|     ! 0 | 5726 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5727 | `		return SXERR_ABORT;` |
|       - | 5728 | `	}` |
|       - | 5729 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      43 | 5730 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 5731 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 5732 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5733 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5734 | `			return SXERR_ABORT;` |
|       - | 5735 | `		}` |
|     ! 0 | 5736 | `		return SXRET_OK;` |
|       - | 5737 | `	}` |
|      43 | 5738 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      43 | 5739 | `	pEnd = 0;` |
|      43 | 5740 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      43 | 5741 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 5742 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 5743 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5744 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5745 | `			return SXERR_ABORT;` |
|       - | 5746 | `		}` |
|     ! 0 | 5747 | `		return SXRET_OK;` |
|       - | 5748 | `	}` |
|       - | 5749 | `	/* Swap token stream */` |
|      43 | 5750 | `	pTmp = pGen->pEnd;` |
|      43 | 5751 | `	pGen->pEnd = pEnd;` |
|       - | 5752 | `	/* Mark as trait */` |
|      43 | 5753 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 5754 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      44 | 5755 | `	for(;;){` |
|     119 | 5756 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      17 | 5757 | `			pGen->pIn++;` |
|       1 | 5758 | `		}` |
|     103 | 5759 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      43 | 5760 | `			break;` |
|       - | 5761 | `		}` |
|      61 | 5762 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5763 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5764 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 5765 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5766 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5767 | `				return SXERR_ABORT;` |
|       - | 5768 | `			}` |
|     ! 0 | 5769 | `			goto done;` |
|       - | 5770 | `		}` |
|      61 | 5771 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      61 | 5772 | `		iAttrflags = 0;` |
|      61 | 5773 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      61 | 5774 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      61 | 5775 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5776 | `				/* Trait uses another trait: use OtherTrait; */` |
|       3 | 5777 | `				pGen->pIn++; /* Jump 'use' */` |
|       1 | 5778 | `				for(;;){` |
|       - | 5779 | `					ph7_class *pUsedTrait;` |
|       - | 5780 | `					SyString *pUsedName;` |
|       3 | 5781 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5782 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5783 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 5784 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5785 | `							return SXERR_ABORT;` |
|       - | 5786 | `						}` |
|     ! 0 | 5787 | `						break;` |
|       - | 5788 | `					}` |
|       3 | 5789 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 5790 | `					{` |
|       - | 5791 | `						SyBlob sResolved;` |
|       3 | 5792 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 5793 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       4 | 5794 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 5795 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 5796 | `						SyBlobRelease(&sResolved);` |
|       - | 5797 | `					}` |
|       3 | 5798 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5799 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 5800 | `					}` |
|       3 | 5801 | `					if( pUsedTrait == 0 ){` |
|     ! 0 | 5802 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5803 | `							"'%z' is not a trait",pUsedName);` |
|     ! 0 | 5804 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5805 | `							return SXERR_ABORT;` |
|       - | 5806 | `						}` |
|     ! 0 | 5807 | `					}else{` |
|       3 | 5808 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 5809 | `					}` |
|       3 | 5810 | `					pGen->pIn++;` |
|       3 | 5811 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       2 | 5812 | `						break;` |
|       - | 5813 | `					}` |
|     ! 0 | 5814 | `					pGen->pIn++;` |
|     ! 0 | 5815 | `				}` |
|       3 | 5816 | `				continue;` |
|       - | 5817 | `			}` |
|      59 | 5818 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      57 | 5819 | `				iProtection = nKwrd;` |
|      57 | 5820 | `				pGen->pIn++;` |
|      57 | 5821 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5822 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5823 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 5824 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5825 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5826 | `						return SXERR_ABORT;` |
|       - | 5827 | `					}` |
|     ! 0 | 5828 | `					goto done;` |
|       - | 5829 | `				}` |
|      57 | 5830 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 | 5831 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 | 5832 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 5833 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5834 | `							return SXERR_ABORT;` |
|       - | 5835 | `						}` |
|     ! 0 | 5836 | `						goto done;` |
|       - | 5837 | `					}` |
|      11 | 5838 | `					continue;` |
|       - | 5839 | `				}` |
|      47 | 5840 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      23 | 5841 | `			}` |
|      49 | 5842 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 5843 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5844 | `					"Traits cannot have constants");` |
|     ! 0 | 5845 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5846 | `					return SXERR_ABORT;` |
|       - | 5847 | `				}` |
|     ! 0 | 5848 | `				goto done;` |
|     ! 0 | 5849 | `			}else{` |
|      49 | 5850 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 5851 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 5852 | `					pGen->pIn++;` |
|       5 | 5853 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 5854 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 5855 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 5856 | `							iProtection = nKwrd;` |
|     ! 0 | 5857 | `							pGen->pIn++;` |
|     ! 0 | 5858 | `						}` |
|       1 | 5859 | `					}` |
|       5 | 5860 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5861 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5862 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 5863 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5864 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5865 | `							return SXERR_ABORT;` |
|       - | 5866 | `						}` |
|     ! 0 | 5867 | `						goto done;` |
|       - | 5868 | `					}` |
|       5 | 5869 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 5870 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 5871 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 5872 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5873 | `								return SXERR_ABORT;` |
|       - | 5874 | `							}` |
|     ! 0 | 5875 | `							goto done;` |
|       - | 5876 | `						}` |
|       3 | 5877 | `						continue;` |
|       - | 5878 | `					}` |
|       3 | 5879 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      46 | 5880 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       3 | 5881 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       3 | 5882 | `					pGen->pIn++;` |
|       3 | 5883 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 5884 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 5885 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       3 | 5886 | `							iProtection = nKwrd;` |
|       3 | 5887 | `							pGen->pIn++;` |
|       1 | 5888 | `						}` |
|       1 | 5889 | `					}` |
|       3 | 5890 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       2 | 5891 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5892 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5893 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 5894 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5895 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5896 | `							return SXERR_ABORT;` |
|       - | 5897 | `						}` |
|     ! 0 | 5898 | `						goto done;` |
|       - | 5899 | `					}` |
|       3 | 5900 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       1 | 5901 | `				}` |
|      47 | 5902 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 5903 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5904 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 5905 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5906 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5907 | `						return SXERR_ABORT;` |
|       - | 5908 | `					}` |
|     ! 0 | 5909 | `					goto done;` |
|       - | 5910 | `				}` |
|      47 | 5911 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 5912 | `					pGen->pIn++;` |
|     ! 0 | 5913 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 5914 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5915 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 5916 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5917 | `							return SXERR_ABORT;` |
|       - | 5918 | `						}` |
|     ! 0 | 5919 | `						goto done;` |
|       - | 5920 | `					}` |
|     ! 0 | 5921 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5922 | `				}else{` |
|      47 | 5923 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 5924 | `				}` |
|      47 | 5925 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5926 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5927 | `						return SXERR_ABORT;` |
|       - | 5928 | `					}` |
|     ! 0 | 5929 | `					goto done;` |
|       - | 5930 | `				}` |
|       - | 5931 | `			}` |
|      24 | 5932 | `		}else{` |
|     ! 0 | 5933 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5934 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5935 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5936 | `					return SXERR_ABORT;` |
|       - | 5937 | `				}` |
|     ! 0 | 5938 | `				goto done;` |
|       - | 5939 | `			}` |
|       - | 5940 | `		}` |
|       1 | 5941 | `	}` |
|       - | 5942 | `	/* Install the trait */` |
|      43 | 5943 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      43 | 5944 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5945 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5946 | `		return SXERR_ABORT;` |
|       - | 5947 | `	}` |
|      21 | 5948 | `done:` |
|       - | 5949 | `	/* Point beyond the trait body */` |
|      43 | 5950 | `	pGen->pIn = &pEnd[1];` |
|      43 | 5951 | `	pGen->pEnd = pTmp;` |
|      43 | 5952 | `	return PH7_OK;` |
|      22 | 5953 |  |
|       - | 5954 | `/*` |
|       - | 5955 | ` * Compile a user-defined class.` |
|       - | 5956 | ` *  According to the PHP language reference manual` |
|       - | 5957 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 5958 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 5959 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 5960 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 5961 | ` *   and functions (called "methods").` |
|       - | 5962 | ` */` |
|   22228 | 5963 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 5964 |  |
|       - | 5965 | `	sxi32 rc;` |
|   22230 | 5966 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   22230 | 5967 | `	return rc;` |
|       2 | 5968 |  |
|       - | 5969 | `/*` |
|       - | 5970 | ` * Exception handling.` |
|       - | 5971 | ` *  According to the PHP language reference manual` |
|       - | 5972 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 5973 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 5974 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 5975 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 5976 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 5977 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 5978 | ` *    (or re-thrown) within a catch block.` |
|       - | 5979 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 5980 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 5981 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 5982 | ` *    been defined with set_exception_handler().` |
|       - | 5983 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 5984 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 5985 | ` */` |
|       - | 5986 | `/*` |
|       - | 5987 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 5988 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 5989 | ` * indicates failure.` |
|       - | 5990 | ` */` |
|    7330 | 5991 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 5992 |  |
|    7332 | 5993 | `	sxi32 rc = SXRET_OK;` |
|    7332 | 5994 | `	if( pRoot->pOp ){` |
|    7328 | 5995 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    3666 | 5996 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 5997 | `			/* Unexpected expression */` |
|     ! 0 | 5998 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 5999 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 6000 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 6001 | `				rc = SXERR_INVALID;` |
|     ! 0 | 6002 | `			}` |
|       2 | 6003 | `		}` |
|    3667 | 6004 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 6005 | `		/* Unexpected expression */` |
|     ! 0 | 6006 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6007 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 6008 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 6009 | `			rc = SXERR_INVALID;` |
|     ! 0 | 6010 | `		}` |
|     ! 0 | 6011 | `	}` |
|    7332 | 6012 | `	return rc;` |
|       2 | 6013 |  |
|       - | 6014 | `/*` |
|       - | 6015 | ` * Compile a 'throw' statement.` |
|       - | 6016 | ` * throw: This is how you trigger an exception.` |
|       - | 6017 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 6018 | ` */` |
|    7330 | 6019 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 6020 |  |
|    7332 | 6021 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6022 | `	GenBlock *pBlock;` |
|       - | 6023 | `	sxu32 nIdx;` |
|       - | 6024 | `	sxi32 rc;` |
|    7332 | 6025 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 6026 | `	/* Compile the expression */` |
|    7332 | 6027 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    7332 | 6028 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 6029 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 6030 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6031 | `			return SXERR_ABORT;` |
|       - | 6032 | `		}` |
|     ! 0 | 6033 | `		return SXRET_OK;` |
|       - | 6034 | `	}` |
|    7332 | 6035 | `	pBlock = pGen->pCurrent;` |
|       - | 6036 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   34134 | 6037 | `	while(pBlock->pParent){` |
|   34130 | 6038 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    7328 | 6039 | `			break;` |
|       - | 6040 | `		}` |
|       - | 6041 | `		/* Point to the parent block */` |
|   26804 | 6042 | `		pBlock = pBlock->pParent;` |
|       2 | 6043 | `	}` |
|       - | 6044 | `	/* Emit the throw instruction */` |
|    7332 | 6045 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 6046 | `	/* Emit the jump */` |
|    7332 | 6047 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    7332 | 6048 | `	return SXRET_OK;` |
|    3667 | 6049 |  |
|       - | 6050 | `/*` |
|       - | 6051 | ` * Compile a 'catch' block.` |
|       - | 6052 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 6053 | ` * an object containing the exception information.` |
|       - | 6054 | ` */` |
|      48 | 6055 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 6056 |  |
|      50 | 6057 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6058 | `	ph7_exception_block sCatch;` |
|       - | 6059 | `	SySet *pInstrContainer;` |
|       - | 6060 | `	GenBlock *pCatch;` |
|       - | 6061 | `	SyToken *pToken;` |
|       - | 6062 | `	SyString *pName;` |
|       - | 6063 | `	char *zDup;` |
|       - | 6064 | `	sxi32 rc;` |
|      50 | 6065 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 6066 | `	/* Zero the structure */` |
|      50 | 6067 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 6068 | `	/* Initialize fields */` |
|      50 | 6069 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|      72 | 6070 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ \|\|` |
|      50 | 6071 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6072 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6073 | `			pToken = pGen->pIn;` |
|     ! 0 | 6074 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6075 | `				pToken--;` |
|     ! 0 | 6076 | `			}` |
|     ! 0 | 6077 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6078 | `				"Catch: Unexpected token '%z',excpecting class name",&pToken->sData);` |
|     ! 0 | 6079 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6080 | `				return SXERR_ABORT;` |
|       - | 6081 | `			}` |
|     ! 0 | 6082 | `			return SXERR_INVALID;` |
|       - | 6083 | `	}` |
|       - | 6084 | `	/* Extract the exception class */` |
|      50 | 6085 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|       - | 6086 | `	/* Duplicate class name */` |
|      50 | 6087 | `	pName = &pGen->pIn->sData;` |
|      50 | 6088 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      50 | 6089 | `	if( zDup == 0 ){` |
|     ! 0 | 6090 | `		goto Mem;` |
|       - | 6091 | `	}` |
|      50 | 6092 | `	SyStringInitFromBuf(&sCatch.sClass,zDup,pName->nByte);` |
|      50 | 6093 | `	pGen->pIn++;` |
|      72 | 6094 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      50 | 6095 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6096 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6097 | `			pToken = pGen->pIn;` |
|     ! 0 | 6098 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6099 | `				pToken--;` |
|     ! 0 | 6100 | `			}` |
|     ! 0 | 6101 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6102 | `				"Catch: Unexpected token '%z',expecting variable name",&pToken->sData);` |
|     ! 0 | 6103 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6104 | `				return SXERR_ABORT;` |
|       - | 6105 | `			}` |
|     ! 0 | 6106 | `			return SXERR_INVALID;` |
|       - | 6107 | `	}` |
|      50 | 6108 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 6109 | `	/* Duplicate instance name */` |
|      50 | 6110 | `	pName = &pGen->pIn->sData;` |
|      50 | 6111 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      50 | 6112 | `	if( zDup == 0 ){` |
|     ! 0 | 6113 | `		goto Mem;` |
|       - | 6114 | `	}` |
|      50 | 6115 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      50 | 6116 | `	pGen->pIn++;` |
|      50 | 6117 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 6118 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 6119 | `		pToken = pGen->pIn;` |
|     ! 0 | 6120 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6121 | `			pToken--;` |
|     ! 0 | 6122 | `		}` |
|     ! 0 | 6123 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6124 | `			"Catch: Unexpected token '%z',expecting right parenthesis ')'",&pToken->sData);` |
|     ! 0 | 6125 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6126 | `			return SXERR_ABORT;` |
|       - | 6127 | `		}` |
|     ! 0 | 6128 | `		return SXERR_INVALID;` |
|       - | 6129 | `	}` |
|       - | 6130 | `	/* Compile the block */` |
|      50 | 6131 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 6132 | `	/* Create the catch block */` |
|      50 | 6133 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      50 | 6134 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6135 | `		return SXERR_ABORT;` |
|       - | 6136 | `	}` |
|       - | 6137 | `	/* Swap bytecode container */` |
|      50 | 6138 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      50 | 6139 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 6140 | `	/* Compile the block */` |
|      50 | 6141 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 6142 | `	/* Fix forward jumps now the destination is resolved  */` |
|      50 | 6143 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6144 | `	/* Emit the DONE instruction */` |
|      50 | 6145 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6146 | `	/* Leave the block */` |
|      50 | 6147 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6148 | `	/* Restore the default container */` |
|      50 | 6149 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 6150 | `	/* Install the catch block */` |
|      50 | 6151 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      50 | 6152 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6153 | `		goto Mem;` |
|       - | 6154 | `	}` |
|      50 | 6155 | `	return SXRET_OK;` |
|     ! 0 | 6156 | `Mem:` |
|     ! 0 | 6157 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6158 | `	return SXERR_ABORT;` |
|      26 | 6159 |  |
|       - | 6160 | `/*` |
|       - | 6161 | ` * Compile a 'try' block.` |
|       - | 6162 | ` * A function using an exception should be in a "try" block.` |
|       - | 6163 | ` * If the exception does not trigger, the code will continue` |
|       - | 6164 | ` * as normal. However if the exception triggers, an exception` |
|       - | 6165 | ` * is "thrown".` |
|       - | 6166 | ` */` |
|      56 | 6167 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 6168 |  |
|       - | 6169 | `	ph7_exception *pException;` |
|      58 | 6170 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6171 | `	GenBlock *pTry;` |
|       - | 6172 | `	sxu32 nJmpIdx;` |
|       - | 6173 | `	sxi32 rc;` |
|       - | 6174 | `	/* Create the exception container */` |
|      58 | 6175 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|      58 | 6176 | `	if( pException == 0 ){` |
|     ! 0 | 6177 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 6178 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6179 | `		return SXERR_ABORT;` |
|       - | 6180 | `	}` |
|       - | 6181 | `	/* Zero the structure */` |
|      58 | 6182 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 6183 | `	/* Initialize fields */` |
|      58 | 6184 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|      58 | 6185 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      58 | 6186 | `	pException->iHasFinally = 0;` |
|      58 | 6187 | `	pException->iFinallyDone = 0;` |
|      58 | 6188 | `	pException->pVm = pGen->pVm;` |
|       - | 6189 | `	/* Create the try block */` |
|      58 | 6190 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      58 | 6191 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6192 | `		return SXERR_ABORT;` |
|       - | 6193 | `	}` |
|       - | 6194 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|      58 | 6195 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 6196 | `	/* Fix the jump later when the destination is resolved */` |
|      58 | 6197 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|      58 | 6198 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 6199 | `	/* Compile the block */` |
|      58 | 6200 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      58 | 6201 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6202 | `		return SXERR_ABORT;` |
|       - | 6203 | `	}` |
|       - | 6204 | `	/* Fix forward jumps now the destination is resolved */` |
|      58 | 6205 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6206 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|      58 | 6207 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 6208 | `	/* Leave the block */` |
|      58 | 6209 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6210 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|      58 | 6211 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      54 | 6212 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 6213 | `		/* Compile one or more catch blocks */` |
|      48 | 6214 | `		for(;;){` |
|      96 | 6215 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      74 | 6216 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      26 | 6217 | `					break;` |
|       - | 6218 | `			}` |
|      50 | 6219 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|      50 | 6220 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6221 | `				return SXERR_ABORT;` |
|       - | 6222 | `			}` |
|       2 | 6223 | `		}` |
|      24 | 6224 | `	}` |
|       - | 6225 | `	/* Compile optional finally block */` |
|      58 | 6226 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      24 | 6227 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 6228 | `		SySet *pInstrContainer;` |
|       - | 6229 | `		GenBlock *pFinBlock;` |
|      21 | 6230 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 6231 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      21 | 6232 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      21 | 6233 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6234 | `			return SXERR_ABORT;` |
|       - | 6235 | `		}` |
|       - | 6236 | `		/* Swap bytecode container */` |
|      21 | 6237 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      21 | 6238 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 6239 | `		/* Compile the finally body */` |
|      21 | 6240 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      21 | 6241 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6242 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 6243 | `			return SXERR_ABORT;` |
|       - | 6244 | `		}` |
|       - | 6245 | `		/* Fix forward jumps now the destination is resolved */` |
|      21 | 6246 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6247 | `		/* Emit DONE to terminate the finally block */` |
|      21 | 6248 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6249 | `		/* Leave the block */` |
|      21 | 6250 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6251 | `		/* Restore the default container */` |
|      21 | 6252 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      21 | 6253 | `		pException->iHasFinally = 1;` |
|      10 | 6254 | `	}` |
|       - | 6255 | `	/* Must have at least one catch or finally */` |
|      58 | 6256 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       3 | 6257 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 6258 | `			"Cannot use try without catch or finally");` |
|       3 | 6259 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6260 | `			return SXERR_ABORT;` |
|       - | 6261 | `		}` |
|       1 | 6262 | `	}` |
|      58 | 6263 | `	return SXRET_OK;` |
|      30 | 6264 |  |
|       - | 6265 | `/*` |
|       - | 6266 | ` * Compile a switch block.` |
|       - | 6267 | ` *  (See block-comment below for more information)` |
|       - | 6268 | ` */` |
|      84 | 6269 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 6270 |  |
|      86 | 6271 | `	sxi32 rc = SXRET_OK;` |
|      86 | 6272 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 6273 | `		/* Unexpected token */` |
|     ! 0 | 6274 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 6275 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6276 | `			return SXERR_ABORT;` |
|       - | 6277 | `		}` |
|     ! 0 | 6278 | `		pGen->pIn++;` |
|     ! 0 | 6279 | `	}` |
|      86 | 6280 | `	pGen->pIn++;` |
|       - | 6281 | `	/* First instruction to execute in this block. */` |
|      86 | 6282 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 6283 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 6284 | `	 * or the '}' token */` |
|     151 | 6285 | `	for(;;){` |
|     304 | 6286 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6287 | `			/* No more input to process */` |
|     ! 0 | 6288 | `			break;` |
|       - | 6289 | `		}` |
|     304 | 6290 | `		rc = SXRET_OK;` |
|     304 | 6291 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      62 | 6292 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      20 | 6293 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 6294 | `					/* Unexpected token */` |
|     ! 0 | 6295 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 6296 | `						&pGen->pIn->sData);` |
|     ! 0 | 6297 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6298 | `						return SXERR_ABORT;` |
|       - | 6299 | `					}` |
|       - | 6300 | `					/* FALL THROUGH */` |
|     ! 0 | 6301 | `				}` |
|      20 | 6302 | `				rc = SXERR_EOF;` |
|      20 | 6303 | `				break;` |
|       - | 6304 | `			}` |
|      23 | 6305 | `		}else{` |
|       - | 6306 | `			sxi32 nKwrd;` |
|       - | 6307 | `			/* Extract the keyword */` |
|     244 | 6308 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     244 | 6309 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      34 | 6310 | `				break;` |
|       - | 6311 | `			}` |
|     180 | 6312 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 6313 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 6314 | `					/* Unexpected token */` |
|     ! 0 | 6315 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 6316 | `						&pGen->pIn->sData);` |
|     ! 0 | 6317 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6318 | `						return SXERR_ABORT;` |
|       - | 6319 | `					}` |
|       - | 6320 | `					/* FALL THROUGH */` |
|     ! 0 | 6321 | `				}` |
|       - | 6322 | `				/* Block compiled */` |
|       3 | 6323 | `				break;` |
|       - | 6324 | `			}` |
|       - | 6325 | `		}` |
|       - | 6326 | `		/* Compile block */` |
|     220 | 6327 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     220 | 6328 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6329 | `			return SXERR_ABORT;` |
|       - | 6330 | `		}` |
|       2 | 6331 | `	}` |
|      86 | 6332 | `	return rc;` |
|      44 | 6333 |  |
|       - | 6334 | `/*` |
|       - | 6335 | ` * Compile a case eXpression.` |
|       - | 6336 | ` *  (See block-comment below for more information)` |
|       - | 6337 | ` */` |
|      70 | 6338 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 6339 |  |
|       - | 6340 | `	SySet *pInstrContainer;` |
|       - | 6341 | `	SyToken *pEnd,*pTmp;` |
|      72 | 6342 | `	sxi32 iNest = 0;` |
|       - | 6343 | `	sxi32 rc;` |
|       - | 6344 | `	/* Delimit the expression */` |
|      72 | 6345 | `	pEnd = pGen->pIn;` |
|     150 | 6346 | `	while( pEnd < pGen->pEnd ){` |
|     150 | 6347 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 6348 | `			/* Increment nesting level */` |
|       3 | 6349 | `			iNest++;` |
|     149 | 6350 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 6351 | `			/* Decrement nesting level */` |
|       3 | 6352 | `			iNest--;` |
|     147 | 6353 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      72 | 6354 | `			break;` |
|       - | 6355 | `		}` |
|      80 | 6356 | `		pEnd++;` |
|       2 | 6357 | `	}` |
|      72 | 6358 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 6359 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 6360 | `		if( rc == SXERR_ABORT ){` |
|       - | 6361 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6362 | `			return SXERR_ABORT;` |
|       - | 6363 | `		}` |
|     ! 0 | 6364 | `	}` |
|       - | 6365 | `	/* Swap token stream */` |
|      72 | 6366 | `	pTmp = pGen->pEnd;` |
|      72 | 6367 | `	pGen->pEnd = pEnd;` |
|      72 | 6368 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      72 | 6369 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      72 | 6370 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 6371 | `	/* Emit the done instruction */` |
|      72 | 6372 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      72 | 6373 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 6374 | `	/* Update token stream */` |
|      72 | 6375 | `	pGen->pIn  = pEnd;` |
|      72 | 6376 | `	pGen->pEnd = pTmp;` |
|      72 | 6377 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6378 | `		return SXERR_ABORT;` |
|       - | 6379 | `	}` |
|      72 | 6380 | `	return SXRET_OK;` |
|      37 | 6381 |  |
|       - | 6382 | `/*` |
|       - | 6383 | ` * Compile the smart switch statement.` |
|       - | 6384 | ` * According to the PHP language reference manual` |
|       - | 6385 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 6386 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 6387 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 6388 | ` *  This is exactly what the switch statement is for.` |
|       - | 6389 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 6390 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 6391 | ` *  of the outer loop, use continue 2.` |
|       - | 6392 | ` *  Note that switch/case does loose comparision.` |
|       - | 6393 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 6394 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 6395 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 6396 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 6397 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 6398 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 6399 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 6400 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 6401 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 6402 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 6403 | ` *  list for the next case.` |
|       - | 6404 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 6405 | ` *  or floating-point numbers and strings.` |
|       - | 6406 | ` */` |
|      20 | 6407 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 6408 |  |
|       - | 6409 | `	GenBlock *pSwitchBlock;` |
|       - | 6410 | `	SyToken *pTmp,*pEnd;` |
|       - | 6411 | `	ph7_switch *pSwitch;` |
|       - | 6412 | `	sxu32 nToken;` |
|       - | 6413 | `	sxu32 nLine;` |
|       - | 6414 | `	sxi32 rc;` |
|      22 | 6415 | `	nLine = pGen->pIn->nLine;` |
|       - | 6416 | `	/* Jump the 'switch' keyword */` |
|      22 | 6417 | `	pGen->pIn++;` |
|      22 | 6418 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 6419 | `		/* Syntax error */` |
|     ! 0 | 6420 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 6421 | `		if( rc == SXERR_ABORT ){` |
|       - | 6422 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6423 | `			return SXERR_ABORT;` |
|       - | 6424 | `		}` |
|     ! 0 | 6425 | `		goto Synchronize;` |
|       - | 6426 | `	}` |
|       - | 6427 | `	/* Jump the left parenthesis '(' */` |
|      22 | 6428 | `	pGen->pIn++;` |
|      22 | 6429 | `	pEnd = 0; /* cc warning */` |
|       - | 6430 | `	/* Create the loop block */` |
|      32 | 6431 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      10 | 6432 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      22 | 6433 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6434 | `		return SXERR_ABORT;` |
|       - | 6435 | `	}` |
|       - | 6436 | `	/* Delimit the condition */` |
|      22 | 6437 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      22 | 6438 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 6439 | `		/* Empty expression */` |
|     ! 0 | 6440 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 6441 | `		if( rc == SXERR_ABORT ){` |
|       - | 6442 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6443 | `			return SXERR_ABORT;` |
|       - | 6444 | `		}` |
|     ! 0 | 6445 | `	}` |
|       - | 6446 | `	/* Swap token streams */` |
|      22 | 6447 | `	pTmp = pGen->pEnd;` |
|      22 | 6448 | `	pGen->pEnd = pEnd;` |
|       - | 6449 | `	/* Compile the expression */` |
|      22 | 6450 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      22 | 6451 | `	if( rc == SXERR_ABORT ){` |
|       - | 6452 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 6453 | `		return SXERR_ABORT;` |
|       - | 6454 | `	}` |
|       - | 6455 | `	/* Update token stream */` |
|      22 | 6456 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 6457 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6458 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 6459 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6460 | `			return SXERR_ABORT;` |
|       - | 6461 | `		}` |
|     ! 0 | 6462 | `		pGen->pIn++;` |
|     ! 0 | 6463 | `	}` |
|      22 | 6464 | `	pGen->pIn  = &pEnd[1];` |
|      22 | 6465 | `	pGen->pEnd = pTmp;` |
|      22 | 6466 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      20 | 6467 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 6468 | `			pTmp = pGen->pIn;` |
|     ! 0 | 6469 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 6470 | `				pTmp--;` |
|     ! 0 | 6471 | `			}` |
|       - | 6472 | `			/* Unexpected token */` |
|     ! 0 | 6473 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 6474 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6475 | `				return SXERR_ABORT;` |
|       - | 6476 | `			}` |
|     ! 0 | 6477 | `			goto Synchronize;` |
|       - | 6478 | `	}` |
|       - | 6479 | `	/* Set the delimiter token */` |
|      22 | 6480 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 6481 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 6482 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 6483 | `	}else{` |
|      20 | 6484 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 6485 | `	}` |
|      22 | 6486 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 6487 | `	/* Create the switch blocks container */` |
|      22 | 6488 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      22 | 6489 | `	if( pSwitch == 0 ){` |
|       - | 6490 | `		/* Abort compilation */` |
|     ! 0 | 6491 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6492 | `		return SXERR_ABORT;` |
|       - | 6493 | `	}` |
|       - | 6494 | `	/* Zero the structure */` |
|      22 | 6495 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 6496 | `	/* Initialize fields */` |
|      22 | 6497 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 6498 | `	/* Emit the switch instruction */` |
|      22 | 6499 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 6500 | `	/* Compile case blocks */` |
|      76 | 6501 | `	for(;;){` |
|       - | 6502 | `		sxu32 nKwrd;` |
|      88 | 6503 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6504 | `			/* No more input to process */` |
|     ! 0 | 6505 | `			break;` |
|       - | 6506 | `		}` |
|      88 | 6507 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6508 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 6509 | `				/* Unexpected token */` |
|     ! 0 | 6510 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 6511 | `					&pGen->pIn->sData);` |
|     ! 0 | 6512 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6513 | `					return SXERR_ABORT;` |
|       - | 6514 | `				}` |
|       - | 6515 | `				/* FALL THROUGH */` |
|     ! 0 | 6516 | `			}` |
|       - | 6517 | `			/* Block compiled */` |
|     ! 0 | 6518 | `			break;` |
|       - | 6519 | `		}` |
|       - | 6520 | `		/* Extract the keyword */` |
|      88 | 6521 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      88 | 6522 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 6523 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 6524 | `				/* Unexpected token */` |
|     ! 0 | 6525 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 6526 | `					&pGen->pIn->sData);` |
|     ! 0 | 6527 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6528 | `					return SXERR_ABORT;` |
|       - | 6529 | `				}` |
|       - | 6530 | `				/* FALL THROUGH */` |
|     ! 0 | 6531 | `			}` |
|       - | 6532 | `			/* Block compiled */` |
|       3 | 6533 | `			break;` |
|       - | 6534 | `		}` |
|      86 | 6535 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 6536 | `			/*` |
|       - | 6537 | `			 * Accroding to the PHP language reference manual` |
|       - | 6538 | `			 *  A special case is the default case. This case matches anything` |
|       - | 6539 | `			 *  that wasn't matched by the other cases.` |
|       - | 6540 | `			 */` |
|      16 | 6541 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 6542 | `				/* Default case already compiled */` |
|     ! 0 | 6543 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 6544 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6545 | `					return SXERR_ABORT;` |
|       - | 6546 | `				}` |
|     ! 0 | 6547 | `			}` |
|      16 | 6548 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 6549 | `			/* Compile the default block */` |
|      16 | 6550 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      16 | 6551 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 6552 | `				return SXERR_ABORT;` |
|      16 | 6553 | `			}else if( rc == SXERR_EOF ){` |
|      14 | 6554 | `				break;` |
|       1 | 6555 | `			}` |
|      73 | 6556 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 6557 | `			ph7_case_expr sCase;` |
|       - | 6558 | `			/* Standard case block */` |
|      72 | 6559 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 6560 | `			/* initialize the structure */` |
|      72 | 6561 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 6562 | `			/* Compile the case expression */` |
|      72 | 6563 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      72 | 6564 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6565 | `				return SXERR_ABORT;` |
|       - | 6566 | `			}` |
|       - | 6567 | `			/* Compile the case block */` |
|      72 | 6568 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 6569 | `			/* Insert in the switch container */` |
|      72 | 6570 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      72 | 6571 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 6572 | `				return SXERR_ABORT;` |
|      72 | 6573 | `			}else if( rc == SXERR_EOF ){` |
|       7 | 6574 | `				break;` |
|       - | 6575 | `			}` |
|      34 | 6576 | `		}else{` |
|       - | 6577 | `			/* Unexpected token */` |
|     ! 0 | 6578 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 6579 | `				&pGen->pIn->sData);` |
|     ! 0 | 6580 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6581 | `				return SXERR_ABORT;` |
|       - | 6582 | `			}` |
|     ! 0 | 6583 | `			break;` |
|       - | 6584 | `		}` |
|       2 | 6585 | `	}` |
|       - | 6586 | `	/* Fix all jumps now the destination is resolved */` |
|      22 | 6587 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      22 | 6588 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6589 | `	/* Release the loop block */` |
|      22 | 6590 | `	GenStateLeaveBlock(pGen,0);` |
|      22 | 6591 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 6592 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      22 | 6593 | `		pGen->pIn++;` |
|      10 | 6594 | `	}` |
|       - | 6595 | `	/* Statement successfully compiled */` |
|      22 | 6596 | `	return SXRET_OK;` |
|     ! 0 | 6597 | `Synchronize:` |
|       - | 6598 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 6599 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 6600 | `		pGen->pIn++;` |
|     ! 0 | 6601 | `	}` |
|     ! 0 | 6602 | `	return SXRET_OK;` |
|      12 | 6603 |  |
|       - | 6604 | `/*` |
|       - | 6605 | ` * Generate bytecode for a given expression tree.` |
|       - | 6606 | ` * If something goes wrong while generating bytecode` |
|       - | 6607 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 6608 | ` * this function takes care of generating the appropriate` |
|       - | 6609 | ` * error message.` |
|       - | 6610 | ` */` |
| 2028890 | 6611 | `static sxi32 GenStateEmitExprCode(` |
|       - | 6612 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 6613 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 6614 | `	sxi32 iFlags /* Control flags */` |
|       - | 6615 | `	)` |
|       2 | 6616 |  |
|       - | 6617 | `	VmInstr *pInstr;` |
|       - | 6618 | `	sxu32 nJmpIdx;` |
| 2028892 | 6619 | `	sxi32 iP1 = 0;` |
| 2028892 | 6620 | `	sxu32 iP2 = 0;` |
| 2028892 | 6621 | `	void *p3  = 0;` |
|       - | 6622 | `	sxi32 iVmOp;` |
|       - | 6623 | `	sxi32 rc;` |
| 2028892 | 6624 | `	if( pNode->xCode ){` |
|       - | 6625 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 6626 | `		/* Compile node */` |
| 1244824 | 6627 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1244824 | 6628 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1244824 | 6629 | `		RE_SWAP_DELIMITER(pGen);` |
| 1244824 | 6630 | `		return rc;` |
|       - | 6631 | `	}` |
|  784070 | 6632 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 6633 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 6634 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 6635 | `		return SXERR_ABORT;` |
|       - | 6636 | `	}` |
|  784070 | 6637 | `	iVmOp = pNode->pOp->iVmOp;` |
|  784070 | 6638 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 6639 | `		sxu32 nJz,nJmp;` |
|       - | 6640 | `		/* Ternary operator require special handling */` |
|       - | 6641 | `		/* Phase#1: Compile the condition */` |
|    1748 | 6642 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1748 | 6643 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6644 | `			return rc;` |
|       - | 6645 | `		}` |
|    1748 | 6646 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1748 | 6647 | `		if( pNode->pLeft ){` |
|       - | 6648 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 6649 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1680 | 6650 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 6651 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1680 | 6652 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1680 | 6653 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6654 | `				return rc;` |
|       - | 6655 | `			}` |
|     841 | 6656 | `		}else{` |
|       - | 6657 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 6658 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 6659 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 6660 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 6661 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 6662 | `		}` |
|       - | 6663 | `		/* Phase#4: Emit the unconditional jump */` |
|    1748 | 6664 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 6665 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1748 | 6666 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1748 | 6667 | `		if( pInstr ){` |
|    1748 | 6668 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     873 | 6669 | `		}` |
|    1748 | 6670 | `		if( !pNode->pLeft ){` |
|       - | 6671 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 6672 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 6673 | `		}` |
|       - | 6674 | `		/* Phase#6: Compile the 'else' expression */` |
|    1748 | 6675 | `		if( pNode->pRight ){` |
|    1748 | 6676 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1748 | 6677 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6678 | `				return rc;` |
|       - | 6679 | `			}` |
|     873 | 6680 | `		}` |
|    1748 | 6681 | `		if( nJmp > 0 ){` |
|       - | 6682 | `			/* Phase#7: Fix the unconditional jump */` |
|    1748 | 6683 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1748 | 6684 | `			if( pInstr ){` |
|    1748 | 6685 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     873 | 6686 | `			}` |
|     873 | 6687 | `		}` |
|       - | 6688 | `		/* All done */` |
|    1748 | 6689 | `		return SXRET_OK;` |
|       - | 6690 | `	}` |
|       - | 6691 | `	/* Generate code for the left tree */` |
|  782324 | 6692 | `	if( pNode->pLeft ){` |
|  782306 | 6693 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 6694 | `			ph7_expr_node **apNode;` |
|       - | 6695 | `			sxi32 n;` |
|       - | 6696 | `			/* Recurse and generate bytecodes for function arguments */` |
|  232142 | 6697 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 6698 | `			/* Read-only load */` |
|  232142 | 6699 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  457392 | 6700 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  225252 | 6701 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  225252 | 6702 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6703 | `					return rc;` |
|       - | 6704 | `				}` |
|  112627 | 6705 | `			}` |
|       - | 6706 | `			/* Total number of given arguments */` |
|  232142 | 6707 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 6708 | `			/* Remove stale flags now */` |
|  232142 | 6709 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  116070 | 6710 | `		}` |
|  782306 | 6711 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  782306 | 6712 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6713 | `			return rc;` |
|       - | 6714 | `		}` |
|  782306 | 6715 | `		if( iVmOp == PH7_OP_CALL ){` |
|  232142 | 6716 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  232142 | 6717 | `			if( pInstr ){` |
|  232142 | 6718 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  231836 | 6719 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 6720 | `					sxu32 nQual;` |
|       - | 6721 | `					/* Prevent constant expansion */` |
|  231836 | 6722 | `					pInstr->iP1 = 0;` |
|       - | 6723 | `					/* Namespace-qualify the function name for CALL */` |
|  231836 | 6724 | `					nQual = GenStateNsQualifyName(pGen,nOrig);` |
|  231836 | 6725 | `					pInstr->iP2 = (sxi32)nQual;` |
|  231836 | 6726 | `					if( nQual != nOrig ){` |
|       - | 6727 | `						/* Name was compiler-qualified: flag CALL for host-function global fallback.` |
|       - | 6728 | `						 * p3 = (void*)1 tells the VM it's safe to strip the NS prefix` |
|       - | 6729 | `						 * and try the short name in hHostFunction. */` |
|      49 | 6730 | `						p3 = (void *)1;` |
|      26 | 6731 | `					}` |
|  116225 | 6732 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 6733 | `					/* Method call,flag that */` |
|     296 | 6734 | `					pInstr->iP2 = 1;` |
|     147 | 6735 | `				}` |
|  116072 | 6736 | `			}` |
|  666236 | 6737 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 6738 | `			ph7_expr_node **apNode;` |
|       - | 6739 | `			sxi32 n;` |
|       - | 6740 | `			/* Recurse and generate bytecodes for array index */` |
|   62336 | 6741 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  112452 | 6742 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   50118 | 6743 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   50118 | 6744 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6745 | `					return rc;` |
|       - | 6746 | `				}` |
|   25060 | 6747 | `			}` |
|   62336 | 6748 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   50118 | 6749 | `				iP1 = 1; /* Node have an index associated with it */` |
|   25058 | 6750 | `			}` |
|   62336 | 6751 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 6752 | `				/* Create an empty entry when the desired index is not found */` |
|   24622 | 6753 | `				iP2 = 1;` |
|   12312 | 6754 | `			}` |
|  518999 | 6755 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 6756 | `			/* POP the left node */` |
|      32 | 6757 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 6758 | `		}` |
|  391152 | 6759 | `	}` |
|  782324 | 6760 | `	rc = SXRET_OK;` |
|  782324 | 6761 | `	nJmpIdx = 0;` |
|       - | 6762 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 6763 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 6764 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  782324 | 6765 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|      98 | 6766 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      98 | 6767 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      98 | 6768 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      98 | 6769 | `			int isSpecial = 0;` |
|      98 | 6770 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|      58 | 6771 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|      58 | 6772 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|      62 | 6773 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      52 | 6774 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      26 | 6775 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      38 | 6776 | `					isSpecial = 1;` |
|      18 | 6777 | `				}` |
|      38 | 6778 | `			}` |
|     118 | 6779 | `			pInstr->iP1 = 0;` |
|     118 | 6780 | `			if( !isSpecial ){` |
|      42 | 6781 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2);` |
|      20 | 6782 | `			}` |
|      38 | 6783 | `		}` |
|      72 | 6784 | `	}` |
|       - | 6785 | `	/* Generate code for the right tree */` |
|  782308 | 6786 | `	if( pNode->pRight ){` |
|  432606 | 6787 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 6788 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    7682 | 6789 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  428766 | 6790 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 6791 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2580 | 6792 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  423637 | 6793 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  189178 | 6794 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|   94588 | 6795 | `		}` |
|  432606 | 6796 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  432606 | 6797 | `		if( iVmOp == PH7_OP_STORE ){` |
|  186628 | 6798 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  186628 | 6799 | `			if( pInstr ){` |
|  186628 | 6800 | `				if( pInstr->iOp == PH7_OP_LOAD_LIST ){` |
|       - | 6801 | `					/* Hide the STORE instruction */` |
|      26 | 6802 | `					iVmOp = 0;` |
|  186616 | 6803 | `				}else if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 6804 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   41446 | 6805 | `					iP2 = 1;` |
|   20724 | 6806 | `				}else{` |
|  145160 | 6807 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 6808 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   24620 | 6809 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   24620 | 6810 | `						iP1 = pInstr->iP1;` |
|   12311 | 6811 | `					}else{` |
|  120542 | 6812 | `						p3 = pInstr->p3;` |
|       - | 6813 | `					}` |
|       - | 6814 | `					/* POP the last dynamic load instruction */` |
|  145160 | 6815 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 6816 | `				}` |
|   93315 | 6817 | `			}` |
|  339293 | 6818 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      44 | 6819 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      44 | 6820 | `			if( pInstr ){` |
|      44 | 6821 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 6822 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 6823 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 6824 | `					 */` |
|      15 | 6825 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 6826 | `					iP1 = pInstr->iP1;` |
|      15 | 6827 | `					iP2 = pInstr->iP2;` |
|      15 | 6828 | `					p3  = pInstr->p3;` |
|       8 | 6829 | `				}else{` |
|      30 | 6830 | `					p3 = pInstr->p3;` |
|       - | 6831 | `				}` |
|      21 | 6832 | `			}` |
|      21 | 6833 | `		}` |
|  216302 | 6834 | `	}` |
|  782308 | 6835 | `	if( iVmOp > 0 ){` |
|  782254 | 6836 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    9948 | 6837 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 6838 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    7310 | 6839 | `				iP1 = 1;` |
|    3656 | 6840 | `			}` |
|  777281 | 6841 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 6842 | `			/* Namespace-qualify the class name for NEW */ {` |
|   12486 | 6843 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   12486 | 6844 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   12474 | 6845 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    6236 | 6846 | `				}` |
|   12486 | 6847 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 6848 | `					/* Prevent constant expansion for class name */` |
|   12484 | 6849 | `					pPeek->iP1 = 0;` |
|   12484 | 6850 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pPeek->iP2);` |
|    6241 | 6851 | `				}` |
|       - | 6852 | `			}` |
|   12486 | 6853 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   12486 | 6854 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 6855 | `				VmInstr *pPrev;` |
|   12474 | 6856 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   12474 | 6857 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 6858 | `					/* Pop the call instruction */` |
|   12474 | 6859 | `					iP1 = pInstr->iP1;` |
|   12474 | 6860 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    6236 | 6861 | `				}` |
|    6238 | 6862 | `			}` |
|  766066 | 6863 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 6864 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 6865 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 6866 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 6867 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 6868 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 6869 | `				int isSpecialIs = 0;` |
|      50 | 6870 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 6871 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 6872 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 6873 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 6874 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 6875 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 6876 | `						isSpecialIs = 1;` |
|       5 | 6877 | `					}` |
|      23 | 6878 | `				}` |
|      52 | 6879 | `				pInstr->iP1 = 0;` |
|      52 | 6880 | `				if( !isSpecialIs ){` |
|      38 | 6881 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2);` |
|      18 | 6882 | `				}` |
|      25 | 6883 | `			}` |
|  759803 | 6884 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 6885 | `			/* Prevent constant expansion for member/property names.` |
|       - | 6886 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 6887 | `			 * should not trigger constant lookup. */` |
|   93086 | 6888 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   93086 | 6889 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   93070 | 6890 | `				pInstr->iP1 = 0;` |
|   46534 | 6891 | `			}` |
|   93086 | 6892 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 6893 | `				/* Static member access,remember that */` |
|      82 | 6894 | `				iP1 = 1;` |
|      82 | 6895 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      82 | 6896 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      10 | 6897 | `					p3 = pInstr->p3;` |
|      10 | 6898 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       4 | 6899 | `				}` |
|      40 | 6900 | `			}` |
|   46542 | 6901 | `		}` |
|       - | 6902 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  782252 | 6903 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  782252 | 6904 | `		if( nJmpIdx > 0 ){` |
|       - | 6905 | `			/* Fix short-circuited jumps now the destination is resolved */` |
|   10260 | 6906 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   10260 | 6907 | `			if( pInstr ){` |
|   10260 | 6908 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5129 | 6909 | `			}` |
|    5129 | 6910 | `		}` |
|  391125 | 6911 | `	}` |
|  782306 | 6912 | `	return rc;` |
| 1014438 | 6913 |  |
|       - | 6914 | `/*` |
|       - | 6915 | ` * Compile a PHP expression.` |
|       - | 6916 | ` * According to the PHP language reference manual:` |
|       - | 6917 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 6918 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 6919 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 6920 | ` *  is "anything that has a value".` |
|       - | 6921 | ` * If something goes wrong while compiling the expression,this` |
|       - | 6922 | ` * function takes care of generating the appropriate error` |
|       - | 6923 | ` * message.` |
|       - | 6924 | ` */` |
|  533594 | 6925 | `static sxi32 PH7_CompileExpr(` |
|       - | 6926 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 6927 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 6928 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 6929 | `	)` |
|       2 | 6930 |  |
|       - | 6931 | `	ph7_expr_node *pRoot;` |
|       - | 6932 | `	SySet sExprNode;` |
|       - | 6933 | `	SyToken *pEnd;` |
|       - | 6934 | `	sxi32 nExpr;` |
|       - | 6935 | `	sxi32 iNest;` |
|       - | 6936 | `	sxi32 rc;` |
|       - | 6937 | `	/* Initialize worker variables */` |
|  533596 | 6938 | `	nExpr = 0;` |
|  533596 | 6939 | `	pRoot = 0;` |
|  533596 | 6940 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  533596 | 6941 | `	SySetAlloc(&sExprNode,0x10);` |
|  533596 | 6942 | `	rc = SXRET_OK;` |
|       - | 6943 | `	/* Delimit the expression */` |
|  533596 | 6944 | `	pEnd = pGen->pIn;` |
|  533596 | 6945 | `	iNest = 0;` |
| 3655048 | 6946 | `	while( pEnd < pGen->pEnd ){` |
| 3462166 | 6947 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 6948 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     180 | 6949 | `			iNest++;` |
| 3462077 | 6950 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     188 | 6951 | `			iNest--;` |
| 3461895 | 6952 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  340850 | 6953 | `			if( iNest <= 0 ){` |
|  340714 | 6954 | `				break;` |
|       - | 6955 | `			}` |
|      68 | 6956 | `		}` |
| 3121454 | 6957 | `		pEnd++;` |
|       2 | 6958 | `	}` |
|  533596 | 6959 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    9900 | 6960 | `		SyToken *pEnd2 = pGen->pIn;` |
|    9900 | 6961 | `		iNest = 0;` |
|       - | 6962 | `		/* Stop at the first comma */` |
|   19822 | 6963 | `		while( pEnd2 < pEnd ){` |
|    9924 | 6964 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       6 | 6965 | `				iNest++;` |
|    9922 | 6966 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       6 | 6967 | `				iNest--;` |
|    9918 | 6968 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 6969 | `				if( iNest <= 0 ){` |
|     ! 0 | 6970 | `					break;` |
|       - | 6971 | `				}` |
|       2 | 6972 | `			}` |
|    9924 | 6973 | `			pEnd2++;` |
|       2 | 6974 | `		}` |
|    9900 | 6975 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 6976 | `			pEnd = pEnd2;` |
|     ! 0 | 6977 | `		}` |
|    4949 | 6978 | `	}` |
|  533596 | 6979 | `	if( pEnd > pGen->pIn ){` |
|  533586 | 6980 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 6981 | `		/* Swap delimiter */` |
|  533586 | 6982 | `		pGen->pEnd = pEnd;` |
|       - | 6983 | `		/* Try to get an expression tree */` |
|  533586 | 6984 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  533586 | 6985 | `		if( rc == SXRET_OK && pRoot ){` |
|  533430 | 6986 | `			rc = SXRET_OK;` |
|  533430 | 6987 | `			if( xTreeValidator ){` |
|       - | 6988 | `				/* Call the upper layer validator callback */` |
|   12618 | 6989 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    6308 | 6990 | `			}` |
|  533430 | 6991 | `			if( rc != SXERR_ABORT ){` |
|       - | 6992 | `				/* Generate code for the given tree */` |
|  533430 | 6993 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  266714 | 6994 | `			}` |
|  533430 | 6995 | `			nExpr = 1;` |
|  266714 | 6996 | `		}` |
|       - | 6997 | `		/* Release the whole tree */` |
|  533586 | 6998 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 6999 | `		/* Synchronize token stream */` |
|  533586 | 7000 | `		pGen->pEnd = pTmp;` |
|  533586 | 7001 | `		pGen->pIn  = pEnd;` |
|  533586 | 7002 | `		if( rc == SXERR_ABORT ){` |
|       3 | 7003 | `			SySetRelease(&sExprNode);` |
|       3 | 7004 | `			return SXERR_ABORT;` |
|       - | 7005 | `		}` |
|  266791 | 7006 | `	}` |
|  533594 | 7007 | `	SySetRelease(&sExprNode);` |
|  533594 | 7008 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  266799 | 7009 |  |
|       - | 7010 | `/*` |
|       - | 7011 | ` * Return a pointer to the node construct handler associated` |
|       - | 7012 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 7013 | ` */` |
|  145920 | 7014 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 7015 |  |
|  145922 | 7016 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 7017 | `		/* Numeric literal: Either real or integer */` |
|   79904 | 7018 | `		return PH7_CompileNumLiteral;` |
|   66020 | 7019 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 7020 | `		/* Double quoted string */` |
|   13474 | 7021 | `		return PH7_CompileString;` |
|   52548 | 7022 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 7023 | `		/* Single quoted string */` |
|   52488 | 7024 | `		return PH7_CompileSimpleString;` |
|      62 | 7025 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 7026 | `		/* Heredoc */` |
|      28 | 7027 | `		return PH7_CompileHereDoc;` |
|      36 | 7028 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 7029 | `		/* Nowdoc */` |
|      29 | 7030 | `		return PH7_CompileNowDoc;` |
|       7 | 7031 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 7032 | `		/* Backtick quoted string */` |
|       5 | 7033 | `		return PH7_CompileBacktic;` |
|       - | 7034 | `	}` |
|       3 | 7035 | `	return 0;` |
|   72962 | 7036 |  |
|       - | 7037 | `/*` |
|       - | 7038 | ` * PHP Language construct table.` |
|       - | 7039 | ` */` |
|       - | 7040 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 7041 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 7042 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 7043 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 7044 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 7045 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 7046 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 7047 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 7048 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 7049 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 7050 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 7051 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 7052 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 7053 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 7054 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 7055 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 7056 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 7057 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 7058 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 7059 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 7060 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 7061 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 7062 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 7063 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  }   /* declare statement */` |
|       - | 7064 | `};` |
|       - | 7065 | `/*` |
|       - | 7066 | ` * Return a pointer to the statement handler routine associated` |
|       - | 7067 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 7068 | ` */` |
|  306832 | 7069 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 7070 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 7071 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 7072 | `	)` |
|       2 | 7073 |  |
|  306834 | 7074 | `	sxu32 n = 0;` |
| 1162287 | 7075 | `	for(;;){` |
| 2324576 | 7076 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   32114 | 7077 | `			break;` |
|       - | 7078 | `		}` |
| 2292464 | 7079 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  274722 | 7080 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 7081 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 7082 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 7083 | `					/* 'static' (class context),return null */` |
|     ! 0 | 7084 | `					return 0;` |
|       - | 7085 | `				}` |
|     ! 0 | 7086 | `			}` |
|       - | 7087 | `			/* Return a pointer to the handler.` |
|       - | 7088 | `			*/` |
|  274722 | 7089 | `			return aLangConstruct[n].xConstruct;` |
|       - | 7090 | `		}` |
| 2017744 | 7091 | `		n++;` |
|       2 | 7092 | `	}` |
|   32114 | 7093 | `	if( pLookahed ){` |
|   32114 | 7094 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    7316 | 7095 | `			return PH7_CompileClassInterface;` |
|   24800 | 7096 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   22230 | 7097 | `			return PH7_CompileClass;` |
|    2572 | 7098 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      43 | 7099 | `			return PH7_CompileTrait;` |
|    2528 | 7100 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       7 | 7101 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       6 | 7102 | `				return PH7_CompileAbstractClass;` |
|    2524 | 7103 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 7104 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 7105 | `				return PH7_CompileFinalClass;` |
|       - | 7106 | `		}` |
|    1261 | 7107 | `	}` |
|       - | 7108 | `	/* Not a language construct */` |
|    2524 | 7109 | `	return 0;` |
|  153418 | 7110 |  |
|       - | 7111 | `/*` |
|       - | 7112 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 7113 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 7114 | ` */` |
|    2522 | 7115 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 7116 |  |
|       - | 7117 | `	int rc;` |
|    2524 | 7118 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|    2524 | 7119 | `	if( rc == FALSE ){` |
|      14 | 7120 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|       - | 7121 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 7122 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 7123 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 7124 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 7125 | `			*/` |
|       - | 7126 | `			){` |
|       6 | 7127 | `				rc = TRUE;` |
|       2 | 7128 | `		}` |
|       6 | 7129 | `	}` |
|    2524 | 7130 | `	return rc;` |
|       2 | 7131 |  |
|       - | 7132 | `/*` |
|       - | 7133 | ` * Compile a PHP chunk.` |
|       - | 7134 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 7135 | ` * takes care of generating the appropriate error message.` |
|       - | 7136 | ` */` |
|  435490 | 7137 | `static sxi32 GenStateCompileChunk(` |
|       - | 7138 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7139 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 7140 | `	)` |
|       2 | 7141 |  |
|       - | 7142 | `	ProcLangConstruct xCons;` |
|       - | 7143 | `	sxi32 rc;` |
|  435492 | 7144 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  258071 | 7145 | `	for(;;){` |
|  516144 | 7146 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7147 | `			/* No more input to process */` |
|   10606 | 7148 | `			break;` |
|       - | 7149 | `		}` |
|  505540 | 7150 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7151 | `			/* Compile block */` |
|      12 | 7152 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 7153 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7154 | `				break;` |
|       - | 7155 | `			}` |
|       7 | 7156 | `		}else{` |
|  505530 | 7157 | `			xCons = 0;` |
|  505530 | 7158 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  306834 | 7159 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 7160 | `				/* Try to extract a language construct handler */` |
|  306834 | 7161 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  306834 | 7162 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 7163 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7164 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 7165 | `						&pGen->pIn->sData);` |
|       9 | 7166 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7167 | `						break;` |
|       - | 7168 | `					}` |
|       - | 7169 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 7170 | `					 * this erroneous statement.` |
|       - | 7171 | `					 */` |
|       9 | 7172 | `					xCons = PH7_ErrorRecover;` |
|       4 | 7173 | `				}` |
|  352114 | 7174 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   28666 | 7175 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 7176 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 7177 | `				xCons = PH7_CompileLabel;` |
|      56 | 7178 | `			}` |
|  505530 | 7179 | `			if( xCons == 0 ){` |
|       - | 7180 | `				/* Assume an expression an try to compile it */` |
|  201100 | 7181 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  201100 | 7182 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 7183 | `					/* Pop l-value */` |
|  200974 | 7184 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  100486 | 7185 | `				}` |
|  100551 | 7186 | `			}else{` |
|       - | 7187 | `				/* Go compile the sucker */` |
|  304432 | 7188 | `				rc = xCons(&(*pGen));` |
|       - | 7189 | `			}` |
|  505530 | 7190 | `			if( rc == SXERR_ABORT ){` |
|       - | 7191 | `				/* Request to abort compilation */` |
|       3 | 7192 | `				break;` |
|       - | 7193 | `			}` |
|       - | 7194 | `		}` |
|       - | 7195 | `		/* Ignore trailing semi-colons ';' */` |
|  829114 | 7196 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  323578 | 7197 | `			pGen->pIn++;` |
|       2 | 7198 | `		}` |
|  505538 | 7199 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 7200 | `			/* Compile a single statement and return */` |
|  424886 | 7201 | `			break;` |
|       - | 7202 | `		}` |
|       - | 7203 | `		/* LOOP ONE */` |
|       - | 7204 | `		/* LOOP TWO */` |
|       - | 7205 | `		/* LOOP THREE */` |
|       - | 7206 | `		/* LOOP FOUR */` |
|       2 | 7207 | `	}` |
|       - | 7208 | `	/* Return compilation status */` |
|  435492 | 7209 | `	return rc;` |
|       2 | 7210 |  |
|       - | 7211 | `/*` |
|       - | 7212 | ` * Compile a Raw PHP chunk.` |
|       - | 7213 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 7214 | ` * takes care of generating the appropriate error message.` |
|       - | 7215 | ` */` |
|   10608 | 7216 | `static sxi32 PH7_CompilePHP(` |
|       - | 7217 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7218 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 7219 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 7220 | `	)` |
|       2 | 7221 |  |
|   10610 | 7222 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 7223 | `	sxi32 rc;` |
|       - | 7224 | `	/* Reset the token set */` |
|   10610 | 7225 | `	SySetReset(&(*pTokenSet));` |
|       - | 7226 | `	/* Mark as the default token set */` |
|   10610 | 7227 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 7228 | `	/* Advance the stream cursor */` |
|   10610 | 7229 | `	pGen->pRawIn++;` |
|       - | 7230 | `	/* Tokenize the PHP chunk first */` |
|   10610 | 7231 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 7232 | `	/* Point to the head and tail of the token stream. */` |
|   10610 | 7233 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   10610 | 7234 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   10610 | 7235 | `	if( is_expr ){` |
|     ! 0 | 7236 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 7237 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 7238 | `			/* A simple expression,compile it */` |
|     ! 0 | 7239 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 7240 | `		}` |
|       - | 7241 | `		/* Emit the DONE instruction */` |
|     ! 0 | 7242 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 7243 | `		return SXRET_OK;` |
|       - | 7244 | `	}` |
|   10610 | 7245 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 7246 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 7247 | `		/*` |
|       - | 7248 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 7249 | `		 * According to the PHP reference manual:` |
|       - | 7250 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 7251 | `		 *  immediately follow` |
|       - | 7252 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 7253 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 7254 | `		 * Symisc extension:` |
|       - | 7255 | `		 *   This short syntax works with all PHP opening` |
|       - | 7256 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 7257 | `		 *   only short tag.` |
|       - | 7258 | `		 */` |
|       - | 7259 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 7260 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 7261 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 7262 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 7263 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 7264 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 7265 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 7266 | `		}` |
|       3 | 7267 | `		return SXRET_OK;` |
|       - | 7268 | `	}` |
|       - | 7269 | `	/* Compile the PHP chunk */` |
|   10608 | 7270 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 7271 | `	/* Fix exceptions jumps */` |
|   10608 | 7272 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7273 | `	/* Fix gotos now, the jump destination is resolved */` |
|   10608 | 7274 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 7275 | `		rc = SXERR_ABORT;` |
|       1 | 7276 | `	}` |
|       - | 7277 | `	/* Reset container */` |
|   10608 | 7278 | `	SySetReset(&pGen->aGoto);` |
|   10608 | 7279 | `	SySetReset(&pGen->aLabel);` |
|       - | 7280 | `	/* Compilation result */` |
|   10608 | 7281 | `	return rc;` |
|    5306 | 7282 |  |
|       - | 7283 | `/*` |
|       - | 7284 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 7285 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 7286 | ` * This is the only compile interface exported from this file.` |
|       - | 7287 | ` */` |
|   12416 | 7288 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 7289 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 7290 | `	SyString *pScript,  /* Script to compile */` |
|       - | 7291 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 7292 | `	)` |
|       2 | 7293 |  |
|       - | 7294 | `	SySet aPhpToken,aRawToken;` |
|       - | 7295 | `	ph7_gen_state *pCodeGen;` |
|       - | 7296 | `	ph7_value *pRawObj;` |
|       - | 7297 | `	sxu32 nObjIdx;` |
|       - | 7298 | `	sxi32 nRawObj;` |
|       - | 7299 | `	int is_expr;` |
|       - | 7300 | `	sxi32 rc;` |
|   12418 | 7301 | `	if( pScript->nByte < 1 ){` |
|       - | 7302 | `		/* Nothing to compile */` |
|     ! 0 | 7303 | `		return PH7_OK;` |
|       - | 7304 | `	}` |
|       - | 7305 | `	/* Initialize the tokens containers */` |
|   12418 | 7306 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   12418 | 7307 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   12418 | 7308 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   12418 | 7309 | `	is_expr = 0;` |
|   12418 | 7310 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 7311 | `		SyToken sTmp;` |
|       - | 7312 | `		/* PHP only: -*/` |
|    2458 | 7313 | `		sTmp.nLine = 1;` |
|    2458 | 7314 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2458 | 7315 | `		sTmp.pUserData = 0;` |
|    2458 | 7316 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2458 | 7317 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2458 | 7318 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 7319 | `			/* A simple PHP expression */` |
|     ! 0 | 7320 | `			is_expr = 1;` |
|     ! 0 | 7321 | `		}` |
|    1230 | 7322 | `	}else{` |
|       - | 7323 | `		/* Tokenize raw text */` |
|    9962 | 7324 | `		SySetAlloc(&aRawToken,32);` |
|    9962 | 7325 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 7326 | `	}` |
|   12418 | 7327 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 7328 | `	/* Process high-level tokens */` |
|   12418 | 7329 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   12418 | 7330 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   12418 | 7331 | `	rc = PH7_OK;` |
|   12418 | 7332 | `	if( is_expr ){` |
|       - | 7333 | `		/* Compile the expression */` |
|     ! 0 | 7334 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 7335 | `		goto cleanup;` |
|       - | 7336 | `	}` |
|   12418 | 7337 | `	nObjIdx = 0;` |
|       - | 7338 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 7339 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 7340 | `	 * preventing namespace bleeding across include()d files. */` |
|   12418 | 7341 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 7342 | `	/* Start the compilation process */` |
|   11192 | 7343 | `	for(;;){` |
|   32990 | 7344 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   12414 | 7345 | `			break; /* No more tokens to process */` |
|       - | 7346 | `		}` |
|   20578 | 7347 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 7348 | `			/* Compile the PHP chunk */` |
|   10610 | 7349 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   10610 | 7350 | `			if( rc == SXERR_ABORT ){` |
|       5 | 7351 | `				break;` |
|       - | 7352 | `			}` |
|   10606 | 7353 | `			continue;` |
|       - | 7354 | `		}` |
|       - | 7355 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    9970 | 7356 | `		nRawObj = 0;` |
|   19938 | 7357 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 7358 | `			/* Consume the raw chunk without any processing */` |
|    9970 | 7359 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    9970 | 7360 | `			if( pRawObj == 0 ){` |
|     ! 0 | 7361 | `				rc = SXERR_MEM;` |
|     ! 0 | 7362 | `				break;` |
|       - | 7363 | `			}` |
|       - | 7364 | `			/* Mark as constant and emit the load constant instruction */` |
|    9970 | 7365 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    9970 | 7366 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    9970 | 7367 | `			++nRawObj;` |
|    9970 | 7368 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 7369 | `		}` |
|    9970 | 7370 | `		if( nRawObj > 0 ){` |
|       - | 7371 | `			/* Emit the consume instruction */` |
|    9970 | 7372 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    4984 | 7373 | `		}` |
|    6210 | 7374 | `	}` |
|    6208 | 7375 | `cleanup:` |
|   12418 | 7376 | `	SySetRelease(&aRawToken);` |
|   12418 | 7377 | `	SySetRelease(&aPhpToken);` |
|   12418 | 7378 | `	return rc;` |
|    6210 | 7379 |  |
|       - | 7380 | `/*` |
|       - | 7381 | ` * Utility routines.Initialize the code generator.` |
|       - | 7382 | ` */` |
|    2434 | 7383 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 7384 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 7385 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 7386 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 7387 | `	)` |
|       2 | 7388 |  |
|    2436 | 7389 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 7390 | `	/* Zero the structure */` |
|    2436 | 7391 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 7392 | `	/* Initial state */` |
|    2436 | 7393 | `	pGen->pVm  = &(*pVm);` |
|    2436 | 7394 | `	pGen->xErr = xErr;` |
|    2436 | 7395 | `	pGen->pErrData = pErrData;` |
|    2436 | 7396 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2436 | 7397 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2436 | 7398 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2436 | 7399 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 7400 | `	/* Error log buffer */` |
|    2436 | 7401 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 7402 | `	/* General purpose working buffer */` |
|    2436 | 7403 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 7404 | `	/* Namespace state */` |
|    2436 | 7405 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2436 | 7406 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 7407 | `	/* Create the global scope */` |
|    2436 | 7408 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 7409 | `	/* Point to the global scope */` |
|    2436 | 7410 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2436 | 7411 | `	return SXRET_OK;` |
|       2 | 7412 |  |
|       - | 7413 | `/*` |
|       - | 7414 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 7415 | ` */` |
|   14616 | 7416 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 7417 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 7418 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 7419 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 7420 | `	)` |
|       2 | 7421 |  |
|   14618 | 7422 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 7423 | `	GenBlock *pBlock,*pParent;` |
|       - | 7424 | `	/* Reset state */` |
|   14618 | 7425 | `	SySetReset(&pGen->aLabel);` |
|   14618 | 7426 | `	SySetReset(&pGen->aGoto);` |
|   14618 | 7427 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   14618 | 7428 | `	SyBlobRelease(&pGen->sWorker);` |
|   14618 | 7429 | `	SyBlobRelease(&pGen->sNamespace);` |
|   14618 | 7430 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   14618 | 7431 | `	SyHashRelease(&pGen->hUseImports);` |
|   14618 | 7432 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 7433 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 7434 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 7435 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 7436 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 7437 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 7438 | `	 * number of unique names, which is acceptable. */` |
|       - | 7439 | `	/* Point to the global scope */` |
|   14618 | 7440 | `	pBlock = pGen->pCurrent;` |
|   14618 | 7441 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 7442 | `		pParent = pBlock->pParent;` |
|     ! 0 | 7443 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 7444 | `		pBlock = pParent;` |
|     ! 0 | 7445 | `	}` |
|   14618 | 7446 | `	pGen->xErr = xErr;` |
|   14618 | 7447 | `	pGen->pErrData = pErrData;` |
|   14618 | 7448 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   14618 | 7449 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   14618 | 7450 | `	pGen->pIn = pGen->pEnd = 0;` |
|   14618 | 7451 | `	pGen->nErr = 0;` |
|   14618 | 7452 | `	return SXRET_OK;` |
|       2 | 7453 |  |
|       - | 7454 | `/*` |
|       - | 7455 | ` * Generate a compile-time error message.` |
|       - | 7456 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 7457 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 7458 | ` * abort compilation immediately.` |
|       - | 7459 | ` */` |
|     430 | 7460 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 7461 |  |
|     432 | 7462 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     432 | 7463 | `	const char *zErr = "Error";` |
|       - | 7464 | `	SyString *pFile;` |
|       - | 7465 | `	va_list ap;` |
|       - | 7466 | `	sxi32 rc;` |
|       - | 7467 | `	/* Reset the working buffer */` |
|     432 | 7468 | `	SyBlobReset(pWorker);` |
|       - | 7469 | `	/* Peek the processed file path if available */` |
|     432 | 7470 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     432 | 7471 | `	if( nErrType == E_ERROR ){` |
|       - | 7472 | `		/* Increment the error counter */` |
|     388 | 7473 | `		pGen->nErr++;` |
|     388 | 7474 | `		if( pGen->nErr > 15 ){` |
|       - | 7475 | `			/* Error count limit reached */` |
|       5 | 7476 | `			if( pGen->xErr ){` |
|       5 | 7477 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 7478 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 7479 | `				if( pFile ){` |
|       5 | 7480 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 7481 | `				}` |
|       5 | 7482 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 7483 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 7484 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 7485 | `				}` |
|       2 | 7486 | `			}` |
|       - | 7487 | `			/* Abort immediately */` |
|       5 | 7488 | `			return SXERR_ABORT;` |
|       - | 7489 | `		}` |
|     191 | 7490 | `	}` |
|     428 | 7491 | `	if( pGen->xErr == 0 ){` |
|       - | 7492 | `		/* No available error consumer,return immediately */` |
|       3 | 7493 | `		return SXRET_OK;` |
|       - | 7494 | `	}` |
|     425 | 7495 | `	switch(nErrType){` |
|     381 | 7496 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      31 | 7497 | `	case E_WARNING: zErr = "Warning";     break;` |
|       7 | 7498 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 7499 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 7500 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 7501 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 7502 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 7503 | `	default:` |
|     ! 0 | 7504 | `		break;` |
|       - | 7505 | `	}` |
|     425 | 7506 | `	rc = SXRET_OK;` |
|       - | 7507 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     425 | 7508 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     425 | 7509 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     425 | 7510 | `	va_start(ap,zFormat);` |
|     425 | 7511 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     425 | 7512 | `	va_end(ap);` |
|     425 | 7513 | `	if( pFile ){` |
|     425 | 7514 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     212 | 7515 | `	}` |
|       - | 7516 | `	/* Append a new line */` |
|     425 | 7517 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     425 | 7518 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 7519 | `		/* Consume the generated error message */` |
|     425 | 7520 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     212 | 7521 | `	}` |
|     425 | 7522 | `	return rc;` |
|     217 | 7523 |  |
|       - | 7524 |  |
