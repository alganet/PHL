# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2806/3755 lines (74.73%)

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
|    2542 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    2544 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    7111 |  131 | `	for(;;){` |
|   14224 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2432 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2432 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2410 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      11 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   11816 |  140 | `		pBlock = pBlock->pParent;` |
|   11816 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1273 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  404094 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  404096 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  404096 |  162 | `	pBlock->pUserData   = pUserData;` |
|  404096 |  163 | `	pBlock->pGen        = pGen;` |
|  404096 |  164 | `	pBlock->iFlags      = iType;` |
|  404096 |  165 | `	pBlock->pParent     = 0;` |
|  404096 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  404096 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  404096 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  401790 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  401792 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  401792 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  401792 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  401792 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  401792 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  401792 |  200 | `	pGen->pCurrent = pBlock;` |
|  401792 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  193008 |  203 | `		*ppBlock = pBlock;` |
|   96503 |  204 | `	}` |
|  401792 |  205 | `	return SXRET_OK;` |
|  200897 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  401784 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  401786 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  401786 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  401786 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  401784 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  401786 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  401786 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  401786 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  401786 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  401784 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  401786 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  401786 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  401786 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  401786 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  401786 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  401786 |  244 | `	return SXRET_OK;` |
|  200894 |  245 |  |
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
|  149512 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  149514 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  149514 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  149514 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  149514 |  265 | `	return rc;` |
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
|  305446 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  305448 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  596976 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  291530 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  113590 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  177942 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   28432 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  149512 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  149512 |  298 | `		if( pInstr ){` |
|  149512 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  149512 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  149512 |  302 | `			aFix[n].nJumpType = -1;` |
|   74755 |  303 | `		}` |
|   74757 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  305448 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|   89460 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|   89462 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|   89608 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|   89460 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|   89592 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|   89460 |  358 | `	return SXRET_OK;` |
|   44732 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  387536 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  387538 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  387538 |  367 | `	if( pEntry == 0 ){` |
|  169412 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  218128 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  218128 |  371 | `	return SXRET_OK;` |
|  193770 |  372 |  |
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
|  169410 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  169412 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  169412 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   84705 |  387 | `	}` |
|  169412 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   75298 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   75300 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   75300 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   75300 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   75300 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   75300 |  408 | `	return pObj;` |
|   37651 |  409 |  |
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
|   75698 |  431 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  432 |  |
|   75700 |  433 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   75700 |  434 | `	sxu32 nIdx = 0;` |
|   75700 |  435 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  436 | `		ph7_value *pObj;` |
|       - |  437 | `		sxi64 iValue;` |
|   75300 |  438 | `		iValue = PH7_TokenValueToInt64(&pToken->sData);` |
|   75300 |  439 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   75300 |  440 | `		if( pObj == 0 ){` |
|     ! 0 |  441 | `			SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  442 | `			return SXERR_ABORT;` |
|       - |  443 | `		}` |
|   75300 |  444 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   37651 |  445 | `	}else{` |
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
|   75700 |  458 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  459 | `	/* Node successfully compiled */` |
|   75700 |  460 | `	return SXRET_OK;` |
|   37851 |  461 |  |
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
|   50100 |  473 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  474 |  |
|   50102 |  475 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  476 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  477 | `	ph7_value *pObj;` |
|       - |  478 | `	sxu32 nIdx;` |
|   50102 |  479 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  480 | `	/* Delimit the string */` |
|   50102 |  481 | `	zIn  = pStr->zString;` |
|   50102 |  482 | `	zEnd = &zIn[pStr->nByte];` |
|   50102 |  483 | `	if( zIn >= zEnd ){` |
|       - |  484 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  485 | `		 * rather than reserving a new object each time. */` |
|     112 |  486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     112 |  487 | `		return SXRET_OK;` |
|       - |  488 | `	}` |
|   49992 |  489 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  490 | `		/* Already processed,emit the load constant instruction` |
|       - |  491 | `		 * and return.` |
|       - |  492 | `		 */` |
|   15088 |  493 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   15088 |  494 | `		return SXRET_OK;` |
|       - |  495 | `	}` |
|       - |  496 | `	/* Reserve a new constant */` |
|   34906 |  497 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   34906 |  498 | `	if( pObj == 0 ){` |
|     ! 0 |  499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  500 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  501 | `		return SXERR_ABORT;` |
|       - |  502 | `	}` |
|   34906 |  503 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  504 | `	/* Compile the node */` |
|   34921 |  505 | `	for(;;){` |
|   69844 |  506 | `		if( zIn >= zEnd ){` |
|       - |  507 | `			/* End of input */` |
|   34906 |  508 | `			break;` |
|       - |  509 | `		}` |
|   34940 |  510 | `		zCur = zIn;` |
|  551580 |  511 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  516642 |  512 | `			zIn++;` |
|       2 |  513 | `		}` |
|   34940 |  514 | `		if( zIn > zCur ){` |
|       - |  515 | `			/* Append raw contents*/` |
|   34922 |  516 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   17460 |  517 | `		}` |
|   34940 |  518 | `		zIn++;` |
|   34940 |  519 | `		if( zIn < zEnd ){` |
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
|   34940 |  534 | `		zIn++;` |
|       2 |  535 | `	}` |
|       - |  536 | `	/* Emit the load constant instruction */` |
|   34906 |  537 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   34906 |  538 | `	if( pStr->nByte < 1024 ){` |
|       - |  539 | `		/* Install in the literal table */` |
|   34906 |  540 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   17452 |  541 | `	}` |
|       - |  542 | `	/* Node successfully compiled */` |
|   34906 |  543 | `	return SXRET_OK;` |
|   25052 |  544 |  |
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
|    1514 |  606 | `static sxi32 GenStateProcessStringExpression(` |
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
|    1516 |  617 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  618 | `	/* Preallocate some slots */` |
|    1516 |  619 | `	SySetAlloc(&sToken,0x08);` |
|       - |  620 | `	/* Tokenize the text */` |
|    1516 |  621 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  622 | `	/* Swap delimiter */` |
|    1516 |  623 | `	pTmpIn  = pGen->pIn;` |
|    1516 |  624 | `	pTmpEnd = pGen->pEnd;` |
|    1516 |  625 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1516 |  626 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  627 | `	/* Compile the expression */` |
|    1516 |  628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  629 | `	/* Restore token stream */` |
|    1516 |  630 | `	pGen->pIn  = pTmpIn;` |
|    1516 |  631 | `	pGen->pEnd = pTmpEnd;` |
|       - |  632 | `	/* Release the token set */` |
|    1516 |  633 | `	SySetRelease(&sToken);` |
|       - |  634 | `	/* Compilation result */` |
|    1516 |  635 | `	return rc;` |
|       2 |  636 |  |
|       - |  637 | `/*` |
|       - |  638 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  639 | ` */` |
|   14248 |  640 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  641 |  |
|       - |  642 | `	ph7_value *pConstObj;` |
|   14250 |  643 | `	sxu32 nIdx = 0;` |
|       - |  644 | `	/* Reserve a new constant */` |
|   14250 |  645 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   14250 |  646 | `	if( pConstObj == 0 ){` |
|     ! 0 |  647 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  648 | `		return 0;` |
|       - |  649 | `	}` |
|   14250 |  650 | `	(*pCount)++;` |
|   14250 |  651 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  652 | `	/* Emit the load constant instruction */` |
|   14250 |  653 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   14250 |  654 | `	return pConstObj;` |
|    7126 |  655 |  |
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
|   13158 |  694 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  695 |  |
|   13160 |  696 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  697 | `	const char *zIn,*zCur,*zEnd;` |
|   13160 |  698 | `	ph7_value *pObj = 0;` |
|       - |  699 | `	sxi32 iCons;` |
|       - |  700 | `	sxi32 rc;` |
|       - |  701 | `	/* Delimit the string */` |
|   13160 |  702 | `	zIn  = pStr->zString;` |
|   13160 |  703 | `	zEnd = &zIn[pStr->nByte];` |
|   13160 |  704 | `	if( zIn >= zEnd ){` |
|       - |  705 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  706 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  707 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  708 | `		 */` |
|     224 |  709 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     224 |  710 | `		return SXRET_OK;` |
|       - |  711 | `	}` |
|   12938 |  712 | `	zCur = 0;` |
|       - |  713 | `	/* Compile the node */` |
|   12938 |  714 | `	iCons = 0;` |
|    7225 |  715 | `	for(;;){` |
|   21696 |  716 | `		zCur = zIn;` |
|  126746 |  717 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  106566 |  718 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      44 |  719 | `				break;` |
|  106482 |  720 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1432 |  721 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     716 |  722 | `					break;` |
|       - |  723 | `			}` |
|  105052 |  724 | `			zIn++;` |
|       2 |  725 | `		}` |
|   21696 |  726 | `		if( zIn > zCur ){` |
|   10702 |  727 | `			if( pObj == 0 ){` |
|   10436 |  728 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   10436 |  729 | `				if( pObj == 0 ){` |
|     ! 0 |  730 | `					return SXERR_ABORT;` |
|       - |  731 | `				}` |
|    5217 |  732 | `			}` |
|   10702 |  733 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    5350 |  734 | `		}` |
|   21696 |  735 | `		if( zIn >= zEnd ){` |
|   12938 |  736 | `			break;` |
|       - |  737 | `		}` |
|    8760 |  738 | `		if( zIn[0] == '\\' ){` |
|    7246 |  739 | `			const char *zPtr = 0;` |
|       - |  740 | `			sxu32 n;` |
|    7246 |  741 | `			zIn++;` |
|    7246 |  742 | `			if( zIn >= zEnd ){` |
|     ! 0 |  743 | `				break;` |
|       - |  744 | `			}` |
|    7246 |  745 | `			if( pObj == 0 ){` |
|    3816 |  746 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    3816 |  747 | `				if( pObj == 0 ){` |
|     ! 0 |  748 | `					return SXERR_ABORT;` |
|       - |  749 | `				}` |
|    1907 |  750 | `			}` |
|    7246 |  751 | `			n = sizeof(char); /* size of conversion */` |
|    7246 |  752 | `			switch( zIn[0] ){` |
|       3 |  753 | `			case '$':` |
|       - |  754 | `				/* Dollar sign */` |
|       7 |  755 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       7 |  756 | `				break;` |
|      36 |  757 | `			case '\\':` |
|       - |  758 | `				/* A literal backslash */` |
|      74 |  759 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      74 |  760 | `				break;` |
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
|    3262 |  773 | `			case 'n':` |
|       - |  774 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    6526 |  775 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    6526 |  776 | `				break;` |
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
|    7246 |  844 | `			zIn += n;` |
|    7246 |  845 | `			continue;` |
|       - |  846 | `		}` |
|    1516 |  847 | `		if( zIn[0] == '{' ){` |
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
|    1430 |  881 | `			const char *zExpr = zIn;` |
|       - |  882 | `			/* Assemble variable name */` |
|     714 |  883 | `			for(;;){` |
|       - |  884 | `				/* Jump leading dollars */` |
|    2858 |  885 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1430 |  886 | `					zIn++;` |
|       2 |  887 | `				}` |
|     714 |  888 | `				for(;;){` |
|    9320 |  889 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7178 |  890 | `						zIn++;` |
|       2 |  891 | `					}` |
|    1430 |  892 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  893 | `						/* UTF-8 stream */` |
|     ! 0 |  894 | `						zIn++;` |
|     ! 0 |  895 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  896 | `							zIn++;` |
|     ! 0 |  897 | `						}` |
|     ! 0 |  898 | `						continue;` |
|       - |  899 | `					}` |
|    1430 |  900 | `					break;` |
|     ! 0 |  901 | `				}` |
|    1430 |  902 | `				if( zIn >= zEnd ){` |
|      79 |  903 | `					break;` |
|       - |  904 | `				}` |
|    1352 |  905 | `				if( zIn[0] == '[' ){` |
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
|    1344 |  923 | `				}else if(zIn[0] == '{' ){` |
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
|    1340 |  941 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  942 | `					/* Member access operator '->' */` |
|     ! 0 |  943 | `					zIn += 2;` |
|    1340 |  944 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  945 | `					/* Static member access operator '::' */` |
|     ! 0 |  946 | `					zIn += 2;` |
|     ! 0 |  947 | `				}else{` |
|     671 |  948 | `					break;` |
|       - |  949 | `				}` |
|     ! 0 |  950 | `			}` |
|       - |  951 | `			/* Process the expression */` |
|    1430 |  952 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1430 |  953 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  954 | `				return SXERR_ABORT;` |
|       - |  955 | `			}` |
|    1430 |  956 | `			if( rc != SXERR_EMPTY ){` |
|    1428 |  957 | `				++iCons;` |
|     713 |  958 | `			}` |
|       - |  959 | `		}` |
|       - |  960 | `		/* Invalidate the previously used constant */` |
|    1516 |  961 | `		pObj = 0;` |
|       2 |  962 | `	}/*for(;;)*/` |
|   12938 |  963 | `	if( iCons > 1 ){` |
|       - |  964 | `		/* Concatenate all compiled constants */` |
|    1164 |  965 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     581 |  966 | `	}` |
|       - |  967 | `	/* Node successfully compiled */` |
|   12938 |  968 | `	return SXRET_OK;` |
|    6581 |  969 |  |
|       - |  970 | `/*` |
|       - |  971 | ` * Compile a double quoted string.` |
|       - |  972 | ` *  See the block-comment above for more information.` |
|       - |  973 | ` */` |
|   13132 |  974 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  975 |  |
|       - |  976 | `	sxi32 rc;` |
|   13134 |  977 | `	rc = GenStateCompileString(&(*pGen));` |
|    6566 |  978 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  979 | `	/* Compilation result */` |
|   13134 |  980 | `	return rc;` |
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
|   13774 | 1012 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   13776 | 1023 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1024 | `	/* Compile the expression*/` |
|   13776 | 1025 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1026 | `	/* Restore token stream */` |
|   13776 | 1027 | `	RE_SWAP_DELIMITER(pGen);` |
|   13776 | 1028 | `	return rc;` |
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
|       - | 1063 | ` * Compile the 'array' language construct.` |
|       - | 1064 | ` *	 According to the PHP language reference manual` |
|       - | 1065 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1066 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1067 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1068 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1069 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1070 | ` */` |
|   20162 | 1071 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1072 |  |
|       - | 1073 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1074 | `	SyToken *pKey,*pCur;` |
|   20164 | 1075 | `	sxi32 iEmitRef = 0;` |
|   20164 | 1076 | `	sxi32 nPair = 0;` |
|       - | 1077 | `	sxi32 iNest;` |
|       - | 1078 | `	sxi32 rc;` |
|       - | 1079 | `	/* Jump the 'array' keyword,the leading left parenthesis and the trailing parenthesis.` |
|       - | 1080 | `	 */` |
|   20164 | 1081 | `	pGen->pIn += 2;` |
|   20164 | 1082 | `	pGen->pEnd--;` |
|   20164 | 1083 | `	xValidator = 0;` |
|   10081 | 1084 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   16380 | 1085 | `	for(;;){` |
|       - | 1086 | `		/* Jump leading commas */` |
|   37038 | 1087 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    4278 | 1088 | `			pGen->pIn++;` |
|       2 | 1089 | `		}` |
|   32762 | 1090 | `		pCur = pGen->pIn;` |
|   32762 | 1091 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1092 | `			/* No more entry to process */` |
|   20152 | 1093 | `			break;` |
|       - | 1094 | `		}` |
|   12612 | 1095 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1096 | `			continue;` |
|       - | 1097 | `		}` |
|       - | 1098 | `		/* Compile the key if available */` |
|   12612 | 1099 | `		pKey = pCur;` |
|   12612 | 1100 | `		iNest = 0;` |
|   34838 | 1101 | `		while( pCur < pGen->pIn ){` |
|   23364 | 1102 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1138 | 1103 | `				break;` |
|       - | 1104 | `			}` |
|   22228 | 1105 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      62 | 1106 | `				iNest++;` |
|   22198 | 1107 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1108 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1109 | `				 * parser will shortly detect any syntax error.` |
|       - | 1110 | `				 */` |
|      62 | 1111 | `				iNest--;` |
|      30 | 1112 | `			}` |
|   22228 | 1113 | `			pCur++;` |
|       2 | 1114 | `		}` |
|   12612 | 1115 | `		rc = SXERR_EMPTY;` |
|   12612 | 1116 | `		if( pCur < pGen->pIn ){` |
|    1138 | 1117 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - | 1118 | `				/* Missing value */` |
|      11 | 1119 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 | 1120 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1121 | `					return SXERR_ABORT;` |
|       - | 1122 | `				}` |
|      11 | 1123 | `				return SXRET_OK;` |
|       - | 1124 | `			}` |
|       - | 1125 | `			/* Compile the expression holding the key */` |
|    1128 | 1126 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - | 1127 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1128 | 1128 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1129 | `				return SXERR_ABORT;` |
|       - | 1130 | `			}` |
|    1128 | 1131 | `			pCur++; /* Jump the '=>' operator */` |
|   12039 | 1132 | `		}else if( pKey == pCur ){` |
|       - | 1133 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1134 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1135 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1136 | `		}else{` |
|       - | 1137 | `			/* Reset back the cursor and point to the entry value */` |
|   11476 | 1138 | `			pCur = pKey;` |
|       - | 1139 | `		}` |
|   12602 | 1140 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1141 | `			/* No available key,load NULL */` |
|   11478 | 1142 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    5738 | 1143 | `		}` |
|   12602 | 1144 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - | 1145 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      34 | 1146 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      34 | 1147 | `			iEmitRef = 1;` |
|      34 | 1148 | `			pCur++; /* Jump the '&' token */` |
|      34 | 1149 | `			if( pCur >= pGen->pIn ){` |
|       - | 1150 | `				/* Missing value */` |
|       3 | 1151 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 | 1152 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1153 | `					return SXERR_ABORT;` |
|       - | 1154 | `				}` |
|       3 | 1155 | `				return SXRET_OK;` |
|       - | 1156 | `			}` |
|      15 | 1157 | `		}` |
|       - | 1158 | `		/* Compile indice value */` |
|   12600 | 1159 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   12600 | 1160 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1161 | `			return SXERR_ABORT;` |
|       - | 1162 | `		}` |
|   12600 | 1163 | `		if( iEmitRef ){` |
|       - | 1164 | `			/* Emit the load reference instruction */` |
|      32 | 1165 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1166 | `		}` |
|   12600 | 1167 | `		xValidator = 0;` |
|   12600 | 1168 | `		iEmitRef = 0;` |
|   12600 | 1169 | `		nPair++;` |
|       2 | 1170 | `	}` |
|       - | 1171 | `	/* Emit the load map instruction */` |
|   20152 | 1172 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1173 | `	/* Node successfully compiled */` |
|   20152 | 1174 | `	return SXRET_OK;` |
|   10083 | 1175 |  |
|       - | 1176 | `/*` |
|       - | 1177 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - | 1178 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1179 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1180 | ` * error message.` |
|       - | 1181 | ` * See the routine responible of compiling the list language construct` |
|       - | 1182 | ` * for more inforation.` |
|       - | 1183 | ` */` |
|      50 | 1184 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1185 |  |
|      52 | 1186 | `	sxi32 rc = SXRET_OK;` |
|      52 | 1187 | `	if( pRoot->pOp ){` |
|     ! 0 | 1188 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 | 1189 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - | 1190 | `				/* Unexpected expression */` |
|     ! 0 | 1191 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1192 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 | 1193 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 | 1194 | `					rc = SXERR_INVALID;` |
|     ! 0 | 1195 | `				}` |
|     ! 0 | 1196 | `		}` |
|      52 | 1197 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1198 | `		/* Unexpected expression */` |
|       3 | 1199 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1200 | `			"list(): Expecting a variable not an expression");` |
|       3 | 1201 | `		if( rc != SXERR_ABORT ){` |
|       3 | 1202 | `			rc = SXERR_INVALID;` |
|       1 | 1203 | `		}` |
|       1 | 1204 | `	}` |
|      52 | 1205 | `	return rc;` |
|       2 | 1206 |  |
|       - | 1207 | `/*` |
|       - | 1208 | ` * Compile the 'list' language construct.` |
|       - | 1209 | ` *  According to the PHP language reference` |
|       - | 1210 | ` *  list(): Assign variables as if they were an array.` |
|       - | 1211 | ` *  list() is used to assign a list of variables in one operation.` |
|       - | 1212 | ` *  Description` |
|       - | 1213 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - | 1214 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - | 1215 | ` *   list() is used to assign a list of variables in one operation.` |
|       - | 1216 | ` *  Parameters` |
|       - | 1217 | ` *   $varname: A variable.` |
|       - | 1218 | ` *  Return Values` |
|       - | 1219 | ` *   The assigned array.` |
|       - | 1220 | ` */` |
|      24 | 1221 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1222 |  |
|       - | 1223 | `	SyToken *pNext;` |
|       - | 1224 | `	sxi32 nExpr;` |
|       - | 1225 | `	sxi32 rc;` |
|      26 | 1226 | `	nExpr = 0;` |
|       - | 1227 | `	/* Jump the 'list' keyword,the leading left parenthesis and the trailing parenthesis */` |
|      26 | 1228 | `	pGen->pIn += 2;` |
|      26 | 1229 | `	pGen->pEnd--;` |
|      12 | 1230 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      80 | 1231 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      56 | 1232 | `		if( pGen->pIn < pNext ){` |
|       - | 1233 | `			/* Compile the expression holding the variable */` |
|      52 | 1234 | `			rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      52 | 1235 | `			if( rc != SXRET_OK ){` |
|       - | 1236 | `				/* Do not bother compiling this expression, it's broken anyway */` |
|     ! 0 | 1237 | `				return SXRET_OK;` |
|       - | 1238 | `			}` |
|      27 | 1239 | `		}else{` |
|       - | 1240 | `			/* Empty entry,load NULL */` |
|       5 | 1241 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - | 1242 | `		}` |
|      56 | 1243 | `		nExpr++;` |
|       - | 1244 | `		/* Advance the stream cursor */` |
|      56 | 1245 | `		pGen->pIn = &pNext[1];` |
|       2 | 1246 | `	}` |
|       - | 1247 | `	/* Emit the LOAD_LIST instruction */` |
|      26 | 1248 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - | 1249 | `	/* Node successfully compiled */` |
|      26 | 1250 | `	return SXRET_OK;` |
|      14 | 1251 |  |
|       - | 1252 | `/* Forward declarations */` |
|       - | 1253 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - | 1254 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - | 1255 | `/*` |
|       - | 1256 | ` * Compile an annoynmous function or a closure.` |
|       - | 1257 | ` * According to the PHP language reference` |
|       - | 1258 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - | 1259 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - | 1260 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - | 1261 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - | 1262 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - | 1263 | ` *  Example Anonymous function variable assignment example` |
|       - | 1264 | ` * <?php` |
|       - | 1265 | ` * $greet = function($name)` |
|       - | 1266 | ` * {` |
|       - | 1267 | ` *    printf("Hello %s\r\n", $name);` |
|       - | 1268 | ` * };` |
|       - | 1269 | ` * $greet('World');` |
|       - | 1270 | ` * $greet('PHP');` |
|       - | 1271 | ` * ?>` |
|       - | 1272 | ` * Note that the implementation of annoynmous function and closure under` |
|       - | 1273 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - | 1274 | ` */` |
|     128 | 1275 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1276 |  |
|       - | 1277 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - | 1278 | `	char zName[512];         /* Unique lambda name */` |
|       - | 1279 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - | 1280 | `							  * one thread is allowed to compile the script.` |
|       - | 1281 | `						      */` |
|       - | 1282 | `	ph7_value *pObj;` |
|       - | 1283 | `	SyString sName;` |
|       - | 1284 | `	sxu32 nIdx;` |
|       - | 1285 | `	sxu32 nLen;` |
|       - | 1286 | `	sxi32 rc;` |
|       - | 1287 |  |
|     130 | 1288 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     130 | 1289 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 | 1290 | `		pGen->pIn++;` |
|     ! 0 | 1291 | `	}` |
|       - | 1292 | `	/* Reserve a constant for the lambda */` |
|     130 | 1293 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     130 | 1294 | `	if( pObj == 0 ){` |
|     ! 0 | 1295 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1296 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1297 | `		return SXERR_ABORT;` |
|       - | 1298 | `	}` |
|       - | 1299 | `	/* Generate a unique name */` |
|     130 | 1300 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - | 1301 | `	/* Make sure the generated name is unique */` |
|     130 | 1302 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 1303 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 | 1304 | `	}` |
|     130 | 1305 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     130 | 1306 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - | 1307 | `	/* Compile the lambda body */` |
|     130 | 1308 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     130 | 1309 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1310 | `		return SXERR_ABORT;` |
|       - | 1311 | `	}` |
|     130 | 1312 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1313 | `		/* Emit the load closure instruction */` |
|      10 | 1314 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       6 | 1315 | `	}else{` |
|       - | 1316 | `		/* Emit the load constant instruction */` |
|     122 | 1317 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1318 | `	}` |
|       - | 1319 | `	/* Node successfully compiled */` |
|     130 | 1320 | `	return SXRET_OK;` |
|      66 | 1321 |  |
|       - | 1322 | `/*` |
|       - | 1323 | ` * Compile a backtick quoted string.` |
|       - | 1324 | ` */` |
|       4 | 1325 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1326 |  |
|       - | 1327 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - | 1328 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - | 1329 | `	 */` |
|       7 | 1330 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - | 1331 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 | 1332 | `		ph7_lib_version()` |
|       - | 1333 | `		);` |
|       - | 1334 | `	/* Load NULL */` |
|       5 | 1335 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1336 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1337 | `	/* Node successfully compiled */` |
|       5 | 1338 | `	return SXRET_OK;` |
|       1 | 1339 |  |
|       - | 1340 | `/*` |
|       - | 1341 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - | 1342 | ` * construct.` |
|       - | 1343 | ` */` |
|      70 | 1344 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1345 |  |
|       - | 1346 | `	SyString *pName;` |
|       - | 1347 | `	sxu32 nKeyID;` |
|       - | 1348 | `	sxi32 rc;` |
|       - | 1349 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      72 | 1350 | `	pName = &pGen->pIn->sData;` |
|      72 | 1351 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      72 | 1352 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      72 | 1353 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 | 1354 | `		SyToken *pTmp,*pNext = 0;` |
|       - | 1355 | `		/* Compile arguments one after one */` |
|       9 | 1356 | `		pTmp = pGen->pEnd;` |
|       - | 1357 | `		/* Symisc eXtension to the PHP programming language:` |
|       - | 1358 | `		 * 'echo' can be used in the context of a function which` |
|       - | 1359 | `		 *  mean that the following expression is valid:` |
|       - | 1360 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - | 1361 | `		 */` |
|       9 | 1362 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 | 1363 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 | 1364 | `			if( pGen->pIn < pNext ){` |
|       9 | 1365 | `				pGen->pEnd = pNext;` |
|       9 | 1366 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 | 1367 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1368 | `					return SXERR_ABORT;` |
|       - | 1369 | `				}` |
|       9 | 1370 | `				if( rc != SXERR_EMPTY ){` |
|       - | 1371 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - | 1372 | `					 * without the overhead of a function call.` |
|       - | 1373 | `					 * This is a very powerful optimization that improve` |
|       - | 1374 | `					 * performance greatly.` |
|       - | 1375 | `					 */` |
|       9 | 1376 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 | 1377 | `				}` |
|       4 | 1378 | `			}` |
|       - | 1379 | `			/* Jump trailing commas */` |
|       9 | 1380 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 1381 | `				pNext++;` |
|     ! 0 | 1382 | `			}` |
|       9 | 1383 | `			pGen->pIn = pNext;` |
|       1 | 1384 | `		}` |
|       - | 1385 | `		/* Restore token stream */` |
|       9 | 1386 | `		pGen->pEnd = pTmp;` |
|       5 | 1387 | `	}else{` |
|      64 | 1388 | `		sxi32 nArg = 0;` |
|      64 | 1389 | `		sxu32 nIdx = 0;` |
|      64 | 1390 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|      64 | 1391 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1392 | `			return SXERR_ABORT;` |
|      64 | 1393 | `		}else if(rc != SXERR_EMPTY ){` |
|      64 | 1394 | `			nArg = 1;` |
|      31 | 1395 | `		}` |
|      64 | 1396 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1397 | `			ph7_value *pObj;` |
|       - | 1398 | `			/* Emit the call instruction */` |
|      18 | 1399 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      18 | 1400 | `			if( pObj == 0 ){` |
|     ! 0 | 1401 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1402 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1403 | `				return SXERR_ABORT;` |
|       - | 1404 | `			}` |
|      18 | 1405 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1406 | `			/* Install in the literal table */` |
|      18 | 1407 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       8 | 1408 | `		}` |
|       - | 1409 | `		/* Emit the call instruction */` |
|      64 | 1410 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      64 | 1411 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - | 1412 | `	}` |
|       - | 1413 | `	/* Node successfully compiled */` |
|      72 | 1414 | `	return SXRET_OK;` |
|      37 | 1415 |  |
|       - | 1416 | `/*` |
|       - | 1417 | ` * Compile a node holding a variable declaration.` |
|       - | 1418 | ` * According to the PHP language reference` |
|       - | 1419 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 1420 | ` *  The variable name is case-sensitive.` |
|       - | 1421 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 1422 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1423 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 1424 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 1425 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 1426 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 1427 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 1428 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 1429 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 1430 | ` *  the chapter on Expressions.` |
|       - | 1431 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 1432 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 1433 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 1434 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 1435 | ` *  is being assigned (the source variable).` |
|       - | 1436 | ` */` |
|  620630 | 1437 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1438 |  |
|  620632 | 1439 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1440 | `	sxi32 iVv;` |
|       - | 1441 | `	sxi32 iP1;` |
|       - | 1442 | `	void *p3;` |
|       - | 1443 | `	sxi32 rc;` |
|  620632 | 1444 | `	iVv = -1; /* Variable variable counter */` |
| 1241274 | 1445 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  620644 | 1446 | `		pGen->pIn++;` |
|  620644 | 1447 | `		iVv++;` |
|       2 | 1448 | `	}` |
|  620632 | 1449 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1450 | `		/* Invalid variable name */` |
|       3 | 1451 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1452 | `		if( rc == SXERR_ABORT ){` |
|       - | 1453 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1454 | `			return SXERR_ABORT;` |
|       - | 1455 | `		}` |
|       3 | 1456 | `		return SXRET_OK;` |
|       - | 1457 | `	}` |
|  620630 | 1458 | `	p3  = 0;` |
|  620630 | 1459 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 1460 | `		/* Dynamic variable creation */` |
|      18 | 1461 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 | 1462 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 | 1463 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1464 | `			/* Empty expression */` |
|       3 | 1465 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1466 | `			return SXRET_OK;` |
|       - | 1467 | `		}` |
|       - | 1468 | `		/* Compile the expression holding the variable name */` |
|      16 | 1469 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 | 1470 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1471 | `			return SXERR_ABORT;` |
|      16 | 1472 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 1473 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 | 1474 | `			return SXRET_OK;` |
|       - | 1475 | `		}` |
|       7 | 1476 | `	}else{` |
|       - | 1477 | `		SyHashEntry *pEntry;` |
|       - | 1478 | `		SyString *pName;` |
|  620614 | 1479 | `		char *zName = 0;` |
|       - | 1480 | `		/* Extract variable name */` |
|  620614 | 1481 | `		pName = &pGen->pIn->sData;` |
|       - | 1482 | `		/* Advance the stream cursor */` |
|  620614 | 1483 | `		pGen->pIn++;` |
|  620614 | 1484 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  620614 | 1485 | `		if( pEntry == 0 ){` |
|       - | 1486 | `			/* Duplicate name */` |
|   91932 | 1487 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   91932 | 1488 | `			if( zName == 0 ){` |
|     ! 0 | 1489 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1490 | `				return SXERR_ABORT;` |
|       - | 1491 | `			}` |
|       - | 1492 | `			/* Install in the hashtable */` |
|   91932 | 1493 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   45967 | 1494 | `		}else{` |
|       - | 1495 | `			/* Name already available */` |
|  528684 | 1496 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1497 | `		}` |
|  620614 | 1498 | `		p3 = (void *)zName;` |
|       - | 1499 | `	}` |
|  620626 | 1500 | `	iP1 = 0;` |
|  620626 | 1501 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  206354 | 1502 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1503 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  206350 | 1504 | `			iP1 = 1;` |
|  103174 | 1505 | `		}` |
|  103176 | 1506 | `	}` |
|       - | 1507 | `	/* Emit the load instruction */` |
|  620626 | 1508 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  620638 | 1509 | `	while( iVv > 0 ){` |
|      13 | 1510 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1511 | `		iVv--;` |
|       1 | 1512 | `	}` |
|       - | 1513 | `	/* Node successfully compiled */` |
|  620626 | 1514 | `	return SXRET_OK;` |
|  310317 | 1515 |  |
|       - | 1516 | `/*` |
|       - | 1517 | ` * Load a literal.` |
|       - | 1518 | ` */` |
|  400918 | 1519 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 1520 |  |
|  400920 | 1521 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1522 | `	ph7_value *pObj;` |
|       - | 1523 | `	SyString *pStr;` |
|       - | 1524 | `	sxu32 nIdx;` |
|       - | 1525 | `	/* Extract token value */` |
|  400920 | 1526 | `	pStr = &pToken->sData;` |
|       - | 1527 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  400920 | 1528 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   75046 | 1529 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1530 | `			/* NULL constant are always indexed at 0 */` |
|   27938 | 1531 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   27938 | 1532 | `			return SXRET_OK;` |
|   47110 | 1533 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1534 | `			/* TRUE constant are always indexed at 1 */` |
|     462 | 1535 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     462 | 1536 | `			return SXRET_OK;` |
|       2 | 1537 | `		}` |
|  385953 | 1538 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   73506 | 1539 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1540 | `			/* FALSE constant are always indexed at 2 */` |
|   30478 | 1541 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   30478 | 1542 | `			return SXRET_OK;` |
|  326873 | 1543 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   62946 | 1544 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1545 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    4612 | 1546 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    4612 | 1547 | `			if( pObj == 0 ){` |
|     ! 0 | 1548 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1549 | `				return SXERR_ABORT;` |
|       - | 1550 | `			}` |
|    4612 | 1551 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1552 | `			/* Emit the load constant instruction */` |
|    4612 | 1553 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    4612 | 1554 | `			return SXRET_OK;` |
|  298964 | 1555 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   16348 | 1556 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - | 1557 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       5 | 1558 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 | 1559 | `			if( pObj == 0 ){` |
|     ! 0 | 1560 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1561 | `				return SXERR_ABORT;` |
|       - | 1562 | `			}` |
|       5 | 1563 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1564 | `				SyString sNs;` |
|       5 | 1565 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       5 | 1566 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       3 | 1567 | `			}else{` |
|     ! 0 | 1568 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - | 1569 | `			}` |
|       5 | 1570 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       5 | 1571 | `			return SXRET_OK;` |
|  298236 | 1572 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    7428 | 1573 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  294516 | 1574 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    7478 | 1575 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 | 1576 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 1577 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 | 1578 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - | 1579 | `				/* Point to the upper block */` |
|      11 | 1580 | `				pBlock = pBlock->pParent;` |
|       1 | 1581 | `			}` |
|      11 | 1582 | `			if( pBlock == 0 ){` |
|       - | 1583 | `				/* Called in the global scope,load NULL */` |
|       5 | 1584 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1585 | `			}else{` |
|       - | 1586 | `				/* Extract the target function/method */` |
|       7 | 1587 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 | 1588 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - | 1589 | `					/* Not a class method,Load null */` |
|       3 | 1590 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1591 | `				}else{` |
|       5 | 1592 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 | 1593 | `					if( pObj == 0 ){` |
|     ! 0 | 1594 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1595 | `						return SXERR_ABORT;` |
|       - | 1596 | `					}` |
|       5 | 1597 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - | 1598 | `					/* Emit the load constant instruction */` |
|       5 | 1599 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1600 | `				}` |
|       - | 1601 | `			}` |
|      11 | 1602 | `			return SXRET_OK;` |
|       - | 1603 | `	}` |
|       - | 1604 | `	/* Query literal table */` |
|  337424 | 1605 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 1606 | `		ph7_value *pLitObj;` |
|       - | 1607 | `		/* Unknown literal,install it in the literal table */` |
|  134448 | 1608 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  134448 | 1609 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1610 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1611 | `			return SXERR_ABORT;` |
|       - | 1612 | `		}` |
|  134448 | 1613 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  134448 | 1614 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   67223 | 1615 | `	}` |
|       - | 1616 | `	/* Emit the load constant instruction */` |
|  337424 | 1617 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  337424 | 1618 | `	return SXRET_OK;` |
|  200461 | 1619 |  |
|       - | 1620 | `/*` |
|       - | 1621 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1622 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1623 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1624 | ` * Otherwise, load the simple literal directly.` |
|       - | 1625 | ` */` |
|  400938 | 1626 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 1627 |  |
|       - | 1628 | `	sxi32 rc;` |
|  400940 | 1629 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1630 | `		return SXRET_OK;` |
|       - | 1631 | `	}` |
|       - | 1632 | `	/* Check if this is a multi-token namespace path */` |
|  400940 | 1633 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 1634 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      21 | 1635 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      21 | 1636 | `		int isAbsolute = 0;` |
|      21 | 1637 | `		SyBlobReset(pWorker);` |
|       - | 1638 | `		/* Check for leading backslash (absolute path) */` |
|      21 | 1639 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      19 | 1640 | `			isAbsolute = 1;` |
|      19 | 1641 | `			pGen->pIn++; /* Skip leading backslash */` |
|       9 | 1642 | `		}` |
|       - | 1643 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      21 | 1644 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 1645 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 1646 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 | 1647 | `		}` |
|       - | 1648 | `		/* Collect all path components */` |
|      81 | 1649 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      81 | 1650 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      31 | 1651 | `				SyBlobAppend(pWorker,"\\",1);` |
|      16 | 1652 | `			}else{` |
|      51 | 1653 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 1654 | `			}` |
|      81 | 1655 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      21 | 1656 | `				pGen->pIn++;` |
|      21 | 1657 | `				break;` |
|       - | 1658 | `			}` |
|      61 | 1659 | `			pGen->pIn++;` |
|       1 | 1660 | `		}` |
|      21 | 1661 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 1662 | `			ph7_value *pObj;` |
|       - | 1663 | `			SyString sPath;` |
|       - | 1664 | `			sxu32 nIdx;` |
|      21 | 1665 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 1666 | `			/* Install in the literal table */` |
|      21 | 1667 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      13 | 1668 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      13 | 1669 | `				if( pObj == 0 ){` |
|     ! 0 | 1670 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1671 | `					return SXERR_ABORT;` |
|       - | 1672 | `				}` |
|      13 | 1673 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      13 | 1674 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       6 | 1675 | `			}` |
|       - | 1676 | `			/* Emit the load constant instruction.` |
|       - | 1677 | `			 * P1=1 means candidate for constant/function/class expansion. */` |
|      21 | 1678 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      21 | 1679 | `			return SXRET_OK;` |
|       - | 1680 | `		}` |
|     ! 0 | 1681 | `	}` |
|       - | 1682 | `	/* Single-token literal: load directly */` |
|  400920 | 1683 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  400920 | 1684 | `	return rc;` |
|  200471 | 1685 |  |
|       - | 1686 | `/*` |
|       - | 1687 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1688 | ` */` |
|  400938 | 1689 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1690 |  |
|       - | 1691 | `	sxi32 rc;` |
|  400940 | 1692 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  400940 | 1693 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1694 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1695 | `		return rc;` |
|       - | 1696 | `	}` |
|       - | 1697 | `	/* Node successfully compiled */` |
|  400940 | 1698 | `	return SXRET_OK;` |
|  200471 | 1699 |  |
|       - | 1700 | `/*` |
|       - | 1701 | ` * Recover from a compile-time error. In other words synchronize` |
|       - | 1702 | ` * the token stream cursor with the first semi-colon seen.` |
|       - | 1703 | ` */` |
|       6 | 1704 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 | 1705 |  |
|       - | 1706 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      13 | 1707 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       7 | 1708 | `		pGen->pIn++;` |
|       1 | 1709 | `	}` |
|       7 | 1710 | `	return SXRET_OK;` |
|       1 | 1711 |  |
|       - | 1712 | `/*` |
|       - | 1713 | ` * Check if the given identifier name is reserved or not.` |
|       - | 1714 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - | 1715 | ` */` |
|      30 | 1716 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 | 1717 |  |
|      32 | 1718 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      12 | 1719 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 | 1720 | `			return TRUE;` |
|      10 | 1721 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 | 1722 | `			return TRUE;` |
|       1 | 1723 | `		}` |
|      24 | 1724 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 | 1725 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 | 1726 | `			return TRUE;` |
|       - | 1727 | `		}` |
|     ! 0 | 1728 | `	}` |
|       - | 1729 | `	/* Not a reserved constant */` |
|      24 | 1730 | `	return FALSE;` |
|      17 | 1731 |  |
|       - | 1732 | `/*` |
|       - | 1733 | ` * Compile the 'const' statement.` |
|       - | 1734 | ` * According to the PHP language reference` |
|       - | 1735 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - | 1736 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - | 1737 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - | 1738 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - | 1739 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1740 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - | 1741 | ` *  Syntax` |
|       - | 1742 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - | 1743 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - | 1744 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - | 1745 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - | 1746 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - | 1747 | ` *  to get a list of all defined constants.` |
|       - | 1748 | ` *` |
|       - | 1749 | ` * Symisc eXtension.` |
|       - | 1750 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - | 1751 | ` *  would allow only simple scalar value.` |
|       - | 1752 | ` *  Example` |
|       - | 1753 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 1754 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 1755 | ` */` |
|      26 | 1756 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 | 1757 |  |
|       - | 1758 | `	SySet *pConsCode,*pInstrContainer;` |
|      28 | 1759 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1760 | `	SyString *pName;` |
|       - | 1761 | `	sxi32 rc;` |
|      28 | 1762 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      28 | 1763 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 1764 | `		/* Invalid constant name */` |
|       7 | 1765 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 | 1766 | `		if( rc == SXERR_ABORT ){` |
|       - | 1767 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1768 | `			return SXERR_ABORT;` |
|       - | 1769 | `		}` |
|       7 | 1770 | `		goto Synchronize;` |
|       - | 1771 | `	}` |
|       - | 1772 | `	/* Peek constant name */` |
|      22 | 1773 | `	pName = &pGen->pIn->sData;` |
|       - | 1774 | `	/* Make sure the constant name isn't reserved */` |
|      22 | 1775 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 1776 | `		/* Reserved constant */` |
|       9 | 1777 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 | 1778 | `		if( rc == SXERR_ABORT ){` |
|       - | 1779 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1780 | `			return SXERR_ABORT;` |
|       - | 1781 | `		}` |
|       9 | 1782 | `		goto Synchronize;` |
|       - | 1783 | `	}` |
|      14 | 1784 | `	pGen->pIn++;` |
|      14 | 1785 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 1786 | `		/* Invalid statement*/` |
|       5 | 1787 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 | 1788 | `		if( rc == SXERR_ABORT ){` |
|       - | 1789 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1790 | `			return SXERR_ABORT;` |
|       - | 1791 | `		}` |
|       5 | 1792 | `		goto Synchronize;` |
|       - | 1793 | `	}` |
|       9 | 1794 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - | 1795 | `	/* Allocate a new constant value container */` |
|       9 | 1796 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       9 | 1797 | `	if( pConsCode == 0 ){` |
|     ! 0 | 1798 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1799 | `		return SXERR_ABORT;` |
|       - | 1800 | `	}` |
|       9 | 1801 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 1802 | `	/* Swap bytecode container */` |
|       9 | 1803 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       9 | 1804 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - | 1805 | `	/* Compile constant value */` |
|       9 | 1806 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 1807 | `	/* Emit the done instruction */` |
|       9 | 1808 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       9 | 1809 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       9 | 1810 | `	if( rc == SXERR_ABORT ){` |
|       - | 1811 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1812 | `		return SXERR_ABORT;` |
|       - | 1813 | `	}` |
|       9 | 1814 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - | 1815 | `	/* Register the constant */` |
|       9 | 1816 | `	rc = PH7_VmRegisterConstant(pGen->pVm,pName,PH7_VmExpandConstantValue,pConsCode);` |
|       9 | 1817 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1818 | `		SySetRelease(pConsCode);` |
|     ! 0 | 1819 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 | 1820 | `	}` |
|       9 | 1821 | `	return SXRET_OK;` |
|       9 | 1822 | `Synchronize:` |
|       - | 1823 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 | 1824 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 | 1825 | `		pGen->pIn++;` |
|       1 | 1826 | `	}` |
|      19 | 1827 | `	return SXRET_OK;` |
|      15 | 1828 |  |
|       - | 1829 | `/*` |
|       - | 1830 | ` * Compile the 'continue' statement.` |
|       - | 1831 | ` * According to the PHP language reference` |
|       - | 1832 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - | 1833 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - | 1834 | ` *  iteration.` |
|       - | 1835 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - | 1836 | ` *  the purposes of continue.` |
|       - | 1837 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - | 1838 | ` *  of enclosing loops it should skip to the end of.` |
|       - | 1839 | ` *  Note:` |
|       - | 1840 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - | 1841 | ` */` |
|    2342 | 1842 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 1843 |  |
|       - | 1844 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1845 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1846 | `	sxu32 nLineLocal;` |
|       - | 1847 | `	sxi32 rc;` |
|    2344 | 1848 | `	nLineLocal = pGen->pIn->nLine;` |
|    2344 | 1849 | `	iLevel = 0;` |
|       - | 1850 | `	/* Jump the 'continue' keyword */` |
|    2344 | 1851 | `	pGen->pIn++;` |
|    2344 | 1852 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 1853 | `		/* optional numeric argument which tells us how many levels` |
|       - | 1854 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 1855 | `		 */` |
|      12 | 1856 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      12 | 1857 | `		if( iLevel < 2 ){` |
|     ! 0 | 1858 | `			iLevel = 0;` |
|     ! 0 | 1859 | `		}` |
|      12 | 1860 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 1861 | `	}` |
|       - | 1862 | `	/* Point to the target loop */` |
|    2344 | 1863 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2344 | 1864 | `	if( pLoop == 0 ){` |
|       - | 1865 | `		/* Illegal continue */` |
|      11 | 1866 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 1867 | `		if( rc == SXERR_ABORT ){` |
|       - | 1868 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1869 | `			return SXERR_ABORT;` |
|       - | 1870 | `		}` |
|       6 | 1871 | `	}else{` |
|    2334 | 1872 | `		sxu32 nInstrIdx = 0;` |
|    2334 | 1873 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - | 1874 | `			/* According to the PHP language reference manual` |
|       - | 1875 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - | 1876 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - | 1877 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - | 1878 | `			 */` |
|       5 | 1879 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 | 1880 | `			if( rc == SXRET_OK ){` |
|       5 | 1881 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 | 1882 | `			}` |
|       3 | 1883 | `		}else{` |
|       - | 1884 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    2330 | 1885 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2330 | 1886 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 1887 | `				JumpFixup sJumpFix;` |
|       - | 1888 | `				/* Post-continue */` |
|       8 | 1889 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       8 | 1890 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       8 | 1891 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       3 | 1892 | `			}` |
|       - | 1893 | `		}` |
|       - | 1894 | `	}` |
|    2344 | 1895 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1896 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 1897 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 1898 | `	}` |
|       - | 1899 | `	/* Statement successfully compiled */` |
|    2344 | 1900 | `	return SXRET_OK;` |
|    1173 | 1901 |  |
|       - | 1902 | `/*` |
|       - | 1903 | ` * Compile the 'break' statement.` |
|       - | 1904 | ` * According to the PHP language reference` |
|       - | 1905 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - | 1906 | ` *  structure.` |
|       - | 1907 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - | 1908 | ` *  enclosing structures are to be broken out of.` |
|       - | 1909 | ` */` |
|      88 | 1910 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 | 1911 |  |
|       - | 1912 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1913 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1914 | `	sxi32 rc;` |
|      90 | 1915 | `	iLevel = 0;` |
|       - | 1916 | `	/* Jump the 'break' keyword */` |
|      90 | 1917 | `	pGen->pIn++;` |
|      90 | 1918 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 1919 | `		/* optional numeric argument which tells us how many levels` |
|       - | 1920 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 1921 | `		 */` |
|      12 | 1922 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      12 | 1923 | `		if( iLevel < 2 ){` |
|     ! 0 | 1924 | `			iLevel = 0;` |
|     ! 0 | 1925 | `		}` |
|      12 | 1926 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 1927 | `	}` |
|       - | 1928 | `	/* Extract the target loop */` |
|      90 | 1929 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|      90 | 1930 | `	if( pLoop == 0 ){` |
|       - | 1931 | `		/* Illegal break */` |
|      17 | 1932 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 | 1933 | `		if( rc == SXERR_ABORT ){` |
|       - | 1934 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1935 | `			return SXERR_ABORT;` |
|       - | 1936 | `		}` |
|       9 | 1937 | `	}else{` |
|       - | 1938 | `		sxu32 nInstrIdx;` |
|      74 | 1939 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      74 | 1940 | `		if( rc == SXRET_OK ){` |
|       - | 1941 | `			/* Fix the jump later when the jump destination is resolved */` |
|      74 | 1942 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      36 | 1943 | `		}` |
|       - | 1944 | `	}` |
|      90 | 1945 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1946 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 1947 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 | 1948 | `	}` |
|       - | 1949 | `	/* Statement successfully compiled */` |
|      90 | 1950 | `	return SXRET_OK;` |
|      46 | 1951 |  |
|       - | 1952 | `/*` |
|       - | 1953 | ` * Compile or record a label.` |
|       - | 1954 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - | 1955 | ` * Example` |
|       - | 1956 | ` *  goto LABEL;` |
|       - | 1957 | ` *   echo 'Foo';` |
|       - | 1958 | ` *  LABEL:` |
|       - | 1959 | ` *   echo 'Bar';` |
|       - | 1960 | ` */` |
|     112 | 1961 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 | 1962 |  |
|       - | 1963 | `	GenBlock *pBlock;` |
|       - | 1964 | `	Label sLabel;` |
|       - | 1965 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 | 1966 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 | 1967 | `	if( pBlock ){` |
|       - | 1968 | `		sxi32 rc;` |
|       7 | 1969 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 | 1970 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 | 1971 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1972 | `			return SXERR_ABORT;` |
|       - | 1973 | `		}` |
|       3 | 1974 | `	}else{` |
|     110 | 1975 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 1976 | `		char *zDup;` |
|       - | 1977 | `		/* Initialize label fields */` |
|     110 | 1978 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - | 1979 | `		/* Duplicate label name */` |
|     110 | 1980 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 | 1981 | `		if( zDup == 0 ){` |
|     ! 0 | 1982 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 1983 | `			return SXERR_ABORT;` |
|       - | 1984 | `		}` |
|     110 | 1985 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 | 1986 | `		sLabel.bRef  = FALSE;` |
|     110 | 1987 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 | 1988 | `		pBlock = pGen->pCurrent;` |
|     218 | 1989 | `		while( pBlock ){` |
|     130 | 1990 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 | 1991 | `				break;` |
|       - | 1992 | `			}` |
|       - | 1993 | `			/* Point to the upper block */` |
|     110 | 1994 | `			pBlock = pBlock->pParent;` |
|       2 | 1995 | `		}` |
|     110 | 1996 | `		if( pBlock ){` |
|      22 | 1997 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 | 1998 | `		}else{` |
|      90 | 1999 | `			sLabel.pFunc = 0;` |
|       - | 2000 | `		}` |
|       - | 2001 | `		/* Insert in label set */` |
|     110 | 2002 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - | 2003 | `	}` |
|     114 | 2004 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 | 2005 | `	return SXRET_OK;` |
|      58 | 2006 |  |
|       - | 2007 | `/*` |
|       - | 2008 | ` * Compile the so hated 'goto' statement.` |
|       - | 2009 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - | 2010 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - | 2011 | ` * a compiler it has to do this.` |
|       - | 2012 | ` * According to the PHP language reference manual` |
|       - | 2013 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - | 2014 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - | 2015 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - | 2016 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - | 2017 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - | 2018 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - | 2019 | ` *   of a multi-level break` |
|       - | 2020 | ` */` |
|     152 | 2021 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 | 2022 |  |
|       - | 2023 | `	JumpFixup sJump;` |
|       - | 2024 | `	sxi32 rc;` |
|     154 | 2025 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 | 2026 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2027 | `		/* Missing label */` |
|     ! 0 | 2028 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 | 2029 | `		if( rc == SXERR_ABORT ){` |
|       - | 2030 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2031 | `			return SXERR_ABORT;` |
|       - | 2032 | `		}` |
|     ! 0 | 2033 | `		return SXRET_OK;` |
|       - | 2034 | `	}` |
|     154 | 2035 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 | 2036 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 | 2037 | `		if( rc == SXERR_ABORT ){` |
|       - | 2038 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2039 | `			return SXERR_ABORT;` |
|       - | 2040 | `		}` |
|       3 | 2041 | `	}else{` |
|     150 | 2042 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 2043 | `		GenBlock *pBlock;` |
|       - | 2044 | `		char *zDup;` |
|       - | 2045 | `		/* Prepare the jump destination */` |
|     150 | 2046 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 | 2047 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - | 2048 | `		/* Duplicate label name */` |
|     150 | 2049 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 | 2050 | `		if( zDup == 0 ){` |
|     ! 0 | 2051 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2052 | `			return SXERR_ABORT;` |
|       - | 2053 | `		}` |
|     150 | 2054 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 | 2055 | `		pBlock = pGen->pCurrent;` |
|     312 | 2056 | `		while( pBlock ){` |
|     196 | 2057 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 | 2058 | `				break;` |
|       - | 2059 | `			}` |
|       - | 2060 | `			/* Point to the upper block */` |
|     164 | 2061 | `			pBlock = pBlock->pParent;` |
|       2 | 2062 | `		}` |
|     150 | 2063 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 | 2064 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 | 2065 | `			if( rc == SXERR_ABORT ){` |
|       - | 2066 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2067 | `				return SXERR_ABORT;` |
|       - | 2068 | `			}` |
|       3 | 2069 | `		}` |
|     150 | 2070 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 | 2071 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 | 2072 | `		}else{` |
|     124 | 2073 | `			sJump.pFunc = 0;` |
|       - | 2074 | `		}` |
|       - | 2075 | `		/* Emit the unconditional jump */` |
|     150 | 2076 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 | 2077 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 | 2078 | `		}` |
|       - | 2079 | `	}` |
|     154 | 2080 | `	pGen->pIn++; /* Jump the label name */` |
|     154 | 2081 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 | 2082 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 | 2083 | `	}` |
|       - | 2084 | `	/* Statement successfully compiled */` |
|     154 | 2085 | `	return SXRET_OK;` |
|      78 | 2086 |  |
|       - | 2087 | `/*` |
|       - | 2088 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - | 2089 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - | 2090 | ` * failure.` |
|       - | 2091 | ` */` |
|      20 | 2092 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 | 2093 |  |
|       - | 2094 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - | 2095 | `	sxu32 nRawObj;` |
|      10 | 2096 | `	sxu32 nObjIdx;` |
|       - | 2097 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - | 2098 | `	 * a PHP block.` |
|       - | 2099 | `	 */` |
|      10 | 2100 | `Consume:` |
|      21 | 2101 | `	nRawObj = nObjIdx = 0;` |
|      21 | 2102 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 | 2103 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 | 2104 | `		if( pRawObj == 0 ){` |
|     ! 0 | 2105 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2106 | `			return SXERR_ABORT;` |
|       - | 2107 | `		}` |
|       - | 2108 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 | 2109 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 | 2110 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 | 2111 | `		++nRawObj;` |
|     ! 0 | 2112 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 | 2113 | `	}` |
|      21 | 2114 | `	if( nRawObj > 0 ){` |
|       - | 2115 | `		/* Emit the consume instruction */` |
|     ! 0 | 2116 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 | 2117 | `	}` |
|      21 | 2118 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 | 2119 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - | 2120 | `		/* Reset the token set */` |
|     ! 0 | 2121 | `		SySetReset(pTokenSet);` |
|       - | 2122 | `		/* Tokenize input */` |
|     ! 0 | 2123 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 | 2124 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - | 2125 | `		/* Point to the fresh token stream */` |
|     ! 0 | 2126 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 | 2127 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - | 2128 | `		/* Advance the stream cursor */` |
|     ! 0 | 2129 | `		pGen->pRawIn++;` |
|       - | 2130 | `		/* TICKET 1433-011 */` |
|     ! 0 | 2131 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 2132 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 2133 | `			sxi32 rc;` |
|       - | 2134 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 | 2135 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 | 2136 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 | 2137 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 | 2138 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 2139 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2140 | `				return SXERR_ABORT;` |
|     ! 0 | 2141 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 | 2142 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 2143 | `			}` |
|     ! 0 | 2144 | `			goto Consume;` |
|       - | 2145 | `		}` |
|     ! 0 | 2146 | `	}else{` |
|       - | 2147 | `		/* No more chunks to process */` |
|      21 | 2148 | `		pGen->pIn = pGen->pEnd;` |
|      21 | 2149 | `		return SXERR_EOF;` |
|       - | 2150 | `	}` |
|     ! 0 | 2151 | `	return SXRET_OK;` |
|      11 | 2152 |  |
|       - | 2153 | `/*` |
|       - | 2154 | ` * Compile a PHP block.` |
|       - | 2155 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - | 2156 | ` * optionally delimited by braces {}.` |
|       - | 2157 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 2158 | ` * and this function takes care of generating the appropriate error` |
|       - | 2159 | ` * message.` |
|       - | 2160 | ` */` |
|  210082 | 2161 | `static sxi32 PH7_CompileBlock(` |
|       - | 2162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2163 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2164 | `	)` |
|       2 | 2165 |  |
|       - | 2166 | `	sxi32 rc;` |
|       - | 2167 | `	sxu32 nLine;` |
|  210084 | 2168 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  208786 | 2169 | `		nLine = pGen->pIn->nLine;` |
|  208786 | 2170 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  208786 | 2171 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2172 | `			return SXERR_ABORT;` |
|       - | 2173 | `		}` |
|  208786 | 2174 | `		pGen->pIn++;` |
|       - | 2175 | `		/* Compile until we hit the closing braces '}' */` |
|  304921 | 2176 | `		for(;;){` |
|  609844 | 2177 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 | 2178 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 | 2179 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2180 | `			 	   return SXERR_ABORT;` |
|       - | 2181 | `				}` |
|      21 | 2182 | `				if( rc == SXERR_EOF ){` |
|       - | 2183 | `					/* No more token to process. Missing closing braces */` |
|      21 | 2184 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 | 2185 | `					break;` |
|       - | 2186 | `				}` |
|     ! 0 | 2187 | `			}` |
|  609824 | 2188 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2189 | `				/* Closing braces found,break immediately*/` |
|  208766 | 2190 | `				pGen->pIn++;` |
|  208766 | 2191 | `				break;` |
|       - | 2192 | `			}` |
|       - | 2193 | `			/* Compile a single statement */` |
|  401060 | 2194 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  401060 | 2195 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2196 | `				return SXERR_ABORT;` |
|       - | 2197 | `			}` |
|       2 | 2198 | `		}` |
|  208786 | 2199 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  105692 | 2200 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 | 2201 | `		pGen->pIn++;` |
|     ! 0 | 2202 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 | 2203 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2204 | `			return SXERR_ABORT;` |
|       - | 2205 | `		}` |
|       - | 2206 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 | 2207 | `		for(;;){` |
|     ! 0 | 2208 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2209 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 | 2210 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2211 | `			 	   return SXERR_ABORT;` |
|       - | 2212 | `				}` |
|     ! 0 | 2213 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - | 2214 | `					/* No more token to process */` |
|     ! 0 | 2215 | `					if( rc == SXERR_EOF ){` |
|     ! 0 | 2216 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - | 2217 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 | 2218 | `					}` |
|     ! 0 | 2219 | `					break;` |
|       - | 2220 | `				}` |
|     ! 0 | 2221 | `			}` |
|     ! 0 | 2222 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 2223 | `				sxi32 nKwrd;` |
|       - | 2224 | `				/* Keyword found */` |
|     ! 0 | 2225 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 | 2226 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 | 2227 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - | 2228 | `						/* Delimiter keyword found,break */` |
|     ! 0 | 2229 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 | 2230 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 | 2231 | `						}` |
|     ! 0 | 2232 | `						break;` |
|       - | 2233 | `				}` |
|     ! 0 | 2234 | `			}` |
|       - | 2235 | `			/* Compile a single statement */` |
|     ! 0 | 2236 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 | 2237 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2238 | `				return SXERR_ABORT;` |
|       - | 2239 | `			}` |
|     ! 0 | 2240 | `		}` |
|     ! 0 | 2241 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 | 2242 | `	}else{` |
|       - | 2243 | `		/* Compile a single statement */` |
|    1300 | 2244 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1300 | 2245 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2246 | `			return SXERR_ABORT;` |
|       - | 2247 | `		}` |
|       - | 2248 | `	}` |
|       - | 2249 | `	/* Jump trailing semi-colons ';' */` |
|  210084 | 2250 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2251 | `		pGen->pIn++;` |
|     ! 0 | 2252 | `	}` |
|  210084 | 2253 | `	return SXRET_OK;` |
|  105043 | 2254 |  |
|       - | 2255 | `/*` |
|       - | 2256 | ` * Compile the gentle 'while' statement.` |
|       - | 2257 | ` * According to the PHP language reference` |
|       - | 2258 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - | 2259 | ` *  The basic form of a while statement is:` |
|       - | 2260 | ` *  while (expr)` |
|       - | 2261 | ` *   statement` |
|       - | 2262 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - | 2263 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - | 2264 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - | 2265 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - | 2266 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - | 2267 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - | 2268 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - | 2269 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - | 2270 | ` *  while (expr):` |
|       - | 2271 | ` *    statement` |
|       - | 2272 | ` *   endwhile;` |
|       - | 2273 | ` */` |
|    9292 | 2274 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 2275 |  |
|    9294 | 2276 | `	GenBlock *pWhileBlock = 0;` |
|    9294 | 2277 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2278 | `	sxu32 nFalseJump;` |
|       - | 2279 | `	sxu32 nLine;` |
|       - | 2280 | `	sxi32 rc;` |
|    9294 | 2281 | `	nLine = pGen->pIn->nLine;` |
|       - | 2282 | `	/* Jump the 'while' keyword */` |
|    9294 | 2283 | `	pGen->pIn++;` |
|    9294 | 2284 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2285 | `		/* Syntax error */` |
|     ! 0 | 2286 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2287 | `		if( rc == SXERR_ABORT ){` |
|       - | 2288 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2289 | `			return SXERR_ABORT;` |
|       - | 2290 | `		}` |
|     ! 0 | 2291 | `		goto Synchronize;` |
|       - | 2292 | `	}` |
|       - | 2293 | `	/* Jump the left parenthesis '(' */` |
|    9294 | 2294 | `	pGen->pIn++;` |
|       - | 2295 | `	/* Create the loop block */` |
|    9294 | 2296 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    9294 | 2297 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2298 | `		return SXERR_ABORT;` |
|       - | 2299 | `	}` |
|       - | 2300 | `	/* Delimit the condition */` |
|    9294 | 2301 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    9294 | 2302 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2303 | `		/* Empty expression */` |
|       3 | 2304 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2305 | `		if( rc == SXERR_ABORT ){` |
|       - | 2306 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2307 | `			return SXERR_ABORT;` |
|       - | 2308 | `		}` |
|       1 | 2309 | `	}` |
|       - | 2310 | `	/* Swap token streams */` |
|    9294 | 2311 | `	pTmp = pGen->pEnd;` |
|    9294 | 2312 | `	pGen->pEnd = pEnd;` |
|       - | 2313 | `	/* Compile the expression */` |
|    9294 | 2314 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    9294 | 2315 | `	if( rc == SXERR_ABORT ){` |
|       - | 2316 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2317 | `		return SXERR_ABORT;` |
|       - | 2318 | `	}` |
|       - | 2319 | `	/* Update token stream */` |
|    9294 | 2320 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2321 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2322 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2323 | `			return SXERR_ABORT;` |
|       - | 2324 | `		}` |
|     ! 0 | 2325 | `		pGen->pIn++;` |
|     ! 0 | 2326 | `	}` |
|       - | 2327 | `	/* Synchronize pointers */` |
|    9294 | 2328 | `	pGen->pIn  = &pEnd[1];` |
|    9294 | 2329 | `	pGen->pEnd = pTmp;` |
|       - | 2330 | `	/* Emit the false jump */` |
|    9294 | 2331 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2332 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    9294 | 2333 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2334 | `	/* Compile the loop body */` |
|    9294 | 2335 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    9294 | 2336 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2337 | `		return SXERR_ABORT;` |
|       - | 2338 | `	}` |
|       - | 2339 | `	/* Emit the unconditional jump to the start of the loop */` |
|    9294 | 2340 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2341 | `	/* Fix all jumps now the destination is resolved */` |
|    9294 | 2342 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2343 | `	/* Release the loop block */` |
|    9294 | 2344 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2345 | `	/* Statement successfully compiled */` |
|    9294 | 2346 | `	return SXRET_OK;` |
|     ! 0 | 2347 | `Synchronize:` |
|       - | 2348 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2349 | `	 * compiling this erroneous block.` |
|       - | 2350 | `	 */` |
|     ! 0 | 2351 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2352 | `		pGen->pIn++;` |
|     ! 0 | 2353 | `	}` |
|     ! 0 | 2354 | `	return SXRET_OK;` |
|    4648 | 2355 |  |
|       - | 2356 | `/*` |
|       - | 2357 | ` * Compile the ugly do..while() statement.` |
|       - | 2358 | ` * According to the PHP language reference` |
|       - | 2359 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - | 2360 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - | 2361 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - | 2362 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - | 2363 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - | 2364 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - | 2365 | ` *  would end immediately).` |
|       - | 2366 | ` *  There is just one syntax for do-while loops:` |
|       - | 2367 | ` *  <?php` |
|       - | 2368 | ` *  $i = 0;` |
|       - | 2369 | ` *  do {` |
|       - | 2370 | ` *   echo $i;` |
|       - | 2371 | ` *  } while ($i > 0);` |
|       - | 2372 | ` * ?>` |
|       - | 2373 | ` */` |
|       2 | 2374 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 | 2375 |  |
|       3 | 2376 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 | 2377 | `	GenBlock *pDoBlock = 0;` |
|       - | 2378 | `	sxu32 nLine;` |
|       - | 2379 | `	sxi32 rc;` |
|       3 | 2380 | `	nLine = pGen->pIn->nLine;` |
|       - | 2381 | `	/* Jump the 'do' keyword */` |
|       3 | 2382 | `	pGen->pIn++;` |
|       - | 2383 | `	/* Create the loop block */` |
|       3 | 2384 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 | 2385 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2386 | `		return SXERR_ABORT;` |
|       - | 2387 | `	}` |
|       - | 2388 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 | 2389 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 | 2390 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 | 2391 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2392 | `		return SXERR_ABORT;` |
|       - | 2393 | `	}` |
|       3 | 2394 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2395 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 | 2396 | `	}` |
|       3 | 2397 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 | 2398 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - | 2399 | `			/* Missing 'while' statement */` |
|       3 | 2400 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 | 2401 | `			if( rc == SXERR_ABORT ){` |
|       - | 2402 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2403 | `				return SXERR_ABORT;` |
|       - | 2404 | `			}` |
|       3 | 2405 | `			goto Synchronize;` |
|       - | 2406 | `	}` |
|       - | 2407 | `	/* Jump the 'while' keyword */` |
|     ! 0 | 2408 | `	pGen->pIn++;` |
|     ! 0 | 2409 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2410 | `		/* Syntax error */` |
|     ! 0 | 2411 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2412 | `		if( rc == SXERR_ABORT ){` |
|       - | 2413 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2414 | `			return SXERR_ABORT;` |
|       - | 2415 | `		}` |
|     ! 0 | 2416 | `		goto Synchronize;` |
|       - | 2417 | `	}` |
|       - | 2418 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 | 2419 | `	pGen->pIn++;` |
|       - | 2420 | `	/* Delimit the condition */` |
|     ! 0 | 2421 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 | 2422 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2423 | `		/* Empty expression */` |
|     ! 0 | 2424 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 | 2425 | `		if( rc == SXERR_ABORT ){` |
|       - | 2426 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2427 | `			return SXERR_ABORT;` |
|       - | 2428 | `		}` |
|     ! 0 | 2429 | `		goto Synchronize;` |
|       - | 2430 | `	}` |
|       - | 2431 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 | 2432 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - | 2433 | `		JumpFixup *aPost;` |
|       - | 2434 | `		VmInstr *pInstr;` |
|       - | 2435 | `		sxu32 nJumpDest;` |
|       - | 2436 | `		sxu32 n;` |
|     ! 0 | 2437 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 | 2438 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 | 2439 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 | 2440 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 | 2441 | `			if( pInstr ){` |
|       - | 2442 | `				/* Fix */` |
|     ! 0 | 2443 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 | 2444 | `			}` |
|     ! 0 | 2445 | `		}` |
|     ! 0 | 2446 | `	}` |
|       - | 2447 | `	/* Swap token streams */` |
|     ! 0 | 2448 | `	pTmp = pGen->pEnd;` |
|     ! 0 | 2449 | `	pGen->pEnd = pEnd;` |
|       - | 2450 | `	/* Compile the expression */` |
|     ! 0 | 2451 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 2452 | `	if( rc == SXERR_ABORT ){` |
|       - | 2453 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2454 | `		return SXERR_ABORT;` |
|       - | 2455 | `	}` |
|       - | 2456 | `	/* Update token stream */` |
|     ! 0 | 2457 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2458 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2459 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2460 | `			return SXERR_ABORT;` |
|       - | 2461 | `		}` |
|     ! 0 | 2462 | `		pGen->pIn++;` |
|     ! 0 | 2463 | `	}` |
|     ! 0 | 2464 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 | 2465 | `	pGen->pEnd = pTmp;` |
|       - | 2466 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 | 2467 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - | 2468 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 | 2469 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2470 | `	/* Release the loop block */` |
|     ! 0 | 2471 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2472 | `	/* Statement successfully compiled */` |
|     ! 0 | 2473 | `	return SXRET_OK;` |
|       1 | 2474 | `Synchronize:` |
|       - | 2475 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2476 | `	 * compiling this erroneous block.` |
|       - | 2477 | `	 */` |
|       3 | 2478 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2479 | `		pGen->pIn++;` |
|     ! 0 | 2480 | `	}` |
|       3 | 2481 | `	return SXRET_OK;` |
|       2 | 2482 |  |
|       - | 2483 | `/*` |
|       - | 2484 | ` * Compile the complex and powerful 'for' statement.` |
|       - | 2485 | ` * According to the PHP language reference` |
|       - | 2486 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - | 2487 | ` *  The syntax of a for loop is:` |
|       - | 2488 | ` *  for (expr1; expr2; expr3)` |
|       - | 2489 | ` *   statement` |
|       - | 2490 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - | 2491 | ` *  the beginning of the loop.` |
|       - | 2492 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - | 2493 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - | 2494 | ` *  to FALSE, the execution of the loop ends.` |
|       - | 2495 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - | 2496 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - | 2497 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - | 2498 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - | 2499 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - | 2500 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - | 2501 | ` *  of using the for truth expression.` |
|       - | 2502 | ` */` |
|    9292 | 2503 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 2504 |  |
|    9294 | 2505 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    9294 | 2506 | `	GenBlock *pForBlock = 0;` |
|       - | 2507 | `	sxu32 nFalseJump;` |
|       - | 2508 | `	sxu32 nLine;` |
|       - | 2509 | `	sxi32 rc;` |
|    9294 | 2510 | `	nLine = pGen->pIn->nLine;` |
|       - | 2511 | `	/* Jump the 'for' keyword */` |
|    9294 | 2512 | `	pGen->pIn++;` |
|    9294 | 2513 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2514 | `		/* Syntax error */` |
|     ! 0 | 2515 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2516 | `		if( rc == SXERR_ABORT ){` |
|       - | 2517 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2518 | `			return SXERR_ABORT;` |
|       - | 2519 | `		}` |
|     ! 0 | 2520 | `		return SXRET_OK;` |
|       - | 2521 | `	}` |
|       - | 2522 | `	/* Jump the left parenthesis '(' */` |
|    9294 | 2523 | `	pGen->pIn++;` |
|       - | 2524 | `	/* Delimit the init-expr;condition;post-expr */` |
|    9294 | 2525 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    9294 | 2526 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2527 | `		/* Empty expression */` |
|     ! 0 | 2528 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 | 2529 | `		if( rc == SXERR_ABORT ){` |
|       - | 2530 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2531 | `			return SXERR_ABORT;` |
|       - | 2532 | `		}` |
|       - | 2533 | `		/* Synchronize */` |
|     ! 0 | 2534 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2535 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2536 | `			pGen->pIn++;` |
|     ! 0 | 2537 | `		}` |
|     ! 0 | 2538 | `		return SXRET_OK;` |
|       - | 2539 | `	}` |
|       - | 2540 | `	/* Swap token streams */` |
|    9294 | 2541 | `	pTmp = pGen->pEnd;` |
|    9294 | 2542 | `	pGen->pEnd = pEnd;` |
|       - | 2543 | `	/* Compile initialization expressions if available */` |
|    9294 | 2544 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2545 | `	/* Pop operand lvalues */` |
|    9294 | 2546 | `	if( rc == SXERR_ABORT ){` |
|       - | 2547 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2548 | `		return SXERR_ABORT;` |
|    9294 | 2549 | `	}else if( rc != SXERR_EMPTY ){` |
|    9292 | 2550 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    4645 | 2551 | `	}` |
|    9294 | 2552 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2553 | `		/* Syntax error */` |
|     ! 0 | 2554 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2555 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 | 2556 | `		if( rc == SXERR_ABORT ){` |
|       - | 2557 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2558 | `			return SXERR_ABORT;` |
|       - | 2559 | `		}` |
|     ! 0 | 2560 | `		return SXRET_OK;` |
|       - | 2561 | `	}` |
|       - | 2562 | `	/* Jump the trailing ';' */` |
|    9294 | 2563 | `	pGen->pIn++;` |
|       - | 2564 | `	/* Create the loop block */` |
|    9294 | 2565 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    9294 | 2566 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2567 | `		return SXERR_ABORT;` |
|       - | 2568 | `	}` |
|       - | 2569 | `	/* Deffer continue jumps */` |
|    9294 | 2570 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 2571 | `	/* Compile the condition */` |
|    9294 | 2572 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    9294 | 2573 | `	if( rc == SXERR_ABORT ){` |
|       - | 2574 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2575 | `		return SXERR_ABORT;` |
|    9294 | 2576 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 2577 | `		/* Emit the false jump */` |
|    9292 | 2578 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2579 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    9292 | 2580 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    4645 | 2581 | `	}` |
|    9294 | 2582 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2583 | `		/* Syntax error */` |
|       5 | 2584 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2585 | `			"for: Expected ';' after conditionals expressions");` |
|       5 | 2586 | `		if( rc == SXERR_ABORT ){` |
|       - | 2587 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2588 | `			return SXERR_ABORT;` |
|       - | 2589 | `		}` |
|       5 | 2590 | `		return SXRET_OK;` |
|       - | 2591 | `	}` |
|       - | 2592 | `	/* Jump the trailing ';' */` |
|    9290 | 2593 | `	pGen->pIn++;` |
|       - | 2594 | `	/* Save the post condition stream */` |
|    9290 | 2595 | `	pPostStart = pGen->pIn;` |
|       - | 2596 | `	/* Compile the loop body */` |
|    9290 | 2597 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    9290 | 2598 | `	pGen->pEnd = pTmp;` |
|    9290 | 2599 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    9290 | 2600 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2601 | `		return SXERR_ABORT;` |
|       - | 2602 | `	}` |
|       - | 2603 | `	/* Fix post-continue jumps */` |
|    9290 | 2604 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 2605 | `		JumpFixup *aPost;` |
|       - | 2606 | `		VmInstr *pInstr;` |
|       - | 2607 | `		sxu32 nJumpDest;` |
|       - | 2608 | `		sxu32 n;` |
|       8 | 2609 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|       8 | 2610 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      14 | 2611 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|       8 | 2612 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       8 | 2613 | `			if( pInstr ){` |
|       - | 2614 | `				/* Fix jump */` |
|       8 | 2615 | `				pInstr->iP2 = nJumpDest;` |
|       3 | 2616 | `			}` |
|       5 | 2617 | `		}` |
|       3 | 2618 | `	}` |
|       - | 2619 | `	/* compile the post-expressions if available */` |
|    9290 | 2620 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2621 | `		pPostStart++;` |
|     ! 0 | 2622 | `	}` |
|    9290 | 2623 | `	if( pPostStart < pEnd ){` |
|       - | 2624 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    9290 | 2625 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    9290 | 2626 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    9290 | 2627 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 2628 | `			/* Syntax error */` |
|     ! 0 | 2629 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 2630 | `			if( rc == SXERR_ABORT ){` |
|       - | 2631 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2632 | `				return SXERR_ABORT;` |
|       - | 2633 | `			}` |
|     ! 0 | 2634 | `			return SXRET_OK;` |
|       - | 2635 | `		}` |
|    9290 | 2636 | `		RE_SWAP_DELIMITER(pGen);` |
|    9290 | 2637 | `		if( rc == SXERR_ABORT ){` |
|       - | 2638 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2639 | `			return SXERR_ABORT;` |
|    9290 | 2640 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 2641 | `			/* Pop operand lvalue */` |
|    9290 | 2642 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    4644 | 2643 | `		}` |
|    4644 | 2644 | `	}` |
|       - | 2645 | `	/* Emit the unconditional jump to the start of the loop */` |
|    9290 | 2646 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 2647 | `	/* Fix all jumps now the destination is resolved */` |
|    9290 | 2648 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2649 | `	/* Release the loop block */` |
|    9290 | 2650 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2651 | `	/* Statement successfully compiled */` |
|    9290 | 2652 | `	return SXRET_OK;` |
|    4648 | 2653 |  |
|       - | 2654 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 2655 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 2656 | ` * are allowed.` |
|       - | 2657 | ` */` |
|    4940 | 2658 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 2659 |  |
|    4942 | 2660 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    4942 | 2661 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 2662 | `		/* Unexpected expression */` |
|     ! 0 | 2663 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 2664 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 2665 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 2666 | `			rc = SXERR_INVALID;` |
|     ! 0 | 2667 | `		}` |
|     ! 0 | 2668 | `	}` |
|    4942 | 2669 | `	return rc;` |
|       2 | 2670 |  |
|       - | 2671 | `/*` |
|       - | 2672 | ` * Compile the 'foreach' statement.` |
|       - | 2673 | ` * According to the PHP language reference` |
|       - | 2674 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - | 2675 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - | 2676 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - | 2677 | ` *  is a minor but useful extension of the first:` |
|       - | 2678 | ` *  foreach (array_expression as $value)` |
|       - | 2679 | ` *    statement` |
|       - | 2680 | ` *  foreach (array_expression as $key => $value)` |
|       - | 2681 | ` *   statement` |
|       - | 2682 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - | 2683 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - | 2684 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - | 2685 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - | 2686 | ` *  to the variable $key on each loop.` |
|       - | 2687 | ` *  Note:` |
|       - | 2688 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - | 2689 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - | 2690 | ` *  Note:` |
|       - | 2691 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - | 2692 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - | 2693 | ` *  or after the foreach without resetting it.` |
|       - | 2694 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - | 2695 | ` *  of copying the value.` |
|       - | 2696 | ` */` |
|    2492 | 2697 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 2698 |  |
|    2494 | 2699 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    2494 | 2700 | `	GenBlock *pForeachBlock = 0;` |
|       - | 2701 | `	ph7_foreach_info *pInfo;` |
|       - | 2702 | `	sxu32 nFalseJump;` |
|       - | 2703 | `	VmInstr *pInstr;` |
|       - | 2704 | `	sxu32 nLine;` |
|       - | 2705 | `	sxi32 rc;` |
|    2494 | 2706 | `	nLine = pGen->pIn->nLine;` |
|       - | 2707 | `	/* Jump the 'foreach' keyword */` |
|    2494 | 2708 | `	pGen->pIn++;` |
|    2494 | 2709 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2710 | `		/* Syntax error */` |
|     ! 0 | 2711 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 2712 | `		if( rc == SXERR_ABORT ){` |
|       - | 2713 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2714 | `			return SXERR_ABORT;` |
|       - | 2715 | `		}` |
|     ! 0 | 2716 | `		goto Synchronize;` |
|       - | 2717 | `	}` |
|       - | 2718 | `	/* Jump the left parenthesis '(' */` |
|    2494 | 2719 | `	pGen->pIn++;` |
|       - | 2720 | `	/* Create the loop block */` |
|    2494 | 2721 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    2494 | 2722 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2723 | `		return SXERR_ABORT;` |
|       - | 2724 | `	}` |
|       - | 2725 | `	/* Delimit the expression */` |
|    2494 | 2726 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    2494 | 2727 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2728 | `		/* Empty expression */` |
|     ! 0 | 2729 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 2730 | `		if( rc == SXERR_ABORT ){` |
|       - | 2731 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2732 | `			return SXERR_ABORT;` |
|       - | 2733 | `		}` |
|       - | 2734 | `		/* Synchronize */` |
|     ! 0 | 2735 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2736 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2737 | `			pGen->pIn++;` |
|     ! 0 | 2738 | `		}` |
|     ! 0 | 2739 | `		return SXRET_OK;` |
|       - | 2740 | `	}` |
|       - | 2741 | `	/* Compile the array expression */` |
|    2494 | 2742 | `	pCur = pGen->pIn;` |
|   16726 | 2743 | `	while( pCur < pEnd ){` |
|   16726 | 2744 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    2504 | 2745 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    2504 | 2746 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 2747 | `				/* Break with the first 'as' found */` |
|    2494 | 2748 | `				break;` |
|       - | 2749 | `			}` |
|       5 | 2750 | `		}` |
|       - | 2751 | `		/* Advance the stream cursor */` |
|   14234 | 2752 | `		pCur++;` |
|       2 | 2753 | `	}` |
|    2494 | 2754 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 2755 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2756 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 2757 | `		if( rc == SXERR_ABORT ){` |
|       - | 2758 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2759 | `			return SXERR_ABORT;` |
|       - | 2760 | `		}` |
|     ! 0 | 2761 | `		goto Synchronize;` |
|       - | 2762 | `	}` |
|       - | 2763 | `	/* Swap token streams */` |
|    2494 | 2764 | `	pTmp = pGen->pEnd;` |
|    2494 | 2765 | `	pGen->pEnd = pCur;` |
|    2494 | 2766 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    2494 | 2767 | `	if( rc == SXERR_ABORT ){` |
|       - | 2768 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2769 | `		return SXERR_ABORT;` |
|       - | 2770 | `	}` |
|       - | 2771 | `	/* Update token stream */` |
|    2494 | 2772 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 2773 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2774 | `		if( rc == SXERR_ABORT ){` |
|       - | 2775 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2776 | `			return SXERR_ABORT;` |
|       - | 2777 | `		}` |
|     ! 0 | 2778 | `		pGen->pIn++;` |
|     ! 0 | 2779 | `	}` |
|    2494 | 2780 | `	pCur++; /* Jump the 'as' keyword */` |
|    2494 | 2781 | `	pGen->pIn = pCur;` |
|    2494 | 2782 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2783 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 2784 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2785 | `			return SXERR_ABORT;` |
|       - | 2786 | `		}` |
|     ! 0 | 2787 | `	}` |
|       - | 2788 | `	/* Create the foreach context */` |
|    2494 | 2789 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    2494 | 2790 | `	if( pInfo == 0 ){` |
|     ! 0 | 2791 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 2792 | `		return SXERR_ABORT;` |
|       - | 2793 | `	}` |
|       - | 2794 | `	/* Zero the structure */` |
|    2494 | 2795 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 2796 | `	/* Initialize structure fields */` |
|    2494 | 2797 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 2798 | `	/* Check if we have a key field */` |
|    7480 | 2799 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    4988 | 2800 | `		pCur++;` |
|       2 | 2801 | `	}` |
|    2494 | 2802 | `	if( pCur < pEnd ){` |
|       - | 2803 | `		/* Compile the expression holding the key name */` |
|    2450 | 2804 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 2805 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 2806 | `			if( rc == SXERR_ABORT ){` |
|       - | 2807 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2808 | `				return SXERR_ABORT;` |
|       - | 2809 | `			}` |
|     ! 0 | 2810 | `		}else{` |
|    2450 | 2811 | `			pGen->pEnd = pCur;` |
|    2450 | 2812 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2450 | 2813 | `			if( rc == SXERR_ABORT ){` |
|       - | 2814 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2815 | `				return SXERR_ABORT;` |
|       - | 2816 | `			}` |
|    2450 | 2817 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2450 | 2818 | `			if( pInstr->p3 ){` |
|       - | 2819 | `				/* Record key name */` |
|    2450 | 2820 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1224 | 2821 | `			}` |
|    2450 | 2822 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 2823 | `		}` |
|    2450 | 2824 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1224 | 2825 | `	}` |
|    2494 | 2826 | `	pGen->pEnd = pEnd;` |
|    2494 | 2827 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2828 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 2829 | `		if( rc == SXERR_ABORT ){` |
|       - | 2830 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2831 | `			return SXERR_ABORT;` |
|       - | 2832 | `		}` |
|     ! 0 | 2833 | `		goto Synchronize;` |
|       - | 2834 | `	}` |
|    2494 | 2835 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       7 | 2836 | `		pGen->pIn++;` |
|       - | 2837 | `		/* Pass by reference  */` |
|       7 | 2838 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       3 | 2839 | `	}` |
|       - | 2840 | `	/* Compile the expression holding the value name */` |
|    2494 | 2841 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2494 | 2842 | `	if( rc == SXERR_ABORT ){` |
|       - | 2843 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2844 | `		return SXERR_ABORT;` |
|       - | 2845 | `	}` |
|    2494 | 2846 | `	pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2494 | 2847 | `	if( pInstr->p3 ){` |
|       - | 2848 | `		/* Record value name */` |
|    2494 | 2849 | `		SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1246 | 2850 | `	}` |
|       - | 2851 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    2494 | 2852 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 2853 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2494 | 2854 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 2855 | `	/* Record the first instruction to execute */` |
|    2494 | 2856 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2857 | `	/* Emit the FOREACH_STEP instruction */` |
|    2494 | 2858 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 2859 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2494 | 2860 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 2861 | `	/* Compile the loop body */` |
|    2494 | 2862 | `	pGen->pIn = &pEnd[1];` |
|    2494 | 2863 | `	pGen->pEnd = pTmp;` |
|    2494 | 2864 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    2494 | 2865 | `	if( rc == SXERR_ABORT ){` |
|       - | 2866 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2867 | `		return SXERR_ABORT;` |
|       - | 2868 | `	}` |
|       - | 2869 | `	/* Emit the unconditional jump to the start of the loop */` |
|    2494 | 2870 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 2871 | `	/* Fix all jumps now the destination is resolved */` |
|    2494 | 2872 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2873 | `	/* Release the loop block */` |
|    2494 | 2874 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2875 | `	/* Statement successfully compiled */` |
|    2494 | 2876 | `	return SXRET_OK;` |
|     ! 0 | 2877 | `Synchronize:` |
|       - | 2878 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2879 | `	 * compiling this erroneous block.` |
|       - | 2880 | `	 */` |
|     ! 0 | 2881 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2882 | `		pGen->pIn++;` |
|     ! 0 | 2883 | `	}` |
|     ! 0 | 2884 | `	return SXRET_OK;` |
|    1248 | 2885 |  |
|       - | 2886 | `/*` |
|       - | 2887 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 2888 | ` * According to the PHP language reference` |
|       - | 2889 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 2890 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 2891 | ` *  that is similar to that of C:` |
|       - | 2892 | ` *  if (expr)` |
|       - | 2893 | ` *   statement` |
|       - | 2894 | ` *  else construct:` |
|       - | 2895 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 2896 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 2897 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 2898 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 2899 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 2900 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 2901 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 2902 | ` *  elseif` |
|       - | 2903 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 2904 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 2905 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 2906 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 2907 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 2908 | ` *   <?php` |
|       - | 2909 | ` *    if ($a > $b) {` |
|       - | 2910 | ` *     echo "a is bigger than b";` |
|       - | 2911 | ` *    } elseif ($a == $b) {` |
|       - | 2912 | ` *     echo "a is equal to b";` |
|       - | 2913 | ` *    } else {` |
|       - | 2914 | ` *     echo "a is smaller than b";` |
|       - | 2915 | ` *    }` |
|       - | 2916 | ` *    ?>` |
|       - | 2917 | ` */` |
|   92792 | 2918 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 2919 |  |
|   92794 | 2920 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   92794 | 2921 | `	GenBlock *pCondBlock = 0;` |
|       - | 2922 | `	sxu32 nJumpIdx;` |
|       - | 2923 | `	sxu32 nKeyID;` |
|       - | 2924 | `	sxi32 rc;` |
|       - | 2925 | `	/* Jump the 'if' keyword */` |
|   92794 | 2926 | `	pGen->pIn++;` |
|   92794 | 2927 | `	pToken = pGen->pIn;` |
|       - | 2928 | `	/* Create the conditional block */` |
|   92794 | 2929 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   92794 | 2930 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2931 | `		return SXERR_ABORT;` |
|       - | 2932 | `	}` |
|       - | 2933 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   51016 | 2934 | `	for(;;){` |
|  102034 | 2935 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2936 | `			/* Syntax error */` |
|     ! 0 | 2937 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 2938 | `				pToken--;` |
|     ! 0 | 2939 | `			}` |
|     ! 0 | 2940 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 2941 | `			if( rc == SXERR_ABORT ){` |
|       - | 2942 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2943 | `				return SXERR_ABORT;` |
|       - | 2944 | `			}` |
|     ! 0 | 2945 | `			goto Synchronize;` |
|       - | 2946 | `		}` |
|       - | 2947 | `		/* Jump the left parenthesis '(' */` |
|  102034 | 2948 | `		pToken++;` |
|       - | 2949 | `		/* Delimit the condition */` |
|  102034 | 2950 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  102034 | 2951 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 2952 | `			/* Syntax error */` |
|     ! 0 | 2953 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 2954 | `				pToken--;` |
|     ! 0 | 2955 | `			}` |
|     ! 0 | 2956 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 2957 | `			if( rc == SXERR_ABORT ){` |
|       - | 2958 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2959 | `				return SXERR_ABORT;` |
|       - | 2960 | `			}` |
|     ! 0 | 2961 | `			goto Synchronize;` |
|       - | 2962 | `		}` |
|       - | 2963 | `		/* Swap token streams */` |
|  102034 | 2964 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 2965 | `		/* Compile the condition */` |
|  102034 | 2966 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2967 | `		/* Update token stream */` |
|  102034 | 2968 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 2969 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2970 | `			pGen->pIn++;` |
|     ! 0 | 2971 | `		}` |
|  102034 | 2972 | `		pGen->pIn  = &pEnd[1];` |
|  102034 | 2973 | `		pGen->pEnd = pTmp;` |
|  102034 | 2974 | `		if( rc == SXERR_ABORT ){` |
|       - | 2975 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2976 | `			return SXERR_ABORT;` |
|       - | 2977 | `		}` |
|       - | 2978 | `		/* Emit the false jump */` |
|  102034 | 2979 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 2980 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  102034 | 2981 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 2982 | `		/* Compile the body */` |
|  102034 | 2983 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  102034 | 2984 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2985 | `			return SXERR_ABORT;` |
|       - | 2986 | `		}` |
|  102034 | 2987 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   27481 | 2988 | `			break;` |
|       - | 2989 | `		}` |
|       - | 2990 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   47076 | 2991 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   47076 | 2992 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   30204 | 2993 | `			break;` |
|       - | 2994 | `		}` |
|       - | 2995 | `		/* Emit the unconditional jump */` |
|   16874 | 2996 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 2997 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   16874 | 2998 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   16874 | 2999 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   12242 | 3000 | `			pToken = &pGen->pIn[1];` |
|   12242 | 3001 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    4638 | 3002 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    3818 | 3003 | `					break;` |
|       - | 3004 | `			}` |
|    4610 | 3005 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2304 | 3006 | `		}` |
|    9242 | 3007 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 3008 | `		/* Synchronize cursors */` |
|    9242 | 3009 | `		pToken = pGen->pIn;` |
|       - | 3010 | `		/* Fix the false jump */` |
|    9242 | 3011 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 3012 | `	} /* For(;;) */` |
|       - | 3013 | `	/* Fix the false jump */` |
|   92794 | 3014 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   92794 | 3015 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   37834 | 3016 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 3017 | `			/* Compile the else block */` |
|    7634 | 3018 | `			pGen->pIn++;` |
|    7634 | 3019 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    7634 | 3020 | `			if( rc == SXERR_ABORT ){` |
|       - | 3021 |  |
|     ! 0 | 3022 | `				return SXERR_ABORT;` |
|       - | 3023 | `			}` |
|    3816 | 3024 | `	}` |
|   92794 | 3025 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3026 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   92794 | 3027 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 3028 | `	/* Release the conditional block */` |
|   92794 | 3029 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3030 | `	/* Statement successfully compiled */` |
|   92794 | 3031 | `	return SXRET_OK;` |
|     ! 0 | 3032 | `Synchronize:` |
|       - | 3033 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 3034 | `	 */` |
|     ! 0 | 3035 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3036 | `		pGen->pIn++;` |
|     ! 0 | 3037 | `	}` |
|     ! 0 | 3038 | `	return SXRET_OK;` |
|   46398 | 3039 |  |
|       - | 3040 | `/*` |
|       - | 3041 | ` * Compile the global construct.` |
|       - | 3042 | ` * According to the PHP language reference` |
|       - | 3043 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 3044 | ` *  to be used in that function.` |
|       - | 3045 | ` *  Example #1 Using global` |
|       - | 3046 | ` *  <?php` |
|       - | 3047 | ` *   $a = 1;` |
|       - | 3048 | ` *   $b = 2;` |
|       - | 3049 | ` *   function Sum()` |
|       - | 3050 | ` *   {` |
|       - | 3051 | ` *    global $a, $b;` |
|       - | 3052 | ` *    $b = $a + $b;` |
|       - | 3053 | ` *   }` |
|       - | 3054 | ` *   Sum();` |
|       - | 3055 | ` *   echo $b;` |
|       - | 3056 | ` *  ?>` |
|       - | 3057 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 3058 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 3059 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 3060 | ` */` |
|      26 | 3061 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 | 3062 |  |
|      28 | 3063 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3064 | `	sxi32 nExpr;` |
|       - | 3065 | `	sxi32 rc;` |
|       - | 3066 | `	/* Jump the 'global' keyword */` |
|      28 | 3067 | `	pGen->pIn++;` |
|      28 | 3068 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 3069 | `		/* Nothing to process */` |
|     ! 0 | 3070 | `		return SXRET_OK;` |
|       - | 3071 | `	}` |
|      28 | 3072 | `	pTmp = pGen->pEnd;` |
|      28 | 3073 | `	nExpr = 0;` |
|      56 | 3074 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      30 | 3075 | `		if( pGen->pIn < pNext ){` |
|      30 | 3076 | `			pGen->pEnd = pNext;` |
|      30 | 3077 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3078 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 3079 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 3080 | `					return SXERR_ABORT;` |
|       - | 3081 | `				}` |
|     ! 0 | 3082 | `			}else{` |
|      30 | 3083 | `				pGen->pIn++;` |
|      30 | 3084 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3085 | `					/* Emit a warning */` |
|     ! 0 | 3086 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 3087 | `				}else{` |
|      30 | 3088 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 | 3089 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3090 | `						return SXERR_ABORT;` |
|      30 | 3091 | `					}else if(rc != SXERR_EMPTY ){` |
|      30 | 3092 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      30 | 3093 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - | 3094 | `							/* Variable name, not a constant */` |
|      30 | 3095 | `							pLast->iP1 = 0;` |
|      14 | 3096 | `						}` |
|      30 | 3097 | `						nExpr++;` |
|      14 | 3098 | `					}` |
|       - | 3099 | `				}` |
|       - | 3100 | `			}` |
|      14 | 3101 | `		}` |
|       - | 3102 | `		/* Next expression in the stream */` |
|      30 | 3103 | `		pGen->pIn = pNext;` |
|       - | 3104 | `		/* Jump trailing commas */` |
|      32 | 3105 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3106 | `			pGen->pIn++;` |
|       1 | 3107 | `		}` |
|       2 | 3108 | `	}` |
|       - | 3109 | `	/* Restore token stream */` |
|      28 | 3110 | `	pGen->pEnd = pTmp;` |
|      28 | 3111 | `	if( nExpr > 0 ){` |
|       - | 3112 | `		/* Emit the uplink instruction */` |
|      28 | 3113 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      13 | 3114 | `	}` |
|      28 | 3115 | `	return SXRET_OK;` |
|      15 | 3116 |  |
|       - | 3117 | `/*` |
|       - | 3118 | ` * Compile the return statement.` |
|       - | 3119 | ` * According to the PHP language reference` |
|       - | 3120 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 3121 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 3122 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 3123 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 3124 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 3125 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 3126 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 3127 | ` *  from within the main script file, then script execution end.` |
|       - | 3128 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 3129 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 3130 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 3131 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 3132 | ` */` |
|   97152 | 3133 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 3134 |  |
|   97154 | 3135 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3136 | `	sxi32 rc;` |
|       - | 3137 | `	/* Jump the 'return' keyword */` |
|   97154 | 3138 | `	pGen->pIn++;` |
|   97154 | 3139 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3140 | `		/* Compile the expression */` |
|   97132 | 3141 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   97132 | 3142 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3143 | `			return SXERR_ABORT;` |
|   97132 | 3144 | `		}else if(rc != SXERR_EMPTY ){` |
|   97132 | 3145 | `			nRet = 1;` |
|   48565 | 3146 | `		}` |
|   48565 | 3147 | `	}` |
|       - | 3148 | `	/* Emit the done instruction */` |
|   97154 | 3149 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|   97154 | 3150 | `	return SXRET_OK;` |
|   48578 | 3151 |  |
|       - | 3152 | `/*` |
|       - | 3153 | ` * Compile the die/exit language construct.` |
|       - | 3154 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 3155 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 3156 | ` */` |
|      88 | 3157 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 | 3158 |  |
|      90 | 3159 | `	sxi32 nExpr = 0;` |
|       - | 3160 | `	sxi32 rc;` |
|       - | 3161 | `	/* Jump the die/exit keyword */` |
|      90 | 3162 | `	pGen->pIn++;` |
|      90 | 3163 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3164 | `		/* Compile the expression */` |
|      90 | 3165 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 | 3166 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3167 | `			return SXERR_ABORT;` |
|      90 | 3168 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 | 3169 | `			nExpr = 1;` |
|      44 | 3170 | `		}` |
|      44 | 3171 | `	}` |
|       - | 3172 | `	/* Emit the HALT instruction */` |
|      90 | 3173 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 | 3174 | `	return SXRET_OK;` |
|      46 | 3175 |  |
|       - | 3176 | `/*` |
|       - | 3177 | ` * Compile the 'echo' language construct.` |
|       - | 3178 | ` */` |
|    9316 | 3179 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 3180 |  |
|    9318 | 3181 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3182 | `	sxi32 rc;` |
|       - | 3183 | `	/* Jump the 'echo' keyword */` |
|    9318 | 3184 | `	pGen->pIn++;` |
|       - | 3185 | `	/* Compile arguments one after one */` |
|    9318 | 3186 | `	pTmp = pGen->pEnd;` |
|   18840 | 3187 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    9524 | 3188 | `		if( pGen->pIn < pNext ){` |
|    9524 | 3189 | `			pGen->pEnd = pNext;` |
|    9524 | 3190 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    9524 | 3191 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3192 | `				return SXERR_ABORT;` |
|    9524 | 3193 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3194 | `				/* Emit the consume instruction */` |
|    9500 | 3195 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    4749 | 3196 | `			}` |
|    4761 | 3197 | `		}` |
|       - | 3198 | `		/* Jump trailing commas */` |
|    9730 | 3199 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     208 | 3200 | `			pNext++;` |
|       2 | 3201 | `		}` |
|    9524 | 3202 | `		pGen->pIn = pNext;` |
|       2 | 3203 | `	}` |
|       - | 3204 | `	/* Restore token stream */` |
|    9318 | 3205 | `	pGen->pEnd = pTmp;` |
|    9318 | 3206 | `	return SXRET_OK;` |
|    4660 | 3207 |  |
|       - | 3208 | `/*` |
|       - | 3209 | ` * Compile the static statement.` |
|       - | 3210 | ` * According to the PHP language reference` |
|       - | 3211 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 3212 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 3213 | ` *  when program execution leaves this scope.` |
|       - | 3214 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 3215 | ` * Symisc eXtension.` |
|       - | 3216 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 3217 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3218 | ` *  Example` |
|       - | 3219 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 3220 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 3221 | ` */` |
|       2 | 3222 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 | 3223 |  |
|       - | 3224 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 3225 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 3226 | `	GenBlock *pBlock;` |
|       - | 3227 | `	SyString *pName;` |
|       - | 3228 | `	char *zDup;` |
|       - | 3229 | `	sxu32 nLine;` |
|       - | 3230 | `	sxi32 rc;` |
|       - | 3231 | `	/* Jump the static keyword */` |
|       3 | 3232 | `	nLine = pGen->pIn->nLine;` |
|       3 | 3233 | `	pGen->pIn++;` |
|       - | 3234 | `	/* Extract the enclosing function if any */` |
|       3 | 3235 | `	pBlock = pGen->pCurrent;` |
|       5 | 3236 | `	while( pBlock ){` |
|       5 | 3237 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 | 3238 | `			break;` |
|       - | 3239 | `		}` |
|       - | 3240 | `		/* Point to the upper block */` |
|       3 | 3241 | `		pBlock = pBlock->pParent;` |
|       1 | 3242 | `	}` |
|       3 | 3243 | `	if( pBlock == 0 ){` |
|       - | 3244 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 3245 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3246 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 | 3247 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3248 | `				return SXERR_ABORT;` |
|       - | 3249 | `			}` |
|     ! 0 | 3250 | `			goto Synchronize;` |
|       - | 3251 | `		}` |
|       - | 3252 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 3253 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 3254 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3255 | `			return SXERR_ABORT;` |
|     ! 0 | 3256 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 3257 | `			/* Emit the POP instruction */` |
|     ! 0 | 3258 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 3259 | `		}` |
|     ! 0 | 3260 | `		return SXRET_OK;` |
|       - | 3261 | `	}` |
|       3 | 3262 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 3263 | `	/* Make sure we are dealing with a valid statement */` |
|       3 | 3264 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 | 3265 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 | 3266 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 | 3267 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3268 | `				return SXERR_ABORT;` |
|       - | 3269 | `			}` |
|       3 | 3270 | `			goto Synchronize;` |
|       - | 3271 | `	}` |
|     ! 0 | 3272 | `	pGen->pIn++;` |
|       - | 3273 | `	/* Extract variable name */` |
|     ! 0 | 3274 | `	pName = &pGen->pIn->sData;` |
|     ! 0 | 3275 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 | 3276 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 3277 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3278 | `		goto Synchronize;` |
|       - | 3279 | `	}` |
|       - | 3280 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 | 3281 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 | 3282 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 3283 | `	/* Duplicate variable name */` |
|     ! 0 | 3284 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 | 3285 | `	if( zDup == 0 ){` |
|     ! 0 | 3286 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3287 | `		return SXERR_ABORT;` |
|       - | 3288 | `	}` |
|     ! 0 | 3289 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 3290 | `	/* Check if we have an expression to compile */` |
|     ! 0 | 3291 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 3292 | `		SySet *pInstrContainer;` |
|       - | 3293 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 3294 | `		 * Static variable can take any complex expression including function` |
|       - | 3295 | `		 * call as their initialization value.` |
|       - | 3296 | `		 * Example:` |
|       - | 3297 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 3298 | `		 */` |
|     ! 0 | 3299 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 3300 | `		/* Swap bytecode container */` |
|     ! 0 | 3301 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 | 3302 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 3303 | `		/* Compile the expression */` |
|     ! 0 | 3304 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3305 | `		/* Emit the done instruction */` |
|     ! 0 | 3306 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 3307 | `		/* Restore default bytecode container */` |
|     ! 0 | 3308 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 3309 | `	}` |
|       - | 3310 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 | 3311 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 | 3312 | `	return SXRET_OK;` |
|       1 | 3313 | `Synchronize:` |
|       - | 3314 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 3315 | `	 * statement.` |
|       - | 3316 | `	 */` |
|       5 | 3317 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 | 3318 | `		pGen->pIn++;` |
|       1 | 3319 | `	}` |
|       3 | 3320 | `	return SXRET_OK;` |
|       2 | 3321 |  |
|       - | 3322 | `/*` |
|       - | 3323 | ` * Compile the var statement.` |
|       - | 3324 | ` * Symisc Extension:` |
|       - | 3325 | ` *      var statement can be used outside of a class definition.` |
|       - | 3326 | ` */` |
|       4 | 3327 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 3328 |  |
|       - | 3329 | `	sxu32 nLine;` |
|       - | 3330 | `	sxi32 rc;` |
|       5 | 3331 | `	nLine = pGen->pIn->nLine;` |
|       - | 3332 | `	/* Jump the 'var' keyword */` |
|       5 | 3333 | `	pGen->pIn++;` |
|       5 | 3334 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 3335 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - | 3336 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 | 3337 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 | 3338 | `			pGen->pIn++;` |
|     ! 0 | 3339 | `		}` |
|     ! 0 | 3340 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3341 | `			return SXERR_ABORT;` |
|       - | 3342 | `		}` |
|     ! 0 | 3343 | `	}else{` |
|       - | 3344 | `		/* Compile the expression */` |
|       5 | 3345 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 | 3346 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3347 | `			return SXERR_ABORT;` |
|       5 | 3348 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 | 3349 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 3350 | `		}` |
|       - | 3351 | `	}` |
|       5 | 3352 | `	return SXRET_OK;` |
|       3 | 3353 |  |
|       - | 3354 | `/*` |
|       - | 3355 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 3356 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 3357 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 3358 | ` */` |
|       - | 3359 | `/*` |
|       - | 3360 | ` * Namespace-qualify a name for CALL/NEW instructions.` |
|       - | 3361 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 3362 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 3363 | ` * qualified name and updates the instruction's operand index.` |
|       - | 3364 | ` *` |
|       - | 3365 | ` * Resolution: use imports -> current NS prefix.` |
|       - | 3366 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 3367 | ` * Returns the (possibly new) literal index.` |
|       - | 3368 | ` */` |
|  231852 | 3369 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx)` |
|       2 | 3370 |  |
|       - | 3371 | `	ph7_value *pLit;` |
|       - | 3372 | `	const char *zLit;` |
|       - | 3373 | `	SyString sQualified;` |
|       - | 3374 | `	sxu32 nLit;` |
|       - | 3375 | `	sxu32 k;` |
|       - | 3376 | `	sxu32 nNewIdx;` |
|       - | 3377 | `	int hasNsSep;` |
|       - | 3378 | `	SyHashEntry *pImport;` |
|       - | 3379 | `	ph7_value *pNew;` |
|  231854 | 3380 | `	if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  231770 | 3381 | `		return nOrigIdx; /* Not in a namespace */` |
|       - | 3382 | `	}` |
|      85 | 3383 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|      85 | 3384 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 3385 | `		return nOrigIdx;` |
|       - | 3386 | `	}` |
|      85 | 3387 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|      85 | 3388 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 3389 | `	/* Skip if already qualified (contains backslash) */` |
|      85 | 3390 | `	hasNsSep = 0;` |
|     381 | 3391 | `	for( k = 0; k < nLit; k++ ){` |
|     339 | 3392 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|     149 | 3393 | `	}` |
|      85 | 3394 | `	if( hasNsSep ){` |
|      43 | 3395 | `		return nOrigIdx;` |
|       - | 3396 | `	}` |
|       - | 3397 | `	/* Build the qualified name into sWorker */` |
|      43 | 3398 | `	SyBlobReset(&pGen->sWorker);` |
|       - | 3399 | `	/* Check use imports first */` |
|      43 | 3400 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zLit,nLit);` |
|      43 | 3401 | `	if( pImport ){` |
|      11 | 3402 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      11 | 3403 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       6 | 3404 | `	}else{` |
|       - | 3405 | `		/* Prepend current namespace */` |
|      33 | 3406 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      33 | 3407 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      33 | 3408 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 3409 | `	}` |
|       - | 3410 | `	/* Look up or create a new literal for the qualified name */` |
|      43 | 3411 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      43 | 3412 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      11 | 3413 | `		return nNewIdx; /* Already interned */` |
|       - | 3414 | `	}` |
|      33 | 3415 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      33 | 3416 | `	if( pNew == 0 ){` |
|     ! 0 | 3417 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 3418 | `	}` |
|      33 | 3419 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      33 | 3420 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      33 | 3421 | `	return nNewIdx;` |
|  115928 | 3422 |  |
|       - | 3423 | `/*` |
|       - | 3424 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 3425 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 3426 | ` * The caller must release pOut when done.` |
|       - | 3427 | ` */` |
|   27948 | 3428 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3429 |  |
|   27950 | 3430 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      31 | 3431 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      31 | 3432 | `		SyBlobAppend(pOut,"\\",1);` |
|      15 | 3433 | `	}` |
|   27950 | 3434 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   27950 | 3435 |  |
|       - | 3436 | `/*` |
|       - | 3437 | ` * Compile a namespace statement` |
|       - | 3438 | ` * According to the PHP language reference manual` |
|       - | 3439 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 3440 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 3441 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 3442 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 3443 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 3444 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 3445 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 3446 | ` *  programming world.` |
|       - | 3447 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 3448 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 3449 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 3450 | ` *  classes/functions/constants.` |
|       - | 3451 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 3452 | ` *  readability of source code.` |
|       - | 3453 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 3454 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 3455 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 3456 | ` *       class MyClass {}` |
|       - | 3457 | ` *       function myfunction() {}` |
|       - | 3458 | ` *       const MYCONST = 1;` |
|       - | 3459 | ` *       $a = new MyClass;` |
|       - | 3460 | ` *       $c = new \my\name\MyClass;` |
|       - | 3461 | ` *       $a = strlen('hi');` |
|       - | 3462 | ` *       $d = namespace\MYCONST;` |
|       - | 3463 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 3464 | ` *       echo constant($d);` |
|       - | 3465 | ` * NOTE` |
|       - | 3466 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3467 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3468 | ` */` |
|       - | 3469 | `/*` |
|       - | 3470 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 3471 | ` */` |
|       6 | 3472 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 | 3473 |  |
|       7 | 3474 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|     ! 0 | 3475 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|     ! 0 | 3476 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|     ! 0 | 3477 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|     ! 0 | 3478 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|     ! 0 | 3479 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|     ! 0 | 3480 | `	return "token";` |
|       4 | 3481 |  |
|      52 | 3482 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       1 | 3483 |  |
|       - | 3484 | `	sxu32 nLine;` |
|       - | 3485 | `	sxi32 rc;` |
|      53 | 3486 | `	nLine = pGen->pIn->nLine;` |
|      53 | 3487 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 3488 | `	/* Reset namespace and clear previous use imports */` |
|      53 | 3489 | `	SyBlobReset(&pGen->sNamespace);` |
|      53 | 3490 | `	SyHashRelease(&pGen->hUseImports);` |
|      53 | 3491 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      53 | 3492 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3493 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 | 3494 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3495 | `		return SXRET_OK;` |
|       - | 3496 | `	}` |
|      53 | 3497 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - | 3498 | `		/* namespace; — switch to global namespace */` |
|     ! 0 | 3499 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3500 | `		return SXRET_OK;` |
|       - | 3501 | `	}` |
|      53 | 3502 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - | 3503 | `		/* namespace { } — global namespace block */` |
|     ! 0 | 3504 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3505 | `		return SXRET_OK;` |
|       - | 3506 | `	}` |
|       - | 3507 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     135 | 3508 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      83 | 3509 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 3510 | `			/* Append backslash separator */` |
|      17 | 3511 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      17 | 3512 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       8 | 3513 | `			}` |
|       9 | 3514 | `		}else{` |
|       - | 3515 | `			/* Append identifier */` |
|      67 | 3516 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 3517 | `		}` |
|      83 | 3518 | `		pGen->pIn++;` |
|       1 | 3519 | `	}` |
|       - | 3520 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - | 3521 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - | 3522 | `	{` |
|      53 | 3523 | `		char *zNsDup = 0;` |
|      53 | 3524 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      76 | 3525 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      50 | 3526 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      25 | 3527 | `		}` |
|      53 | 3528 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - | 3529 | `	}` |
|      53 | 3530 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 | 3531 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3532 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 3533 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 | 3534 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3535 | `			return SXERR_ABORT;` |
|       - | 3536 | `		}` |
|       2 | 3537 | `	}` |
|      53 | 3538 | `	return SXRET_OK;` |
|      27 | 3539 |  |
|       - | 3540 | `/*` |
|       - | 3541 | ` * Compile the 'use' statement` |
|       - | 3542 | ` * According to the PHP language reference manual` |
|       - | 3543 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 3544 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 3545 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 3546 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 3547 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 3548 | ` *  a function or constant is not supported.` |
|       - | 3549 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 3550 | ` * NOTE` |
|       - | 3551 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3552 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3553 | ` */` |
|      22 | 3554 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       1 | 3555 |  |
|       - | 3556 | `	sxu32 nLine;` |
|       - | 3557 | `	sxi32 rc;` |
|       - | 3558 | `	SyBlob sPath;` |
|       - | 3559 | `	SyString sAlias;` |
|       - | 3560 | `	SyToken *pLast;` |
|       - | 3561 | `	char *zDup;` |
|      23 | 3562 | `	nLine = pGen->pIn->nLine;` |
|      23 | 3563 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|      23 | 3564 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 3565 | `	/* Process one or more use declarations separated by commas */` |
|      12 | 3566 | `	for(;;){` |
|      25 | 3567 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3568 | `			break;` |
|       - | 3569 | `		}` |
|      25 | 3570 | `		SyBlobReset(&sPath);` |
|      25 | 3571 | `		pLast = 0;` |
|       - | 3572 | `		/* Collect the full namespace path */` |
|     101 | 3573 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      77 | 3574 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      49 | 3575 | `				pLast = pGen->pIn;` |
|      49 | 3576 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      29 | 3577 | `					SyBlobAppend(&sPath,"\\",1);` |
|      14 | 3578 | `				}` |
|      49 | 3579 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      24 | 3580 | `			}` |
|      77 | 3581 | `			pGen->pIn++;` |
|       1 | 3582 | `		}` |
|      25 | 3583 | `		if( pLast == 0 ){` |
|       - | 3584 | `			/* Empty path */` |
|       5 | 3585 | `			break;` |
|       - | 3586 | `		}` |
|       - | 3587 | `		/* Default alias is the last component of the path */` |
|      21 | 3588 | `		sAlias = pLast->sData;` |
|       - | 3589 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      20 | 3590 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      13 | 3591 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       5 | 3592 | `			pGen->pIn++; /* Jump 'as' */` |
|       5 | 3593 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       5 | 3594 | `				sAlias = pGen->pIn->sData;` |
|       5 | 3595 | `				pGen->pIn++;` |
|       2 | 3596 | `			}` |
|       2 | 3597 | `		}` |
|       - | 3598 | `		/* Register the import: alias -> FQN.` |
|       - | 3599 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - | 3600 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - | 3601 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      31 | 3602 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      20 | 3603 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      21 | 3604 | `		if( zDup ){` |
|       - | 3605 | `			char *zAliasDup;` |
|      21 | 3606 | `			SyHashInsert(&pGen->hUseImports,sAlias.zString,sAlias.nByte,zDup);` |
|       - | 3607 | `			/* Duplicate the alias key for the VM hash (token pointers may not survive to runtime) */` |
|      21 | 3608 | `			zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      21 | 3609 | `			if( zAliasDup ){` |
|      21 | 3610 | `				SyHashInsert(&pGen->pVm->hUseImports,zAliasDup,sAlias.nByte,zDup);` |
|      10 | 3611 | `			}` |
|      10 | 3612 | `		}` |
|       - | 3613 | `		/* Check for comma (multiple use declarations) */` |
|      21 | 3614 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3615 | `			pGen->pIn++;` |
|       2 | 3616 | `		}else{` |
|      10 | 3617 | `			break;` |
|       - | 3618 | `		}` |
|       1 | 3619 | `	}` |
|      23 | 3620 | `	SyBlobRelease(&sPath);` |
|      23 | 3621 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 3622 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 3623 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 3624 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3625 | `			return SXERR_ABORT;` |
|       - | 3626 | `		}` |
|       1 | 3627 | `	}` |
|      23 | 3628 | `	return SXRET_OK;` |
|      12 | 3629 |  |
|       - | 3630 | `/*` |
|       - | 3631 | ` * Compile the stupid 'declare' language construct.` |
|       - | 3632 | ` *` |
|       - | 3633 | ` * According to the PHP language reference manual.` |
|       - | 3634 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 3635 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 3636 | ` *  declare (directive)` |
|       - | 3637 | ` *   statement` |
|       - | 3638 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 3639 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 3640 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 3641 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 3642 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 3643 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 3644 | ` * <?php` |
|       - | 3645 | ` * // these are the same:` |
|       - | 3646 | ` * // you can use this:` |
|       - | 3647 | ` * declare(ticks=1) {` |
|       - | 3648 | ` *   // entire script here` |
|       - | 3649 | ` * }` |
|       - | 3650 | ` * // or you can use this:` |
|       - | 3651 | ` * declare(ticks=1);` |
|       - | 3652 | ` * // entire script here` |
|       - | 3653 | ` * ?>` |
|       - | 3654 | ` *` |
|       - | 3655 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 3656 | ` */` |
|       8 | 3657 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 3658 |  |
|       9 | 3659 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 3660 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 3661 | `	sxi32 rc;` |
|       9 | 3662 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 3663 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 3664 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 3665 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3666 | `			return SXERR_ABORT;` |
|       - | 3667 | `		}` |
|       5 | 3668 | `		goto Synchro;` |
|       - | 3669 | `	}` |
|       5 | 3670 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 3671 | `	/* Delimit the directive */` |
|       5 | 3672 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 3673 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 3674 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 3675 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3676 | `			return SXERR_ABORT;` |
|       - | 3677 | `		}` |
|     ! 0 | 3678 | `		return SXRET_OK;` |
|       - | 3679 | `	}` |
|       - | 3680 | `	/* Update the cursor */` |
|       5 | 3681 | `	pGen->pIn = &pEnd[1];` |
|       5 | 3682 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 3683 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 3684 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3685 | `			return SXERR_ABORT;` |
|       - | 3686 | `		}` |
|     ! 0 | 3687 | `	}` |
|       - | 3688 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 3689 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 3690 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 3691 | `		ph7_lib_version()` |
|       - | 3692 | `		);` |
|       - | 3693 | `	/*All done */` |
|       5 | 3694 | `	return SXRET_OK;` |
|       2 | 3695 | `Synchro:` |
|       - | 3696 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 3697 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 3698 | `		pGen->pIn++;` |
|       1 | 3699 | `	}` |
|       5 | 3700 | `	return SXRET_OK;` |
|       5 | 3701 |  |
|       - | 3702 | `/*` |
|       - | 3703 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 3704 | ` * as follows:` |
|       - | 3705 | ` * function makecoffee($type = "cappuccino")` |
|       - | 3706 | ` * {` |
|       - | 3707 | ` *   return "Making a cup of $type.\n";` |
|       - | 3708 | ` * }` |
|       - | 3709 | ` * Symisc eXtension.` |
|       - | 3710 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 3711 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 3712 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 3713 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 3714 | ` *      {` |
|       - | 3715 | ` *       var_dump($a);` |
|       - | 3716 | ` *      }` |
|       - | 3717 | ` *     //call test without args` |
|       - | 3718 | ` *      test();` |
|       - | 3719 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 3720 | ` *      Example:` |
|       - | 3721 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 3722 | ` * 3 -) Function overloading!!` |
|       - | 3723 | ` *      Example:` |
|       - | 3724 | ` *      function foo($a) {` |
|       - | 3725 | ` *   	  return $a.PHP_EOL;` |
|       - | 3726 | ` *	    }` |
|       - | 3727 | ` *	    function foo($a, $b) {` |
|       - | 3728 | ` *   	  return $a + $b;` |
|       - | 3729 | ` *	    }` |
|       - | 3730 | ` *	    echo foo(5); // Prints "5"` |
|       - | 3731 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 3732 | ` *      // Same arg` |
|       - | 3733 | ` *	   function foo(string $a)` |
|       - | 3734 | ` *	   {` |
|       - | 3735 | ` *	     echo "a is a string\n";` |
|       - | 3736 | ` *	     var_dump($a);` |
|       - | 3737 | ` *	   }` |
|       - | 3738 | ` *	  function foo(int $a)` |
|       - | 3739 | ` *	  {` |
|       - | 3740 | ` *	    echo "a is integer\n";` |
|       - | 3741 | ` *	    var_dump($a);` |
|       - | 3742 | ` *	  }` |
|       - | 3743 | ` *	  function foo(array $a)` |
|       - | 3744 | ` *	  {` |
|       - | 3745 | ` * 	    echo "a is an array\n";` |
|       - | 3746 | ` * 	    var_dump($a);` |
|       - | 3747 | ` *	  }` |
|       - | 3748 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 3749 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 3750 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 3751 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 3752 | ` * introduced by the PH7 engine.` |
|       - | 3753 | ` */` |
|   29960 | 3754 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 3755 |  |
|       - | 3756 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 3757 | `	SySet *pInstrContainer;` |
|       - | 3758 | `	sxi32 rc;` |
|       - | 3759 | `	/* Swap token stream */` |
|   29962 | 3760 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   29962 | 3761 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   29962 | 3762 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 3763 | `	/* Compile the expression holding the argument value */` |
|   29962 | 3764 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3765 | `	/* Emit the done instruction */` |
|   29962 | 3766 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   29962 | 3767 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   29962 | 3768 | `	RE_SWAP_DELIMITER(pGen);` |
|   29962 | 3769 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3770 | `		return SXERR_ABORT;` |
|       - | 3771 | `	}` |
|   29962 | 3772 | `	return SXRET_OK;` |
|   14982 | 3773 |  |
|       - | 3774 | `/*` |
|       - | 3775 | ` * Collect function arguments one after one.` |
|       - | 3776 | ` * According to the PHP language reference manual.` |
|       - | 3777 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 3778 | ` * list of expressions.` |
|       - | 3779 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 3780 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 3781 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 3782 | ` * for more information.` |
|       - | 3783 | ` * Example #1 Passing arrays to functions` |
|       - | 3784 | ` * <?php` |
|       - | 3785 | ` * function takes_array($input)` |
|       - | 3786 | ` * {` |
|       - | 3787 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 3788 | ` * }` |
|       - | 3789 | ` * ?>` |
|       - | 3790 | ` * Making arguments be passed by reference` |
|       - | 3791 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 3792 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 3793 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 3794 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 3795 | ` * to the argument name in the function definition:` |
|       - | 3796 | ` * Example #2 Passing function parameters by reference` |
|       - | 3797 | ` * <?php` |
|       - | 3798 | ` * function add_some_extra(&$string)` |
|       - | 3799 | ` * {` |
|       - | 3800 | ` *   $string .= 'and something extra.';` |
|       - | 3801 | ` * }` |
|       - | 3802 | ` * $str = 'This is a string, ';` |
|       - | 3803 | ` * add_some_extra($str);` |
|       - | 3804 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 3805 | ` * ?>` |
|       - | 3806 | ` *` |
|       - | 3807 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 3808 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 3809 | ` * on these extension.` |
|       - | 3810 | ` */` |
|   32696 | 3811 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 3812 |  |
|       - | 3813 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 3814 | `	SyToken *pIn;  /* Token stream */` |
|       - | 3815 | `	SyBlob sSig;         /* Function signature */` |
|       - | 3816 | `	char *zDup;          /* Copy of argument name */` |
|       - | 3817 | `	sxi32 rc;` |
|       - | 3818 |  |
|   32698 | 3819 | `	pIn = pGen->pIn;` |
|   32698 | 3820 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 3821 | `	/* Process arguments one after one */` |
|   44447 | 3822 | `	for(;;){` |
|   88896 | 3823 | `		if( pIn >= pEnd ){` |
|       - | 3824 | `			/* No more arguments to process */` |
|   32696 | 3825 | `			break;` |
|       - | 3826 | `		}` |
|   56202 | 3827 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   56202 | 3828 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   56202 | 3829 | `		if( pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   46084 | 3830 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   41476 | 3831 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   41476 | 3832 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 3833 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   41476 | 3834 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 3835 | `					sArg.nType = MEMOBJ_BOOL;` |
|   41476 | 3836 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   11522 | 3837 | `					sArg.nType = MEMOBJ_INT;` |
|   35716 | 3838 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   29954 | 3839 | `					sArg.nType = MEMOBJ_STRING;` |
|   14979 | 3840 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 3841 | `					sArg.nType = MEMOBJ_REAL;` |
|     ! 0 | 3842 | `				}else{` |
|       4 | 3843 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 3844 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 3845 | `						&pIn->sData);` |
|       - | 3846 | `				}` |
|   20739 | 3847 | `			}else{` |
|    4610 | 3848 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 3849 | `				char *zDupLocal;` |
|       - | 3850 | `				/* Argument must be a class instance,record that*/` |
|    4610 | 3851 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    4610 | 3852 | `				if( zDupLocal ){` |
|    4610 | 3853 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    4610 | 3854 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2304 | 3855 | `				}` |
|       - | 3856 | `			}` |
|   46084 | 3857 | `			pIn++;` |
|   23041 | 3858 | `		}` |
|   56202 | 3859 | `		if( pIn >= pEnd ){` |
|     ! 0 | 3860 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 3861 | `			return rc;` |
|       - | 3862 | `		}` |
|   56202 | 3863 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 3864 | `			/* Pass by reference,record that */` |
|    2326 | 3865 | `			sArg.iFlags = VM_FUNC_ARG_BY_REF;` |
|    2326 | 3866 | `			pIn++;` |
|    1162 | 3867 | `		}` |
|   56202 | 3868 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 3869 | `			/* Invalid argument */` |
|     ! 0 | 3870 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 3871 | `			return rc;` |
|       - | 3872 | `		}` |
|   56202 | 3873 | `		pIn++; /* Jump the dollar sign */` |
|       - | 3874 | `		/* Copy argument name */` |
|   56202 | 3875 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   56202 | 3876 | `		if( zDup == 0 ){` |
|     ! 0 | 3877 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 3878 | `			return SXERR_ABORT;` |
|       - | 3879 | `		}` |
|   56202 | 3880 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   56202 | 3881 | `		pIn++;` |
|   56202 | 3882 | `		if( pIn < pEnd ){` |
|   35038 | 3883 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 3884 | `				SyToken *pDefend;` |
|   29964 | 3885 | `				sxi32 iNest = 0;` |
|   29964 | 3886 | `				pIn++; /* Jump the equal sign */` |
|   29964 | 3887 | `				pDefend = pIn;` |
|       - | 3888 | `				/* Process the default value associated with this argument */` |
|   64532 | 3889 | `				while( pDefend < pEnd ){` |
|   53002 | 3890 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   18434 | 3891 | `						break;` |
|       - | 3892 | `					}` |
|   34570 | 3893 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 3894 | `						/* Increment nesting level */` |
|    2306 | 3895 | `						iNest++;` |
|   33418 | 3896 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 3897 | `						/* Decrement nesting level */` |
|    2306 | 3898 | `						iNest--;` |
|    1152 | 3899 | `					}` |
|   34570 | 3900 | `					pDefend++;` |
|       2 | 3901 | `				}` |
|   29964 | 3902 | `				if( pIn >= pDefend ){` |
|       3 | 3903 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 3904 | `					return rc;` |
|       - | 3905 | `				}` |
|       - | 3906 | `				/* Process default value */` |
|   29962 | 3907 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   29962 | 3908 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 3909 | `					return rc;` |
|       - | 3910 | `				}` |
|       - | 3911 | `				/* Point beyond the default value */` |
|   29962 | 3912 | `				pIn = pDefend;` |
|   14980 | 3913 | `			}` |
|   35036 | 3914 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 3915 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 3916 | `				return rc;` |
|       - | 3917 | `			}` |
|   35036 | 3918 | `			pIn++; /* Jump the trailing comma */` |
|   17517 | 3919 | `		}` |
|       - | 3920 | `		/* Append argument signature */` |
|   56200 | 3921 | `		if( sArg.nType > 0 ){` |
|   46082 | 3922 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 3923 | `				/* Class name */` |
|    4610 | 3924 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2306 | 3925 | `			}else{` |
|       - | 3926 | `				int c;` |
|   41474 | 3927 | `				c = 'n'; /* cc warning */` |
|       - | 3928 | `				/* Type leading character */` |
|   41474 | 3929 | `				switch(sArg.nType){` |
|     ! 0 | 3930 | `				case MEMOBJ_HASHMAP:` |
|       - | 3931 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 3932 | `					c = 'h';` |
|     ! 0 | 3933 | `					break;` |
|    5760 | 3934 | `				case MEMOBJ_INT:` |
|       - | 3935 | `					/* Integer */` |
|   11522 | 3936 | `					c = 'i';` |
|   11522 | 3937 | `					break;` |
|     ! 0 | 3938 | `				case MEMOBJ_BOOL:` |
|       - | 3939 | `					/* Bool */` |
|     ! 0 | 3940 | `					c = 'b';` |
|     ! 0 | 3941 | `					break;` |
|     ! 0 | 3942 | `				case MEMOBJ_REAL:` |
|       - | 3943 | `					/* Float */` |
|     ! 0 | 3944 | `					c = 'f';` |
|     ! 0 | 3945 | `					break;` |
|   14976 | 3946 | `				case MEMOBJ_STRING:` |
|       - | 3947 | `					/* String */` |
|   29954 | 3948 | `					c = 's';` |
|   29952 | 3949 | `					break;` |
|     ! 0 | 3950 | `				default:` |
|     ! 0 | 3951 | `					break;` |
|       - | 3952 | `				}` |
|   41474 | 3953 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 3954 | `			}` |
|   23042 | 3955 | `		}else{` |
|       - | 3956 | `			/* No type is associated with this parameter which mean` |
|       - | 3957 | `			 * that this function is not condidate for overloading.` |
|       - | 3958 | `			 */` |
|   10120 | 3959 | `			SyBlobRelease(&sSig);` |
|       - | 3960 | `		}` |
|       - | 3961 | `		/* Save in the argument set */` |
|   56200 | 3962 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 3963 | `	}` |
|   32696 | 3964 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 3965 | `		/* Save function signature */` |
|   27650 | 3966 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   13824 | 3967 | `	}` |
|   32696 | 3968 | `	return SXRET_OK;` |
|   16350 | 3969 |  |
|       - | 3970 | `/*` |
|       - | 3971 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 3972 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 3973 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 3974 | ` */` |
|   79046 | 3975 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 3976 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 3977 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 3978 | `	)` |
|       2 | 3979 |  |
|       - | 3980 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 3981 | `	GenBlock *pBlock;` |
|       - | 3982 | `	sxu32 nGotoOfft;` |
|       - | 3983 | `	sxi32 rc;` |
|       - | 3984 | `	/* Attach the new function */` |
|   79048 | 3985 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   79048 | 3986 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3987 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 3988 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3989 | `		return SXERR_ABORT;` |
|       - | 3990 | `	}` |
|   79048 | 3991 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 3992 | `	/* Swap bytecode containers */` |
|   79048 | 3993 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   79048 | 3994 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 3995 | `	/* Compile the body */` |
|   79048 | 3996 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 3997 | `	/* Fix exception jumps now the destination is resolved */` |
|   79048 | 3998 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3999 | `	/* Emit the final return if not yet done */` |
|   79048 | 4000 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4001 | `	/* Fix gotos jumps now the destination is resolved */` |
|   79048 | 4002 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 4003 | `		rc = SXERR_ABORT;` |
|     ! 0 | 4004 | `	}` |
|   79048 | 4005 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 4006 | `	/* Restore the default container */` |
|   79048 | 4007 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4008 | `	/* Leave function block */` |
|   79048 | 4009 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   79048 | 4010 | `	if( rc == SXERR_ABORT ){` |
|       - | 4011 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4012 | `		return SXERR_ABORT;` |
|       - | 4013 | `	}` |
|       - | 4014 | `	/* All done, function body compiled */` |
|   79048 | 4015 | `	return SXRET_OK;` |
|   39525 | 4016 |  |
|       - | 4017 | `/*` |
|       - | 4018 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 4019 | ` * According to the PHP language reference manual.` |
|       - | 4020 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 4021 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 4022 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 4023 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4024 | ` *  Functions need not be defined before they are referenced.` |
|       - | 4025 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 4026 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 4027 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 4028 | ` *  calls with over 32-64 recursion levels.` |
|       - | 4029 | ` *` |
|       - | 4030 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 4031 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 4032 | ` * on these extension.` |
|       - | 4033 | ` */` |
|   30446 | 4034 | `static sxi32 GenStateCompileFunc(` |
|       - | 4035 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4036 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 4037 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 4038 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 4039 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 4040 | `	)` |
|       2 | 4041 |  |
|       - | 4042 | `	ph7_vm_func *pFunc;` |
|       - | 4043 | `	SyToken *pEnd;` |
|       - | 4044 | `	sxu32 nLine;` |
|       - | 4045 | `	char *zName;` |
|       - | 4046 | `	sxi32 rc;` |
|       - | 4047 | `	/* Extract line number */` |
|   30448 | 4048 | `	nLine = pGen->pIn->nLine;` |
|       - | 4049 | `	/* Jump the left parenthesis '(' */` |
|   30448 | 4050 | `	pGen->pIn++;` |
|       - | 4051 | `	/* Delimit the function signature */` |
|   30448 | 4052 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   30448 | 4053 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4054 | `		/* Syntax error */` |
|       7 | 4055 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 4056 | `		if( rc == SXERR_ABORT ){` |
|       - | 4057 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4058 | `			return SXERR_ABORT;` |
|       - | 4059 | `		}` |
|       7 | 4060 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 4061 | `		return SXRET_OK;` |
|       - | 4062 | `	}` |
|       - | 4063 | `	/* Create the function state */` |
|   30442 | 4064 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   30442 | 4065 | `	if( pFunc == 0 ){` |
|     ! 0 | 4066 | `		goto OutOfMem;` |
|       - | 4067 | `	}` |
|       - | 4068 | `	/* Build the function name, prepending namespace if active */` |
|   30446 | 4069 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 4070 | `		SyBlob sFQN;` |
|       - | 4071 | `		sxu32 nLen;` |
|       9 | 4072 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       9 | 4073 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       9 | 4074 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       9 | 4075 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       9 | 4076 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       9 | 4077 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       9 | 4078 | `		SyBlobRelease(&sFQN);` |
|       9 | 4079 | `		if( zName == 0 ){` |
|     ! 0 | 4080 | `			goto OutOfMem;` |
|       - | 4081 | `		}` |
|       9 | 4082 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       5 | 4083 | `	}else{` |
|   30434 | 4084 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   30434 | 4085 | `		if( zName == 0 ){` |
|     ! 0 | 4086 | `			goto OutOfMem;` |
|       - | 4087 | `		}` |
|   30434 | 4088 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 4089 | `	}` |
|   30442 | 4090 | `	if( pGen->pIn < pEnd ){` |
|       - | 4091 | `		/* Collect function arguments */` |
|   21130 | 4092 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   21130 | 4093 | `		if( rc == SXERR_ABORT ){` |
|       - | 4094 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4095 | `			return SXERR_ABORT;` |
|       - | 4096 | `		}` |
|   10564 | 4097 | `	}` |
|       - | 4098 | `	/* Compile function body */` |
|   30442 | 4099 | `	pGen->pIn = &pEnd[1];` |
|   30442 | 4100 | `	if( bHandleClosure ){` |
|       - | 4101 | `		ph7_vm_func_closure_env sEnv;` |
|     130 | 4102 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     128 | 4103 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      70 | 4104 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      10 | 4105 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 4106 | `				/* Closure,record environment variable */` |
|      10 | 4107 | `				pGen->pIn++;` |
|      10 | 4108 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 4109 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 4110 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4111 | `						return SXERR_ABORT;` |
|       - | 4112 | `					}` |
|     ! 0 | 4113 | `				}` |
|      10 | 4114 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 4115 | `				/* Compile until we hit the first closing parenthesis */` |
|      18 | 4116 | `				while( pGen->pIn < pGen->pEnd ){` |
|      18 | 4117 | `					int iFlagsLocal = 0;` |
|      18 | 4118 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      10 | 4119 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      10 | 4120 | `						break;` |
|       - | 4121 | `					}` |
|      10 | 4122 | `					nLineLocal = pGen->pIn->nLine;` |
|      10 | 4123 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 4124 | `						/* Pass by reference,record that */` |
|     ! 0 | 4125 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 4126 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 4127 | `							);` |
|     ! 0 | 4128 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 4129 | `						pGen->pIn++;` |
|     ! 0 | 4130 | `					}` |
|       8 | 4131 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      10 | 4132 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 4133 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 4134 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 4135 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4136 | `								return SXERR_ABORT;` |
|       - | 4137 | `							}` |
|       - | 4138 | `							/* Find the closing parenthesis */` |
|     ! 0 | 4139 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 4140 | `								pGen->pIn++;` |
|     ! 0 | 4141 | `							}` |
|     ! 0 | 4142 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 4143 | `								pGen->pIn++;` |
|     ! 0 | 4144 | `							}` |
|     ! 0 | 4145 | `							break;` |
|       - | 4146 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 4147 | `					}else{` |
|       - | 4148 | `						SyString *pNameLocal;` |
|       - | 4149 | `						char *zDup;` |
|       - | 4150 | `						/* Duplicate variable name */` |
|      10 | 4151 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      10 | 4152 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      10 | 4153 | `						if( zDup ){` |
|       - | 4154 | `							/* Zero the structure */` |
|      10 | 4155 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      10 | 4156 | `							sEnv.iFlags = iFlagsLocal;` |
|      10 | 4157 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      10 | 4158 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      10 | 4159 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 4160 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 4161 | `									got_this = 1;` |
|     ! 0 | 4162 | `							}` |
|       - | 4163 | `							/* Save imported variable */` |
|      10 | 4164 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       6 | 4165 | `						}else{` |
|     ! 0 | 4166 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4167 | `							 return SXERR_ABORT;` |
|       - | 4168 | `						}` |
|       - | 4169 | `					}` |
|      10 | 4170 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      10 | 4171 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4172 | `						/* Ignore trailing commas */` |
|     ! 0 | 4173 | `						pGen->pIn++;` |
|     ! 0 | 4174 | `					}` |
|       2 | 4175 | `				}` |
|      10 | 4176 | `				if( !got_this ){` |
|       - | 4177 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 4178 | `					 * available to the closure environment.` |
|       - | 4179 | `					 */` |
|      10 | 4180 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      10 | 4181 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      10 | 4182 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      10 | 4183 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      10 | 4184 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       4 | 4185 | `				}` |
|      10 | 4186 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 4187 | `					/* Mark as closure */` |
|      10 | 4188 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       4 | 4189 | `				}` |
|       4 | 4190 | `		}` |
|      64 | 4191 | `	}` |
|       - | 4192 | `	/* Compile the body */` |
|   30442 | 4193 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   30442 | 4194 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4195 | `		return SXERR_ABORT;` |
|       - | 4196 | `	}` |
|   30442 | 4197 | `	if( ppFunc ){` |
|     130 | 4198 | `		*ppFunc = pFunc;` |
|      64 | 4199 | `	}` |
|   30442 | 4200 | `	rc = SXRET_OK;` |
|   30442 | 4201 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 4202 | `		/* Finally register the function */` |
|   30434 | 4203 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   15216 | 4204 | `	}` |
|   30442 | 4205 | `	if( rc == SXRET_OK ){` |
|   30442 | 4206 | `		return SXRET_OK;` |
|       - | 4207 | `	}` |
|       - | 4208 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 4209 | `OutOfMem:` |
|       - | 4210 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 4211 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 4212 | `	 */` |
|     ! 0 | 4213 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 4214 | `	return SXERR_ABORT;` |
|   15225 | 4215 |  |
|       - | 4216 | `/*` |
|       - | 4217 | ` * Compile a standard PHP function.` |
|       - | 4218 | ` *  Refer to the block-comment above for more information.` |
|       - | 4219 | ` */` |
|   30324 | 4220 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 4221 |  |
|       - | 4222 | `	SyString *pName;` |
|       - | 4223 | `	sxi32 iFlags;` |
|       - | 4224 | `	sxu32 nLine;` |
|       - | 4225 | `	sxi32 rc;` |
|       - | 4226 |  |
|   30326 | 4227 | `	nLine = pGen->pIn->nLine;` |
|   30326 | 4228 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   30326 | 4229 | `	iFlags = 0;` |
|   30326 | 4230 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4231 | `		/* Return by reference,remember that */` |
|       7 | 4232 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4233 | `		/* Jump the '&' token */` |
|       7 | 4234 | `		pGen->pIn++;` |
|       3 | 4235 | `	}` |
|   30326 | 4236 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4237 | `		/* Invalid function name */` |
|       5 | 4238 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 4239 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4240 | `			return SXERR_ABORT;` |
|       - | 4241 | `		}` |
|       - | 4242 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 4243 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 4244 | `			pGen->pIn++;` |
|       1 | 4245 | `		}` |
|       5 | 4246 | `		return SXRET_OK;` |
|       - | 4247 | `	}` |
|   30322 | 4248 | `	pName = &pGen->pIn->sData;` |
|   30322 | 4249 | `	nLine = pGen->pIn->nLine;` |
|       - | 4250 | `	/* Jump the function name */` |
|   30322 | 4251 | `	pGen->pIn++;` |
|   30322 | 4252 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4253 | `		/* Syntax error */` |
|       3 | 4254 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 4255 | `		if( rc == SXERR_ABORT ){` |
|       - | 4256 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4257 | `			return SXERR_ABORT;` |
|       - | 4258 | `		}` |
|       - | 4259 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 4260 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4261 | `			pGen->pIn++;` |
|     ! 0 | 4262 | `		}` |
|       3 | 4263 | `		return SXRET_OK;` |
|       - | 4264 | `	}` |
|       - | 4265 | `	/* Compile function body */` |
|   30320 | 4266 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   30320 | 4267 | `	return rc;` |
|   15164 | 4268 |  |
|       - | 4269 | `/*` |
|       - | 4270 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 4271 | ` * According to the PHP language reference manual` |
|       - | 4272 | ` *  Visibility:` |
|       - | 4273 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 4274 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 4275 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 4276 | ` *  Members declared protected can be accessed only within the class` |
|       - | 4277 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 4278 | ` *  may only be accessed by the class that defines the member.` |
|       - | 4279 | ` */` |
|   90278 | 4280 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 4281 |  |
|   90280 | 4282 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|      40 | 4283 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   90242 | 4284 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   16156 | 4285 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 4286 | `	}` |
|       - | 4287 | `	/* Assume public by default */` |
|   74088 | 4288 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   45141 | 4289 |  |
|       - | 4290 | `/*` |
|       - | 4291 | ` * Compile a class constant.` |
|       - | 4292 | ` * According to the PHP language reference manual` |
|       - | 4293 | ` *  Class Constants` |
|       - | 4294 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 4295 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 4296 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 4297 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 4298 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 4299 | ` *   It's also possible for interfaces to have constants.` |
|       - | 4300 | ` * Symisc eXtension.` |
|       - | 4301 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 4302 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4303 | ` *  Example:` |
|       - | 4304 | ` *   class Test{` |
|       - | 4305 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4306 | ` *   };` |
|       - | 4307 | ` *   var_dump(TEST::MyConst);` |
|       - | 4308 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4309 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4310 | ` */` |
|      10 | 4311 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4312 |  |
|      12 | 4313 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4314 | `	SySet *pInstrContainer;` |
|       - | 4315 | `	ph7_class_attr *pCons;` |
|       - | 4316 | `	SyString *pName;` |
|       - | 4317 | `	sxi32 rc;` |
|       - | 4318 | `	/* Extract visibility level */` |
|      12 | 4319 | `	iProtection = GetProtectionLevel(iProtection);` |
|      12 | 4320 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       5 | 4321 | `loop:` |
|       - | 4322 | `	/* Mark as constant */` |
|      12 | 4323 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      12 | 4324 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4325 | `		/* Invalid constant name */` |
|     ! 0 | 4326 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 4327 | `		if( rc == SXERR_ABORT ){` |
|       - | 4328 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4329 | `			return SXERR_ABORT;` |
|       - | 4330 | `		}` |
|     ! 0 | 4331 | `		goto Synchronize;` |
|       - | 4332 | `	}` |
|       - | 4333 | `	/* Peek constant name */` |
|      12 | 4334 | `	pName = &pGen->pIn->sData;` |
|       - | 4335 | `	/* Make sure the constant name isn't reserved */` |
|      12 | 4336 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 4337 | `		/* Reserved constant name */` |
|     ! 0 | 4338 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 4339 | `		if( rc == SXERR_ABORT ){` |
|       - | 4340 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4341 | `			return SXERR_ABORT;` |
|       - | 4342 | `		}` |
|     ! 0 | 4343 | `		goto Synchronize;` |
|       - | 4344 | `	}` |
|       - | 4345 | `	/* Advance the stream cursor */` |
|      12 | 4346 | `	pGen->pIn++;` |
|      12 | 4347 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 4348 | `		/* Invalid declaration */` |
|     ! 0 | 4349 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 4350 | `		if( rc == SXERR_ABORT ){` |
|       - | 4351 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4352 | `			return SXERR_ABORT;` |
|       - | 4353 | `		}` |
|     ! 0 | 4354 | `		goto Synchronize;` |
|       - | 4355 | `	}` |
|      12 | 4356 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 4357 | `	/* Allocate a new class attribute */` |
|      12 | 4358 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      12 | 4359 | `	if( pCons == 0 ){` |
|     ! 0 | 4360 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4361 | `		return SXERR_ABORT;` |
|       - | 4362 | `	}` |
|       - | 4363 | `	/* Swap bytecode container */` |
|      12 | 4364 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      12 | 4365 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 4366 | `	/* Compile constant value.` |
|       - | 4367 | `	 */` |
|      12 | 4368 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      12 | 4369 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 4370 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 4371 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4372 | `			return SXERR_ABORT;` |
|       - | 4373 | `		}` |
|       1 | 4374 | `	}` |
|       - | 4375 | `	/* Emit the done instruction */` |
|      12 | 4376 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      12 | 4377 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      12 | 4378 | `	if( rc == SXERR_ABORT ){` |
|       - | 4379 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4380 | `		return SXERR_ABORT;` |
|       - | 4381 | `	}` |
|       - | 4382 | `	/* All done,install the constant */` |
|      12 | 4383 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      12 | 4384 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4385 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4386 | `		return SXERR_ABORT;` |
|       - | 4387 | `	}` |
|      12 | 4388 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4389 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 4390 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4391 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 4392 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4393 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4394 | `				pTok--;` |
|     ! 0 | 4395 | `			}` |
|     ! 0 | 4396 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4397 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 4398 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4399 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4400 | `				return SXERR_ABORT;` |
|       - | 4401 | `			}` |
|     ! 0 | 4402 | `		}else{` |
|     ! 0 | 4403 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 4404 | `				goto loop;` |
|       - | 4405 | `			}` |
|       - | 4406 | `		}` |
|     ! 0 | 4407 | `	}` |
|      12 | 4408 | `	return SXRET_OK;` |
|     ! 0 | 4409 | `Synchronize:` |
|       - | 4410 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4411 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 4412 | `		pGen->pIn++;` |
|     ! 0 | 4413 | `	}` |
|     ! 0 | 4414 | `	return SXERR_CORRUPT;` |
|       7 | 4415 |  |
|       - | 4416 | `/*` |
|       - | 4417 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 4418 | ` * According to the PHP language reference manual` |
|       - | 4419 | ` *  Properties` |
|       - | 4420 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 4421 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 4422 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 4423 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 4424 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 4425 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 4426 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 4427 | ` * Symisc eXtension.` |
|       - | 4428 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 4429 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4430 | ` *  Example:` |
|       - | 4431 | ` *   class Test{` |
|       - | 4432 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4433 | ` *   };` |
|       - | 4434 | ` *   var_dump(TEST::myVar);` |
|       - | 4435 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4436 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4437 | ` */` |
|   23214 | 4438 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4439 |  |
|   23216 | 4440 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4441 | `	ph7_class_attr *pAttr;` |
|       - | 4442 | `	SyString *pName;` |
|       - | 4443 | `	sxi32 rc;` |
|       - | 4444 | `	/* Extract visibility level */` |
|   23216 | 4445 | `	iProtection = GetProtectionLevel(iProtection);` |
|   11607 | 4446 | `loop:` |
|   23216 | 4447 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   23216 | 4448 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 4449 | `		/* Invalid attribute name */` |
|     ! 0 | 4450 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 4451 | `		if( rc == SXERR_ABORT ){` |
|       - | 4452 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4453 | `			return SXERR_ABORT;` |
|       - | 4454 | `		}` |
|     ! 0 | 4455 | `		goto Synchronize;` |
|       - | 4456 | `	}` |
|       - | 4457 | `	/* Peek attribute name */` |
|   23216 | 4458 | `	pName = &pGen->pIn->sData;` |
|       - | 4459 | `	/* Advance the stream cursor */` |
|   23216 | 4460 | `	pGen->pIn++;` |
|   23216 | 4461 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 4462 | `		/* Invalid declaration */` |
|       3 | 4463 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 4464 | `		if( rc == SXERR_ABORT ){` |
|       - | 4465 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4466 | `			return SXERR_ABORT;` |
|       - | 4467 | `		}` |
|       3 | 4468 | `		goto Synchronize;` |
|       - | 4469 | `	}` |
|       - | 4470 | `	/* Allocate a new class attribute */` |
|   23214 | 4471 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   23214 | 4472 | `	if( pAttr == 0 ){` |
|     ! 0 | 4473 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 4474 | `		return SXERR_ABORT;` |
|       - | 4475 | `	}` |
|   23214 | 4476 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 4477 | `		SySet *pInstrContainer;` |
|    9354 | 4478 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 4479 | `		/* Swap bytecode container */` |
|    9354 | 4480 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    9354 | 4481 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 4482 | `		/* Compile attribute value.` |
|       - | 4483 | `		 */` |
|    9354 | 4484 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    9354 | 4485 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 4486 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 4487 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4488 | `				return SXERR_ABORT;` |
|       - | 4489 | `			}` |
|     ! 0 | 4490 | `		}` |
|       - | 4491 | `		/* Emit the done instruction */` |
|    9354 | 4492 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    9354 | 4493 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    4676 | 4494 | `	}` |
|       - | 4495 | `	/* All done,install the attribute */` |
|   23214 | 4496 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   23214 | 4497 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4498 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4499 | `		return SXERR_ABORT;` |
|       - | 4500 | `	}` |
|   23214 | 4501 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4502 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 4503 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4504 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 4505 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4506 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4507 | `				pTok--;` |
|     ! 0 | 4508 | `			}` |
|     ! 0 | 4509 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4510 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4511 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4512 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4513 | `				return SXERR_ABORT;` |
|       - | 4514 | `			}` |
|     ! 0 | 4515 | `		}else{` |
|     ! 0 | 4516 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 4517 | `				goto loop;` |
|       - | 4518 | `			}` |
|       - | 4519 | `		}` |
|     ! 0 | 4520 | `	}` |
|   23214 | 4521 | `	return SXRET_OK;` |
|       1 | 4522 | `Synchronize:` |
|       - | 4523 | `	/* Synchronize with the first semi-colon */` |
|       5 | 4524 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 4525 | `		pGen->pIn++;` |
|       1 | 4526 | `	}` |
|       3 | 4527 | `	return SXERR_CORRUPT;` |
|   11609 | 4528 |  |
|       - | 4529 | `/*` |
|       - | 4530 | ` * Compile a class method.` |
|       - | 4531 | ` *` |
|       - | 4532 | ` * Refer to the official documentation for more information` |
|       - | 4533 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 4534 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 4535 | ` * overloading and many more.` |
|       - | 4536 | ` */` |
|   67054 | 4537 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 4538 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4539 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 4540 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 4541 | `	int doBody,          /* TRUE to process method body */` |
|       - | 4542 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 4543 | `	)` |
|       2 | 4544 |  |
|   67056 | 4545 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4546 | `	ph7_class_method *pMeth;` |
|       - | 4547 | `	sxi32 iFuncFlags;` |
|       - | 4548 | `	SyString *pName;` |
|       - | 4549 | `	SyToken *pEnd;` |
|       - | 4550 | `	sxi32 rc;` |
|       - | 4551 | `	/* Extract visibility level */` |
|   67056 | 4552 | `	iProtection = GetProtectionLevel(iProtection);` |
|   67056 | 4553 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   67056 | 4554 | `	iFuncFlags = 0;` |
|   67056 | 4555 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4556 | `		/* Invalid method name */` |
|     ! 0 | 4557 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4558 | `		if( rc == SXERR_ABORT ){` |
|       - | 4559 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4560 | `			return SXERR_ABORT;` |
|       - | 4561 | `		}` |
|     ! 0 | 4562 | `		goto Synchronize;` |
|       - | 4563 | `	}` |
|   67056 | 4564 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4565 | `		/* Return by reference,remember that */` |
|     ! 0 | 4566 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4567 | `		/* Jump the '&' token */` |
|     ! 0 | 4568 | `		pGen->pIn++;` |
|     ! 0 | 4569 | `	}` |
|   67056 | 4570 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID)) == 0 ){` |
|       - | 4571 | `		/* Invalid method name */` |
|     ! 0 | 4572 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4573 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4574 | `			return SXERR_ABORT;` |
|       - | 4575 | `		}` |
|     ! 0 | 4576 | `		goto Synchronize;` |
|       - | 4577 | `	}` |
|       - | 4578 | `	/* Peek method name */` |
|   67056 | 4579 | `	pName = &pGen->pIn->sData;` |
|   67056 | 4580 | `	nLine = pGen->pIn->nLine;` |
|       - | 4581 | `	/* Jump the method name */` |
|   67056 | 4582 | `	pGen->pIn++;` |
|   67056 | 4583 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 4584 | `		/* Abstract method */` |
|       8 | 4585 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 4586 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4587 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 4588 | `				&pClass->sName,pName);` |
|     ! 0 | 4589 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4590 | `				return SXERR_ABORT;` |
|       - | 4591 | `			}` |
|     ! 0 | 4592 | `		}` |
|       - | 4593 | `		/* Assemble method signature only */` |
|       8 | 4594 | `		doBody = FALSE;` |
|       3 | 4595 | `	}` |
|   67056 | 4596 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4597 | `		/* Syntax error */` |
|     ! 0 | 4598 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 4599 | `		if( rc == SXERR_ABORT ){` |
|       - | 4600 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4601 | `			return SXERR_ABORT;` |
|       - | 4602 | `		}` |
|     ! 0 | 4603 | `		goto Synchronize;` |
|       - | 4604 | `	}` |
|       - | 4605 | `	/* Allocate a new class_method instance */` |
|   67056 | 4606 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   67056 | 4607 | `	if( pMeth == 0 ){` |
|     ! 0 | 4608 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4609 | `		return SXERR_ABORT;` |
|       - | 4610 | `	}` |
|       - | 4611 | `	/* Jump the left parenthesis '(' */` |
|   67056 | 4612 | `	pGen->pIn++;` |
|   67056 | 4613 | `	pEnd = 0; /* cc warning */` |
|       - | 4614 | `	/* Delimit the method signature */` |
|   67056 | 4615 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   67056 | 4616 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4617 | `		/* Syntax error */` |
|       3 | 4618 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 4619 | `		if( rc == SXERR_ABORT ){` |
|       - | 4620 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4621 | `			return SXERR_ABORT;` |
|       - | 4622 | `		}` |
|       3 | 4623 | `		goto Synchronize;` |
|       - | 4624 | `	}` |
|   67054 | 4625 | `	if( pGen->pIn < pEnd ){` |
|       - | 4626 | `		/* Collect method arguments */` |
|   11570 | 4627 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   11570 | 4628 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4629 | `			return SXERR_ABORT;` |
|       - | 4630 | `		}` |
|    5784 | 4631 | `	}` |
|       - | 4632 | `	/* Point beyond method signature */` |
|   67054 | 4633 | `	pGen->pIn = &pEnd[1];` |
|   67054 | 4634 | `	if( doBody ){` |
|       - | 4635 | `		/* Compile method body */` |
|   48608 | 4636 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   48608 | 4637 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4638 | `			return SXERR_ABORT;` |
|       - | 4639 | `		}` |
|   24305 | 4640 | `	}else{` |
|       - | 4641 | `		/* Only method signature is allowed */` |
|   18448 | 4642 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 4643 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4644 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 4645 | `				if( rc == SXERR_ABORT ){` |
|       - | 4646 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4647 | `					return SXERR_ABORT;` |
|       - | 4648 | `				}` |
|     ! 0 | 4649 | `				return SXERR_CORRUPT;` |
|       - | 4650 | `			}` |
|       - | 4651 | `	}` |
|       - | 4652 | `	/* All done,install the method */` |
|   67054 | 4653 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   67054 | 4654 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4655 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4656 | `		return SXERR_ABORT;` |
|       - | 4657 | `	}` |
|   67054 | 4658 | `	return SXRET_OK;` |
|       1 | 4659 | `Synchronize:` |
|       - | 4660 | `	/* Synchronize with the first semi-colon */` |
|       7 | 4661 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 4662 | `		pGen->pIn++;` |
|       1 | 4663 | `	}` |
|       3 | 4664 | `	return SXERR_CORRUPT;` |
|   33529 | 4665 |  |
|       - | 4666 | `/*` |
|       - | 4667 | ` * Compile an object interface.` |
|       - | 4668 | ` *  According to the PHP language reference manual` |
|       - | 4669 | ` *   Object Interfaces:` |
|       - | 4670 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 4671 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 4672 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 4673 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 4674 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 4675 | ` */` |
|    6924 | 4676 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 4677 |  |
|    6926 | 4678 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4679 | `	ph7_class *pClass,*pBase;` |
|       - | 4680 | `	SyToken *pEnd,*pTmp;` |
|       - | 4681 | `	SyString *pName;` |
|       - | 4682 | `	sxi32 nKwrd;` |
|       - | 4683 | `	sxi32 rc;` |
|       - | 4684 | `	/* Jump the 'interface' keyword */` |
|    6926 | 4685 | `	pGen->pIn++;` |
|       - | 4686 | `	/* Extract interface name */` |
|    6926 | 4687 | `	pName = &pGen->pIn->sData;` |
|       - | 4688 | `	/* Advance the stream cursor */` |
|    6926 | 4689 | `	pGen->pIn++;` |
|       - | 4690 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 4691 | `		SyBlob sFQN;` |
|       - | 4692 | `		SyString sFQNStr;` |
|    6926 | 4693 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    6926 | 4694 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    6926 | 4695 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    6926 | 4696 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    6926 | 4697 | `		SyBlobRelease(&sFQN);` |
|       - | 4698 | `	}` |
|    6926 | 4699 | `	if( pClass == 0 ){` |
|     ! 0 | 4700 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4701 | `		return SXERR_ABORT;` |
|       - | 4702 | `	}` |
|       - | 4703 | `	/* Mark as an interface */` |
|    6926 | 4704 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 4705 | `	/* Assume no base class is given */` |
|    6926 | 4706 | `	pBase = 0;` |
|    6926 | 4707 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 4708 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 4709 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 4710 | `			SyString *pBaseName;` |
|       - | 4711 | `			/* Extract base interface */` |
|       3 | 4712 | `			pGen->pIn++;` |
|       3 | 4713 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4714 | `				/* Syntax error */` |
|     ! 0 | 4715 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4716 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 4717 | `					pName);` |
|     ! 0 | 4718 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4719 | `				if( rc == SXERR_ABORT ){` |
|       - | 4720 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4721 | `					return SXERR_ABORT;` |
|       - | 4722 | `				}` |
|     ! 0 | 4723 | `				return SXRET_OK;` |
|       - | 4724 | `			}` |
|       3 | 4725 | `			pBaseName = &pGen->pIn->sData;` |
|       3 | 4726 | `			pBase = PH7_VmExtractClass(pGen->pVm,pBaseName->zString,pBaseName->nByte,FALSE,0);` |
|       - | 4727 | `			/* Only interfaces is allowed */` |
|       3 | 4728 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 4729 | `				pBase = pBase->pNextName;` |
|     ! 0 | 4730 | `			}` |
|       3 | 4731 | `			if( pBase == 0 ){` |
|       - | 4732 | `				/* Inexistant interface */` |
|     ! 0 | 4733 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 4734 | `				if( rc == SXERR_ABORT ){` |
|       - | 4735 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4736 | `					return SXERR_ABORT;` |
|       - | 4737 | `				}` |
|     ! 0 | 4738 | `			}` |
|       - | 4739 | `			/* Advance the stream cursor */` |
|       3 | 4740 | `			pGen->pIn++;` |
|       1 | 4741 | `		}` |
|       1 | 4742 | `	}` |
|    6926 | 4743 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 4744 | `		/* Syntax error */` |
|     ! 0 | 4745 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 4746 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4747 | `		if( rc == SXERR_ABORT ){` |
|       - | 4748 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4749 | `			return SXERR_ABORT;` |
|       - | 4750 | `		}` |
|     ! 0 | 4751 | `		return SXRET_OK;` |
|       - | 4752 | `	}` |
|    6926 | 4753 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    6926 | 4754 | `	pEnd = 0; /* cc warning */` |
|       - | 4755 | `	/* Delimit the interface body */` |
|    6926 | 4756 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    6926 | 4757 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4758 | `		/* Syntax error */` |
|     ! 0 | 4759 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 4760 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4761 | `		if( rc == SXERR_ABORT ){` |
|       - | 4762 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4763 | `			return SXERR_ABORT;` |
|       - | 4764 | `		}` |
|     ! 0 | 4765 | `		return SXRET_OK;` |
|       - | 4766 | `	}` |
|       - | 4767 | `	/* Swap token stream */` |
|    6926 | 4768 | `	pTmp = pGen->pEnd;` |
|    6926 | 4769 | `	pGen->pEnd = pEnd;` |
|       - | 4770 | `	/* Start the parse process` |
|       - | 4771 | `	 * Note (According to the PHP reference manual):` |
|       - | 4772 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 4773 | `	 *  Only 'public' visibility is allowed.` |
|       - | 4774 | `	 */` |
|   12683 | 4775 | `	for(;;){` |
|       - | 4776 | `		/* Jump leading/trailing semi-colons */` |
|   43810 | 4777 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   18444 | 4778 | `			pGen->pIn++;` |
|       2 | 4779 | `		}` |
|   25368 | 4780 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4781 | `			/* End of interface body */` |
|    6926 | 4782 | `			break;` |
|       - | 4783 | `		}` |
|   18444 | 4784 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4785 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4786 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 4787 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 4788 | `			if( rc == SXERR_ABORT ){` |
|       - | 4789 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4790 | `				return SXERR_ABORT;` |
|       - | 4791 | `			}` |
|     ! 0 | 4792 | `			goto done;` |
|       - | 4793 | `		}` |
|       - | 4794 | `		/* Extract the current keyword */` |
|   18444 | 4795 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   18444 | 4796 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 4797 | `			/* Emit a warning and switch to public visibility */` |
|     ! 0 | 4798 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"interface: Access type must be public");` |
|     ! 0 | 4799 | `			nKwrd = PH7_TKWRD_PUBLIC;` |
|     ! 0 | 4800 | `		}` |
|   18444 | 4801 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 4802 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4803 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 4804 | `			if( rc == SXERR_ABORT ){` |
|       - | 4805 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4806 | `				return SXERR_ABORT;` |
|       - | 4807 | `			}` |
|     ! 0 | 4808 | `			goto done;` |
|       - | 4809 | `		}` |
|   18444 | 4810 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 4811 | `			/* Advance the stream cursor */` |
|   18440 | 4812 | `			pGen->pIn++;` |
|   18440 | 4813 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4814 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4815 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 4816 | `				if( rc == SXERR_ABORT ){` |
|       - | 4817 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4818 | `					return SXERR_ABORT;` |
|       - | 4819 | `				}` |
|     ! 0 | 4820 | `				goto done;` |
|       - | 4821 | `			}` |
|   18440 | 4822 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   18440 | 4823 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 4824 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4825 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 4826 | `				if( rc == SXERR_ABORT ){` |
|       - | 4827 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4828 | `					return SXERR_ABORT;` |
|       - | 4829 | `				}` |
|     ! 0 | 4830 | `				goto done;` |
|       - | 4831 | `			}` |
|    9219 | 4832 | `		}` |
|   18444 | 4833 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 4834 | `			/* Parse constant */` |
|       3 | 4835 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 4836 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 4837 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4838 | `					return SXERR_ABORT;` |
|       - | 4839 | `				}` |
|     ! 0 | 4840 | `				goto done;` |
|       - | 4841 | `			}` |
|       2 | 4842 | `		}else{` |
|   18442 | 4843 | `			sxi32 iFlags = 0;` |
|   18442 | 4844 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 4845 | `				/* Static method,record that */` |
|     ! 0 | 4846 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 4847 | `				/* Advance the stream cursor */` |
|     ! 0 | 4848 | `				pGen->pIn++;` |
|     ! 0 | 4849 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 4850 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 4851 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4852 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 4853 | `						if( rc == SXERR_ABORT ){` |
|       - | 4854 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 4855 | `							return SXERR_ABORT;` |
|       - | 4856 | `						}` |
|     ! 0 | 4857 | `						goto done;` |
|       - | 4858 | `				}` |
|     ! 0 | 4859 | `			}` |
|       - | 4860 | `			/* Process method signature */` |
|   18442 | 4861 | `			rc = GenStateCompileClassMethod(&(*pGen),0,FALSE/* Only method signature*/,iFlags,pClass);` |
|   18442 | 4862 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 4863 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4864 | `					return SXERR_ABORT;` |
|       - | 4865 | `				}` |
|     ! 0 | 4866 | `				goto done;` |
|       - | 4867 | `			}` |
|       - | 4868 | `		}` |
|       2 | 4869 | `	}` |
|       - | 4870 | `	/* Install the interface */` |
|    6926 | 4871 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    6926 | 4872 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 4873 | `		/* Inherit from the base interface */` |
|       3 | 4874 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 4875 | `	}` |
|    6926 | 4876 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4877 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4878 | `		return SXERR_ABORT;` |
|       - | 4879 | `	}` |
|    3462 | 4880 | `done:` |
|       - | 4881 | `	/* Point beyond the interface body */` |
|    6926 | 4882 | `	pGen->pIn  = &pEnd[1];` |
|    6926 | 4883 | `	pGen->pEnd = pTmp;` |
|    6926 | 4884 | `	return PH7_OK;` |
|    3464 | 4885 |  |
|       - | 4886 | `/*` |
|       - | 4887 | ` * Compile a user-defined class.` |
|       - | 4888 | ` * According to the PHP language reference manual` |
|       - | 4889 | ` *  class` |
|       - | 4890 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 4891 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 4892 | ` *  of the properties and methods belonging to the class.` |
|       - | 4893 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 4894 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 4895 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 4896 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4897 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 4898 | ` *  (called "methods").` |
|       - | 4899 | ` */` |
|   21024 | 4900 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 4901 |  |
|   21026 | 4902 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4903 | `	ph7_class *pClass,*pBase;` |
|       - | 4904 | `	SyToken *pEnd,*pTmp;` |
|       - | 4905 | `	sxi32 iProtection;` |
|       - | 4906 | `	SySet aInterfaces;` |
|       - | 4907 | `	sxi32 iAttrflags;` |
|       - | 4908 | `	SyString *pName;` |
|       - | 4909 | `	sxi32 nKwrd;` |
|       - | 4910 | `	sxi32 rc;` |
|       - | 4911 | `	/* Jump the 'class' keyword */` |
|   21026 | 4912 | `	pGen->pIn++;` |
|   21026 | 4913 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4914 | `		/* Syntax error */` |
|     ! 0 | 4915 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 4916 | `		if( rc == SXERR_ABORT ){` |
|       - | 4917 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4918 | `			return SXERR_ABORT;` |
|       - | 4919 | `		}` |
|       - | 4920 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 4921 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 4922 | `			pGen->pIn++;` |
|     ! 0 | 4923 | `		}` |
|     ! 0 | 4924 | `		return SXRET_OK;` |
|       - | 4925 | `	}` |
|       - | 4926 | `	/* Extract class name */` |
|   21026 | 4927 | `	pName = &pGen->pIn->sData;` |
|       - | 4928 | `	/* Advance the stream cursor */` |
|   21026 | 4929 | `	pGen->pIn++;` |
|       - | 4930 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 4931 | `		SyBlob sFQN;` |
|       - | 4932 | `		SyString sFQNStr;` |
|   21026 | 4933 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   21026 | 4934 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   21026 | 4935 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   21026 | 4936 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   21026 | 4937 | `		SyBlobRelease(&sFQN);` |
|       - | 4938 | `	}` |
|   21026 | 4939 | `	if( pClass == 0 ){` |
|     ! 0 | 4940 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4941 | `		return SXERR_ABORT;` |
|       - | 4942 | `	}` |
|       - | 4943 | `	/* implemented interfaces container */` |
|   21026 | 4944 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|       - | 4945 | `	/* Assume a standalone class */` |
|   21026 | 4946 | `	pBase = 0;` |
|   21026 | 4947 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 4948 | `		SyString *pBaseName;` |
|   13872 | 4949 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   13872 | 4950 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   13868 | 4951 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   13868 | 4952 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4953 | `				/* Syntax error */` |
|     ! 0 | 4954 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4955 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 4956 | `					pName);` |
|     ! 0 | 4957 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4958 | `				if( rc == SXERR_ABORT ){` |
|       - | 4959 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4960 | `					return SXERR_ABORT;` |
|       - | 4961 | `				}` |
|     ! 0 | 4962 | `				return SXRET_OK;` |
|       - | 4963 | `			}` |
|       - | 4964 | `			/* Extract base class name */` |
|   13868 | 4965 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 4966 | `			/* Perform the query */` |
|   13868 | 4967 | `			pBase = PH7_VmExtractClass(pGen->pVm,pBaseName->zString,pBaseName->nByte,FALSE,0);` |
|       - | 4968 | `			/* Interfaces are not allowed */` |
|   13868 | 4969 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 4970 | `				pBase = pBase->pNextName;` |
|     ! 0 | 4971 | `			}` |
|   13868 | 4972 | `			if( pBase == 0 ){` |
|       - | 4973 | `				/* Inexistant base class */` |
|     ! 0 | 4974 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 4975 | `				if( rc == SXERR_ABORT ){` |
|       - | 4976 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4977 | `					return SXERR_ABORT;` |
|       - | 4978 | `				}` |
|     ! 0 | 4979 | `			}else{` |
|   13868 | 4980 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 4981 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 4982 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 4983 | `					if( rc == SXERR_ABORT ){` |
|       - | 4984 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 4985 | `						return SXERR_ABORT;` |
|       - | 4986 | `					}` |
|     ! 0 | 4987 | `				}` |
|       - | 4988 | `			}` |
|       - | 4989 | `			/* Advance the stream cursor */` |
|   13868 | 4990 | `			pGen->pIn++;` |
|    6933 | 4991 | `		}` |
|   13872 | 4992 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 4993 | `			ph7_class *pInterface;` |
|       - | 4994 | `			SyString *pIntName;` |
|       - | 4995 | `			/* Interface implementation */` |
|       8 | 4996 | `			pGen->pIn++; /* Advance the stream cursor */` |
|       3 | 4997 | `			for(;;){` |
|       8 | 4998 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4999 | `					/* Syntax error */` |
|     ! 0 | 5000 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5001 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 5002 | `						pName);` |
|     ! 0 | 5003 | `					if( rc == SXERR_ABORT ){` |
|       - | 5004 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5005 | `						return SXERR_ABORT;` |
|       - | 5006 | `					}` |
|     ! 0 | 5007 | `					break;` |
|       - | 5008 | `				}` |
|       - | 5009 | `				/* Extract interface name */` |
|       8 | 5010 | `				pIntName = &pGen->pIn->sData;` |
|       - | 5011 | `				/* Make sure the interface is already defined */` |
|       8 | 5012 | `				pInterface = PH7_VmExtractClass(pGen->pVm,pIntName->zString,pIntName->nByte,FALSE,0);` |
|       - | 5013 | `				/* Only interfaces are allowed */` |
|       8 | 5014 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5015 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 5016 | `				}` |
|       8 | 5017 | `				if( pInterface == 0 ){` |
|       - | 5018 | `					/* Inexistant interface */` |
|     ! 0 | 5019 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 5020 | `					if( rc == SXERR_ABORT ){` |
|       - | 5021 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5022 | `						return SXERR_ABORT;` |
|       - | 5023 | `					}` |
|     ! 0 | 5024 | `				}else{` |
|       - | 5025 | `					/* Register interface */` |
|       8 | 5026 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 5027 | `				}` |
|       - | 5028 | `				/* Advance the stream cursor */` |
|       8 | 5029 | `				pGen->pIn++;` |
|       8 | 5030 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       5 | 5031 | `					break;` |
|       - | 5032 | `				}` |
|     ! 0 | 5033 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 5034 | `			}` |
|       3 | 5035 | `		}` |
|    6935 | 5036 | `	}` |
|   21026 | 5037 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5038 | `		/* Syntax error */` |
|     ! 0 | 5039 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 5040 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5041 | `		if( rc == SXERR_ABORT ){` |
|       - | 5042 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5043 | `			return SXERR_ABORT;` |
|       - | 5044 | `		}` |
|     ! 0 | 5045 | `		return SXRET_OK;` |
|       - | 5046 | `	}` |
|   21026 | 5047 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   21026 | 5048 | `	pEnd = 0; /* cc warning */` |
|       - | 5049 | `	/* Delimit the class body */` |
|   21026 | 5050 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   21026 | 5051 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5052 | `		/* Syntax error */` |
|     ! 0 | 5053 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 5054 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5055 | `		if( rc == SXERR_ABORT ){` |
|       - | 5056 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5057 | `			return SXERR_ABORT;` |
|       - | 5058 | `		}` |
|     ! 0 | 5059 | `		return SXRET_OK;` |
|       - | 5060 | `	}` |
|       - | 5061 | `	/* Swap token stream */` |
|   21026 | 5062 | `	pTmp = pGen->pEnd;` |
|   21026 | 5063 | `	pGen->pEnd = pEnd;` |
|       - | 5064 | `	/* Set the inherited flags */` |
|   21026 | 5065 | `	pClass->iFlags = iFlags;` |
|       - | 5066 | `	/* Start the parse process */` |
|   34825 | 5067 | `	for(;;){` |
|       - | 5068 | `		/* Jump leading/trailing semi-colons */` |
|  116084 | 5069 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   23228 | 5070 | `			pGen->pIn++;` |
|       2 | 5071 | `		}` |
|   92858 | 5072 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5073 | `			/* End of class body */` |
|   21022 | 5074 | `			break;` |
|       - | 5075 | `		}` |
|   71838 | 5076 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5077 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5078 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5079 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5080 | `			if( rc == SXERR_ABORT ){` |
|       - | 5081 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5082 | `				return SXERR_ABORT;` |
|       - | 5083 | `			}` |
|     ! 0 | 5084 | `			goto done;` |
|       - | 5085 | `		}` |
|       - | 5086 | `		/* Assume public visibility */` |
|   71838 | 5087 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   71838 | 5088 | `		iAttrflags = 0;` |
|   71838 | 5089 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 5090 | `			/* Extract the current keyword */` |
|   71838 | 5091 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   71838 | 5092 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|   69472 | 5093 | `				iProtection = nKwrd;` |
|   69472 | 5094 | `				pGen->pIn++; /* Jump the visibility token */` |
|   69472 | 5095 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5096 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5097 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5098 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5099 | `					if( rc == SXERR_ABORT ){` |
|       - | 5100 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5101 | `						return SXERR_ABORT;` |
|       - | 5102 | `					}` |
|     ! 0 | 5103 | `					goto done;` |
|       - | 5104 | `				}` |
|   69472 | 5105 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5106 | `					/* Attribute declaration */` |
|   23206 | 5107 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   23206 | 5108 | `					if( rc != SXRET_OK ){` |
|       3 | 5109 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5110 | `							return SXERR_ABORT;` |
|       - | 5111 | `						}` |
|       3 | 5112 | `						goto done;` |
|       - | 5113 | `					}` |
|   23204 | 5114 | `					continue;` |
|       - | 5115 | `				}` |
|       - | 5116 | `				/* Extract the keyword */` |
|   46268 | 5117 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   23133 | 5118 | `			}` |
|   48634 | 5119 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5120 | `				/* Process constant declaration */` |
|      10 | 5121 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 | 5122 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5123 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5124 | `						return SXERR_ABORT;` |
|       - | 5125 | `					}` |
|     ! 0 | 5126 | `					goto done;` |
|       - | 5127 | `				}` |
|       6 | 5128 | `			}else{` |
|   48626 | 5129 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5130 | `					/* Static method or attribute,record that */` |
|      23 | 5131 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      23 | 5132 | `					pGen->pIn++; /* Jump the static keyword */` |
|      23 | 5133 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5134 | `						/* Extract the keyword */` |
|      19 | 5135 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      19 | 5136 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 5137 | `							iProtection = nKwrd;` |
|     ! 0 | 5138 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 5139 | `						}` |
|       9 | 5140 | `					}` |
|      23 | 5141 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5142 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5143 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 5144 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5145 | `						if( rc == SXERR_ABORT ){` |
|       - | 5146 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5147 | `							return SXERR_ABORT;` |
|       - | 5148 | `						}` |
|     ! 0 | 5149 | `						goto done;` |
|       - | 5150 | `					}` |
|      23 | 5151 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5152 | `						/* Attribute declaration */` |
|       5 | 5153 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 5154 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 5155 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5156 | `								return SXERR_ABORT;` |
|       - | 5157 | `							}` |
|     ! 0 | 5158 | `							goto done;` |
|       - | 5159 | `						}` |
|       5 | 5160 | `						continue;` |
|       - | 5161 | `					}` |
|       - | 5162 | `					/* Extract the keyword */` |
|      19 | 5163 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   48613 | 5164 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 5165 | `					/* Abstract method,record that */` |
|       8 | 5166 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 5167 | `					/* Mark the whole class as abstract */` |
|       8 | 5168 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 5169 | `					/* Advance the stream cursor */` |
|       8 | 5170 | `					pGen->pIn++;` |
|       8 | 5171 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       8 | 5172 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       8 | 5173 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 | 5174 | `							iProtection = nKwrd;` |
|       6 | 5175 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 5176 | `						}` |
|       3 | 5177 | `					}` |
|       8 | 5178 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       6 | 5179 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5180 | `							/* Static method */` |
|     ! 0 | 5181 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5182 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5183 | `					}` |
|       8 | 5184 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       6 | 5185 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5186 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5187 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 5188 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5189 | `							if( rc == SXERR_ABORT ){` |
|       - | 5190 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5191 | `								return SXERR_ABORT;` |
|       - | 5192 | `							}` |
|     ! 0 | 5193 | `							goto done;` |
|       - | 5194 | `					}` |
|       8 | 5195 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   48601 | 5196 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 5197 | `					/* final method ,record that */` |
|       5 | 5198 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 5199 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 5200 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5201 | `						/* Extract the keyword */` |
|       5 | 5202 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 5203 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 5204 | `							iProtection = nKwrd;` |
|       5 | 5205 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 5206 | `						}` |
|       2 | 5207 | `					}` |
|       5 | 5208 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 5209 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5210 | `							/* Static method */` |
|     ! 0 | 5211 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5212 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5213 | `					}` |
|       5 | 5214 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 5215 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5216 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5217 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 5218 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5219 | `							if( rc == SXERR_ABORT ){` |
|       - | 5220 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5221 | `								return SXERR_ABORT;` |
|       - | 5222 | `							}` |
|     ! 0 | 5223 | `							goto done;` |
|       - | 5224 | `					}` |
|       5 | 5225 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 5226 | `				}` |
|   48622 | 5227 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 5228 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5229 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 5230 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5231 | `						if( rc == SXERR_ABORT ){` |
|       - | 5232 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5233 | `							return SXERR_ABORT;` |
|       - | 5234 | `						}` |
|     ! 0 | 5235 | `						goto done;` |
|       - | 5236 | `				}` |
|   48622 | 5237 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 5238 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 5239 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 5240 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5241 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 5242 | `						if( rc == SXERR_ABORT ){` |
|       - | 5243 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5244 | `							return SXERR_ABORT;` |
|       - | 5245 | `						}` |
|     ! 0 | 5246 | `						goto done;` |
|       - | 5247 | `					}` |
|       - | 5248 | `					/* Attribute declaration */` |
|       7 | 5249 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 5250 | `				}else{` |
|       - | 5251 | `					/* Process method declaration */` |
|   48616 | 5252 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 5253 | `				}` |
|   48622 | 5254 | `				if( rc != SXRET_OK ){` |
|       3 | 5255 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5256 | `						return SXERR_ABORT;` |
|       - | 5257 | `					}` |
|       3 | 5258 | `					goto done;` |
|       - | 5259 | `				}` |
|       - | 5260 | `			}` |
|   24315 | 5261 | `		}else{` |
|       - | 5262 | `			/* Attribute declaration */` |
|     ! 0 | 5263 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5264 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5265 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5266 | `					return SXERR_ABORT;` |
|       - | 5267 | `				}` |
|     ! 0 | 5268 | `				goto done;` |
|       - | 5269 | `			}` |
|       - | 5270 | `		}` |
|       2 | 5271 | `	}` |
|       - | 5272 | `	/* Install the class */` |
|   21022 | 5273 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   21022 | 5274 | `	if( rc == SXRET_OK ){` |
|       - | 5275 | `		ph7_class **apInterface;` |
|       - | 5276 | `		sxu32 n;` |
|   21022 | 5277 | `		if( pBase ){` |
|       - | 5278 | `			/* Inherit from base class and mark as a subclass */` |
|   13868 | 5279 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    6933 | 5280 | `		}` |
|   21022 | 5281 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   21028 | 5282 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 5283 | `			/* Implements one or more interface */` |
|       8 | 5284 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|       8 | 5285 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5286 | `				break;` |
|       - | 5287 | `			}` |
|       5 | 5288 | `		}` |
|   10510 | 5289 | `	}` |
|   21022 | 5290 | `	SySetRelease(&aInterfaces);` |
|   21022 | 5291 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5292 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5293 | `		return SXERR_ABORT;` |
|       - | 5294 | `	}` |
|   10510 | 5295 | `done:` |
|       - | 5296 | `	/* Point beyond the class body */` |
|   21026 | 5297 | `	pGen->pIn = &pEnd[1];` |
|   21026 | 5298 | `	pGen->pEnd = pTmp;` |
|   21026 | 5299 | `	return PH7_OK;` |
|   10514 | 5300 |  |
|       - | 5301 | `/*` |
|       - | 5302 | ` * Compile a user-defined abstract class.` |
|       - | 5303 | ` *  According to the PHP language reference manual` |
|       - | 5304 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 5305 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 5306 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 5307 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 5308 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 5309 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 5310 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 5311 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 5312 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 5313 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 5314 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 5315 | ` *   could differ.` |
|       - | 5316 | ` */` |
|       4 | 5317 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 5318 |  |
|       - | 5319 | `	sxi32 rc;` |
|       6 | 5320 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|       6 | 5321 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|       6 | 5322 | `	return rc;` |
|       2 | 5323 |  |
|       - | 5324 | `/*` |
|       - | 5325 | ` * Compile a user-defined final class.` |
|       - | 5326 | ` *  According to the PHP language reference manual` |
|       - | 5327 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 5328 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 5329 | ` *    final then it cannot be extended.` |
|       - | 5330 | ` */` |
|       2 | 5331 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 5332 |  |
|       - | 5333 | `	sxi32 rc;` |
|       3 | 5334 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 5335 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 5336 | `	return rc;` |
|       1 | 5337 |  |
|       - | 5338 | `/*` |
|       - | 5339 | ` * Compile a user-defined class.` |
|       - | 5340 | ` *  According to the PHP language reference manual` |
|       - | 5341 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 5342 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 5343 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 5344 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 5345 | ` *   and functions (called "methods").` |
|       - | 5346 | ` */` |
|   21018 | 5347 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 5348 |  |
|       - | 5349 | `	sxi32 rc;` |
|   21020 | 5350 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   21020 | 5351 | `	return rc;` |
|       2 | 5352 |  |
|       - | 5353 | `/*` |
|       - | 5354 | ` * Exception handling.` |
|       - | 5355 | ` *  According to the PHP language reference manual` |
|       - | 5356 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 5357 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 5358 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 5359 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 5360 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 5361 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 5362 | ` *    (or re-thrown) within a catch block.` |
|       - | 5363 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 5364 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 5365 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 5366 | ` *    been defined with set_exception_handler().` |
|       - | 5367 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 5368 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 5369 | ` */` |
|       - | 5370 | `/*` |
|       - | 5371 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 5372 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 5373 | ` * indicates failure.` |
|       - | 5374 | ` */` |
|    6930 | 5375 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 5376 |  |
|    6932 | 5377 | `	sxi32 rc = SXRET_OK;` |
|    6932 | 5378 | `	if( pRoot->pOp ){` |
|    6928 | 5379 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    3466 | 5380 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 5381 | `			/* Unexpected expression */` |
|     ! 0 | 5382 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 5383 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 5384 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 5385 | `				rc = SXERR_INVALID;` |
|     ! 0 | 5386 | `			}` |
|       2 | 5387 | `		}` |
|    3467 | 5388 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 5389 | `		/* Unexpected expression */` |
|     ! 0 | 5390 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 5391 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 5392 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 5393 | `			rc = SXERR_INVALID;` |
|     ! 0 | 5394 | `		}` |
|     ! 0 | 5395 | `	}` |
|    6932 | 5396 | `	return rc;` |
|       2 | 5397 |  |
|       - | 5398 | `/*` |
|       - | 5399 | ` * Compile a 'throw' statement.` |
|       - | 5400 | ` * throw: This is how you trigger an exception.` |
|       - | 5401 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 5402 | ` */` |
|    6930 | 5403 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 5404 |  |
|    6932 | 5405 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5406 | `	GenBlock *pBlock;` |
|       - | 5407 | `	sxu32 nIdx;` |
|       - | 5408 | `	sxi32 rc;` |
|    6932 | 5409 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 5410 | `	/* Compile the expression */` |
|    6932 | 5411 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    6932 | 5412 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 5413 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 5414 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5415 | `			return SXERR_ABORT;` |
|       - | 5416 | `		}` |
|     ! 0 | 5417 | `		return SXRET_OK;` |
|       - | 5418 | `	}` |
|    6932 | 5419 | `	pBlock = pGen->pCurrent;` |
|       - | 5420 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   32294 | 5421 | `	while(pBlock->pParent){` |
|   32290 | 5422 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    6928 | 5423 | `			break;` |
|       - | 5424 | `		}` |
|       - | 5425 | `		/* Point to the parent block */` |
|   25364 | 5426 | `		pBlock = pBlock->pParent;` |
|       2 | 5427 | `	}` |
|       - | 5428 | `	/* Emit the throw instruction */` |
|    6932 | 5429 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 5430 | `	/* Emit the jump */` |
|    6932 | 5431 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    6932 | 5432 | `	return SXRET_OK;` |
|    3467 | 5433 |  |
|       - | 5434 | `/*` |
|       - | 5435 | ` * Compile a 'catch' block.` |
|       - | 5436 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 5437 | ` * an object containing the exception information.` |
|       - | 5438 | ` */` |
|      34 | 5439 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 5440 |  |
|      36 | 5441 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5442 | `	ph7_exception_block sCatch;` |
|       - | 5443 | `	SySet *pInstrContainer;` |
|       - | 5444 | `	GenBlock *pCatch;` |
|       - | 5445 | `	SyToken *pToken;` |
|       - | 5446 | `	SyString *pName;` |
|       - | 5447 | `	char *zDup;` |
|       - | 5448 | `	sxi32 rc;` |
|      36 | 5449 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 5450 | `	/* Zero the structure */` |
|      36 | 5451 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 5452 | `	/* Initialize fields */` |
|      36 | 5453 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|      51 | 5454 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ \|\|` |
|      36 | 5455 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5456 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 5457 | `			pToken = pGen->pIn;` |
|     ! 0 | 5458 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 5459 | `				pToken--;` |
|     ! 0 | 5460 | `			}` |
|     ! 0 | 5461 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 5462 | `				"Catch: Unexpected token '%z',excpecting class name",&pToken->sData);` |
|     ! 0 | 5463 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5464 | `				return SXERR_ABORT;` |
|       - | 5465 | `			}` |
|     ! 0 | 5466 | `			return SXERR_INVALID;` |
|       - | 5467 | `	}` |
|       - | 5468 | `	/* Extract the exception class */` |
|      36 | 5469 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|       - | 5470 | `	/* Duplicate class name */` |
|      36 | 5471 | `	pName = &pGen->pIn->sData;` |
|      36 | 5472 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      36 | 5473 | `	if( zDup == 0 ){` |
|     ! 0 | 5474 | `		goto Mem;` |
|       - | 5475 | `	}` |
|      36 | 5476 | `	SyStringInitFromBuf(&sCatch.sClass,zDup,pName->nByte);` |
|      36 | 5477 | `	pGen->pIn++;` |
|      51 | 5478 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      36 | 5479 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5480 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 5481 | `			pToken = pGen->pIn;` |
|     ! 0 | 5482 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 5483 | `				pToken--;` |
|     ! 0 | 5484 | `			}` |
|     ! 0 | 5485 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 5486 | `				"Catch: Unexpected token '%z',expecting variable name",&pToken->sData);` |
|     ! 0 | 5487 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5488 | `				return SXERR_ABORT;` |
|       - | 5489 | `			}` |
|     ! 0 | 5490 | `			return SXERR_INVALID;` |
|       - | 5491 | `	}` |
|      36 | 5492 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 5493 | `	/* Duplicate instance name */` |
|      36 | 5494 | `	pName = &pGen->pIn->sData;` |
|      36 | 5495 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      36 | 5496 | `	if( zDup == 0 ){` |
|     ! 0 | 5497 | `		goto Mem;` |
|       - | 5498 | `	}` |
|      36 | 5499 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      36 | 5500 | `	pGen->pIn++;` |
|      36 | 5501 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 5502 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 5503 | `		pToken = pGen->pIn;` |
|     ! 0 | 5504 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 5505 | `			pToken--;` |
|     ! 0 | 5506 | `		}` |
|     ! 0 | 5507 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 5508 | `			"Catch: Unexpected token '%z',expecting right parenthesis ')'",&pToken->sData);` |
|     ! 0 | 5509 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5510 | `			return SXERR_ABORT;` |
|       - | 5511 | `		}` |
|     ! 0 | 5512 | `		return SXERR_INVALID;` |
|       - | 5513 | `	}` |
|       - | 5514 | `	/* Compile the block */` |
|      36 | 5515 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 5516 | `	/* Create the catch block */` |
|      36 | 5517 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      36 | 5518 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5519 | `		return SXERR_ABORT;` |
|       - | 5520 | `	}` |
|       - | 5521 | `	/* Swap bytecode container */` |
|      36 | 5522 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      36 | 5523 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 5524 | `	/* Compile the block */` |
|      36 | 5525 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 5526 | `	/* Fix forward jumps now the destination is resolved  */` |
|      36 | 5527 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 5528 | `	/* Emit the DONE instruction */` |
|      36 | 5529 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 5530 | `	/* Leave the block */` |
|      36 | 5531 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 5532 | `	/* Restore the default container */` |
|      36 | 5533 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 5534 | `	/* Install the catch block */` |
|      36 | 5535 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      36 | 5536 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5537 | `		goto Mem;` |
|       - | 5538 | `	}` |
|      36 | 5539 | `	return SXRET_OK;` |
|     ! 0 | 5540 | `Mem:` |
|     ! 0 | 5541 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5542 | `	return SXERR_ABORT;` |
|      19 | 5543 |  |
|       - | 5544 | `/*` |
|       - | 5545 | ` * Compile a 'try' block.` |
|       - | 5546 | ` * A function using an exception should be in a "try" block.` |
|       - | 5547 | ` * If the exception does not trigger, the code will continue` |
|       - | 5548 | ` * as normal. However if the exception triggers, an exception` |
|       - | 5549 | ` * is "thrown".` |
|       - | 5550 | ` */` |
|      36 | 5551 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 5552 |  |
|       - | 5553 | `	ph7_exception *pException;` |
|       - | 5554 | `	GenBlock *pTry;` |
|       - | 5555 | `	sxu32 nJmpIdx;` |
|       - | 5556 | `	sxi32 rc;` |
|       - | 5557 | `	/* Create the exception container */` |
|      38 | 5558 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|      38 | 5559 | `	if( pException == 0 ){` |
|     ! 0 | 5560 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 5561 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5562 | `		return SXERR_ABORT;` |
|       - | 5563 | `	}` |
|       - | 5564 | `	/* Zero the structure */` |
|      38 | 5565 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 5566 | `	/* Initialize fields */` |
|      38 | 5567 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|      38 | 5568 | `	pException->pVm = pGen->pVm;` |
|       - | 5569 | `	/* Create the try block */` |
|      38 | 5570 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      38 | 5571 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5572 | `		return SXERR_ABORT;` |
|       - | 5573 | `	}` |
|       - | 5574 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|      38 | 5575 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 5576 | `	/* Fix the jump later when the destination is resolved */` |
|      38 | 5577 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|      38 | 5578 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 5579 | `	/* Compile the block */` |
|      38 | 5580 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      38 | 5581 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5582 | `		return SXERR_ABORT;` |
|       - | 5583 | `	}` |
|       - | 5584 | `	/* Fix forward jumps now the destination is resolved */` |
|      38 | 5585 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 5586 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|      38 | 5587 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 5588 | `	/* Leave the block */` |
|      38 | 5589 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 5590 | `	/* Compile the catch block */` |
|      38 | 5591 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      34 | 5592 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       3 | 5593 | `			SyToken *pTok = pGen->pIn;` |
|       3 | 5594 | `			if( pTok >= pGen->pEnd ){` |
|       3 | 5595 | `				pTok--; /* Point back */` |
|       1 | 5596 | `			}` |
|       - | 5597 | `			/* Unexpected token */` |
|       4 | 5598 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTok->nLine,` |
|       1 | 5599 | `				"Try: Unexpected token '%z',expecting 'catch' block",&pTok->sData);` |
|       3 | 5600 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5601 | `				return SXERR_ABORT;` |
|       - | 5602 | `			}` |
|       3 | 5603 | `			return SXRET_OK;` |
|       - | 5604 | `	}` |
|       - | 5605 | `	/* Compile one or more catch blocks */` |
|      34 | 5606 | `	for(;;){` |
|      68 | 5607 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      47 | 5608 | `			\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       - | 5609 | `				/* No more blocks */` |
|      19 | 5610 | `				break;` |
|       - | 5611 | `		}` |
|       - | 5612 | `		/* Compile the catch block */` |
|      36 | 5613 | `		rc = PH7_CompileCatch(&(*pGen),pException);` |
|      36 | 5614 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5615 | `			return SXERR_ABORT;` |
|       - | 5616 | `		}` |
|       2 | 5617 | ` 	}` |
|      36 | 5618 | `	return SXRET_OK;` |
|      20 | 5619 |  |
|       - | 5620 | `/*` |
|       - | 5621 | ` * Compile a switch block.` |
|       - | 5622 | ` *  (See block-comment below for more information)` |
|       - | 5623 | ` */` |
|      84 | 5624 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 5625 |  |
|      86 | 5626 | `	sxi32 rc = SXRET_OK;` |
|      86 | 5627 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 5628 | `		/* Unexpected token */` |
|     ! 0 | 5629 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 5630 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5631 | `			return SXERR_ABORT;` |
|       - | 5632 | `		}` |
|     ! 0 | 5633 | `		pGen->pIn++;` |
|     ! 0 | 5634 | `	}` |
|      86 | 5635 | `	pGen->pIn++;` |
|       - | 5636 | `	/* First instruction to execute in this block. */` |
|      86 | 5637 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 5638 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 5639 | `	 * or the '}' token */` |
|     151 | 5640 | `	for(;;){` |
|     304 | 5641 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5642 | `			/* No more input to process */` |
|     ! 0 | 5643 | `			break;` |
|       - | 5644 | `		}` |
|     304 | 5645 | `		rc = SXRET_OK;` |
|     304 | 5646 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      62 | 5647 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      20 | 5648 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 5649 | `					/* Unexpected token */` |
|     ! 0 | 5650 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 5651 | `						&pGen->pIn->sData);` |
|     ! 0 | 5652 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5653 | `						return SXERR_ABORT;` |
|       - | 5654 | `					}` |
|       - | 5655 | `					/* FALL THROUGH */` |
|     ! 0 | 5656 | `				}` |
|      20 | 5657 | `				rc = SXERR_EOF;` |
|      20 | 5658 | `				break;` |
|       - | 5659 | `			}` |
|      23 | 5660 | `		}else{` |
|       - | 5661 | `			sxi32 nKwrd;` |
|       - | 5662 | `			/* Extract the keyword */` |
|     244 | 5663 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     244 | 5664 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      34 | 5665 | `				break;` |
|       - | 5666 | `			}` |
|     180 | 5667 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 5668 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 5669 | `					/* Unexpected token */` |
|     ! 0 | 5670 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 5671 | `						&pGen->pIn->sData);` |
|     ! 0 | 5672 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5673 | `						return SXERR_ABORT;` |
|       - | 5674 | `					}` |
|       - | 5675 | `					/* FALL THROUGH */` |
|     ! 0 | 5676 | `				}` |
|       - | 5677 | `				/* Block compiled */` |
|       3 | 5678 | `				break;` |
|       - | 5679 | `			}` |
|       - | 5680 | `		}` |
|       - | 5681 | `		/* Compile block */` |
|     220 | 5682 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     220 | 5683 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5684 | `			return SXERR_ABORT;` |
|       - | 5685 | `		}` |
|       2 | 5686 | `	}` |
|      86 | 5687 | `	return rc;` |
|      44 | 5688 |  |
|       - | 5689 | `/*` |
|       - | 5690 | ` * Compile a case eXpression.` |
|       - | 5691 | ` *  (See block-comment below for more information)` |
|       - | 5692 | ` */` |
|      70 | 5693 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 5694 |  |
|       - | 5695 | `	SySet *pInstrContainer;` |
|       - | 5696 | `	SyToken *pEnd,*pTmp;` |
|      72 | 5697 | `	sxi32 iNest = 0;` |
|       - | 5698 | `	sxi32 rc;` |
|       - | 5699 | `	/* Delimit the expression */` |
|      72 | 5700 | `	pEnd = pGen->pIn;` |
|     150 | 5701 | `	while( pEnd < pGen->pEnd ){` |
|     150 | 5702 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 5703 | `			/* Increment nesting level */` |
|       3 | 5704 | `			iNest++;` |
|     149 | 5705 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 5706 | `			/* Decrement nesting level */` |
|       3 | 5707 | `			iNest--;` |
|     147 | 5708 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      72 | 5709 | `			break;` |
|       - | 5710 | `		}` |
|      80 | 5711 | `		pEnd++;` |
|       2 | 5712 | `	}` |
|      72 | 5713 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 5714 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 5715 | `		if( rc == SXERR_ABORT ){` |
|       - | 5716 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5717 | `			return SXERR_ABORT;` |
|       - | 5718 | `		}` |
|     ! 0 | 5719 | `	}` |
|       - | 5720 | `	/* Swap token stream */` |
|      72 | 5721 | `	pTmp = pGen->pEnd;` |
|      72 | 5722 | `	pGen->pEnd = pEnd;` |
|      72 | 5723 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      72 | 5724 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      72 | 5725 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 5726 | `	/* Emit the done instruction */` |
|      72 | 5727 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      72 | 5728 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 5729 | `	/* Update token stream */` |
|      72 | 5730 | `	pGen->pIn  = pEnd;` |
|      72 | 5731 | `	pGen->pEnd = pTmp;` |
|      72 | 5732 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5733 | `		return SXERR_ABORT;` |
|       - | 5734 | `	}` |
|      72 | 5735 | `	return SXRET_OK;` |
|      37 | 5736 |  |
|       - | 5737 | `/*` |
|       - | 5738 | ` * Compile the smart switch statement.` |
|       - | 5739 | ` * According to the PHP language reference manual` |
|       - | 5740 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 5741 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 5742 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 5743 | ` *  This is exactly what the switch statement is for.` |
|       - | 5744 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 5745 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 5746 | ` *  of the outer loop, use continue 2.` |
|       - | 5747 | ` *  Note that switch/case does loose comparision.` |
|       - | 5748 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 5749 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 5750 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 5751 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 5752 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 5753 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 5754 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 5755 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 5756 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 5757 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 5758 | ` *  list for the next case.` |
|       - | 5759 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 5760 | ` *  or floating-point numbers and strings.` |
|       - | 5761 | ` */` |
|      20 | 5762 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 5763 |  |
|       - | 5764 | `	GenBlock *pSwitchBlock;` |
|       - | 5765 | `	SyToken *pTmp,*pEnd;` |
|       - | 5766 | `	ph7_switch *pSwitch;` |
|       - | 5767 | `	sxu32 nToken;` |
|       - | 5768 | `	sxu32 nLine;` |
|       - | 5769 | `	sxi32 rc;` |
|      22 | 5770 | `	nLine = pGen->pIn->nLine;` |
|       - | 5771 | `	/* Jump the 'switch' keyword */` |
|      22 | 5772 | `	pGen->pIn++;` |
|      22 | 5773 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 5774 | `		/* Syntax error */` |
|     ! 0 | 5775 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 5776 | `		if( rc == SXERR_ABORT ){` |
|       - | 5777 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5778 | `			return SXERR_ABORT;` |
|       - | 5779 | `		}` |
|     ! 0 | 5780 | `		goto Synchronize;` |
|       - | 5781 | `	}` |
|       - | 5782 | `	/* Jump the left parenthesis '(' */` |
|      22 | 5783 | `	pGen->pIn++;` |
|      22 | 5784 | `	pEnd = 0; /* cc warning */` |
|       - | 5785 | `	/* Create the loop block */` |
|      32 | 5786 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      10 | 5787 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      22 | 5788 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5789 | `		return SXERR_ABORT;` |
|       - | 5790 | `	}` |
|       - | 5791 | `	/* Delimit the condition */` |
|      22 | 5792 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      22 | 5793 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 5794 | `		/* Empty expression */` |
|     ! 0 | 5795 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 5796 | `		if( rc == SXERR_ABORT ){` |
|       - | 5797 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5798 | `			return SXERR_ABORT;` |
|       - | 5799 | `		}` |
|     ! 0 | 5800 | `	}` |
|       - | 5801 | `	/* Swap token streams */` |
|      22 | 5802 | `	pTmp = pGen->pEnd;` |
|      22 | 5803 | `	pGen->pEnd = pEnd;` |
|       - | 5804 | `	/* Compile the expression */` |
|      22 | 5805 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      22 | 5806 | `	if( rc == SXERR_ABORT ){` |
|       - | 5807 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 5808 | `		return SXERR_ABORT;` |
|       - | 5809 | `	}` |
|       - | 5810 | `	/* Update token stream */` |
|      22 | 5811 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 5812 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5813 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 5814 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5815 | `			return SXERR_ABORT;` |
|       - | 5816 | `		}` |
|     ! 0 | 5817 | `		pGen->pIn++;` |
|     ! 0 | 5818 | `	}` |
|      22 | 5819 | `	pGen->pIn  = &pEnd[1];` |
|      22 | 5820 | `	pGen->pEnd = pTmp;` |
|      22 | 5821 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      20 | 5822 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 5823 | `			pTmp = pGen->pIn;` |
|     ! 0 | 5824 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 5825 | `				pTmp--;` |
|     ! 0 | 5826 | `			}` |
|       - | 5827 | `			/* Unexpected token */` |
|     ! 0 | 5828 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 5829 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5830 | `				return SXERR_ABORT;` |
|       - | 5831 | `			}` |
|     ! 0 | 5832 | `			goto Synchronize;` |
|       - | 5833 | `	}` |
|       - | 5834 | `	/* Set the delimiter token */` |
|      22 | 5835 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 5836 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 5837 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 5838 | `	}else{` |
|      20 | 5839 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 5840 | `	}` |
|      22 | 5841 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 5842 | `	/* Create the switch blocks container */` |
|      22 | 5843 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      22 | 5844 | `	if( pSwitch == 0 ){` |
|       - | 5845 | `		/* Abort compilation */` |
|     ! 0 | 5846 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5847 | `		return SXERR_ABORT;` |
|       - | 5848 | `	}` |
|       - | 5849 | `	/* Zero the structure */` |
|      22 | 5850 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 5851 | `	/* Initialize fields */` |
|      22 | 5852 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 5853 | `	/* Emit the switch instruction */` |
|      22 | 5854 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 5855 | `	/* Compile case blocks */` |
|      76 | 5856 | `	for(;;){` |
|       - | 5857 | `		sxu32 nKwrd;` |
|      88 | 5858 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5859 | `			/* No more input to process */` |
|     ! 0 | 5860 | `			break;` |
|       - | 5861 | `		}` |
|      88 | 5862 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5863 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 5864 | `				/* Unexpected token */` |
|     ! 0 | 5865 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 5866 | `					&pGen->pIn->sData);` |
|     ! 0 | 5867 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5868 | `					return SXERR_ABORT;` |
|       - | 5869 | `				}` |
|       - | 5870 | `				/* FALL THROUGH */` |
|     ! 0 | 5871 | `			}` |
|       - | 5872 | `			/* Block compiled */` |
|     ! 0 | 5873 | `			break;` |
|       - | 5874 | `		}` |
|       - | 5875 | `		/* Extract the keyword */` |
|      88 | 5876 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      88 | 5877 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 5878 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 5879 | `				/* Unexpected token */` |
|     ! 0 | 5880 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 5881 | `					&pGen->pIn->sData);` |
|     ! 0 | 5882 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5883 | `					return SXERR_ABORT;` |
|       - | 5884 | `				}` |
|       - | 5885 | `				/* FALL THROUGH */` |
|     ! 0 | 5886 | `			}` |
|       - | 5887 | `			/* Block compiled */` |
|       3 | 5888 | `			break;` |
|       - | 5889 | `		}` |
|      86 | 5890 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 5891 | `			/*` |
|       - | 5892 | `			 * Accroding to the PHP language reference manual` |
|       - | 5893 | `			 *  A special case is the default case. This case matches anything` |
|       - | 5894 | `			 *  that wasn't matched by the other cases.` |
|       - | 5895 | `			 */` |
|      16 | 5896 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 5897 | `				/* Default case already compiled */` |
|     ! 0 | 5898 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 5899 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5900 | `					return SXERR_ABORT;` |
|       - | 5901 | `				}` |
|     ! 0 | 5902 | `			}` |
|      16 | 5903 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 5904 | `			/* Compile the default block */` |
|      16 | 5905 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      16 | 5906 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 5907 | `				return SXERR_ABORT;` |
|      16 | 5908 | `			}else if( rc == SXERR_EOF ){` |
|      14 | 5909 | `				break;` |
|       1 | 5910 | `			}` |
|      73 | 5911 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 5912 | `			ph7_case_expr sCase;` |
|       - | 5913 | `			/* Standard case block */` |
|      72 | 5914 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 5915 | `			/* initialize the structure */` |
|      72 | 5916 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 5917 | `			/* Compile the case expression */` |
|      72 | 5918 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      72 | 5919 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5920 | `				return SXERR_ABORT;` |
|       - | 5921 | `			}` |
|       - | 5922 | `			/* Compile the case block */` |
|      72 | 5923 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 5924 | `			/* Insert in the switch container */` |
|      72 | 5925 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      72 | 5926 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 5927 | `				return SXERR_ABORT;` |
|      72 | 5928 | `			}else if( rc == SXERR_EOF ){` |
|       7 | 5929 | `				break;` |
|       - | 5930 | `			}` |
|      34 | 5931 | `		}else{` |
|       - | 5932 | `			/* Unexpected token */` |
|     ! 0 | 5933 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 5934 | `				&pGen->pIn->sData);` |
|     ! 0 | 5935 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5936 | `				return SXERR_ABORT;` |
|       - | 5937 | `			}` |
|     ! 0 | 5938 | `			break;` |
|       - | 5939 | `		}` |
|       2 | 5940 | `	}` |
|       - | 5941 | `	/* Fix all jumps now the destination is resolved */` |
|      22 | 5942 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      22 | 5943 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 5944 | `	/* Release the loop block */` |
|      22 | 5945 | `	GenStateLeaveBlock(pGen,0);` |
|      22 | 5946 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 5947 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      22 | 5948 | `		pGen->pIn++;` |
|      10 | 5949 | `	}` |
|       - | 5950 | `	/* Statement successfully compiled */` |
|      22 | 5951 | `	return SXRET_OK;` |
|     ! 0 | 5952 | `Synchronize:` |
|       - | 5953 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 5954 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 5955 | `		pGen->pIn++;` |
|     ! 0 | 5956 | `	}` |
|     ! 0 | 5957 | `	return SXRET_OK;` |
|      12 | 5958 |  |
|       - | 5959 | `/*` |
|       - | 5960 | ` * Generate bytecode for a given expression tree.` |
|       - | 5961 | ` * If something goes wrong while generating bytecode` |
|       - | 5962 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 5963 | ` * this function takes care of generating the appropriate` |
|       - | 5964 | ` * error message.` |
|       - | 5965 | ` */` |
| 1924368 | 5966 | `static sxi32 GenStateEmitExprCode(` |
|       - | 5967 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 5968 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 5969 | `	sxi32 iFlags /* Control flags */` |
|       - | 5970 | `	)` |
|       2 | 5971 |  |
|       - | 5972 | `	VmInstr *pInstr;` |
|       - | 5973 | `	sxu32 nJmpIdx;` |
| 1924370 | 5974 | `	sxi32 iP1 = 0;` |
| 1924370 | 5975 | `	sxu32 iP2 = 0;` |
| 1924370 | 5976 | `	void *p3  = 0;` |
|       - | 5977 | `	sxi32 iVmOp;` |
|       - | 5978 | `	sxi32 rc;` |
| 1924370 | 5979 | `	if( pNode->xCode ){` |
|       - | 5980 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 5981 | `		/* Compile node */` |
| 1180942 | 5982 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1180942 | 5983 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1180942 | 5984 | `		RE_SWAP_DELIMITER(pGen);` |
| 1180942 | 5985 | `		return rc;` |
|       - | 5986 | `	}` |
|  743430 | 5987 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 5988 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 5989 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 5990 | `		return SXERR_ABORT;` |
|       - | 5991 | `	}` |
|  743430 | 5992 | `	iVmOp = pNode->pOp->iVmOp;` |
|  743430 | 5993 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 5994 | `		sxu32 nJz,nJmp;` |
|       - | 5995 | `		/* Ternary operator require special handling */` |
|       - | 5996 | `		/* Phase#1: Compile the condition */` |
|    1728 | 5997 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1728 | 5998 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 5999 | `			return rc;` |
|       - | 6000 | `		}` |
|    1728 | 6001 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1728 | 6002 | `		if( pNode->pLeft ){` |
|       - | 6003 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 6004 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1660 | 6005 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 6006 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1660 | 6007 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1660 | 6008 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6009 | `				return rc;` |
|       - | 6010 | `			}` |
|     831 | 6011 | `		}else{` |
|       - | 6012 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 6013 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 6014 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 6015 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 6016 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 6017 | `		}` |
|       - | 6018 | `		/* Phase#4: Emit the unconditional jump */` |
|    1728 | 6019 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 6020 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1728 | 6021 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1728 | 6022 | `		if( pInstr ){` |
|    1728 | 6023 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     863 | 6024 | `		}` |
|    1728 | 6025 | `		if( !pNode->pLeft ){` |
|       - | 6026 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 6027 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 6028 | `		}` |
|       - | 6029 | `		/* Phase#6: Compile the 'else' expression */` |
|    1728 | 6030 | `		if( pNode->pRight ){` |
|    1728 | 6031 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1728 | 6032 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6033 | `				return rc;` |
|       - | 6034 | `			}` |
|     863 | 6035 | `		}` |
|    1728 | 6036 | `		if( nJmp > 0 ){` |
|       - | 6037 | `			/* Phase#7: Fix the unconditional jump */` |
|    1728 | 6038 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1728 | 6039 | `			if( pInstr ){` |
|    1728 | 6040 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     863 | 6041 | `			}` |
|     863 | 6042 | `		}` |
|       - | 6043 | `		/* All done */` |
|    1728 | 6044 | `		return SXRET_OK;` |
|       - | 6045 | `	}` |
|       - | 6046 | `	/* Generate code for the left tree */` |
|  741704 | 6047 | `	if( pNode->pLeft ){` |
|  741704 | 6048 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 6049 | `			ph7_expr_node **apNode;` |
|       - | 6050 | `			sxi32 n;` |
|       - | 6051 | `			/* Recurse and generate bytecodes for function arguments */` |
|  220264 | 6052 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 6053 | `			/* Read-only load */` |
|  220264 | 6054 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  434422 | 6055 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  214160 | 6056 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  214160 | 6057 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6058 | `					return rc;` |
|       - | 6059 | `				}` |
|  107081 | 6060 | `			}` |
|       - | 6061 | `			/* Total number of given arguments */` |
|  220264 | 6062 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 6063 | `			/* Remove stale flags now */` |
|  220264 | 6064 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  110131 | 6065 | `		}` |
|  741704 | 6066 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  741704 | 6067 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6068 | `			return rc;` |
|       - | 6069 | `		}` |
|  741704 | 6070 | `		if( iVmOp == PH7_OP_CALL ){` |
|  220264 | 6071 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  220264 | 6072 | `			if( pInstr ){` |
|  220264 | 6073 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  220062 | 6074 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 6075 | `					sxu32 nQual;` |
|       - | 6076 | `					/* Prevent constant expansion */` |
|  220062 | 6077 | `					pInstr->iP1 = 0;` |
|       - | 6078 | `					/* Namespace-qualify the function name for CALL */` |
|  220062 | 6079 | `					nQual = GenStateNsQualifyName(pGen,nOrig);` |
|  220062 | 6080 | `					pInstr->iP2 = (sxi32)nQual;` |
|  220062 | 6081 | `					if( nQual != nOrig ){` |
|       - | 6082 | `						/* Name was compiler-qualified: flag CALL for host-function global fallback.` |
|       - | 6083 | `						 * p3 = (void*)1 tells the VM it's safe to strip the NS prefix` |
|       - | 6084 | `						 * and try the short name in hHostFunction. */` |
|      43 | 6085 | `						p3 = (void *)1;` |
|      23 | 6086 | `					}` |
|  110234 | 6087 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 6088 | `					/* Method call,flag that */` |
|     190 | 6089 | `					pInstr->iP2 = 1;` |
|      94 | 6090 | `				}` |
|  110133 | 6091 | `			}` |
|  631573 | 6092 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 6093 | `			ph7_expr_node **apNode;` |
|       - | 6094 | `			sxi32 n;` |
|       - | 6095 | `			/* Recurse and generate bytecodes for array index */` |
|   58994 | 6096 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  106418 | 6097 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   47426 | 6098 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   47426 | 6099 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6100 | `					return rc;` |
|       - | 6101 | `				}` |
|   23714 | 6102 | `			}` |
|   58994 | 6103 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   47426 | 6104 | `				iP1 = 1; /* Node have an index associated with it */` |
|   23712 | 6105 | `			}` |
|   58994 | 6106 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 6107 | `				/* Create an empty entry when the desired index is not found */` |
|   23322 | 6108 | `				iP2 = 1;` |
|   11662 | 6109 | `			}` |
|  491946 | 6110 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 6111 | `			/* POP the left node */` |
|      32 | 6112 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 6113 | `		}` |
|  370851 | 6114 | `	}` |
|  741704 | 6115 | `	rc = SXRET_OK;` |
|  741704 | 6116 | `	nJmpIdx = 0;` |
|       - | 6117 | `	/* Generate code for the right tree */` |
|  741704 | 6118 | `	if( pNode->pRight ){` |
|  410130 | 6119 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 6120 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    7292 | 6121 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  406485 | 6122 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 6123 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2450 | 6124 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  401616 | 6125 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  179368 | 6126 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|   89683 | 6127 | `		}` |
|  410130 | 6128 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  410130 | 6129 | `		if( iVmOp == PH7_OP_STORE ){` |
|  176950 | 6130 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  176950 | 6131 | `			if( pInstr ){` |
|  176950 | 6132 | `				if( pInstr->iOp == PH7_OP_LOAD_LIST ){` |
|       - | 6133 | `					/* Hide the STORE instruction */` |
|      26 | 6134 | `					iVmOp = 0;` |
|  176938 | 6135 | `				}else if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 6136 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   39232 | 6137 | `					iP2 = 1;` |
|   19617 | 6138 | `				}else{` |
|  137696 | 6139 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 6140 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   23320 | 6141 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   23320 | 6142 | `						iP1 = pInstr->iP1;` |
|   11661 | 6143 | `					}else{` |
|  114378 | 6144 | `						p3 = pInstr->p3;` |
|       - | 6145 | `					}` |
|       - | 6146 | `					/* POP the last dynamic load instruction */` |
|  137696 | 6147 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 6148 | `				}` |
|   88476 | 6149 | `			}` |
|  321656 | 6150 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      44 | 6151 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      44 | 6152 | `			if( pInstr ){` |
|      44 | 6153 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 6154 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 6155 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 6156 | `					 */` |
|      15 | 6157 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 6158 | `					iP1 = pInstr->iP1;` |
|      15 | 6159 | `					iP2 = pInstr->iP2;` |
|      15 | 6160 | `					p3  = pInstr->p3;` |
|       8 | 6161 | `				}else{` |
|      30 | 6162 | `					p3 = pInstr->p3;` |
|       - | 6163 | `				}` |
|      21 | 6164 | `			}` |
|      21 | 6165 | `		}` |
|  205064 | 6166 | `	}` |
|  741704 | 6167 | `	if( iVmOp > 0 ){` |
|  741650 | 6168 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    9422 | 6169 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 6170 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    6920 | 6171 | `				iP1 = 1;` |
|    3461 | 6172 | `			}` |
|  736940 | 6173 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 6174 | `			/* Namespace-qualify the class name for NEW */ {` |
|   11798 | 6175 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   11798 | 6176 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   11786 | 6177 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    5892 | 6178 | `				}` |
|   11798 | 6179 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 6180 | `					/* Prevent constant expansion for class name */` |
|   11794 | 6181 | `					pPeek->iP1 = 0;` |
|   11794 | 6182 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pPeek->iP2);` |
|    5896 | 6183 | `				}` |
|       - | 6184 | `			}` |
|   11798 | 6185 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   11798 | 6186 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 6187 | `				VmInstr *pPrev;` |
|   11786 | 6188 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   11786 | 6189 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 6190 | `					/* Pop the call instruction */` |
|   11786 | 6191 | `					iP1 = pInstr->iP1;` |
|   11786 | 6192 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    5892 | 6193 | `				}` |
|    5894 | 6194 | `			}` |
|  726332 | 6195 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 6196 | `			/* instanceof: right operand is a class name, not a constant */` |
|      38 | 6197 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      38 | 6198 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      38 | 6199 | `				pInstr->iP1 = 0;` |
|      20 | 6200 | `			}` |
|  720416 | 6201 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 6202 | `			/* Prevent constant expansion for member/property names.` |
|       - | 6203 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 6204 | `			 * should not trigger constant lookup. */` |
|   87996 | 6205 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   87996 | 6206 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   87986 | 6207 | `				pInstr->iP1 = 0;` |
|   43992 | 6208 | `			}` |
|   87996 | 6209 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 6210 | `				/* Static member access,remember that */` |
|      53 | 6211 | `				iP1 = 1;` |
|      53 | 6212 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      53 | 6213 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|       3 | 6214 | `					p3 = pInstr->p3;` |
|       3 | 6215 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       1 | 6216 | `				}` |
|      26 | 6217 | `			}` |
|   43997 | 6218 | `		}` |
|       - | 6219 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  741650 | 6220 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  741650 | 6221 | `		if( nJmpIdx > 0 ){` |
|       - | 6222 | `			/* Fix short-circuited jumps now the destination is resolved */` |
|    9740 | 6223 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    9740 | 6224 | `			if( pInstr ){` |
|    9740 | 6225 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    4869 | 6226 | `			}` |
|    4869 | 6227 | `		}` |
|  370824 | 6228 | `	}` |
|  741704 | 6229 | `	return rc;` |
|  962186 | 6230 |  |
|       - | 6231 | `/*` |
|       - | 6232 | ` * Compile a PHP expression.` |
|       - | 6233 | ` * According to the PHP language reference manual:` |
|       - | 6234 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 6235 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 6236 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 6237 | ` *  is "anything that has a value".` |
|       - | 6238 | ` * If something goes wrong while compiling the expression,this` |
|       - | 6239 | ` * function takes care of generating the appropriate error` |
|       - | 6240 | ` * message.` |
|       - | 6241 | ` */` |
|  506016 | 6242 | `static sxi32 PH7_CompileExpr(` |
|       - | 6243 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 6244 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 6245 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 6246 | `	)` |
|       2 | 6247 |  |
|       - | 6248 | `	ph7_expr_node *pRoot;` |
|       - | 6249 | `	SySet sExprNode;` |
|       - | 6250 | `	SyToken *pEnd;` |
|       - | 6251 | `	sxi32 nExpr;` |
|       - | 6252 | `	sxi32 iNest;` |
|       - | 6253 | `	sxi32 rc;` |
|       - | 6254 | `	/* Initialize worker variables */` |
|  506018 | 6255 | `	nExpr = 0;` |
|  506018 | 6256 | `	pRoot = 0;` |
|  506018 | 6257 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  506018 | 6258 | `	SySetAlloc(&sExprNode,0x10);` |
|  506018 | 6259 | `	rc = SXRET_OK;` |
|       - | 6260 | `	/* Delimit the expression */` |
|  506018 | 6261 | `	pEnd = pGen->pIn;` |
|  506018 | 6262 | `	iNest = 0;` |
| 3467082 | 6263 | `	while( pEnd < pGen->pEnd ){` |
| 3284046 | 6264 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 6265 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     184 | 6266 | `			iNest++;` |
| 3283955 | 6267 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     192 | 6268 | `			iNest--;` |
| 3283769 | 6269 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  323120 | 6270 | `			if( iNest <= 0 ){` |
|  322982 | 6271 | `				break;` |
|       - | 6272 | `			}` |
|      69 | 6273 | `		}` |
| 2961066 | 6274 | `		pEnd++;` |
|       2 | 6275 | `	}` |
|  506018 | 6276 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    9364 | 6277 | `		SyToken *pEnd2 = pGen->pIn;` |
|    9364 | 6278 | `		iNest = 0;` |
|       - | 6279 | `		/* Stop at the first comma */` |
|   18746 | 6280 | `		while( pEnd2 < pEnd ){` |
|    9384 | 6281 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       3 | 6282 | `				iNest++;` |
|    9383 | 6283 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       3 | 6284 | `				iNest--;` |
|    9381 | 6285 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 6286 | `				if( iNest <= 0 ){` |
|     ! 0 | 6287 | `					break;` |
|       - | 6288 | `				}` |
|       2 | 6289 | `			}` |
|    9384 | 6290 | `			pEnd2++;` |
|       2 | 6291 | `		}` |
|    9364 | 6292 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 6293 | `			pEnd = pEnd2;` |
|     ! 0 | 6294 | `		}` |
|    4681 | 6295 | `	}` |
|  506018 | 6296 | `	if( pEnd > pGen->pIn ){` |
|  506010 | 6297 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 6298 | `		/* Swap delimiter */` |
|  506010 | 6299 | `		pGen->pEnd = pEnd;` |
|       - | 6300 | `		/* Try to get an expression tree */` |
|  506010 | 6301 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  506010 | 6302 | `		if( rc == SXRET_OK && pRoot ){` |
|  505848 | 6303 | `			rc = SXRET_OK;` |
|  505848 | 6304 | `			if( xTreeValidator ){` |
|       - | 6305 | `				/* Call the upper layer validator callback */` |
|   11952 | 6306 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    5975 | 6307 | `			}` |
|  505848 | 6308 | `			if( rc != SXERR_ABORT ){` |
|       - | 6309 | `				/* Generate code for the given tree */` |
|  505848 | 6310 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  252923 | 6311 | `			}` |
|  505848 | 6312 | `			nExpr = 1;` |
|  252923 | 6313 | `		}` |
|       - | 6314 | `		/* Release the whole tree */` |
|  506010 | 6315 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 6316 | `		/* Synchronize token stream */` |
|  506010 | 6317 | `		pGen->pEnd = pTmp;` |
|  506010 | 6318 | `		pGen->pIn  = pEnd;` |
|  506010 | 6319 | `		if( rc == SXERR_ABORT ){` |
|       3 | 6320 | `			SySetRelease(&sExprNode);` |
|       3 | 6321 | `			return SXERR_ABORT;` |
|       - | 6322 | `		}` |
|  253003 | 6323 | `	}` |
|  506016 | 6324 | `	SySetRelease(&sExprNode);` |
|  506016 | 6325 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  253010 | 6326 |  |
|       - | 6327 | `/*` |
|       - | 6328 | ` * Return a pointer to the node construct handler associated` |
|       - | 6329 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 6330 | ` */` |
|  139070 | 6331 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 6332 |  |
|  139072 | 6333 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 6334 | `		/* Numeric literal: Either real or integer */` |
|   75774 | 6335 | `		return PH7_CompileNumLiteral;` |
|   63300 | 6336 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 6337 | `		/* Double quoted string */` |
|   13140 | 6338 | `		return PH7_CompileString;` |
|   50162 | 6339 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 6340 | `		/* Single quoted string */` |
|   50102 | 6341 | `		return PH7_CompileSimpleString;` |
|      62 | 6342 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 6343 | `		/* Heredoc */` |
|      28 | 6344 | `		return PH7_CompileHereDoc;` |
|      36 | 6345 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 6346 | `		/* Nowdoc */` |
|      29 | 6347 | `		return PH7_CompileNowDoc;` |
|       7 | 6348 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 6349 | `		/* Backtick quoted string */` |
|       5 | 6350 | `		return PH7_CompileBacktic;` |
|       - | 6351 | `	}` |
|       3 | 6352 | `	return 0;` |
|   69537 | 6353 |  |
|       - | 6354 | `/*` |
|       - | 6355 | ` * PHP Language construct table.` |
|       - | 6356 | ` */` |
|       - | 6357 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 6358 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 6359 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 6360 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 6361 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 6362 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 6363 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 6364 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 6365 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 6366 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 6367 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 6368 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 6369 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 6370 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 6371 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 6372 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 6373 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 6374 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 6375 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 6376 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 6377 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 6378 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 6379 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 6380 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  }   /* declare statement */` |
|       - | 6381 | `};` |
|       - | 6382 | `/*` |
|       - | 6383 | ` * Return a pointer to the statement handler routine associated` |
|       - | 6384 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 6385 | ` */` |
|  290924 | 6386 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 6387 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 6388 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 6389 | `	)` |
|       2 | 6390 |  |
|  290926 | 6391 | `	sxu32 n = 0;` |
| 1101156 | 6392 | `	for(;;){` |
| 2202314 | 6393 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   30468 | 6394 | `			break;` |
|       - | 6395 | `		}` |
| 2171848 | 6396 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  260460 | 6397 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 6398 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 6399 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 6400 | `					/* 'static' (class context),return null */` |
|     ! 0 | 6401 | `					return 0;` |
|       - | 6402 | `				}` |
|     ! 0 | 6403 | `			}` |
|       - | 6404 | `			/* Return a pointer to the handler.` |
|       - | 6405 | `			*/` |
|  260460 | 6406 | `			return aLangConstruct[n].xConstruct;` |
|       - | 6407 | `		}` |
| 1911390 | 6408 | `		n++;` |
|       2 | 6409 | `	}` |
|   30468 | 6410 | `	if( pLookahed ){` |
|   30468 | 6411 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    6926 | 6412 | `			return PH7_CompileClassInterface;` |
|   23544 | 6413 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   21020 | 6414 | `			return PH7_CompileClass;` |
|    2524 | 6415 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       7 | 6416 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       6 | 6417 | `				return PH7_CompileAbstractClass;` |
|    2520 | 6418 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 6419 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 6420 | `				return PH7_CompileFinalClass;` |
|       - | 6421 | `		}` |
|    1259 | 6422 | `	}` |
|       - | 6423 | `	/* Not a language construct */` |
|    2520 | 6424 | `	return 0;` |
|  145464 | 6425 |  |
|       - | 6426 | `/*` |
|       - | 6427 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 6428 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 6429 | ` */` |
|    2518 | 6430 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 6431 |  |
|       - | 6432 | `	int rc;` |
|    2520 | 6433 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|    2520 | 6434 | `	if( rc == FALSE ){` |
|      10 | 6435 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|       - | 6436 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 6437 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 6438 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 6439 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 6440 | `			*/` |
|       - | 6441 | `			){` |
|       3 | 6442 | `				rc = TRUE;` |
|       1 | 6443 | `		}` |
|       4 | 6444 | `	}` |
|    2520 | 6445 | `	return rc;` |
|       2 | 6446 |  |
|       - | 6447 | `/*` |
|       - | 6448 | ` * Compile a PHP chunk.` |
|       - | 6449 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 6450 | ` * takes care of generating the appropriate error message.` |
|       - | 6451 | ` */` |
|  412770 | 6452 | `static sxi32 GenStateCompileChunk(` |
|       - | 6453 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 6454 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 6455 | `	)` |
|       2 | 6456 |  |
|       - | 6457 | `	ProcLangConstruct xCons;` |
|       - | 6458 | `	sxi32 rc;` |
|  412772 | 6459 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  244919 | 6460 | `	for(;;){` |
|  489840 | 6461 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6462 | `			/* No more input to process */` |
|   10414 | 6463 | `			break;` |
|       - | 6464 | `		}` |
|  479428 | 6465 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 6466 | `			/* Compile block */` |
|      12 | 6467 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 6468 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6469 | `				break;` |
|       - | 6470 | `			}` |
|       7 | 6471 | `		}else{` |
|  479418 | 6472 | `			xCons = 0;` |
|  479418 | 6473 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  290926 | 6474 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 6475 | `				/* Try to extract a language construct handler */` |
|  290926 | 6476 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  290926 | 6477 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      10 | 6478 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6479 | `						"Syntax error: Unexpected keyword '%z'",` |
|       6 | 6480 | `						&pGen->pIn->sData);` |
|       7 | 6481 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6482 | `						break;` |
|       - | 6483 | `					}` |
|       - | 6484 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 6485 | `					 * this erroneous statement.` |
|       - | 6486 | `					 */` |
|       7 | 6487 | `					xCons = PH7_ErrorRecover;` |
|       3 | 6488 | `				}` |
|  333956 | 6489 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   27238 | 6490 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 6491 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 6492 | `				xCons = PH7_CompileLabel;` |
|      56 | 6493 | `			}` |
|  479418 | 6494 | `			if( xCons == 0 ){` |
|       - | 6495 | `				/* Assume an expression an try to compile it */` |
|  190894 | 6496 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  190894 | 6497 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 6498 | `					/* Pop l-value */` |
|  190764 | 6499 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   95381 | 6500 | `				}` |
|   95448 | 6501 | `			}else{` |
|       - | 6502 | `				/* Go compile the sucker */` |
|  288526 | 6503 | `				rc = xCons(&(*pGen));` |
|       - | 6504 | `			}` |
|  479418 | 6505 | `			if( rc == SXERR_ABORT ){` |
|       - | 6506 | `				/* Request to abort compilation */` |
|       3 | 6507 | `				break;` |
|       - | 6508 | `			}` |
|       - | 6509 | `		}` |
|       - | 6510 | `		/* Ignore trailing semi-colons ';' */` |
|  786490 | 6511 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  307066 | 6512 | `			pGen->pIn++;` |
|       2 | 6513 | `		}` |
|  479426 | 6514 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 6515 | `			/* Compile a single statement and return */` |
|  402358 | 6516 | `			break;` |
|       - | 6517 | `		}` |
|       - | 6518 | `		/* LOOP ONE */` |
|       - | 6519 | `		/* LOOP TWO */` |
|       - | 6520 | `		/* LOOP THREE */` |
|       - | 6521 | `		/* LOOP FOUR */` |
|       2 | 6522 | `	}` |
|       - | 6523 | `	/* Return compilation status */` |
|  412772 | 6524 | `	return rc;` |
|       2 | 6525 |  |
|       - | 6526 | `/*` |
|       - | 6527 | ` * Compile a Raw PHP chunk.` |
|       - | 6528 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 6529 | ` * takes care of generating the appropriate error message.` |
|       - | 6530 | ` */` |
|   10416 | 6531 | `static sxi32 PH7_CompilePHP(` |
|       - | 6532 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 6533 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 6534 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 6535 | `	)` |
|       2 | 6536 |  |
|   10418 | 6537 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 6538 | `	sxi32 rc;` |
|       - | 6539 | `	/* Reset the token set */` |
|   10418 | 6540 | `	SySetReset(&(*pTokenSet));` |
|       - | 6541 | `	/* Mark as the default token set */` |
|   10418 | 6542 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 6543 | `	/* Advance the stream cursor */` |
|   10418 | 6544 | `	pGen->pRawIn++;` |
|       - | 6545 | `	/* Tokenize the PHP chunk first */` |
|   10418 | 6546 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 6547 | `	/* Point to the head and tail of the token stream. */` |
|   10418 | 6548 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   10418 | 6549 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   10418 | 6550 | `	if( is_expr ){` |
|     ! 0 | 6551 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 6552 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 6553 | `			/* A simple expression,compile it */` |
|     ! 0 | 6554 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 6555 | `		}` |
|       - | 6556 | `		/* Emit the DONE instruction */` |
|     ! 0 | 6557 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 6558 | `		return SXRET_OK;` |
|       - | 6559 | `	}` |
|   10418 | 6560 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 6561 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 6562 | `		/*` |
|       - | 6563 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 6564 | `		 * According to the PHP reference manual:` |
|       - | 6565 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 6566 | `		 *  immediately follow` |
|       - | 6567 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 6568 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 6569 | `		 * Symisc extension:` |
|       - | 6570 | `		 *   This short syntax works with all PHP opening` |
|       - | 6571 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 6572 | `		 *   only short tag.` |
|       - | 6573 | `		 */` |
|       - | 6574 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 6575 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 6576 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 6577 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 6578 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 6579 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 6580 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 6581 | `		}` |
|       3 | 6582 | `		return SXRET_OK;` |
|       - | 6583 | `	}` |
|       - | 6584 | `	/* Compile the PHP chunk */` |
|   10416 | 6585 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 6586 | `	/* Fix exceptions jumps */` |
|   10416 | 6587 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6588 | `	/* Fix gotos now, the jump destination is resolved */` |
|   10416 | 6589 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 6590 | `		rc = SXERR_ABORT;` |
|       1 | 6591 | `	}` |
|       - | 6592 | `	/* Reset container */` |
|   10416 | 6593 | `	SySetReset(&pGen->aGoto);` |
|   10416 | 6594 | `	SySetReset(&pGen->aLabel);` |
|       - | 6595 | `	/* Compilation result */` |
|   10416 | 6596 | `	return rc;` |
|    5210 | 6597 |  |
|       - | 6598 | `/*` |
|       - | 6599 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 6600 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 6601 | ` * This is the only compile interface exported from this file.` |
|       - | 6602 | ` */` |
|   12156 | 6603 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 6604 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 6605 | `	SyString *pScript,  /* Script to compile */` |
|       - | 6606 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 6607 | `	)` |
|       2 | 6608 |  |
|       - | 6609 | `	SySet aPhpToken,aRawToken;` |
|       - | 6610 | `	ph7_gen_state *pCodeGen;` |
|       - | 6611 | `	ph7_value *pRawObj;` |
|       - | 6612 | `	sxu32 nObjIdx;` |
|       - | 6613 | `	sxi32 nRawObj;` |
|       - | 6614 | `	int is_expr;` |
|       - | 6615 | `	sxi32 rc;` |
|   12158 | 6616 | `	if( pScript->nByte < 1 ){` |
|       - | 6617 | `		/* Nothing to compile */` |
|     ! 0 | 6618 | `		return PH7_OK;` |
|       - | 6619 | `	}` |
|       - | 6620 | `	/* Initialize the tokens containers */` |
|   12158 | 6621 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   12158 | 6622 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   12158 | 6623 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   12158 | 6624 | `	is_expr = 0;` |
|   12158 | 6625 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 6626 | `		SyToken sTmp;` |
|       - | 6627 | `		/* PHP only: -*/` |
|    2328 | 6628 | `		sTmp.nLine = 1;` |
|    2328 | 6629 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2328 | 6630 | `		sTmp.pUserData = 0;` |
|    2328 | 6631 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2328 | 6632 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2328 | 6633 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 6634 | `			/* A simple PHP expression */` |
|     ! 0 | 6635 | `			is_expr = 1;` |
|     ! 0 | 6636 | `		}` |
|    1165 | 6637 | `	}else{` |
|       - | 6638 | `		/* Tokenize raw text */` |
|    9832 | 6639 | `		SySetAlloc(&aRawToken,32);` |
|    9832 | 6640 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 6641 | `	}` |
|   12158 | 6642 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 6643 | `	/* Process high-level tokens */` |
|   12158 | 6644 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   12158 | 6645 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   12158 | 6646 | `	rc = PH7_OK;` |
|   12158 | 6647 | `	if( is_expr ){` |
|       - | 6648 | `		/* Compile the expression */` |
|     ! 0 | 6649 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 6650 | `		goto cleanup;` |
|       - | 6651 | `	}` |
|   12158 | 6652 | `	nObjIdx = 0;` |
|       - | 6653 | `	/* Start the compilation process */` |
|   10997 | 6654 | `	for(;;){` |
|   32408 | 6655 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   12154 | 6656 | `			break; /* No more tokens to process */` |
|       - | 6657 | `		}` |
|   20256 | 6658 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 6659 | `			/* Compile the PHP chunk */` |
|   10418 | 6660 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   10418 | 6661 | `			if( rc == SXERR_ABORT ){` |
|       5 | 6662 | `				break;` |
|       - | 6663 | `			}` |
|   10414 | 6664 | `			continue;` |
|       - | 6665 | `		}` |
|       - | 6666 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    9840 | 6667 | `		nRawObj = 0;` |
|   19678 | 6668 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 6669 | `			/* Consume the raw chunk without any processing */` |
|    9840 | 6670 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    9840 | 6671 | `			if( pRawObj == 0 ){` |
|     ! 0 | 6672 | `				rc = SXERR_MEM;` |
|     ! 0 | 6673 | `				break;` |
|       - | 6674 | `			}` |
|       - | 6675 | `			/* Mark as constant and emit the load constant instruction */` |
|    9840 | 6676 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    9840 | 6677 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    9840 | 6678 | `			++nRawObj;` |
|    9840 | 6679 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 6680 | `		}` |
|    9840 | 6681 | `		if( nRawObj > 0 ){` |
|       - | 6682 | `			/* Emit the consume instruction */` |
|    9840 | 6683 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    4919 | 6684 | `		}` |
|    6080 | 6685 | `	}` |
|    6078 | 6686 | `cleanup:` |
|   12158 | 6687 | `	SySetRelease(&aRawToken);` |
|   12158 | 6688 | `	SySetRelease(&aPhpToken);` |
|   12158 | 6689 | `	return rc;` |
|    6080 | 6690 |  |
|       - | 6691 | `/*` |
|       - | 6692 | ` * Utility routines.Initialize the code generator.` |
|       - | 6693 | ` */` |
|    2304 | 6694 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 6695 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 6696 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 6697 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 6698 | `	)` |
|       2 | 6699 |  |
|    2306 | 6700 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 6701 | `	/* Zero the structure */` |
|    2306 | 6702 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 6703 | `	/* Initial state */` |
|    2306 | 6704 | `	pGen->pVm  = &(*pVm);` |
|    2306 | 6705 | `	pGen->xErr = xErr;` |
|    2306 | 6706 | `	pGen->pErrData = pErrData;` |
|    2306 | 6707 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2306 | 6708 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2306 | 6709 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2306 | 6710 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 6711 | `	/* Error log buffer */` |
|    2306 | 6712 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 6713 | `	/* General purpose working buffer */` |
|    2306 | 6714 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 6715 | `	/* Namespace state */` |
|    2306 | 6716 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2306 | 6717 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 6718 | `	/* Create the global scope */` |
|    2306 | 6719 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 6720 | `	/* Point to the global scope */` |
|    2306 | 6721 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2306 | 6722 | `	return SXRET_OK;` |
|       2 | 6723 |  |
|       - | 6724 | `/*` |
|       - | 6725 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 6726 | ` */` |
|   14228 | 6727 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 6728 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 6729 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 6730 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 6731 | `	)` |
|       2 | 6732 |  |
|   14230 | 6733 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 6734 | `	GenBlock *pBlock,*pParent;` |
|       - | 6735 | `	/* Reset state */` |
|   14230 | 6736 | `	SySetReset(&pGen->aLabel);` |
|   14230 | 6737 | `	SySetReset(&pGen->aGoto);` |
|   14230 | 6738 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   14230 | 6739 | `	SyBlobRelease(&pGen->sWorker);` |
|   14230 | 6740 | `	SyBlobRelease(&pGen->sNamespace);` |
|   14230 | 6741 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   14230 | 6742 | `	SyHashRelease(&pGen->hUseImports);` |
|   14230 | 6743 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 6744 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 6745 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 6746 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 6747 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 6748 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 6749 | `	 * number of unique names, which is acceptable. */` |
|       - | 6750 | `	/* Point to the global scope */` |
|   14230 | 6751 | `	pBlock = pGen->pCurrent;` |
|   14230 | 6752 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 6753 | `		pParent = pBlock->pParent;` |
|     ! 0 | 6754 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 6755 | `		pBlock = pParent;` |
|     ! 0 | 6756 | `	}` |
|   14230 | 6757 | `	pGen->xErr = xErr;` |
|   14230 | 6758 | `	pGen->pErrData = pErrData;` |
|   14230 | 6759 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   14230 | 6760 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   14230 | 6761 | `	pGen->pIn = pGen->pEnd = 0;` |
|   14230 | 6762 | `	pGen->nErr = 0;` |
|   14230 | 6763 | `	return SXRET_OK;` |
|       2 | 6764 |  |
|       - | 6765 | `/*` |
|       - | 6766 | ` * Generate a compile-time error message.` |
|       - | 6767 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 6768 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 6769 | ` * abort compilation immediately.` |
|       - | 6770 | ` */` |
|     430 | 6771 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 6772 |  |
|     432 | 6773 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     432 | 6774 | `	const char *zErr = "Error";` |
|       - | 6775 | `	SyString *pFile;` |
|       - | 6776 | `	va_list ap;` |
|       - | 6777 | `	sxi32 rc;` |
|       - | 6778 | `	/* Reset the working buffer */` |
|     432 | 6779 | `	SyBlobReset(pWorker);` |
|       - | 6780 | `	/* Peek the processed file path if available */` |
|     432 | 6781 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     432 | 6782 | `	if( nErrType == E_ERROR ){` |
|       - | 6783 | `		/* Increment the error counter */` |
|     388 | 6784 | `		pGen->nErr++;` |
|     388 | 6785 | `		if( pGen->nErr > 15 ){` |
|       - | 6786 | `			/* Error count limit reached */` |
|       5 | 6787 | `			if( pGen->xErr ){` |
|       5 | 6788 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 6789 | `				SyBlobFormat(pWorker,"Error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 6790 | `				if( pFile ){` |
|       5 | 6791 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 6792 | `				}` |
|       5 | 6793 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 6794 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 6795 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 6796 | `				}` |
|       2 | 6797 | `			}` |
|       - | 6798 | `			/* Abort immediately */` |
|       5 | 6799 | `			return SXERR_ABORT;` |
|       - | 6800 | `		}` |
|     191 | 6801 | `	}` |
|     428 | 6802 | `	if( pGen->xErr == 0 ){` |
|       - | 6803 | `		/* No available error consumer,return immediately */` |
|       3 | 6804 | `		return SXRET_OK;` |
|       - | 6805 | `	}` |
|     425 | 6806 | `	switch(nErrType){` |
|      31 | 6807 | `	case E_WARNING: zErr = "Warning";     break;` |
|       7 | 6808 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 6809 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 6810 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 6811 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 6812 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     190 | 6813 | `	default:` |
|     380 | 6814 | `		break;` |
|       - | 6815 | `	}` |
|     425 | 6816 | `	rc = SXRET_OK;` |
|       - | 6817 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     425 | 6818 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     425 | 6819 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     425 | 6820 | `	va_start(ap,zFormat);` |
|     425 | 6821 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     425 | 6822 | `	va_end(ap);` |
|     425 | 6823 | `	if( pFile ){` |
|     425 | 6824 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     212 | 6825 | `	}` |
|       - | 6826 | `	/* Append a new line */` |
|     425 | 6827 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     425 | 6828 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 6829 | `		/* Consume the generated error message */` |
|     425 | 6830 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     212 | 6831 | `	}` |
|     425 | 6832 | `	return rc;` |
|     217 | 6833 |  |
|       - | 6834 |  |
