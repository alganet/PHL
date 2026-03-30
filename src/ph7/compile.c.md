# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3218/4293 lines (74.96%)

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
|    2700 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    2702 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    7582 |  131 | `	for(;;){` |
|   15166 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2590 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2590 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2568 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      11 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   12600 |  140 | `		pBlock = pBlock->pParent;` |
|   12600 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1352 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  431338 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  431340 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  431340 |  162 | `	pBlock->pUserData   = pUserData;` |
|  431340 |  163 | `	pBlock->pGen        = pGen;` |
|  431340 |  164 | `	pBlock->iFlags      = iType;` |
|  431340 |  165 | `	pBlock->pParent     = 0;` |
|  431340 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  431340 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  431340 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  428878 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  428880 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  428880 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  428880 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  428880 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  428880 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  428880 |  200 | `	pGen->pCurrent = pBlock;` |
|  428880 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  206010 |  203 | `		*ppBlock = pBlock;` |
|  103004 |  204 | `	}` |
|  428880 |  205 | `	return SXRET_OK;` |
|  214441 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  428870 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  428872 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  428872 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  428872 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  428870 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  428872 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  428872 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  428872 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  428872 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  428870 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  428872 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  428872 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  428872 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  428872 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  428872 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  428872 |  244 | `	return SXRET_OK;` |
|  214437 |  245 |  |
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
|  159406 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  159408 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  159408 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  159408 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  159408 |  265 | `	return rc;` |
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
|  325390 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  325392 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  636178 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  310788 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  121082 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  189708 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   30304 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  159406 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  159406 |  298 | `		if( pInstr ){` |
|  159406 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  159406 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  159406 |  302 | `			aFix[n].nJumpType = -1;` |
|   79702 |  303 | `		}` |
|   79704 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  325392 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|   95130 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|   95132 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|   95278 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|   95130 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|   95262 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|   95130 |  358 | `	return SXRET_OK;` |
|   47567 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  412592 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  412594 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  412594 |  367 | `	if( pEntry == 0 ){` |
|  180748 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  231848 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  231848 |  371 | `	return SXRET_OK;` |
|  206298 |  372 |  |
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
|  180746 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  180748 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  180748 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   90373 |  387 | `	}` |
|  180748 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   80254 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   80256 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   80256 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   80256 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   80256 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   80256 |  408 | `	return pObj;` |
|   40129 |  409 |  |
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
|   80654 |  431 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  432 |  |
|   80656 |  433 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   80656 |  434 | `	sxu32 nIdx = 0;` |
|   80656 |  435 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  436 | `		ph7_value *pObj;` |
|       - |  437 | `		sxi64 iValue;` |
|   80256 |  438 | `		iValue = PH7_TokenValueToInt64(&pToken->sData);` |
|   80256 |  439 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   80256 |  440 | `		if( pObj == 0 ){` |
|     ! 0 |  441 | `			SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  442 | `			return SXERR_ABORT;` |
|       - |  443 | `		}` |
|   80256 |  444 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   40129 |  445 | `	}else{` |
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
|   80656 |  458 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  459 | `	/* Node successfully compiled */` |
|   80656 |  460 | `	return SXRET_OK;` |
|   40329 |  461 |  |
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
|   52972 |  473 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  474 |  |
|   52974 |  475 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  476 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  477 | `	ph7_value *pObj;` |
|       - |  478 | `	sxu32 nIdx;` |
|   52974 |  479 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  480 | `	/* Delimit the string */` |
|   52974 |  481 | `	zIn  = pStr->zString;` |
|   52974 |  482 | `	zEnd = &zIn[pStr->nByte];` |
|   52974 |  483 | `	if( zIn >= zEnd ){` |
|       - |  484 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  485 | `		 * rather than reserving a new object each time. */` |
|     112 |  486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     112 |  487 | `		return SXRET_OK;` |
|       - |  488 | `	}` |
|   52864 |  489 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  490 | `		/* Already processed,emit the load constant instruction` |
|       - |  491 | `		 * and return.` |
|       - |  492 | `		 */` |
|   15724 |  493 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   15724 |  494 | `		return SXRET_OK;` |
|       - |  495 | `	}` |
|       - |  496 | `	/* Reserve a new constant */` |
|   37142 |  497 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   37142 |  498 | `	if( pObj == 0 ){` |
|     ! 0 |  499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  500 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  501 | `		return SXERR_ABORT;` |
|       - |  502 | `	}` |
|   37142 |  503 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  504 | `	/* Compile the node */` |
|   37157 |  505 | `	for(;;){` |
|   74316 |  506 | `		if( zIn >= zEnd ){` |
|       - |  507 | `			/* End of input */` |
|   37142 |  508 | `			break;` |
|       - |  509 | `		}` |
|   37176 |  510 | `		zCur = zIn;` |
|  587162 |  511 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  549988 |  512 | `			zIn++;` |
|       2 |  513 | `		}` |
|   37176 |  514 | `		if( zIn > zCur ){` |
|       - |  515 | `			/* Append raw contents*/` |
|   37158 |  516 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   18578 |  517 | `		}` |
|   37176 |  518 | `		zIn++;` |
|   37176 |  519 | `		if( zIn < zEnd ){` |
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
|   37176 |  534 | `		zIn++;` |
|       2 |  535 | `	}` |
|       - |  536 | `	/* Emit the load constant instruction */` |
|   37142 |  537 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   37142 |  538 | `	if( pStr->nByte < 1024 ){` |
|       - |  539 | `		/* Install in the literal table */` |
|   37142 |  540 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   18570 |  541 | `	}` |
|       - |  542 | `	/* Node successfully compiled */` |
|   37142 |  543 | `	return SXRET_OK;` |
|   26488 |  544 |  |
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
|    1542 |  606 | `static sxi32 GenStateProcessStringExpression(` |
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
|    1544 |  617 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  618 | `	/* Preallocate some slots */` |
|    1544 |  619 | `	SySetAlloc(&sToken,0x08);` |
|       - |  620 | `	/* Tokenize the text */` |
|    1544 |  621 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  622 | `	/* Swap delimiter */` |
|    1544 |  623 | `	pTmpIn  = pGen->pIn;` |
|    1544 |  624 | `	pTmpEnd = pGen->pEnd;` |
|    1544 |  625 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1544 |  626 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  627 | `	/* Compile the expression */` |
|    1544 |  628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  629 | `	/* Restore token stream */` |
|    1544 |  630 | `	pGen->pIn  = pTmpIn;` |
|    1544 |  631 | `	pGen->pEnd = pTmpEnd;` |
|       - |  632 | `	/* Release the token set */` |
|    1544 |  633 | `	SySetRelease(&sToken);` |
|       - |  634 | `	/* Compilation result */` |
|    1544 |  635 | `	return rc;` |
|       2 |  636 |  |
|       - |  637 | `/*` |
|       - |  638 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  639 | ` */` |
|   14612 |  640 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  641 |  |
|       - |  642 | `	ph7_value *pConstObj;` |
|   14614 |  643 | `	sxu32 nIdx = 0;` |
|       - |  644 | `	/* Reserve a new constant */` |
|   14614 |  645 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   14614 |  646 | `	if( pConstObj == 0 ){` |
|     ! 0 |  647 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  648 | `		return 0;` |
|       - |  649 | `	}` |
|   14614 |  650 | `	(*pCount)++;` |
|   14614 |  651 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  652 | `	/* Emit the load constant instruction */` |
|   14614 |  653 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   14614 |  654 | `	return pConstObj;` |
|    7308 |  655 |  |
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
|   13506 |  694 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  695 |  |
|   13508 |  696 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  697 | `	const char *zIn,*zCur,*zEnd;` |
|   13508 |  698 | `	ph7_value *pObj = 0;` |
|       - |  699 | `	sxi32 iCons;` |
|       - |  700 | `	sxi32 rc;` |
|       - |  701 | `	/* Delimit the string */` |
|   13508 |  702 | `	zIn  = pStr->zString;` |
|   13508 |  703 | `	zEnd = &zIn[pStr->nByte];` |
|   13508 |  704 | `	if( zIn >= zEnd ){` |
|       - |  705 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  706 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  707 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  708 | `		 */` |
|     224 |  709 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     224 |  710 | `		return SXRET_OK;` |
|       - |  711 | `	}` |
|   13286 |  712 | `	zCur = 0;` |
|       - |  713 | `	/* Compile the node */` |
|   13286 |  714 | `	iCons = 0;` |
|    7413 |  715 | `	for(;;){` |
|   22322 |  716 | `		zCur = zIn;` |
|  128334 |  717 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  107556 |  718 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      44 |  719 | `				break;` |
|  107472 |  720 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1460 |  721 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     730 |  722 | `					break;` |
|       - |  723 | `			}` |
|  106014 |  724 | `			zIn++;` |
|       2 |  725 | `		}` |
|   22322 |  726 | `		if( zIn > zCur ){` |
|   10870 |  727 | `			if( pObj == 0 ){` |
|   10600 |  728 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   10600 |  729 | `				if( pObj == 0 ){` |
|     ! 0 |  730 | `					return SXERR_ABORT;` |
|       - |  731 | `				}` |
|    5299 |  732 | `			}` |
|   10870 |  733 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    5434 |  734 | `		}` |
|   22322 |  735 | `		if( zIn >= zEnd ){` |
|   13286 |  736 | `			break;` |
|       - |  737 | `		}` |
|    9038 |  738 | `		if( zIn[0] == '\\' ){` |
|    7496 |  739 | `			const char *zPtr = 0;` |
|       - |  740 | `			sxu32 n;` |
|    7496 |  741 | `			zIn++;` |
|    7496 |  742 | `			if( zIn >= zEnd ){` |
|     ! 0 |  743 | `				break;` |
|       - |  744 | `			}` |
|    7496 |  745 | `			if( pObj == 0 ){` |
|    4016 |  746 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    4016 |  747 | `				if( pObj == 0 ){` |
|     ! 0 |  748 | `					return SXERR_ABORT;` |
|       - |  749 | `				}` |
|    2007 |  750 | `			}` |
|    7496 |  751 | `			n = sizeof(char); /* size of conversion */` |
|    7496 |  752 | `			switch( zIn[0] ){` |
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
|    3385 |  773 | `			case 'n':` |
|       - |  774 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    6772 |  775 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    6772 |  776 | `				break;` |
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
|    7496 |  844 | `			zIn += n;` |
|    7496 |  845 | `			continue;` |
|       - |  846 | `		}` |
|    1544 |  847 | `		if( zIn[0] == '{' ){` |
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
|    1458 |  881 | `			const char *zExpr = zIn;` |
|       - |  882 | `			/* Assemble variable name */` |
|     728 |  883 | `			for(;;){` |
|       - |  884 | `				/* Jump leading dollars */` |
|    2914 |  885 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1458 |  886 | `					zIn++;` |
|       2 |  887 | `				}` |
|     728 |  888 | `				for(;;){` |
|    9404 |  889 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7220 |  890 | `						zIn++;` |
|       2 |  891 | `					}` |
|    1458 |  892 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  893 | `						/* UTF-8 stream */` |
|     ! 0 |  894 | `						zIn++;` |
|     ! 0 |  895 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  896 | `							zIn++;` |
|     ! 0 |  897 | `						}` |
|     ! 0 |  898 | `						continue;` |
|       - |  899 | `					}` |
|    1458 |  900 | `					break;` |
|     ! 0 |  901 | `				}` |
|    1458 |  902 | `				if( zIn >= zEnd ){` |
|      79 |  903 | `					break;` |
|       - |  904 | `				}` |
|    1380 |  905 | `				if( zIn[0] == '[' ){` |
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
|    1372 |  923 | `				}else if(zIn[0] == '{' ){` |
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
|    1368 |  941 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  942 | `					/* Member access operator '->' */` |
|     ! 0 |  943 | `					zIn += 2;` |
|    1368 |  944 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  945 | `					/* Static member access operator '::' */` |
|     ! 0 |  946 | `					zIn += 2;` |
|     ! 0 |  947 | `				}else{` |
|     685 |  948 | `					break;` |
|       - |  949 | `				}` |
|     ! 0 |  950 | `			}` |
|       - |  951 | `			/* Process the expression */` |
|    1458 |  952 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1458 |  953 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  954 | `				return SXERR_ABORT;` |
|       - |  955 | `			}` |
|    1458 |  956 | `			if( rc != SXERR_EMPTY ){` |
|    1456 |  957 | `				++iCons;` |
|     727 |  958 | `			}` |
|       - |  959 | `		}` |
|       - |  960 | `		/* Invalidate the previously used constant */` |
|    1544 |  961 | `		pObj = 0;` |
|       2 |  962 | `	}/*for(;;)*/` |
|   13286 |  963 | `	if( iCons > 1 ){` |
|       - |  964 | `		/* Concatenate all compiled constants */` |
|    1180 |  965 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     589 |  966 | `	}` |
|       - |  967 | `	/* Node successfully compiled */` |
|   13286 |  968 | `	return SXRET_OK;` |
|    6755 |  969 |  |
|       - |  970 | `/*` |
|       - |  971 | ` * Compile a double quoted string.` |
|       - |  972 | ` *  See the block-comment above for more information.` |
|       - |  973 | ` */` |
|   13480 |  974 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  975 |  |
|       - |  976 | `	sxi32 rc;` |
|   13482 |  977 | `	rc = GenStateCompileString(&(*pGen));` |
|    6740 |  978 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  979 | `	/* Compilation result */` |
|   13482 |  980 | `	return rc;` |
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
|   14642 | 1012 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   14644 | 1023 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1024 | `	/* Compile the expression*/` |
|   14644 | 1025 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1026 | `	/* Restore token stream */` |
|   14644 | 1027 | `	RE_SWAP_DELIMITER(pGen);` |
|   14644 | 1028 | `	return rc;` |
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
|   21506 | 1067 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 | 1068 |  |
|       - | 1069 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1070 | `	SyToken *pKey,*pCur;` |
|   21508 | 1071 | `	sxi32 iEmitRef = 0;` |
|   21508 | 1072 | `	sxi32 nPair = 0;` |
|       - | 1073 | `	sxi32 iNest;` |
|       - | 1074 | `	sxi32 rc;` |
|   21508 | 1075 | `	xValidator = 0;` |
|   17467 | 1076 | `	for(;;){` |
|       - | 1077 | `		/* Jump leading commas */` |
|   39488 | 1078 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    4554 | 1079 | `			pGen->pIn++;` |
|       2 | 1080 | `		}` |
|   34936 | 1081 | `		pCur = pGen->pIn;` |
|   34936 | 1082 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1083 | `			/* No more entry to process */` |
|   21496 | 1084 | `			break;` |
|       - | 1085 | `		}` |
|   13442 | 1086 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1087 | `			continue;` |
|       - | 1088 | `		}` |
|       - | 1089 | `		/* Compile the key if available */` |
|   13442 | 1090 | `		pKey = pCur;` |
|   13442 | 1091 | `		iNest = 0;` |
|   37238 | 1092 | `		while( pCur < pGen->pIn ){` |
|   24964 | 1093 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1168 | 1094 | `				break;` |
|       - | 1095 | `			}` |
|   23798 | 1096 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      66 | 1097 | `				iNest++;` |
|   23766 | 1098 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1099 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1100 | `				 * parser will shortly detect any syntax error.` |
|       - | 1101 | `				 */` |
|      66 | 1102 | `				iNest--;` |
|      32 | 1103 | `			}` |
|   23798 | 1104 | `			pCur++;` |
|       2 | 1105 | `		}` |
|   13442 | 1106 | `		rc = SXERR_EMPTY;` |
|   13442 | 1107 | `		if( pCur < pGen->pIn ){` |
|    1168 | 1108 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - | 1109 | `				/* Missing value */` |
|      11 | 1110 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 | 1111 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1112 | `					return SXERR_ABORT;` |
|       - | 1113 | `				}` |
|      11 | 1114 | `				return SXRET_OK;` |
|       - | 1115 | `			}` |
|       - | 1116 | `			/* Compile the expression holding the key */` |
|    1158 | 1117 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - | 1118 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1158 | 1119 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1120 | `				return SXERR_ABORT;` |
|       - | 1121 | `			}` |
|    1158 | 1122 | `			pCur++; /* Jump the '=>' operator */` |
|   12854 | 1123 | `		}else if( pKey == pCur ){` |
|       - | 1124 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1125 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1126 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1127 | `		}else{` |
|       - | 1128 | `			/* Reset back the cursor and point to the entry value */` |
|   12276 | 1129 | `			pCur = pKey;` |
|       - | 1130 | `		}` |
|   13432 | 1131 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1132 | `			/* No available key,load NULL */` |
|   12278 | 1133 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    6138 | 1134 | `		}` |
|   13432 | 1135 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   13430 | 1150 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   13430 | 1151 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1152 | `			return SXERR_ABORT;` |
|       - | 1153 | `		}` |
|   13430 | 1154 | `		if( iEmitRef ){` |
|       - | 1155 | `			/* Emit the load reference instruction */` |
|      32 | 1156 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1157 | `		}` |
|   13430 | 1158 | `		xValidator = 0;` |
|   13430 | 1159 | `		iEmitRef = 0;` |
|   13430 | 1160 | `		nPair++;` |
|       2 | 1161 | `	}` |
|       - | 1162 | `	/* Emit the load map instruction */` |
|   21496 | 1163 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1164 | `	/* Node successfully compiled */` |
|   21496 | 1165 | `	return SXRET_OK;` |
|   10755 | 1166 |  |
|       - | 1167 | `/*` |
|       - | 1168 | ` * Compile the 'array' language construct.` |
|       - | 1169 | ` *	 According to the PHP language reference manual` |
|       - | 1170 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1171 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1172 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1173 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1174 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1175 | ` */` |
|   21418 | 1176 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1177 |  |
|       - | 1178 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   21420 | 1179 | `	pGen->pIn += 2;` |
|   21420 | 1180 | `	pGen->pEnd--;` |
|   10709 | 1181 | `	SXUNUSED(iCompileFlag);` |
|   21420 | 1182 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1183 |  |
|       - | 1184 | `/*` |
|       - | 1185 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - | 1186 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - | 1187 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - | 1188 | ` */` |
|      88 | 1189 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1190 |  |
|       - | 1191 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      89 | 1192 | `	pGen->pIn++;` |
|      89 | 1193 | `	pGen->pEnd--;` |
|      44 | 1194 | `	SXUNUSED(iCompileFlag);` |
|      89 | 1195 | `	return GenStateCompileArrayBody(pGen);` |
|       1 | 1196 |  |
|       - | 1197 | `/*` |
|       - | 1198 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - | 1199 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1200 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1201 | ` * error message.` |
|       - | 1202 | ` * See the routine responible of compiling the list language construct` |
|       - | 1203 | ` * for more inforation.` |
|       - | 1204 | ` */` |
|      58 | 1205 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1206 |  |
|      60 | 1207 | `	sxi32 rc = SXRET_OK;` |
|      60 | 1208 | `	if( pRoot->pOp ){` |
|     ! 0 | 1209 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 | 1210 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - | 1211 | `				/* Unexpected expression */` |
|     ! 0 | 1212 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1213 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 | 1214 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 | 1215 | `					rc = SXERR_INVALID;` |
|     ! 0 | 1216 | `				}` |
|     ! 0 | 1217 | `		}` |
|      60 | 1218 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1219 | `		/* Unexpected expression */` |
|       3 | 1220 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1221 | `			"list(): Expecting a variable not an expression");` |
|       3 | 1222 | `		if( rc != SXERR_ABORT ){` |
|       3 | 1223 | `			rc = SXERR_INVALID;` |
|       1 | 1224 | `		}` |
|       1 | 1225 | `	}` |
|      60 | 1226 | `	return rc;` |
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
|      28 | 1242 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1243 |  |
|       - | 1244 | `	SyToken *pNext;` |
|       - | 1245 | `	sxi32 nExpr;` |
|       - | 1246 | `	sxi32 rc;` |
|      30 | 1247 | `	nExpr = 0;` |
|       - | 1248 | `	/* Jump the 'list' keyword,the leading left parenthesis and the trailing parenthesis */` |
|      30 | 1249 | `	pGen->pIn += 2;` |
|      30 | 1250 | `	pGen->pEnd--;` |
|      14 | 1251 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      92 | 1252 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      64 | 1253 | `		if( pGen->pIn < pNext ){` |
|       - | 1254 | `			/* Compile the expression holding the variable */` |
|      60 | 1255 | `			rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      60 | 1256 | `			if( rc != SXRET_OK ){` |
|       - | 1257 | `				/* Do not bother compiling this expression, it's broken anyway */` |
|     ! 0 | 1258 | `				return SXRET_OK;` |
|       - | 1259 | `			}` |
|      31 | 1260 | `		}else{` |
|       - | 1261 | `			/* Empty entry,load NULL */` |
|       5 | 1262 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - | 1263 | `		}` |
|      64 | 1264 | `		nExpr++;` |
|       - | 1265 | `		/* Advance the stream cursor */` |
|      64 | 1266 | `		pGen->pIn = &pNext[1];` |
|       2 | 1267 | `	}` |
|       - | 1268 | `	/* Emit the LOAD_LIST instruction */` |
|      30 | 1269 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - | 1270 | `	/* Node successfully compiled */` |
|      30 | 1271 | `	return SXRET_OK;` |
|      16 | 1272 |  |
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
|  661416 | 1458 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1459 |  |
|  661418 | 1460 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1461 | `	sxi32 iVv;` |
|       - | 1462 | `	sxi32 iP1;` |
|       - | 1463 | `	void *p3;` |
|       - | 1464 | `	sxi32 rc;` |
|  661418 | 1465 | `	iVv = -1; /* Variable variable counter */` |
| 1322846 | 1466 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  661430 | 1467 | `		pGen->pIn++;` |
|  661430 | 1468 | `		iVv++;` |
|       2 | 1469 | `	}` |
|  661418 | 1470 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1471 | `		/* Invalid variable name */` |
|       3 | 1472 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1473 | `		if( rc == SXERR_ABORT ){` |
|       - | 1474 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1475 | `			return SXERR_ABORT;` |
|       - | 1476 | `		}` |
|       3 | 1477 | `		return SXRET_OK;` |
|       - | 1478 | `	}` |
|  661416 | 1479 | `	p3  = 0;` |
|  661416 | 1480 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
|  661400 | 1500 | `		char *zName = 0;` |
|       - | 1501 | `		/* Extract variable name */` |
|  661400 | 1502 | `		pName = &pGen->pIn->sData;` |
|       - | 1503 | `		/* Advance the stream cursor */` |
|  661400 | 1504 | `		pGen->pIn++;` |
|  661400 | 1505 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  661400 | 1506 | `		if( pEntry == 0 ){` |
|       - | 1507 | `			/* Duplicate name */` |
|   98088 | 1508 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   98088 | 1509 | `			if( zName == 0 ){` |
|     ! 0 | 1510 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1511 | `				return SXERR_ABORT;` |
|       - | 1512 | `			}` |
|       - | 1513 | `			/* Install in the hashtable */` |
|   98088 | 1514 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   49045 | 1515 | `		}else{` |
|       - | 1516 | `			/* Name already available */` |
|  563314 | 1517 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1518 | `		}` |
|  661400 | 1519 | `		p3 = (void *)zName;` |
|       - | 1520 | `	}` |
|  661412 | 1521 | `	iP1 = 0;` |
|  661412 | 1522 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  219530 | 1523 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1524 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  219526 | 1525 | `			iP1 = 1;` |
|  109762 | 1526 | `		}` |
|  109764 | 1527 | `	}` |
|       - | 1528 | `	/* Emit the load instruction */` |
|  661412 | 1529 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  661424 | 1530 | `	while( iVv > 0 ){` |
|      13 | 1531 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1532 | `		iVv--;` |
|       1 | 1533 | `	}` |
|       - | 1534 | `	/* Node successfully compiled */` |
|  661412 | 1535 | `	return SXRET_OK;` |
|  330710 | 1536 |  |
|       - | 1537 | `/*` |
|       - | 1538 | ` * Load a literal.` |
|       - | 1539 | ` */` |
|  427304 | 1540 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 1541 |  |
|  427306 | 1542 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1543 | `	ph7_value *pObj;` |
|       - | 1544 | `	SyString *pStr;` |
|       - | 1545 | `	sxu32 nIdx;` |
|       - | 1546 | `	/* Extract token value */` |
|  427306 | 1547 | `	pStr = &pToken->sData;` |
|       - | 1548 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  427306 | 1549 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   80070 | 1550 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1551 | `			/* NULL constant are always indexed at 0 */` |
|   29810 | 1552 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   29810 | 1553 | `			return SXRET_OK;` |
|   50262 | 1554 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1555 | `			/* TRUE constant are always indexed at 1 */` |
|     464 | 1556 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     464 | 1557 | `			return SXRET_OK;` |
|       2 | 1558 | `		}` |
|  411273 | 1559 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   78272 | 1560 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1561 | `			/* FALSE constant are always indexed at 2 */` |
|   32508 | 1562 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   32508 | 1563 | `			return SXRET_OK;` |
|  348318 | 1564 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   67172 | 1565 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1566 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    4924 | 1567 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    4924 | 1568 | `			if( pObj == 0 ){` |
|     ! 0 | 1569 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1570 | `				return SXERR_ABORT;` |
|       - | 1571 | `			}` |
|    4924 | 1572 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1573 | `			/* Emit the load constant instruction */` |
|    4924 | 1574 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    4924 | 1575 | `			return SXRET_OK;` |
|  318530 | 1576 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   17440 | 1577 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
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
|  317733 | 1593 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    7906 | 1594 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  313774 | 1595 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    7954 | 1596 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|  359594 | 1626 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 1627 | `		ph7_value *pLitObj;` |
|       - | 1628 | `		/* Unknown literal,install it in the literal table */` |
|  143542 | 1629 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  143542 | 1630 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1631 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1632 | `			return SXERR_ABORT;` |
|       - | 1633 | `		}` |
|  143542 | 1634 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  143542 | 1635 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   71770 | 1636 | `	}` |
|       - | 1637 | `	/* Emit the load constant instruction */` |
|  359594 | 1638 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  359594 | 1639 | `	return SXRET_OK;` |
|  213654 | 1640 |  |
|       - | 1641 | `/*` |
|       - | 1642 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1643 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1644 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1645 | ` * Otherwise, load the simple literal directly.` |
|       - | 1646 | ` */` |
|  427324 | 1647 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 1648 |  |
|       - | 1649 | `	sxi32 rc;` |
|  427326 | 1650 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1651 | `		return SXRET_OK;` |
|       - | 1652 | `	}` |
|       - | 1653 | `	/* Check if this is a multi-token namespace path */` |
|  427326 | 1654 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
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
|  427306 | 1704 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  427306 | 1705 | `	return rc;` |
|  213664 | 1706 |  |
|       - | 1707 | `/*` |
|       - | 1708 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1709 | ` */` |
|  427324 | 1710 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1711 |  |
|       - | 1712 | `	sxi32 rc;` |
|  427326 | 1713 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  427326 | 1714 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1715 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1716 | `		return rc;` |
|       - | 1717 | `	}` |
|       - | 1718 | `	/* Node successfully compiled */` |
|  427326 | 1719 | `	return SXRET_OK;` |
|  213664 | 1720 |  |
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
|    2498 | 1863 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 1864 |  |
|       - | 1865 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1866 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1867 | `	sxu32 nLineLocal;` |
|       - | 1868 | `	sxi32 rc;` |
|    2500 | 1869 | `	nLineLocal = pGen->pIn->nLine;` |
|    2500 | 1870 | `	iLevel = 0;` |
|       - | 1871 | `	/* Jump the 'continue' keyword */` |
|    2500 | 1872 | `	pGen->pIn++;` |
|    2500 | 1873 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|    2500 | 1884 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2500 | 1885 | `	if( pLoop == 0 ){` |
|       - | 1886 | `		/* Illegal continue */` |
|      11 | 1887 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 1888 | `		if( rc == SXERR_ABORT ){` |
|       - | 1889 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1890 | `			return SXERR_ABORT;` |
|       - | 1891 | `		}` |
|       6 | 1892 | `	}else{` |
|    2490 | 1893 | `		sxu32 nInstrIdx = 0;` |
|    2490 | 1894 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
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
|    2486 | 1906 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2486 | 1907 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 1908 | `				JumpFixup sJumpFix;` |
|       - | 1909 | `				/* Post-continue */` |
|       8 | 1910 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       8 | 1911 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       8 | 1912 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       3 | 1913 | `			}` |
|       - | 1914 | `		}` |
|       - | 1915 | `	}` |
|    2500 | 1916 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1917 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 1918 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 1919 | `	}` |
|       - | 1920 | `	/* Statement successfully compiled */` |
|    2500 | 1921 | `	return SXRET_OK;` |
|    1251 | 1922 |  |
|       - | 1923 | `/*` |
|       - | 1924 | ` * Compile the 'break' statement.` |
|       - | 1925 | ` * According to the PHP language reference` |
|       - | 1926 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - | 1927 | ` *  structure.` |
|       - | 1928 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - | 1929 | ` *  enclosing structures are to be broken out of.` |
|       - | 1930 | ` */` |
|      90 | 1931 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 | 1932 |  |
|       - | 1933 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1934 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1935 | `	sxi32 rc;` |
|      92 | 1936 | `	iLevel = 0;` |
|       - | 1937 | `	/* Jump the 'break' keyword */` |
|      92 | 1938 | `	pGen->pIn++;` |
|      92 | 1939 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|      92 | 1950 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|      92 | 1951 | `	if( pLoop == 0 ){` |
|       - | 1952 | `		/* Illegal break */` |
|      17 | 1953 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 | 1954 | `		if( rc == SXERR_ABORT ){` |
|       - | 1955 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1956 | `			return SXERR_ABORT;` |
|       - | 1957 | `		}` |
|       9 | 1958 | `	}else{` |
|       - | 1959 | `		sxu32 nInstrIdx;` |
|      76 | 1960 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      76 | 1961 | `		if( rc == SXRET_OK ){` |
|       - | 1962 | `			/* Fix the jump later when the jump destination is resolved */` |
|      76 | 1963 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      37 | 1964 | `		}` |
|       - | 1965 | `	}` |
|      92 | 1966 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1967 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 1968 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 | 1969 | `	}` |
|       - | 1970 | `	/* Statement successfully compiled */` |
|      92 | 1971 | `	return SXRET_OK;` |
|      47 | 1972 |  |
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
|  224174 | 2182 | `static sxi32 PH7_CompileBlock(` |
|       - | 2183 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2184 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2185 | `	)` |
|       2 | 2186 |  |
|       - | 2187 | `	sxi32 rc;` |
|       - | 2188 | `	sxu32 nLine;` |
|  224176 | 2189 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  222872 | 2190 | `		nLine = pGen->pIn->nLine;` |
|  222872 | 2191 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  222872 | 2192 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2193 | `			return SXERR_ABORT;` |
|       - | 2194 | `		}` |
|  222872 | 2195 | `		pGen->pIn++;` |
|       - | 2196 | `		/* Compile until we hit the closing braces '}' */` |
|  325490 | 2197 | `		for(;;){` |
|  650982 | 2198 | `			if( pGen->pIn >= pGen->pEnd ){` |
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
|  650962 | 2209 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2210 | `				/* Closing braces found,break immediately*/` |
|  222852 | 2211 | `				pGen->pIn++;` |
|  222852 | 2212 | `				break;` |
|       - | 2213 | `			}` |
|       - | 2214 | `			/* Compile a single statement */` |
|  428112 | 2215 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  428112 | 2216 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2217 | `				return SXERR_ABORT;` |
|       - | 2218 | `			}` |
|       2 | 2219 | `		}` |
|  222872 | 2220 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  112741 | 2221 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|    1306 | 2265 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1306 | 2266 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2267 | `			return SXERR_ABORT;` |
|       - | 2268 | `		}` |
|       - | 2269 | `	}` |
|       - | 2270 | `	/* Jump trailing semi-colons ';' */` |
|  224176 | 2271 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2272 | `		pGen->pIn++;` |
|     ! 0 | 2273 | `	}` |
|  224176 | 2274 | `	return SXRET_OK;` |
|  112089 | 2275 |  |
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
|    9916 | 2295 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 2296 |  |
|    9918 | 2297 | `	GenBlock *pWhileBlock = 0;` |
|    9918 | 2298 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2299 | `	sxu32 nFalseJump;` |
|       - | 2300 | `	sxu32 nLine;` |
|       - | 2301 | `	sxi32 rc;` |
|    9918 | 2302 | `	nLine = pGen->pIn->nLine;` |
|       - | 2303 | `	/* Jump the 'while' keyword */` |
|    9918 | 2304 | `	pGen->pIn++;` |
|    9918 | 2305 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2306 | `		/* Syntax error */` |
|     ! 0 | 2307 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2308 | `		if( rc == SXERR_ABORT ){` |
|       - | 2309 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2310 | `			return SXERR_ABORT;` |
|       - | 2311 | `		}` |
|     ! 0 | 2312 | `		goto Synchronize;` |
|       - | 2313 | `	}` |
|       - | 2314 | `	/* Jump the left parenthesis '(' */` |
|    9918 | 2315 | `	pGen->pIn++;` |
|       - | 2316 | `	/* Create the loop block */` |
|    9918 | 2317 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    9918 | 2318 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2319 | `		return SXERR_ABORT;` |
|       - | 2320 | `	}` |
|       - | 2321 | `	/* Delimit the condition */` |
|    9918 | 2322 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    9918 | 2323 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2324 | `		/* Empty expression */` |
|       3 | 2325 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2326 | `		if( rc == SXERR_ABORT ){` |
|       - | 2327 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2328 | `			return SXERR_ABORT;` |
|       - | 2329 | `		}` |
|       1 | 2330 | `	}` |
|       - | 2331 | `	/* Swap token streams */` |
|    9918 | 2332 | `	pTmp = pGen->pEnd;` |
|    9918 | 2333 | `	pGen->pEnd = pEnd;` |
|       - | 2334 | `	/* Compile the expression */` |
|    9918 | 2335 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    9918 | 2336 | `	if( rc == SXERR_ABORT ){` |
|       - | 2337 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2338 | `		return SXERR_ABORT;` |
|       - | 2339 | `	}` |
|       - | 2340 | `	/* Update token stream */` |
|    9918 | 2341 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2342 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2343 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2344 | `			return SXERR_ABORT;` |
|       - | 2345 | `		}` |
|     ! 0 | 2346 | `		pGen->pIn++;` |
|     ! 0 | 2347 | `	}` |
|       - | 2348 | `	/* Synchronize pointers */` |
|    9918 | 2349 | `	pGen->pIn  = &pEnd[1];` |
|    9918 | 2350 | `	pGen->pEnd = pTmp;` |
|       - | 2351 | `	/* Emit the false jump */` |
|    9918 | 2352 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2353 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    9918 | 2354 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2355 | `	/* Compile the loop body */` |
|    9918 | 2356 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    9918 | 2357 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2358 | `		return SXERR_ABORT;` |
|       - | 2359 | `	}` |
|       - | 2360 | `	/* Emit the unconditional jump to the start of the loop */` |
|    9918 | 2361 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2362 | `	/* Fix all jumps now the destination is resolved */` |
|    9918 | 2363 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2364 | `	/* Release the loop block */` |
|    9918 | 2365 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2366 | `	/* Statement successfully compiled */` |
|    9918 | 2367 | `	return SXRET_OK;` |
|     ! 0 | 2368 | `Synchronize:` |
|       - | 2369 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2370 | `	 * compiling this erroneous block.` |
|       - | 2371 | `	 */` |
|     ! 0 | 2372 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2373 | `		pGen->pIn++;` |
|     ! 0 | 2374 | `	}` |
|     ! 0 | 2375 | `	return SXRET_OK;` |
|    4960 | 2376 |  |
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
|    9918 | 2524 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 2525 |  |
|    9920 | 2526 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    9920 | 2527 | `	GenBlock *pForBlock = 0;` |
|       - | 2528 | `	sxu32 nFalseJump;` |
|       - | 2529 | `	sxu32 nLine;` |
|       - | 2530 | `	sxi32 rc;` |
|    9920 | 2531 | `	nLine = pGen->pIn->nLine;` |
|       - | 2532 | `	/* Jump the 'for' keyword */` |
|    9920 | 2533 | `	pGen->pIn++;` |
|    9920 | 2534 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2535 | `		/* Syntax error */` |
|     ! 0 | 2536 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2537 | `		if( rc == SXERR_ABORT ){` |
|       - | 2538 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2539 | `			return SXERR_ABORT;` |
|       - | 2540 | `		}` |
|     ! 0 | 2541 | `		return SXRET_OK;` |
|       - | 2542 | `	}` |
|       - | 2543 | `	/* Jump the left parenthesis '(' */` |
|    9920 | 2544 | `	pGen->pIn++;` |
|       - | 2545 | `	/* Delimit the init-expr;condition;post-expr */` |
|    9920 | 2546 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    9920 | 2547 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|    9920 | 2562 | `	pTmp = pGen->pEnd;` |
|    9920 | 2563 | `	pGen->pEnd = pEnd;` |
|       - | 2564 | `	/* Compile initialization expressions if available */` |
|    9920 | 2565 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2566 | `	/* Pop operand lvalues */` |
|    9920 | 2567 | `	if( rc == SXERR_ABORT ){` |
|       - | 2568 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2569 | `		return SXERR_ABORT;` |
|    9920 | 2570 | `	}else if( rc != SXERR_EMPTY ){` |
|    9918 | 2571 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    4958 | 2572 | `	}` |
|    9920 | 2573 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|    9920 | 2584 | `	pGen->pIn++;` |
|       - | 2585 | `	/* Create the loop block */` |
|    9920 | 2586 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    9920 | 2587 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2588 | `		return SXERR_ABORT;` |
|       - | 2589 | `	}` |
|       - | 2590 | `	/* Deffer continue jumps */` |
|    9920 | 2591 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 2592 | `	/* Compile the condition */` |
|    9920 | 2593 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    9920 | 2594 | `	if( rc == SXERR_ABORT ){` |
|       - | 2595 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2596 | `		return SXERR_ABORT;` |
|    9920 | 2597 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 2598 | `		/* Emit the false jump */` |
|    9918 | 2599 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2600 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    9918 | 2601 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    4958 | 2602 | `	}` |
|    9920 | 2603 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|    9916 | 2614 | `	pGen->pIn++;` |
|       - | 2615 | `	/* Save the post condition stream */` |
|    9916 | 2616 | `	pPostStart = pGen->pIn;` |
|       - | 2617 | `	/* Compile the loop body */` |
|    9916 | 2618 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    9916 | 2619 | `	pGen->pEnd = pTmp;` |
|    9916 | 2620 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    9916 | 2621 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2622 | `		return SXERR_ABORT;` |
|       - | 2623 | `	}` |
|       - | 2624 | `	/* Fix post-continue jumps */` |
|    9916 | 2625 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
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
|    9916 | 2641 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2642 | `		pPostStart++;` |
|     ! 0 | 2643 | `	}` |
|    9916 | 2644 | `	if( pPostStart < pEnd ){` |
|       - | 2645 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    9916 | 2646 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    9916 | 2647 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    9916 | 2648 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 2649 | `			/* Syntax error */` |
|     ! 0 | 2650 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 2651 | `			if( rc == SXERR_ABORT ){` |
|       - | 2652 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2653 | `				return SXERR_ABORT;` |
|       - | 2654 | `			}` |
|     ! 0 | 2655 | `			return SXRET_OK;` |
|       - | 2656 | `		}` |
|    9916 | 2657 | `		RE_SWAP_DELIMITER(pGen);` |
|    9916 | 2658 | `		if( rc == SXERR_ABORT ){` |
|       - | 2659 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2660 | `			return SXERR_ABORT;` |
|    9916 | 2661 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 2662 | `			/* Pop operand lvalue */` |
|    9916 | 2663 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    4957 | 2664 | `		}` |
|    4957 | 2665 | `	}` |
|       - | 2666 | `	/* Emit the unconditional jump to the start of the loop */` |
|    9916 | 2667 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 2668 | `	/* Fix all jumps now the destination is resolved */` |
|    9916 | 2669 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2670 | `	/* Release the loop block */` |
|    9916 | 2671 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2672 | `	/* Statement successfully compiled */` |
|    9916 | 2673 | `	return SXRET_OK;` |
|    4961 | 2674 |  |
|       - | 2675 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 2676 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 2677 | ` * are allowed.` |
|       - | 2678 | ` */` |
|    5270 | 2679 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 2680 |  |
|    5272 | 2681 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    5272 | 2682 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 2683 | `		/* Unexpected expression */` |
|     ! 0 | 2684 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 2685 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 2686 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 2687 | `			rc = SXERR_INVALID;` |
|     ! 0 | 2688 | `		}` |
|     ! 0 | 2689 | `	}` |
|    5272 | 2690 | `	return rc;` |
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
|    2664 | 2718 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 2719 |  |
|    2666 | 2720 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    2666 | 2721 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    2666 | 2722 | `	GenBlock *pForeachBlock = 0;` |
|       - | 2723 | `	ph7_foreach_info *pInfo;` |
|       - | 2724 | `	sxu32 nFalseJump;` |
|       - | 2725 | `	VmInstr *pInstr;` |
|       - | 2726 | `	sxu32 nLine;` |
|       - | 2727 | `	sxi32 rc;` |
|    2666 | 2728 | `	nLine = pGen->pIn->nLine;` |
|       - | 2729 | `	/* Jump the 'foreach' keyword */` |
|    2666 | 2730 | `	pGen->pIn++;` |
|    2666 | 2731 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2732 | `		/* Syntax error */` |
|     ! 0 | 2733 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 2734 | `		if( rc == SXERR_ABORT ){` |
|       - | 2735 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2736 | `			return SXERR_ABORT;` |
|       - | 2737 | `		}` |
|     ! 0 | 2738 | `		goto Synchronize;` |
|       - | 2739 | `	}` |
|       - | 2740 | `	/* Jump the left parenthesis '(' */` |
|    2666 | 2741 | `	pGen->pIn++;` |
|       - | 2742 | `	/* Create the loop block */` |
|    2666 | 2743 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    2666 | 2744 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2745 | `		return SXERR_ABORT;` |
|       - | 2746 | `	}` |
|       - | 2747 | `	/* Delimit the expression */` |
|    2666 | 2748 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    2666 | 2749 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2750 | `		/* Empty expression */` |
|     ! 0 | 2751 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 2752 | `		if( rc == SXERR_ABORT ){` |
|       - | 2753 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2754 | `			return SXERR_ABORT;` |
|       - | 2755 | `		}` |
|       - | 2756 | `		/* Synchronize */` |
|     ! 0 | 2757 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2758 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2759 | `			pGen->pIn++;` |
|     ! 0 | 2760 | `		}` |
|     ! 0 | 2761 | `		return SXRET_OK;` |
|       - | 2762 | `	}` |
|       - | 2763 | `	/* Compile the array expression */` |
|    2666 | 2764 | `	pCur = pGen->pIn;` |
|   17906 | 2765 | `	while( pCur < pEnd ){` |
|   17906 | 2766 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    2676 | 2767 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    2676 | 2768 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 2769 | `				/* Break with the first 'as' found */` |
|    2666 | 2770 | `				break;` |
|       - | 2771 | `			}` |
|       5 | 2772 | `		}` |
|       - | 2773 | `		/* Advance the stream cursor */` |
|   15242 | 2774 | `		pCur++;` |
|       2 | 2775 | `	}` |
|    2666 | 2776 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 2777 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2778 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 2779 | `		if( rc == SXERR_ABORT ){` |
|       - | 2780 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2781 | `			return SXERR_ABORT;` |
|       - | 2782 | `		}` |
|     ! 0 | 2783 | `		goto Synchronize;` |
|       - | 2784 | `	}` |
|       - | 2785 | `	/* Swap token streams */` |
|    2666 | 2786 | `	pTmp = pGen->pEnd;` |
|    2666 | 2787 | `	pGen->pEnd = pCur;` |
|    2666 | 2788 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    2666 | 2789 | `	if( rc == SXERR_ABORT ){` |
|       - | 2790 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2791 | `		return SXERR_ABORT;` |
|       - | 2792 | `	}` |
|       - | 2793 | `	/* Update token stream */` |
|    2666 | 2794 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 2795 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2796 | `		if( rc == SXERR_ABORT ){` |
|       - | 2797 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2798 | `			return SXERR_ABORT;` |
|       - | 2799 | `		}` |
|     ! 0 | 2800 | `		pGen->pIn++;` |
|     ! 0 | 2801 | `	}` |
|    2666 | 2802 | `	pCur++; /* Jump the 'as' keyword */` |
|    2666 | 2803 | `	pGen->pIn = pCur;` |
|    2666 | 2804 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2805 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 2806 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2807 | `			return SXERR_ABORT;` |
|       - | 2808 | `		}` |
|     ! 0 | 2809 | `	}` |
|       - | 2810 | `	/* Create the foreach context */` |
|    2666 | 2811 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    2666 | 2812 | `	if( pInfo == 0 ){` |
|     ! 0 | 2813 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 2814 | `		return SXERR_ABORT;` |
|       - | 2815 | `	}` |
|       - | 2816 | `	/* Zero the structure */` |
|    2666 | 2817 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 2818 | `	/* Initialize structure fields */` |
|    2666 | 2819 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 2820 | `	/* Check if we have a key field */` |
|    8006 | 2821 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    5342 | 2822 | `		pCur++;` |
|       2 | 2823 | `	}` |
|    2666 | 2824 | `	if( pCur < pEnd ){` |
|       - | 2825 | `		/* Compile the expression holding the key name */` |
|    2614 | 2826 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 2827 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 2828 | `			if( rc == SXERR_ABORT ){` |
|       - | 2829 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2830 | `				return SXERR_ABORT;` |
|       - | 2831 | `			}` |
|     ! 0 | 2832 | `		}else{` |
|    2614 | 2833 | `			pGen->pEnd = pCur;` |
|    2614 | 2834 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2614 | 2835 | `			if( rc == SXERR_ABORT ){` |
|       - | 2836 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2837 | `				return SXERR_ABORT;` |
|       - | 2838 | `			}` |
|    2614 | 2839 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2614 | 2840 | `			if( pInstr->p3 ){` |
|       - | 2841 | `				/* Record key name */` |
|    2614 | 2842 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1306 | 2843 | `			}` |
|    2614 | 2844 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 2845 | `		}` |
|    2614 | 2846 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1306 | 2847 | `	}` |
|    2666 | 2848 | `	pGen->pEnd = pEnd;` |
|    2666 | 2849 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2850 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 2851 | `		if( rc == SXERR_ABORT ){` |
|       - | 2852 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2853 | `			return SXERR_ABORT;` |
|       - | 2854 | `		}` |
|     ! 0 | 2855 | `		goto Synchronize;` |
|       - | 2856 | `	}` |
|    2666 | 2857 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       7 | 2858 | `		pGen->pIn++;` |
|       - | 2859 | `		/* Pass by reference  */` |
|       7 | 2860 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       3 | 2861 | `	}` |
|       - | 2862 | `	/* Check if the value target is list() */` |
|    2666 | 2863 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       6 | 2864 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 2865 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - | 2866 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - | 2867 | `		 */` |
|       - | 2868 | `		static int iForeachListCnt = 0;` |
|       - | 2869 | `		char zTmp[128];` |
|       - | 2870 | `		sxu32 nLen;` |
|       - | 2871 | `		char *zDup;` |
|       7 | 2872 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       7 | 2873 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       7 | 2874 | `		if( zDup == 0 ){` |
|     ! 0 | 2875 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2876 | `			return SXERR_ABORT;` |
|       - | 2877 | `		}` |
|       7 | 2878 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 2879 | `		/* Save list() token boundaries */` |
|       7 | 2880 | `		pListStart = pGen->pIn;` |
|       - | 2881 | `		/* Advance past list(...) — validate parentheses */` |
|       7 | 2882 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       7 | 2883 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 | 2884 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - | 2885 | `				"foreach: Expected '(' after 'list'");` |
|       3 | 2886 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2887 | `				return SXERR_ABORT;` |
|       - | 2888 | `			}` |
|       3 | 2889 | `			goto Synchronize;` |
|       - | 2890 | `		}` |
|       5 | 2891 | `		pGen->pIn++; /* Jump '(' */` |
|       5 | 2892 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       5 | 2893 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 2894 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 2895 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 | 2896 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2897 | `				return SXERR_ABORT;` |
|       - | 2898 | `			}` |
|     ! 0 | 2899 | `			goto Synchronize;` |
|       - | 2900 | `		}` |
|       5 | 2901 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       5 | 2902 | `		pListEnd = pGen->pIn;` |
|       5 | 2903 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       3 | 2904 | `	}else{` |
|       - | 2905 | `		/* Compile the expression holding the value name */` |
|    2660 | 2906 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2660 | 2907 | `		if( rc == SXERR_ABORT ){` |
|       - | 2908 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2909 | `			return SXERR_ABORT;` |
|       - | 2910 | `		}` |
|    2660 | 2911 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2660 | 2912 | `		if( pInstr->p3 ){` |
|       - | 2913 | `			/* Record value name */` |
|    2660 | 2914 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1329 | 2915 | `		}` |
|       - | 2916 | `	}` |
|       - | 2917 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    2664 | 2918 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 2919 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2664 | 2920 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 2921 | `	/* Record the first instruction to execute */` |
|    2664 | 2922 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2923 | `	/* Emit the FOREACH_STEP instruction */` |
|    2664 | 2924 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 2925 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2664 | 2926 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 2927 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    2664 | 2928 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - | 2929 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - | 2930 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - | 2931 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - | 2932 | `		 */` |
|       5 | 2933 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - | 2934 | `		/* Compile list(...) body directly — this pushes variables and emits LOAD_LIST.` |
|       - | 2935 | `		 * We position the tokens at the list keyword so PH7_CompileList picks up` |
|       - | 2936 | `		 * the opening '(' and the variable names inside.` |
|       - | 2937 | `		 */` |
|       5 | 2938 | `		pSavedIn = pGen->pIn;` |
|       5 | 2939 | `		pSavedEnd = pGen->pEnd;` |
|       5 | 2940 | `		pGen->pIn = pListStart;` |
|       5 | 2941 | `		pGen->pEnd = pListEnd;` |
|       5 | 2942 | `		rc = PH7_CompileList(&(*pGen),0);` |
|       5 | 2943 | `		pGen->pIn = pSavedIn;` |
|       5 | 2944 | `		pGen->pEnd = pSavedEnd;` |
|       5 | 2945 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2946 | `			return SXERR_ABORT;` |
|       - | 2947 | `		}` |
|       - | 2948 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       5 | 2949 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 2950 | `	}` |
|       - | 2951 | `	/* Compile the loop body */` |
|    2664 | 2952 | `	pGen->pIn = &pEnd[1];` |
|    2664 | 2953 | `	pGen->pEnd = pTmp;` |
|    2664 | 2954 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    2664 | 2955 | `	if( rc == SXERR_ABORT ){` |
|       - | 2956 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2957 | `		return SXERR_ABORT;` |
|       - | 2958 | `	}` |
|       - | 2959 | `	/* Emit the unconditional jump to the start of the loop */` |
|    2664 | 2960 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 2961 | `	/* Fix all jumps now the destination is resolved */` |
|    2664 | 2962 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2963 | `	/* Release the loop block */` |
|    2664 | 2964 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2965 | `	/* Statement successfully compiled */` |
|    2664 | 2966 | `	return SXRET_OK;` |
|       1 | 2967 | `Synchronize:` |
|       - | 2968 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2969 | `	 * compiling this erroneous block.` |
|       - | 2970 | `	 */` |
|       3 | 2971 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2972 | `		pGen->pIn++;` |
|     ! 0 | 2973 | `	}` |
|       3 | 2974 | `	return SXRET_OK;` |
|    1334 | 2975 |  |
|       - | 2976 | `/*` |
|       - | 2977 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 2978 | ` * According to the PHP language reference` |
|       - | 2979 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 2980 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 2981 | ` *  that is similar to that of C:` |
|       - | 2982 | ` *  if (expr)` |
|       - | 2983 | ` *   statement` |
|       - | 2984 | ` *  else construct:` |
|       - | 2985 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 2986 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 2987 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 2988 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 2989 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 2990 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 2991 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 2992 | ` *  elseif` |
|       - | 2993 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 2994 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 2995 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 2996 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 2997 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 2998 | ` *   <?php` |
|       - | 2999 | ` *    if ($a > $b) {` |
|       - | 3000 | ` *     echo "a is bigger than b";` |
|       - | 3001 | ` *    } elseif ($a == $b) {` |
|       - | 3002 | ` *     echo "a is equal to b";` |
|       - | 3003 | ` *    } else {` |
|       - | 3004 | ` *     echo "a is smaller than b";` |
|       - | 3005 | ` *    }` |
|       - | 3006 | ` *    ?>` |
|       - | 3007 | ` */` |
|   98880 | 3008 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 3009 |  |
|   98882 | 3010 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   98882 | 3011 | `	GenBlock *pCondBlock = 0;` |
|       - | 3012 | `	sxu32 nJumpIdx;` |
|       - | 3013 | `	sxu32 nKeyID;` |
|       - | 3014 | `	sxi32 rc;` |
|       - | 3015 | `	/* Jump the 'if' keyword */` |
|   98882 | 3016 | `	pGen->pIn++;` |
|   98882 | 3017 | `	pToken = pGen->pIn;` |
|       - | 3018 | `	/* Create the conditional block */` |
|   98882 | 3019 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   98882 | 3020 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3021 | `		return SXERR_ABORT;` |
|       - | 3022 | `	}` |
|       - | 3023 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   54372 | 3024 | `	for(;;){` |
|  108746 | 3025 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3026 | `			/* Syntax error */` |
|     ! 0 | 3027 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3028 | `				pToken--;` |
|     ! 0 | 3029 | `			}` |
|     ! 0 | 3030 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 3031 | `			if( rc == SXERR_ABORT ){` |
|       - | 3032 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3033 | `				return SXERR_ABORT;` |
|       - | 3034 | `			}` |
|     ! 0 | 3035 | `			goto Synchronize;` |
|       - | 3036 | `		}` |
|       - | 3037 | `		/* Jump the left parenthesis '(' */` |
|  108746 | 3038 | `		pToken++;` |
|       - | 3039 | `		/* Delimit the condition */` |
|  108746 | 3040 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  108746 | 3041 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 3042 | `			/* Syntax error */` |
|     ! 0 | 3043 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3044 | `				pToken--;` |
|     ! 0 | 3045 | `			}` |
|     ! 0 | 3046 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 3047 | `			if( rc == SXERR_ABORT ){` |
|       - | 3048 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3049 | `				return SXERR_ABORT;` |
|       - | 3050 | `			}` |
|     ! 0 | 3051 | `			goto Synchronize;` |
|       - | 3052 | `		}` |
|       - | 3053 | `		/* Swap token streams */` |
|  108746 | 3054 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 3055 | `		/* Compile the condition */` |
|  108746 | 3056 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3057 | `		/* Update token stream */` |
|  108746 | 3058 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 3059 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3060 | `			pGen->pIn++;` |
|     ! 0 | 3061 | `		}` |
|  108746 | 3062 | `		pGen->pIn  = &pEnd[1];` |
|  108746 | 3063 | `		pGen->pEnd = pTmp;` |
|  108746 | 3064 | `		if( rc == SXERR_ABORT ){` |
|       - | 3065 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3066 | `			return SXERR_ABORT;` |
|       - | 3067 | `		}` |
|       - | 3068 | `		/* Emit the false jump */` |
|  108746 | 3069 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 3070 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  108746 | 3071 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 3072 | `		/* Compile the body */` |
|  108746 | 3073 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  108746 | 3074 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3075 | `			return SXERR_ABORT;` |
|       - | 3076 | `		}` |
|  108746 | 3077 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   29273 | 3078 | `			break;` |
|       - | 3079 | `		}` |
|       - | 3080 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   50204 | 3081 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   50204 | 3082 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   32240 | 3083 | `			break;` |
|       - | 3084 | `		}` |
|       - | 3085 | `		/* Emit the unconditional jump */` |
|   17966 | 3086 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 3087 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   17966 | 3088 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   17966 | 3089 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   13022 | 3090 | `			pToken = &pGen->pIn[1];` |
|   13022 | 3091 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    4950 | 3092 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4052 | 3093 | `					break;` |
|       - | 3094 | `			}` |
|    4922 | 3095 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2460 | 3096 | `		}` |
|    9866 | 3097 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 3098 | `		/* Synchronize cursors */` |
|    9866 | 3099 | `		pToken = pGen->pIn;` |
|       - | 3100 | `		/* Fix the false jump */` |
|    9866 | 3101 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 3102 | `	} /* For(;;) */` |
|       - | 3103 | `	/* Fix the false jump */` |
|   98882 | 3104 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   98882 | 3105 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   40338 | 3106 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 3107 | `			/* Compile the else block */` |
|    8102 | 3108 | `			pGen->pIn++;` |
|    8102 | 3109 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    8102 | 3110 | `			if( rc == SXERR_ABORT ){` |
|       - | 3111 |  |
|     ! 0 | 3112 | `				return SXERR_ABORT;` |
|       - | 3113 | `			}` |
|    4050 | 3114 | `	}` |
|   98882 | 3115 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3116 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   98882 | 3117 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 3118 | `	/* Release the conditional block */` |
|   98882 | 3119 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3120 | `	/* Statement successfully compiled */` |
|   98882 | 3121 | `	return SXRET_OK;` |
|     ! 0 | 3122 | `Synchronize:` |
|       - | 3123 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 3124 | `	 */` |
|     ! 0 | 3125 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3126 | `		pGen->pIn++;` |
|     ! 0 | 3127 | `	}` |
|     ! 0 | 3128 | `	return SXRET_OK;` |
|   49442 | 3129 |  |
|       - | 3130 | `/*` |
|       - | 3131 | ` * Compile the global construct.` |
|       - | 3132 | ` * According to the PHP language reference` |
|       - | 3133 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 3134 | ` *  to be used in that function.` |
|       - | 3135 | ` *  Example #1 Using global` |
|       - | 3136 | ` *  <?php` |
|       - | 3137 | ` *   $a = 1;` |
|       - | 3138 | ` *   $b = 2;` |
|       - | 3139 | ` *   function Sum()` |
|       - | 3140 | ` *   {` |
|       - | 3141 | ` *    global $a, $b;` |
|       - | 3142 | ` *    $b = $a + $b;` |
|       - | 3143 | ` *   }` |
|       - | 3144 | ` *   Sum();` |
|       - | 3145 | ` *   echo $b;` |
|       - | 3146 | ` *  ?>` |
|       - | 3147 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 3148 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 3149 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 3150 | ` */` |
|      26 | 3151 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 | 3152 |  |
|      28 | 3153 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3154 | `	sxi32 nExpr;` |
|       - | 3155 | `	sxi32 rc;` |
|       - | 3156 | `	/* Jump the 'global' keyword */` |
|      28 | 3157 | `	pGen->pIn++;` |
|      28 | 3158 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 3159 | `		/* Nothing to process */` |
|     ! 0 | 3160 | `		return SXRET_OK;` |
|       - | 3161 | `	}` |
|      28 | 3162 | `	pTmp = pGen->pEnd;` |
|      28 | 3163 | `	nExpr = 0;` |
|      56 | 3164 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      30 | 3165 | `		if( pGen->pIn < pNext ){` |
|      30 | 3166 | `			pGen->pEnd = pNext;` |
|      30 | 3167 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3168 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 3169 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 3170 | `					return SXERR_ABORT;` |
|       - | 3171 | `				}` |
|     ! 0 | 3172 | `			}else{` |
|      30 | 3173 | `				pGen->pIn++;` |
|      30 | 3174 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3175 | `					/* Emit a warning */` |
|     ! 0 | 3176 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 3177 | `				}else{` |
|      30 | 3178 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 | 3179 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3180 | `						return SXERR_ABORT;` |
|      30 | 3181 | `					}else if(rc != SXERR_EMPTY ){` |
|      30 | 3182 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      30 | 3183 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - | 3184 | `							/* Variable name, not a constant */` |
|      30 | 3185 | `							pLast->iP1 = 0;` |
|      14 | 3186 | `						}` |
|      30 | 3187 | `						nExpr++;` |
|      14 | 3188 | `					}` |
|       - | 3189 | `				}` |
|       - | 3190 | `			}` |
|      14 | 3191 | `		}` |
|       - | 3192 | `		/* Next expression in the stream */` |
|      30 | 3193 | `		pGen->pIn = pNext;` |
|       - | 3194 | `		/* Jump trailing commas */` |
|      32 | 3195 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3196 | `			pGen->pIn++;` |
|       1 | 3197 | `		}` |
|       2 | 3198 | `	}` |
|       - | 3199 | `	/* Restore token stream */` |
|      28 | 3200 | `	pGen->pEnd = pTmp;` |
|      28 | 3201 | `	if( nExpr > 0 ){` |
|       - | 3202 | `		/* Emit the uplink instruction */` |
|      28 | 3203 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      13 | 3204 | `	}` |
|      28 | 3205 | `	return SXRET_OK;` |
|      15 | 3206 |  |
|       - | 3207 | `/*` |
|       - | 3208 | ` * Compile the return statement.` |
|       - | 3209 | ` * According to the PHP language reference` |
|       - | 3210 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 3211 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 3212 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 3213 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 3214 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 3215 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 3216 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 3217 | ` *  from within the main script file, then script execution end.` |
|       - | 3218 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 3219 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 3220 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 3221 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 3222 | ` */` |
|  103806 | 3223 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 3224 |  |
|  103808 | 3225 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3226 | `	sxi32 rc;` |
|       - | 3227 | `	/* Jump the 'return' keyword */` |
|  103808 | 3228 | `	pGen->pIn++;` |
|  103808 | 3229 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3230 | `		/* Compile the expression */` |
|  103786 | 3231 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  103786 | 3232 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3233 | `			return SXERR_ABORT;` |
|  103786 | 3234 | `		}else if(rc != SXERR_EMPTY ){` |
|  103786 | 3235 | `			nRet = 1;` |
|   51892 | 3236 | `		}` |
|   51892 | 3237 | `	}` |
|       - | 3238 | `	/* Emit the done instruction */` |
|  103808 | 3239 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  103808 | 3240 | `	return SXRET_OK;` |
|   51905 | 3241 |  |
|       - | 3242 | `/*` |
|       - | 3243 | ` * Compile the die/exit language construct.` |
|       - | 3244 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 3245 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 3246 | ` */` |
|      88 | 3247 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 | 3248 |  |
|      90 | 3249 | `	sxi32 nExpr = 0;` |
|       - | 3250 | `	sxi32 rc;` |
|       - | 3251 | `	/* Jump the die/exit keyword */` |
|      90 | 3252 | `	pGen->pIn++;` |
|      90 | 3253 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3254 | `		/* Compile the expression */` |
|      90 | 3255 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 | 3256 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3257 | `			return SXERR_ABORT;` |
|      90 | 3258 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 | 3259 | `			nExpr = 1;` |
|      44 | 3260 | `		}` |
|      44 | 3261 | `	}` |
|       - | 3262 | `	/* Emit the HALT instruction */` |
|      90 | 3263 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 | 3264 | `	return SXRET_OK;` |
|      46 | 3265 |  |
|       - | 3266 | `/*` |
|       - | 3267 | ` * Compile the 'echo' language construct.` |
|       - | 3268 | ` */` |
|    9560 | 3269 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 3270 |  |
|    9562 | 3271 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3272 | `	sxi32 rc;` |
|       - | 3273 | `	/* Jump the 'echo' keyword */` |
|    9562 | 3274 | `	pGen->pIn++;` |
|       - | 3275 | `	/* Compile arguments one after one */` |
|    9562 | 3276 | `	pTmp = pGen->pEnd;` |
|   19506 | 3277 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    9946 | 3278 | `		if( pGen->pIn < pNext ){` |
|    9946 | 3279 | `			pGen->pEnd = pNext;` |
|    9946 | 3280 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    9946 | 3281 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3282 | `				return SXERR_ABORT;` |
|    9946 | 3283 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3284 | `				/* Emit the consume instruction */` |
|    9922 | 3285 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    4960 | 3286 | `			}` |
|    4972 | 3287 | `		}` |
|       - | 3288 | `		/* Jump trailing commas */` |
|   10330 | 3289 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     386 | 3290 | `			pNext++;` |
|       2 | 3291 | `		}` |
|    9946 | 3292 | `		pGen->pIn = pNext;` |
|       2 | 3293 | `	}` |
|       - | 3294 | `	/* Restore token stream */` |
|    9562 | 3295 | `	pGen->pEnd = pTmp;` |
|    9562 | 3296 | `	return SXRET_OK;` |
|    4782 | 3297 |  |
|       - | 3298 | `/*` |
|       - | 3299 | ` * Compile the static statement.` |
|       - | 3300 | ` * According to the PHP language reference` |
|       - | 3301 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 3302 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 3303 | ` *  when program execution leaves this scope.` |
|       - | 3304 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 3305 | ` * Symisc eXtension.` |
|       - | 3306 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 3307 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3308 | ` *  Example` |
|       - | 3309 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 3310 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 3311 | ` */` |
|       2 | 3312 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 | 3313 |  |
|       - | 3314 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 3315 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 3316 | `	GenBlock *pBlock;` |
|       - | 3317 | `	SyString *pName;` |
|       - | 3318 | `	char *zDup;` |
|       - | 3319 | `	sxu32 nLine;` |
|       - | 3320 | `	sxi32 rc;` |
|       - | 3321 | `	/* Jump the static keyword */` |
|       3 | 3322 | `	nLine = pGen->pIn->nLine;` |
|       3 | 3323 | `	pGen->pIn++;` |
|       - | 3324 | `	/* Extract the enclosing function if any */` |
|       3 | 3325 | `	pBlock = pGen->pCurrent;` |
|       5 | 3326 | `	while( pBlock ){` |
|       5 | 3327 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 | 3328 | `			break;` |
|       - | 3329 | `		}` |
|       - | 3330 | `		/* Point to the upper block */` |
|       3 | 3331 | `		pBlock = pBlock->pParent;` |
|       1 | 3332 | `	}` |
|       3 | 3333 | `	if( pBlock == 0 ){` |
|       - | 3334 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 3335 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3336 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 | 3337 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3338 | `				return SXERR_ABORT;` |
|       - | 3339 | `			}` |
|     ! 0 | 3340 | `			goto Synchronize;` |
|       - | 3341 | `		}` |
|       - | 3342 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 3343 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 3344 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3345 | `			return SXERR_ABORT;` |
|     ! 0 | 3346 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 3347 | `			/* Emit the POP instruction */` |
|     ! 0 | 3348 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 3349 | `		}` |
|     ! 0 | 3350 | `		return SXRET_OK;` |
|       - | 3351 | `	}` |
|       3 | 3352 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 3353 | `	/* Make sure we are dealing with a valid statement */` |
|       3 | 3354 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 | 3355 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 | 3356 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 | 3357 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3358 | `				return SXERR_ABORT;` |
|       - | 3359 | `			}` |
|       3 | 3360 | `			goto Synchronize;` |
|       - | 3361 | `	}` |
|     ! 0 | 3362 | `	pGen->pIn++;` |
|       - | 3363 | `	/* Extract variable name */` |
|     ! 0 | 3364 | `	pName = &pGen->pIn->sData;` |
|     ! 0 | 3365 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 | 3366 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 3367 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3368 | `		goto Synchronize;` |
|       - | 3369 | `	}` |
|       - | 3370 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 | 3371 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 | 3372 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 3373 | `	/* Duplicate variable name */` |
|     ! 0 | 3374 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 | 3375 | `	if( zDup == 0 ){` |
|     ! 0 | 3376 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3377 | `		return SXERR_ABORT;` |
|       - | 3378 | `	}` |
|     ! 0 | 3379 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 3380 | `	/* Check if we have an expression to compile */` |
|     ! 0 | 3381 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 3382 | `		SySet *pInstrContainer;` |
|       - | 3383 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 3384 | `		 * Static variable can take any complex expression including function` |
|       - | 3385 | `		 * call as their initialization value.` |
|       - | 3386 | `		 * Example:` |
|       - | 3387 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 3388 | `		 */` |
|     ! 0 | 3389 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 3390 | `		/* Swap bytecode container */` |
|     ! 0 | 3391 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 | 3392 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 3393 | `		/* Compile the expression */` |
|     ! 0 | 3394 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3395 | `		/* Emit the done instruction */` |
|     ! 0 | 3396 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 3397 | `		/* Restore default bytecode container */` |
|     ! 0 | 3398 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 3399 | `	}` |
|       - | 3400 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 | 3401 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 | 3402 | `	return SXRET_OK;` |
|       1 | 3403 | `Synchronize:` |
|       - | 3404 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 3405 | `	 * statement.` |
|       - | 3406 | `	 */` |
|       5 | 3407 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 | 3408 | `		pGen->pIn++;` |
|       1 | 3409 | `	}` |
|       3 | 3410 | `	return SXRET_OK;` |
|       2 | 3411 |  |
|       - | 3412 | `/*` |
|       - | 3413 | ` * Compile the var statement.` |
|       - | 3414 | ` * Symisc Extension:` |
|       - | 3415 | ` *      var statement can be used outside of a class definition.` |
|       - | 3416 | ` */` |
|       4 | 3417 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 3418 |  |
|       - | 3419 | `	sxu32 nLine;` |
|       - | 3420 | `	sxi32 rc;` |
|       5 | 3421 | `	nLine = pGen->pIn->nLine;` |
|       - | 3422 | `	/* Jump the 'var' keyword */` |
|       5 | 3423 | `	pGen->pIn++;` |
|       5 | 3424 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 3425 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - | 3426 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 | 3427 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 | 3428 | `			pGen->pIn++;` |
|     ! 0 | 3429 | `		}` |
|     ! 0 | 3430 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3431 | `			return SXERR_ABORT;` |
|       - | 3432 | `		}` |
|     ! 0 | 3433 | `	}else{` |
|       - | 3434 | `		/* Compile the expression */` |
|       5 | 3435 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 | 3436 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3437 | `			return SXERR_ABORT;` |
|       5 | 3438 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 | 3439 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 3440 | `		}` |
|       - | 3441 | `	}` |
|       5 | 3442 | `	return SXRET_OK;` |
|       3 | 3443 |  |
|       - | 3444 | `/*` |
|       - | 3445 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 3446 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 3447 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 3448 | ` */` |
|       - | 3449 | `/*` |
|       - | 3450 | ` * Namespace-qualify a name for CALL/NEW instructions.` |
|       - | 3451 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 3452 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 3453 | ` * qualified name and updates the instruction's operand index.` |
|       - | 3454 | ` *` |
|       - | 3455 | ` * Resolution: use imports -> current NS prefix.` |
|       - | 3456 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 3457 | ` * Returns the (possibly new) literal index.` |
|       - | 3458 | ` */` |
|  246888 | 3459 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx)` |
|       2 | 3460 |  |
|       - | 3461 | `	ph7_value *pLit;` |
|       - | 3462 | `	const char *zLit;` |
|       - | 3463 | `	SyString sQualified;` |
|       - | 3464 | `	sxu32 nLit;` |
|       - | 3465 | `	sxu32 k;` |
|       - | 3466 | `	sxu32 nNewIdx;` |
|       - | 3467 | `	int hasNsSep;` |
|       - | 3468 | `	SyHashEntry *pImport;` |
|       - | 3469 | `	ph7_value *pNew;` |
|  246890 | 3470 | `	if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  246784 | 3471 | `		return nOrigIdx; /* Not in a namespace */` |
|       - | 3472 | `	}` |
|     107 | 3473 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|     107 | 3474 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 3475 | `		return nOrigIdx;` |
|       - | 3476 | `	}` |
|     107 | 3477 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|     107 | 3478 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 3479 | `	/* Skip if already qualified (contains backslash) */` |
|     107 | 3480 | `	hasNsSep = 0;` |
|     521 | 3481 | `	for( k = 0; k < nLit; k++ ){` |
|     465 | 3482 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|     208 | 3483 | `	}` |
|     107 | 3484 | `	if( hasNsSep ){` |
|      51 | 3485 | `		return nOrigIdx;` |
|       - | 3486 | `	}` |
|       - | 3487 | `	/* Build the qualified name into sWorker */` |
|      57 | 3488 | `	SyBlobReset(&pGen->sWorker);` |
|       - | 3489 | `	/* Check use imports first */` |
|      57 | 3490 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zLit,nLit);` |
|      57 | 3491 | `	if( pImport ){` |
|      15 | 3492 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      15 | 3493 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       8 | 3494 | `	}else{` |
|       - | 3495 | `		/* Prepend current namespace */` |
|      43 | 3496 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      43 | 3497 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      43 | 3498 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 3499 | `	}` |
|       - | 3500 | `	/* Look up or create a new literal for the qualified name */` |
|      57 | 3501 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      57 | 3502 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      17 | 3503 | `		return nNewIdx; /* Already interned */` |
|       - | 3504 | `	}` |
|      41 | 3505 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      41 | 3506 | `	if( pNew == 0 ){` |
|     ! 0 | 3507 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 3508 | `	}` |
|      41 | 3509 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      41 | 3510 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      41 | 3511 | `	return nNewIdx;` |
|  123446 | 3512 |  |
|       - | 3513 | `/*` |
|       - | 3514 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 3515 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 3516 | ` */` |
|   14864 | 3517 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3518 |  |
|       - | 3519 | `	SyHashEntry *pImport;` |
|       - | 3520 | `	/* Check use imports first */` |
|   14866 | 3521 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   14866 | 3522 | `	if( pImport ){` |
|       7 | 3523 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       7 | 3524 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       7 | 3525 | `		return;` |
|       - | 3526 | `	}` |
|       - | 3527 | `	/* Prepend current namespace if active */` |
|   14860 | 3528 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 3529 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 3530 | `		SyBlobAppend(pOut,"\\",1);` |
|       1 | 3531 | `	}` |
|   14860 | 3532 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    7434 | 3533 |  |
|       - | 3534 | `/*` |
|       - | 3535 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 3536 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 3537 | ` * The caller must release pOut when done.` |
|       - | 3538 | ` */` |
|   29908 | 3539 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3540 |  |
|   29910 | 3541 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      33 | 3542 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      33 | 3543 | `		SyBlobAppend(pOut,"\\",1);` |
|      16 | 3544 | `	}` |
|   29910 | 3545 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   29910 | 3546 |  |
|       - | 3547 | `/*` |
|       - | 3548 | ` * Compile a namespace statement` |
|       - | 3549 | ` * According to the PHP language reference manual` |
|       - | 3550 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 3551 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 3552 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 3553 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 3554 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 3555 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 3556 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 3557 | ` *  programming world.` |
|       - | 3558 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 3559 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 3560 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 3561 | ` *  classes/functions/constants.` |
|       - | 3562 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 3563 | ` *  readability of source code.` |
|       - | 3564 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 3565 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 3566 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 3567 | ` *       class MyClass {}` |
|       - | 3568 | ` *       function myfunction() {}` |
|       - | 3569 | ` *       const MYCONST = 1;` |
|       - | 3570 | ` *       $a = new MyClass;` |
|       - | 3571 | ` *       $c = new \my\name\MyClass;` |
|       - | 3572 | ` *       $a = strlen('hi');` |
|       - | 3573 | ` *       $d = namespace\MYCONST;` |
|       - | 3574 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 3575 | ` *       echo constant($d);` |
|       - | 3576 | ` * NOTE` |
|       - | 3577 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3578 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3579 | ` */` |
|       - | 3580 | `/*` |
|       - | 3581 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 3582 | ` */` |
|       6 | 3583 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 | 3584 |  |
|       7 | 3585 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|     ! 0 | 3586 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|     ! 0 | 3587 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|     ! 0 | 3588 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|     ! 0 | 3589 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|     ! 0 | 3590 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|     ! 0 | 3591 | `	return "token";` |
|       4 | 3592 |  |
|      50 | 3593 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       1 | 3594 |  |
|       - | 3595 | `	sxu32 nLine;` |
|       - | 3596 | `	sxi32 rc;` |
|      51 | 3597 | `	nLine = pGen->pIn->nLine;` |
|      51 | 3598 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 3599 | `	/* Reset namespace and clear previous use imports */` |
|      51 | 3600 | `	SyBlobReset(&pGen->sNamespace);` |
|      51 | 3601 | `	SyHashRelease(&pGen->hUseImports);` |
|      51 | 3602 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      51 | 3603 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3604 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 | 3605 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3606 | `		return SXRET_OK;` |
|       - | 3607 | `	}` |
|      51 | 3608 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - | 3609 | `		/* namespace; — switch to global namespace */` |
|     ! 0 | 3610 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3611 | `		return SXRET_OK;` |
|       - | 3612 | `	}` |
|      51 | 3613 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - | 3614 | `		/* namespace { } — global namespace block */` |
|     ! 0 | 3615 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3616 | `		return SXRET_OK;` |
|       - | 3617 | `	}` |
|       - | 3618 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     131 | 3619 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      81 | 3620 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 3621 | `			/* Append backslash separator */` |
|      17 | 3622 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      17 | 3623 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       8 | 3624 | `			}` |
|       9 | 3625 | `		}else{` |
|       - | 3626 | `			/* Append identifier */` |
|      65 | 3627 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 3628 | `		}` |
|      81 | 3629 | `		pGen->pIn++;` |
|       1 | 3630 | `	}` |
|       - | 3631 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - | 3632 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - | 3633 | `	{` |
|      51 | 3634 | `		char *zNsDup = 0;` |
|      51 | 3635 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      73 | 3636 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      48 | 3637 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      24 | 3638 | `		}` |
|      51 | 3639 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - | 3640 | `	}` |
|      51 | 3641 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 | 3642 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3643 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 3644 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 | 3645 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3646 | `			return SXERR_ABORT;` |
|       - | 3647 | `		}` |
|       2 | 3648 | `	}` |
|      51 | 3649 | `	return SXRET_OK;` |
|      26 | 3650 |  |
|       - | 3651 | `/*` |
|       - | 3652 | ` * Compile the 'use' statement` |
|       - | 3653 | ` * According to the PHP language reference manual` |
|       - | 3654 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 3655 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 3656 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 3657 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 3658 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 3659 | ` *  a function or constant is not supported.` |
|       - | 3660 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 3661 | ` * NOTE` |
|       - | 3662 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3663 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3664 | ` */` |
|      22 | 3665 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       1 | 3666 |  |
|       - | 3667 | `	sxu32 nLine;` |
|       - | 3668 | `	sxi32 rc;` |
|       - | 3669 | `	SyBlob sPath;` |
|       - | 3670 | `	SyString sAlias;` |
|       - | 3671 | `	SyToken *pLast;` |
|       - | 3672 | `	char *zDup;` |
|      23 | 3673 | `	nLine = pGen->pIn->nLine;` |
|      23 | 3674 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|      23 | 3675 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 3676 | `	/* Process one or more use declarations separated by commas */` |
|      12 | 3677 | `	for(;;){` |
|      25 | 3678 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3679 | `			break;` |
|       - | 3680 | `		}` |
|      25 | 3681 | `		SyBlobReset(&sPath);` |
|      25 | 3682 | `		pLast = 0;` |
|       - | 3683 | `		/* Collect the full namespace path */` |
|     101 | 3684 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      77 | 3685 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      49 | 3686 | `				pLast = pGen->pIn;` |
|      49 | 3687 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      29 | 3688 | `					SyBlobAppend(&sPath,"\\",1);` |
|      14 | 3689 | `				}` |
|      49 | 3690 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      24 | 3691 | `			}` |
|      77 | 3692 | `			pGen->pIn++;` |
|       1 | 3693 | `		}` |
|      25 | 3694 | `		if( pLast == 0 ){` |
|       - | 3695 | `			/* Empty path */` |
|       5 | 3696 | `			break;` |
|       - | 3697 | `		}` |
|       - | 3698 | `		/* Default alias is the last component of the path */` |
|      21 | 3699 | `		sAlias = pLast->sData;` |
|       - | 3700 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      20 | 3701 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      13 | 3702 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       5 | 3703 | `			pGen->pIn++; /* Jump 'as' */` |
|       5 | 3704 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       5 | 3705 | `				sAlias = pGen->pIn->sData;` |
|       5 | 3706 | `				pGen->pIn++;` |
|       2 | 3707 | `			}` |
|       2 | 3708 | `		}` |
|       - | 3709 | `		/* Register the import: alias -> FQN.` |
|       - | 3710 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - | 3711 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - | 3712 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      31 | 3713 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      20 | 3714 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      21 | 3715 | `		if( zDup ){` |
|       - | 3716 | `			char *zAliasDup;` |
|      21 | 3717 | `			SyHashInsert(&pGen->hUseImports,sAlias.zString,sAlias.nByte,zDup);` |
|       - | 3718 | `			/* Duplicate the alias key for the VM hash (token pointers may not survive to runtime) */` |
|      21 | 3719 | `			zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      21 | 3720 | `			if( zAliasDup ){` |
|      21 | 3721 | `				SyHashInsert(&pGen->pVm->hUseImports,zAliasDup,sAlias.nByte,zDup);` |
|      10 | 3722 | `			}` |
|      10 | 3723 | `		}` |
|       - | 3724 | `		/* Check for comma (multiple use declarations) */` |
|      21 | 3725 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3726 | `			pGen->pIn++;` |
|       2 | 3727 | `		}else{` |
|      10 | 3728 | `			break;` |
|       - | 3729 | `		}` |
|       1 | 3730 | `	}` |
|      23 | 3731 | `	SyBlobRelease(&sPath);` |
|      23 | 3732 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 3733 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 3734 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 3735 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3736 | `			return SXERR_ABORT;` |
|       - | 3737 | `		}` |
|       1 | 3738 | `	}` |
|      23 | 3739 | `	return SXRET_OK;` |
|      12 | 3740 |  |
|       - | 3741 | `/*` |
|       - | 3742 | ` * Compile the stupid 'declare' language construct.` |
|       - | 3743 | ` *` |
|       - | 3744 | ` * According to the PHP language reference manual.` |
|       - | 3745 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 3746 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 3747 | ` *  declare (directive)` |
|       - | 3748 | ` *   statement` |
|       - | 3749 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 3750 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 3751 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 3752 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 3753 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 3754 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 3755 | ` * <?php` |
|       - | 3756 | ` * // these are the same:` |
|       - | 3757 | ` * // you can use this:` |
|       - | 3758 | ` * declare(ticks=1) {` |
|       - | 3759 | ` *   // entire script here` |
|       - | 3760 | ` * }` |
|       - | 3761 | ` * // or you can use this:` |
|       - | 3762 | ` * declare(ticks=1);` |
|       - | 3763 | ` * // entire script here` |
|       - | 3764 | ` * ?>` |
|       - | 3765 | ` *` |
|       - | 3766 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 3767 | ` */` |
|       8 | 3768 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 3769 |  |
|       9 | 3770 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 3771 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 3772 | `	sxi32 rc;` |
|       9 | 3773 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 3774 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 3775 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 3776 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3777 | `			return SXERR_ABORT;` |
|       - | 3778 | `		}` |
|       5 | 3779 | `		goto Synchro;` |
|       - | 3780 | `	}` |
|       5 | 3781 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 3782 | `	/* Delimit the directive */` |
|       5 | 3783 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 3784 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 3785 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 3786 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3787 | `			return SXERR_ABORT;` |
|       - | 3788 | `		}` |
|     ! 0 | 3789 | `		return SXRET_OK;` |
|       - | 3790 | `	}` |
|       - | 3791 | `	/* Update the cursor */` |
|       5 | 3792 | `	pGen->pIn = &pEnd[1];` |
|       5 | 3793 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 3794 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 3795 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3796 | `			return SXERR_ABORT;` |
|       - | 3797 | `		}` |
|     ! 0 | 3798 | `	}` |
|       - | 3799 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 3800 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 3801 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 3802 | `		ph7_lib_version()` |
|       - | 3803 | `		);` |
|       - | 3804 | `	/*All done */` |
|       5 | 3805 | `	return SXRET_OK;` |
|       2 | 3806 | `Synchro:` |
|       - | 3807 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 3808 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 3809 | `		pGen->pIn++;` |
|       1 | 3810 | `	}` |
|       5 | 3811 | `	return SXRET_OK;` |
|       5 | 3812 |  |
|       - | 3813 | `/*` |
|       - | 3814 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 3815 | ` * as follows:` |
|       - | 3816 | ` * function makecoffee($type = "cappuccino")` |
|       - | 3817 | ` * {` |
|       - | 3818 | ` *   return "Making a cup of $type.\n";` |
|       - | 3819 | ` * }` |
|       - | 3820 | ` * Symisc eXtension.` |
|       - | 3821 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 3822 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 3823 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 3824 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 3825 | ` *      {` |
|       - | 3826 | ` *       var_dump($a);` |
|       - | 3827 | ` *      }` |
|       - | 3828 | ` *     //call test without args` |
|       - | 3829 | ` *      test();` |
|       - | 3830 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 3831 | ` *      Example:` |
|       - | 3832 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 3833 | ` * 3 -) Function overloading!!` |
|       - | 3834 | ` *      Example:` |
|       - | 3835 | ` *      function foo($a) {` |
|       - | 3836 | ` *   	  return $a.PHP_EOL;` |
|       - | 3837 | ` *	    }` |
|       - | 3838 | ` *	    function foo($a, $b) {` |
|       - | 3839 | ` *   	  return $a + $b;` |
|       - | 3840 | ` *	    }` |
|       - | 3841 | ` *	    echo foo(5); // Prints "5"` |
|       - | 3842 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 3843 | ` *      // Same arg` |
|       - | 3844 | ` *	   function foo(string $a)` |
|       - | 3845 | ` *	   {` |
|       - | 3846 | ` *	     echo "a is a string\n";` |
|       - | 3847 | ` *	     var_dump($a);` |
|       - | 3848 | ` *	   }` |
|       - | 3849 | ` *	  function foo(int $a)` |
|       - | 3850 | ` *	  {` |
|       - | 3851 | ` *	    echo "a is integer\n";` |
|       - | 3852 | ` *	    var_dump($a);` |
|       - | 3853 | ` *	  }` |
|       - | 3854 | ` *	  function foo(array $a)` |
|       - | 3855 | ` *	  {` |
|       - | 3856 | ` * 	    echo "a is an array\n";` |
|       - | 3857 | ` * 	    var_dump($a);` |
|       - | 3858 | ` *	  }` |
|       - | 3859 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 3860 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 3861 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 3862 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 3863 | ` * introduced by the PH7 engine.` |
|       - | 3864 | ` */` |
|   31988 | 3865 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 3866 |  |
|       - | 3867 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 3868 | `	SySet *pInstrContainer;` |
|       - | 3869 | `	sxi32 rc;` |
|       - | 3870 | `	/* Swap token stream */` |
|   31990 | 3871 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   31990 | 3872 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   31990 | 3873 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 3874 | `	/* Compile the expression holding the argument value */` |
|   31990 | 3875 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3876 | `	/* Emit the done instruction */` |
|   31990 | 3877 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   31990 | 3878 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   31990 | 3879 | `	RE_SWAP_DELIMITER(pGen);` |
|   31990 | 3880 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3881 | `		return SXERR_ABORT;` |
|       - | 3882 | `	}` |
|   31990 | 3883 | `	return SXRET_OK;` |
|   15996 | 3884 |  |
|       - | 3885 | `/*` |
|       - | 3886 | ` * Collect function arguments one after one.` |
|       - | 3887 | ` * According to the PHP language reference manual.` |
|       - | 3888 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 3889 | ` * list of expressions.` |
|       - | 3890 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 3891 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 3892 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 3893 | ` * for more information.` |
|       - | 3894 | ` * Example #1 Passing arrays to functions` |
|       - | 3895 | ` * <?php` |
|       - | 3896 | ` * function takes_array($input)` |
|       - | 3897 | ` * {` |
|       - | 3898 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 3899 | ` * }` |
|       - | 3900 | ` * ?>` |
|       - | 3901 | ` * Making arguments be passed by reference` |
|       - | 3902 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 3903 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 3904 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 3905 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 3906 | ` * to the argument name in the function definition:` |
|       - | 3907 | ` * Example #2 Passing function parameters by reference` |
|       - | 3908 | ` * <?php` |
|       - | 3909 | ` * function add_some_extra(&$string)` |
|       - | 3910 | ` * {` |
|       - | 3911 | ` *   $string .= 'and something extra.';` |
|       - | 3912 | ` * }` |
|       - | 3913 | ` * $str = 'This is a string, ';` |
|       - | 3914 | ` * add_some_extra($str);` |
|       - | 3915 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 3916 | ` * ?>` |
|       - | 3917 | ` *` |
|       - | 3918 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 3919 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 3920 | ` * on these extension.` |
|       - | 3921 | ` */` |
|   34896 | 3922 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 3923 |  |
|       - | 3924 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 3925 | `	SyToken *pIn;  /* Token stream */` |
|       - | 3926 | `	SyBlob sSig;         /* Function signature */` |
|       - | 3927 | `	char *zDup;          /* Copy of argument name */` |
|       - | 3928 | `	sxi32 rc;` |
|       - | 3929 |  |
|   34898 | 3930 | `	pIn = pGen->pIn;` |
|   34898 | 3931 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 3932 | `	/* Process arguments one after one */` |
|   47428 | 3933 | `	for(;;){` |
|   94858 | 3934 | `		if( pIn >= pEnd ){` |
|       - | 3935 | `			/* No more arguments to process */` |
|   34896 | 3936 | `			break;` |
|       - | 3937 | `		}` |
|   59964 | 3938 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   59964 | 3939 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   59964 | 3940 | `		if( pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   49204 | 3941 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   44284 | 3942 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   44284 | 3943 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 3944 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   44284 | 3945 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 3946 | `					sArg.nType = MEMOBJ_BOOL;` |
|   44284 | 3947 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   12302 | 3948 | `					sArg.nType = MEMOBJ_INT;` |
|   38134 | 3949 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   31982 | 3950 | `					sArg.nType = MEMOBJ_STRING;` |
|   15993 | 3951 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 3952 | `					sArg.nType = MEMOBJ_REAL;` |
|     ! 0 | 3953 | `				}else{` |
|       4 | 3954 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 3955 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 3956 | `						&pIn->sData);` |
|       - | 3957 | `				}` |
|   22143 | 3958 | `			}else{` |
|    4922 | 3959 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 3960 | `				char *zDupLocal;` |
|       - | 3961 | `				/* Argument must be a class instance,record that*/` |
|    4922 | 3962 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    4922 | 3963 | `				if( zDupLocal ){` |
|    4922 | 3964 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    4922 | 3965 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2460 | 3966 | `				}` |
|       - | 3967 | `			}` |
|   49204 | 3968 | `			pIn++;` |
|   24601 | 3969 | `		}` |
|   59964 | 3970 | `		if( pIn >= pEnd ){` |
|     ! 0 | 3971 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 3972 | `			return rc;` |
|       - | 3973 | `		}` |
|   59964 | 3974 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 3975 | `			/* Pass by reference,record that */` |
|    2482 | 3976 | `			sArg.iFlags = VM_FUNC_ARG_BY_REF;` |
|    2482 | 3977 | `			pIn++;` |
|    1240 | 3978 | `		}` |
|   59964 | 3979 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 3980 | `			/* Invalid argument */` |
|     ! 0 | 3981 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 3982 | `			return rc;` |
|       - | 3983 | `		}` |
|   59964 | 3984 | `		pIn++; /* Jump the dollar sign */` |
|       - | 3985 | `		/* Copy argument name */` |
|   59964 | 3986 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   59964 | 3987 | `		if( zDup == 0 ){` |
|     ! 0 | 3988 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 3989 | `			return SXERR_ABORT;` |
|       - | 3990 | `		}` |
|   59964 | 3991 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   59964 | 3992 | `		pIn++;` |
|   59964 | 3993 | `		if( pIn < pEnd ){` |
|   37380 | 3994 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 3995 | `				SyToken *pDefend;` |
|   31992 | 3996 | `				sxi32 iNest = 0;` |
|   31992 | 3997 | `				pIn++; /* Jump the equal sign */` |
|   31992 | 3998 | `				pDefend = pIn;` |
|       - | 3999 | `				/* Process the default value associated with this argument */` |
|   68900 | 4000 | `				while( pDefend < pEnd ){` |
|   56590 | 4001 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   19682 | 4002 | `						break;` |
|       - | 4003 | `					}` |
|   36910 | 4004 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 4005 | `						/* Increment nesting level */` |
|    2462 | 4006 | `						iNest++;` |
|   35680 | 4007 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 4008 | `						/* Decrement nesting level */` |
|    2462 | 4009 | `						iNest--;` |
|    1230 | 4010 | `					}` |
|   36910 | 4011 | `					pDefend++;` |
|       2 | 4012 | `				}` |
|   31992 | 4013 | `				if( pIn >= pDefend ){` |
|       3 | 4014 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 4015 | `					return rc;` |
|       - | 4016 | `				}` |
|       - | 4017 | `				/* Process default value */` |
|   31990 | 4018 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   31990 | 4019 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 4020 | `					return rc;` |
|       - | 4021 | `				}` |
|       - | 4022 | `				/* Point beyond the default value */` |
|   31990 | 4023 | `				pIn = pDefend;` |
|   15994 | 4024 | `			}` |
|   37378 | 4025 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 4026 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 4027 | `				return rc;` |
|       - | 4028 | `			}` |
|   37378 | 4029 | `			pIn++; /* Jump the trailing comma */` |
|   18688 | 4030 | `		}` |
|       - | 4031 | `		/* Append argument signature */` |
|   59962 | 4032 | `		if( sArg.nType > 0 ){` |
|   49202 | 4033 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 4034 | `				/* Class name */` |
|    4922 | 4035 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2462 | 4036 | `			}else{` |
|       - | 4037 | `				int c;` |
|   44282 | 4038 | `				c = 'n'; /* cc warning */` |
|       - | 4039 | `				/* Type leading character */` |
|   44282 | 4040 | `				switch(sArg.nType){` |
|     ! 0 | 4041 | `				case MEMOBJ_HASHMAP:` |
|       - | 4042 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 4043 | `					c = 'h';` |
|     ! 0 | 4044 | `					break;` |
|    6150 | 4045 | `				case MEMOBJ_INT:` |
|       - | 4046 | `					/* Integer */` |
|   12302 | 4047 | `					c = 'i';` |
|   12302 | 4048 | `					break;` |
|     ! 0 | 4049 | `				case MEMOBJ_BOOL:` |
|       - | 4050 | `					/* Bool */` |
|     ! 0 | 4051 | `					c = 'b';` |
|     ! 0 | 4052 | `					break;` |
|     ! 0 | 4053 | `				case MEMOBJ_REAL:` |
|       - | 4054 | `					/* Float */` |
|     ! 0 | 4055 | `					c = 'f';` |
|     ! 0 | 4056 | `					break;` |
|   15990 | 4057 | `				case MEMOBJ_STRING:` |
|       - | 4058 | `					/* String */` |
|   31982 | 4059 | `					c = 's';` |
|   31980 | 4060 | `					break;` |
|     ! 0 | 4061 | `				default:` |
|     ! 0 | 4062 | `					break;` |
|       - | 4063 | `				}` |
|   44282 | 4064 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 4065 | `			}` |
|   24602 | 4066 | `		}else{` |
|       - | 4067 | `			/* No type is associated with this parameter which mean` |
|       - | 4068 | `			 * that this function is not condidate for overloading.` |
|       - | 4069 | `			 */` |
|   10762 | 4070 | `			SyBlobRelease(&sSig);` |
|       - | 4071 | `		}` |
|       - | 4072 | `		/* Save in the argument set */` |
|   59962 | 4073 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 4074 | `	}` |
|   34896 | 4075 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 4076 | `		/* Save function signature */` |
|   29522 | 4077 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   14760 | 4078 | `	}` |
|   34896 | 4079 | `	return SXRET_OK;` |
|   17450 | 4080 |  |
|       - | 4081 | `/*` |
|       - | 4082 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 4083 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 4084 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 4085 | ` */` |
|   84484 | 4086 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 4087 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 4088 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 4089 | `	)` |
|       2 | 4090 |  |
|       - | 4091 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 4092 | `	GenBlock *pBlock;` |
|       - | 4093 | `	sxu32 nGotoOfft;` |
|       - | 4094 | `	sxi32 rc;` |
|       - | 4095 | `	/* Attach the new function */` |
|   84486 | 4096 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   84486 | 4097 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4098 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 4099 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4100 | `		return SXERR_ABORT;` |
|       - | 4101 | `	}` |
|   84486 | 4102 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 4103 | `	/* Swap bytecode containers */` |
|   84486 | 4104 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   84486 | 4105 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 4106 | `	/* Compile the body */` |
|   84486 | 4107 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 4108 | `	/* Fix exception jumps now the destination is resolved */` |
|   84486 | 4109 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4110 | `	/* Emit the final return if not yet done */` |
|   84486 | 4111 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4112 | `	/* Fix gotos jumps now the destination is resolved */` |
|   84486 | 4113 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 4114 | `		rc = SXERR_ABORT;` |
|     ! 0 | 4115 | `	}` |
|   84486 | 4116 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 4117 | `	/* Restore the default container */` |
|   84486 | 4118 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4119 | `	/* Leave function block */` |
|   84486 | 4120 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   84486 | 4121 | `	if( rc == SXERR_ABORT ){` |
|       - | 4122 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4123 | `		return SXERR_ABORT;` |
|       - | 4124 | `	}` |
|       - | 4125 | `	/* All done, function body compiled */` |
|   84486 | 4126 | `	return SXRET_OK;` |
|   42244 | 4127 |  |
|       - | 4128 | `/*` |
|       - | 4129 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 4130 | ` * According to the PHP language reference manual.` |
|       - | 4131 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 4132 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 4133 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 4134 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4135 | ` *  Functions need not be defined before they are referenced.` |
|       - | 4136 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 4137 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 4138 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 4139 | ` *  calls with over 32-64 recursion levels.` |
|       - | 4140 | ` *` |
|       - | 4141 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 4142 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 4143 | ` * on these extension.` |
|       - | 4144 | ` */` |
|   32484 | 4145 | `static sxi32 GenStateCompileFunc(` |
|       - | 4146 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4147 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 4148 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 4149 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 4150 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 4151 | `	)` |
|       2 | 4152 |  |
|       - | 4153 | `	ph7_vm_func *pFunc;` |
|       - | 4154 | `	SyToken *pEnd;` |
|       - | 4155 | `	sxu32 nLine;` |
|       - | 4156 | `	char *zName;` |
|       - | 4157 | `	sxi32 rc;` |
|       - | 4158 | `	/* Extract line number */` |
|   32486 | 4159 | `	nLine = pGen->pIn->nLine;` |
|       - | 4160 | `	/* Jump the left parenthesis '(' */` |
|   32486 | 4161 | `	pGen->pIn++;` |
|       - | 4162 | `	/* Delimit the function signature */` |
|   32486 | 4163 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   32486 | 4164 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4165 | `		/* Syntax error */` |
|       7 | 4166 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 4167 | `		if( rc == SXERR_ABORT ){` |
|       - | 4168 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4169 | `			return SXERR_ABORT;` |
|       - | 4170 | `		}` |
|       7 | 4171 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 4172 | `		return SXRET_OK;` |
|       - | 4173 | `	}` |
|       - | 4174 | `	/* Create the function state */` |
|   32480 | 4175 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   32480 | 4176 | `	if( pFunc == 0 ){` |
|     ! 0 | 4177 | `		goto OutOfMem;` |
|       - | 4178 | `	}` |
|       - | 4179 | `	/* Build the function name, prepending namespace if active */` |
|   32484 | 4180 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 4181 | `		SyBlob sFQN;` |
|       - | 4182 | `		sxu32 nLen;` |
|       9 | 4183 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       9 | 4184 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       9 | 4185 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       9 | 4186 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       9 | 4187 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       9 | 4188 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       9 | 4189 | `		SyBlobRelease(&sFQN);` |
|       9 | 4190 | `		if( zName == 0 ){` |
|     ! 0 | 4191 | `			goto OutOfMem;` |
|       - | 4192 | `		}` |
|       9 | 4193 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       5 | 4194 | `	}else{` |
|   32472 | 4195 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   32472 | 4196 | `		if( zName == 0 ){` |
|     ! 0 | 4197 | `			goto OutOfMem;` |
|       - | 4198 | `		}` |
|   32472 | 4199 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 4200 | `	}` |
|   32480 | 4201 | `	if( pGen->pIn < pEnd ){` |
|       - | 4202 | `		/* Collect function arguments */` |
|   22534 | 4203 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   22534 | 4204 | `		if( rc == SXERR_ABORT ){` |
|       - | 4205 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4206 | `			return SXERR_ABORT;` |
|       - | 4207 | `		}` |
|   11266 | 4208 | `	}` |
|       - | 4209 | `	/* Compile function body */` |
|   32480 | 4210 | `	pGen->pIn = &pEnd[1];` |
|   32480 | 4211 | `	if( bHandleClosure ){` |
|       - | 4212 | `		ph7_vm_func_closure_env sEnv;` |
|     130 | 4213 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     128 | 4214 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      70 | 4215 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      10 | 4216 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 4217 | `				/* Closure,record environment variable */` |
|      10 | 4218 | `				pGen->pIn++;` |
|      10 | 4219 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 4220 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 4221 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4222 | `						return SXERR_ABORT;` |
|       - | 4223 | `					}` |
|     ! 0 | 4224 | `				}` |
|      10 | 4225 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 4226 | `				/* Compile until we hit the first closing parenthesis */` |
|      18 | 4227 | `				while( pGen->pIn < pGen->pEnd ){` |
|      18 | 4228 | `					int iFlagsLocal = 0;` |
|      18 | 4229 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      10 | 4230 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      10 | 4231 | `						break;` |
|       - | 4232 | `					}` |
|      10 | 4233 | `					nLineLocal = pGen->pIn->nLine;` |
|      10 | 4234 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 4235 | `						/* Pass by reference,record that */` |
|     ! 0 | 4236 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 4237 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 4238 | `							);` |
|     ! 0 | 4239 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 4240 | `						pGen->pIn++;` |
|     ! 0 | 4241 | `					}` |
|       8 | 4242 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      10 | 4243 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 4244 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 4245 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 4246 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4247 | `								return SXERR_ABORT;` |
|       - | 4248 | `							}` |
|       - | 4249 | `							/* Find the closing parenthesis */` |
|     ! 0 | 4250 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 4251 | `								pGen->pIn++;` |
|     ! 0 | 4252 | `							}` |
|     ! 0 | 4253 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 4254 | `								pGen->pIn++;` |
|     ! 0 | 4255 | `							}` |
|     ! 0 | 4256 | `							break;` |
|       - | 4257 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 4258 | `					}else{` |
|       - | 4259 | `						SyString *pNameLocal;` |
|       - | 4260 | `						char *zDup;` |
|       - | 4261 | `						/* Duplicate variable name */` |
|      10 | 4262 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      10 | 4263 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      10 | 4264 | `						if( zDup ){` |
|       - | 4265 | `							/* Zero the structure */` |
|      10 | 4266 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      10 | 4267 | `							sEnv.iFlags = iFlagsLocal;` |
|      10 | 4268 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      10 | 4269 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      10 | 4270 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 4271 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 4272 | `									got_this = 1;` |
|     ! 0 | 4273 | `							}` |
|       - | 4274 | `							/* Save imported variable */` |
|      10 | 4275 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       6 | 4276 | `						}else{` |
|     ! 0 | 4277 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4278 | `							 return SXERR_ABORT;` |
|       - | 4279 | `						}` |
|       - | 4280 | `					}` |
|      10 | 4281 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      10 | 4282 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4283 | `						/* Ignore trailing commas */` |
|     ! 0 | 4284 | `						pGen->pIn++;` |
|     ! 0 | 4285 | `					}` |
|       2 | 4286 | `				}` |
|      10 | 4287 | `				if( !got_this ){` |
|       - | 4288 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 4289 | `					 * available to the closure environment.` |
|       - | 4290 | `					 */` |
|      10 | 4291 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      10 | 4292 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      10 | 4293 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      10 | 4294 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      10 | 4295 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       4 | 4296 | `				}` |
|      10 | 4297 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 4298 | `					/* Mark as closure */` |
|      10 | 4299 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       4 | 4300 | `				}` |
|       4 | 4301 | `		}` |
|      64 | 4302 | `	}` |
|       - | 4303 | `	/* Compile the body */` |
|   32480 | 4304 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   32480 | 4305 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4306 | `		return SXERR_ABORT;` |
|       - | 4307 | `	}` |
|   32480 | 4308 | `	if( ppFunc ){` |
|     130 | 4309 | `		*ppFunc = pFunc;` |
|      64 | 4310 | `	}` |
|   32480 | 4311 | `	rc = SXRET_OK;` |
|   32480 | 4312 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 4313 | `		/* Finally register the function */` |
|   32472 | 4314 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   16235 | 4315 | `	}` |
|   32480 | 4316 | `	if( rc == SXRET_OK ){` |
|   32480 | 4317 | `		return SXRET_OK;` |
|       - | 4318 | `	}` |
|       - | 4319 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 4320 | `OutOfMem:` |
|       - | 4321 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 4322 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 4323 | `	 */` |
|     ! 0 | 4324 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 4325 | `	return SXERR_ABORT;` |
|   16244 | 4326 |  |
|       - | 4327 | `/*` |
|       - | 4328 | ` * Compile a standard PHP function.` |
|       - | 4329 | ` *  Refer to the block-comment above for more information.` |
|       - | 4330 | ` */` |
|   32362 | 4331 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 4332 |  |
|       - | 4333 | `	SyString *pName;` |
|       - | 4334 | `	sxi32 iFlags;` |
|       - | 4335 | `	sxu32 nLine;` |
|       - | 4336 | `	sxi32 rc;` |
|       - | 4337 |  |
|   32364 | 4338 | `	nLine = pGen->pIn->nLine;` |
|   32364 | 4339 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   32364 | 4340 | `	iFlags = 0;` |
|   32364 | 4341 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4342 | `		/* Return by reference,remember that */` |
|       7 | 4343 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4344 | `		/* Jump the '&' token */` |
|       7 | 4345 | `		pGen->pIn++;` |
|       3 | 4346 | `	}` |
|   32364 | 4347 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4348 | `		/* Invalid function name */` |
|       5 | 4349 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 4350 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4351 | `			return SXERR_ABORT;` |
|       - | 4352 | `		}` |
|       - | 4353 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 4354 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 4355 | `			pGen->pIn++;` |
|       1 | 4356 | `		}` |
|       5 | 4357 | `		return SXRET_OK;` |
|       - | 4358 | `	}` |
|   32360 | 4359 | `	pName = &pGen->pIn->sData;` |
|   32360 | 4360 | `	nLine = pGen->pIn->nLine;` |
|       - | 4361 | `	/* Jump the function name */` |
|   32360 | 4362 | `	pGen->pIn++;` |
|   32360 | 4363 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4364 | `		/* Syntax error */` |
|       3 | 4365 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 4366 | `		if( rc == SXERR_ABORT ){` |
|       - | 4367 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4368 | `			return SXERR_ABORT;` |
|       - | 4369 | `		}` |
|       - | 4370 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 4371 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4372 | `			pGen->pIn++;` |
|     ! 0 | 4373 | `		}` |
|       3 | 4374 | `		return SXRET_OK;` |
|       - | 4375 | `	}` |
|       - | 4376 | `	/* Compile function body */` |
|   32358 | 4377 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   32358 | 4378 | `	return rc;` |
|   16183 | 4379 |  |
|       - | 4380 | `/*` |
|       - | 4381 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 4382 | ` * According to the PHP language reference manual` |
|       - | 4383 | ` *  Visibility:` |
|       - | 4384 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 4385 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 4386 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 4387 | ` *  Members declared protected can be accessed only within the class` |
|       - | 4388 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 4389 | ` *  may only be accessed by the class that defines the member.` |
|       - | 4390 | ` */` |
|   96518 | 4391 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 4392 |  |
|   96520 | 4393 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|      56 | 4394 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   96466 | 4395 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   17248 | 4396 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 4397 | `	}` |
|       - | 4398 | `	/* Assume public by default */` |
|   79220 | 4399 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   48261 | 4400 |  |
|       - | 4401 | `/*` |
|       - | 4402 | ` * Compile a class constant.` |
|       - | 4403 | ` * According to the PHP language reference manual` |
|       - | 4404 | ` *  Class Constants` |
|       - | 4405 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 4406 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 4407 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 4408 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 4409 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 4410 | ` *   It's also possible for interfaces to have constants.` |
|       - | 4411 | ` * Symisc eXtension.` |
|       - | 4412 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 4413 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4414 | ` *  Example:` |
|       - | 4415 | ` *   class Test{` |
|       - | 4416 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4417 | ` *   };` |
|       - | 4418 | ` *   var_dump(TEST::MyConst);` |
|       - | 4419 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4420 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4421 | ` */` |
|      10 | 4422 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4423 |  |
|      12 | 4424 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4425 | `	SySet *pInstrContainer;` |
|       - | 4426 | `	ph7_class_attr *pCons;` |
|       - | 4427 | `	SyString *pName;` |
|       - | 4428 | `	sxi32 rc;` |
|       - | 4429 | `	/* Extract visibility level */` |
|      12 | 4430 | `	iProtection = GetProtectionLevel(iProtection);` |
|      12 | 4431 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       5 | 4432 | `loop:` |
|       - | 4433 | `	/* Mark as constant */` |
|      12 | 4434 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      12 | 4435 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4436 | `		/* Invalid constant name */` |
|     ! 0 | 4437 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 4438 | `		if( rc == SXERR_ABORT ){` |
|       - | 4439 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4440 | `			return SXERR_ABORT;` |
|       - | 4441 | `		}` |
|     ! 0 | 4442 | `		goto Synchronize;` |
|       - | 4443 | `	}` |
|       - | 4444 | `	/* Peek constant name */` |
|      12 | 4445 | `	pName = &pGen->pIn->sData;` |
|       - | 4446 | `	/* Make sure the constant name isn't reserved */` |
|      12 | 4447 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 4448 | `		/* Reserved constant name */` |
|     ! 0 | 4449 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 4450 | `		if( rc == SXERR_ABORT ){` |
|       - | 4451 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4452 | `			return SXERR_ABORT;` |
|       - | 4453 | `		}` |
|     ! 0 | 4454 | `		goto Synchronize;` |
|       - | 4455 | `	}` |
|       - | 4456 | `	/* Advance the stream cursor */` |
|      12 | 4457 | `	pGen->pIn++;` |
|      12 | 4458 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 4459 | `		/* Invalid declaration */` |
|     ! 0 | 4460 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 4461 | `		if( rc == SXERR_ABORT ){` |
|       - | 4462 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4463 | `			return SXERR_ABORT;` |
|       - | 4464 | `		}` |
|     ! 0 | 4465 | `		goto Synchronize;` |
|       - | 4466 | `	}` |
|      12 | 4467 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 4468 | `	/* Allocate a new class attribute */` |
|      12 | 4469 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      12 | 4470 | `	if( pCons == 0 ){` |
|     ! 0 | 4471 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4472 | `		return SXERR_ABORT;` |
|       - | 4473 | `	}` |
|       - | 4474 | `	/* Swap bytecode container */` |
|      12 | 4475 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      12 | 4476 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 4477 | `	/* Compile constant value.` |
|       - | 4478 | `	 */` |
|      12 | 4479 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      12 | 4480 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 4481 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 4482 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4483 | `			return SXERR_ABORT;` |
|       - | 4484 | `		}` |
|       1 | 4485 | `	}` |
|       - | 4486 | `	/* Emit the done instruction */` |
|      12 | 4487 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      12 | 4488 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      12 | 4489 | `	if( rc == SXERR_ABORT ){` |
|       - | 4490 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4491 | `		return SXERR_ABORT;` |
|       - | 4492 | `	}` |
|       - | 4493 | `	/* All done,install the constant */` |
|      12 | 4494 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      12 | 4495 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4496 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4497 | `		return SXERR_ABORT;` |
|       - | 4498 | `	}` |
|      12 | 4499 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4500 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 4501 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4502 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 4503 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4504 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4505 | `				pTok--;` |
|     ! 0 | 4506 | `			}` |
|     ! 0 | 4507 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4508 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 4509 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4510 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4511 | `				return SXERR_ABORT;` |
|       - | 4512 | `			}` |
|     ! 0 | 4513 | `		}else{` |
|     ! 0 | 4514 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 4515 | `				goto loop;` |
|       - | 4516 | `			}` |
|       - | 4517 | `		}` |
|     ! 0 | 4518 | `	}` |
|      12 | 4519 | `	return SXRET_OK;` |
|     ! 0 | 4520 | `Synchronize:` |
|       - | 4521 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4522 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 4523 | `		pGen->pIn++;` |
|     ! 0 | 4524 | `	}` |
|     ! 0 | 4525 | `	return SXERR_CORRUPT;` |
|       7 | 4526 |  |
|       - | 4527 | `/*` |
|       - | 4528 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 4529 | ` * According to the PHP language reference manual` |
|       - | 4530 | ` *  Properties` |
|       - | 4531 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 4532 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 4533 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 4534 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 4535 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 4536 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 4537 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 4538 | ` * Symisc eXtension.` |
|       - | 4539 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 4540 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4541 | ` *  Example:` |
|       - | 4542 | ` *   class Test{` |
|       - | 4543 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4544 | ` *   };` |
|       - | 4545 | ` *   var_dump(TEST::myVar);` |
|       - | 4546 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4547 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4548 | ` */` |
|   24804 | 4549 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4550 |  |
|   24806 | 4551 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4552 | `	ph7_class_attr *pAttr;` |
|       - | 4553 | `	SyString *pName;` |
|       - | 4554 | `	sxi32 rc;` |
|       - | 4555 | `	/* Extract visibility level */` |
|   24806 | 4556 | `	iProtection = GetProtectionLevel(iProtection);` |
|   12402 | 4557 | `loop:` |
|   24806 | 4558 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   24806 | 4559 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 4560 | `		/* Invalid attribute name */` |
|     ! 0 | 4561 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 4562 | `		if( rc == SXERR_ABORT ){` |
|       - | 4563 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4564 | `			return SXERR_ABORT;` |
|       - | 4565 | `		}` |
|     ! 0 | 4566 | `		goto Synchronize;` |
|       - | 4567 | `	}` |
|       - | 4568 | `	/* Peek attribute name */` |
|   24806 | 4569 | `	pName = &pGen->pIn->sData;` |
|       - | 4570 | `	/* Advance the stream cursor */` |
|   24806 | 4571 | `	pGen->pIn++;` |
|   24806 | 4572 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 4573 | `		/* Invalid declaration */` |
|       3 | 4574 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 4575 | `		if( rc == SXERR_ABORT ){` |
|       - | 4576 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4577 | `			return SXERR_ABORT;` |
|       - | 4578 | `		}` |
|       3 | 4579 | `		goto Synchronize;` |
|       - | 4580 | `	}` |
|       - | 4581 | `	/* Allocate a new class attribute */` |
|   24804 | 4582 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   24804 | 4583 | `	if( pAttr == 0 ){` |
|     ! 0 | 4584 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 4585 | `		return SXERR_ABORT;` |
|       - | 4586 | `	}` |
|   24804 | 4587 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 4588 | `		SySet *pInstrContainer;` |
|    9998 | 4589 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 4590 | `		/* Swap bytecode container */` |
|    9998 | 4591 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    9998 | 4592 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 4593 | `		/* Compile attribute value.` |
|       - | 4594 | `		 */` |
|    9998 | 4595 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    9998 | 4596 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 4597 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 4598 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4599 | `				return SXERR_ABORT;` |
|       - | 4600 | `			}` |
|     ! 0 | 4601 | `		}` |
|       - | 4602 | `		/* Emit the done instruction */` |
|    9998 | 4603 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    9998 | 4604 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    4998 | 4605 | `	}` |
|       - | 4606 | `	/* All done,install the attribute */` |
|   24804 | 4607 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   24804 | 4608 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4609 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4610 | `		return SXERR_ABORT;` |
|       - | 4611 | `	}` |
|   24804 | 4612 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4613 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 4614 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4615 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 4616 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4617 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4618 | `				pTok--;` |
|     ! 0 | 4619 | `			}` |
|     ! 0 | 4620 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4621 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4622 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4623 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4624 | `				return SXERR_ABORT;` |
|       - | 4625 | `			}` |
|     ! 0 | 4626 | `		}else{` |
|     ! 0 | 4627 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 4628 | `				goto loop;` |
|       - | 4629 | `			}` |
|       - | 4630 | `		}` |
|     ! 0 | 4631 | `	}` |
|   24804 | 4632 | `	return SXRET_OK;` |
|       1 | 4633 | `Synchronize:` |
|       - | 4634 | `	/* Synchronize with the first semi-colon */` |
|       5 | 4635 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 4636 | `		pGen->pIn++;` |
|       1 | 4637 | `	}` |
|       3 | 4638 | `	return SXERR_CORRUPT;` |
|   12404 | 4639 |  |
|       - | 4640 | `/*` |
|       - | 4641 | ` * Compile a class method.` |
|       - | 4642 | ` *` |
|       - | 4643 | ` * Refer to the official documentation for more information` |
|       - | 4644 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 4645 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 4646 | ` * overloading and many more.` |
|       - | 4647 | ` */` |
|   71704 | 4648 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 4649 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4650 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 4651 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 4652 | `	int doBody,          /* TRUE to process method body */` |
|       - | 4653 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 4654 | `	)` |
|       2 | 4655 |  |
|   71706 | 4656 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4657 | `	ph7_class_method *pMeth;` |
|       - | 4658 | `	sxi32 iFuncFlags;` |
|       - | 4659 | `	SyString *pName;` |
|       - | 4660 | `	SyToken *pEnd;` |
|       - | 4661 | `	sxi32 rc;` |
|       - | 4662 | `	/* Extract visibility level */` |
|   71706 | 4663 | `	iProtection = GetProtectionLevel(iProtection);` |
|   71706 | 4664 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   71706 | 4665 | `	iFuncFlags = 0;` |
|   71706 | 4666 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4667 | `		/* Invalid method name */` |
|     ! 0 | 4668 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4669 | `		if( rc == SXERR_ABORT ){` |
|       - | 4670 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4671 | `			return SXERR_ABORT;` |
|       - | 4672 | `		}` |
|     ! 0 | 4673 | `		goto Synchronize;` |
|       - | 4674 | `	}` |
|   71706 | 4675 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4676 | `		/* Return by reference,remember that */` |
|     ! 0 | 4677 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4678 | `		/* Jump the '&' token */` |
|     ! 0 | 4679 | `		pGen->pIn++;` |
|     ! 0 | 4680 | `	}` |
|   71706 | 4681 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID)) == 0 ){` |
|       - | 4682 | `		/* Invalid method name */` |
|     ! 0 | 4683 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4684 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4685 | `			return SXERR_ABORT;` |
|       - | 4686 | `		}` |
|     ! 0 | 4687 | `		goto Synchronize;` |
|       - | 4688 | `	}` |
|       - | 4689 | `	/* Peek method name */` |
|   71706 | 4690 | `	pName = &pGen->pIn->sData;` |
|   71706 | 4691 | `	nLine = pGen->pIn->nLine;` |
|       - | 4692 | `	/* Jump the method name */` |
|   71706 | 4693 | `	pGen->pIn++;` |
|   71706 | 4694 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 4695 | `		/* Abstract method */` |
|      10 | 4696 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 4697 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4698 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 4699 | `				&pClass->sName,pName);` |
|     ! 0 | 4700 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4701 | `				return SXERR_ABORT;` |
|       - | 4702 | `			}` |
|     ! 0 | 4703 | `		}` |
|       - | 4704 | `		/* Assemble method signature only */` |
|      10 | 4705 | `		doBody = FALSE;` |
|       4 | 4706 | `	}` |
|   71706 | 4707 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4708 | `		/* Syntax error */` |
|     ! 0 | 4709 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 4710 | `		if( rc == SXERR_ABORT ){` |
|       - | 4711 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4712 | `			return SXERR_ABORT;` |
|       - | 4713 | `		}` |
|     ! 0 | 4714 | `		goto Synchronize;` |
|       - | 4715 | `	}` |
|       - | 4716 | `	/* Allocate a new class_method instance */` |
|   71706 | 4717 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   71706 | 4718 | `	if( pMeth == 0 ){` |
|     ! 0 | 4719 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4720 | `		return SXERR_ABORT;` |
|       - | 4721 | `	}` |
|       - | 4722 | `	/* Jump the left parenthesis '(' */` |
|   71706 | 4723 | `	pGen->pIn++;` |
|   71706 | 4724 | `	pEnd = 0; /* cc warning */` |
|       - | 4725 | `	/* Delimit the method signature */` |
|   71706 | 4726 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   71706 | 4727 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4728 | `		/* Syntax error */` |
|       3 | 4729 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 4730 | `		if( rc == SXERR_ABORT ){` |
|       - | 4731 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4732 | `			return SXERR_ABORT;` |
|       - | 4733 | `		}` |
|       3 | 4734 | `		goto Synchronize;` |
|       - | 4735 | `	}` |
|   71704 | 4736 | `	if( pGen->pIn < pEnd ){` |
|       - | 4737 | `		/* Collect method arguments */` |
|   12366 | 4738 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   12366 | 4739 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4740 | `			return SXERR_ABORT;` |
|       - | 4741 | `		}` |
|    6182 | 4742 | `	}` |
|       - | 4743 | `	/* Point beyond method signature */` |
|   71704 | 4744 | `	pGen->pIn = &pEnd[1];` |
|   71704 | 4745 | `	if( doBody ){` |
|       - | 4746 | `		/* Compile method body */` |
|   52008 | 4747 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   52008 | 4748 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4749 | `			return SXERR_ABORT;` |
|       - | 4750 | `		}` |
|   26005 | 4751 | `	}else{` |
|       - | 4752 | `		/* Only method signature is allowed */` |
|   19698 | 4753 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 4754 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4755 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 4756 | `				if( rc == SXERR_ABORT ){` |
|       - | 4757 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4758 | `					return SXERR_ABORT;` |
|       - | 4759 | `				}` |
|     ! 0 | 4760 | `				return SXERR_CORRUPT;` |
|       - | 4761 | `			}` |
|       - | 4762 | `	}` |
|       - | 4763 | `	/* All done,install the method */` |
|   71704 | 4764 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   71704 | 4765 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4766 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4767 | `		return SXERR_ABORT;` |
|       - | 4768 | `	}` |
|   71704 | 4769 | `	return SXRET_OK;` |
|       1 | 4770 | `Synchronize:` |
|       - | 4771 | `	/* Synchronize with the first semi-colon */` |
|       7 | 4772 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 4773 | `		pGen->pIn++;` |
|       1 | 4774 | `	}` |
|       3 | 4775 | `	return SXERR_CORRUPT;` |
|   35854 | 4776 |  |
|       - | 4777 | `/*` |
|       - | 4778 | ` * Compile an object interface.` |
|       - | 4779 | ` *  According to the PHP language reference manual` |
|       - | 4780 | ` *   Object Interfaces:` |
|       - | 4781 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 4782 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 4783 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 4784 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 4785 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 4786 | ` */` |
|    7392 | 4787 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 4788 |  |
|    7394 | 4789 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4790 | `	ph7_class *pClass,*pBase;` |
|       - | 4791 | `	SyToken *pEnd,*pTmp;` |
|       - | 4792 | `	SyString *pName;` |
|       - | 4793 | `	sxi32 nKwrd;` |
|       - | 4794 | `	sxi32 rc;` |
|       - | 4795 | `	/* Jump the 'interface' keyword */` |
|    7394 | 4796 | `	pGen->pIn++;` |
|       - | 4797 | `	/* Extract interface name */` |
|    7394 | 4798 | `	pName = &pGen->pIn->sData;` |
|       - | 4799 | `	/* Advance the stream cursor */` |
|    7394 | 4800 | `	pGen->pIn++;` |
|       - | 4801 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 4802 | `		SyBlob sFQN;` |
|       - | 4803 | `		SyString sFQNStr;` |
|    7394 | 4804 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    7394 | 4805 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    7394 | 4806 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    7394 | 4807 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    7394 | 4808 | `		SyBlobRelease(&sFQN);` |
|       - | 4809 | `	}` |
|    7394 | 4810 | `	if( pClass == 0 ){` |
|     ! 0 | 4811 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4812 | `		return SXERR_ABORT;` |
|       - | 4813 | `	}` |
|       - | 4814 | `	/* Mark as an interface */` |
|    7394 | 4815 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 4816 | `	/* Assume no base class is given */` |
|    7394 | 4817 | `	pBase = 0;` |
|    7394 | 4818 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 4819 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 4820 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 4821 | `			SyString *pBaseName;` |
|       - | 4822 | `			/* Extract base interface */` |
|       3 | 4823 | `			pGen->pIn++;` |
|       3 | 4824 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4825 | `				/* Syntax error */` |
|     ! 0 | 4826 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4827 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 4828 | `					pName);` |
|     ! 0 | 4829 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4830 | `				if( rc == SXERR_ABORT ){` |
|       - | 4831 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4832 | `					return SXERR_ABORT;` |
|       - | 4833 | `				}` |
|     ! 0 | 4834 | `				return SXRET_OK;` |
|       - | 4835 | `			}` |
|       3 | 4836 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 4837 | `			{` |
|       - | 4838 | `				SyBlob sResolved;` |
|       3 | 4839 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 4840 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 | 4841 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 4842 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 4843 | `				SyBlobRelease(&sResolved);` |
|       - | 4844 | `			}` |
|       - | 4845 | `			/* Only interfaces is allowed */` |
|       3 | 4846 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 4847 | `				pBase = pBase->pNextName;` |
|     ! 0 | 4848 | `			}` |
|       3 | 4849 | `			if( pBase == 0 ){` |
|       - | 4850 | `				/* Inexistant interface */` |
|     ! 0 | 4851 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 4852 | `				if( rc == SXERR_ABORT ){` |
|       - | 4853 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4854 | `					return SXERR_ABORT;` |
|       - | 4855 | `				}` |
|     ! 0 | 4856 | `			}` |
|       - | 4857 | `			/* Advance the stream cursor */` |
|       3 | 4858 | `			pGen->pIn++;` |
|       1 | 4859 | `		}` |
|       1 | 4860 | `	}` |
|    7394 | 4861 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 4862 | `		/* Syntax error */` |
|     ! 0 | 4863 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 4864 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4865 | `		if( rc == SXERR_ABORT ){` |
|       - | 4866 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4867 | `			return SXERR_ABORT;` |
|       - | 4868 | `		}` |
|     ! 0 | 4869 | `		return SXRET_OK;` |
|       - | 4870 | `	}` |
|    7394 | 4871 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    7394 | 4872 | `	pEnd = 0; /* cc warning */` |
|       - | 4873 | `	/* Delimit the interface body */` |
|    7394 | 4874 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    7394 | 4875 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4876 | `		/* Syntax error */` |
|     ! 0 | 4877 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 4878 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4879 | `		if( rc == SXERR_ABORT ){` |
|       - | 4880 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4881 | `			return SXERR_ABORT;` |
|       - | 4882 | `		}` |
|     ! 0 | 4883 | `		return SXRET_OK;` |
|       - | 4884 | `	}` |
|       - | 4885 | `	/* Swap token stream */` |
|    7394 | 4886 | `	pTmp = pGen->pEnd;` |
|    7394 | 4887 | `	pGen->pEnd = pEnd;` |
|       - | 4888 | `	/* Start the parse process` |
|       - | 4889 | `	 * Note (According to the PHP reference manual):` |
|       - | 4890 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 4891 | `	 *  Only 'public' visibility is allowed.` |
|       - | 4892 | `	 */` |
|   13541 | 4893 | `	for(;;){` |
|       - | 4894 | `		/* Jump leading/trailing semi-colons */` |
|   46774 | 4895 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   19692 | 4896 | `			pGen->pIn++;` |
|       2 | 4897 | `		}` |
|   27084 | 4898 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4899 | `			/* End of interface body */` |
|    7394 | 4900 | `			break;` |
|       - | 4901 | `		}` |
|   19692 | 4902 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4903 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4904 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 4905 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 4906 | `			if( rc == SXERR_ABORT ){` |
|       - | 4907 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4908 | `				return SXERR_ABORT;` |
|       - | 4909 | `			}` |
|     ! 0 | 4910 | `			goto done;` |
|       - | 4911 | `		}` |
|       - | 4912 | `		/* Extract the current keyword */` |
|   19692 | 4913 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   19692 | 4914 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 4915 | `			/* Emit a warning and switch to public visibility */` |
|     ! 0 | 4916 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"interface: Access type must be public");` |
|     ! 0 | 4917 | `			nKwrd = PH7_TKWRD_PUBLIC;` |
|     ! 0 | 4918 | `		}` |
|   19692 | 4919 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 4920 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4921 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 4922 | `			if( rc == SXERR_ABORT ){` |
|       - | 4923 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4924 | `				return SXERR_ABORT;` |
|       - | 4925 | `			}` |
|     ! 0 | 4926 | `			goto done;` |
|       - | 4927 | `		}` |
|   19692 | 4928 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 4929 | `			/* Advance the stream cursor */` |
|   19688 | 4930 | `			pGen->pIn++;` |
|   19688 | 4931 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4932 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4933 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 4934 | `				if( rc == SXERR_ABORT ){` |
|       - | 4935 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4936 | `					return SXERR_ABORT;` |
|       - | 4937 | `				}` |
|     ! 0 | 4938 | `				goto done;` |
|       - | 4939 | `			}` |
|   19688 | 4940 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   19688 | 4941 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 4942 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4943 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 4944 | `				if( rc == SXERR_ABORT ){` |
|       - | 4945 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4946 | `					return SXERR_ABORT;` |
|       - | 4947 | `				}` |
|     ! 0 | 4948 | `				goto done;` |
|       - | 4949 | `			}` |
|    9843 | 4950 | `		}` |
|   19692 | 4951 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 4952 | `			/* Parse constant */` |
|       3 | 4953 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 4954 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 4955 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4956 | `					return SXERR_ABORT;` |
|       - | 4957 | `				}` |
|     ! 0 | 4958 | `				goto done;` |
|       - | 4959 | `			}` |
|       2 | 4960 | `		}else{` |
|   19690 | 4961 | `			sxi32 iFlags = 0;` |
|   19690 | 4962 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 4963 | `				/* Static method,record that */` |
|     ! 0 | 4964 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 4965 | `				/* Advance the stream cursor */` |
|     ! 0 | 4966 | `				pGen->pIn++;` |
|     ! 0 | 4967 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 4968 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 4969 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4970 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 4971 | `						if( rc == SXERR_ABORT ){` |
|       - | 4972 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 4973 | `							return SXERR_ABORT;` |
|       - | 4974 | `						}` |
|     ! 0 | 4975 | `						goto done;` |
|       - | 4976 | `				}` |
|     ! 0 | 4977 | `			}` |
|       - | 4978 | `			/* Process method signature */` |
|   19690 | 4979 | `			rc = GenStateCompileClassMethod(&(*pGen),0,FALSE/* Only method signature*/,iFlags,pClass);` |
|   19690 | 4980 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 4981 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4982 | `					return SXERR_ABORT;` |
|       - | 4983 | `				}` |
|     ! 0 | 4984 | `				goto done;` |
|       - | 4985 | `			}` |
|       - | 4986 | `		}` |
|       2 | 4987 | `	}` |
|       - | 4988 | `	/* Install the interface */` |
|    7394 | 4989 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    7394 | 4990 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 4991 | `		/* Inherit from the base interface */` |
|       3 | 4992 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 4993 | `	}` |
|    7394 | 4994 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4995 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4996 | `		return SXERR_ABORT;` |
|       - | 4997 | `	}` |
|    3696 | 4998 | `done:` |
|       - | 4999 | `	/* Point beyond the interface body */` |
|    7394 | 5000 | `	pGen->pIn  = &pEnd[1];` |
|    7394 | 5001 | `	pGen->pEnd = pTmp;` |
|    7394 | 5002 | `	return PH7_OK;` |
|    3698 | 5003 |  |
|       - | 5004 | `/*` |
|       - | 5005 | ` * Compile a user-defined class.` |
|       - | 5006 | ` * According to the PHP language reference manual` |
|       - | 5007 | ` *  class` |
|       - | 5008 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 5009 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 5010 | ` *  of the properties and methods belonging to the class.` |
|       - | 5011 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 5012 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 5013 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 5014 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 5015 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 5016 | ` *  (called "methods").` |
|       - | 5017 | ` */` |
|       - | 5018 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 5019 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 5020 | `struct TraitUseEntry {` |
|       - | 5021 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 5022 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 5023 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 5024 | `};` |
|   22474 | 5025 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 5026 |  |
|   22476 | 5027 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5028 | `	ph7_class *pClass,*pBase;` |
|       - | 5029 | `	SyToken *pEnd,*pTmp;` |
|       - | 5030 | `	sxi32 iProtection;` |
|       - | 5031 | `	SySet aInterfaces;` |
|       - | 5032 | `	SySet aUseEntries;` |
|       - | 5033 | `	sxi32 iAttrflags;` |
|       - | 5034 | `	SyString *pName;` |
|       - | 5035 | `	sxi32 nKwrd;` |
|       - | 5036 | `	sxi32 rc;` |
|       - | 5037 | `	/* Jump the 'class' keyword */` |
|   22476 | 5038 | `	pGen->pIn++;` |
|   22476 | 5039 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5040 | `		/* Syntax error */` |
|     ! 0 | 5041 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 5042 | `		if( rc == SXERR_ABORT ){` |
|       - | 5043 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5044 | `			return SXERR_ABORT;` |
|       - | 5045 | `		}` |
|       - | 5046 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 5047 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 5048 | `			pGen->pIn++;` |
|     ! 0 | 5049 | `		}` |
|     ! 0 | 5050 | `		return SXRET_OK;` |
|       - | 5051 | `	}` |
|       - | 5052 | `	/* Extract class name */` |
|   22476 | 5053 | `	pName = &pGen->pIn->sData;` |
|       - | 5054 | `	/* Advance the stream cursor */` |
|   22476 | 5055 | `	pGen->pIn++;` |
|       - | 5056 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5057 | `		SyBlob sFQN;` |
|       - | 5058 | `		SyString sFQNStr;` |
|   22476 | 5059 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   22476 | 5060 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   22476 | 5061 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   22476 | 5062 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   22476 | 5063 | `		SyBlobRelease(&sFQN);` |
|       - | 5064 | `	}` |
|   22476 | 5065 | `	if( pClass == 0 ){` |
|     ! 0 | 5066 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5067 | `		return SXERR_ABORT;` |
|       - | 5068 | `	}` |
|       - | 5069 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   22476 | 5070 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   22476 | 5071 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 5072 | `	/* Assume a standalone class */` |
|   22476 | 5073 | `	pBase = 0;` |
|   22476 | 5074 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5075 | `		SyString *pBaseName;` |
|   14822 | 5076 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   14822 | 5077 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   14812 | 5078 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   14812 | 5079 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5080 | `				/* Syntax error */` |
|     ! 0 | 5081 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5082 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 5083 | `					pName);` |
|     ! 0 | 5084 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5085 | `				if( rc == SXERR_ABORT ){` |
|       - | 5086 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5087 | `					return SXERR_ABORT;` |
|       - | 5088 | `				}` |
|     ! 0 | 5089 | `				return SXRET_OK;` |
|       - | 5090 | `			}` |
|       - | 5091 | `			/* Extract base class name and resolve through namespace/imports */` |
|   14812 | 5092 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5093 | `			{` |
|       - | 5094 | `				SyBlob sResolved;` |
|   14812 | 5095 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   14812 | 5096 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   22217 | 5097 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   14810 | 5098 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   14812 | 5099 | `				SyBlobRelease(&sResolved);` |
|       - | 5100 | `			}` |
|       - | 5101 | `			/* Interfaces are not allowed */` |
|   14812 | 5102 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 5103 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5104 | `			}` |
|   14812 | 5105 | `			if( pBase == 0 ){` |
|       - | 5106 | `				/* Inexistant base class */` |
|     ! 0 | 5107 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 5108 | `				if( rc == SXERR_ABORT ){` |
|       - | 5109 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5110 | `					return SXERR_ABORT;` |
|       - | 5111 | `				}` |
|     ! 0 | 5112 | `			}else{` |
|   14812 | 5113 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 5114 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 5115 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 5116 | `					if( rc == SXERR_ABORT ){` |
|       - | 5117 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5118 | `						return SXERR_ABORT;` |
|       - | 5119 | `					}` |
|     ! 0 | 5120 | `				}` |
|       - | 5121 | `			}` |
|       - | 5122 | `			/* Advance the stream cursor */` |
|   14812 | 5123 | `			pGen->pIn++;` |
|    7405 | 5124 | `		}` |
|   14822 | 5125 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 5126 | `			ph7_class *pInterface;` |
|       - | 5127 | `			SyString *pIntName;` |
|       - | 5128 | `			/* Interface implementation */` |
|      14 | 5129 | `			pGen->pIn++; /* Advance the stream cursor */` |
|       6 | 5130 | `			for(;;){` |
|      14 | 5131 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5132 | `					/* Syntax error */` |
|     ! 0 | 5133 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5134 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 5135 | `						pName);` |
|     ! 0 | 5136 | `					if( rc == SXERR_ABORT ){` |
|       - | 5137 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5138 | `						return SXERR_ABORT;` |
|       - | 5139 | `					}` |
|     ! 0 | 5140 | `					break;` |
|       - | 5141 | `				}` |
|       - | 5142 | `				/* Extract interface name and resolve through namespace/imports */` |
|      14 | 5143 | `				pIntName = &pGen->pIn->sData;` |
|       - | 5144 | `				{` |
|       - | 5145 | `					SyBlob sResolved;` |
|      14 | 5146 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      14 | 5147 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|      26 | 5148 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|      12 | 5149 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      14 | 5150 | `					SyBlobRelease(&sResolved);` |
|       - | 5151 | `				}` |
|       - | 5152 | `				/* Only interfaces are allowed */` |
|      14 | 5153 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5154 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 5155 | `				}` |
|      14 | 5156 | `				if( pInterface == 0 ){` |
|       - | 5157 | `					/* Inexistant interface */` |
|     ! 0 | 5158 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 5159 | `					if( rc == SXERR_ABORT ){` |
|       - | 5160 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5161 | `						return SXERR_ABORT;` |
|       - | 5162 | `					}` |
|     ! 0 | 5163 | `				}else{` |
|       - | 5164 | `					/* Register interface */` |
|      14 | 5165 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 5166 | `				}` |
|       - | 5167 | `				/* Advance the stream cursor */` |
|      14 | 5168 | `				pGen->pIn++;` |
|      14 | 5169 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       8 | 5170 | `					break;` |
|       - | 5171 | `				}` |
|     ! 0 | 5172 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 5173 | `			}` |
|       6 | 5174 | `		}` |
|    7410 | 5175 | `	}` |
|   22476 | 5176 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5177 | `		/* Syntax error */` |
|     ! 0 | 5178 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 5179 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5180 | `		if( rc == SXERR_ABORT ){` |
|       - | 5181 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5182 | `			return SXERR_ABORT;` |
|       - | 5183 | `		}` |
|     ! 0 | 5184 | `		return SXRET_OK;` |
|       - | 5185 | `	}` |
|   22476 | 5186 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   22476 | 5187 | `	pEnd = 0; /* cc warning */` |
|       - | 5188 | `	/* Delimit the class body */` |
|   22476 | 5189 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   22476 | 5190 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5191 | `		/* Syntax error */` |
|     ! 0 | 5192 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 5193 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5194 | `		if( rc == SXERR_ABORT ){` |
|       - | 5195 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5196 | `			return SXERR_ABORT;` |
|       - | 5197 | `		}` |
|     ! 0 | 5198 | `		return SXRET_OK;` |
|       - | 5199 | `	}` |
|       - | 5200 | `	/* Swap token stream */` |
|   22476 | 5201 | `	pTmp = pGen->pEnd;` |
|   22476 | 5202 | `	pGen->pEnd = pEnd;` |
|       - | 5203 | `	/* Set the inherited flags */` |
|   22476 | 5204 | `	pClass->iFlags = iFlags;` |
|       - | 5205 | `	/* Start the parse process */` |
|   37228 | 5206 | `	for(;;){` |
|       - | 5207 | `		/* Jump leading/trailing semi-colons */` |
|  124098 | 5208 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   24828 | 5209 | `			pGen->pIn++;` |
|       2 | 5210 | `		}` |
|   99272 | 5211 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5212 | `			/* End of class body */` |
|   22472 | 5213 | `			break;` |
|       - | 5214 | `		}` |
|   76802 | 5215 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5216 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5217 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5218 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5219 | `			if( rc == SXERR_ABORT ){` |
|       - | 5220 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5221 | `				return SXERR_ABORT;` |
|       - | 5222 | `			}` |
|     ! 0 | 5223 | `			goto done;` |
|       - | 5224 | `		}` |
|       - | 5225 | `		/* Assume public visibility */` |
|   76802 | 5226 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   76802 | 5227 | `		iAttrflags = 0;` |
|   76802 | 5228 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 5229 | `			/* Extract the current keyword */` |
|   76802 | 5230 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   76802 | 5231 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5232 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 5233 | `				TraitUseEntry sUse;` |
|      31 | 5234 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      31 | 5235 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      31 | 5236 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      23 | 5237 | `				for(;;){` |
|       - | 5238 | `					ph7_class *pTrait;` |
|       - | 5239 | `					SyString *pTraitName;` |
|      39 | 5240 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5241 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5242 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 5243 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5244 | `							return SXERR_ABORT;` |
|       - | 5245 | `						}` |
|     ! 0 | 5246 | `						break;` |
|       - | 5247 | `					}` |
|      39 | 5248 | `					pTraitName = &pGen->pIn->sData;` |
|       - | 5249 | `					/* Resolve trait name through namespace/imports */ {` |
|       - | 5250 | `						SyBlob sResolved;` |
|      39 | 5251 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      39 | 5252 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      77 | 5253 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      38 | 5254 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      39 | 5255 | `						SyBlobRelease(&sResolved);` |
|       - | 5256 | `					}` |
|       - | 5257 | `					/* Only traits are allowed */` |
|      39 | 5258 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5259 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 5260 | `					}` |
|      39 | 5261 | `					if( pTrait == 0 ){` |
|     ! 0 | 5262 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5263 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 | 5264 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5265 | `							return SXERR_ABORT;` |
|       - | 5266 | `						}` |
|     ! 0 | 5267 | `					}else{` |
|      39 | 5268 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 5269 | `					}` |
|      39 | 5270 | `					pGen->pIn++; /* Advance past trait name */` |
|      39 | 5271 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      16 | 5272 | `						break;` |
|       - | 5273 | `					}` |
|       9 | 5274 | `					pGen->pIn++; /* Jump the comma */` |
|       1 | 5275 | `				}` |
|       - | 5276 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      31 | 5277 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 5278 | `					SyToken *pBlock;` |
|       9 | 5279 | `					pGen->pIn++; /* Jump '{' */` |
|       9 | 5280 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 | 5281 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 | 5282 | `					sUse.pResolvEnd = pBlock;` |
|       9 | 5283 | `					if( pBlock < pGen->pEnd ){` |
|       9 | 5284 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 | 5285 | `					}else{` |
|     ! 0 | 5286 | `						pGen->pIn = pGen->pEnd;` |
|       - | 5287 | `					}` |
|       4 | 5288 | `				}` |
|      31 | 5289 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 5290 | `				/* The semicolon will be consumed by the outer loop */` |
|      31 | 5291 | `				continue;` |
|       - | 5292 | `			}` |
|   76772 | 5293 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|   74218 | 5294 | `				iProtection = nKwrd;` |
|   74218 | 5295 | `				pGen->pIn++; /* Jump the visibility token */` |
|   74218 | 5296 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5297 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5298 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5299 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5300 | `					if( rc == SXERR_ABORT ){` |
|       - | 5301 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5302 | `						return SXERR_ABORT;` |
|       - | 5303 | `					}` |
|     ! 0 | 5304 | `					goto done;` |
|       - | 5305 | `				}` |
|   74218 | 5306 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5307 | `					/* Attribute declaration */` |
|   24784 | 5308 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   24784 | 5309 | `					if( rc != SXRET_OK ){` |
|       3 | 5310 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5311 | `							return SXERR_ABORT;` |
|       - | 5312 | `						}` |
|       3 | 5313 | `						goto done;` |
|       - | 5314 | `					}` |
|   24782 | 5315 | `					continue;` |
|       - | 5316 | `				}` |
|       - | 5317 | `				/* Extract the keyword */` |
|   49436 | 5318 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   24717 | 5319 | `			}` |
|   51990 | 5320 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5321 | `				/* Process constant declaration */` |
|      10 | 5322 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 | 5323 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5324 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5325 | `						return SXERR_ABORT;` |
|       - | 5326 | `					}` |
|     ! 0 | 5327 | `					goto done;` |
|       - | 5328 | `				}` |
|       6 | 5329 | `			}else{` |
|   51982 | 5330 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5331 | `					/* Static method or attribute,record that */` |
|      23 | 5332 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      23 | 5333 | `					pGen->pIn++; /* Jump the static keyword */` |
|      23 | 5334 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5335 | `						/* Extract the keyword */` |
|      19 | 5336 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      19 | 5337 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 5338 | `							iProtection = nKwrd;` |
|     ! 0 | 5339 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 5340 | `						}` |
|       9 | 5341 | `					}` |
|      23 | 5342 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5343 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5344 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 5345 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5346 | `						if( rc == SXERR_ABORT ){` |
|       - | 5347 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5348 | `							return SXERR_ABORT;` |
|       - | 5349 | `						}` |
|     ! 0 | 5350 | `						goto done;` |
|       - | 5351 | `					}` |
|      23 | 5352 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5353 | `						/* Attribute declaration */` |
|       5 | 5354 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 5355 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 5356 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5357 | `								return SXERR_ABORT;` |
|       - | 5358 | `							}` |
|     ! 0 | 5359 | `							goto done;` |
|       - | 5360 | `						}` |
|       5 | 5361 | `						continue;` |
|       - | 5362 | `					}` |
|       - | 5363 | `					/* Extract the keyword */` |
|      19 | 5364 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   51969 | 5365 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 5366 | `					/* Abstract method,record that */` |
|       8 | 5367 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 5368 | `					/* Mark the whole class as abstract */` |
|       8 | 5369 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 5370 | `					/* Advance the stream cursor */` |
|       8 | 5371 | `					pGen->pIn++;` |
|       8 | 5372 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       8 | 5373 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       8 | 5374 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 | 5375 | `							iProtection = nKwrd;` |
|       6 | 5376 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 5377 | `						}` |
|       3 | 5378 | `					}` |
|       8 | 5379 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       6 | 5380 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5381 | `							/* Static method */` |
|     ! 0 | 5382 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5383 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5384 | `					}` |
|       8 | 5385 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       6 | 5386 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5387 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5388 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 5389 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5390 | `							if( rc == SXERR_ABORT ){` |
|       - | 5391 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5392 | `								return SXERR_ABORT;` |
|       - | 5393 | `							}` |
|     ! 0 | 5394 | `							goto done;` |
|       - | 5395 | `					}` |
|       8 | 5396 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   51957 | 5397 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 5398 | `					/* final method ,record that */` |
|       5 | 5399 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 5400 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 5401 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5402 | `						/* Extract the keyword */` |
|       5 | 5403 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 5404 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 5405 | `							iProtection = nKwrd;` |
|       5 | 5406 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 5407 | `						}` |
|       2 | 5408 | `					}` |
|       5 | 5409 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 5410 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5411 | `							/* Static method */` |
|     ! 0 | 5412 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5413 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5414 | `					}` |
|       5 | 5415 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 5416 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5417 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5418 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 5419 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5420 | `							if( rc == SXERR_ABORT ){` |
|       - | 5421 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5422 | `								return SXERR_ABORT;` |
|       - | 5423 | `							}` |
|     ! 0 | 5424 | `							goto done;` |
|       - | 5425 | `					}` |
|       5 | 5426 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 5427 | `				}` |
|   51978 | 5428 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 5429 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5430 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 5431 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5432 | `						if( rc == SXERR_ABORT ){` |
|       - | 5433 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5434 | `							return SXERR_ABORT;` |
|       - | 5435 | `						}` |
|     ! 0 | 5436 | `						goto done;` |
|       - | 5437 | `				}` |
|   51978 | 5438 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 5439 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 5440 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 5441 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5442 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 5443 | `						if( rc == SXERR_ABORT ){` |
|       - | 5444 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5445 | `							return SXERR_ABORT;` |
|       - | 5446 | `						}` |
|     ! 0 | 5447 | `						goto done;` |
|       - | 5448 | `					}` |
|       - | 5449 | `					/* Attribute declaration */` |
|       7 | 5450 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 5451 | `				}else{` |
|       - | 5452 | `					/* Process method declaration */` |
|   51972 | 5453 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 5454 | `				}` |
|   51978 | 5455 | `				if( rc != SXRET_OK ){` |
|       3 | 5456 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5457 | `						return SXERR_ABORT;` |
|       - | 5458 | `					}` |
|       3 | 5459 | `					goto done;` |
|       - | 5460 | `				}` |
|       - | 5461 | `			}` |
|   25993 | 5462 | `		}else{` |
|       - | 5463 | `			/* Attribute declaration */` |
|     ! 0 | 5464 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5465 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5466 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5467 | `					return SXERR_ABORT;` |
|       - | 5468 | `				}` |
|     ! 0 | 5469 | `				goto done;` |
|       - | 5470 | `			}` |
|       - | 5471 | `		}` |
|       2 | 5472 | `	}` |
|       - | 5473 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 5474 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 5475 | `	 */` |
|       - | 5476 | `	{` |
|       - | 5477 | `		TraitUseEntry *apUse;` |
|       - | 5478 | `		sxu32 nU;` |
|   22472 | 5479 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   22502 | 5480 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      31 | 5481 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      31 | 5482 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      31 | 5483 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      31 | 5484 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 5485 | `			sxu32 nT;` |
|      31 | 5486 | `			if( !hasResolution ){` |
|       - | 5487 | `				/* No conflict resolution block: use standard trait application */` |
|      51 | 5488 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      29 | 5489 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      29 | 5490 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 5491 | `						break;` |
|       - | 5492 | `					}` |
|      15 | 5493 | `				}` |
|      12 | 5494 | `			}else{` |
|       - | 5495 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 5496 | `				 * then use the block to resolve method conflicts.` |
|       - | 5497 | `				 */` |
|       - | 5498 | `				SyToken *pR;` |
|      19 | 5499 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 | 5500 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 5501 | `					ph7_class_attr *pAR;` |
|       - | 5502 | `					SyHashEntry *pER;` |
|       - | 5503 | `					SyString *pNR;` |
|      11 | 5504 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 | 5505 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 5506 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 5507 | `						pNR = &pAR->sName;` |
|     ! 0 | 5508 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 5509 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 5510 | `						}` |
|     ! 0 | 5511 | `					}` |
|      11 | 5512 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 | 5513 | `				}` |
|       - | 5514 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 | 5515 | `				pR = pUse->pResolvStart;` |
|      21 | 5516 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 5517 | `					SyString sTrait,sMethod;` |
|       - | 5518 | `					ph7_class *pSrcTrait;` |
|       - | 5519 | `					ph7_class_method *pMeth;` |
|       - | 5520 | `					sxi32 nRKwrd;` |
|      33 | 5521 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 5522 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 5523 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 5524 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 5525 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 5526 | `					sMethod = pR->sData;` |
|      13 | 5527 | `					pR++;` |
|      13 | 5528 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 5529 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 5530 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 5531 | `							sTrait = sMethod;` |
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
|      13 | 5544 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 | 5545 | `						pSrcTrait = 0;` |
|       7 | 5546 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 | 5547 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 | 5548 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 | 5549 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 | 5550 | `								pSrcTrait = apTrait[nT];` |
|       5 | 5551 | `								break;` |
|       - | 5552 | `							}` |
|       2 | 5553 | `						}` |
|       5 | 5554 | `						if( pSrcTrait ){` |
|       5 | 5555 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 | 5556 | `							if( pMeth ){` |
|       5 | 5557 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 | 5558 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 | 5559 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 | 5560 | `								}` |
|       2 | 5561 | `							}` |
|       2 | 5562 | `						}` |
|       2 | 5563 | `					}` |
|      29 | 5564 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 5565 | `				}` |
|       - | 5566 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 | 5567 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 5568 | `					ph7_class_method *pMR;` |
|       - | 5569 | `					SyHashEntry *pER;` |
|       - | 5570 | `					SyString *pNR;` |
|      11 | 5571 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 | 5572 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 | 5573 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 | 5574 | `						pNR = &pMR->sFunc.sName;` |
|      19 | 5575 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 | 5576 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 | 5577 | `						}` |
|       1 | 5578 | `					}` |
|       6 | 5579 | `				}` |
|       - | 5580 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 | 5581 | `				pR = pUse->pResolvStart;` |
|      21 | 5582 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 5583 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 5584 | `					ph7_class *pSrcTrait;` |
|       - | 5585 | `					ph7_class_method *pMeth;` |
|      21 | 5586 | `					int hasQual = 0;` |
|       - | 5587 | `					sxi32 nRKwrd;` |
|      33 | 5588 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 5589 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 5590 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 5591 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 5592 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 | 5593 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 5594 | `					sMethod = pR->sData;` |
|      13 | 5595 | `					pR++;` |
|      13 | 5596 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 5597 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 5598 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 5599 | `							sTrait = sMethod;` |
|       7 | 5600 | `							hasQual = 1;` |
|       7 | 5601 | `							pR++;` |
|       7 | 5602 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 5603 | `							sMethod = pR->sData;` |
|       7 | 5604 | `							pR++;` |
|       3 | 5605 | `						}` |
|       3 | 5606 | `					}` |
|      13 | 5607 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5608 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 5609 | `						continue;` |
|       - | 5610 | `					}` |
|      13 | 5611 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 5612 | `					pR++;` |
|      13 | 5613 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 | 5614 | `						sxi32 iNewVis = -1;` |
|       9 | 5615 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 | 5616 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 | 5617 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 | 5618 | `								iNewVis = nAK;` |
|       7 | 5619 | `								pR++;` |
|       3 | 5620 | `							}` |
|       3 | 5621 | `						}` |
|       9 | 5622 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 | 5623 | `							sAlias = pR->sData;` |
|       7 | 5624 | `							pR++;` |
|       3 | 5625 | `						}` |
|       9 | 5626 | `						pMeth = 0;` |
|       9 | 5627 | `						if( hasQual ){` |
|       3 | 5628 | `							pSrcTrait = 0;` |
|       5 | 5629 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 | 5630 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 | 5631 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 | 5632 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 | 5633 | `									pSrcTrait = apTrait[nT];` |
|       3 | 5634 | `									break;` |
|       - | 5635 | `								}` |
|       2 | 5636 | `							}` |
|       3 | 5637 | `							if( pSrcTrait ){` |
|       3 | 5638 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 | 5639 | `							}` |
|       2 | 5640 | `						}else{` |
|       7 | 5641 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 5642 | `						}` |
|       9 | 5643 | `						if( pMeth ){` |
|       9 | 5644 | `							if( sAlias.nByte > 0 ){` |
|       - | 5645 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 5646 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 5647 | `								 */` |
|       - | 5648 | `								ph7_class_method *pAlias;` |
|       - | 5649 | `								char *zAliasDup;` |
|       7 | 5650 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 | 5651 | `								if( pAlias ){` |
|       7 | 5652 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 | 5653 | `									if( iNewVis >= 0 ){` |
|       5 | 5654 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 5655 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 5656 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 | 5657 | `									}` |
|       7 | 5658 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 | 5659 | `									if( zAliasDup ){` |
|       7 | 5660 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 | 5661 | `									}` |
|       4 | 5662 | `								}` |
|       6 | 5663 | `							}else if( iNewVis >= 0 ){` |
|       - | 5664 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 5665 | `								ph7_class_method *pCopy;` |
|       3 | 5666 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 5667 | `								if( pCopy ){` |
|       3 | 5668 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 5669 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 5670 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 5671 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 5672 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 5673 | `									/* Replace the method in the class hash */` |
|       3 | 5674 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 5675 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 5676 | `								}` |
|       1 | 5677 | `							}` |
|       4 | 5678 | `						}` |
|       4 | 5679 | `						SXUNUSED(hasQual);` |
|       4 | 5680 | `					}` |
|      17 | 5681 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 5682 | `				}` |
|       - | 5683 | `			}` |
|      31 | 5684 | `			SySetRelease(&pUse->aTraits);` |
|      16 | 5685 | `		}` |
|       - | 5686 | `	}` |
|       - | 5687 | `	/* Install the class */` |
|   22472 | 5688 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   22472 | 5689 | `	if( rc == SXRET_OK ){` |
|       - | 5690 | `		ph7_class **apInterface;` |
|       - | 5691 | `		sxu32 n;` |
|   22472 | 5692 | `		if( pBase ){` |
|       - | 5693 | `			/* Inherit from base class and mark as a subclass */` |
|   14812 | 5694 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    7405 | 5695 | `		}` |
|   22472 | 5696 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   22484 | 5697 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 5698 | `			/* Implements one or more interface */` |
|      14 | 5699 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|      14 | 5700 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5701 | `				break;` |
|       - | 5702 | `			}` |
|       8 | 5703 | `		}` |
|   11235 | 5704 | `	}` |
|   22472 | 5705 | `	SySetRelease(&aUseEntries);` |
|   22472 | 5706 | `	SySetRelease(&aInterfaces);` |
|   22472 | 5707 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5708 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5709 | `		return SXERR_ABORT;` |
|       - | 5710 | `	}` |
|   11235 | 5711 | `done:` |
|       - | 5712 | `	/* Point beyond the class body */` |
|   22476 | 5713 | `	pGen->pIn = &pEnd[1];` |
|   22476 | 5714 | `	pGen->pEnd = pTmp;` |
|   22476 | 5715 | `	return PH7_OK;` |
|   11239 | 5716 |  |
|       - | 5717 | `/*` |
|       - | 5718 | ` * Compile a user-defined abstract class.` |
|       - | 5719 | ` *  According to the PHP language reference manual` |
|       - | 5720 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 5721 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 5722 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 5723 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 5724 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 5725 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 5726 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 5727 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 5728 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 5729 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 5730 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 5731 | ` *   could differ.` |
|       - | 5732 | ` */` |
|       4 | 5733 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 5734 |  |
|       - | 5735 | `	sxi32 rc;` |
|       6 | 5736 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|       6 | 5737 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|       6 | 5738 | `	return rc;` |
|       2 | 5739 |  |
|       - | 5740 | `/*` |
|       - | 5741 | ` * Compile a user-defined final class.` |
|       - | 5742 | ` *  According to the PHP language reference manual` |
|       - | 5743 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 5744 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 5745 | ` *    final then it cannot be extended.` |
|       - | 5746 | ` */` |
|       2 | 5747 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 5748 |  |
|       - | 5749 | `	sxi32 rc;` |
|       3 | 5750 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 5751 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 5752 | `	return rc;` |
|       1 | 5753 |  |
|       - | 5754 | `/*` |
|       - | 5755 | ` * Compile a user-defined trait.` |
|       - | 5756 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 5757 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 5758 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 5759 | ` */` |
|      42 | 5760 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       1 | 5761 |  |
|      43 | 5762 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5763 | `	ph7_class *pClass;` |
|       - | 5764 | `	SyToken *pEnd,*pTmp;` |
|       - | 5765 | `	sxi32 iProtection;` |
|       - | 5766 | `	sxi32 iAttrflags;` |
|       - | 5767 | `	SyString *pName;` |
|       - | 5768 | `	sxi32 nKwrd;` |
|       - | 5769 | `	sxi32 rc;` |
|       - | 5770 | `	/* Jump the 'trait' keyword */` |
|      43 | 5771 | `	pGen->pIn++;` |
|      43 | 5772 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5773 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 5774 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5775 | `			return SXERR_ABORT;` |
|       - | 5776 | `		}` |
|     ! 0 | 5777 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 5778 | `			pGen->pIn++;` |
|     ! 0 | 5779 | `		}` |
|     ! 0 | 5780 | `		return SXRET_OK;` |
|       - | 5781 | `	}` |
|       - | 5782 | `	/* Extract trait name */` |
|      43 | 5783 | `	pName = &pGen->pIn->sData;` |
|      43 | 5784 | `	pGen->pIn++;` |
|       - | 5785 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5786 | `		SyBlob sFQN;` |
|       - | 5787 | `		SyString sFQNStr;` |
|      43 | 5788 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      43 | 5789 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      43 | 5790 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      43 | 5791 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      43 | 5792 | `		SyBlobRelease(&sFQN);` |
|       - | 5793 | `	}` |
|      43 | 5794 | `	if( pClass == 0 ){` |
|     ! 0 | 5795 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5796 | `		return SXERR_ABORT;` |
|       - | 5797 | `	}` |
|       - | 5798 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      43 | 5799 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 5800 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 5801 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5802 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5803 | `			return SXERR_ABORT;` |
|       - | 5804 | `		}` |
|     ! 0 | 5805 | `		return SXRET_OK;` |
|       - | 5806 | `	}` |
|      43 | 5807 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      43 | 5808 | `	pEnd = 0;` |
|      43 | 5809 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      43 | 5810 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 5811 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 5812 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5813 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5814 | `			return SXERR_ABORT;` |
|       - | 5815 | `		}` |
|     ! 0 | 5816 | `		return SXRET_OK;` |
|       - | 5817 | `	}` |
|       - | 5818 | `	/* Swap token stream */` |
|      43 | 5819 | `	pTmp = pGen->pEnd;` |
|      43 | 5820 | `	pGen->pEnd = pEnd;` |
|       - | 5821 | `	/* Mark as trait */` |
|      43 | 5822 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 5823 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      44 | 5824 | `	for(;;){` |
|     119 | 5825 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      17 | 5826 | `			pGen->pIn++;` |
|       1 | 5827 | `		}` |
|     103 | 5828 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      43 | 5829 | `			break;` |
|       - | 5830 | `		}` |
|      61 | 5831 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5832 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5833 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 5834 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5835 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5836 | `				return SXERR_ABORT;` |
|       - | 5837 | `			}` |
|     ! 0 | 5838 | `			goto done;` |
|       - | 5839 | `		}` |
|      61 | 5840 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      61 | 5841 | `		iAttrflags = 0;` |
|      61 | 5842 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      61 | 5843 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      61 | 5844 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5845 | `				/* Trait uses another trait: use OtherTrait; */` |
|       3 | 5846 | `				pGen->pIn++; /* Jump 'use' */` |
|       1 | 5847 | `				for(;;){` |
|       - | 5848 | `					ph7_class *pUsedTrait;` |
|       - | 5849 | `					SyString *pUsedName;` |
|       3 | 5850 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5851 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5852 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 5853 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5854 | `							return SXERR_ABORT;` |
|       - | 5855 | `						}` |
|     ! 0 | 5856 | `						break;` |
|       - | 5857 | `					}` |
|       3 | 5858 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 5859 | `					{` |
|       - | 5860 | `						SyBlob sResolved;` |
|       3 | 5861 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 5862 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       4 | 5863 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 5864 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 5865 | `						SyBlobRelease(&sResolved);` |
|       - | 5866 | `					}` |
|       3 | 5867 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5868 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 5869 | `					}` |
|       3 | 5870 | `					if( pUsedTrait == 0 ){` |
|     ! 0 | 5871 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5872 | `							"'%z' is not a trait",pUsedName);` |
|     ! 0 | 5873 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5874 | `							return SXERR_ABORT;` |
|       - | 5875 | `						}` |
|     ! 0 | 5876 | `					}else{` |
|       3 | 5877 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 5878 | `					}` |
|       3 | 5879 | `					pGen->pIn++;` |
|       3 | 5880 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       2 | 5881 | `						break;` |
|       - | 5882 | `					}` |
|     ! 0 | 5883 | `					pGen->pIn++;` |
|     ! 0 | 5884 | `				}` |
|       3 | 5885 | `				continue;` |
|       - | 5886 | `			}` |
|      59 | 5887 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      57 | 5888 | `				iProtection = nKwrd;` |
|      57 | 5889 | `				pGen->pIn++;` |
|      57 | 5890 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5891 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5892 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 5893 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5894 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5895 | `						return SXERR_ABORT;` |
|       - | 5896 | `					}` |
|     ! 0 | 5897 | `					goto done;` |
|       - | 5898 | `				}` |
|      57 | 5899 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 | 5900 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 | 5901 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 5902 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5903 | `							return SXERR_ABORT;` |
|       - | 5904 | `						}` |
|     ! 0 | 5905 | `						goto done;` |
|       - | 5906 | `					}` |
|      11 | 5907 | `					continue;` |
|       - | 5908 | `				}` |
|      47 | 5909 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      23 | 5910 | `			}` |
|      49 | 5911 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 5912 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5913 | `					"Traits cannot have constants");` |
|     ! 0 | 5914 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5915 | `					return SXERR_ABORT;` |
|       - | 5916 | `				}` |
|     ! 0 | 5917 | `				goto done;` |
|     ! 0 | 5918 | `			}else{` |
|      49 | 5919 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 5920 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 5921 | `					pGen->pIn++;` |
|       5 | 5922 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 5923 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 5924 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 5925 | `							iProtection = nKwrd;` |
|     ! 0 | 5926 | `							pGen->pIn++;` |
|     ! 0 | 5927 | `						}` |
|       1 | 5928 | `					}` |
|       5 | 5929 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5930 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5931 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 5932 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5933 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5934 | `							return SXERR_ABORT;` |
|       - | 5935 | `						}` |
|     ! 0 | 5936 | `						goto done;` |
|       - | 5937 | `					}` |
|       5 | 5938 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 5939 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 5940 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 5941 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5942 | `								return SXERR_ABORT;` |
|       - | 5943 | `							}` |
|     ! 0 | 5944 | `							goto done;` |
|       - | 5945 | `						}` |
|       3 | 5946 | `						continue;` |
|       - | 5947 | `					}` |
|       3 | 5948 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      46 | 5949 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       3 | 5950 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       3 | 5951 | `					pGen->pIn++;` |
|       3 | 5952 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 5953 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 5954 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       3 | 5955 | `							iProtection = nKwrd;` |
|       3 | 5956 | `							pGen->pIn++;` |
|       1 | 5957 | `						}` |
|       1 | 5958 | `					}` |
|       3 | 5959 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       2 | 5960 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5961 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5962 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 5963 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5964 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5965 | `							return SXERR_ABORT;` |
|       - | 5966 | `						}` |
|     ! 0 | 5967 | `						goto done;` |
|       - | 5968 | `					}` |
|       3 | 5969 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       1 | 5970 | `				}` |
|      47 | 5971 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 5972 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5973 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 5974 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5975 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5976 | `						return SXERR_ABORT;` |
|       - | 5977 | `					}` |
|     ! 0 | 5978 | `					goto done;` |
|       - | 5979 | `				}` |
|      47 | 5980 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 5981 | `					pGen->pIn++;` |
|     ! 0 | 5982 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 5983 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5984 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 5985 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5986 | `							return SXERR_ABORT;` |
|       - | 5987 | `						}` |
|     ! 0 | 5988 | `						goto done;` |
|       - | 5989 | `					}` |
|     ! 0 | 5990 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5991 | `				}else{` |
|      47 | 5992 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 5993 | `				}` |
|      47 | 5994 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5995 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5996 | `						return SXERR_ABORT;` |
|       - | 5997 | `					}` |
|     ! 0 | 5998 | `					goto done;` |
|       - | 5999 | `				}` |
|       - | 6000 | `			}` |
|      24 | 6001 | `		}else{` |
|     ! 0 | 6002 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6003 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6004 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6005 | `					return SXERR_ABORT;` |
|       - | 6006 | `				}` |
|     ! 0 | 6007 | `				goto done;` |
|       - | 6008 | `			}` |
|       - | 6009 | `		}` |
|       1 | 6010 | `	}` |
|       - | 6011 | `	/* Install the trait */` |
|      43 | 6012 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      43 | 6013 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6014 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6015 | `		return SXERR_ABORT;` |
|       - | 6016 | `	}` |
|      21 | 6017 | `done:` |
|       - | 6018 | `	/* Point beyond the trait body */` |
|      43 | 6019 | `	pGen->pIn = &pEnd[1];` |
|      43 | 6020 | `	pGen->pEnd = pTmp;` |
|      43 | 6021 | `	return PH7_OK;` |
|      22 | 6022 |  |
|       - | 6023 | `/*` |
|       - | 6024 | ` * Compile a user-defined class.` |
|       - | 6025 | ` *  According to the PHP language reference manual` |
|       - | 6026 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 6027 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 6028 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 6029 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 6030 | ` *   and functions (called "methods").` |
|       - | 6031 | ` */` |
|   22468 | 6032 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 6033 |  |
|       - | 6034 | `	sxi32 rc;` |
|   22470 | 6035 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   22470 | 6036 | `	return rc;` |
|       2 | 6037 |  |
|       - | 6038 | `/*` |
|       - | 6039 | ` * Exception handling.` |
|       - | 6040 | ` *  According to the PHP language reference manual` |
|       - | 6041 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 6042 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 6043 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 6044 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 6045 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 6046 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 6047 | ` *    (or re-thrown) within a catch block.` |
|       - | 6048 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 6049 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 6050 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 6051 | ` *    been defined with set_exception_handler().` |
|       - | 6052 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 6053 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 6054 | ` */` |
|       - | 6055 | `/*` |
|       - | 6056 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 6057 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 6058 | ` * indicates failure.` |
|       - | 6059 | ` */` |
|    7408 | 6060 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 6061 |  |
|    7410 | 6062 | `	sxi32 rc = SXRET_OK;` |
|    7410 | 6063 | `	if( pRoot->pOp ){` |
|    7406 | 6064 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    3705 | 6065 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 6066 | `			/* Unexpected expression */` |
|     ! 0 | 6067 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6068 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 6069 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 6070 | `				rc = SXERR_INVALID;` |
|     ! 0 | 6071 | `			}` |
|       2 | 6072 | `		}` |
|    3706 | 6073 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 6074 | `		/* Unexpected expression */` |
|     ! 0 | 6075 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6076 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 6077 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 6078 | `			rc = SXERR_INVALID;` |
|     ! 0 | 6079 | `		}` |
|     ! 0 | 6080 | `	}` |
|    7410 | 6081 | `	return rc;` |
|       2 | 6082 |  |
|       - | 6083 | `/*` |
|       - | 6084 | ` * Compile a 'throw' statement.` |
|       - | 6085 | ` * throw: This is how you trigger an exception.` |
|       - | 6086 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 6087 | ` */` |
|    7408 | 6088 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 6089 |  |
|    7410 | 6090 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6091 | `	GenBlock *pBlock;` |
|       - | 6092 | `	sxu32 nIdx;` |
|       - | 6093 | `	sxi32 rc;` |
|    7410 | 6094 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 6095 | `	/* Compile the expression */` |
|    7410 | 6096 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    7410 | 6097 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 6098 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 6099 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6100 | `			return SXERR_ABORT;` |
|       - | 6101 | `		}` |
|     ! 0 | 6102 | `		return SXRET_OK;` |
|       - | 6103 | `	}` |
|    7410 | 6104 | `	pBlock = pGen->pCurrent;` |
|       - | 6105 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   34498 | 6106 | `	while(pBlock->pParent){` |
|   34494 | 6107 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    7406 | 6108 | `			break;` |
|       - | 6109 | `		}` |
|       - | 6110 | `		/* Point to the parent block */` |
|   27090 | 6111 | `		pBlock = pBlock->pParent;` |
|       2 | 6112 | `	}` |
|       - | 6113 | `	/* Emit the throw instruction */` |
|    7410 | 6114 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 6115 | `	/* Emit the jump */` |
|    7410 | 6116 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    7410 | 6117 | `	return SXRET_OK;` |
|    3706 | 6118 |  |
|       - | 6119 | `/*` |
|       - | 6120 | ` * Compile a 'catch' block.` |
|       - | 6121 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 6122 | ` * an object containing the exception information.` |
|       - | 6123 | ` */` |
|      48 | 6124 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 6125 |  |
|      50 | 6126 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6127 | `	ph7_exception_block sCatch;` |
|       - | 6128 | `	SySet *pInstrContainer;` |
|       - | 6129 | `	GenBlock *pCatch;` |
|       - | 6130 | `	SyToken *pToken;` |
|       - | 6131 | `	SyString *pName;` |
|       - | 6132 | `	char *zDup;` |
|       - | 6133 | `	sxi32 rc;` |
|      50 | 6134 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 6135 | `	/* Zero the structure */` |
|      50 | 6136 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 6137 | `	/* Initialize fields */` |
|      50 | 6138 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|      72 | 6139 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ \|\|` |
|      50 | 6140 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6141 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6142 | `			pToken = pGen->pIn;` |
|     ! 0 | 6143 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6144 | `				pToken--;` |
|     ! 0 | 6145 | `			}` |
|     ! 0 | 6146 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6147 | `				"Catch: Unexpected token '%z',excpecting class name",&pToken->sData);` |
|     ! 0 | 6148 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6149 | `				return SXERR_ABORT;` |
|       - | 6150 | `			}` |
|     ! 0 | 6151 | `			return SXERR_INVALID;` |
|       - | 6152 | `	}` |
|       - | 6153 | `	/* Extract the exception class */` |
|      50 | 6154 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|       - | 6155 | `	/* Duplicate class name */` |
|      50 | 6156 | `	pName = &pGen->pIn->sData;` |
|      50 | 6157 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      50 | 6158 | `	if( zDup == 0 ){` |
|     ! 0 | 6159 | `		goto Mem;` |
|       - | 6160 | `	}` |
|      50 | 6161 | `	SyStringInitFromBuf(&sCatch.sClass,zDup,pName->nByte);` |
|      50 | 6162 | `	pGen->pIn++;` |
|      72 | 6163 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      50 | 6164 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6165 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6166 | `			pToken = pGen->pIn;` |
|     ! 0 | 6167 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6168 | `				pToken--;` |
|     ! 0 | 6169 | `			}` |
|     ! 0 | 6170 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6171 | `				"Catch: Unexpected token '%z',expecting variable name",&pToken->sData);` |
|     ! 0 | 6172 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6173 | `				return SXERR_ABORT;` |
|       - | 6174 | `			}` |
|     ! 0 | 6175 | `			return SXERR_INVALID;` |
|       - | 6176 | `	}` |
|      50 | 6177 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 6178 | `	/* Duplicate instance name */` |
|      50 | 6179 | `	pName = &pGen->pIn->sData;` |
|      50 | 6180 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      50 | 6181 | `	if( zDup == 0 ){` |
|     ! 0 | 6182 | `		goto Mem;` |
|       - | 6183 | `	}` |
|      50 | 6184 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      50 | 6185 | `	pGen->pIn++;` |
|      50 | 6186 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 6187 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 6188 | `		pToken = pGen->pIn;` |
|     ! 0 | 6189 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6190 | `			pToken--;` |
|     ! 0 | 6191 | `		}` |
|     ! 0 | 6192 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6193 | `			"Catch: Unexpected token '%z',expecting right parenthesis ')'",&pToken->sData);` |
|     ! 0 | 6194 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6195 | `			return SXERR_ABORT;` |
|       - | 6196 | `		}` |
|     ! 0 | 6197 | `		return SXERR_INVALID;` |
|       - | 6198 | `	}` |
|       - | 6199 | `	/* Compile the block */` |
|      50 | 6200 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 6201 | `	/* Create the catch block */` |
|      50 | 6202 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      50 | 6203 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6204 | `		return SXERR_ABORT;` |
|       - | 6205 | `	}` |
|       - | 6206 | `	/* Swap bytecode container */` |
|      50 | 6207 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      50 | 6208 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 6209 | `	/* Compile the block */` |
|      50 | 6210 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 6211 | `	/* Fix forward jumps now the destination is resolved  */` |
|      50 | 6212 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6213 | `	/* Emit the DONE instruction */` |
|      50 | 6214 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6215 | `	/* Leave the block */` |
|      50 | 6216 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6217 | `	/* Restore the default container */` |
|      50 | 6218 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 6219 | `	/* Install the catch block */` |
|      50 | 6220 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      50 | 6221 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6222 | `		goto Mem;` |
|       - | 6223 | `	}` |
|      50 | 6224 | `	return SXRET_OK;` |
|     ! 0 | 6225 | `Mem:` |
|     ! 0 | 6226 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6227 | `	return SXERR_ABORT;` |
|      26 | 6228 |  |
|       - | 6229 | `/*` |
|       - | 6230 | ` * Compile a 'try' block.` |
|       - | 6231 | ` * A function using an exception should be in a "try" block.` |
|       - | 6232 | ` * If the exception does not trigger, the code will continue` |
|       - | 6233 | ` * as normal. However if the exception triggers, an exception` |
|       - | 6234 | ` * is "thrown".` |
|       - | 6235 | ` */` |
|      56 | 6236 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 6237 |  |
|       - | 6238 | `	ph7_exception *pException;` |
|      58 | 6239 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6240 | `	GenBlock *pTry;` |
|       - | 6241 | `	sxu32 nJmpIdx;` |
|       - | 6242 | `	sxi32 rc;` |
|       - | 6243 | `	/* Create the exception container */` |
|      58 | 6244 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|      58 | 6245 | `	if( pException == 0 ){` |
|     ! 0 | 6246 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 6247 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6248 | `		return SXERR_ABORT;` |
|       - | 6249 | `	}` |
|       - | 6250 | `	/* Zero the structure */` |
|      58 | 6251 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 6252 | `	/* Initialize fields */` |
|      58 | 6253 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|      58 | 6254 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      58 | 6255 | `	pException->iHasFinally = 0;` |
|      58 | 6256 | `	pException->iFinallyDone = 0;` |
|      58 | 6257 | `	pException->pVm = pGen->pVm;` |
|       - | 6258 | `	/* Create the try block */` |
|      58 | 6259 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      58 | 6260 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6261 | `		return SXERR_ABORT;` |
|       - | 6262 | `	}` |
|       - | 6263 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|      58 | 6264 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 6265 | `	/* Fix the jump later when the destination is resolved */` |
|      58 | 6266 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|      58 | 6267 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 6268 | `	/* Compile the block */` |
|      58 | 6269 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      58 | 6270 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6271 | `		return SXERR_ABORT;` |
|       - | 6272 | `	}` |
|       - | 6273 | `	/* Fix forward jumps now the destination is resolved */` |
|      58 | 6274 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6275 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|      58 | 6276 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 6277 | `	/* Leave the block */` |
|      58 | 6278 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6279 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|      58 | 6280 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      54 | 6281 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 6282 | `		/* Compile one or more catch blocks */` |
|      48 | 6283 | `		for(;;){` |
|      96 | 6284 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      74 | 6285 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      26 | 6286 | `					break;` |
|       - | 6287 | `			}` |
|      50 | 6288 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|      50 | 6289 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6290 | `				return SXERR_ABORT;` |
|       - | 6291 | `			}` |
|       2 | 6292 | `		}` |
|      24 | 6293 | `	}` |
|       - | 6294 | `	/* Compile optional finally block */` |
|      58 | 6295 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      24 | 6296 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 6297 | `		SySet *pInstrContainer;` |
|       - | 6298 | `		GenBlock *pFinBlock;` |
|      21 | 6299 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 6300 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      21 | 6301 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      21 | 6302 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6303 | `			return SXERR_ABORT;` |
|       - | 6304 | `		}` |
|       - | 6305 | `		/* Swap bytecode container */` |
|      21 | 6306 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      21 | 6307 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 6308 | `		/* Compile the finally body */` |
|      21 | 6309 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      21 | 6310 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6311 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 6312 | `			return SXERR_ABORT;` |
|       - | 6313 | `		}` |
|       - | 6314 | `		/* Fix forward jumps now the destination is resolved */` |
|      21 | 6315 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6316 | `		/* Emit DONE to terminate the finally block */` |
|      21 | 6317 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6318 | `		/* Leave the block */` |
|      21 | 6319 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6320 | `		/* Restore the default container */` |
|      21 | 6321 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      21 | 6322 | `		pException->iHasFinally = 1;` |
|      10 | 6323 | `	}` |
|       - | 6324 | `	/* Must have at least one catch or finally */` |
|      58 | 6325 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       3 | 6326 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 6327 | `			"Cannot use try without catch or finally");` |
|       3 | 6328 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6329 | `			return SXERR_ABORT;` |
|       - | 6330 | `		}` |
|       1 | 6331 | `	}` |
|      58 | 6332 | `	return SXRET_OK;` |
|      30 | 6333 |  |
|       - | 6334 | `/*` |
|       - | 6335 | ` * Compile a switch block.` |
|       - | 6336 | ` *  (See block-comment below for more information)` |
|       - | 6337 | ` */` |
|      84 | 6338 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 6339 |  |
|      86 | 6340 | `	sxi32 rc = SXRET_OK;` |
|      86 | 6341 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 6342 | `		/* Unexpected token */` |
|     ! 0 | 6343 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 6344 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6345 | `			return SXERR_ABORT;` |
|       - | 6346 | `		}` |
|     ! 0 | 6347 | `		pGen->pIn++;` |
|     ! 0 | 6348 | `	}` |
|      86 | 6349 | `	pGen->pIn++;` |
|       - | 6350 | `	/* First instruction to execute in this block. */` |
|      86 | 6351 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 6352 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 6353 | `	 * or the '}' token */` |
|     151 | 6354 | `	for(;;){` |
|     304 | 6355 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6356 | `			/* No more input to process */` |
|     ! 0 | 6357 | `			break;` |
|       - | 6358 | `		}` |
|     304 | 6359 | `		rc = SXRET_OK;` |
|     304 | 6360 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      62 | 6361 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      20 | 6362 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 6363 | `					/* Unexpected token */` |
|     ! 0 | 6364 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 6365 | `						&pGen->pIn->sData);` |
|     ! 0 | 6366 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6367 | `						return SXERR_ABORT;` |
|       - | 6368 | `					}` |
|       - | 6369 | `					/* FALL THROUGH */` |
|     ! 0 | 6370 | `				}` |
|      20 | 6371 | `				rc = SXERR_EOF;` |
|      20 | 6372 | `				break;` |
|       - | 6373 | `			}` |
|      23 | 6374 | `		}else{` |
|       - | 6375 | `			sxi32 nKwrd;` |
|       - | 6376 | `			/* Extract the keyword */` |
|     244 | 6377 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     244 | 6378 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      34 | 6379 | `				break;` |
|       - | 6380 | `			}` |
|     180 | 6381 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 6382 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 6383 | `					/* Unexpected token */` |
|     ! 0 | 6384 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 6385 | `						&pGen->pIn->sData);` |
|     ! 0 | 6386 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6387 | `						return SXERR_ABORT;` |
|       - | 6388 | `					}` |
|       - | 6389 | `					/* FALL THROUGH */` |
|     ! 0 | 6390 | `				}` |
|       - | 6391 | `				/* Block compiled */` |
|       3 | 6392 | `				break;` |
|       - | 6393 | `			}` |
|       - | 6394 | `		}` |
|       - | 6395 | `		/* Compile block */` |
|     220 | 6396 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     220 | 6397 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6398 | `			return SXERR_ABORT;` |
|       - | 6399 | `		}` |
|       2 | 6400 | `	}` |
|      86 | 6401 | `	return rc;` |
|      44 | 6402 |  |
|       - | 6403 | `/*` |
|       - | 6404 | ` * Compile a case eXpression.` |
|       - | 6405 | ` *  (See block-comment below for more information)` |
|       - | 6406 | ` */` |
|      70 | 6407 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 6408 |  |
|       - | 6409 | `	SySet *pInstrContainer;` |
|       - | 6410 | `	SyToken *pEnd,*pTmp;` |
|      72 | 6411 | `	sxi32 iNest = 0;` |
|       - | 6412 | `	sxi32 rc;` |
|       - | 6413 | `	/* Delimit the expression */` |
|      72 | 6414 | `	pEnd = pGen->pIn;` |
|     150 | 6415 | `	while( pEnd < pGen->pEnd ){` |
|     150 | 6416 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 6417 | `			/* Increment nesting level */` |
|       3 | 6418 | `			iNest++;` |
|     149 | 6419 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 6420 | `			/* Decrement nesting level */` |
|       3 | 6421 | `			iNest--;` |
|     147 | 6422 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      72 | 6423 | `			break;` |
|       - | 6424 | `		}` |
|      80 | 6425 | `		pEnd++;` |
|       2 | 6426 | `	}` |
|      72 | 6427 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 6428 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 6429 | `		if( rc == SXERR_ABORT ){` |
|       - | 6430 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6431 | `			return SXERR_ABORT;` |
|       - | 6432 | `		}` |
|     ! 0 | 6433 | `	}` |
|       - | 6434 | `	/* Swap token stream */` |
|      72 | 6435 | `	pTmp = pGen->pEnd;` |
|      72 | 6436 | `	pGen->pEnd = pEnd;` |
|      72 | 6437 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      72 | 6438 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      72 | 6439 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 6440 | `	/* Emit the done instruction */` |
|      72 | 6441 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      72 | 6442 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 6443 | `	/* Update token stream */` |
|      72 | 6444 | `	pGen->pIn  = pEnd;` |
|      72 | 6445 | `	pGen->pEnd = pTmp;` |
|      72 | 6446 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6447 | `		return SXERR_ABORT;` |
|       - | 6448 | `	}` |
|      72 | 6449 | `	return SXRET_OK;` |
|      37 | 6450 |  |
|       - | 6451 | `/*` |
|       - | 6452 | ` * Compile the smart switch statement.` |
|       - | 6453 | ` * According to the PHP language reference manual` |
|       - | 6454 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 6455 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 6456 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 6457 | ` *  This is exactly what the switch statement is for.` |
|       - | 6458 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 6459 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 6460 | ` *  of the outer loop, use continue 2.` |
|       - | 6461 | ` *  Note that switch/case does loose comparision.` |
|       - | 6462 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 6463 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 6464 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 6465 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 6466 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 6467 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 6468 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 6469 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 6470 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 6471 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 6472 | ` *  list for the next case.` |
|       - | 6473 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 6474 | ` *  or floating-point numbers and strings.` |
|       - | 6475 | ` */` |
|      20 | 6476 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 6477 |  |
|       - | 6478 | `	GenBlock *pSwitchBlock;` |
|       - | 6479 | `	SyToken *pTmp,*pEnd;` |
|       - | 6480 | `	ph7_switch *pSwitch;` |
|       - | 6481 | `	sxu32 nToken;` |
|       - | 6482 | `	sxu32 nLine;` |
|       - | 6483 | `	sxi32 rc;` |
|      22 | 6484 | `	nLine = pGen->pIn->nLine;` |
|       - | 6485 | `	/* Jump the 'switch' keyword */` |
|      22 | 6486 | `	pGen->pIn++;` |
|      22 | 6487 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 6488 | `		/* Syntax error */` |
|     ! 0 | 6489 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 6490 | `		if( rc == SXERR_ABORT ){` |
|       - | 6491 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6492 | `			return SXERR_ABORT;` |
|       - | 6493 | `		}` |
|     ! 0 | 6494 | `		goto Synchronize;` |
|       - | 6495 | `	}` |
|       - | 6496 | `	/* Jump the left parenthesis '(' */` |
|      22 | 6497 | `	pGen->pIn++;` |
|      22 | 6498 | `	pEnd = 0; /* cc warning */` |
|       - | 6499 | `	/* Create the loop block */` |
|      32 | 6500 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      10 | 6501 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      22 | 6502 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6503 | `		return SXERR_ABORT;` |
|       - | 6504 | `	}` |
|       - | 6505 | `	/* Delimit the condition */` |
|      22 | 6506 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      22 | 6507 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 6508 | `		/* Empty expression */` |
|     ! 0 | 6509 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 6510 | `		if( rc == SXERR_ABORT ){` |
|       - | 6511 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6512 | `			return SXERR_ABORT;` |
|       - | 6513 | `		}` |
|     ! 0 | 6514 | `	}` |
|       - | 6515 | `	/* Swap token streams */` |
|      22 | 6516 | `	pTmp = pGen->pEnd;` |
|      22 | 6517 | `	pGen->pEnd = pEnd;` |
|       - | 6518 | `	/* Compile the expression */` |
|      22 | 6519 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      22 | 6520 | `	if( rc == SXERR_ABORT ){` |
|       - | 6521 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 6522 | `		return SXERR_ABORT;` |
|       - | 6523 | `	}` |
|       - | 6524 | `	/* Update token stream */` |
|      22 | 6525 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 6526 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6527 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 6528 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6529 | `			return SXERR_ABORT;` |
|       - | 6530 | `		}` |
|     ! 0 | 6531 | `		pGen->pIn++;` |
|     ! 0 | 6532 | `	}` |
|      22 | 6533 | `	pGen->pIn  = &pEnd[1];` |
|      22 | 6534 | `	pGen->pEnd = pTmp;` |
|      22 | 6535 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      20 | 6536 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 6537 | `			pTmp = pGen->pIn;` |
|     ! 0 | 6538 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 6539 | `				pTmp--;` |
|     ! 0 | 6540 | `			}` |
|       - | 6541 | `			/* Unexpected token */` |
|     ! 0 | 6542 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 6543 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6544 | `				return SXERR_ABORT;` |
|       - | 6545 | `			}` |
|     ! 0 | 6546 | `			goto Synchronize;` |
|       - | 6547 | `	}` |
|       - | 6548 | `	/* Set the delimiter token */` |
|      22 | 6549 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 6550 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 6551 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 6552 | `	}else{` |
|      20 | 6553 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 6554 | `	}` |
|      22 | 6555 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 6556 | `	/* Create the switch blocks container */` |
|      22 | 6557 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      22 | 6558 | `	if( pSwitch == 0 ){` |
|       - | 6559 | `		/* Abort compilation */` |
|     ! 0 | 6560 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6561 | `		return SXERR_ABORT;` |
|       - | 6562 | `	}` |
|       - | 6563 | `	/* Zero the structure */` |
|      22 | 6564 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 6565 | `	/* Initialize fields */` |
|      22 | 6566 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 6567 | `	/* Emit the switch instruction */` |
|      22 | 6568 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 6569 | `	/* Compile case blocks */` |
|      76 | 6570 | `	for(;;){` |
|       - | 6571 | `		sxu32 nKwrd;` |
|      88 | 6572 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6573 | `			/* No more input to process */` |
|     ! 0 | 6574 | `			break;` |
|       - | 6575 | `		}` |
|      88 | 6576 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6577 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 6578 | `				/* Unexpected token */` |
|     ! 0 | 6579 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 6580 | `					&pGen->pIn->sData);` |
|     ! 0 | 6581 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6582 | `					return SXERR_ABORT;` |
|       - | 6583 | `				}` |
|       - | 6584 | `				/* FALL THROUGH */` |
|     ! 0 | 6585 | `			}` |
|       - | 6586 | `			/* Block compiled */` |
|     ! 0 | 6587 | `			break;` |
|       - | 6588 | `		}` |
|       - | 6589 | `		/* Extract the keyword */` |
|      88 | 6590 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      88 | 6591 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 6592 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 6593 | `				/* Unexpected token */` |
|     ! 0 | 6594 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 6595 | `					&pGen->pIn->sData);` |
|     ! 0 | 6596 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6597 | `					return SXERR_ABORT;` |
|       - | 6598 | `				}` |
|       - | 6599 | `				/* FALL THROUGH */` |
|     ! 0 | 6600 | `			}` |
|       - | 6601 | `			/* Block compiled */` |
|       3 | 6602 | `			break;` |
|       - | 6603 | `		}` |
|      86 | 6604 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 6605 | `			/*` |
|       - | 6606 | `			 * Accroding to the PHP language reference manual` |
|       - | 6607 | `			 *  A special case is the default case. This case matches anything` |
|       - | 6608 | `			 *  that wasn't matched by the other cases.` |
|       - | 6609 | `			 */` |
|      16 | 6610 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 6611 | `				/* Default case already compiled */` |
|     ! 0 | 6612 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 6613 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6614 | `					return SXERR_ABORT;` |
|       - | 6615 | `				}` |
|     ! 0 | 6616 | `			}` |
|      16 | 6617 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 6618 | `			/* Compile the default block */` |
|      16 | 6619 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      16 | 6620 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 6621 | `				return SXERR_ABORT;` |
|      16 | 6622 | `			}else if( rc == SXERR_EOF ){` |
|      14 | 6623 | `				break;` |
|       1 | 6624 | `			}` |
|      73 | 6625 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 6626 | `			ph7_case_expr sCase;` |
|       - | 6627 | `			/* Standard case block */` |
|      72 | 6628 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 6629 | `			/* initialize the structure */` |
|      72 | 6630 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 6631 | `			/* Compile the case expression */` |
|      72 | 6632 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      72 | 6633 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6634 | `				return SXERR_ABORT;` |
|       - | 6635 | `			}` |
|       - | 6636 | `			/* Compile the case block */` |
|      72 | 6637 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 6638 | `			/* Insert in the switch container */` |
|      72 | 6639 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      72 | 6640 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 6641 | `				return SXERR_ABORT;` |
|      72 | 6642 | `			}else if( rc == SXERR_EOF ){` |
|       7 | 6643 | `				break;` |
|       - | 6644 | `			}` |
|      34 | 6645 | `		}else{` |
|       - | 6646 | `			/* Unexpected token */` |
|     ! 0 | 6647 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 6648 | `				&pGen->pIn->sData);` |
|     ! 0 | 6649 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6650 | `				return SXERR_ABORT;` |
|       - | 6651 | `			}` |
|     ! 0 | 6652 | `			break;` |
|       - | 6653 | `		}` |
|       2 | 6654 | `	}` |
|       - | 6655 | `	/* Fix all jumps now the destination is resolved */` |
|      22 | 6656 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      22 | 6657 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6658 | `	/* Release the loop block */` |
|      22 | 6659 | `	GenStateLeaveBlock(pGen,0);` |
|      22 | 6660 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 6661 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      22 | 6662 | `		pGen->pIn++;` |
|      10 | 6663 | `	}` |
|       - | 6664 | `	/* Statement successfully compiled */` |
|      22 | 6665 | `	return SXRET_OK;` |
|     ! 0 | 6666 | `Synchronize:` |
|       - | 6667 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 6668 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 6669 | `		pGen->pIn++;` |
|     ! 0 | 6670 | `	}` |
|     ! 0 | 6671 | `	return SXRET_OK;` |
|      12 | 6672 |  |
|       - | 6673 | `/*` |
|       - | 6674 | ` * Generate bytecode for a given expression tree.` |
|       - | 6675 | ` * If something goes wrong while generating bytecode` |
|       - | 6676 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 6677 | ` * this function takes care of generating the appropriate` |
|       - | 6678 | ` * error message.` |
|       - | 6679 | ` */` |
| 2049836 | 6680 | `static sxi32 GenStateEmitExprCode(` |
|       - | 6681 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 6682 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 6683 | `	sxi32 iFlags /* Control flags */` |
|       - | 6684 | `	)` |
|       2 | 6685 |  |
|       - | 6686 | `	VmInstr *pInstr;` |
|       - | 6687 | `	sxu32 nJmpIdx;` |
| 2049838 | 6688 | `	sxi32 iP1 = 0;` |
| 2049838 | 6689 | `	sxu32 iP2 = 0;` |
| 2049838 | 6690 | `	void *p3  = 0;` |
|       - | 6691 | `	sxi32 iVmOp;` |
|       - | 6692 | `	sxi32 rc;` |
| 2049838 | 6693 | `	if( pNode->xCode ){` |
|       - | 6694 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 6695 | `		/* Compile node */` |
| 1257634 | 6696 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1257634 | 6697 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1257634 | 6698 | `		RE_SWAP_DELIMITER(pGen);` |
| 1257634 | 6699 | `		return rc;` |
|       - | 6700 | `	}` |
|  792206 | 6701 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 6702 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 6703 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 6704 | `		return SXERR_ABORT;` |
|       - | 6705 | `	}` |
|  792206 | 6706 | `	iVmOp = pNode->pOp->iVmOp;` |
|  792206 | 6707 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 6708 | `		sxu32 nJz,nJmp;` |
|       - | 6709 | `		/* Ternary operator require special handling */` |
|       - | 6710 | `		/* Phase#1: Compile the condition */` |
|    1748 | 6711 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1748 | 6712 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6713 | `			return rc;` |
|       - | 6714 | `		}` |
|    1748 | 6715 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1748 | 6716 | `		if( pNode->pLeft ){` |
|       - | 6717 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 6718 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1680 | 6719 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 6720 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1680 | 6721 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1680 | 6722 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6723 | `				return rc;` |
|       - | 6724 | `			}` |
|     841 | 6725 | `		}else{` |
|       - | 6726 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 6727 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 6728 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 6729 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 6730 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 6731 | `		}` |
|       - | 6732 | `		/* Phase#4: Emit the unconditional jump */` |
|    1748 | 6733 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 6734 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1748 | 6735 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1748 | 6736 | `		if( pInstr ){` |
|    1748 | 6737 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     873 | 6738 | `		}` |
|    1748 | 6739 | `		if( !pNode->pLeft ){` |
|       - | 6740 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 6741 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 6742 | `		}` |
|       - | 6743 | `		/* Phase#6: Compile the 'else' expression */` |
|    1748 | 6744 | `		if( pNode->pRight ){` |
|    1748 | 6745 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1748 | 6746 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6747 | `				return rc;` |
|       - | 6748 | `			}` |
|     873 | 6749 | `		}` |
|    1748 | 6750 | `		if( nJmp > 0 ){` |
|       - | 6751 | `			/* Phase#7: Fix the unconditional jump */` |
|    1748 | 6752 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1748 | 6753 | `			if( pInstr ){` |
|    1748 | 6754 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     873 | 6755 | `			}` |
|     873 | 6756 | `		}` |
|       - | 6757 | `		/* All done */` |
|    1748 | 6758 | `		return SXRET_OK;` |
|       - | 6759 | `	}` |
|       - | 6760 | `	/* Generate code for the left tree */` |
|  790460 | 6761 | `	if( pNode->pLeft ){` |
|  790442 | 6762 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 6763 | `			ph7_expr_node **apNode;` |
|       - | 6764 | `			sxi32 n;` |
|       - | 6765 | `			/* Recurse and generate bytecodes for function arguments */` |
|  234504 | 6766 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 6767 | `			/* Read-only load */` |
|  234504 | 6768 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  461984 | 6769 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  227482 | 6770 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  227482 | 6771 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6772 | `					return rc;` |
|       - | 6773 | `				}` |
|  113742 | 6774 | `			}` |
|       - | 6775 | `			/* Total number of given arguments */` |
|  234504 | 6776 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 6777 | `			/* Remove stale flags now */` |
|  234504 | 6778 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  117251 | 6779 | `		}` |
|  790442 | 6780 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  790442 | 6781 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6782 | `			return rc;` |
|       - | 6783 | `		}` |
|  790442 | 6784 | `		if( iVmOp == PH7_OP_CALL ){` |
|  234504 | 6785 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  234504 | 6786 | `			if( pInstr ){` |
|  234504 | 6787 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  234196 | 6788 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 6789 | `					sxu32 nQual;` |
|       - | 6790 | `					/* Prevent constant expansion */` |
|  234196 | 6791 | `					pInstr->iP1 = 0;` |
|       - | 6792 | `					/* Namespace-qualify the function name for CALL */` |
|  234196 | 6793 | `					nQual = GenStateNsQualifyName(pGen,nOrig);` |
|  234196 | 6794 | `					pInstr->iP2 = (sxi32)nQual;` |
|  234196 | 6795 | `					if( nQual != nOrig ){` |
|       - | 6796 | `						/* Name was compiler-qualified: flag CALL for host-function global fallback.` |
|       - | 6797 | `						 * p3 = (void*)1 tells the VM it's safe to strip the NS prefix` |
|       - | 6798 | `						 * and try the short name in hHostFunction. */` |
|      49 | 6799 | `						p3 = (void *)1;` |
|      26 | 6800 | `					}` |
|  117407 | 6801 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 6802 | `					/* Method call,flag that */` |
|     298 | 6803 | `					pInstr->iP2 = 1;` |
|     148 | 6804 | `				}` |
|  117253 | 6805 | `			}` |
|  673191 | 6806 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 6807 | `			ph7_expr_node **apNode;` |
|       - | 6808 | `			sxi32 n;` |
|       - | 6809 | `			/* Recurse and generate bytecodes for array index */` |
|   62986 | 6810 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  113622 | 6811 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   50638 | 6812 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   50638 | 6813 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6814 | `					return rc;` |
|       - | 6815 | `				}` |
|   25320 | 6816 | `			}` |
|   62986 | 6817 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   50638 | 6818 | `				iP1 = 1; /* Node have an index associated with it */` |
|   25318 | 6819 | `			}` |
|   62986 | 6820 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 6821 | `				/* Create an empty entry when the desired index is not found */` |
|   24882 | 6822 | `				iP2 = 1;` |
|   12442 | 6823 | `			}` |
|  524448 | 6824 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 6825 | `			/* POP the left node */` |
|      32 | 6826 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 6827 | `		}` |
|  395220 | 6828 | `	}` |
|  790460 | 6829 | `	rc = SXRET_OK;` |
|  790460 | 6830 | `	nJmpIdx = 0;` |
|       - | 6831 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 6832 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 6833 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  790460 | 6834 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|      98 | 6835 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      98 | 6836 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      98 | 6837 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      98 | 6838 | `			int isSpecial = 0;` |
|      98 | 6839 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|      58 | 6840 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|      58 | 6841 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|      62 | 6842 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      52 | 6843 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      26 | 6844 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      38 | 6845 | `					isSpecial = 1;` |
|      18 | 6846 | `				}` |
|      38 | 6847 | `			}` |
|     118 | 6848 | `			pInstr->iP1 = 0;` |
|     118 | 6849 | `			if( !isSpecial ){` |
|      42 | 6850 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2);` |
|      20 | 6851 | `			}` |
|      38 | 6852 | `		}` |
|      72 | 6853 | `	}` |
|       - | 6854 | `	/* Generate code for the right tree */` |
|  790444 | 6855 | `	if( pNode->pRight ){` |
|  437140 | 6856 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 6857 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    7760 | 6858 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  433261 | 6859 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 6860 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2606 | 6861 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  428080 | 6862 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  191148 | 6863 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|   95573 | 6864 | `		}` |
|  437140 | 6865 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  437140 | 6866 | `		if( iVmOp == PH7_OP_STORE ){` |
|  188572 | 6867 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  188572 | 6868 | `			if( pInstr ){` |
|  188572 | 6869 | `				if( pInstr->iOp == PH7_OP_LOAD_LIST ){` |
|       - | 6870 | `					/* Hide the STORE instruction */` |
|      26 | 6871 | `					iVmOp = 0;` |
|  188560 | 6872 | `				}else if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 6873 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   41900 | 6874 | `					iP2 = 1;` |
|   20951 | 6875 | `				}else{` |
|  146650 | 6876 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 6877 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   24880 | 6878 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   24880 | 6879 | `						iP1 = pInstr->iP1;` |
|   12441 | 6880 | `					}else{` |
|  121772 | 6881 | `						p3 = pInstr->p3;` |
|       - | 6882 | `					}` |
|       - | 6883 | `					/* POP the last dynamic load instruction */` |
|  146650 | 6884 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 6885 | `				}` |
|   94287 | 6886 | `			}` |
|  342855 | 6887 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      44 | 6888 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      44 | 6889 | `			if( pInstr ){` |
|      44 | 6890 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 6891 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 6892 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 6893 | `					 */` |
|      15 | 6894 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 6895 | `					iP1 = pInstr->iP1;` |
|      15 | 6896 | `					iP2 = pInstr->iP2;` |
|      15 | 6897 | `					p3  = pInstr->p3;` |
|       8 | 6898 | `				}else{` |
|      30 | 6899 | `					p3 = pInstr->p3;` |
|       - | 6900 | `				}` |
|      21 | 6901 | `			}` |
|      21 | 6902 | `		}` |
|  218569 | 6903 | `	}` |
|  790444 | 6904 | `	if( iVmOp > 0 ){` |
|  790390 | 6905 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   10058 | 6906 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 6907 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    7388 | 6908 | `				iP1 = 1;` |
|    3695 | 6909 | `			}` |
|  785362 | 6910 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 6911 | `			/* Namespace-qualify the class name for NEW */ {` |
|   12622 | 6912 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   12622 | 6913 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   12610 | 6914 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    6304 | 6915 | `				}` |
|   12622 | 6916 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 6917 | `					/* Prevent constant expansion for class name */` |
|   12620 | 6918 | `					pPeek->iP1 = 0;` |
|   12620 | 6919 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pPeek->iP2);` |
|    6309 | 6920 | `				}` |
|       - | 6921 | `			}` |
|   12622 | 6922 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   12622 | 6923 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 6924 | `				VmInstr *pPrev;` |
|   12610 | 6925 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   12610 | 6926 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 6927 | `					/* Pop the call instruction */` |
|   12610 | 6928 | `					iP1 = pInstr->iP1;` |
|   12610 | 6929 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    6304 | 6930 | `				}` |
|    6306 | 6931 | `			}` |
|  774024 | 6932 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 6933 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 6934 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 6935 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 6936 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 6937 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 6938 | `				int isSpecialIs = 0;` |
|      50 | 6939 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 6940 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 6941 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 6942 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 6943 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 6944 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 6945 | `						isSpecialIs = 1;` |
|       5 | 6946 | `					}` |
|      23 | 6947 | `				}` |
|      52 | 6948 | `				pInstr->iP1 = 0;` |
|      52 | 6949 | `				if( !isSpecialIs ){` |
|      38 | 6950 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2);` |
|      18 | 6951 | `				}` |
|      25 | 6952 | `			}` |
|  767693 | 6953 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 6954 | `			/* Prevent constant expansion for member/property names.` |
|       - | 6955 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 6956 | `			 * should not trigger constant lookup. */` |
|   94120 | 6957 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   94120 | 6958 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   94104 | 6959 | `				pInstr->iP1 = 0;` |
|   47051 | 6960 | `			}` |
|   94120 | 6961 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 6962 | `				/* Static member access,remember that */` |
|      82 | 6963 | `				iP1 = 1;` |
|      82 | 6964 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      82 | 6965 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      10 | 6966 | `					p3 = pInstr->p3;` |
|      10 | 6967 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       4 | 6968 | `				}` |
|      40 | 6969 | `			}` |
|   47059 | 6970 | `		}` |
|       - | 6971 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  790388 | 6972 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  790388 | 6973 | `		if( nJmpIdx > 0 ){` |
|       - | 6974 | `			/* Fix short-circuited jumps now the destination is resolved */` |
|   10364 | 6975 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   10364 | 6976 | `			if( pInstr ){` |
|   10364 | 6977 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5181 | 6978 | `			}` |
|    5181 | 6979 | `		}` |
|  395193 | 6980 | `	}` |
|  790442 | 6981 | `	return rc;` |
| 1024911 | 6982 |  |
|       - | 6983 | `/*` |
|       - | 6984 | ` * Compile a PHP expression.` |
|       - | 6985 | ` * According to the PHP language reference manual:` |
|       - | 6986 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 6987 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 6988 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 6989 | ` *  is "anything that has a value".` |
|       - | 6990 | ` * If something goes wrong while compiling the expression,this` |
|       - | 6991 | ` * function takes care of generating the appropriate error` |
|       - | 6992 | ` * message.` |
|       - | 6993 | ` */` |
|  539120 | 6994 | `static sxi32 PH7_CompileExpr(` |
|       - | 6995 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 6996 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 6997 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 6998 | `	)` |
|       2 | 6999 |  |
|       - | 7000 | `	ph7_expr_node *pRoot;` |
|       - | 7001 | `	SySet sExprNode;` |
|       - | 7002 | `	SyToken *pEnd;` |
|       - | 7003 | `	sxi32 nExpr;` |
|       - | 7004 | `	sxi32 iNest;` |
|       - | 7005 | `	sxi32 rc;` |
|       - | 7006 | `	/* Initialize worker variables */` |
|  539122 | 7007 | `	nExpr = 0;` |
|  539122 | 7008 | `	pRoot = 0;` |
|  539122 | 7009 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  539122 | 7010 | `	SySetAlloc(&sExprNode,0x10);` |
|  539122 | 7011 | `	rc = SXRET_OK;` |
|       - | 7012 | `	/* Delimit the expression */` |
|  539122 | 7013 | `	pEnd = pGen->pIn;` |
|  539122 | 7014 | `	iNest = 0;` |
| 3692758 | 7015 | `	while( pEnd < pGen->pEnd ){` |
| 3497914 | 7016 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7017 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     180 | 7018 | `			iNest++;` |
| 3497825 | 7019 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     188 | 7020 | `			iNest--;` |
| 3497643 | 7021 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  344414 | 7022 | `			if( iNest <= 0 ){` |
|  344278 | 7023 | `				break;` |
|       - | 7024 | `			}` |
|      68 | 7025 | `		}` |
| 3153638 | 7026 | `		pEnd++;` |
|       2 | 7027 | `	}` |
|  539122 | 7028 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   10008 | 7029 | `		SyToken *pEnd2 = pGen->pIn;` |
|   10008 | 7030 | `		iNest = 0;` |
|       - | 7031 | `		/* Stop at the first comma */` |
|   20038 | 7032 | `		while( pEnd2 < pEnd ){` |
|   10032 | 7033 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       6 | 7034 | `				iNest++;` |
|   10030 | 7035 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       6 | 7036 | `				iNest--;` |
|   10026 | 7037 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 7038 | `				if( iNest <= 0 ){` |
|     ! 0 | 7039 | `					break;` |
|       - | 7040 | `				}` |
|       2 | 7041 | `			}` |
|   10032 | 7042 | `			pEnd2++;` |
|       2 | 7043 | `		}` |
|   10008 | 7044 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 7045 | `			pEnd = pEnd2;` |
|     ! 0 | 7046 | `		}` |
|    5003 | 7047 | `	}` |
|  539122 | 7048 | `	if( pEnd > pGen->pIn ){` |
|  539112 | 7049 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 7050 | `		/* Swap delimiter */` |
|  539112 | 7051 | `		pGen->pEnd = pEnd;` |
|       - | 7052 | `		/* Try to get an expression tree */` |
|  539112 | 7053 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  539112 | 7054 | `		if( rc == SXRET_OK && pRoot ){` |
|  538956 | 7055 | `			rc = SXRET_OK;` |
|  538956 | 7056 | `			if( xTreeValidator ){` |
|       - | 7057 | `				/* Call the upper layer validator callback */` |
|   12768 | 7058 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    6383 | 7059 | `			}` |
|  538956 | 7060 | `			if( rc != SXERR_ABORT ){` |
|       - | 7061 | `				/* Generate code for the given tree */` |
|  538956 | 7062 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  269477 | 7063 | `			}` |
|  538956 | 7064 | `			nExpr = 1;` |
|  269477 | 7065 | `		}` |
|       - | 7066 | `		/* Release the whole tree */` |
|  539112 | 7067 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 7068 | `		/* Synchronize token stream */` |
|  539112 | 7069 | `		pGen->pEnd = pTmp;` |
|  539112 | 7070 | `		pGen->pIn  = pEnd;` |
|  539112 | 7071 | `		if( rc == SXERR_ABORT ){` |
|       3 | 7072 | `			SySetRelease(&sExprNode);` |
|       3 | 7073 | `			return SXERR_ABORT;` |
|       - | 7074 | `		}` |
|  269554 | 7075 | `	}` |
|  539120 | 7076 | `	SySetRelease(&sExprNode);` |
|  539120 | 7077 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  269562 | 7078 |  |
|       - | 7079 | `/*` |
|       - | 7080 | ` * Return a pointer to the node construct handler associated` |
|       - | 7081 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 7082 | ` */` |
|  147240 | 7083 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 7084 |  |
|  147242 | 7085 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 7086 | `		/* Numeric literal: Either real or integer */` |
|   80724 | 7087 | `		return PH7_CompileNumLiteral;` |
|   66520 | 7088 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 7089 | `		/* Double quoted string */` |
|   13488 | 7090 | `		return PH7_CompileString;` |
|   53034 | 7091 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 7092 | `		/* Single quoted string */` |
|   52974 | 7093 | `		return PH7_CompileSimpleString;` |
|      62 | 7094 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 7095 | `		/* Heredoc */` |
|      28 | 7096 | `		return PH7_CompileHereDoc;` |
|      36 | 7097 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 7098 | `		/* Nowdoc */` |
|      29 | 7099 | `		return PH7_CompileNowDoc;` |
|       7 | 7100 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 7101 | `		/* Backtick quoted string */` |
|       5 | 7102 | `		return PH7_CompileBacktic;` |
|       - | 7103 | `	}` |
|       3 | 7104 | `	return 0;` |
|   73622 | 7105 |  |
|       - | 7106 | `/*` |
|       - | 7107 | ` * PHP Language construct table.` |
|       - | 7108 | ` */` |
|       - | 7109 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 7110 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 7111 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 7112 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 7113 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 7114 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 7115 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 7116 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 7117 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 7118 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 7119 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 7120 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 7121 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 7122 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 7123 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 7124 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 7125 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 7126 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 7127 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 7128 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 7129 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 7130 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 7131 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 7132 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  }   /* declare statement */` |
|       - | 7133 | `};` |
|       - | 7134 | `/*` |
|       - | 7135 | ` * Return a pointer to the statement handler routine associated` |
|       - | 7136 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 7137 | ` */` |
|  309988 | 7138 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 7139 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 7140 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 7141 | `	)` |
|       2 | 7142 |  |
|  309990 | 7143 | `	sxu32 n = 0;` |
| 1174364 | 7144 | `	for(;;){` |
| 2348730 | 7145 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   32432 | 7146 | `			break;` |
|       - | 7147 | `		}` |
| 2316300 | 7148 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  277560 | 7149 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 7150 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 7151 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 7152 | `					/* 'static' (class context),return null */` |
|     ! 0 | 7153 | `					return 0;` |
|       - | 7154 | `				}` |
|     ! 0 | 7155 | `			}` |
|       - | 7156 | `			/* Return a pointer to the handler.` |
|       - | 7157 | `			*/` |
|  277560 | 7158 | `			return aLangConstruct[n].xConstruct;` |
|       - | 7159 | `		}` |
| 2038742 | 7160 | `		n++;` |
|       2 | 7161 | `	}` |
|   32432 | 7162 | `	if( pLookahed ){` |
|   32432 | 7163 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    7394 | 7164 | `			return PH7_CompileClassInterface;` |
|   25040 | 7165 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   22470 | 7166 | `			return PH7_CompileClass;` |
|    2572 | 7167 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      43 | 7168 | `			return PH7_CompileTrait;` |
|    2528 | 7169 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       7 | 7170 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       6 | 7171 | `				return PH7_CompileAbstractClass;` |
|    2524 | 7172 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 7173 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 7174 | `				return PH7_CompileFinalClass;` |
|       - | 7175 | `		}` |
|    1261 | 7176 | `	}` |
|       - | 7177 | `	/* Not a language construct */` |
|    2524 | 7178 | `	return 0;` |
|  154996 | 7179 |  |
|       - | 7180 | `/*` |
|       - | 7181 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 7182 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 7183 | ` */` |
|    2522 | 7184 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 7185 |  |
|       - | 7186 | `	int rc;` |
|    2524 | 7187 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|    2524 | 7188 | `	if( rc == FALSE ){` |
|      14 | 7189 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|       - | 7190 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 7191 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 7192 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 7193 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 7194 | `			*/` |
|       - | 7195 | `			){` |
|       6 | 7196 | `				rc = TRUE;` |
|       2 | 7197 | `		}` |
|       6 | 7198 | `	}` |
|    2524 | 7199 | `	return rc;` |
|       2 | 7200 |  |
|       - | 7201 | `/*` |
|       - | 7202 | ` * Compile a PHP chunk.` |
|       - | 7203 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 7204 | ` * takes care of generating the appropriate error message.` |
|       - | 7205 | ` */` |
|  440060 | 7206 | `static sxi32 GenStateCompileChunk(` |
|       - | 7207 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7208 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 7209 | `	)` |
|       2 | 7210 |  |
|       - | 7211 | `	ProcLangConstruct xCons;` |
|       - | 7212 | `	sxi32 rc;` |
|  440062 | 7213 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  260699 | 7214 | `	for(;;){` |
|  521400 | 7215 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7216 | `			/* No more input to process */` |
|   10646 | 7217 | `			break;` |
|       - | 7218 | `		}` |
|  510756 | 7219 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7220 | `			/* Compile block */` |
|      12 | 7221 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 7222 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7223 | `				break;` |
|       - | 7224 | `			}` |
|       7 | 7225 | `		}else{` |
|  510746 | 7226 | `			xCons = 0;` |
|  510746 | 7227 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  309990 | 7228 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 7229 | `				/* Try to extract a language construct handler */` |
|  309990 | 7230 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  309990 | 7231 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 7232 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7233 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 7234 | `						&pGen->pIn->sData);` |
|       9 | 7235 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7236 | `						break;` |
|       - | 7237 | `					}` |
|       - | 7238 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 7239 | `					 * this erroneous statement.` |
|       - | 7240 | `					 */` |
|       9 | 7241 | `					xCons = PH7_ErrorRecover;` |
|       4 | 7242 | `				}` |
|  355752 | 7243 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   28958 | 7244 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 7245 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 7246 | `				xCons = PH7_CompileLabel;` |
|      56 | 7247 | `			}` |
|  510746 | 7248 | `			if( xCons == 0 ){` |
|       - | 7249 | `				/* Assume an expression an try to compile it */` |
|  203160 | 7250 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  203160 | 7251 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 7252 | `					/* Pop l-value */` |
|  203034 | 7253 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  101516 | 7254 | `				}` |
|  101581 | 7255 | `			}else{` |
|       - | 7256 | `				/* Go compile the sucker */` |
|  307588 | 7257 | `				rc = xCons(&(*pGen));` |
|       - | 7258 | `			}` |
|  510746 | 7259 | `			if( rc == SXERR_ABORT ){` |
|       - | 7260 | `				/* Request to abort compilation */` |
|       3 | 7261 | `				break;` |
|       - | 7262 | `			}` |
|       - | 7263 | `		}` |
|       - | 7264 | `		/* Ignore trailing semi-colons ';' */` |
|  837620 | 7265 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  326868 | 7266 | `			pGen->pIn++;` |
|       2 | 7267 | `		}` |
|  510754 | 7268 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 7269 | `			/* Compile a single statement and return */` |
|  429416 | 7270 | `			break;` |
|       - | 7271 | `		}` |
|       - | 7272 | `		/* LOOP ONE */` |
|       - | 7273 | `		/* LOOP TWO */` |
|       - | 7274 | `		/* LOOP THREE */` |
|       - | 7275 | `		/* LOOP FOUR */` |
|       2 | 7276 | `	}` |
|       - | 7277 | `	/* Return compilation status */` |
|  440062 | 7278 | `	return rc;` |
|       2 | 7279 |  |
|       - | 7280 | `/*` |
|       - | 7281 | ` * Compile a Raw PHP chunk.` |
|       - | 7282 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 7283 | ` * takes care of generating the appropriate error message.` |
|       - | 7284 | ` */` |
|   10648 | 7285 | `static sxi32 PH7_CompilePHP(` |
|       - | 7286 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7287 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 7288 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 7289 | `	)` |
|       2 | 7290 |  |
|   10650 | 7291 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 7292 | `	sxi32 rc;` |
|       - | 7293 | `	/* Reset the token set */` |
|   10650 | 7294 | `	SySetReset(&(*pTokenSet));` |
|       - | 7295 | `	/* Mark as the default token set */` |
|   10650 | 7296 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 7297 | `	/* Advance the stream cursor */` |
|   10650 | 7298 | `	pGen->pRawIn++;` |
|       - | 7299 | `	/* Tokenize the PHP chunk first */` |
|   10650 | 7300 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 7301 | `	/* Point to the head and tail of the token stream. */` |
|   10650 | 7302 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   10650 | 7303 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   10650 | 7304 | `	if( is_expr ){` |
|     ! 0 | 7305 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 7306 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 7307 | `			/* A simple expression,compile it */` |
|     ! 0 | 7308 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 7309 | `		}` |
|       - | 7310 | `		/* Emit the DONE instruction */` |
|     ! 0 | 7311 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 7312 | `		return SXRET_OK;` |
|       - | 7313 | `	}` |
|   10650 | 7314 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 7315 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 7316 | `		/*` |
|       - | 7317 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 7318 | `		 * According to the PHP reference manual:` |
|       - | 7319 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 7320 | `		 *  immediately follow` |
|       - | 7321 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 7322 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 7323 | `		 * Symisc extension:` |
|       - | 7324 | `		 *   This short syntax works with all PHP opening` |
|       - | 7325 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 7326 | `		 *   only short tag.` |
|       - | 7327 | `		 */` |
|       - | 7328 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 7329 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 7330 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 7331 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 7332 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 7333 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 7334 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 7335 | `		}` |
|       3 | 7336 | `		return SXRET_OK;` |
|       - | 7337 | `	}` |
|       - | 7338 | `	/* Compile the PHP chunk */` |
|   10648 | 7339 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 7340 | `	/* Fix exceptions jumps */` |
|   10648 | 7341 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7342 | `	/* Fix gotos now, the jump destination is resolved */` |
|   10648 | 7343 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 7344 | `		rc = SXERR_ABORT;` |
|       1 | 7345 | `	}` |
|       - | 7346 | `	/* Reset container */` |
|   10648 | 7347 | `	SySetReset(&pGen->aGoto);` |
|   10648 | 7348 | `	SySetReset(&pGen->aLabel);` |
|       - | 7349 | `	/* Compilation result */` |
|   10648 | 7350 | `	return rc;` |
|    5326 | 7351 |  |
|       - | 7352 | `/*` |
|       - | 7353 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 7354 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 7355 | ` * This is the only compile interface exported from this file.` |
|       - | 7356 | ` */` |
|   12468 | 7357 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 7358 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 7359 | `	SyString *pScript,  /* Script to compile */` |
|       - | 7360 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 7361 | `	)` |
|       2 | 7362 |  |
|       - | 7363 | `	SySet aPhpToken,aRawToken;` |
|       - | 7364 | `	ph7_gen_state *pCodeGen;` |
|       - | 7365 | `	ph7_value *pRawObj;` |
|       - | 7366 | `	sxu32 nObjIdx;` |
|       - | 7367 | `	sxi32 nRawObj;` |
|       - | 7368 | `	int is_expr;` |
|       - | 7369 | `	sxi32 rc;` |
|   12470 | 7370 | `	if( pScript->nByte < 1 ){` |
|       - | 7371 | `		/* Nothing to compile */` |
|     ! 0 | 7372 | `		return PH7_OK;` |
|       - | 7373 | `	}` |
|       - | 7374 | `	/* Initialize the tokens containers */` |
|   12470 | 7375 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   12470 | 7376 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   12470 | 7377 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   12470 | 7378 | `	is_expr = 0;` |
|   12470 | 7379 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 7380 | `		SyToken sTmp;` |
|       - | 7381 | `		/* PHP only: -*/` |
|    2484 | 7382 | `		sTmp.nLine = 1;` |
|    2484 | 7383 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2484 | 7384 | `		sTmp.pUserData = 0;` |
|    2484 | 7385 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2484 | 7386 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2484 | 7387 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 7388 | `			/* A simple PHP expression */` |
|     ! 0 | 7389 | `			is_expr = 1;` |
|     ! 0 | 7390 | `		}` |
|    1243 | 7391 | `	}else{` |
|       - | 7392 | `		/* Tokenize raw text */` |
|    9988 | 7393 | `		SySetAlloc(&aRawToken,32);` |
|    9988 | 7394 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 7395 | `	}` |
|   12470 | 7396 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 7397 | `	/* Process high-level tokens */` |
|   12470 | 7398 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   12470 | 7399 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   12470 | 7400 | `	rc = PH7_OK;` |
|   12470 | 7401 | `	if( is_expr ){` |
|       - | 7402 | `		/* Compile the expression */` |
|     ! 0 | 7403 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 7404 | `		goto cleanup;` |
|       - | 7405 | `	}` |
|   12470 | 7406 | `	nObjIdx = 0;` |
|       - | 7407 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 7408 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 7409 | `	 * preventing namespace bleeding across include()d files. */` |
|   12470 | 7410 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 7411 | `	/* Start the compilation process */` |
|   11231 | 7412 | `	for(;;){` |
|   33108 | 7413 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   12466 | 7414 | `			break; /* No more tokens to process */` |
|       - | 7415 | `		}` |
|   20644 | 7416 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 7417 | `			/* Compile the PHP chunk */` |
|   10650 | 7418 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   10650 | 7419 | `			if( rc == SXERR_ABORT ){` |
|       5 | 7420 | `				break;` |
|       - | 7421 | `			}` |
|   10646 | 7422 | `			continue;` |
|       - | 7423 | `		}` |
|       - | 7424 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    9996 | 7425 | `		nRawObj = 0;` |
|   19990 | 7426 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 7427 | `			/* Consume the raw chunk without any processing */` |
|    9996 | 7428 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    9996 | 7429 | `			if( pRawObj == 0 ){` |
|     ! 0 | 7430 | `				rc = SXERR_MEM;` |
|     ! 0 | 7431 | `				break;` |
|       - | 7432 | `			}` |
|       - | 7433 | `			/* Mark as constant and emit the load constant instruction */` |
|    9996 | 7434 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    9996 | 7435 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    9996 | 7436 | `			++nRawObj;` |
|    9996 | 7437 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 7438 | `		}` |
|    9996 | 7439 | `		if( nRawObj > 0 ){` |
|       - | 7440 | `			/* Emit the consume instruction */` |
|    9996 | 7441 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    4997 | 7442 | `		}` |
|    6236 | 7443 | `	}` |
|    6234 | 7444 | `cleanup:` |
|   12470 | 7445 | `	SySetRelease(&aRawToken);` |
|   12470 | 7446 | `	SySetRelease(&aPhpToken);` |
|   12470 | 7447 | `	return rc;` |
|    6236 | 7448 |  |
|       - | 7449 | `/*` |
|       - | 7450 | ` * Utility routines.Initialize the code generator.` |
|       - | 7451 | ` */` |
|    2460 | 7452 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 7453 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 7454 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 7455 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 7456 | `	)` |
|       2 | 7457 |  |
|    2462 | 7458 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 7459 | `	/* Zero the structure */` |
|    2462 | 7460 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 7461 | `	/* Initial state */` |
|    2462 | 7462 | `	pGen->pVm  = &(*pVm);` |
|    2462 | 7463 | `	pGen->xErr = xErr;` |
|    2462 | 7464 | `	pGen->pErrData = pErrData;` |
|    2462 | 7465 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2462 | 7466 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2462 | 7467 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2462 | 7468 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 7469 | `	/* Error log buffer */` |
|    2462 | 7470 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 7471 | `	/* General purpose working buffer */` |
|    2462 | 7472 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 7473 | `	/* Namespace state */` |
|    2462 | 7474 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2462 | 7475 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 7476 | `	/* Create the global scope */` |
|    2462 | 7477 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 7478 | `	/* Point to the global scope */` |
|    2462 | 7479 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2462 | 7480 | `	return SXRET_OK;` |
|       2 | 7481 |  |
|       - | 7482 | `/*` |
|       - | 7483 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 7484 | ` */` |
|   14692 | 7485 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 7486 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 7487 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 7488 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 7489 | `	)` |
|       2 | 7490 |  |
|   14694 | 7491 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 7492 | `	GenBlock *pBlock,*pParent;` |
|       - | 7493 | `	/* Reset state */` |
|   14694 | 7494 | `	SySetReset(&pGen->aLabel);` |
|   14694 | 7495 | `	SySetReset(&pGen->aGoto);` |
|   14694 | 7496 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   14694 | 7497 | `	SyBlobRelease(&pGen->sWorker);` |
|   14694 | 7498 | `	SyBlobRelease(&pGen->sNamespace);` |
|   14694 | 7499 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   14694 | 7500 | `	SyHashRelease(&pGen->hUseImports);` |
|   14694 | 7501 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 7502 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 7503 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 7504 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 7505 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 7506 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 7507 | `	 * number of unique names, which is acceptable. */` |
|       - | 7508 | `	/* Point to the global scope */` |
|   14694 | 7509 | `	pBlock = pGen->pCurrent;` |
|   14694 | 7510 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 7511 | `		pParent = pBlock->pParent;` |
|     ! 0 | 7512 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 7513 | `		pBlock = pParent;` |
|     ! 0 | 7514 | `	}` |
|   14694 | 7515 | `	pGen->xErr = xErr;` |
|   14694 | 7516 | `	pGen->pErrData = pErrData;` |
|   14694 | 7517 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   14694 | 7518 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   14694 | 7519 | `	pGen->pIn = pGen->pEnd = 0;` |
|   14694 | 7520 | `	pGen->nErr = 0;` |
|   14694 | 7521 | `	return SXRET_OK;` |
|       2 | 7522 |  |
|       - | 7523 | `/*` |
|       - | 7524 | ` * Generate a compile-time error message.` |
|       - | 7525 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 7526 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 7527 | ` * abort compilation immediately.` |
|       - | 7528 | ` */` |
|     432 | 7529 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 7530 |  |
|     434 | 7531 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     434 | 7532 | `	const char *zErr = "Error";` |
|       - | 7533 | `	SyString *pFile;` |
|       - | 7534 | `	va_list ap;` |
|       - | 7535 | `	sxi32 rc;` |
|       - | 7536 | `	/* Reset the working buffer */` |
|     434 | 7537 | `	SyBlobReset(pWorker);` |
|       - | 7538 | `	/* Peek the processed file path if available */` |
|     434 | 7539 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     434 | 7540 | `	if( nErrType == E_ERROR ){` |
|       - | 7541 | `		/* Increment the error counter */` |
|     390 | 7542 | `		pGen->nErr++;` |
|     390 | 7543 | `		if( pGen->nErr > 15 ){` |
|       - | 7544 | `			/* Error count limit reached */` |
|       5 | 7545 | `			if( pGen->xErr ){` |
|       5 | 7546 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 7547 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 7548 | `				if( pFile ){` |
|       5 | 7549 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 7550 | `				}` |
|       5 | 7551 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 7552 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 7553 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 7554 | `				}` |
|       2 | 7555 | `			}` |
|       - | 7556 | `			/* Abort immediately */` |
|       5 | 7557 | `			return SXERR_ABORT;` |
|       - | 7558 | `		}` |
|     192 | 7559 | `	}` |
|     430 | 7560 | `	if( pGen->xErr == 0 ){` |
|       - | 7561 | `		/* No available error consumer,return immediately */` |
|       3 | 7562 | `		return SXRET_OK;` |
|       - | 7563 | `	}` |
|     427 | 7564 | `	switch(nErrType){` |
|     383 | 7565 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      31 | 7566 | `	case E_WARNING: zErr = "Warning";     break;` |
|       7 | 7567 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 7568 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 7569 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 7570 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 7571 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 7572 | `	default:` |
|     ! 0 | 7573 | `		break;` |
|       - | 7574 | `	}` |
|     427 | 7575 | `	rc = SXRET_OK;` |
|       - | 7576 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     427 | 7577 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     427 | 7578 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     427 | 7579 | `	va_start(ap,zFormat);` |
|     427 | 7580 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     427 | 7581 | `	va_end(ap);` |
|     427 | 7582 | `	if( pFile ){` |
|     427 | 7583 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     213 | 7584 | `	}` |
|       - | 7585 | `	/* Append a new line */` |
|     427 | 7586 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     427 | 7587 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 7588 | `		/* Consume the generated error message */` |
|     427 | 7589 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     213 | 7590 | `	}` |
|     427 | 7591 | `	return rc;` |
|     218 | 7592 |  |
|       - | 7593 |  |
