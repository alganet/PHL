# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3613/4718 lines (76.58%)

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
|    2834 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    2836 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    7945 |  131 | `	for(;;){` |
|   15892 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2724 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2724 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2702 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      11 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   13192 |  140 | `		pBlock = pBlock->pParent;` |
|   13192 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1419 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  549936 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  549938 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  549938 |  162 | `	pBlock->pUserData   = pUserData;` |
|  549938 |  163 | `	pBlock->pGen        = pGen;` |
|  549938 |  164 | `	pBlock->iFlags      = iType;` |
|  549938 |  165 | `	pBlock->pParent     = 0;` |
|  549938 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  549938 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  549938 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  547360 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  547362 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  547362 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  547362 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  547362 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  547362 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  547362 |  200 | `	pGen->pCurrent = pBlock;` |
|  547362 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  264878 |  203 | `		*ppBlock = pBlock;` |
|  132438 |  204 | `	}` |
|  547362 |  205 | `	return SXRET_OK;` |
|  273682 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  547352 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  547354 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  547354 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  547354 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  547352 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  547354 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  547354 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  547354 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  547354 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  547352 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  547354 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  547354 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  547354 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  547354 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  547354 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  547354 |  244 | `	return SXRET_OK;` |
|  273678 |  245 |  |
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
|  166956 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  166958 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  166958 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  166958 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  166958 |  265 | `	return rc;` |
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
|  389906 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  389908 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  715296 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  325390 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  126740 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  198652 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   31698 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  166956 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  166956 |  298 | `		if( pInstr ){` |
|  166956 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  166956 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  166956 |  302 | `			aFix[n].nJumpType = -1;` |
|   83477 |  303 | `		}` |
|   83479 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  389908 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|  148802 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|  148804 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|  148950 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  148802 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  148934 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|  148802 |  358 | `	return SXRET_OK;` |
|   74403 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  484668 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  484670 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  484670 |  367 | `	if( pEntry == 0 ){` |
|  238880 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  245792 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  245792 |  371 | `	return SXRET_OK;` |
|  242336 |  372 |  |
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
|  238878 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  238880 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  238880 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  119439 |  387 | `	}` |
|  238880 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   84678 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   84680 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   84680 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   84680 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   84680 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   84680 |  408 | `	return pObj;` |
|   42341 |  409 |  |
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
|   85080 |  431 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  432 |  |
|   85082 |  433 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   85082 |  434 | `	sxu32 nIdx = 0;` |
|   85082 |  435 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  436 | `		ph7_value *pObj;` |
|       - |  437 | `		sxi64 iValue;` |
|   84680 |  438 | `		iValue = PH7_TokenValueToInt64(&pToken->sData);` |
|   84680 |  439 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   84680 |  440 | `		if( pObj == 0 ){` |
|     ! 0 |  441 | `			SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  442 | `			return SXERR_ABORT;` |
|       - |  443 | `		}` |
|   84680 |  444 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   42341 |  445 | `	}else{` |
|       - |  446 | `		/* Real number */` |
|       - |  447 | `		ph7_value *pObj;` |
|       - |  448 | `		/* Reserve a new constant */` |
|     404 |  449 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     404 |  450 | `		if( pObj == 0 ){` |
|     ! 0 |  451 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  452 | `			return SXERR_ABORT;` |
|       - |  453 | `		}` |
|     404 |  454 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&pToken->sData);` |
|     404 |  455 | `		PH7_MemObjToReal(pObj);` |
|       - |  456 | `	}` |
|       - |  457 | `	/* Emit the load constant instruction */` |
|   85082 |  458 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  459 | `	/* Node successfully compiled */` |
|   85082 |  460 | `	return SXRET_OK;` |
|   42542 |  461 |  |
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
|   55774 |  473 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  474 |  |
|   55776 |  475 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  476 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  477 | `	ph7_value *pObj;` |
|       - |  478 | `	sxu32 nIdx;` |
|   55776 |  479 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  480 | `	/* Delimit the string */` |
|   55776 |  481 | `	zIn  = pStr->zString;` |
|   55776 |  482 | `	zEnd = &zIn[pStr->nByte];` |
|   55776 |  483 | `	if( zIn >= zEnd ){` |
|       - |  484 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  485 | `		 * rather than reserving a new object each time. */` |
|     138 |  486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     138 |  487 | `		return SXRET_OK;` |
|       - |  488 | `	}` |
|   55640 |  489 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  490 | `		/* Already processed,emit the load constant instruction` |
|       - |  491 | `		 * and return.` |
|       - |  492 | `		 */` |
|   16532 |  493 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   16532 |  494 | `		return SXRET_OK;` |
|       - |  495 | `	}` |
|       - |  496 | `	/* Reserve a new constant */` |
|   39110 |  497 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   39110 |  498 | `	if( pObj == 0 ){` |
|     ! 0 |  499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  500 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  501 | `		return SXERR_ABORT;` |
|       - |  502 | `	}` |
|   39110 |  503 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  504 | `	/* Compile the node */` |
|   39149 |  505 | `	for(;;){` |
|   78300 |  506 | `		if( zIn >= zEnd ){` |
|       - |  507 | `			/* End of input */` |
|   39110 |  508 | `			break;` |
|       - |  509 | `		}` |
|   39192 |  510 | `		zCur = zIn;` |
|  622114 |  511 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  582924 |  512 | `			zIn++;` |
|       2 |  513 | `		}` |
|   39192 |  514 | `		if( zIn > zCur ){` |
|       - |  515 | `			/* Append raw contents*/` |
|   39172 |  516 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   19585 |  517 | `		}` |
|   39192 |  518 | `		zIn++;` |
|   39192 |  519 | `		if( zIn < zEnd ){` |
|     103 |  520 | `			if( zIn[0] == '\\' ){` |
|       - |  521 | `				/* A literal backslash */` |
|      23 |  522 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      92 |  523 | `			}else if( zIn[0] == '\'' ){` |
|       - |  524 | `				/* A single quote */` |
|      11 |  525 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |  526 | `			}else{` |
|       - |  527 | `				/* verbatim copy */` |
|      71 |  528 | `				zIn--;` |
|      71 |  529 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      71 |  530 | `				zIn++;` |
|       - |  531 | `			}` |
|      51 |  532 | `		}` |
|       - |  533 | `		/* Advance the stream cursor */` |
|   39192 |  534 | `		zIn++;` |
|       2 |  535 | `	}` |
|       - |  536 | `	/* Emit the load constant instruction */` |
|   39110 |  537 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   39110 |  538 | `	if( pStr->nByte < 1024 ){` |
|       - |  539 | `		/* Install in the literal table */` |
|   39110 |  540 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   19554 |  541 | `	}` |
|       - |  542 | `	/* Node successfully compiled */` |
|   39110 |  543 | `	return SXRET_OK;` |
|   27889 |  544 |  |
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
|    1650 |  606 | `static sxi32 GenStateProcessStringExpression(` |
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
|    1652 |  617 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  618 | `	/* Preallocate some slots */` |
|    1652 |  619 | `	SySetAlloc(&sToken,0x08);` |
|       - |  620 | `	/* Tokenize the text */` |
|    1652 |  621 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  622 | `	/* Swap delimiter */` |
|    1652 |  623 | `	pTmpIn  = pGen->pIn;` |
|    1652 |  624 | `	pTmpEnd = pGen->pEnd;` |
|    1652 |  625 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1652 |  626 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  627 | `	/* Compile the expression */` |
|    1652 |  628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  629 | `	/* Restore token stream */` |
|    1652 |  630 | `	pGen->pIn  = pTmpIn;` |
|    1652 |  631 | `	pGen->pEnd = pTmpEnd;` |
|       - |  632 | `	/* Release the token set */` |
|    1652 |  633 | `	SySetRelease(&sToken);` |
|       - |  634 | `	/* Compilation result */` |
|    1652 |  635 | `	return rc;` |
|       2 |  636 |  |
|       - |  637 | `/*` |
|       - |  638 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  639 | ` */` |
|   16066 |  640 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  641 |  |
|       - |  642 | `	ph7_value *pConstObj;` |
|   16068 |  643 | `	sxu32 nIdx = 0;` |
|       - |  644 | `	/* Reserve a new constant */` |
|   16068 |  645 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   16068 |  646 | `	if( pConstObj == 0 ){` |
|     ! 0 |  647 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  648 | `		return 0;` |
|       - |  649 | `	}` |
|   16068 |  650 | `	(*pCount)++;` |
|   16068 |  651 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  652 | `	/* Emit the load constant instruction */` |
|   16068 |  653 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   16068 |  654 | `	return pConstObj;` |
|    8035 |  655 |  |
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
|   14894 |  694 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  695 |  |
|   14896 |  696 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  697 | `	const char *zIn,*zCur,*zEnd;` |
|   14896 |  698 | `	ph7_value *pObj = 0;` |
|       - |  699 | `	sxi32 iCons;` |
|       - |  700 | `	sxi32 rc;` |
|       - |  701 | `	/* Delimit the string */` |
|   14896 |  702 | `	zIn  = pStr->zString;` |
|   14896 |  703 | `	zEnd = &zIn[pStr->nByte];` |
|   14896 |  704 | `	if( zIn >= zEnd ){` |
|       - |  705 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  706 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  707 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  708 | `		 */` |
|     224 |  709 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     224 |  710 | `		return SXRET_OK;` |
|       - |  711 | `	}` |
|   14674 |  712 | `	zCur = 0;` |
|       - |  713 | `	/* Compile the node */` |
|   14674 |  714 | `	iCons = 0;` |
|    8161 |  715 | `	for(;;){` |
|   24652 |  716 | `		zCur = zIn;` |
|  135494 |  717 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  112494 |  718 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      44 |  719 | `				break;` |
|  112410 |  720 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1568 |  721 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     784 |  722 | `					break;` |
|       - |  723 | `			}` |
|  110844 |  724 | `			zIn++;` |
|       2 |  725 | `		}` |
|   24652 |  726 | `		if( zIn > zCur ){` |
|   11606 |  727 | `			if( pObj == 0 ){` |
|   11330 |  728 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   11330 |  729 | `				if( pObj == 0 ){` |
|     ! 0 |  730 | `					return SXERR_ABORT;` |
|       - |  731 | `				}` |
|    5664 |  732 | `			}` |
|   11606 |  733 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    5802 |  734 | `		}` |
|   24652 |  735 | `		if( zIn >= zEnd ){` |
|   14674 |  736 | `			break;` |
|       - |  737 | `		}` |
|    9980 |  738 | `		if( zIn[0] == '\\' ){` |
|    8330 |  739 | `			const char *zPtr = 0;` |
|       - |  740 | `			sxu32 n;` |
|    8330 |  741 | `			zIn++;` |
|    8330 |  742 | `			if( zIn >= zEnd ){` |
|     ! 0 |  743 | `				break;` |
|       - |  744 | `			}` |
|    8330 |  745 | `			if( pObj == 0 ){` |
|    4740 |  746 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    4740 |  747 | `				if( pObj == 0 ){` |
|     ! 0 |  748 | `					return SXERR_ABORT;` |
|       - |  749 | `				}` |
|    2369 |  750 | `			}` |
|    8330 |  751 | `			n = sizeof(char); /* size of conversion */` |
|    8330 |  752 | `			switch( zIn[0] ){` |
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
|    3799 |  773 | `			case 'n':` |
|       - |  774 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    7600 |  775 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    7600 |  776 | `				break;` |
|      19 |  777 | `			case 'r':` |
|       - |  778 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      40 |  779 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      40 |  780 | `				break;` |
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
|    8330 |  844 | `			zIn += n;` |
|    8330 |  845 | `			continue;` |
|       - |  846 | `		}` |
|    1652 |  847 | `		if( zIn[0] == '{' ){` |
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
|    1566 |  881 | `			const char *zExpr = zIn;` |
|       - |  882 | `			/* Assemble variable name */` |
|     782 |  883 | `			for(;;){` |
|       - |  884 | `				/* Jump leading dollars */` |
|    3130 |  885 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1566 |  886 | `					zIn++;` |
|       2 |  887 | `				}` |
|     782 |  888 | `				for(;;){` |
|    9908 |  889 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7562 |  890 | `						zIn++;` |
|       2 |  891 | `					}` |
|    1566 |  892 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  893 | `						/* UTF-8 stream */` |
|     ! 0 |  894 | `						zIn++;` |
|     ! 0 |  895 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  896 | `							zIn++;` |
|     ! 0 |  897 | `						}` |
|     ! 0 |  898 | `						continue;` |
|       - |  899 | `					}` |
|    1566 |  900 | `					break;` |
|     ! 0 |  901 | `				}` |
|    1566 |  902 | `				if( zIn >= zEnd ){` |
|      96 |  903 | `					break;` |
|       - |  904 | `				}` |
|    1472 |  905 | `				if( zIn[0] == '[' ){` |
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
|    1464 |  923 | `				}else if(zIn[0] == '{' ){` |
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
|    1460 |  941 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  942 | `					/* Member access operator '->' */` |
|     ! 0 |  943 | `					zIn += 2;` |
|    1460 |  944 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  945 | `					/* Static member access operator '::' */` |
|     ! 0 |  946 | `					zIn += 2;` |
|     ! 0 |  947 | `				}else{` |
|     731 |  948 | `					break;` |
|       - |  949 | `				}` |
|     ! 0 |  950 | `			}` |
|       - |  951 | `			/* Process the expression */` |
|    1566 |  952 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1566 |  953 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  954 | `				return SXERR_ABORT;` |
|       - |  955 | `			}` |
|    1566 |  956 | `			if( rc != SXERR_EMPTY ){` |
|    1564 |  957 | `				++iCons;` |
|     781 |  958 | `			}` |
|       - |  959 | `		}` |
|       - |  960 | `		/* Invalidate the previously used constant */` |
|    1652 |  961 | `		pObj = 0;` |
|       2 |  962 | `	}/*for(;;)*/` |
|   14674 |  963 | `	if( iCons > 1 ){` |
|       - |  964 | `		/* Concatenate all compiled constants */` |
|    1260 |  965 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     629 |  966 | `	}` |
|       - |  967 | `	/* Node successfully compiled */` |
|   14674 |  968 | `	return SXRET_OK;` |
|    7449 |  969 |  |
|       - |  970 | `/*` |
|       - |  971 | ` * Compile a double quoted string.` |
|       - |  972 | ` *  See the block-comment above for more information.` |
|       - |  973 | ` */` |
|   14868 |  974 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  975 |  |
|       - |  976 | `	sxi32 rc;` |
|   14870 |  977 | `	rc = GenStateCompileString(&(*pGen));` |
|    7434 |  978 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  979 | `	/* Compilation result */` |
|   14870 |  980 | `	return rc;` |
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
|   15374 | 1012 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   15376 | 1023 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1024 | `	/* Compile the expression*/` |
|   15376 | 1025 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1026 | `	/* Restore token stream */` |
|   15376 | 1027 | `	RE_SWAP_DELIMITER(pGen);` |
|   15376 | 1028 | `	return rc;` |
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
|   22538 | 1067 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 | 1068 |  |
|       - | 1069 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1070 | `	SyToken *pKey,*pCur;` |
|   22540 | 1071 | `	sxi32 iEmitRef = 0;` |
|   22540 | 1072 | `	sxi32 nPair = 0;` |
|       - | 1073 | `	sxi32 iNest;` |
|       - | 1074 | `	sxi32 rc;` |
|   22540 | 1075 | `	xValidator = 0;` |
|   18334 | 1076 | `	for(;;){` |
|       - | 1077 | `		/* Jump leading commas */` |
|   41478 | 1078 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    4810 | 1079 | `			pGen->pIn++;` |
|       2 | 1080 | `		}` |
|   36670 | 1081 | `		pCur = pGen->pIn;` |
|   36670 | 1082 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1083 | `			/* No more entry to process */` |
|   22528 | 1084 | `			break;` |
|       - | 1085 | `		}` |
|   14144 | 1086 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1087 | `			continue;` |
|       - | 1088 | `		}` |
|       - | 1089 | `		/* Compile the key if available */` |
|   14144 | 1090 | `		pKey = pCur;` |
|   14144 | 1091 | `		iNest = 0;` |
|   39198 | 1092 | `		while( pCur < pGen->pIn ){` |
|   26246 | 1093 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1192 | 1094 | `				break;` |
|       - | 1095 | `			}` |
|   25056 | 1096 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      78 | 1097 | `				iNest++;` |
|   25018 | 1098 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1099 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1100 | `				 * parser will shortly detect any syntax error.` |
|       - | 1101 | `				 */` |
|      78 | 1102 | `				iNest--;` |
|      38 | 1103 | `			}` |
|   25056 | 1104 | `			pCur++;` |
|       2 | 1105 | `		}` |
|   14144 | 1106 | `		rc = SXERR_EMPTY;` |
|   14144 | 1107 | `		if( pCur < pGen->pIn ){` |
|    1192 | 1108 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - | 1109 | `				/* Missing value */` |
|      11 | 1110 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 | 1111 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1112 | `					return SXERR_ABORT;` |
|       - | 1113 | `				}` |
|      11 | 1114 | `				return SXRET_OK;` |
|       - | 1115 | `			}` |
|       - | 1116 | `			/* Compile the expression holding the key */` |
|    1182 | 1117 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - | 1118 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1182 | 1119 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1120 | `				return SXERR_ABORT;` |
|       - | 1121 | `			}` |
|    1182 | 1122 | `			pCur++; /* Jump the '=>' operator */` |
|   13544 | 1123 | `		}else if( pKey == pCur ){` |
|       - | 1124 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1125 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1126 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1127 | `		}else{` |
|       - | 1128 | `			/* Reset back the cursor and point to the entry value */` |
|   12954 | 1129 | `			pCur = pKey;` |
|       - | 1130 | `		}` |
|   14134 | 1131 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1132 | `			/* No available key,load NULL */` |
|   12956 | 1133 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    6477 | 1134 | `		}` |
|   14134 | 1135 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   14132 | 1150 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   14132 | 1151 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1152 | `			return SXERR_ABORT;` |
|       - | 1153 | `		}` |
|   14132 | 1154 | `		if( iEmitRef ){` |
|       - | 1155 | `			/* Emit the load reference instruction */` |
|      32 | 1156 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1157 | `		}` |
|   14132 | 1158 | `		xValidator = 0;` |
|   14132 | 1159 | `		iEmitRef = 0;` |
|   14132 | 1160 | `		nPair++;` |
|       2 | 1161 | `	}` |
|       - | 1162 | `	/* Emit the load map instruction */` |
|   22528 | 1163 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1164 | `	/* Node successfully compiled */` |
|   22528 | 1165 | `	return SXRET_OK;` |
|   11271 | 1166 |  |
|       - | 1167 | `/*` |
|       - | 1168 | ` * Compile the 'array' language construct.` |
|       - | 1169 | ` *	 According to the PHP language reference manual` |
|       - | 1170 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1171 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1172 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1173 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1174 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1175 | ` */` |
|   22362 | 1176 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1177 |  |
|       - | 1178 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   22364 | 1179 | `	pGen->pIn += 2;` |
|   22364 | 1180 | `	pGen->pEnd--;` |
|   11181 | 1181 | `	SXUNUSED(iCompileFlag);` |
|   22364 | 1182 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1183 |  |
|       - | 1184 | `/*` |
|       - | 1185 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - | 1186 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - | 1187 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - | 1188 | ` */` |
|     176 | 1189 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1190 |  |
|       - | 1191 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     178 | 1192 | `	pGen->pIn++;` |
|     178 | 1193 | `	pGen->pEnd--;` |
|      88 | 1194 | `	SXUNUSED(iCompileFlag);` |
|     178 | 1195 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1196 |  |
|       - | 1197 | `/*` |
|       - | 1198 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - | 1199 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1200 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1201 | ` * error message.` |
|       - | 1202 | ` * See the routine responible of compiling the list language construct` |
|       - | 1203 | ` * for more inforation.` |
|       - | 1204 | ` */` |
|      64 | 1205 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1206 |  |
|      66 | 1207 | `	sxi32 rc = SXRET_OK;` |
|      66 | 1208 | `	if( pRoot->pOp ){` |
|     ! 0 | 1209 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 | 1210 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - | 1211 | `				/* Unexpected expression */` |
|     ! 0 | 1212 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1213 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 | 1214 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 | 1215 | `					rc = SXERR_INVALID;` |
|     ! 0 | 1216 | `				}` |
|     ! 0 | 1217 | `		}` |
|      66 | 1218 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1219 | `		/* Unexpected expression */` |
|       3 | 1220 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1221 | `			"list(): Expecting a variable not an expression");` |
|       3 | 1222 | `		if( rc != SXERR_ABORT ){` |
|       3 | 1223 | `			rc = SXERR_INVALID;` |
|       1 | 1224 | `		}` |
|       1 | 1225 | `	}` |
|      66 | 1226 | `	return rc;` |
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
|       - | 1242 | `/* Nested list entry recorded during first pass of PH7_CompileList */` |
|       - | 1243 | `struct NestedListEntry {` |
|       - | 1244 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - | 1245 | `	SyToken *pStart;     /* Token range: 'list' keyword */` |
|       - | 1246 | `	SyToken *pEnd;       /* Token range: past closing ')' */` |
|       - | 1247 | `};` |
|      32 | 1248 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1249 |  |
|       - | 1250 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - | 1251 | `	SyToken *pNext;` |
|       - | 1252 | `	sxi32 nExpr;` |
|       - | 1253 | `	sxi32 rc;` |
|      34 | 1254 | `	nExpr = 0;` |
|      34 | 1255 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|       - | 1256 | `	/* Jump the 'list' keyword,the leading left parenthesis and the trailing parenthesis */` |
|      34 | 1257 | `	pGen->pIn += 2;` |
|      34 | 1258 | `	pGen->pEnd--;` |
|      16 | 1259 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|     104 | 1260 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      72 | 1261 | `		if( pGen->pIn < pNext ){` |
|       - | 1262 | `			/* Check for nested list() */` |
|      68 | 1263 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 | 1264 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 1265 | `				/* Record this nested list for post-processing */` |
|       3 | 1266 | `				SyToken *pListEnd = 0;` |
|       3 | 1267 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 | 1268 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 | 1269 | `				}` |
|       3 | 1270 | `				if( pListEnd ){` |
|       - | 1271 | `					struct NestedListEntry sEntry;` |
|       3 | 1272 | `					sEntry.nIndex = nExpr;` |
|       3 | 1273 | `					sEntry.pStart = pGen->pIn;` |
|       3 | 1274 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 | 1275 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 | 1276 | `				}` |
|       - | 1277 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 | 1278 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1279 | `			}else{` |
|       - | 1280 | `				/* Compile the expression holding the variable */` |
|      66 | 1281 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      66 | 1282 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 1283 | `					return SXRET_OK;` |
|       - | 1284 | `				}` |
|       - | 1285 | `			}` |
|      35 | 1286 | `		}else{` |
|       - | 1287 | `			/* Empty entry,load NULL */` |
|       5 | 1288 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - | 1289 | `		}` |
|      72 | 1290 | `		nExpr++;` |
|       - | 1291 | `		/* Advance the stream cursor */` |
|      72 | 1292 | `		pGen->pIn = &pNext[1];` |
|       2 | 1293 | `	}` |
|       - | 1294 | `	/* Emit the LOAD_LIST instruction */` |
|      34 | 1295 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - | 1296 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - | 1297 | `	 * For each nested list() entry, emit code to extract the sub-array` |
|       - | 1298 | `	 * at the corresponding index and recursively destructure it.` |
|       - | 1299 | `	 */` |
|      34 | 1300 | `	if( SySetUsed(&sNested) > 0 ){` |
|       3 | 1301 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - | 1302 | `		sxu32 i;` |
|       5 | 1303 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|       3 | 1304 | `			SyToken *pSavedIn = pGen->pIn;` |
|       3 | 1305 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 1306 | `			ph7_value *pIdx;` |
|       - | 1307 | `			sxu32 nConstIdx;` |
|       - | 1308 | `			/* DUP the source array (it's on stack top) */` |
|       3 | 1309 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - | 1310 | `			/* Push the integer index for this nested entry */` |
|       3 | 1311 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|       3 | 1312 | `			if( pIdx == 0 ){` |
|     ! 0 | 1313 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1314 | `				SySetRelease(&sNested);` |
|     ! 0 | 1315 | `				return SXERR_ABORT;` |
|       - | 1316 | `			}` |
|       3 | 1317 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|       3 | 1318 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - | 1319 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index] */` |
|       3 | 1320 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,0,0,0);` |
|       - | 1321 | `			/* Recursively compile the inner list() */` |
|       3 | 1322 | `			pGen->pIn = apNested[i].pStart;` |
|       3 | 1323 | `			pGen->pEnd = apNested[i].pEnd;` |
|       3 | 1324 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       3 | 1325 | `			pGen->pIn = pSavedIn;` |
|       3 | 1326 | `			pGen->pEnd = pSavedEnd;` |
|       3 | 1327 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1328 | `				SySetRelease(&sNested);` |
|     ! 0 | 1329 | `				return SXERR_ABORT;` |
|       - | 1330 | `			}` |
|       - | 1331 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|       3 | 1332 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 1333 | `		}` |
|       1 | 1334 | `	}` |
|      34 | 1335 | `	SySetRelease(&sNested);` |
|       - | 1336 | `	/* Node successfully compiled */` |
|      34 | 1337 | `	return SXRET_OK;` |
|      18 | 1338 |  |
|       - | 1339 | `/* Forward declarations */` |
|       - | 1340 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - | 1341 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - | 1342 | `/*` |
|       - | 1343 | ` * Compile an annoynmous function or a closure.` |
|       - | 1344 | ` * According to the PHP language reference` |
|       - | 1345 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - | 1346 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - | 1347 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - | 1348 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - | 1349 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - | 1350 | ` *  Example Anonymous function variable assignment example` |
|       - | 1351 | ` * <?php` |
|       - | 1352 | ` * $greet = function($name)` |
|       - | 1353 | ` * {` |
|       - | 1354 | ` *    printf("Hello %s\r\n", $name);` |
|       - | 1355 | ` * };` |
|       - | 1356 | ` * $greet('World');` |
|       - | 1357 | ` * $greet('PHP');` |
|       - | 1358 | ` * ?>` |
|       - | 1359 | ` * Note that the implementation of annoynmous function and closure under` |
|       - | 1360 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - | 1361 | ` */` |
|     166 | 1362 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1363 |  |
|       - | 1364 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - | 1365 | `	char zName[512];         /* Unique lambda name */` |
|       - | 1366 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - | 1367 | `							  * one thread is allowed to compile the script.` |
|       - | 1368 | `						      */` |
|       - | 1369 | `	ph7_value *pObj;` |
|       - | 1370 | `	SyString sName;` |
|       - | 1371 | `	sxu32 nIdx;` |
|       - | 1372 | `	sxu32 nLen;` |
|       - | 1373 | `	sxi32 rc;` |
|       - | 1374 |  |
|     168 | 1375 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     168 | 1376 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 | 1377 | `		pGen->pIn++;` |
|     ! 0 | 1378 | `	}` |
|       - | 1379 | `	/* Reserve a constant for the lambda */` |
|     168 | 1380 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     168 | 1381 | `	if( pObj == 0 ){` |
|     ! 0 | 1382 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1383 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1384 | `		return SXERR_ABORT;` |
|       - | 1385 | `	}` |
|       - | 1386 | `	/* Generate a unique name */` |
|     168 | 1387 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - | 1388 | `	/* Make sure the generated name is unique */` |
|     168 | 1389 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 1390 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 | 1391 | `	}` |
|     168 | 1392 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     168 | 1393 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - | 1394 | `	/* Compile the lambda body */` |
|     168 | 1395 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     168 | 1396 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1397 | `		return SXERR_ABORT;` |
|       - | 1398 | `	}` |
|     168 | 1399 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1400 | `		/* Emit the load closure instruction */` |
|      14 | 1401 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       8 | 1402 | `	}else{` |
|       - | 1403 | `		/* Emit the load constant instruction */` |
|     156 | 1404 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1405 | `	}` |
|       - | 1406 | `	/* Node successfully compiled */` |
|     168 | 1407 | `	return SXRET_OK;` |
|      85 | 1408 |  |
|       - | 1409 | `/*` |
|       - | 1410 | ` * Compile a backtick quoted string.` |
|       - | 1411 | ` */` |
|       4 | 1412 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1413 |  |
|       - | 1414 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - | 1415 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - | 1416 | `	 */` |
|       7 | 1417 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - | 1418 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 | 1419 | `		ph7_lib_version()` |
|       - | 1420 | `		);` |
|       - | 1421 | `	/* Load NULL */` |
|       5 | 1422 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1423 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1424 | `	/* Node successfully compiled */` |
|       5 | 1425 | `	return SXRET_OK;` |
|       1 | 1426 |  |
|       - | 1427 | `/*` |
|       - | 1428 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - | 1429 | ` * construct.` |
|       - | 1430 | ` */` |
|      72 | 1431 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1432 |  |
|       - | 1433 | `	SyString *pName;` |
|       - | 1434 | `	sxu32 nKeyID;` |
|       - | 1435 | `	sxi32 rc;` |
|       - | 1436 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      74 | 1437 | `	pName = &pGen->pIn->sData;` |
|      74 | 1438 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      74 | 1439 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      74 | 1440 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 | 1441 | `		SyToken *pTmp,*pNext = 0;` |
|       - | 1442 | `		/* Compile arguments one after one */` |
|       9 | 1443 | `		pTmp = pGen->pEnd;` |
|       - | 1444 | `		/* Symisc eXtension to the PHP programming language:` |
|       - | 1445 | `		 * 'echo' can be used in the context of a function which` |
|       - | 1446 | `		 *  mean that the following expression is valid:` |
|       - | 1447 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - | 1448 | `		 */` |
|       9 | 1449 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 | 1450 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 | 1451 | `			if( pGen->pIn < pNext ){` |
|       9 | 1452 | `				pGen->pEnd = pNext;` |
|       9 | 1453 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 | 1454 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1455 | `					return SXERR_ABORT;` |
|       - | 1456 | `				}` |
|       9 | 1457 | `				if( rc != SXERR_EMPTY ){` |
|       - | 1458 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - | 1459 | `					 * without the overhead of a function call.` |
|       - | 1460 | `					 * This is a very powerful optimization that improve` |
|       - | 1461 | `					 * performance greatly.` |
|       - | 1462 | `					 */` |
|       9 | 1463 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 | 1464 | `				}` |
|       4 | 1465 | `			}` |
|       - | 1466 | `			/* Jump trailing commas */` |
|       9 | 1467 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 1468 | `				pNext++;` |
|     ! 0 | 1469 | `			}` |
|       9 | 1470 | `			pGen->pIn = pNext;` |
|       1 | 1471 | `		}` |
|       - | 1472 | `		/* Restore token stream */` |
|       9 | 1473 | `		pGen->pEnd = pTmp;` |
|       5 | 1474 | `	}else{` |
|      66 | 1475 | `		sxi32 nArg = 0;` |
|      66 | 1476 | `		sxu32 nIdx = 0;` |
|      66 | 1477 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      66 | 1478 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1479 | `			return SXERR_ABORT;` |
|      66 | 1480 | `		}else if(rc != SXERR_EMPTY ){` |
|      66 | 1481 | `			nArg = 1;` |
|      32 | 1482 | `		}` |
|      66 | 1483 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1484 | `			ph7_value *pObj;` |
|       - | 1485 | `			/* Emit the call instruction */` |
|      20 | 1486 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 | 1487 | `			if( pObj == 0 ){` |
|     ! 0 | 1488 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1489 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1490 | `				return SXERR_ABORT;` |
|       - | 1491 | `			}` |
|      20 | 1492 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1493 | `			/* Install in the literal table */` |
|      20 | 1494 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       9 | 1495 | `		}` |
|       - | 1496 | `		/* Emit the call instruction */` |
|      66 | 1497 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      66 | 1498 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - | 1499 | `	}` |
|       - | 1500 | `	/* Node successfully compiled */` |
|      74 | 1501 | `	return SXRET_OK;` |
|      38 | 1502 |  |
|       - | 1503 | `/*` |
|       - | 1504 | ` * Compile a node holding a variable declaration.` |
|       - | 1505 | ` * According to the PHP language reference` |
|       - | 1506 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 1507 | ` *  The variable name is case-sensitive.` |
|       - | 1508 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 1509 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1510 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 1511 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 1512 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 1513 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 1514 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 1515 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 1516 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 1517 | ` *  the chapter on Expressions.` |
|       - | 1518 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 1519 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 1520 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 1521 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 1522 | ` *  is being assigned (the source variable).` |
|       - | 1523 | ` */` |
|  753090 | 1524 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1525 |  |
|  753092 | 1526 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1527 | `	sxi32 iVv;` |
|       - | 1528 | `	sxi32 iP1;` |
|       - | 1529 | `	void *p3;` |
|       - | 1530 | `	sxi32 rc;` |
|  753092 | 1531 | `	iVv = -1; /* Variable variable counter */` |
| 1506194 | 1532 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  753104 | 1533 | `		pGen->pIn++;` |
|  753104 | 1534 | `		iVv++;` |
|       2 | 1535 | `	}` |
|  753092 | 1536 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1537 | `		/* Invalid variable name */` |
|     ! 0 | 1538 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 | 1539 | `		if( rc == SXERR_ABORT ){` |
|       - | 1540 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1541 | `			return SXERR_ABORT;` |
|       - | 1542 | `		}` |
|     ! 0 | 1543 | `		return SXRET_OK;` |
|       - | 1544 | `	}` |
|  753092 | 1545 | `	p3  = 0;` |
|  753092 | 1546 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 1547 | `		/* Dynamic variable creation */` |
|      18 | 1548 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 | 1549 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 | 1550 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1551 | `			/* Empty expression */` |
|       3 | 1552 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1553 | `			return SXRET_OK;` |
|       - | 1554 | `		}` |
|       - | 1555 | `		/* Compile the expression holding the variable name */` |
|      16 | 1556 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 | 1557 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1558 | `			return SXERR_ABORT;` |
|      16 | 1559 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 1560 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 | 1561 | `			return SXRET_OK;` |
|       - | 1562 | `		}` |
|       7 | 1563 | `	}else{` |
|       - | 1564 | `		SyHashEntry *pEntry;` |
|       - | 1565 | `		SyString *pName;` |
|  753076 | 1566 | `		char *zName = 0;` |
|       - | 1567 | `		/* Extract variable name */` |
|  753076 | 1568 | `		pName = &pGen->pIn->sData;` |
|       - | 1569 | `		/* Advance the stream cursor */` |
|  753076 | 1570 | `		pGen->pIn++;` |
|  753076 | 1571 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  753076 | 1572 | `		if( pEntry == 0 ){` |
|       - | 1573 | `			/* Duplicate name */` |
|  108276 | 1574 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  108276 | 1575 | `			if( zName == 0 ){` |
|     ! 0 | 1576 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1577 | `				return SXERR_ABORT;` |
|       - | 1578 | `			}` |
|       - | 1579 | `			/* Install in the hashtable */` |
|  108276 | 1580 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   54139 | 1581 | `		}else{` |
|       - | 1582 | `			/* Name already available */` |
|  644802 | 1583 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1584 | `		}` |
|  753076 | 1585 | `		p3 = (void *)zName;` |
|       - | 1586 | `	}` |
|  753088 | 1587 | `	iP1 = 0;` |
|  753088 | 1588 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  289704 | 1589 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1590 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  283772 | 1591 | `			iP1 = 1;` |
|  141885 | 1592 | `		}` |
|  144851 | 1593 | `	}` |
|       - | 1594 | `	/* Emit the load instruction */` |
|  753088 | 1595 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  753100 | 1596 | `	while( iVv > 0 ){` |
|      13 | 1597 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1598 | `		iVv--;` |
|       1 | 1599 | `	}` |
|       - | 1600 | `	/* Node successfully compiled */` |
|  753088 | 1601 | `	return SXRET_OK;` |
|  376547 | 1602 |  |
|       - | 1603 | `/*` |
|       - | 1604 | ` * Load a literal.` |
|       - | 1605 | ` */` |
|  504980 | 1606 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 1607 |  |
|  504982 | 1608 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1609 | `	ph7_value *pObj;` |
|       - | 1610 | `	SyString *pStr;` |
|       - | 1611 | `	sxu32 nIdx;` |
|       - | 1612 | `	/* Extract token value */` |
|  504982 | 1613 | `	pStr = &pToken->sData;` |
|       - | 1614 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  504982 | 1615 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   91666 | 1616 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1617 | `			/* NULL constant are always indexed at 0 */` |
|   38974 | 1618 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   38974 | 1619 | `			return SXRET_OK;` |
|   52694 | 1620 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1621 | `			/* TRUE constant are always indexed at 1 */` |
|     478 | 1622 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     478 | 1623 | `			return SXRET_OK;` |
|       2 | 1624 | `		}` |
|  479282 | 1625 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   79712 | 1626 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1627 | `			/* FALSE constant are always indexed at 2 */` |
|   34038 | 1628 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   34038 | 1629 | `			return SXRET_OK;` |
|  414467 | 1630 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   70370 | 1631 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1632 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5156 | 1633 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5156 | 1634 | `			if( pObj == 0 ){` |
|     ! 0 | 1635 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1636 | `				return SXERR_ABORT;` |
|       - | 1637 | `			}` |
|    5156 | 1638 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1639 | `			/* Emit the load constant instruction */` |
|    5156 | 1640 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5156 | 1641 | `			return SXRET_OK;` |
|  387119 | 1642 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   25982 | 1643 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - | 1644 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 | 1645 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 | 1646 | `			if( pObj == 0 ){` |
|     ! 0 | 1647 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1648 | `				return SXERR_ABORT;` |
|       - | 1649 | `			}` |
|       7 | 1650 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1651 | `				SyString sNs;` |
|       7 | 1652 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 | 1653 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 | 1654 | `			}else{` |
|     ! 0 | 1655 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - | 1656 | `			}` |
|       7 | 1657 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 | 1658 | `			return SXRET_OK;` |
|  386319 | 1659 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   10880 | 1660 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  380873 | 1661 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   13520 | 1662 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 | 1663 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 1664 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 | 1665 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - | 1666 | `				/* Point to the upper block */` |
|      11 | 1667 | `				pBlock = pBlock->pParent;` |
|       1 | 1668 | `			}` |
|      11 | 1669 | `			if( pBlock == 0 ){` |
|       - | 1670 | `				/* Called in the global scope,load NULL */` |
|       5 | 1671 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1672 | `			}else{` |
|       - | 1673 | `				/* Extract the target function/method */` |
|       7 | 1674 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 | 1675 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - | 1676 | `					/* Not a class method,Load null */` |
|       3 | 1677 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1678 | `				}else{` |
|       5 | 1679 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 | 1680 | `					if( pObj == 0 ){` |
|     ! 0 | 1681 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1682 | `						return SXERR_ABORT;` |
|       - | 1683 | `					}` |
|       5 | 1684 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - | 1685 | `					/* Emit the load constant instruction */` |
|       5 | 1686 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1687 | `				}` |
|       - | 1688 | `			}` |
|      11 | 1689 | `			return SXRET_OK;` |
|       - | 1690 | `	}` |
|       - | 1691 | `	/* Query literal table */` |
|  426328 | 1692 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 1693 | `		ph7_value *pLitObj;` |
|       - | 1694 | `		/* Unknown literal,install it in the literal table */` |
|  199398 | 1695 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  199398 | 1696 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1697 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1698 | `			return SXERR_ABORT;` |
|       - | 1699 | `		}` |
|  199398 | 1700 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  199398 | 1701 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   99698 | 1702 | `	}` |
|       - | 1703 | `	/* Emit the load constant instruction */` |
|  426328 | 1704 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  426328 | 1705 | `	return SXRET_OK;` |
|  252492 | 1706 |  |
|       - | 1707 | `/*` |
|       - | 1708 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1709 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1710 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1711 | ` * Otherwise, load the simple literal directly.` |
|       - | 1712 | ` */` |
|  505004 | 1713 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 1714 |  |
|       - | 1715 | `	sxi32 rc;` |
|  505006 | 1716 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1717 | `		return SXRET_OK;` |
|       - | 1718 | `	}` |
|       - | 1719 | `	/* Check if this is a multi-token namespace path */` |
|  505006 | 1720 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 1721 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      26 | 1722 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      26 | 1723 | `		int isAbsolute = 0;` |
|      26 | 1724 | `		SyBlobReset(pWorker);` |
|       - | 1725 | `		/* Check for leading backslash (absolute path) */` |
|      26 | 1726 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      24 | 1727 | `			isAbsolute = 1;` |
|      24 | 1728 | `			pGen->pIn++; /* Skip leading backslash */` |
|      11 | 1729 | `		}` |
|       - | 1730 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      26 | 1731 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 1732 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 1733 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 | 1734 | `		}` |
|       - | 1735 | `		/* Collect all path components */` |
|     102 | 1736 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     102 | 1737 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      40 | 1738 | `				SyBlobAppend(pWorker,"\\",1);` |
|      21 | 1739 | `			}else{` |
|      64 | 1740 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 1741 | `			}` |
|     102 | 1742 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      26 | 1743 | `				pGen->pIn++;` |
|      26 | 1744 | `				break;` |
|       - | 1745 | `			}` |
|      78 | 1746 | `			pGen->pIn++;` |
|       2 | 1747 | `		}` |
|      26 | 1748 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 1749 | `			ph7_value *pObj;` |
|       - | 1750 | `			SyString sPath;` |
|       - | 1751 | `			sxu32 nIdx;` |
|      26 | 1752 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 1753 | `			/* Install in the literal table */` |
|      26 | 1754 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      13 | 1755 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      13 | 1756 | `				if( pObj == 0 ){` |
|     ! 0 | 1757 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1758 | `					return SXERR_ABORT;` |
|       - | 1759 | `				}` |
|      13 | 1760 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      13 | 1761 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       6 | 1762 | `			}` |
|       - | 1763 | `			/* Emit the load constant instruction.` |
|       - | 1764 | `			 * P1=1 means candidate for constant/function/class expansion. */` |
|      26 | 1765 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      26 | 1766 | `			return SXRET_OK;` |
|       - | 1767 | `		}` |
|     ! 0 | 1768 | `	}` |
|       - | 1769 | `	/* Single-token literal: load directly */` |
|  504982 | 1770 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  504982 | 1771 | `	return rc;` |
|  252504 | 1772 |  |
|       - | 1773 | `/*` |
|       - | 1774 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1775 | ` */` |
|  505004 | 1776 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1777 |  |
|       - | 1778 | `	sxi32 rc;` |
|  505006 | 1779 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  505006 | 1780 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1781 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1782 | `		return rc;` |
|       - | 1783 | `	}` |
|       - | 1784 | `	/* Node successfully compiled */` |
|  505006 | 1785 | `	return SXRET_OK;` |
|  252504 | 1786 |  |
|       - | 1787 | `/*` |
|       - | 1788 | ` * Recover from a compile-time error. In other words synchronize` |
|       - | 1789 | ` * the token stream cursor with the first semi-colon seen.` |
|       - | 1790 | ` */` |
|       8 | 1791 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 | 1792 |  |
|       - | 1793 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 | 1794 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 | 1795 | `		pGen->pIn++;` |
|       1 | 1796 | `	}` |
|       9 | 1797 | `	return SXRET_OK;` |
|       1 | 1798 |  |
|       - | 1799 | `/*` |
|       - | 1800 | ` * Check if the given identifier name is reserved or not.` |
|       - | 1801 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - | 1802 | ` */` |
|      30 | 1803 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 | 1804 |  |
|      32 | 1805 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      12 | 1806 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 | 1807 | `			return TRUE;` |
|      10 | 1808 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 | 1809 | `			return TRUE;` |
|       1 | 1810 | `		}` |
|      24 | 1811 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 | 1812 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 | 1813 | `			return TRUE;` |
|       - | 1814 | `		}` |
|     ! 0 | 1815 | `	}` |
|       - | 1816 | `	/* Not a reserved constant */` |
|      24 | 1817 | `	return FALSE;` |
|      17 | 1818 |  |
|       - | 1819 | `/*` |
|       - | 1820 | ` * Compile the 'const' statement.` |
|       - | 1821 | ` * According to the PHP language reference` |
|       - | 1822 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - | 1823 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - | 1824 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - | 1825 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - | 1826 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1827 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - | 1828 | ` *  Syntax` |
|       - | 1829 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - | 1830 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - | 1831 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - | 1832 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - | 1833 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - | 1834 | ` *  to get a list of all defined constants.` |
|       - | 1835 | ` *` |
|       - | 1836 | ` * Symisc eXtension.` |
|       - | 1837 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - | 1838 | ` *  would allow only simple scalar value.` |
|       - | 1839 | ` *  Example` |
|       - | 1840 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 1841 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 1842 | ` */` |
|      26 | 1843 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 | 1844 |  |
|       - | 1845 | `	SySet *pConsCode,*pInstrContainer;` |
|      28 | 1846 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1847 | `	SyString *pName;` |
|       - | 1848 | `	sxi32 rc;` |
|      28 | 1849 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      28 | 1850 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 1851 | `		/* Invalid constant name */` |
|       7 | 1852 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 | 1853 | `		if( rc == SXERR_ABORT ){` |
|       - | 1854 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1855 | `			return SXERR_ABORT;` |
|       - | 1856 | `		}` |
|       7 | 1857 | `		goto Synchronize;` |
|       - | 1858 | `	}` |
|       - | 1859 | `	/* Peek constant name */` |
|      22 | 1860 | `	pName = &pGen->pIn->sData;` |
|       - | 1861 | `	/* Make sure the constant name isn't reserved */` |
|      22 | 1862 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 1863 | `		/* Reserved constant */` |
|       9 | 1864 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 | 1865 | `		if( rc == SXERR_ABORT ){` |
|       - | 1866 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1867 | `			return SXERR_ABORT;` |
|       - | 1868 | `		}` |
|       9 | 1869 | `		goto Synchronize;` |
|       - | 1870 | `	}` |
|      14 | 1871 | `	pGen->pIn++;` |
|      14 | 1872 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 1873 | `		/* Invalid statement*/` |
|       5 | 1874 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 | 1875 | `		if( rc == SXERR_ABORT ){` |
|       - | 1876 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1877 | `			return SXERR_ABORT;` |
|       - | 1878 | `		}` |
|       5 | 1879 | `		goto Synchronize;` |
|       - | 1880 | `	}` |
|       9 | 1881 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - | 1882 | `	/* Allocate a new constant value container */` |
|       9 | 1883 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       9 | 1884 | `	if( pConsCode == 0 ){` |
|     ! 0 | 1885 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1886 | `		return SXERR_ABORT;` |
|       - | 1887 | `	}` |
|       9 | 1888 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 1889 | `	/* Swap bytecode container */` |
|       9 | 1890 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       9 | 1891 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - | 1892 | `	/* Compile constant value */` |
|       9 | 1893 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 1894 | `	/* Emit the done instruction */` |
|       9 | 1895 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       9 | 1896 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       9 | 1897 | `	if( rc == SXERR_ABORT ){` |
|       - | 1898 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1899 | `		return SXERR_ABORT;` |
|       - | 1900 | `	}` |
|       9 | 1901 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - | 1902 | `	/* Register the constant */` |
|       9 | 1903 | `	rc = PH7_VmRegisterConstant(pGen->pVm,pName,PH7_VmExpandConstantValue,pConsCode);` |
|       9 | 1904 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1905 | `		SySetRelease(pConsCode);` |
|     ! 0 | 1906 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 | 1907 | `	}` |
|       9 | 1908 | `	return SXRET_OK;` |
|       9 | 1909 | `Synchronize:` |
|       - | 1910 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 | 1911 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 | 1912 | `		pGen->pIn++;` |
|       1 | 1913 | `	}` |
|      19 | 1914 | `	return SXRET_OK;` |
|      15 | 1915 |  |
|       - | 1916 | `/*` |
|       - | 1917 | ` * Compile the 'continue' statement.` |
|       - | 1918 | ` * According to the PHP language reference` |
|       - | 1919 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - | 1920 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - | 1921 | ` *  iteration.` |
|       - | 1922 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - | 1923 | ` *  the purposes of continue.` |
|       - | 1924 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - | 1925 | ` *  of enclosing loops it should skip to the end of.` |
|       - | 1926 | ` *  Note:` |
|       - | 1927 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - | 1928 | ` */` |
|       - | 1929 | `/*` |
|       - | 1930 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - | 1931 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - | 1932 | ` * break/continue crosses a try boundary.` |
|       - | 1933 | ` *` |
|       - | 1934 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - | 1935 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - | 1936 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - | 1937 | ` */` |
|    2696 | 1938 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 | 1939 |  |
|    2698 | 1940 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   15738 | 1941 | `	while( pBlock && pBlock != pTarget ){` |
|   13042 | 1942 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 | 1943 | `			if( pBlock->pUserData ){` |
|       - | 1944 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 | 1945 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 | 1946 | `			}else{` |
|       - | 1947 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - | 1948 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - | 1949 | `				 * exception context from a sub-execution.` |
|       - | 1950 | `				 */` |
|     ! 0 | 1951 | `				break;` |
|       - | 1952 | `			}` |
|       1 | 1953 | `		}` |
|   13042 | 1954 | `		pBlock = pBlock->pParent;` |
|       2 | 1955 | `	}` |
|    2698 | 1956 |  |
|    2616 | 1957 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 1958 |  |
|       - | 1959 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1960 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1961 | `	sxu32 nLineLocal;` |
|       - | 1962 | `	sxi32 rc;` |
|    2618 | 1963 | `	nLineLocal = pGen->pIn->nLine;` |
|    2618 | 1964 | `	iLevel = 0;` |
|       - | 1965 | `	/* Jump the 'continue' keyword */` |
|    2618 | 1966 | `	pGen->pIn++;` |
|    2618 | 1967 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 1968 | `		/* optional numeric argument which tells us how many levels` |
|       - | 1969 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 1970 | `		 */` |
|      12 | 1971 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      12 | 1972 | `		if( iLevel < 2 ){` |
|     ! 0 | 1973 | `			iLevel = 0;` |
|     ! 0 | 1974 | `		}` |
|      12 | 1975 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 1976 | `	}` |
|       - | 1977 | `	/* Point to the target loop */` |
|    2618 | 1978 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2618 | 1979 | `	if( pLoop == 0 ){` |
|       - | 1980 | `		/* Illegal continue */` |
|      11 | 1981 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 1982 | `		if( rc == SXERR_ABORT ){` |
|       - | 1983 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1984 | `			return SXERR_ABORT;` |
|       - | 1985 | `		}` |
|       6 | 1986 | `	}else{` |
|    2608 | 1987 | `		sxu32 nInstrIdx = 0;` |
|       - | 1988 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2608 | 1989 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2608 | 1990 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - | 1991 | `			/* According to the PHP language reference manual` |
|       - | 1992 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - | 1993 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - | 1994 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - | 1995 | `			 */` |
|       5 | 1996 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 | 1997 | `			if( rc == SXRET_OK ){` |
|       5 | 1998 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 | 1999 | `			}` |
|       3 | 2000 | `		}else{` |
|       - | 2001 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    2604 | 2002 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2604 | 2003 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 2004 | `				JumpFixup sJumpFix;` |
|       - | 2005 | `				/* Post-continue */` |
|       9 | 2006 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       9 | 2007 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       9 | 2008 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       4 | 2009 | `			}` |
|       - | 2010 | `		}` |
|       - | 2011 | `	}` |
|    2618 | 2012 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2013 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2014 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 2015 | `	}` |
|       - | 2016 | `	/* Statement successfully compiled */` |
|    2618 | 2017 | `	return SXRET_OK;` |
|    1310 | 2018 |  |
|       - | 2019 | `/*` |
|       - | 2020 | ` * Compile the 'break' statement.` |
|       - | 2021 | ` * According to the PHP language reference` |
|       - | 2022 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - | 2023 | ` *  structure.` |
|       - | 2024 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - | 2025 | ` *  enclosing structures are to be broken out of.` |
|       - | 2026 | ` */` |
|     106 | 2027 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 | 2028 |  |
|       - | 2029 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2030 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2031 | `	sxi32 rc;` |
|     108 | 2032 | `	iLevel = 0;` |
|       - | 2033 | `	/* Jump the 'break' keyword */` |
|     108 | 2034 | `	pGen->pIn++;` |
|     108 | 2035 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 2036 | `		/* optional numeric argument which tells us how many levels` |
|       - | 2037 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 2038 | `		 */` |
|      12 | 2039 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      12 | 2040 | `		if( iLevel < 2 ){` |
|     ! 0 | 2041 | `			iLevel = 0;` |
|     ! 0 | 2042 | `		}` |
|      12 | 2043 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 2044 | `	}` |
|       - | 2045 | `	/* Extract the target loop */` |
|     108 | 2046 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     108 | 2047 | `	if( pLoop == 0 ){` |
|       - | 2048 | `		/* Illegal break */` |
|      17 | 2049 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 | 2050 | `		if( rc == SXERR_ABORT ){` |
|       - | 2051 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2052 | `			return SXERR_ABORT;` |
|       - | 2053 | `		}` |
|       9 | 2054 | `	}else{` |
|       - | 2055 | `		sxu32 nInstrIdx;` |
|       - | 2056 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|      92 | 2057 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|      92 | 2058 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      92 | 2059 | `		if( rc == SXRET_OK ){` |
|       - | 2060 | `			/* Fix the jump later when the jump destination is resolved */` |
|      92 | 2061 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      45 | 2062 | `		}` |
|       - | 2063 | `	}` |
|     108 | 2064 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2065 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2066 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 | 2067 | `	}` |
|       - | 2068 | `	/* Statement successfully compiled */` |
|     108 | 2069 | `	return SXRET_OK;` |
|      55 | 2070 |  |
|       - | 2071 | `/*` |
|       - | 2072 | ` * Compile or record a label.` |
|       - | 2073 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - | 2074 | ` * Example` |
|       - | 2075 | ` *  goto LABEL;` |
|       - | 2076 | ` *   echo 'Foo';` |
|       - | 2077 | ` *  LABEL:` |
|       - | 2078 | ` *   echo 'Bar';` |
|       - | 2079 | ` */` |
|     112 | 2080 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 | 2081 |  |
|       - | 2082 | `	GenBlock *pBlock;` |
|       - | 2083 | `	Label sLabel;` |
|       - | 2084 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 | 2085 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 | 2086 | `	if( pBlock ){` |
|       - | 2087 | `		sxi32 rc;` |
|       7 | 2088 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 | 2089 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 | 2090 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2091 | `			return SXERR_ABORT;` |
|       - | 2092 | `		}` |
|       3 | 2093 | `	}else{` |
|     110 | 2094 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 2095 | `		char *zDup;` |
|       - | 2096 | `		/* Initialize label fields */` |
|     110 | 2097 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2098 | `		/* Duplicate label name */` |
|     110 | 2099 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 | 2100 | `		if( zDup == 0 ){` |
|     ! 0 | 2101 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2102 | `			return SXERR_ABORT;` |
|       - | 2103 | `		}` |
|     110 | 2104 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 | 2105 | `		sLabel.bRef  = FALSE;` |
|     110 | 2106 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 | 2107 | `		pBlock = pGen->pCurrent;` |
|     218 | 2108 | `		while( pBlock ){` |
|     130 | 2109 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 | 2110 | `				break;` |
|       - | 2111 | `			}` |
|       - | 2112 | `			/* Point to the upper block */` |
|     110 | 2113 | `			pBlock = pBlock->pParent;` |
|       2 | 2114 | `		}` |
|     110 | 2115 | `		if( pBlock ){` |
|      22 | 2116 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 | 2117 | `		}else{` |
|      90 | 2118 | `			sLabel.pFunc = 0;` |
|       - | 2119 | `		}` |
|       - | 2120 | `		/* Insert in label set */` |
|     110 | 2121 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - | 2122 | `	}` |
|     114 | 2123 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 | 2124 | `	return SXRET_OK;` |
|      58 | 2125 |  |
|       - | 2126 | `/*` |
|       - | 2127 | ` * Compile the so hated 'goto' statement.` |
|       - | 2128 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - | 2129 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - | 2130 | ` * a compiler it has to do this.` |
|       - | 2131 | ` * According to the PHP language reference manual` |
|       - | 2132 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - | 2133 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - | 2134 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - | 2135 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - | 2136 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - | 2137 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - | 2138 | ` *   of a multi-level break` |
|       - | 2139 | ` */` |
|     152 | 2140 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 | 2141 |  |
|       - | 2142 | `	JumpFixup sJump;` |
|       - | 2143 | `	sxi32 rc;` |
|     154 | 2144 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 | 2145 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2146 | `		/* Missing label */` |
|     ! 0 | 2147 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 | 2148 | `		if( rc == SXERR_ABORT ){` |
|       - | 2149 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2150 | `			return SXERR_ABORT;` |
|       - | 2151 | `		}` |
|     ! 0 | 2152 | `		return SXRET_OK;` |
|       - | 2153 | `	}` |
|     154 | 2154 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 | 2155 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 | 2156 | `		if( rc == SXERR_ABORT ){` |
|       - | 2157 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2158 | `			return SXERR_ABORT;` |
|       - | 2159 | `		}` |
|       3 | 2160 | `	}else{` |
|     150 | 2161 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 2162 | `		GenBlock *pBlock;` |
|       - | 2163 | `		char *zDup;` |
|       - | 2164 | `		/* Prepare the jump destination */` |
|     150 | 2165 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 | 2166 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - | 2167 | `		/* Duplicate label name */` |
|     150 | 2168 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 | 2169 | `		if( zDup == 0 ){` |
|     ! 0 | 2170 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2171 | `			return SXERR_ABORT;` |
|       - | 2172 | `		}` |
|     150 | 2173 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 | 2174 | `		pBlock = pGen->pCurrent;` |
|     312 | 2175 | `		while( pBlock ){` |
|     196 | 2176 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 | 2177 | `				break;` |
|       - | 2178 | `			}` |
|       - | 2179 | `			/* Point to the upper block */` |
|     164 | 2180 | `			pBlock = pBlock->pParent;` |
|       2 | 2181 | `		}` |
|     150 | 2182 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 | 2183 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 | 2184 | `			if( rc == SXERR_ABORT ){` |
|       - | 2185 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2186 | `				return SXERR_ABORT;` |
|       - | 2187 | `			}` |
|       3 | 2188 | `		}` |
|     150 | 2189 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 | 2190 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 | 2191 | `		}else{` |
|     124 | 2192 | `			sJump.pFunc = 0;` |
|       - | 2193 | `		}` |
|       - | 2194 | `		/* Emit the unconditional jump */` |
|     150 | 2195 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 | 2196 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 | 2197 | `		}` |
|       - | 2198 | `	}` |
|     154 | 2199 | `	pGen->pIn++; /* Jump the label name */` |
|     154 | 2200 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 | 2201 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 | 2202 | `	}` |
|       - | 2203 | `	/* Statement successfully compiled */` |
|     154 | 2204 | `	return SXRET_OK;` |
|      78 | 2205 |  |
|       - | 2206 | `/*` |
|       - | 2207 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - | 2208 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - | 2209 | ` * failure.` |
|       - | 2210 | ` */` |
|      20 | 2211 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 | 2212 |  |
|       - | 2213 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - | 2214 | `	sxu32 nRawObj;` |
|      10 | 2215 | `	sxu32 nObjIdx;` |
|       - | 2216 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - | 2217 | `	 * a PHP block.` |
|       - | 2218 | `	 */` |
|      10 | 2219 | `Consume:` |
|      21 | 2220 | `	nRawObj = nObjIdx = 0;` |
|      21 | 2221 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 | 2222 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 | 2223 | `		if( pRawObj == 0 ){` |
|     ! 0 | 2224 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2225 | `			return SXERR_ABORT;` |
|       - | 2226 | `		}` |
|       - | 2227 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 | 2228 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 | 2229 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 | 2230 | `		++nRawObj;` |
|     ! 0 | 2231 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 | 2232 | `	}` |
|      21 | 2233 | `	if( nRawObj > 0 ){` |
|       - | 2234 | `		/* Emit the consume instruction */` |
|     ! 0 | 2235 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 | 2236 | `	}` |
|      21 | 2237 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 | 2238 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - | 2239 | `		/* Reset the token set */` |
|     ! 0 | 2240 | `		SySetReset(pTokenSet);` |
|       - | 2241 | `		/* Tokenize input */` |
|     ! 0 | 2242 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 | 2243 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - | 2244 | `		/* Point to the fresh token stream */` |
|     ! 0 | 2245 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 | 2246 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - | 2247 | `		/* Advance the stream cursor */` |
|     ! 0 | 2248 | `		pGen->pRawIn++;` |
|       - | 2249 | `		/* TICKET 1433-011 */` |
|     ! 0 | 2250 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 2251 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 2252 | `			sxi32 rc;` |
|       - | 2253 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 | 2254 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 | 2255 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 | 2256 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 | 2257 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 2258 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2259 | `				return SXERR_ABORT;` |
|     ! 0 | 2260 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 | 2261 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 2262 | `			}` |
|     ! 0 | 2263 | `			goto Consume;` |
|       - | 2264 | `		}` |
|     ! 0 | 2265 | `	}else{` |
|       - | 2266 | `		/* No more chunks to process */` |
|      21 | 2267 | `		pGen->pIn = pGen->pEnd;` |
|      21 | 2268 | `		return SXERR_EOF;` |
|       - | 2269 | `	}` |
|     ! 0 | 2270 | `	return SXRET_OK;` |
|      11 | 2271 |  |
|       - | 2272 | `/*` |
|       - | 2273 | ` * Compile a PHP block.` |
|       - | 2274 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - | 2275 | ` * optionally delimited by braces {}.` |
|       - | 2276 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 2277 | ` * and this function takes care of generating the appropriate error` |
|       - | 2278 | ` * message.` |
|       - | 2279 | ` */` |
|  283878 | 2280 | `static sxi32 PH7_CompileBlock(` |
|       - | 2281 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2282 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2283 | `	)` |
|       2 | 2284 |  |
|       - | 2285 | `	sxi32 rc;` |
|       - | 2286 | `	sxu32 nLine;` |
|  283880 | 2287 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  282486 | 2288 | `		nLine = pGen->pIn->nLine;` |
|  282486 | 2289 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  282486 | 2290 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2291 | `			return SXERR_ABORT;` |
|       - | 2292 | `		}` |
|  282486 | 2293 | `		pGen->pIn++;` |
|       - | 2294 | `		/* Compile until we hit the closing braces '}' */` |
|  389976 | 2295 | `		for(;;){` |
|  779954 | 2296 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 | 2297 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 | 2298 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2299 | `			 	   return SXERR_ABORT;` |
|       - | 2300 | `				}` |
|      21 | 2301 | `				if( rc == SXERR_EOF ){` |
|       - | 2302 | `					/* No more token to process. Missing closing braces */` |
|      21 | 2303 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 | 2304 | `					break;` |
|       - | 2305 | `				}` |
|     ! 0 | 2306 | `			}` |
|  779934 | 2307 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2308 | `				/* Closing braces found,break immediately*/` |
|  282466 | 2309 | `				pGen->pIn++;` |
|  282466 | 2310 | `				break;` |
|       - | 2311 | `			}` |
|       - | 2312 | `			/* Compile a single statement */` |
|  497470 | 2313 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  497470 | 2314 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2315 | `				return SXERR_ABORT;` |
|       - | 2316 | `			}` |
|       2 | 2317 | `		}` |
|  282486 | 2318 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  142638 | 2319 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 | 2320 | `		pGen->pIn++;` |
|     ! 0 | 2321 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 | 2322 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2323 | `			return SXERR_ABORT;` |
|       - | 2324 | `		}` |
|       - | 2325 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 | 2326 | `		for(;;){` |
|     ! 0 | 2327 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2328 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 | 2329 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2330 | `			 	   return SXERR_ABORT;` |
|       - | 2331 | `				}` |
|     ! 0 | 2332 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - | 2333 | `					/* No more token to process */` |
|     ! 0 | 2334 | `					if( rc == SXERR_EOF ){` |
|     ! 0 | 2335 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - | 2336 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 | 2337 | `					}` |
|     ! 0 | 2338 | `					break;` |
|       - | 2339 | `				}` |
|     ! 0 | 2340 | `			}` |
|     ! 0 | 2341 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 2342 | `				sxi32 nKwrd;` |
|       - | 2343 | `				/* Keyword found */` |
|     ! 0 | 2344 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 | 2345 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 | 2346 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - | 2347 | `						/* Delimiter keyword found,break */` |
|     ! 0 | 2348 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 | 2349 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 | 2350 | `						}` |
|     ! 0 | 2351 | `						break;` |
|       - | 2352 | `				}` |
|     ! 0 | 2353 | `			}` |
|       - | 2354 | `			/* Compile a single statement */` |
|     ! 0 | 2355 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 | 2356 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2357 | `				return SXERR_ABORT;` |
|       - | 2358 | `			}` |
|     ! 0 | 2359 | `		}` |
|     ! 0 | 2360 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 | 2361 | `	}else{` |
|       - | 2362 | `		/* Compile a single statement */` |
|    1396 | 2363 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1396 | 2364 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2365 | `			return SXERR_ABORT;` |
|       - | 2366 | `		}` |
|       - | 2367 | `	}` |
|       - | 2368 | `	/* Jump trailing semi-colons ';' */` |
|  283880 | 2369 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2370 | `		pGen->pIn++;` |
|     ! 0 | 2371 | `	}` |
|  283880 | 2372 | `	return SXRET_OK;` |
|  141941 | 2373 |  |
|       - | 2374 | `/*` |
|       - | 2375 | ` * Compile the gentle 'while' statement.` |
|       - | 2376 | ` * According to the PHP language reference` |
|       - | 2377 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - | 2378 | ` *  The basic form of a while statement is:` |
|       - | 2379 | ` *  while (expr)` |
|       - | 2380 | ` *   statement` |
|       - | 2381 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - | 2382 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - | 2383 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - | 2384 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - | 2385 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - | 2386 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - | 2387 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - | 2388 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - | 2389 | ` *  while (expr):` |
|       - | 2390 | ` *    statement` |
|       - | 2391 | ` *   endwhile;` |
|       - | 2392 | ` */` |
|   10406 | 2393 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 2394 |  |
|   10408 | 2395 | `	GenBlock *pWhileBlock = 0;` |
|   10408 | 2396 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2397 | `	sxu32 nFalseJump;` |
|       - | 2398 | `	sxu32 nLine;` |
|       - | 2399 | `	sxi32 rc;` |
|   10408 | 2400 | `	nLine = pGen->pIn->nLine;` |
|       - | 2401 | `	/* Jump the 'while' keyword */` |
|   10408 | 2402 | `	pGen->pIn++;` |
|   10408 | 2403 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2404 | `		/* Syntax error */` |
|     ! 0 | 2405 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2406 | `		if( rc == SXERR_ABORT ){` |
|       - | 2407 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2408 | `			return SXERR_ABORT;` |
|       - | 2409 | `		}` |
|     ! 0 | 2410 | `		goto Synchronize;` |
|       - | 2411 | `	}` |
|       - | 2412 | `	/* Jump the left parenthesis '(' */` |
|   10408 | 2413 | `	pGen->pIn++;` |
|       - | 2414 | `	/* Create the loop block */` |
|   10408 | 2415 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   10408 | 2416 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2417 | `		return SXERR_ABORT;` |
|       - | 2418 | `	}` |
|       - | 2419 | `	/* Delimit the condition */` |
|   10408 | 2420 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10408 | 2421 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2422 | `		/* Empty expression */` |
|       3 | 2423 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2424 | `		if( rc == SXERR_ABORT ){` |
|       - | 2425 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2426 | `			return SXERR_ABORT;` |
|       - | 2427 | `		}` |
|       1 | 2428 | `	}` |
|       - | 2429 | `	/* Swap token streams */` |
|   10408 | 2430 | `	pTmp = pGen->pEnd;` |
|   10408 | 2431 | `	pGen->pEnd = pEnd;` |
|       - | 2432 | `	/* Compile the expression */` |
|   10408 | 2433 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10408 | 2434 | `	if( rc == SXERR_ABORT ){` |
|       - | 2435 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2436 | `		return SXERR_ABORT;` |
|       - | 2437 | `	}` |
|       - | 2438 | `	/* Update token stream */` |
|   10408 | 2439 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2440 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2441 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2442 | `			return SXERR_ABORT;` |
|       - | 2443 | `		}` |
|     ! 0 | 2444 | `		pGen->pIn++;` |
|     ! 0 | 2445 | `	}` |
|       - | 2446 | `	/* Synchronize pointers */` |
|   10408 | 2447 | `	pGen->pIn  = &pEnd[1];` |
|   10408 | 2448 | `	pGen->pEnd = pTmp;` |
|       - | 2449 | `	/* Emit the false jump */` |
|   10408 | 2450 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2451 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10408 | 2452 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2453 | `	/* Compile the loop body */` |
|   10408 | 2454 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   10408 | 2455 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2456 | `		return SXERR_ABORT;` |
|       - | 2457 | `	}` |
|       - | 2458 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10408 | 2459 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2460 | `	/* Fix all jumps now the destination is resolved */` |
|   10408 | 2461 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2462 | `	/* Release the loop block */` |
|   10408 | 2463 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2464 | `	/* Statement successfully compiled */` |
|   10408 | 2465 | `	return SXRET_OK;` |
|     ! 0 | 2466 | `Synchronize:` |
|       - | 2467 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2468 | `	 * compiling this erroneous block.` |
|       - | 2469 | `	 */` |
|     ! 0 | 2470 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2471 | `		pGen->pIn++;` |
|     ! 0 | 2472 | `	}` |
|     ! 0 | 2473 | `	return SXRET_OK;` |
|    5205 | 2474 |  |
|       - | 2475 | `/*` |
|       - | 2476 | ` * Compile the ugly do..while() statement.` |
|       - | 2477 | ` * According to the PHP language reference` |
|       - | 2478 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - | 2479 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - | 2480 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - | 2481 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - | 2482 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - | 2483 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - | 2484 | ` *  would end immediately).` |
|       - | 2485 | ` *  There is just one syntax for do-while loops:` |
|       - | 2486 | ` *  <?php` |
|       - | 2487 | ` *  $i = 0;` |
|       - | 2488 | ` *  do {` |
|       - | 2489 | ` *   echo $i;` |
|       - | 2490 | ` *  } while ($i > 0);` |
|       - | 2491 | ` * ?>` |
|       - | 2492 | ` */` |
|       2 | 2493 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 | 2494 |  |
|       3 | 2495 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 | 2496 | `	GenBlock *pDoBlock = 0;` |
|       - | 2497 | `	sxu32 nLine;` |
|       - | 2498 | `	sxi32 rc;` |
|       3 | 2499 | `	nLine = pGen->pIn->nLine;` |
|       - | 2500 | `	/* Jump the 'do' keyword */` |
|       3 | 2501 | `	pGen->pIn++;` |
|       - | 2502 | `	/* Create the loop block */` |
|       3 | 2503 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 | 2504 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2505 | `		return SXERR_ABORT;` |
|       - | 2506 | `	}` |
|       - | 2507 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 | 2508 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 | 2509 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 | 2510 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2511 | `		return SXERR_ABORT;` |
|       - | 2512 | `	}` |
|       3 | 2513 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2514 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 | 2515 | `	}` |
|       3 | 2516 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 | 2517 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - | 2518 | `			/* Missing 'while' statement */` |
|       3 | 2519 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 | 2520 | `			if( rc == SXERR_ABORT ){` |
|       - | 2521 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2522 | `				return SXERR_ABORT;` |
|       - | 2523 | `			}` |
|       3 | 2524 | `			goto Synchronize;` |
|       - | 2525 | `	}` |
|       - | 2526 | `	/* Jump the 'while' keyword */` |
|     ! 0 | 2527 | `	pGen->pIn++;` |
|     ! 0 | 2528 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2529 | `		/* Syntax error */` |
|     ! 0 | 2530 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2531 | `		if( rc == SXERR_ABORT ){` |
|       - | 2532 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2533 | `			return SXERR_ABORT;` |
|       - | 2534 | `		}` |
|     ! 0 | 2535 | `		goto Synchronize;` |
|       - | 2536 | `	}` |
|       - | 2537 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 | 2538 | `	pGen->pIn++;` |
|       - | 2539 | `	/* Delimit the condition */` |
|     ! 0 | 2540 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 | 2541 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2542 | `		/* Empty expression */` |
|     ! 0 | 2543 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 | 2544 | `		if( rc == SXERR_ABORT ){` |
|       - | 2545 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2546 | `			return SXERR_ABORT;` |
|       - | 2547 | `		}` |
|     ! 0 | 2548 | `		goto Synchronize;` |
|       - | 2549 | `	}` |
|       - | 2550 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 | 2551 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - | 2552 | `		JumpFixup *aPost;` |
|       - | 2553 | `		VmInstr *pInstr;` |
|       - | 2554 | `		sxu32 nJumpDest;` |
|       - | 2555 | `		sxu32 n;` |
|     ! 0 | 2556 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 | 2557 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 | 2558 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 | 2559 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 | 2560 | `			if( pInstr ){` |
|       - | 2561 | `				/* Fix */` |
|     ! 0 | 2562 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 | 2563 | `			}` |
|     ! 0 | 2564 | `		}` |
|     ! 0 | 2565 | `	}` |
|       - | 2566 | `	/* Swap token streams */` |
|     ! 0 | 2567 | `	pTmp = pGen->pEnd;` |
|     ! 0 | 2568 | `	pGen->pEnd = pEnd;` |
|       - | 2569 | `	/* Compile the expression */` |
|     ! 0 | 2570 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 2571 | `	if( rc == SXERR_ABORT ){` |
|       - | 2572 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2573 | `		return SXERR_ABORT;` |
|       - | 2574 | `	}` |
|       - | 2575 | `	/* Update token stream */` |
|     ! 0 | 2576 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2577 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2578 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2579 | `			return SXERR_ABORT;` |
|       - | 2580 | `		}` |
|     ! 0 | 2581 | `		pGen->pIn++;` |
|     ! 0 | 2582 | `	}` |
|     ! 0 | 2583 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 | 2584 | `	pGen->pEnd = pTmp;` |
|       - | 2585 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 | 2586 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - | 2587 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 | 2588 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2589 | `	/* Release the loop block */` |
|     ! 0 | 2590 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2591 | `	/* Statement successfully compiled */` |
|     ! 0 | 2592 | `	return SXRET_OK;` |
|       1 | 2593 | `Synchronize:` |
|       - | 2594 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2595 | `	 * compiling this erroneous block.` |
|       - | 2596 | `	 */` |
|       3 | 2597 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2598 | `		pGen->pIn++;` |
|     ! 0 | 2599 | `	}` |
|       3 | 2600 | `	return SXRET_OK;` |
|       2 | 2601 |  |
|       - | 2602 | `/*` |
|       - | 2603 | ` * Compile the complex and powerful 'for' statement.` |
|       - | 2604 | ` * According to the PHP language reference` |
|       - | 2605 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - | 2606 | ` *  The syntax of a for loop is:` |
|       - | 2607 | ` *  for (expr1; expr2; expr3)` |
|       - | 2608 | ` *   statement` |
|       - | 2609 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - | 2610 | ` *  the beginning of the loop.` |
|       - | 2611 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - | 2612 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - | 2613 | ` *  to FALSE, the execution of the loop ends.` |
|       - | 2614 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - | 2615 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - | 2616 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - | 2617 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - | 2618 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - | 2619 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - | 2620 | ` *  of using the for truth expression.` |
|       - | 2621 | ` */` |
|   10390 | 2622 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 2623 |  |
|   10392 | 2624 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   10392 | 2625 | `	GenBlock *pForBlock = 0;` |
|       - | 2626 | `	sxu32 nFalseJump;` |
|       - | 2627 | `	sxu32 nLine;` |
|       - | 2628 | `	sxi32 rc;` |
|   10392 | 2629 | `	nLine = pGen->pIn->nLine;` |
|       - | 2630 | `	/* Jump the 'for' keyword */` |
|   10392 | 2631 | `	pGen->pIn++;` |
|   10392 | 2632 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2633 | `		/* Syntax error */` |
|     ! 0 | 2634 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2635 | `		if( rc == SXERR_ABORT ){` |
|       - | 2636 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2637 | `			return SXERR_ABORT;` |
|       - | 2638 | `		}` |
|     ! 0 | 2639 | `		return SXRET_OK;` |
|       - | 2640 | `	}` |
|       - | 2641 | `	/* Jump the left parenthesis '(' */` |
|   10392 | 2642 | `	pGen->pIn++;` |
|       - | 2643 | `	/* Delimit the init-expr;condition;post-expr */` |
|   10392 | 2644 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10392 | 2645 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2646 | `		/* Empty expression */` |
|     ! 0 | 2647 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 | 2648 | `		if( rc == SXERR_ABORT ){` |
|       - | 2649 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2650 | `			return SXERR_ABORT;` |
|       - | 2651 | `		}` |
|       - | 2652 | `		/* Synchronize */` |
|     ! 0 | 2653 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2654 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2655 | `			pGen->pIn++;` |
|     ! 0 | 2656 | `		}` |
|     ! 0 | 2657 | `		return SXRET_OK;` |
|       - | 2658 | `	}` |
|       - | 2659 | `	/* Swap token streams */` |
|   10392 | 2660 | `	pTmp = pGen->pEnd;` |
|   10392 | 2661 | `	pGen->pEnd = pEnd;` |
|       - | 2662 | `	/* Compile initialization expressions if available */` |
|   10392 | 2663 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2664 | `	/* Pop operand lvalues */` |
|   10392 | 2665 | `	if( rc == SXERR_ABORT ){` |
|       - | 2666 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2667 | `		return SXERR_ABORT;` |
|   10392 | 2668 | `	}else if( rc != SXERR_EMPTY ){` |
|   10390 | 2669 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5194 | 2670 | `	}` |
|   10392 | 2671 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2672 | `		/* Syntax error */` |
|     ! 0 | 2673 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2674 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 | 2675 | `		if( rc == SXERR_ABORT ){` |
|       - | 2676 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2677 | `			return SXERR_ABORT;` |
|       - | 2678 | `		}` |
|     ! 0 | 2679 | `		return SXRET_OK;` |
|       - | 2680 | `	}` |
|       - | 2681 | `	/* Jump the trailing ';' */` |
|   10392 | 2682 | `	pGen->pIn++;` |
|       - | 2683 | `	/* Create the loop block */` |
|   10392 | 2684 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   10392 | 2685 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2686 | `		return SXERR_ABORT;` |
|       - | 2687 | `	}` |
|       - | 2688 | `	/* Deffer continue jumps */` |
|   10392 | 2689 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 2690 | `	/* Compile the condition */` |
|   10392 | 2691 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10392 | 2692 | `	if( rc == SXERR_ABORT ){` |
|       - | 2693 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2694 | `		return SXERR_ABORT;` |
|   10392 | 2695 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 2696 | `		/* Emit the false jump */` |
|   10390 | 2697 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2698 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10390 | 2699 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5194 | 2700 | `	}` |
|   10392 | 2701 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2702 | `		/* Syntax error */` |
|       5 | 2703 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2704 | `			"for: Expected ';' after conditionals expressions");` |
|       5 | 2705 | `		if( rc == SXERR_ABORT ){` |
|       - | 2706 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2707 | `			return SXERR_ABORT;` |
|       - | 2708 | `		}` |
|       5 | 2709 | `		return SXRET_OK;` |
|       - | 2710 | `	}` |
|       - | 2711 | `	/* Jump the trailing ';' */` |
|   10388 | 2712 | `	pGen->pIn++;` |
|       - | 2713 | `	/* Save the post condition stream */` |
|   10388 | 2714 | `	pPostStart = pGen->pIn;` |
|       - | 2715 | `	/* Compile the loop body */` |
|   10388 | 2716 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   10388 | 2717 | `	pGen->pEnd = pTmp;` |
|   10388 | 2718 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   10388 | 2719 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2720 | `		return SXERR_ABORT;` |
|       - | 2721 | `	}` |
|       - | 2722 | `	/* Fix post-continue jumps */` |
|   10388 | 2723 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 2724 | `		JumpFixup *aPost;` |
|       - | 2725 | `		VmInstr *pInstr;` |
|       - | 2726 | `		sxu32 nJumpDest;` |
|       - | 2727 | `		sxu32 n;` |
|       9 | 2728 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|       9 | 2729 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      17 | 2730 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|       9 | 2731 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       9 | 2732 | `			if( pInstr ){` |
|       - | 2733 | `				/* Fix jump */` |
|       9 | 2734 | `				pInstr->iP2 = nJumpDest;` |
|       4 | 2735 | `			}` |
|       5 | 2736 | `		}` |
|       4 | 2737 | `	}` |
|       - | 2738 | `	/* compile the post-expressions if available */` |
|   10388 | 2739 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2740 | `		pPostStart++;` |
|     ! 0 | 2741 | `	}` |
|   10388 | 2742 | `	if( pPostStart < pEnd ){` |
|       - | 2743 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   10388 | 2744 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   10388 | 2745 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10388 | 2746 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 2747 | `			/* Syntax error */` |
|     ! 0 | 2748 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 2749 | `			if( rc == SXERR_ABORT ){` |
|       - | 2750 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2751 | `				return SXERR_ABORT;` |
|       - | 2752 | `			}` |
|     ! 0 | 2753 | `			return SXRET_OK;` |
|       - | 2754 | `		}` |
|   10388 | 2755 | `		RE_SWAP_DELIMITER(pGen);` |
|   10388 | 2756 | `		if( rc == SXERR_ABORT ){` |
|       - | 2757 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2758 | `			return SXERR_ABORT;` |
|   10388 | 2759 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 2760 | `			/* Pop operand lvalue */` |
|   10388 | 2761 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5193 | 2762 | `		}` |
|    5193 | 2763 | `	}` |
|       - | 2764 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10388 | 2765 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 2766 | `	/* Fix all jumps now the destination is resolved */` |
|   10388 | 2767 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2768 | `	/* Release the loop block */` |
|   10388 | 2769 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2770 | `	/* Statement successfully compiled */` |
|   10388 | 2771 | `	return SXRET_OK;` |
|    5197 | 2772 |  |
|       - | 2773 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 2774 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 2775 | ` * are allowed.` |
|       - | 2776 | ` */` |
|    5548 | 2777 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 2778 |  |
|    5550 | 2779 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    5550 | 2780 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 2781 | `		/* Unexpected expression */` |
|     ! 0 | 2782 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 2783 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 2784 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 2785 | `			rc = SXERR_INVALID;` |
|     ! 0 | 2786 | `		}` |
|     ! 0 | 2787 | `	}` |
|    5550 | 2788 | `	return rc;` |
|       2 | 2789 |  |
|       - | 2790 | `/*` |
|       - | 2791 | ` * Compile the 'foreach' statement.` |
|       - | 2792 | ` * According to the PHP language reference` |
|       - | 2793 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - | 2794 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - | 2795 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - | 2796 | ` *  is a minor but useful extension of the first:` |
|       - | 2797 | ` *  foreach (array_expression as $value)` |
|       - | 2798 | ` *    statement` |
|       - | 2799 | ` *  foreach (array_expression as $key => $value)` |
|       - | 2800 | ` *   statement` |
|       - | 2801 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - | 2802 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - | 2803 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - | 2804 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - | 2805 | ` *  to the variable $key on each loop.` |
|       - | 2806 | ` *  Note:` |
|       - | 2807 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - | 2808 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - | 2809 | ` *  Note:` |
|       - | 2810 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - | 2811 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - | 2812 | ` *  or after the foreach without resetting it.` |
|       - | 2813 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - | 2814 | ` *  of copying the value.` |
|       - | 2815 | ` */` |
|    2822 | 2816 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 2817 |  |
|    2824 | 2818 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    2824 | 2819 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    2824 | 2820 | `	GenBlock *pForeachBlock = 0;` |
|       - | 2821 | `	ph7_foreach_info *pInfo;` |
|       - | 2822 | `	sxu32 nFalseJump;` |
|       - | 2823 | `	VmInstr *pInstr;` |
|       - | 2824 | `	sxu32 nLine;` |
|       - | 2825 | `	sxi32 rc;` |
|    2824 | 2826 | `	nLine = pGen->pIn->nLine;` |
|       - | 2827 | `	/* Jump the 'foreach' keyword */` |
|    2824 | 2828 | `	pGen->pIn++;` |
|    2824 | 2829 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2830 | `		/* Syntax error */` |
|     ! 0 | 2831 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 2832 | `		if( rc == SXERR_ABORT ){` |
|       - | 2833 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2834 | `			return SXERR_ABORT;` |
|       - | 2835 | `		}` |
|     ! 0 | 2836 | `		goto Synchronize;` |
|       - | 2837 | `	}` |
|       - | 2838 | `	/* Jump the left parenthesis '(' */` |
|    2824 | 2839 | `	pGen->pIn++;` |
|       - | 2840 | `	/* Create the loop block */` |
|    2824 | 2841 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    2824 | 2842 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2843 | `		return SXERR_ABORT;` |
|       - | 2844 | `	}` |
|       - | 2845 | `	/* Delimit the expression */` |
|    2824 | 2846 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    2824 | 2847 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2848 | `		/* Empty expression */` |
|     ! 0 | 2849 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 2850 | `		if( rc == SXERR_ABORT ){` |
|       - | 2851 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2852 | `			return SXERR_ABORT;` |
|       - | 2853 | `		}` |
|       - | 2854 | `		/* Synchronize */` |
|     ! 0 | 2855 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2856 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2857 | `			pGen->pIn++;` |
|     ! 0 | 2858 | `		}` |
|     ! 0 | 2859 | `		return SXRET_OK;` |
|       - | 2860 | `	}` |
|       - | 2861 | `	/* Compile the array expression */` |
|    2824 | 2862 | `	pCur = pGen->pIn;` |
|   18878 | 2863 | `	while( pCur < pEnd ){` |
|   18878 | 2864 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    2834 | 2865 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    2834 | 2866 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 2867 | `				/* Break with the first 'as' found */` |
|    2824 | 2868 | `				break;` |
|       - | 2869 | `			}` |
|       5 | 2870 | `		}` |
|       - | 2871 | `		/* Advance the stream cursor */` |
|   16056 | 2872 | `		pCur++;` |
|       2 | 2873 | `	}` |
|    2824 | 2874 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 2875 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2876 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 2877 | `		if( rc == SXERR_ABORT ){` |
|       - | 2878 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2879 | `			return SXERR_ABORT;` |
|       - | 2880 | `		}` |
|     ! 0 | 2881 | `		goto Synchronize;` |
|       - | 2882 | `	}` |
|       - | 2883 | `	/* Swap token streams */` |
|    2824 | 2884 | `	pTmp = pGen->pEnd;` |
|    2824 | 2885 | `	pGen->pEnd = pCur;` |
|    2824 | 2886 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    2824 | 2887 | `	if( rc == SXERR_ABORT ){` |
|       - | 2888 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2889 | `		return SXERR_ABORT;` |
|       - | 2890 | `	}` |
|       - | 2891 | `	/* Update token stream */` |
|    2824 | 2892 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 2893 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2894 | `		if( rc == SXERR_ABORT ){` |
|       - | 2895 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2896 | `			return SXERR_ABORT;` |
|       - | 2897 | `		}` |
|     ! 0 | 2898 | `		pGen->pIn++;` |
|     ! 0 | 2899 | `	}` |
|    2824 | 2900 | `	pCur++; /* Jump the 'as' keyword */` |
|    2824 | 2901 | `	pGen->pIn = pCur;` |
|    2824 | 2902 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2903 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 2904 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2905 | `			return SXERR_ABORT;` |
|       - | 2906 | `		}` |
|     ! 0 | 2907 | `	}` |
|       - | 2908 | `	/* Create the foreach context */` |
|    2824 | 2909 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    2824 | 2910 | `	if( pInfo == 0 ){` |
|     ! 0 | 2911 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 2912 | `		return SXERR_ABORT;` |
|       - | 2913 | `	}` |
|       - | 2914 | `	/* Zero the structure */` |
|    2824 | 2915 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 2916 | `	/* Initialize structure fields */` |
|    2824 | 2917 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 2918 | `	/* Check if we have a key field */` |
|    8508 | 2919 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    5686 | 2920 | `		pCur++;` |
|       2 | 2921 | `	}` |
|    2824 | 2922 | `	if( pCur < pEnd ){` |
|       - | 2923 | `		/* Compile the expression holding the key name */` |
|    2736 | 2924 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 2925 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 2926 | `			if( rc == SXERR_ABORT ){` |
|       - | 2927 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2928 | `				return SXERR_ABORT;` |
|       - | 2929 | `			}` |
|     ! 0 | 2930 | `		}else{` |
|    2736 | 2931 | `			pGen->pEnd = pCur;` |
|    2736 | 2932 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2736 | 2933 | `			if( rc == SXERR_ABORT ){` |
|       - | 2934 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2935 | `				return SXERR_ABORT;` |
|       - | 2936 | `			}` |
|    2736 | 2937 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2736 | 2938 | `			if( pInstr->p3 ){` |
|       - | 2939 | `				/* Record key name */` |
|    2736 | 2940 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1367 | 2941 | `			}` |
|    2736 | 2942 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 2943 | `		}` |
|    2736 | 2944 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1367 | 2945 | `	}` |
|    2824 | 2946 | `	pGen->pEnd = pEnd;` |
|    2824 | 2947 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2948 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 2949 | `		if( rc == SXERR_ABORT ){` |
|       - | 2950 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2951 | `			return SXERR_ABORT;` |
|       - | 2952 | `		}` |
|     ! 0 | 2953 | `		goto Synchronize;` |
|       - | 2954 | `	}` |
|    2824 | 2955 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 | 2956 | `		pGen->pIn++;` |
|       - | 2957 | `		/* Pass by reference  */` |
|      11 | 2958 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 | 2959 | `	}` |
|       - | 2960 | `	/* Check if the value target is list() */` |
|    2824 | 2961 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 2962 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 2963 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - | 2964 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - | 2965 | `		 */` |
|       - | 2966 | `		static int iForeachListCnt = 0;` |
|       - | 2967 | `		char zTmp[128];` |
|       - | 2968 | `		sxu32 nLen;` |
|       - | 2969 | `		char *zDup;` |
|      10 | 2970 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 | 2971 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 | 2972 | `		if( zDup == 0 ){` |
|     ! 0 | 2973 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2974 | `			return SXERR_ABORT;` |
|       - | 2975 | `		}` |
|      10 | 2976 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 2977 | `		/* Save list() token boundaries */` |
|      10 | 2978 | `		pListStart = pGen->pIn;` |
|       - | 2979 | `		/* Advance past list(...) — validate parentheses */` |
|      10 | 2980 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 | 2981 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 | 2982 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - | 2983 | `				"foreach: Expected '(' after 'list'");` |
|       3 | 2984 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2985 | `				return SXERR_ABORT;` |
|       - | 2986 | `			}` |
|       3 | 2987 | `			goto Synchronize;` |
|       - | 2988 | `		}` |
|       7 | 2989 | `		pGen->pIn++; /* Jump '(' */` |
|       7 | 2990 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 | 2991 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 2992 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 2993 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 | 2994 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2995 | `				return SXERR_ABORT;` |
|       - | 2996 | `			}` |
|     ! 0 | 2997 | `			goto Synchronize;` |
|       - | 2998 | `		}` |
|       7 | 2999 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 | 3000 | `		pListEnd = pGen->pIn;` |
|       7 | 3001 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       4 | 3002 | `	}else{` |
|       - | 3003 | `		/* Compile the expression holding the value name */` |
|    2816 | 3004 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2816 | 3005 | `		if( rc == SXERR_ABORT ){` |
|       - | 3006 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3007 | `			return SXERR_ABORT;` |
|       - | 3008 | `		}` |
|    2816 | 3009 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2816 | 3010 | `		if( pInstr->p3 ){` |
|       - | 3011 | `			/* Record value name */` |
|    2816 | 3012 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1407 | 3013 | `		}` |
|       - | 3014 | `	}` |
|       - | 3015 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    2822 | 3016 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 3017 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2822 | 3018 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 3019 | `	/* Record the first instruction to execute */` |
|    2822 | 3020 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3021 | `	/* Emit the FOREACH_STEP instruction */` |
|    2822 | 3022 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 3023 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2822 | 3024 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 3025 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    2822 | 3026 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - | 3027 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - | 3028 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - | 3029 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - | 3030 | `		 */` |
|       7 | 3031 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - | 3032 | `		/* Compile list(...) body directly — this pushes variables and emits LOAD_LIST.` |
|       - | 3033 | `		 * We position the tokens at the list keyword so PH7_CompileList picks up` |
|       - | 3034 | `		 * the opening '(' and the variable names inside.` |
|       - | 3035 | `		 */` |
|       7 | 3036 | `		pSavedIn = pGen->pIn;` |
|       7 | 3037 | `		pSavedEnd = pGen->pEnd;` |
|       7 | 3038 | `		pGen->pIn = pListStart;` |
|       7 | 3039 | `		pGen->pEnd = pListEnd;` |
|       7 | 3040 | `		rc = PH7_CompileList(&(*pGen),0);` |
|       7 | 3041 | `		pGen->pIn = pSavedIn;` |
|       7 | 3042 | `		pGen->pEnd = pSavedEnd;` |
|       7 | 3043 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3044 | `			return SXERR_ABORT;` |
|       - | 3045 | `		}` |
|       - | 3046 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       7 | 3047 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       3 | 3048 | `	}` |
|       - | 3049 | `	/* Compile the loop body */` |
|    2822 | 3050 | `	pGen->pIn = &pEnd[1];` |
|    2822 | 3051 | `	pGen->pEnd = pTmp;` |
|    2822 | 3052 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    2822 | 3053 | `	if( rc == SXERR_ABORT ){` |
|       - | 3054 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3055 | `		return SXERR_ABORT;` |
|       - | 3056 | `	}` |
|       - | 3057 | `	/* Emit the unconditional jump to the start of the loop */` |
|    2822 | 3058 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 3059 | `	/* Fix all jumps now the destination is resolved */` |
|    2822 | 3060 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3061 | `	/* Release the loop block */` |
|    2822 | 3062 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3063 | `	/* Statement successfully compiled */` |
|    2822 | 3064 | `	return SXRET_OK;` |
|       1 | 3065 | `Synchronize:` |
|       - | 3066 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 3067 | `	 * compiling this erroneous block.` |
|       - | 3068 | `	 */` |
|       3 | 3069 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3070 | `		pGen->pIn++;` |
|     ! 0 | 3071 | `	}` |
|       3 | 3072 | `	return SXRET_OK;` |
|    1413 | 3073 |  |
|       - | 3074 | `/*` |
|       - | 3075 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 3076 | ` * According to the PHP language reference` |
|       - | 3077 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 3078 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 3079 | ` *  that is similar to that of C:` |
|       - | 3080 | ` *  if (expr)` |
|       - | 3081 | ` *   statement` |
|       - | 3082 | ` *  else construct:` |
|       - | 3083 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 3084 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 3085 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 3086 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 3087 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 3088 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 3089 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 3090 | ` *  elseif` |
|       - | 3091 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 3092 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 3093 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 3094 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 3095 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 3096 | ` *   <?php` |
|       - | 3097 | ` *    if ($a > $b) {` |
|       - | 3098 | ` *     echo "a is bigger than b";` |
|       - | 3099 | ` *    } elseif ($a == $b) {` |
|       - | 3100 | ` *     echo "a is equal to b";` |
|       - | 3101 | ` *    } else {` |
|       - | 3102 | ` *     echo "a is smaller than b";` |
|       - | 3103 | ` *    }` |
|       - | 3104 | ` *    ?>` |
|       - | 3105 | ` */` |
|  103494 | 3106 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 3107 |  |
|  103496 | 3108 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  103496 | 3109 | `	GenBlock *pCondBlock = 0;` |
|       - | 3110 | `	sxu32 nJumpIdx;` |
|       - | 3111 | `	sxu32 nKeyID;` |
|       - | 3112 | `	sxi32 rc;` |
|       - | 3113 | `	/* Jump the 'if' keyword */` |
|  103496 | 3114 | `	pGen->pIn++;` |
|  103496 | 3115 | `	pToken = pGen->pIn;` |
|       - | 3116 | `	/* Create the conditional block */` |
|  103496 | 3117 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  103496 | 3118 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3119 | `		return SXERR_ABORT;` |
|       - | 3120 | `	}` |
|       - | 3121 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   56911 | 3122 | `	for(;;){` |
|  113824 | 3123 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3124 | `			/* Syntax error */` |
|     ! 0 | 3125 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3126 | `				pToken--;` |
|     ! 0 | 3127 | `			}` |
|     ! 0 | 3128 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 3129 | `			if( rc == SXERR_ABORT ){` |
|       - | 3130 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3131 | `				return SXERR_ABORT;` |
|       - | 3132 | `			}` |
|     ! 0 | 3133 | `			goto Synchronize;` |
|       - | 3134 | `		}` |
|       - | 3135 | `		/* Jump the left parenthesis '(' */` |
|  113824 | 3136 | `		pToken++;` |
|       - | 3137 | `		/* Delimit the condition */` |
|  113824 | 3138 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  113824 | 3139 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 3140 | `			/* Syntax error */` |
|     ! 0 | 3141 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3142 | `				pToken--;` |
|     ! 0 | 3143 | `			}` |
|     ! 0 | 3144 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 3145 | `			if( rc == SXERR_ABORT ){` |
|       - | 3146 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3147 | `				return SXERR_ABORT;` |
|       - | 3148 | `			}` |
|     ! 0 | 3149 | `			goto Synchronize;` |
|       - | 3150 | `		}` |
|       - | 3151 | `		/* Swap token streams */` |
|  113824 | 3152 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 3153 | `		/* Compile the condition */` |
|  113824 | 3154 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3155 | `		/* Update token stream */` |
|  113824 | 3156 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 3157 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3158 | `			pGen->pIn++;` |
|     ! 0 | 3159 | `		}` |
|  113824 | 3160 | `		pGen->pIn  = &pEnd[1];` |
|  113824 | 3161 | `		pGen->pEnd = pTmp;` |
|  113824 | 3162 | `		if( rc == SXERR_ABORT ){` |
|       - | 3163 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3164 | `			return SXERR_ABORT;` |
|       - | 3165 | `		}` |
|       - | 3166 | `		/* Emit the false jump */` |
|  113824 | 3167 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 3168 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  113824 | 3169 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 3170 | `		/* Compile the body */` |
|  113824 | 3171 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  113824 | 3172 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3173 | `			return SXERR_ABORT;` |
|       - | 3174 | `		}` |
|  113824 | 3175 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   30644 | 3176 | `			break;` |
|       - | 3177 | `		}` |
|       - | 3178 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   52540 | 3179 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   52540 | 3180 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   33762 | 3181 | `			break;` |
|       - | 3182 | `		}` |
|       - | 3183 | `		/* Emit the unconditional jump */` |
|   18780 | 3184 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 3185 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   18780 | 3186 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   18780 | 3187 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   13604 | 3188 | `			pToken = &pGen->pIn[1];` |
|   13604 | 3189 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5182 | 3190 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4227 | 3191 | `					break;` |
|       - | 3192 | `			}` |
|    5154 | 3193 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2576 | 3194 | `		}` |
|   10330 | 3195 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 3196 | `		/* Synchronize cursors */` |
|   10330 | 3197 | `		pToken = pGen->pIn;` |
|       - | 3198 | `		/* Fix the false jump */` |
|   10330 | 3199 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 3200 | `	} /* For(;;) */` |
|       - | 3201 | `	/* Fix the false jump */` |
|  103496 | 3202 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  103496 | 3203 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   42210 | 3204 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 3205 | `			/* Compile the else block */` |
|    8452 | 3206 | `			pGen->pIn++;` |
|    8452 | 3207 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    8452 | 3208 | `			if( rc == SXERR_ABORT ){` |
|       - | 3209 |  |
|     ! 0 | 3210 | `				return SXERR_ABORT;` |
|       - | 3211 | `			}` |
|    4225 | 3212 | `	}` |
|  103496 | 3213 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3214 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  103496 | 3215 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 3216 | `	/* Release the conditional block */` |
|  103496 | 3217 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3218 | `	/* Statement successfully compiled */` |
|  103496 | 3219 | `	return SXRET_OK;` |
|     ! 0 | 3220 | `Synchronize:` |
|       - | 3221 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 3222 | `	 */` |
|     ! 0 | 3223 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3224 | `		pGen->pIn++;` |
|     ! 0 | 3225 | `	}` |
|     ! 0 | 3226 | `	return SXRET_OK;` |
|   51749 | 3227 |  |
|       - | 3228 | `/*` |
|       - | 3229 | ` * Compile the global construct.` |
|       - | 3230 | ` * According to the PHP language reference` |
|       - | 3231 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 3232 | ` *  to be used in that function.` |
|       - | 3233 | ` *  Example #1 Using global` |
|       - | 3234 | ` *  <?php` |
|       - | 3235 | ` *   $a = 1;` |
|       - | 3236 | ` *   $b = 2;` |
|       - | 3237 | ` *   function Sum()` |
|       - | 3238 | ` *   {` |
|       - | 3239 | ` *    global $a, $b;` |
|       - | 3240 | ` *    $b = $a + $b;` |
|       - | 3241 | ` *   }` |
|       - | 3242 | ` *   Sum();` |
|       - | 3243 | ` *   echo $b;` |
|       - | 3244 | ` *  ?>` |
|       - | 3245 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 3246 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 3247 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 3248 | ` */` |
|      26 | 3249 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 | 3250 |  |
|      28 | 3251 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3252 | `	sxi32 nExpr;` |
|       - | 3253 | `	sxi32 rc;` |
|       - | 3254 | `	/* Jump the 'global' keyword */` |
|      28 | 3255 | `	pGen->pIn++;` |
|      28 | 3256 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 3257 | `		/* Nothing to process */` |
|     ! 0 | 3258 | `		return SXRET_OK;` |
|       - | 3259 | `	}` |
|      28 | 3260 | `	pTmp = pGen->pEnd;` |
|      28 | 3261 | `	nExpr = 0;` |
|      56 | 3262 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      30 | 3263 | `		if( pGen->pIn < pNext ){` |
|      30 | 3264 | `			pGen->pEnd = pNext;` |
|      30 | 3265 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3266 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 3267 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 3268 | `					return SXERR_ABORT;` |
|       - | 3269 | `				}` |
|     ! 0 | 3270 | `			}else{` |
|      30 | 3271 | `				pGen->pIn++;` |
|      30 | 3272 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3273 | `					/* Emit a warning */` |
|     ! 0 | 3274 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 3275 | `				}else{` |
|      30 | 3276 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 | 3277 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3278 | `						return SXERR_ABORT;` |
|      30 | 3279 | `					}else if(rc != SXERR_EMPTY ){` |
|      30 | 3280 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      30 | 3281 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - | 3282 | `							/* Variable name, not a constant */` |
|      30 | 3283 | `							pLast->iP1 = 0;` |
|      14 | 3284 | `						}` |
|      30 | 3285 | `						nExpr++;` |
|      14 | 3286 | `					}` |
|       - | 3287 | `				}` |
|       - | 3288 | `			}` |
|      14 | 3289 | `		}` |
|       - | 3290 | `		/* Next expression in the stream */` |
|      30 | 3291 | `		pGen->pIn = pNext;` |
|       - | 3292 | `		/* Jump trailing commas */` |
|      32 | 3293 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3294 | `			pGen->pIn++;` |
|       1 | 3295 | `		}` |
|       2 | 3296 | `	}` |
|       - | 3297 | `	/* Restore token stream */` |
|      28 | 3298 | `	pGen->pEnd = pTmp;` |
|      28 | 3299 | `	if( nExpr > 0 ){` |
|       - | 3300 | `		/* Emit the uplink instruction */` |
|      28 | 3301 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      13 | 3302 | `	}` |
|      28 | 3303 | `	return SXRET_OK;` |
|      15 | 3304 |  |
|       - | 3305 | `/*` |
|       - | 3306 | ` * Compile the return statement.` |
|       - | 3307 | ` * According to the PHP language reference` |
|       - | 3308 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 3309 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 3310 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 3311 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 3312 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 3313 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 3314 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 3315 | ` *  from within the main script file, then script execution end.` |
|       - | 3316 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 3317 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 3318 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 3319 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 3320 | ` */` |
|  150036 | 3321 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 3322 |  |
|  150038 | 3323 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3324 | `	sxi32 rc;` |
|       - | 3325 | `	/* Jump the 'return' keyword */` |
|  150038 | 3326 | `	pGen->pIn++;` |
|  150038 | 3327 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3328 | `		/* Compile the expression */` |
|  150016 | 3329 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  150016 | 3330 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3331 | `			return SXERR_ABORT;` |
|  150016 | 3332 | `		}else if(rc != SXERR_EMPTY ){` |
|  150016 | 3333 | `			nRet = 1;` |
|   75007 | 3334 | `		}` |
|   75007 | 3335 | `	}` |
|       - | 3336 | `	/* Emit the done instruction */` |
|  150038 | 3337 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  150038 | 3338 | `	return SXRET_OK;` |
|   75020 | 3339 |  |
|       - | 3340 | `/*` |
|       - | 3341 | ` * Compile a yield expression.` |
|       - | 3342 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - | 3343 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - | 3344 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - | 3345 | ` */` |
|      32 | 3346 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 3347 |  |
|       - | 3348 | `	SyToken *pTmp, *pSplit;` |
|      34 | 3349 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      34 | 3350 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - | 3351 | `	sxi32 rc;` |
|      16 | 3352 | `	(void)iCompileFlag;` |
|       - | 3353 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      34 | 3354 | `	pGen->pIn++;` |
|       - | 3355 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - | 3356 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      34 | 3357 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3358 | `		/* Bare yield — no value */` |
|     ! 0 | 3359 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 | 3360 | `		return SXRET_OK;` |
|       - | 3361 | `	}` |
|       - | 3362 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      34 | 3363 | `	pSplit = 0;` |
|       - | 3364 | `	{` |
|      34 | 3365 | `		SyToken *pCur = pGen->pIn;` |
|      34 | 3366 | `		sxi32 nNest = 0;` |
|      78 | 3367 | `		while( pCur < pGen->pEnd ){` |
|      52 | 3368 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 | 3369 | `				nNest++;` |
|      52 | 3370 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 | 3371 | `				nNest--;` |
|      52 | 3372 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 | 3373 | `				pSplit = pCur;` |
|       7 | 3374 | `				break;` |
|       - | 3375 | `			}` |
|      46 | 3376 | `			pCur++;` |
|       2 | 3377 | `		}` |
|       - | 3378 | `	}` |
|      34 | 3379 | `	pTmp = pGen->pEnd;` |
|      34 | 3380 | `	if( pSplit ){` |
|       - | 3381 | `		/* yield $key => $value */` |
|       7 | 3382 | `		pGen->pEnd = pSplit;` |
|       7 | 3383 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 3384 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 3385 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       7 | 3386 | `		pGen->pEnd = pTmp;` |
|       7 | 3387 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 3388 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 3389 | `		iP1 = 1;` |
|       7 | 3390 | `		iP2 = 1;` |
|       4 | 3391 | `	}else{` |
|       - | 3392 | `		/* yield $value */` |
|      28 | 3393 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      28 | 3394 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      28 | 3395 | `		if( rc != SXERR_EMPTY ){` |
|      28 | 3396 | `			iP1 = 1;` |
|      13 | 3397 | `		}` |
|       - | 3398 | `	}` |
|      34 | 3399 | `	pGen->pEnd = pTmp;` |
|      34 | 3400 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      34 | 3401 | `	return SXRET_OK;` |
|      18 | 3402 |  |
|       - | 3403 | `/*` |
|       - | 3404 | ` * Compile the die/exit language construct.` |
|       - | 3405 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 3406 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 3407 | ` */` |
|      88 | 3408 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 | 3409 |  |
|      90 | 3410 | `	sxi32 nExpr = 0;` |
|       - | 3411 | `	sxi32 rc;` |
|       - | 3412 | `	/* Jump the die/exit keyword */` |
|      90 | 3413 | `	pGen->pIn++;` |
|      90 | 3414 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3415 | `		/* Compile the expression */` |
|      90 | 3416 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 | 3417 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3418 | `			return SXERR_ABORT;` |
|      90 | 3419 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 | 3420 | `			nExpr = 1;` |
|      44 | 3421 | `		}` |
|      44 | 3422 | `	}` |
|       - | 3423 | `	/* Emit the HALT instruction */` |
|      90 | 3424 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 | 3425 | `	return SXRET_OK;` |
|      46 | 3426 |  |
|       - | 3427 | `/*` |
|       - | 3428 | ` * Compile the 'echo' language construct.` |
|       - | 3429 | ` */` |
|   10512 | 3430 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 3431 |  |
|   10514 | 3432 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3433 | `	sxi32 rc;` |
|       - | 3434 | `	/* Jump the 'echo' keyword */` |
|   10514 | 3435 | `	pGen->pIn++;` |
|       - | 3436 | `	/* Compile arguments one after one */` |
|   10514 | 3437 | `	pTmp = pGen->pEnd;` |
|   21414 | 3438 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   10902 | 3439 | `		if( pGen->pIn < pNext ){` |
|   10902 | 3440 | `			pGen->pEnd = pNext;` |
|   10902 | 3441 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   10902 | 3442 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3443 | `				return SXERR_ABORT;` |
|   10902 | 3444 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3445 | `				/* Emit the consume instruction */` |
|   10878 | 3446 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    5438 | 3447 | `			}` |
|    5450 | 3448 | `		}` |
|       - | 3449 | `		/* Jump trailing commas */` |
|   11290 | 3450 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     390 | 3451 | `			pNext++;` |
|       2 | 3452 | `		}` |
|   10902 | 3453 | `		pGen->pIn = pNext;` |
|       2 | 3454 | `	}` |
|       - | 3455 | `	/* Restore token stream */` |
|   10514 | 3456 | `	pGen->pEnd = pTmp;` |
|   10514 | 3457 | `	return SXRET_OK;` |
|    5258 | 3458 |  |
|       - | 3459 | `/*` |
|       - | 3460 | ` * Compile the static statement.` |
|       - | 3461 | ` * According to the PHP language reference` |
|       - | 3462 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 3463 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 3464 | ` *  when program execution leaves this scope.` |
|       - | 3465 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 3466 | ` * Symisc eXtension.` |
|       - | 3467 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 3468 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3469 | ` *  Example` |
|       - | 3470 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 3471 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 3472 | ` */` |
|       2 | 3473 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 | 3474 |  |
|       - | 3475 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 3476 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 3477 | `	GenBlock *pBlock;` |
|       - | 3478 | `	SyString *pName;` |
|       - | 3479 | `	char *zDup;` |
|       - | 3480 | `	sxu32 nLine;` |
|       - | 3481 | `	sxi32 rc;` |
|       - | 3482 | `	/* Jump the static keyword */` |
|       3 | 3483 | `	nLine = pGen->pIn->nLine;` |
|       3 | 3484 | `	pGen->pIn++;` |
|       - | 3485 | `	/* Extract the enclosing function if any */` |
|       3 | 3486 | `	pBlock = pGen->pCurrent;` |
|       5 | 3487 | `	while( pBlock ){` |
|       5 | 3488 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 | 3489 | `			break;` |
|       - | 3490 | `		}` |
|       - | 3491 | `		/* Point to the upper block */` |
|       3 | 3492 | `		pBlock = pBlock->pParent;` |
|       1 | 3493 | `	}` |
|       3 | 3494 | `	if( pBlock == 0 ){` |
|       - | 3495 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 3496 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3497 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 | 3498 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3499 | `				return SXERR_ABORT;` |
|       - | 3500 | `			}` |
|     ! 0 | 3501 | `			goto Synchronize;` |
|       - | 3502 | `		}` |
|       - | 3503 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 3504 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 3505 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3506 | `			return SXERR_ABORT;` |
|     ! 0 | 3507 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 3508 | `			/* Emit the POP instruction */` |
|     ! 0 | 3509 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 3510 | `		}` |
|     ! 0 | 3511 | `		return SXRET_OK;` |
|       - | 3512 | `	}` |
|       3 | 3513 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 3514 | `	/* Make sure we are dealing with a valid statement */` |
|       3 | 3515 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 | 3516 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 | 3517 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 | 3518 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3519 | `				return SXERR_ABORT;` |
|       - | 3520 | `			}` |
|       3 | 3521 | `			goto Synchronize;` |
|       - | 3522 | `	}` |
|     ! 0 | 3523 | `	pGen->pIn++;` |
|       - | 3524 | `	/* Extract variable name */` |
|     ! 0 | 3525 | `	pName = &pGen->pIn->sData;` |
|     ! 0 | 3526 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 | 3527 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 3528 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3529 | `		goto Synchronize;` |
|       - | 3530 | `	}` |
|       - | 3531 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 | 3532 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 | 3533 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 3534 | `	/* Duplicate variable name */` |
|     ! 0 | 3535 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 | 3536 | `	if( zDup == 0 ){` |
|     ! 0 | 3537 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3538 | `		return SXERR_ABORT;` |
|       - | 3539 | `	}` |
|     ! 0 | 3540 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 3541 | `	/* Check if we have an expression to compile */` |
|     ! 0 | 3542 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 3543 | `		SySet *pInstrContainer;` |
|       - | 3544 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 3545 | `		 * Static variable can take any complex expression including function` |
|       - | 3546 | `		 * call as their initialization value.` |
|       - | 3547 | `		 * Example:` |
|       - | 3548 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 3549 | `		 */` |
|     ! 0 | 3550 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 3551 | `		/* Swap bytecode container */` |
|     ! 0 | 3552 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 | 3553 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 3554 | `		/* Compile the expression */` |
|     ! 0 | 3555 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3556 | `		/* Emit the done instruction */` |
|     ! 0 | 3557 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 3558 | `		/* Restore default bytecode container */` |
|     ! 0 | 3559 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 3560 | `	}` |
|       - | 3561 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 | 3562 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 | 3563 | `	return SXRET_OK;` |
|       1 | 3564 | `Synchronize:` |
|       - | 3565 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 3566 | `	 * statement.` |
|       - | 3567 | `	 */` |
|       5 | 3568 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 | 3569 | `		pGen->pIn++;` |
|       1 | 3570 | `	}` |
|       3 | 3571 | `	return SXRET_OK;` |
|       2 | 3572 |  |
|       - | 3573 | `/*` |
|       - | 3574 | ` * Compile the var statement.` |
|       - | 3575 | ` * Symisc Extension:` |
|       - | 3576 | ` *      var statement can be used outside of a class definition.` |
|       - | 3577 | ` */` |
|       4 | 3578 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 3579 |  |
|       - | 3580 | `	sxu32 nLine;` |
|       - | 3581 | `	sxi32 rc;` |
|       5 | 3582 | `	nLine = pGen->pIn->nLine;` |
|       - | 3583 | `	/* Jump the 'var' keyword */` |
|       5 | 3584 | `	pGen->pIn++;` |
|       5 | 3585 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 3586 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - | 3587 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 | 3588 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 | 3589 | `			pGen->pIn++;` |
|     ! 0 | 3590 | `		}` |
|     ! 0 | 3591 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3592 | `			return SXERR_ABORT;` |
|       - | 3593 | `		}` |
|     ! 0 | 3594 | `	}else{` |
|       - | 3595 | `		/* Compile the expression */` |
|       5 | 3596 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 | 3597 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3598 | `			return SXERR_ABORT;` |
|       5 | 3599 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 | 3600 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 3601 | `		}` |
|       - | 3602 | `	}` |
|       5 | 3603 | `	return SXRET_OK;` |
|       3 | 3604 |  |
|       - | 3605 | `/*` |
|       - | 3606 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 3607 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 3608 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 3609 | ` */` |
|       - | 3610 | `/*` |
|       - | 3611 | ` * Namespace-qualify a name for CALL/NEW instructions.` |
|       - | 3612 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 3613 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 3614 | ` * qualified name and updates the instruction's operand index.` |
|       - | 3615 | ` *` |
|       - | 3616 | ` * Resolution: use imports -> current NS prefix.` |
|       - | 3617 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 3618 | ` * Returns the (possibly new) literal index.` |
|       - | 3619 | ` */` |
|  308194 | 3620 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx)` |
|       2 | 3621 |  |
|       - | 3622 | `	ph7_value *pLit;` |
|       - | 3623 | `	const char *zLit;` |
|       - | 3624 | `	SyString sQualified;` |
|       - | 3625 | `	sxu32 nLit;` |
|       - | 3626 | `	sxu32 k;` |
|       - | 3627 | `	sxu32 nNewIdx;` |
|       - | 3628 | `	int hasNsSep;` |
|       - | 3629 | `	SyHashEntry *pImport;` |
|       - | 3630 | `	ph7_value *pNew;` |
|  308196 | 3631 | `	if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  308074 | 3632 | `		return nOrigIdx; /* Not in a namespace */` |
|       - | 3633 | `	}` |
|     124 | 3634 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|     124 | 3635 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 3636 | `		return nOrigIdx;` |
|       - | 3637 | `	}` |
|     124 | 3638 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|     124 | 3639 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 3640 | `	/* Skip if already qualified (contains backslash) */` |
|     124 | 3641 | `	hasNsSep = 0;` |
|     680 | 3642 | `	for( k = 0; k < nLit; k++ ){` |
|     612 | 3643 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|     280 | 3644 | `	}` |
|     124 | 3645 | `	if( hasNsSep ){` |
|      56 | 3646 | `		return nOrigIdx;` |
|       - | 3647 | `	}` |
|       - | 3648 | `	/* Build the qualified name into sWorker */` |
|      70 | 3649 | `	SyBlobReset(&pGen->sWorker);` |
|       - | 3650 | `	/* Check use imports first */` |
|      70 | 3651 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zLit,nLit);` |
|      70 | 3652 | `	if( pImport ){` |
|      23 | 3653 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      23 | 3654 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      12 | 3655 | `	}else{` |
|       - | 3656 | `		/* Prepend current namespace */` |
|      48 | 3657 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      48 | 3658 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      48 | 3659 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 3660 | `	}` |
|       - | 3661 | `	/* Look up or create a new literal for the qualified name */` |
|      70 | 3662 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      70 | 3663 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      23 | 3664 | `		return nNewIdx; /* Already interned */` |
|       - | 3665 | `	}` |
|      48 | 3666 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      48 | 3667 | `	if( pNew == 0 ){` |
|     ! 0 | 3668 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 3669 | `	}` |
|      48 | 3670 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      48 | 3671 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      48 | 3672 | `	return nNewIdx;` |
|  154099 | 3673 |  |
|       - | 3674 | `/*` |
|       - | 3675 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 3676 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 3677 | ` */` |
|   20766 | 3678 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3679 |  |
|       - | 3680 | `	SyHashEntry *pImport;` |
|       - | 3681 | `	/* Check use imports first */` |
|   20768 | 3682 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   20768 | 3683 | `	if( pImport ){` |
|       7 | 3684 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       7 | 3685 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       7 | 3686 | `		return;` |
|       - | 3687 | `	}` |
|       - | 3688 | `	/* Prepend current namespace if active */` |
|   20762 | 3689 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 3690 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 3691 | `		SyBlobAppend(pOut,"\\",1);` |
|       1 | 3692 | `	}` |
|   20762 | 3693 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   10385 | 3694 |  |
|       - | 3695 | `/*` |
|       - | 3696 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 3697 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 3698 | ` * The caller must release pOut when done.` |
|       - | 3699 | ` */` |
|   39140 | 3700 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3701 |  |
|   39142 | 3702 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      38 | 3703 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      38 | 3704 | `		SyBlobAppend(pOut,"\\",1);` |
|      18 | 3705 | `	}` |
|   39142 | 3706 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   39142 | 3707 |  |
|       - | 3708 | `/*` |
|       - | 3709 | ` * Compile a namespace statement` |
|       - | 3710 | ` * According to the PHP language reference manual` |
|       - | 3711 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 3712 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 3713 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 3714 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 3715 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 3716 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 3717 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 3718 | ` *  programming world.` |
|       - | 3719 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 3720 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 3721 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 3722 | ` *  classes/functions/constants.` |
|       - | 3723 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 3724 | ` *  readability of source code.` |
|       - | 3725 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 3726 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 3727 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 3728 | ` *       class MyClass {}` |
|       - | 3729 | ` *       function myfunction() {}` |
|       - | 3730 | ` *       const MYCONST = 1;` |
|       - | 3731 | ` *       $a = new MyClass;` |
|       - | 3732 | ` *       $c = new \my\name\MyClass;` |
|       - | 3733 | ` *       $a = strlen('hi');` |
|       - | 3734 | ` *       $d = namespace\MYCONST;` |
|       - | 3735 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 3736 | ` *       echo constant($d);` |
|       - | 3737 | ` * NOTE` |
|       - | 3738 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3739 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3740 | ` */` |
|       - | 3741 | `/*` |
|       - | 3742 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 3743 | ` */` |
|       6 | 3744 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 | 3745 |  |
|       7 | 3746 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|     ! 0 | 3747 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|     ! 0 | 3748 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|     ! 0 | 3749 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|     ! 0 | 3750 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|     ! 0 | 3751 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|     ! 0 | 3752 | `	return "token";` |
|       4 | 3753 |  |
|      66 | 3754 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       2 | 3755 |  |
|       - | 3756 | `	sxu32 nLine;` |
|       - | 3757 | `	sxi32 rc;` |
|      68 | 3758 | `	nLine = pGen->pIn->nLine;` |
|      68 | 3759 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 3760 | `	/* Reset namespace and clear previous use imports */` |
|      68 | 3761 | `	SyBlobReset(&pGen->sNamespace);` |
|      68 | 3762 | `	SyHashRelease(&pGen->hUseImports);` |
|      68 | 3763 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      68 | 3764 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3765 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 | 3766 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3767 | `		return SXRET_OK;` |
|       - | 3768 | `	}` |
|      68 | 3769 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - | 3770 | `		/* namespace; — switch to global namespace */` |
|     ! 0 | 3771 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3772 | `		return SXRET_OK;` |
|       - | 3773 | `	}` |
|      68 | 3774 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - | 3775 | `		/* namespace { } — global namespace block */` |
|     ! 0 | 3776 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3777 | `		return SXRET_OK;` |
|       - | 3778 | `	}` |
|       - | 3779 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     172 | 3780 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     106 | 3781 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 3782 | `			/* Append backslash separator */` |
|      21 | 3783 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      21 | 3784 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      10 | 3785 | `			}` |
|      11 | 3786 | `		}else{` |
|       - | 3787 | `			/* Append identifier */` |
|      86 | 3788 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 3789 | `		}` |
|     106 | 3790 | `		pGen->pIn++;` |
|       2 | 3791 | `	}` |
|       - | 3792 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - | 3793 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - | 3794 | `	{` |
|      68 | 3795 | `		char *zNsDup = 0;` |
|      68 | 3796 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      98 | 3797 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      64 | 3798 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      32 | 3799 | `		}` |
|      68 | 3800 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - | 3801 | `	}` |
|      68 | 3802 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 | 3803 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3804 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 3805 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 | 3806 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3807 | `			return SXERR_ABORT;` |
|       - | 3808 | `		}` |
|       2 | 3809 | `	}` |
|      68 | 3810 | `	return SXRET_OK;` |
|      35 | 3811 |  |
|       - | 3812 | `/*` |
|       - | 3813 | ` * Compile the 'use' statement` |
|       - | 3814 | ` * According to the PHP language reference manual` |
|       - | 3815 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 3816 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 3817 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 3818 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 3819 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 3820 | ` *  a function or constant is not supported.` |
|       - | 3821 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 3822 | ` * NOTE` |
|       - | 3823 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3824 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3825 | ` */` |
|      42 | 3826 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       2 | 3827 |  |
|       - | 3828 | `	sxu32 nLine;` |
|       - | 3829 | `	sxi32 rc;` |
|       - | 3830 | `	SyBlob sPath;` |
|       - | 3831 | `	SyString sAlias;` |
|       - | 3832 | `	SyToken *pLast;` |
|       - | 3833 | `	char *zDup;` |
|      44 | 3834 | `	nLine = pGen->pIn->nLine;` |
|      44 | 3835 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - | 3836 | `	/* Skip 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      44 | 3837 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       9 | 3838 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       9 | 3839 | `		if( nKey == PH7_TKWRD_FUNCTION \|\| nKey == PH7_TKWRD_CONST ){` |
|       9 | 3840 | `			pGen->pIn++;` |
|       4 | 3841 | `		}` |
|       4 | 3842 | `	}` |
|      44 | 3843 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 3844 | `	/* Process one or more use declarations separated by commas */` |
|      22 | 3845 | `	for(;;){` |
|      46 | 3846 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3847 | `			break;` |
|       - | 3848 | `		}` |
|      46 | 3849 | `		SyBlobReset(&sPath);` |
|      46 | 3850 | `		pLast = 0;` |
|       - | 3851 | `		/* Collect the full namespace path */` |
|     174 | 3852 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     130 | 3853 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      86 | 3854 | `				pLast = pGen->pIn;` |
|      86 | 3855 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      46 | 3856 | `					SyBlobAppend(&sPath,"\\",1);` |
|      22 | 3857 | `				}` |
|      86 | 3858 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      42 | 3859 | `			}` |
|     130 | 3860 | `			pGen->pIn++;` |
|       2 | 3861 | `		}` |
|      46 | 3862 | `		if( pLast == 0 ){` |
|       - | 3863 | `			/* Empty path */` |
|       5 | 3864 | `			break;` |
|       - | 3865 | `		}` |
|       - | 3866 | `		/* Default alias is the last component of the path */` |
|      42 | 3867 | `		sAlias = pLast->sData;` |
|       - | 3868 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      40 | 3869 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      26 | 3870 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      10 | 3871 | `			pGen->pIn++; /* Jump 'as' */` |
|      10 | 3872 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      10 | 3873 | `				sAlias = pGen->pIn->sData;` |
|      10 | 3874 | `				pGen->pIn++;` |
|       4 | 3875 | `			}` |
|       4 | 3876 | `		}` |
|       - | 3877 | `		/* Check for duplicate import alias */` |
|      42 | 3878 | `		if( SyHashGet(&pGen->hUseImports,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 | 3879 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3880 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 | 3881 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 | 3882 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3883 | `				SyBlobRelease(&sPath);` |
|     ! 0 | 3884 | `				return SXERR_ABORT;` |
|       - | 3885 | `			}` |
|       2 | 3886 | `		}` |
|       - | 3887 | `		/* Register the import: alias -> FQN.` |
|       - | 3888 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - | 3889 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - | 3890 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      62 | 3891 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      40 | 3892 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      42 | 3893 | `		if( zDup ){` |
|       - | 3894 | `			char *zAliasDup;` |
|      42 | 3895 | `			SyHashInsert(&pGen->hUseImports,sAlias.zString,sAlias.nByte,zDup);` |
|       - | 3896 | `			/* Duplicate the alias key for the VM hash (token pointers may not survive to runtime) */` |
|      42 | 3897 | `			zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      42 | 3898 | `			if( zAliasDup ){` |
|      42 | 3899 | `				SyHashInsert(&pGen->pVm->hUseImports,zAliasDup,sAlias.nByte,zDup);` |
|      20 | 3900 | `			}` |
|      20 | 3901 | `		}` |
|       - | 3902 | `		/* Check for comma (multiple use declarations) */` |
|      42 | 3903 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3904 | `			pGen->pIn++;` |
|       2 | 3905 | `		}else{` |
|      21 | 3906 | `			break;` |
|       - | 3907 | `		}` |
|       1 | 3908 | `	}` |
|      44 | 3909 | `	SyBlobRelease(&sPath);` |
|      44 | 3910 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 3911 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 3912 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 3913 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3914 | `			return SXERR_ABORT;` |
|       - | 3915 | `		}` |
|       1 | 3916 | `	}` |
|      44 | 3917 | `	return SXRET_OK;` |
|      23 | 3918 |  |
|       - | 3919 | `/*` |
|       - | 3920 | ` * Compile the stupid 'declare' language construct.` |
|       - | 3921 | ` *` |
|       - | 3922 | ` * According to the PHP language reference manual.` |
|       - | 3923 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 3924 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 3925 | ` *  declare (directive)` |
|       - | 3926 | ` *   statement` |
|       - | 3927 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 3928 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 3929 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 3930 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 3931 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 3932 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 3933 | ` * <?php` |
|       - | 3934 | ` * // these are the same:` |
|       - | 3935 | ` * // you can use this:` |
|       - | 3936 | ` * declare(ticks=1) {` |
|       - | 3937 | ` *   // entire script here` |
|       - | 3938 | ` * }` |
|       - | 3939 | ` * // or you can use this:` |
|       - | 3940 | ` * declare(ticks=1);` |
|       - | 3941 | ` * // entire script here` |
|       - | 3942 | ` * ?>` |
|       - | 3943 | ` *` |
|       - | 3944 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 3945 | ` */` |
|       8 | 3946 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 3947 |  |
|       9 | 3948 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 3949 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 3950 | `	sxi32 rc;` |
|       9 | 3951 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 3952 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 3953 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 3954 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3955 | `			return SXERR_ABORT;` |
|       - | 3956 | `		}` |
|       5 | 3957 | `		goto Synchro;` |
|       - | 3958 | `	}` |
|       5 | 3959 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 3960 | `	/* Delimit the directive */` |
|       5 | 3961 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 3962 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 3963 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 3964 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3965 | `			return SXERR_ABORT;` |
|       - | 3966 | `		}` |
|     ! 0 | 3967 | `		return SXRET_OK;` |
|       - | 3968 | `	}` |
|       - | 3969 | `	/* Update the cursor */` |
|       5 | 3970 | `	pGen->pIn = &pEnd[1];` |
|       5 | 3971 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 3972 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 3973 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3974 | `			return SXERR_ABORT;` |
|       - | 3975 | `		}` |
|     ! 0 | 3976 | `	}` |
|       - | 3977 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 3978 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 3979 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 3980 | `		ph7_lib_version()` |
|       - | 3981 | `		);` |
|       - | 3982 | `	/*All done */` |
|       5 | 3983 | `	return SXRET_OK;` |
|       2 | 3984 | `Synchro:` |
|       - | 3985 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 3986 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 3987 | `		pGen->pIn++;` |
|       1 | 3988 | `	}` |
|       5 | 3989 | `	return SXRET_OK;` |
|       5 | 3990 |  |
|       - | 3991 | `/*` |
|       - | 3992 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 3993 | ` * as follows:` |
|       - | 3994 | ` * function makecoffee($type = "cappuccino")` |
|       - | 3995 | ` * {` |
|       - | 3996 | ` *   return "Making a cup of $type.\n";` |
|       - | 3997 | ` * }` |
|       - | 3998 | ` * Symisc eXtension.` |
|       - | 3999 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 4000 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 4001 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 4002 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 4003 | ` *      {` |
|       - | 4004 | ` *       var_dump($a);` |
|       - | 4005 | ` *      }` |
|       - | 4006 | ` *     //call test without args` |
|       - | 4007 | ` *      test();` |
|       - | 4008 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 4009 | ` *      Example:` |
|       - | 4010 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 4011 | ` * 3 -) Function overloading!!` |
|       - | 4012 | ` *      Example:` |
|       - | 4013 | ` *      function foo($a) {` |
|       - | 4014 | ` *   	  return $a.PHP_EOL;` |
|       - | 4015 | ` *	    }` |
|       - | 4016 | ` *	    function foo($a, $b) {` |
|       - | 4017 | ` *   	  return $a + $b;` |
|       - | 4018 | ` *	    }` |
|       - | 4019 | ` *	    echo foo(5); // Prints "5"` |
|       - | 4020 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 4021 | ` *      // Same arg` |
|       - | 4022 | ` *	   function foo(string $a)` |
|       - | 4023 | ` *	   {` |
|       - | 4024 | ` *	     echo "a is a string\n";` |
|       - | 4025 | ` *	     var_dump($a);` |
|       - | 4026 | ` *	   }` |
|       - | 4027 | ` *	  function foo(int $a)` |
|       - | 4028 | ` *	  {` |
|       - | 4029 | ` *	    echo "a is integer\n";` |
|       - | 4030 | ` *	    var_dump($a);` |
|       - | 4031 | ` *	  }` |
|       - | 4032 | ` *	  function foo(array $a)` |
|       - | 4033 | ` *	  {` |
|       - | 4034 | ` * 	    echo "a is an array\n";` |
|       - | 4035 | ` * 	    var_dump($a);` |
|       - | 4036 | ` *	  }` |
|       - | 4037 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 4038 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 4039 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 4040 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 4041 | ` * introduced by the PH7 engine.` |
|       - | 4042 | ` */` |
|   41226 | 4043 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 4044 |  |
|       - | 4045 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 4046 | `	SySet *pInstrContainer;` |
|       - | 4047 | `	sxi32 rc;` |
|       - | 4048 | `	/* Swap token stream */` |
|   41228 | 4049 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   41228 | 4050 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   41228 | 4051 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 4052 | `	/* Compile the expression holding the argument value */` |
|   41228 | 4053 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4054 | `	/* Emit the done instruction */` |
|   41228 | 4055 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   41228 | 4056 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   41228 | 4057 | `	RE_SWAP_DELIMITER(pGen);` |
|   41228 | 4058 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4059 | `		return SXERR_ABORT;` |
|       - | 4060 | `	}` |
|   41228 | 4061 | `	return SXRET_OK;` |
|   20615 | 4062 |  |
|       - | 4063 | `/*` |
|       - | 4064 | ` * Collect function arguments one after one.` |
|       - | 4065 | ` * According to the PHP language reference manual.` |
|       - | 4066 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 4067 | ` * list of expressions.` |
|       - | 4068 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 4069 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 4070 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 4071 | ` * for more information.` |
|       - | 4072 | ` * Example #1 Passing arrays to functions` |
|       - | 4073 | ` * <?php` |
|       - | 4074 | ` * function takes_array($input)` |
|       - | 4075 | ` * {` |
|       - | 4076 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 4077 | ` * }` |
|       - | 4078 | ` * ?>` |
|       - | 4079 | ` * Making arguments be passed by reference` |
|       - | 4080 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 4081 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 4082 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 4083 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 4084 | ` * to the argument name in the function definition:` |
|       - | 4085 | ` * Example #2 Passing function parameters by reference` |
|       - | 4086 | ` * <?php` |
|       - | 4087 | ` * function add_some_extra(&$string)` |
|       - | 4088 | ` * {` |
|       - | 4089 | ` *   $string .= 'and something extra.';` |
|       - | 4090 | ` * }` |
|       - | 4091 | ` * $str = 'This is a string, ';` |
|       - | 4092 | ` * add_some_extra($str);` |
|       - | 4093 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 4094 | ` * ?>` |
|       - | 4095 | ` *` |
|       - | 4096 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 4097 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 4098 | ` * on these extension.` |
|       - | 4099 | ` */` |
|   49502 | 4100 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 4101 |  |
|       - | 4102 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 4103 | `	SyToken *pIn;  /* Token stream */` |
|       - | 4104 | `	SyBlob sSig;         /* Function signature */` |
|       - | 4105 | `	char *zDup;          /* Copy of argument name */` |
|       - | 4106 | `	sxi32 rc;` |
|       - | 4107 |  |
|   49504 | 4108 | `	pIn = pGen->pIn;` |
|   49504 | 4109 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 4110 | `	/* Process arguments one after one */` |
|   62632 | 4111 | `	for(;;){` |
|  125266 | 4112 | `		if( pIn >= pEnd ){` |
|       - | 4113 | `			/* No more arguments to process */` |
|   49502 | 4114 | `			break;` |
|       - | 4115 | `		}` |
|   75766 | 4116 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   75766 | 4117 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 4118 | `		/* Detect nullable prefix '?' on type hints */` |
|   75766 | 4119 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      11 | 4120 | `			sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|      11 | 4121 | `			pIn++;` |
|       5 | 4122 | `		}` |
|       - | 4123 | `		/* Skip leading namespace separator '\' on FQN type hints like \Throwable */` |
|   75766 | 4124 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       5 | 4125 | `			pIn++;` |
|       2 | 4126 | `		}` |
|   75766 | 4127 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|   51552 | 4128 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   46396 | 4129 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   46396 | 4130 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 4131 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   46396 | 4132 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 4133 | `					sArg.nType = MEMOBJ_BOOL;` |
|   46396 | 4134 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   12890 | 4135 | `					sArg.nType = MEMOBJ_INT;` |
|   39952 | 4136 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   33506 | 4137 | `					sArg.nType = MEMOBJ_STRING;` |
|   16755 | 4138 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 4139 | `					sArg.nType = MEMOBJ_REAL;` |
|     ! 0 | 4140 | `				}else{` |
|       4 | 4141 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 4142 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 4143 | `						&pIn->sData);` |
|       - | 4144 | `				}` |
|   23199 | 4145 | `			}else{` |
|    5158 | 4146 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 4147 | `				char *zDupLocal;` |
|       - | 4148 | `				/* Argument must be a class instance,record that*/` |
|    5158 | 4149 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    5158 | 4150 | `				if( zDupLocal ){` |
|    5158 | 4151 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    5158 | 4152 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2578 | 4153 | `				}` |
|       - | 4154 | `			}` |
|   51552 | 4155 | `			pIn++;` |
|   25775 | 4156 | `		}` |
|   75766 | 4157 | `		if( pIn >= pEnd ){` |
|     ! 0 | 4158 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 4159 | `			return rc;` |
|       - | 4160 | `		}` |
|   75766 | 4161 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 4162 | `			/* Pass by reference,record that */` |
|    2602 | 4163 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    2602 | 4164 | `			pIn++;` |
|    1300 | 4165 | `		}` |
|   75766 | 4166 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - | 4167 | `			/* Variadic parameter: ...$args */` |
|      23 | 4168 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      23 | 4169 | `			pIn++;` |
|      11 | 4170 | `		}` |
|   75766 | 4171 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4172 | `			/* Invalid argument */` |
|     ! 0 | 4173 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 4174 | `			return rc;` |
|       - | 4175 | `		}` |
|   75766 | 4176 | `		pIn++; /* Jump the dollar sign */` |
|       - | 4177 | `		/* Copy argument name */` |
|   75766 | 4178 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   75766 | 4179 | `		if( zDup == 0 ){` |
|     ! 0 | 4180 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 4181 | `			return SXERR_ABORT;` |
|       - | 4182 | `		}` |
|   75766 | 4183 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   75766 | 4184 | `		pIn++;` |
|   75766 | 4185 | `		if( pIn < pEnd ){` |
|   46886 | 4186 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 4187 | `				SyToken *pDefend;` |
|   41230 | 4188 | `				sxi32 iNest = 0;` |
|   41230 | 4189 | `				pIn++; /* Jump the equal sign */` |
|   41230 | 4190 | `				pDefend = pIn;` |
|       - | 4191 | `				/* Process the default value associated with this argument */` |
|   87608 | 4192 | `				while( pDefend < pEnd ){` |
|   66988 | 4193 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   20610 | 4194 | `						break;` |
|       - | 4195 | `					}` |
|   46380 | 4196 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 4197 | `						/* Increment nesting level */` |
|    2578 | 4198 | `						iNest++;` |
|   45092 | 4199 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 4200 | `						/* Decrement nesting level */` |
|    2578 | 4201 | `						iNest--;` |
|    1288 | 4202 | `					}` |
|   46380 | 4203 | `					pDefend++;` |
|       2 | 4204 | `				}` |
|   41230 | 4205 | `				if( pIn >= pDefend ){` |
|       3 | 4206 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 4207 | `					return rc;` |
|       - | 4208 | `				}` |
|       - | 4209 | `				/* Process default value */` |
|   41228 | 4210 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   41228 | 4211 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 4212 | `					return rc;` |
|       - | 4213 | `				}` |
|       - | 4214 | `				/* Point beyond the default value */` |
|   41228 | 4215 | `				pIn = pDefend;` |
|   20613 | 4216 | `			}` |
|   46884 | 4217 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 4218 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 4219 | `				return rc;` |
|       - | 4220 | `			}` |
|   46884 | 4221 | `			pIn++; /* Jump the trailing comma */` |
|   23441 | 4222 | `		}` |
|       - | 4223 | `		/* Append argument signature */` |
|   75764 | 4224 | `		if( sArg.nType > 0 ){` |
|   51550 | 4225 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 4226 | `				/* Class name */` |
|    5158 | 4227 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2580 | 4228 | `			}else{` |
|       - | 4229 | `				int c;` |
|   46394 | 4230 | `				c = 'n'; /* cc warning */` |
|       - | 4231 | `				/* Type leading character */` |
|   46394 | 4232 | `				switch(sArg.nType){` |
|     ! 0 | 4233 | `				case MEMOBJ_HASHMAP:` |
|       - | 4234 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 4235 | `					c = 'h';` |
|     ! 0 | 4236 | `					break;` |
|    6444 | 4237 | `				case MEMOBJ_INT:` |
|       - | 4238 | `					/* Integer */` |
|   12890 | 4239 | `					c = 'i';` |
|   12890 | 4240 | `					break;` |
|     ! 0 | 4241 | `				case MEMOBJ_BOOL:` |
|       - | 4242 | `					/* Bool */` |
|     ! 0 | 4243 | `					c = 'b';` |
|     ! 0 | 4244 | `					break;` |
|     ! 0 | 4245 | `				case MEMOBJ_REAL:` |
|       - | 4246 | `					/* Float */` |
|     ! 0 | 4247 | `					c = 'f';` |
|     ! 0 | 4248 | `					break;` |
|   16752 | 4249 | `				case MEMOBJ_STRING:` |
|       - | 4250 | `					/* String */` |
|   33506 | 4251 | `					c = 's';` |
|   33504 | 4252 | `					break;` |
|     ! 0 | 4253 | `				default:` |
|     ! 0 | 4254 | `					break;` |
|       - | 4255 | `				}` |
|   46394 | 4256 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 4257 | `			}` |
|   25776 | 4258 | `		}else{` |
|       - | 4259 | `			/* No type is associated with this parameter which mean` |
|       - | 4260 | `			 * that this function is not condidate for overloading.` |
|       - | 4261 | `			 */` |
|   24216 | 4262 | `			SyBlobRelease(&sSig);` |
|       - | 4263 | `		}` |
|       - | 4264 | `		/* Save in the argument set */` |
|   75764 | 4265 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 4266 | `	}` |
|   49502 | 4267 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 4268 | `		/* Save function signature */` |
|   30936 | 4269 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   15467 | 4270 | `	}` |
|   49502 | 4271 | `	return SXRET_OK;` |
|   24753 | 4272 |  |
|       - | 4273 | `/*` |
|       - | 4274 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 4275 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 4276 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 4277 | ` */` |
|  137586 | 4278 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 4279 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 4280 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 4281 | `	)` |
|       2 | 4282 |  |
|       - | 4283 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 4284 | `	GenBlock *pBlock;` |
|       - | 4285 | `	sxu32 nGotoOfft;` |
|       - | 4286 | `	sxi32 rc;` |
|       - | 4287 | `	/* Attach the new function */` |
|  137588 | 4288 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  137588 | 4289 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4290 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 4291 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4292 | `		return SXERR_ABORT;` |
|       - | 4293 | `	}` |
|  137588 | 4294 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 4295 | `	/* Swap bytecode containers */` |
|  137588 | 4296 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  137588 | 4297 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 4298 | `	/* Compile the body */` |
|  137588 | 4299 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 4300 | `	/* Fix exception jumps now the destination is resolved */` |
|  137588 | 4301 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4302 | `	/* Emit the final return if not yet done */` |
|  137588 | 4303 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4304 | `	/* Fix gotos jumps now the destination is resolved */` |
|  137588 | 4305 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 4306 | `		rc = SXERR_ABORT;` |
|     ! 0 | 4307 | `	}` |
|  137588 | 4308 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 4309 | `	/* Restore the default container */` |
|  137588 | 4310 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4311 | `	/* Leave function block */` |
|  137588 | 4312 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  137588 | 4313 | `	if( rc == SXERR_ABORT ){` |
|       - | 4314 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4315 | `		return SXERR_ABORT;` |
|       - | 4316 | `	}` |
|       - | 4317 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - | 4318 | `	{` |
|  137588 | 4319 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - | 4320 | `		sxu32 i;` |
| 2856768 | 4321 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 2719198 | 4322 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      18 | 4323 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      18 | 4324 | `				break;` |
|       - | 4325 | `			}` |
| 1359592 | 4326 | `		}` |
|       - | 4327 | `	}` |
|       - | 4328 | `	/* All done, function body compiled */` |
|  137588 | 4329 | `	return SXRET_OK;` |
|   68795 | 4330 |  |
|       - | 4331 | `/*` |
|       - | 4332 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 4333 | ` * According to the PHP language reference manual.` |
|       - | 4334 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 4335 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 4336 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 4337 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4338 | ` *  Functions need not be defined before they are referenced.` |
|       - | 4339 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 4340 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 4341 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 4342 | ` *  calls with over 32-64 recursion levels.` |
|       - | 4343 | ` *` |
|       - | 4344 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 4345 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 4346 | ` * on these extension.` |
|       - | 4347 | ` */` |
|       - | 4348 | `/*` |
|       - | 4349 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - | 4350 | ` */` |
|       6 | 4351 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       1 | 4352 |  |
|       - | 4353 | `	sxu32 i;` |
|      31 | 4354 | `	for( i = 0; i < n; i++ ){` |
|      25 | 4355 | `		int a = zA[i], b = zB[i];` |
|      25 | 4356 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      25 | 4357 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      25 | 4358 | `		if( a != b ) return a - b;` |
|      13 | 4359 | `	}` |
|       7 | 4360 | `	return 0;` |
|       4 | 4361 |  |
|       - | 4362 | `/*` |
|       - | 4363 | ` * Helper: set the return type to a class/self/parent/static sentinel.` |
|       - | 4364 | ` */` |
|       2 | 4365 | `static void GenStateSetReturnClass(ph7_gen_state *pGen, ph7_vm_func *pFunc, const char *zName, sxu32 nByte)` |
|       1 | 4366 |  |
|       3 | 4367 | `	char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator, zName, nByte);` |
|       3 | 4368 | `	if( zDup ){` |
|       3 | 4369 | `		pFunc->nReturnType = SXU32_HIGH;` |
|       3 | 4370 | `		SyStringInitFromBuf(&pFunc->sReturnClass, zDup, nByte);` |
|       1 | 4371 | `	}` |
|       3 | 4372 |  |
|       - | 4373 | `/*` |
|       - | 4374 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - | 4375 | `` * pGen->pIn should point to the token after `)`.`` |
|       - | 4376 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - | 4377 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - | 4378 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, and nullable `: ?type`.`` |
|       - | 4379 | ` */` |
|  158242 | 4380 | `static void GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 | 4381 |  |
|  158244 | 4382 | `	SyToken *pCur = pGen->pIn;` |
|  158244 | 4383 | `	pFunc->nReturnType = 0;` |
|  158244 | 4384 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  158244 | 4385 | `	if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_COLON) == 0 ){` |
|  158190 | 4386 | `		return; /* No return type */` |
|       - | 4387 | `	}` |
|      55 | 4388 | `	pCur++; /* Skip ':' */` |
|      55 | 4389 | `	if( pCur >= pGen->pEnd ){` |
|     ! 0 | 4390 | `		pGen->pIn = pCur;` |
|     ! 0 | 4391 | `		return;` |
|       - | 4392 | `	}` |
|       - | 4393 | `	/* Handle nullable prefix '?' (tokenized as PH7_TK_OP with '?' operator) */` |
|      55 | 4394 | `	if( (pCur->nType & PH7_TK_OP) && pCur->sData.nByte == 1 && pCur->sData.zString[0] == '?' ){` |
|       7 | 4395 | `		pCur++;` |
|       7 | 4396 | `		if( pCur >= pGen->pEnd ){` |
|     ! 0 | 4397 | `			pGen->pIn = pCur;` |
|     ! 0 | 4398 | `			return;` |
|       - | 4399 | `		}` |
|       3 | 4400 | `	}` |
|      55 | 4401 | `	if( pCur->nType & PH7_TK_KEYWORD ){` |
|      49 | 4402 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pCur->pUserData));` |
|      49 | 4403 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|       3 | 4404 | `			pFunc->nReturnType = MEMOBJ_HASHMAP;` |
|      48 | 4405 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       3 | 4406 | `			pFunc->nReturnType = MEMOBJ_BOOL;` |
|      46 | 4407 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|      17 | 4408 | `			pFunc->nReturnType = MEMOBJ_INT;` |
|      37 | 4409 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|      25 | 4410 | `			pFunc->nReturnType = MEMOBJ_STRING;` |
|      17 | 4411 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       3 | 4412 | `			pFunc->nReturnType = MEMOBJ_REAL;` |
|       4 | 4413 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT \|\| nKey == PH7_TKWRD_STATIC ){` |
|       - | 4414 | `			/* self/parent/static — store as class sentinel */` |
|       3 | 4415 | `			GenStateSetReturnClass(pGen, pFunc, pCur->sData.zString, pCur->sData.nByte);` |
|       1 | 4416 | `		}` |
|      49 | 4417 | `		pCur++;` |
|      31 | 4418 | `	}else if( pCur->nType & PH7_TK_ID ){` |
|       7 | 4419 | `		SyString *pType = &pCur->sData;` |
|       7 | 4420 | `		if( pType->nByte == 4 && SyMemcmpNoCase(pType->zString, "void", 4) == 0 ){` |
|       7 | 4421 | `			pFunc->nReturnType = MEMOBJ_VOID;` |
|       4 | 4422 | `		}else{` |
|       - | 4423 | `			/* Class/interface name */` |
|     ! 0 | 4424 | `			GenStateSetReturnClass(pGen, pFunc, pType->zString, pType->nByte);` |
|       - | 4425 | `		}` |
|       7 | 4426 | `		pCur++;` |
|       3 | 4427 | `	}` |
|      55 | 4428 | `	pGen->pIn = pCur;` |
|   79123 | 4429 |  |
|       - | 4430 |  |
|   34146 | 4431 | `static sxi32 GenStateCompileFunc(` |
|       - | 4432 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4433 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 4434 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 4435 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 4436 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 4437 | `	)` |
|       2 | 4438 |  |
|       - | 4439 | `	ph7_vm_func *pFunc;` |
|       - | 4440 | `	SyToken *pEnd;` |
|       - | 4441 | `	sxu32 nLine;` |
|       - | 4442 | `	char *zName;` |
|       - | 4443 | `	sxi32 rc;` |
|       - | 4444 | `	/* Extract line number */` |
|   34148 | 4445 | `	nLine = pGen->pIn->nLine;` |
|       - | 4446 | `	/* Jump the left parenthesis '(' */` |
|   34148 | 4447 | `	pGen->pIn++;` |
|       - | 4448 | `	/* Delimit the function signature */` |
|   34148 | 4449 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   34148 | 4450 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4451 | `		/* Syntax error */` |
|       7 | 4452 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 4453 | `		if( rc == SXERR_ABORT ){` |
|       - | 4454 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4455 | `			return SXERR_ABORT;` |
|       - | 4456 | `		}` |
|       7 | 4457 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 4458 | `		return SXRET_OK;` |
|       - | 4459 | `	}` |
|       - | 4460 | `	/* Create the function state */` |
|   34142 | 4461 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   34142 | 4462 | `	if( pFunc == 0 ){` |
|     ! 0 | 4463 | `		goto OutOfMem;` |
|       - | 4464 | `	}` |
|       - | 4465 | `	/* Build the function name, prepending namespace if active */` |
|   34146 | 4466 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 4467 | `		SyBlob sFQN;` |
|       - | 4468 | `		sxu32 nLen;` |
|      10 | 4469 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      10 | 4470 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      10 | 4471 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      10 | 4472 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      10 | 4473 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      10 | 4474 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      10 | 4475 | `		SyBlobRelease(&sFQN);` |
|      10 | 4476 | `		if( zName == 0 ){` |
|     ! 0 | 4477 | `			goto OutOfMem;` |
|       - | 4478 | `		}` |
|      10 | 4479 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       6 | 4480 | `	}else{` |
|   34134 | 4481 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   34134 | 4482 | `		if( zName == 0 ){` |
|     ! 0 | 4483 | `			goto OutOfMem;` |
|       - | 4484 | `		}` |
|   34134 | 4485 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 4486 | `	}` |
|   34142 | 4487 | `	if( pGen->pIn < pEnd ){` |
|       - | 4488 | `		/* Collect function arguments */` |
|   23664 | 4489 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   23664 | 4490 | `		if( rc == SXERR_ABORT ){` |
|       - | 4491 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4492 | `			return SXERR_ABORT;` |
|       - | 4493 | `		}` |
|   11831 | 4494 | `	}` |
|       - | 4495 | `	/* Point past ')' and parse optional return type ': type' */` |
|   34142 | 4496 | `	pGen->pIn = &pEnd[1];` |
|   34142 | 4497 | `	GenStateParseReturnType(pGen, pFunc);` |
|   34142 | 4498 | `	if( bHandleClosure ){` |
|       - | 4499 | `		ph7_vm_func_closure_env sEnv;` |
|     168 | 4500 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     166 | 4501 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      91 | 4502 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      14 | 4503 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 4504 | `				/* Closure,record environment variable */` |
|      14 | 4505 | `				pGen->pIn++;` |
|      14 | 4506 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 4507 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 4508 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4509 | `						return SXERR_ABORT;` |
|       - | 4510 | `					}` |
|     ! 0 | 4511 | `				}` |
|      14 | 4512 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 4513 | `				/* Compile until we hit the first closing parenthesis */` |
|      28 | 4514 | `				while( pGen->pIn < pGen->pEnd ){` |
|      28 | 4515 | `					int iFlagsLocal = 0;` |
|      28 | 4516 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      14 | 4517 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      14 | 4518 | `						break;` |
|       - | 4519 | `					}` |
|      16 | 4520 | `					nLineLocal = pGen->pIn->nLine;` |
|      16 | 4521 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 4522 | `						/* Pass by reference,record that */` |
|     ! 0 | 4523 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 4524 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 4525 | `							);` |
|     ! 0 | 4526 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 4527 | `						pGen->pIn++;` |
|     ! 0 | 4528 | `					}` |
|      14 | 4529 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      16 | 4530 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 4531 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 4532 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 4533 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4534 | `								return SXERR_ABORT;` |
|       - | 4535 | `							}` |
|       - | 4536 | `							/* Find the closing parenthesis */` |
|     ! 0 | 4537 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 4538 | `								pGen->pIn++;` |
|     ! 0 | 4539 | `							}` |
|     ! 0 | 4540 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 4541 | `								pGen->pIn++;` |
|     ! 0 | 4542 | `							}` |
|     ! 0 | 4543 | `							break;` |
|       - | 4544 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 4545 | `					}else{` |
|       - | 4546 | `						SyString *pNameLocal;` |
|       - | 4547 | `						char *zDup;` |
|       - | 4548 | `						/* Duplicate variable name */` |
|      16 | 4549 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      16 | 4550 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      16 | 4551 | `						if( zDup ){` |
|       - | 4552 | `							/* Zero the structure */` |
|      16 | 4553 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 | 4554 | `							sEnv.iFlags = iFlagsLocal;` |
|      16 | 4555 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 | 4556 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      16 | 4557 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 4558 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 4559 | `									got_this = 1;` |
|     ! 0 | 4560 | `							}` |
|       - | 4561 | `							/* Save imported variable */` |
|      16 | 4562 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       9 | 4563 | `						}else{` |
|     ! 0 | 4564 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4565 | `							 return SXERR_ABORT;` |
|       - | 4566 | `						}` |
|       - | 4567 | `					}` |
|      16 | 4568 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      18 | 4569 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4570 | `						/* Ignore trailing commas */` |
|       3 | 4571 | `						pGen->pIn++;` |
|       1 | 4572 | `					}` |
|       2 | 4573 | `				}` |
|      14 | 4574 | `				if( !got_this ){` |
|       - | 4575 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 4576 | `					 * available to the closure environment.` |
|       - | 4577 | `					 */` |
|      14 | 4578 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      14 | 4579 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      14 | 4580 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      14 | 4581 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      14 | 4582 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       6 | 4583 | `				}` |
|      14 | 4584 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 4585 | `					/* Mark as closure */` |
|      14 | 4586 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       6 | 4587 | `				}` |
|       6 | 4588 | `		}` |
|      83 | 4589 | `	}` |
|       - | 4590 | `	/* Compile the body */` |
|   34142 | 4591 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   34142 | 4592 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4593 | `		return SXERR_ABORT;` |
|       - | 4594 | `	}` |
|   34142 | 4595 | `	if( ppFunc ){` |
|     168 | 4596 | `		*ppFunc = pFunc;` |
|      83 | 4597 | `	}` |
|   34142 | 4598 | `	rc = SXRET_OK;` |
|   34142 | 4599 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 4600 | `		/* Finally register the function */` |
|   34130 | 4601 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   17064 | 4602 | `	}` |
|   34142 | 4603 | `	if( rc == SXRET_OK ){` |
|   34142 | 4604 | `		return SXRET_OK;` |
|       - | 4605 | `	}` |
|       - | 4606 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 4607 | `OutOfMem:` |
|       - | 4608 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 4609 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 4610 | `	 */` |
|     ! 0 | 4611 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 4612 | `	return SXERR_ABORT;` |
|   17075 | 4613 |  |
|       - | 4614 | `/*` |
|       - | 4615 | ` * Compile a standard PHP function.` |
|       - | 4616 | ` *  Refer to the block-comment above for more information.` |
|       - | 4617 | ` */` |
|   33986 | 4618 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 4619 |  |
|       - | 4620 | `	SyString *pName;` |
|       - | 4621 | `	sxi32 iFlags;` |
|       - | 4622 | `	sxu32 nLine;` |
|       - | 4623 | `	sxi32 rc;` |
|       - | 4624 |  |
|   33988 | 4625 | `	nLine = pGen->pIn->nLine;` |
|   33988 | 4626 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   33988 | 4627 | `	iFlags = 0;` |
|   33988 | 4628 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4629 | `		/* Return by reference,remember that */` |
|       7 | 4630 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4631 | `		/* Jump the '&' token */` |
|       7 | 4632 | `		pGen->pIn++;` |
|       3 | 4633 | `	}` |
|   33988 | 4634 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4635 | `		/* Invalid function name */` |
|       5 | 4636 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 4637 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4638 | `			return SXERR_ABORT;` |
|       - | 4639 | `		}` |
|       - | 4640 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 4641 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 4642 | `			pGen->pIn++;` |
|       1 | 4643 | `		}` |
|       5 | 4644 | `		return SXRET_OK;` |
|       - | 4645 | `	}` |
|   33984 | 4646 | `	pName = &pGen->pIn->sData;` |
|   33984 | 4647 | `	nLine = pGen->pIn->nLine;` |
|       - | 4648 | `	/* Jump the function name */` |
|   33984 | 4649 | `	pGen->pIn++;` |
|   33984 | 4650 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4651 | `		/* Syntax error */` |
|       3 | 4652 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 4653 | `		if( rc == SXERR_ABORT ){` |
|       - | 4654 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4655 | `			return SXERR_ABORT;` |
|       - | 4656 | `		}` |
|       - | 4657 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 4658 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4659 | `			pGen->pIn++;` |
|     ! 0 | 4660 | `		}` |
|       3 | 4661 | `		return SXRET_OK;` |
|       - | 4662 | `	}` |
|       - | 4663 | `	/* Compile function body */` |
|   33982 | 4664 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   33982 | 4665 | `	return rc;` |
|   16995 | 4666 |  |
|       - | 4667 | `/*` |
|       - | 4668 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 4669 | ` * According to the PHP language reference manual` |
|       - | 4670 | ` *  Visibility:` |
|       - | 4671 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 4672 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 4673 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 4674 | ` *  Members declared protected can be accessed only within the class` |
|       - | 4675 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 4676 | ` *  may only be accessed by the class that defines the member.` |
|       - | 4677 | ` */` |
|  157814 | 4678 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 4679 |  |
|  157816 | 4680 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    7790 | 4681 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  150028 | 4682 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   18062 | 4683 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 4684 | `	}` |
|       - | 4685 | `	/* Assume public by default */` |
|  131968 | 4686 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   78909 | 4687 |  |
|       - | 4688 | `/*` |
|       - | 4689 | ` * Compile a class constant.` |
|       - | 4690 | ` * According to the PHP language reference manual` |
|       - | 4691 | ` *  Class Constants` |
|       - | 4692 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 4693 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 4694 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 4695 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 4696 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 4697 | ` *   It's also possible for interfaces to have constants.` |
|       - | 4698 | ` * Symisc eXtension.` |
|       - | 4699 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 4700 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4701 | ` *  Example:` |
|       - | 4702 | ` *   class Test{` |
|       - | 4703 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4704 | ` *   };` |
|       - | 4705 | ` *   var_dump(TEST::MyConst);` |
|       - | 4706 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4707 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4708 | ` */` |
|      10 | 4709 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4710 |  |
|      12 | 4711 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4712 | `	SySet *pInstrContainer;` |
|       - | 4713 | `	ph7_class_attr *pCons;` |
|       - | 4714 | `	SyString *pName;` |
|       - | 4715 | `	sxi32 rc;` |
|       - | 4716 | `	/* Extract visibility level */` |
|      12 | 4717 | `	iProtection = GetProtectionLevel(iProtection);` |
|      12 | 4718 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       5 | 4719 | `loop:` |
|       - | 4720 | `	/* Mark as constant */` |
|      12 | 4721 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      12 | 4722 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4723 | `		/* Invalid constant name */` |
|     ! 0 | 4724 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 4725 | `		if( rc == SXERR_ABORT ){` |
|       - | 4726 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4727 | `			return SXERR_ABORT;` |
|       - | 4728 | `		}` |
|     ! 0 | 4729 | `		goto Synchronize;` |
|       - | 4730 | `	}` |
|       - | 4731 | `	/* Peek constant name */` |
|      12 | 4732 | `	pName = &pGen->pIn->sData;` |
|       - | 4733 | `	/* Make sure the constant name isn't reserved */` |
|      12 | 4734 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 4735 | `		/* Reserved constant name */` |
|     ! 0 | 4736 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 4737 | `		if( rc == SXERR_ABORT ){` |
|       - | 4738 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4739 | `			return SXERR_ABORT;` |
|       - | 4740 | `		}` |
|     ! 0 | 4741 | `		goto Synchronize;` |
|       - | 4742 | `	}` |
|       - | 4743 | `	/* Advance the stream cursor */` |
|      12 | 4744 | `	pGen->pIn++;` |
|      12 | 4745 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 4746 | `		/* Invalid declaration */` |
|     ! 0 | 4747 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 4748 | `		if( rc == SXERR_ABORT ){` |
|       - | 4749 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4750 | `			return SXERR_ABORT;` |
|       - | 4751 | `		}` |
|     ! 0 | 4752 | `		goto Synchronize;` |
|       - | 4753 | `	}` |
|      12 | 4754 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 4755 | `	/* Allocate a new class attribute */` |
|      12 | 4756 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      12 | 4757 | `	if( pCons == 0 ){` |
|     ! 0 | 4758 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4759 | `		return SXERR_ABORT;` |
|       - | 4760 | `	}` |
|       - | 4761 | `	/* Swap bytecode container */` |
|      12 | 4762 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      12 | 4763 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 4764 | `	/* Compile constant value.` |
|       - | 4765 | `	 */` |
|      12 | 4766 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      12 | 4767 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 4768 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 4769 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4770 | `			return SXERR_ABORT;` |
|       - | 4771 | `		}` |
|       1 | 4772 | `	}` |
|       - | 4773 | `	/* Emit the done instruction */` |
|      12 | 4774 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      12 | 4775 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      12 | 4776 | `	if( rc == SXERR_ABORT ){` |
|       - | 4777 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4778 | `		return SXERR_ABORT;` |
|       - | 4779 | `	}` |
|       - | 4780 | `	/* All done,install the constant */` |
|      12 | 4781 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      12 | 4782 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4783 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4784 | `		return SXERR_ABORT;` |
|       - | 4785 | `	}` |
|      12 | 4786 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4787 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 4788 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4789 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 4790 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4791 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4792 | `				pTok--;` |
|     ! 0 | 4793 | `			}` |
|     ! 0 | 4794 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4795 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 4796 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4797 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4798 | `				return SXERR_ABORT;` |
|       - | 4799 | `			}` |
|     ! 0 | 4800 | `		}else{` |
|     ! 0 | 4801 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 4802 | `				goto loop;` |
|       - | 4803 | `			}` |
|       - | 4804 | `		}` |
|     ! 0 | 4805 | `	}` |
|      12 | 4806 | `	return SXRET_OK;` |
|     ! 0 | 4807 | `Synchronize:` |
|       - | 4808 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4809 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 4810 | `		pGen->pIn++;` |
|     ! 0 | 4811 | `	}` |
|     ! 0 | 4812 | `	return SXERR_CORRUPT;` |
|       7 | 4813 |  |
|       - | 4814 | `/*` |
|       - | 4815 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 4816 | ` * According to the PHP language reference manual` |
|       - | 4817 | ` *  Properties` |
|       - | 4818 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 4819 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 4820 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 4821 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 4822 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 4823 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 4824 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 4825 | ` * Symisc eXtension.` |
|       - | 4826 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 4827 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4828 | ` *  Example:` |
|       - | 4829 | ` *   class Test{` |
|       - | 4830 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4831 | ` *   };` |
|       - | 4832 | ` *   var_dump(TEST::myVar);` |
|       - | 4833 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4834 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4835 | ` */` |
|   33700 | 4836 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4837 |  |
|   33702 | 4838 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4839 | `	ph7_class_attr *pAttr;` |
|       - | 4840 | `	SyString *pName;` |
|       - | 4841 | `	sxi32 rc;` |
|       - | 4842 | `	/* Extract visibility level */` |
|   33702 | 4843 | `	iProtection = GetProtectionLevel(iProtection);` |
|   16850 | 4844 | `loop:` |
|   33702 | 4845 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   33702 | 4846 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 4847 | `		/* Invalid attribute name */` |
|     ! 0 | 4848 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 4849 | `		if( rc == SXERR_ABORT ){` |
|       - | 4850 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4851 | `			return SXERR_ABORT;` |
|       - | 4852 | `		}` |
|     ! 0 | 4853 | `		goto Synchronize;` |
|       - | 4854 | `	}` |
|       - | 4855 | `	/* Peek attribute name */` |
|   33702 | 4856 | `	pName = &pGen->pIn->sData;` |
|       - | 4857 | `	/* Advance the stream cursor */` |
|   33702 | 4858 | `	pGen->pIn++;` |
|   33702 | 4859 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 4860 | `		/* Invalid declaration */` |
|       3 | 4861 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 4862 | `		if( rc == SXERR_ABORT ){` |
|       - | 4863 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4864 | `			return SXERR_ABORT;` |
|       - | 4865 | `		}` |
|       3 | 4866 | `		goto Synchronize;` |
|       - | 4867 | `	}` |
|       - | 4868 | `	/* Allocate a new class attribute */` |
|   33700 | 4869 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   33700 | 4870 | `	if( pAttr == 0 ){` |
|     ! 0 | 4871 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 4872 | `		return SXERR_ABORT;` |
|       - | 4873 | `	}` |
|   33700 | 4874 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 4875 | `		SySet *pInstrContainer;` |
|   10466 | 4876 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 4877 | `		/* Swap bytecode container */` |
|   10466 | 4878 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   10466 | 4879 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 4880 | `		/* Compile attribute value.` |
|       - | 4881 | `		 */` |
|   10466 | 4882 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   10466 | 4883 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 4884 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 4885 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4886 | `				return SXERR_ABORT;` |
|       - | 4887 | `			}` |
|     ! 0 | 4888 | `		}` |
|       - | 4889 | `		/* Emit the done instruction */` |
|   10466 | 4890 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   10466 | 4891 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5232 | 4892 | `	}` |
|       - | 4893 | `	/* All done,install the attribute */` |
|   33700 | 4894 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   33700 | 4895 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4896 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4897 | `		return SXERR_ABORT;` |
|       - | 4898 | `	}` |
|   33700 | 4899 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4900 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 4901 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4902 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 4903 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4904 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4905 | `				pTok--;` |
|     ! 0 | 4906 | `			}` |
|     ! 0 | 4907 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4908 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4909 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4910 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4911 | `				return SXERR_ABORT;` |
|       - | 4912 | `			}` |
|     ! 0 | 4913 | `		}else{` |
|     ! 0 | 4914 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 4915 | `				goto loop;` |
|       - | 4916 | `			}` |
|       - | 4917 | `		}` |
|     ! 0 | 4918 | `	}` |
|   33700 | 4919 | `	return SXRET_OK;` |
|       1 | 4920 | `Synchronize:` |
|       - | 4921 | `	/* Synchronize with the first semi-colon */` |
|       5 | 4922 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 4923 | `		pGen->pIn++;` |
|       1 | 4924 | `	}` |
|       3 | 4925 | `	return SXERR_CORRUPT;` |
|   16852 | 4926 |  |
|       - | 4927 | `/*` |
|       - | 4928 | ` * Compile a class method.` |
|       - | 4929 | ` *` |
|       - | 4930 | ` * Refer to the official documentation for more information` |
|       - | 4931 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 4932 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 4933 | ` * overloading and many more.` |
|       - | 4934 | ` */` |
|  124104 | 4935 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 4936 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4937 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 4938 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 4939 | `	int doBody,          /* TRUE to process method body */` |
|       - | 4940 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 4941 | `	)` |
|       2 | 4942 |  |
|  124106 | 4943 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4944 | `	ph7_class_method *pMeth;` |
|       - | 4945 | `	sxi32 iFuncFlags;` |
|       - | 4946 | `	SyString *pName;` |
|       - | 4947 | `	SyToken *pEnd;` |
|       - | 4948 | `	sxi32 rc;` |
|       - | 4949 | `	/* Extract visibility level */` |
|  124106 | 4950 | `	iProtection = GetProtectionLevel(iProtection);` |
|  124106 | 4951 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  124106 | 4952 | `	iFuncFlags = 0;` |
|  124106 | 4953 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4954 | `		/* Invalid method name */` |
|     ! 0 | 4955 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4956 | `		if( rc == SXERR_ABORT ){` |
|       - | 4957 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4958 | `			return SXERR_ABORT;` |
|       - | 4959 | `		}` |
|     ! 0 | 4960 | `		goto Synchronize;` |
|       - | 4961 | `	}` |
|  124106 | 4962 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4963 | `		/* Return by reference,remember that */` |
|     ! 0 | 4964 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4965 | `		/* Jump the '&' token */` |
|     ! 0 | 4966 | `		pGen->pIn++;` |
|     ! 0 | 4967 | `	}` |
|  124106 | 4968 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4969 | `		/* Invalid method name */` |
|     ! 0 | 4970 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4971 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4972 | `			return SXERR_ABORT;` |
|       - | 4973 | `		}` |
|     ! 0 | 4974 | `		goto Synchronize;` |
|       - | 4975 | `	}` |
|       - | 4976 | `	/* Peek method name */` |
|  124106 | 4977 | `	pName = &pGen->pIn->sData;` |
|  124106 | 4978 | `	nLine = pGen->pIn->nLine;` |
|       - | 4979 | `	/* Jump the method name */` |
|  124106 | 4980 | `	pGen->pIn++;` |
|  124106 | 4981 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 4982 | `		/* Abstract method */` |
|   20658 | 4983 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 4984 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4985 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 4986 | `				&pClass->sName,pName);` |
|     ! 0 | 4987 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4988 | `				return SXERR_ABORT;` |
|       - | 4989 | `			}` |
|     ! 0 | 4990 | `		}` |
|       - | 4991 | `		/* Assemble method signature only */` |
|   20658 | 4992 | `		doBody = FALSE;` |
|   10328 | 4993 | `	}` |
|  124106 | 4994 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4995 | `		/* Syntax error */` |
|     ! 0 | 4996 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 4997 | `		if( rc == SXERR_ABORT ){` |
|       - | 4998 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4999 | `			return SXERR_ABORT;` |
|       - | 5000 | `		}` |
|     ! 0 | 5001 | `		goto Synchronize;` |
|       - | 5002 | `	}` |
|       - | 5003 | `	/* Allocate a new class_method instance */` |
|  124106 | 5004 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  124106 | 5005 | `	if( pMeth == 0 ){` |
|     ! 0 | 5006 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5007 | `		return SXERR_ABORT;` |
|       - | 5008 | `	}` |
|       - | 5009 | `	/* Jump the left parenthesis '(' */` |
|  124106 | 5010 | `	pGen->pIn++;` |
|  124106 | 5011 | `	pEnd = 0; /* cc warning */` |
|       - | 5012 | `	/* Delimit the method signature */` |
|  124106 | 5013 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  124106 | 5014 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5015 | `		/* Syntax error */` |
|       3 | 5016 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 5017 | `		if( rc == SXERR_ABORT ){` |
|       - | 5018 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5019 | `			return SXERR_ABORT;` |
|       - | 5020 | `		}` |
|       3 | 5021 | `		goto Synchronize;` |
|       - | 5022 | `	}` |
|  124104 | 5023 | `	if( pGen->pIn < pEnd ){` |
|       - | 5024 | `		/* Collect method arguments */` |
|   25842 | 5025 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   25842 | 5026 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5027 | `			return SXERR_ABORT;` |
|       - | 5028 | `		}` |
|   12920 | 5029 | `	}` |
|       - | 5030 | `	/* Point past ')' and parse optional return type ': type' */` |
|  124104 | 5031 | `	pGen->pIn = &pEnd[1];` |
|  124104 | 5032 | `	GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  124104 | 5033 | `	if( doBody ){` |
|       - | 5034 | `		/* Compile method body */` |
|  103448 | 5035 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  103448 | 5036 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5037 | `			return SXERR_ABORT;` |
|       - | 5038 | `		}` |
|   51725 | 5039 | `	}else{` |
|       - | 5040 | `		/* Only method signature is allowed */` |
|   20658 | 5041 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 5042 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5043 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 5044 | `				if( rc == SXERR_ABORT ){` |
|       - | 5045 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5046 | `					return SXERR_ABORT;` |
|       - | 5047 | `				}` |
|     ! 0 | 5048 | `				return SXERR_CORRUPT;` |
|       - | 5049 | `			}` |
|       - | 5050 | `	}` |
|       - | 5051 | `	/* All done,install the method */` |
|  124104 | 5052 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  124104 | 5053 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5054 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5055 | `		return SXERR_ABORT;` |
|       - | 5056 | `	}` |
|  124104 | 5057 | `	return SXRET_OK;` |
|       1 | 5058 | `Synchronize:` |
|       - | 5059 | `	/* Synchronize with the first semi-colon */` |
|       7 | 5060 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 5061 | `		pGen->pIn++;` |
|       1 | 5062 | `	}` |
|       3 | 5063 | `	return SXERR_CORRUPT;` |
|   62054 | 5064 |  |
|       - | 5065 | `/*` |
|       - | 5066 | ` * Compile an object interface.` |
|       - | 5067 | ` *  According to the PHP language reference manual` |
|       - | 5068 | ` *   Object Interfaces:` |
|       - | 5069 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 5070 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 5071 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 5072 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 5073 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 5074 | ` */` |
|    7764 | 5075 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 5076 |  |
|    7766 | 5077 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5078 | `	ph7_class *pClass,*pBase;` |
|       - | 5079 | `	SyToken *pEnd,*pTmp;` |
|       - | 5080 | `	SyString *pName;` |
|       - | 5081 | `	sxi32 nKwrd;` |
|       - | 5082 | `	sxi32 rc;` |
|       - | 5083 | `	/* Jump the 'interface' keyword */` |
|    7766 | 5084 | `	pGen->pIn++;` |
|       - | 5085 | `	/* Extract interface name */` |
|    7766 | 5086 | `	pName = &pGen->pIn->sData;` |
|       - | 5087 | `	/* Advance the stream cursor */` |
|    7766 | 5088 | `	pGen->pIn++;` |
|       - | 5089 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5090 | `		SyBlob sFQN;` |
|       - | 5091 | `		SyString sFQNStr;` |
|    7766 | 5092 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    7766 | 5093 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    7766 | 5094 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    7766 | 5095 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    7766 | 5096 | `		SyBlobRelease(&sFQN);` |
|       - | 5097 | `	}` |
|    7766 | 5098 | `	if( pClass == 0 ){` |
|     ! 0 | 5099 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5100 | `		return SXERR_ABORT;` |
|       - | 5101 | `	}` |
|       - | 5102 | `	/* Mark as an interface */` |
|    7766 | 5103 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 5104 | `	/* Assume no base class is given */` |
|    7766 | 5105 | `	pBase = 0;` |
|    7766 | 5106 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 5107 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 5108 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 5109 | `			SyString *pBaseName;` |
|       - | 5110 | `			/* Extract base interface */` |
|       3 | 5111 | `			pGen->pIn++;` |
|       3 | 5112 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5113 | `				/* Syntax error */` |
|     ! 0 | 5114 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5115 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 5116 | `					pName);` |
|     ! 0 | 5117 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5118 | `				if( rc == SXERR_ABORT ){` |
|       - | 5119 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5120 | `					return SXERR_ABORT;` |
|       - | 5121 | `				}` |
|     ! 0 | 5122 | `				return SXRET_OK;` |
|       - | 5123 | `			}` |
|       3 | 5124 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5125 | `			{` |
|       - | 5126 | `				SyBlob sResolved;` |
|       3 | 5127 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 5128 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 | 5129 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 5130 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 5131 | `				SyBlobRelease(&sResolved);` |
|       - | 5132 | `			}` |
|       - | 5133 | `			/* Only interfaces is allowed */` |
|       3 | 5134 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5135 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5136 | `			}` |
|       3 | 5137 | `			if( pBase == 0 ){` |
|       - | 5138 | `				/* Inexistant interface */` |
|     ! 0 | 5139 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 5140 | `				if( rc == SXERR_ABORT ){` |
|       - | 5141 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5142 | `					return SXERR_ABORT;` |
|       - | 5143 | `				}` |
|     ! 0 | 5144 | `			}` |
|       - | 5145 | `			/* Advance the stream cursor */` |
|       3 | 5146 | `			pGen->pIn++;` |
|       1 | 5147 | `		}` |
|       1 | 5148 | `	}` |
|    7766 | 5149 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5150 | `		/* Syntax error */` |
|     ! 0 | 5151 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 5152 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5153 | `		if( rc == SXERR_ABORT ){` |
|       - | 5154 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5155 | `			return SXERR_ABORT;` |
|       - | 5156 | `		}` |
|     ! 0 | 5157 | `		return SXRET_OK;` |
|       - | 5158 | `	}` |
|    7766 | 5159 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    7766 | 5160 | `	pEnd = 0; /* cc warning */` |
|       - | 5161 | `	/* Delimit the interface body */` |
|    7766 | 5162 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    7766 | 5163 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5164 | `		/* Syntax error */` |
|     ! 0 | 5165 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 5166 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5167 | `		if( rc == SXERR_ABORT ){` |
|       - | 5168 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5169 | `			return SXERR_ABORT;` |
|       - | 5170 | `		}` |
|     ! 0 | 5171 | `		return SXRET_OK;` |
|       - | 5172 | `	}` |
|       - | 5173 | `	/* Swap token stream */` |
|    7766 | 5174 | `	pTmp = pGen->pEnd;` |
|    7766 | 5175 | `	pGen->pEnd = pEnd;` |
|       - | 5176 | `	/* Start the parse process` |
|       - | 5177 | `	 * Note (According to the PHP reference manual):` |
|       - | 5178 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 5179 | `	 *  Only 'public' visibility is allowed.` |
|       - | 5180 | `	 */` |
|   14205 | 5181 | `	for(;;){` |
|       - | 5182 | `		/* Jump leading/trailing semi-colons */` |
|   49058 | 5183 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   20648 | 5184 | `			pGen->pIn++;` |
|       2 | 5185 | `		}` |
|   28412 | 5186 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5187 | `			/* End of interface body */` |
|    7766 | 5188 | `			break;` |
|       - | 5189 | `		}` |
|   20648 | 5190 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5191 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5192 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 5193 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5194 | `			if( rc == SXERR_ABORT ){` |
|       - | 5195 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5196 | `				return SXERR_ABORT;` |
|       - | 5197 | `			}` |
|     ! 0 | 5198 | `			goto done;` |
|       - | 5199 | `		}` |
|       - | 5200 | `		/* Extract the current keyword */` |
|   20648 | 5201 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   20648 | 5202 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 5203 | `			/* Emit a warning and switch to public visibility */` |
|     ! 0 | 5204 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"interface: Access type must be public");` |
|     ! 0 | 5205 | `			nKwrd = PH7_TKWRD_PUBLIC;` |
|     ! 0 | 5206 | `		}` |
|   20648 | 5207 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5208 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5209 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5210 | `			if( rc == SXERR_ABORT ){` |
|       - | 5211 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5212 | `				return SXERR_ABORT;` |
|       - | 5213 | `			}` |
|     ! 0 | 5214 | `			goto done;` |
|       - | 5215 | `		}` |
|   20648 | 5216 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 5217 | `			/* Advance the stream cursor */` |
|   20644 | 5218 | `			pGen->pIn++;` |
|   20644 | 5219 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5220 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5221 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5222 | `				if( rc == SXERR_ABORT ){` |
|       - | 5223 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5224 | `					return SXERR_ABORT;` |
|       - | 5225 | `				}` |
|     ! 0 | 5226 | `				goto done;` |
|       - | 5227 | `			}` |
|   20644 | 5228 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   20644 | 5229 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5230 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5231 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5232 | `				if( rc == SXERR_ABORT ){` |
|       - | 5233 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5234 | `					return SXERR_ABORT;` |
|       - | 5235 | `				}` |
|     ! 0 | 5236 | `				goto done;` |
|       - | 5237 | `			}` |
|   10321 | 5238 | `		}` |
|   20648 | 5239 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5240 | `			/* Parse constant */` |
|       3 | 5241 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 5242 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5243 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5244 | `					return SXERR_ABORT;` |
|       - | 5245 | `				}` |
|     ! 0 | 5246 | `				goto done;` |
|       - | 5247 | `			}` |
|       2 | 5248 | `		}else{` |
|   20646 | 5249 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   20646 | 5250 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5251 | `				/* Static method,record that */` |
|     ! 0 | 5252 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 5253 | `				/* Advance the stream cursor */` |
|     ! 0 | 5254 | `				pGen->pIn++;` |
|     ! 0 | 5255 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 5256 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5257 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5258 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5259 | `						if( rc == SXERR_ABORT ){` |
|       - | 5260 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5261 | `							return SXERR_ABORT;` |
|       - | 5262 | `						}` |
|     ! 0 | 5263 | `						goto done;` |
|       - | 5264 | `				}` |
|     ! 0 | 5265 | `			}` |
|       - | 5266 | `			/* Process method signature (no body for interface methods) */` |
|   20646 | 5267 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   20646 | 5268 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5269 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5270 | `					return SXERR_ABORT;` |
|       - | 5271 | `				}` |
|     ! 0 | 5272 | `				goto done;` |
|       - | 5273 | `			}` |
|       - | 5274 | `		}` |
|       2 | 5275 | `	}` |
|       - | 5276 | `	/* Install the interface */` |
|    7766 | 5277 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    7766 | 5278 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 5279 | `		/* Inherit from the base interface */` |
|       3 | 5280 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 5281 | `	}` |
|    7766 | 5282 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5283 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5284 | `		return SXERR_ABORT;` |
|       - | 5285 | `	}` |
|    3882 | 5286 | `done:` |
|       - | 5287 | `	/* Point beyond the interface body */` |
|    7766 | 5288 | `	pGen->pIn  = &pEnd[1];` |
|    7766 | 5289 | `	pGen->pEnd = pTmp;` |
|    7766 | 5290 | `	return PH7_OK;` |
|    3884 | 5291 |  |
|       - | 5292 | `/*` |
|       - | 5293 | ` * Compile a user-defined class.` |
|       - | 5294 | ` * According to the PHP language reference manual` |
|       - | 5295 | ` *  class` |
|       - | 5296 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 5297 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 5298 | ` *  of the properties and methods belonging to the class.` |
|       - | 5299 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 5300 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 5301 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 5302 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 5303 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 5304 | ` *  (called "methods").` |
|       - | 5305 | ` */` |
|       - | 5306 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 5307 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 5308 | `struct TraitUseEntry {` |
|       - | 5309 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 5310 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 5311 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 5312 | `};` |
|       - | 5313 | `/*` |
|       - | 5314 | ` * Validate that methods implementing interface contracts have compatible` |
|       - | 5315 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - | 5316 | ` */` |
|   31320 | 5317 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5318 |  |
|       - | 5319 | `	ph7_class **apIface;` |
|       - | 5320 | `	sxu32 nIface,i;` |
|       - | 5321 | `	sxi32 rc;` |
|   31322 | 5322 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 | 5323 | `		return SXRET_OK;` |
|       - | 5324 | `	}` |
|   31322 | 5325 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   31322 | 5326 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   33936 | 5327 | `	for(i = 0; i < nIface; i++){` |
|    2616 | 5328 | `		ph7_class *pIface = apIface[i];` |
|       - | 5329 | `		SyHashEntry *pEntry;` |
|    2616 | 5330 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   15574 | 5331 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   12960 | 5332 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - | 5333 | `			ph7_class_method *pImplMeth;` |
|   12960 | 5334 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - | 5335 | `			/* Find the implementing method in the class */` |
|   12960 | 5336 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   12960 | 5337 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 | 5338 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - | 5339 | `			}` |
|       - | 5340 | `			/* Check visibility: interface methods must be implemented as public */` |
|   12946 | 5341 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 | 5342 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5343 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 | 5344 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 | 5345 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5346 | `					return SXERR_ABORT;` |
|       - | 5347 | `				}` |
|       1 | 5348 | `			}` |
|       - | 5349 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - | 5350 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - | 5351 | `			 */` |
|       - | 5352 | `			{` |
|   12946 | 5353 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   12946 | 5354 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   12946 | 5355 | `				int sigError = 0;` |
|   12946 | 5356 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 | 5357 | `					sigError = 1;` |
|   12945 | 5358 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - | 5359 | `					/* Extra parameters must all have default values */` |
|       5 | 5360 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - | 5361 | `					sxu32 k;` |
|       7 | 5362 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 | 5363 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 | 5364 | `							sigError = 1;` |
|       3 | 5365 | `							break;` |
|       - | 5366 | `						}` |
|       2 | 5367 | `					}` |
|       2 | 5368 | `				}` |
|   12946 | 5369 | `				if( sigError ){` |
|       - | 5370 | `					SyBlob sImplSig, sIfaceSig;` |
|       - | 5371 | `					ph7_vm_func_arg *aArgs;` |
|       - | 5372 | `					sxu32 j;` |
|       5 | 5373 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 | 5374 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - | 5375 | `					/* Build implementing method signature */` |
|       5 | 5376 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 | 5377 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 | 5378 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 | 5379 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 | 5380 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5381 | `					}` |
|       - | 5382 | `					/* Build interface method signature */` |
|       5 | 5383 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 | 5384 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 | 5385 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 | 5386 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 | 5387 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5388 | `					}` |
|       7 | 5389 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5390 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 | 5391 | `						&pClass->sName,pMName,` |
|       4 | 5392 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 | 5393 | `						&pIface->sName,pMName,` |
|       4 | 5394 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 | 5395 | `					SyBlobRelease(&sImplSig);` |
|       5 | 5396 | `					SyBlobRelease(&sIfaceSig);` |
|       5 | 5397 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5398 | `						return SXERR_ABORT;` |
|       - | 5399 | `					}` |
|       2 | 5400 | `				}` |
|       - | 5401 | `			}` |
|       2 | 5402 | `		}` |
|    1309 | 5403 | `	}` |
|   31322 | 5404 | `	return SXRET_OK;` |
|   15662 | 5405 |  |
|       - | 5406 | `/*` |
|       - | 5407 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - | 5408 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - | 5409 | ` */` |
|   31320 | 5410 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5411 |  |
|       - | 5412 | `	ph7_class_method *pMeth;` |
|       - | 5413 | `	SyHashEntry *pEntry;` |
|       - | 5414 | `	sxu32 nAbstract;` |
|       - | 5415 | `	SyBlob sMsg;` |
|       - | 5416 | `	sxi32 rc;` |
|       - | 5417 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   31322 | 5418 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      20 | 5419 | `		return SXRET_OK;` |
|       - | 5420 | `	}` |
|       - | 5421 | `	/* Count abstract methods */` |
|   31304 | 5422 | `	nAbstract = 0;` |
|   31304 | 5423 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  294556 | 5424 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  263254 | 5425 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  263254 | 5426 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 | 5427 | `			nAbstract++;` |
|       8 | 5428 | `		}` |
|       2 | 5429 | `	}` |
|   31304 | 5430 | `	if( nAbstract == 0 ){` |
|   31290 | 5431 | `		return SXRET_OK;` |
|       - | 5432 | `	}` |
|       - | 5433 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 | 5434 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 | 5435 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - | 5436 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 | 5437 | `		&pClass->sName,nAbstract,` |
|       7 | 5438 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 | 5439 | `		(nAbstract > 1 ? "s" : ""));` |
|       - | 5440 | `	/* Second pass: list methods with origins */` |
|       - | 5441 | `	{` |
|      15 | 5442 | `		sxu32 nListed = 0;` |
|      15 | 5443 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 | 5444 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 | 5445 | `			ph7_class *pOrigin = 0;` |
|       - | 5446 | `			SyString *pMName;` |
|      19 | 5447 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 | 5448 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 | 5449 | `				continue;` |
|       - | 5450 | `			}` |
|      17 | 5451 | `			pMName = &pMeth->sFunc.sName;` |
|      17 | 5452 | `			if( nListed > 0 ){` |
|       3 | 5453 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 | 5454 | `			}` |
|       - | 5455 | `			/* Find the origin of this abstract method.` |
|       - | 5456 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - | 5457 | `			 * inheritance chains) take precedence for interface-declared` |
|       - | 5458 | `			 * methods. Abstract class methods only win when the class` |
|       - | 5459 | `			 * itself declared the abstract method (not inherited from` |
|       - | 5460 | `			 * an interface). Trait methods are adopted into the using` |
|       - | 5461 | `			 * class's namespace.` |
|       - | 5462 | `			 */` |
|       - | 5463 | `			{` |
|       - | 5464 | `				ph7_class **apIface;` |
|       - | 5465 | `				ph7_class **apTrait;` |
|       - | 5466 | `				ph7_class *pWalk;` |
|       - | 5467 | `				sxu32 i;` |
|       - | 5468 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - | 5469 | `				 * (one that was written in the class body, not inherited from an` |
|       - | 5470 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - | 5471 | `				 */` |
|      17 | 5472 | `				if( pClass->pBase ){` |
|       9 | 5473 | `					pWalk = pClass->pBase;` |
|      17 | 5474 | `					while( pWalk ){` |
|       - | 5475 | `						ph7_class_method *pParentMeth;` |
|      11 | 5476 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 | 5477 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - | 5478 | `							/* Exclude methods that came from an interface anywhere` |
|       - | 5479 | `							 * in this class's ancestor chain.` |
|       - | 5480 | `							 */` |
|      11 | 5481 | `							int fromIface = 0;` |
|      11 | 5482 | `							ph7_class *pAnc = pWalk;` |
|      15 | 5483 | `							while( pAnc ){` |
|       - | 5484 | `								ph7_class **apPI;` |
|       - | 5485 | `								sxu32 j;` |
|      13 | 5486 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 | 5487 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 | 5488 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 | 5489 | `										fromIface = 1;` |
|       9 | 5490 | `										break;` |
|       - | 5491 | `									}` |
|     ! 0 | 5492 | `								}` |
|      13 | 5493 | `								if( fromIface ) break;` |
|       5 | 5494 | `								pAnc = pAnc->pBase;` |
|       1 | 5495 | `							}` |
|      11 | 5496 | `							if( !fromIface ){` |
|       3 | 5497 | `								pOrigin = pWalk;` |
|       3 | 5498 | `								break;` |
|       - | 5499 | `							}` |
|       4 | 5500 | `						}` |
|       9 | 5501 | `						pWalk = pWalk->pBase;` |
|       1 | 5502 | `					}` |
|       4 | 5503 | `				}` |
|       - | 5504 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - | 5505 | `				 * each interface's own parent chain for the deepest origin.` |
|       - | 5506 | `				 */` |
|      17 | 5507 | `				if( !pOrigin ){` |
|      15 | 5508 | `					pWalk = pClass;` |
|      37 | 5509 | `					while( pWalk && !pOrigin ){` |
|      23 | 5510 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 | 5511 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 | 5512 | `							ph7_class *pIface = apIface[i];` |
|      13 | 5513 | `							ph7_class *pDeepest = 0;` |
|      25 | 5514 | `							while( pIface ){` |
|      13 | 5515 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 | 5516 | `									pDeepest = pIface;` |
|       6 | 5517 | `								}` |
|      13 | 5518 | `								pIface = pIface->pBase;` |
|       1 | 5519 | `							}` |
|      13 | 5520 | `							if( pDeepest ){` |
|      13 | 5521 | `								pOrigin = pDeepest;` |
|      13 | 5522 | `								break;` |
|       - | 5523 | `							}` |
|     ! 0 | 5524 | `						}` |
|      23 | 5525 | `						pWalk = pWalk->pBase;` |
|       1 | 5526 | `					}` |
|       7 | 5527 | `				}` |
|       - | 5528 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 | 5529 | `				if( !pOrigin ){` |
|       3 | 5530 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 | 5531 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 | 5532 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 | 5533 | `							pOrigin = pClass;` |
|       3 | 5534 | `							break;` |
|       - | 5535 | `						}` |
|     ! 0 | 5536 | `					}` |
|       1 | 5537 | `				}` |
|       - | 5538 | `			}` |
|      17 | 5539 | `			if( pOrigin ){` |
|      17 | 5540 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 | 5541 | `			}else{` |
|       - | 5542 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 | 5543 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - | 5544 | `			}` |
|      17 | 5545 | `			nListed++;` |
|       1 | 5546 | `		}` |
|       - | 5547 | `	}` |
|      15 | 5548 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 | 5549 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 | 5550 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 | 5551 | `	SyBlobRelease(&sMsg);` |
|      15 | 5552 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5553 | `		return SXERR_ABORT;` |
|       - | 5554 | `	}` |
|      15 | 5555 | `	return SXRET_OK;` |
|   15662 | 5556 |  |
|   31324 | 5557 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 5558 |  |
|   31326 | 5559 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5560 | `	ph7_class *pClass,*pBase;` |
|       - | 5561 | `	SyToken *pEnd,*pTmp;` |
|       - | 5562 | `	sxi32 iProtection;` |
|       - | 5563 | `	SySet aInterfaces;` |
|       - | 5564 | `	SySet aUseEntries;` |
|       - | 5565 | `	sxi32 iAttrflags;` |
|       - | 5566 | `	SyString *pName;` |
|       - | 5567 | `	sxi32 nKwrd;` |
|       - | 5568 | `	sxi32 rc;` |
|       - | 5569 | `	/* Jump the 'class' keyword */` |
|   31326 | 5570 | `	pGen->pIn++;` |
|   31326 | 5571 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5572 | `		/* Syntax error */` |
|     ! 0 | 5573 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 5574 | `		if( rc == SXERR_ABORT ){` |
|       - | 5575 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5576 | `			return SXERR_ABORT;` |
|       - | 5577 | `		}` |
|       - | 5578 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 5579 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 5580 | `			pGen->pIn++;` |
|     ! 0 | 5581 | `		}` |
|     ! 0 | 5582 | `		return SXRET_OK;` |
|       - | 5583 | `	}` |
|       - | 5584 | `	/* Extract class name */` |
|   31326 | 5585 | `	pName = &pGen->pIn->sData;` |
|       - | 5586 | `	/* Advance the stream cursor */` |
|   31326 | 5587 | `	pGen->pIn++;` |
|       - | 5588 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5589 | `		SyBlob sFQN;` |
|       - | 5590 | `		SyString sFQNStr;` |
|   31326 | 5591 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   31326 | 5592 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   31326 | 5593 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   31326 | 5594 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   31326 | 5595 | `		SyBlobRelease(&sFQN);` |
|       - | 5596 | `	}` |
|   31326 | 5597 | `	if( pClass == 0 ){` |
|     ! 0 | 5598 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5599 | `		return SXERR_ABORT;` |
|       - | 5600 | `	}` |
|       - | 5601 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   31326 | 5602 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   31326 | 5603 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 5604 | `	/* Assume a standalone class */` |
|   31326 | 5605 | `	pBase = 0;` |
|   31326 | 5606 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5607 | `		SyString *pBaseName;` |
|   20712 | 5608 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   20712 | 5609 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   18100 | 5610 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   18100 | 5611 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5612 | `				/* Syntax error */` |
|     ! 0 | 5613 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5614 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 5615 | `					pName);` |
|     ! 0 | 5616 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5617 | `				if( rc == SXERR_ABORT ){` |
|       - | 5618 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5619 | `					return SXERR_ABORT;` |
|       - | 5620 | `				}` |
|     ! 0 | 5621 | `				return SXRET_OK;` |
|       - | 5622 | `			}` |
|       - | 5623 | `			/* Extract base class name and resolve through namespace/imports */` |
|   18100 | 5624 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5625 | `			{` |
|       - | 5626 | `				SyBlob sResolved;` |
|   18100 | 5627 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   18100 | 5628 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   27149 | 5629 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   18098 | 5630 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   18100 | 5631 | `				SyBlobRelease(&sResolved);` |
|       - | 5632 | `			}` |
|       - | 5633 | `			/* Interfaces are not allowed */` |
|   18100 | 5634 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 5635 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5636 | `			}` |
|   18100 | 5637 | `			if( pBase == 0 ){` |
|       - | 5638 | `				/* Inexistant base class */` |
|     ! 0 | 5639 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 5640 | `				if( rc == SXERR_ABORT ){` |
|       - | 5641 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5642 | `					return SXERR_ABORT;` |
|       - | 5643 | `				}` |
|     ! 0 | 5644 | `			}else{` |
|   18100 | 5645 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 5646 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 5647 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 5648 | `					if( rc == SXERR_ABORT ){` |
|       - | 5649 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5650 | `						return SXERR_ABORT;` |
|       - | 5651 | `					}` |
|     ! 0 | 5652 | `				}` |
|       - | 5653 | `			}` |
|       - | 5654 | `			/* Advance the stream cursor */` |
|   18100 | 5655 | `			pGen->pIn++;` |
|    9049 | 5656 | `		}` |
|   20712 | 5657 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 5658 | `			ph7_class *pInterface;` |
|       - | 5659 | `			SyString *pIntName;` |
|       - | 5660 | `			/* Interface implementation */` |
|    2616 | 5661 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    1307 | 5662 | `			for(;;){` |
|    2616 | 5663 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5664 | `					/* Syntax error */` |
|     ! 0 | 5665 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5666 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 5667 | `						pName);` |
|     ! 0 | 5668 | `					if( rc == SXERR_ABORT ){` |
|       - | 5669 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5670 | `						return SXERR_ABORT;` |
|       - | 5671 | `					}` |
|     ! 0 | 5672 | `					break;` |
|       - | 5673 | `				}` |
|       - | 5674 | `				/* Extract interface name and resolve through namespace/imports */` |
|    2616 | 5675 | `				pIntName = &pGen->pIn->sData;` |
|       - | 5676 | `				{` |
|       - | 5677 | `					SyBlob sResolved;` |
|    2616 | 5678 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    2616 | 5679 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|    5230 | 5680 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    2614 | 5681 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    2616 | 5682 | `					SyBlobRelease(&sResolved);` |
|       - | 5683 | `				}` |
|       - | 5684 | `				/* Only interfaces are allowed */` |
|    2616 | 5685 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5686 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 5687 | `				}` |
|    2616 | 5688 | `				if( pInterface == 0 ){` |
|       - | 5689 | `					/* Inexistant interface */` |
|     ! 0 | 5690 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 5691 | `					if( rc == SXERR_ABORT ){` |
|       - | 5692 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5693 | `						return SXERR_ABORT;` |
|       - | 5694 | `					}` |
|     ! 0 | 5695 | `				}else{` |
|       - | 5696 | `					/* Register interface */` |
|    2616 | 5697 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 5698 | `				}` |
|       - | 5699 | `				/* Advance the stream cursor */` |
|    2616 | 5700 | `				pGen->pIn++;` |
|    2616 | 5701 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    1309 | 5702 | `					break;` |
|       - | 5703 | `				}` |
|     ! 0 | 5704 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 5705 | `			}` |
|    1307 | 5706 | `		}` |
|   10355 | 5707 | `	}` |
|   31326 | 5708 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5709 | `		/* Syntax error */` |
|     ! 0 | 5710 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 5711 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5712 | `		if( rc == SXERR_ABORT ){` |
|       - | 5713 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5714 | `			return SXERR_ABORT;` |
|       - | 5715 | `		}` |
|     ! 0 | 5716 | `		return SXRET_OK;` |
|       - | 5717 | `	}` |
|   31326 | 5718 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   31326 | 5719 | `	pEnd = 0; /* cc warning */` |
|       - | 5720 | `	/* Delimit the class body */` |
|   31326 | 5721 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   31326 | 5722 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5723 | `		/* Syntax error */` |
|     ! 0 | 5724 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 5725 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5726 | `		if( rc == SXERR_ABORT ){` |
|       - | 5727 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5728 | `			return SXERR_ABORT;` |
|       - | 5729 | `		}` |
|     ! 0 | 5730 | `		return SXRET_OK;` |
|       - | 5731 | `	}` |
|       - | 5732 | `	/* Swap token stream */` |
|   31326 | 5733 | `	pTmp = pGen->pEnd;` |
|   31326 | 5734 | `	pGen->pEnd = pEnd;` |
|       - | 5735 | `	/* Set the inherited flags */` |
|   31326 | 5736 | `	pClass->iFlags = iFlags;` |
|       - | 5737 | `	/* Start the parse process */` |
|   67371 | 5738 | `	for(;;){` |
|       - | 5739 | `		/* Jump leading/trailing semi-colons */` |
|  202198 | 5740 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   33736 | 5741 | `			pGen->pIn++;` |
|       2 | 5742 | `		}` |
|  168464 | 5743 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5744 | `			/* End of class body */` |
|   31322 | 5745 | `			break;` |
|       - | 5746 | `		}` |
|  137144 | 5747 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5748 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5749 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5750 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5751 | `			if( rc == SXERR_ABORT ){` |
|       - | 5752 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5753 | `				return SXERR_ABORT;` |
|       - | 5754 | `			}` |
|     ! 0 | 5755 | `			goto done;` |
|       - | 5756 | `		}` |
|       - | 5757 | `		/* Assume public visibility */` |
|  137144 | 5758 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  137144 | 5759 | `		iAttrflags = 0;` |
|  137144 | 5760 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 5761 | `			/* Extract the current keyword */` |
|  137144 | 5762 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  137144 | 5763 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5764 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 5765 | `				TraitUseEntry sUse;` |
|      41 | 5766 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      41 | 5767 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      41 | 5768 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      28 | 5769 | `				for(;;){` |
|       - | 5770 | `					ph7_class *pTrait;` |
|       - | 5771 | `					SyString *pTraitName;` |
|      49 | 5772 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5773 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5774 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 5775 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5776 | `							return SXERR_ABORT;` |
|       - | 5777 | `						}` |
|     ! 0 | 5778 | `						break;` |
|       - | 5779 | `					}` |
|      49 | 5780 | `					pTraitName = &pGen->pIn->sData;` |
|       - | 5781 | `					/* Resolve trait name through namespace/imports */ {` |
|       - | 5782 | `						SyBlob sResolved;` |
|      49 | 5783 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      49 | 5784 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      97 | 5785 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      48 | 5786 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      49 | 5787 | `						SyBlobRelease(&sResolved);` |
|       - | 5788 | `					}` |
|       - | 5789 | `					/* Only traits are allowed */` |
|      49 | 5790 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5791 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 5792 | `					}` |
|      49 | 5793 | `					if( pTrait == 0 ){` |
|     ! 0 | 5794 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5795 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 | 5796 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5797 | `							return SXERR_ABORT;` |
|       - | 5798 | `						}` |
|     ! 0 | 5799 | `					}else{` |
|      49 | 5800 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 5801 | `					}` |
|      49 | 5802 | `					pGen->pIn++; /* Advance past trait name */` |
|      49 | 5803 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      21 | 5804 | `						break;` |
|       - | 5805 | `					}` |
|       9 | 5806 | `					pGen->pIn++; /* Jump the comma */` |
|       1 | 5807 | `				}` |
|       - | 5808 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      41 | 5809 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 5810 | `					SyToken *pBlock;` |
|       9 | 5811 | `					pGen->pIn++; /* Jump '{' */` |
|       9 | 5812 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 | 5813 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 | 5814 | `					sUse.pResolvEnd = pBlock;` |
|       9 | 5815 | `					if( pBlock < pGen->pEnd ){` |
|       9 | 5816 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 | 5817 | `					}else{` |
|     ! 0 | 5818 | `						pGen->pIn = pGen->pEnd;` |
|       - | 5819 | `					}` |
|       4 | 5820 | `				}` |
|      41 | 5821 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 5822 | `				/* The semicolon will be consumed by the outer loop */` |
|      41 | 5823 | `				continue;` |
|       - | 5824 | `			}` |
|  137104 | 5825 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  134422 | 5826 | `				iProtection = nKwrd;` |
|  134422 | 5827 | `				pGen->pIn++; /* Jump the visibility token */` |
|  134422 | 5828 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5829 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5830 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5831 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5832 | `					if( rc == SXERR_ABORT ){` |
|       - | 5833 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5834 | `						return SXERR_ABORT;` |
|       - | 5835 | `					}` |
|     ! 0 | 5836 | `					goto done;` |
|       - | 5837 | `				}` |
|  134422 | 5838 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5839 | `					/* Attribute declaration */` |
|   33680 | 5840 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   33680 | 5841 | `					if( rc != SXRET_OK ){` |
|       3 | 5842 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5843 | `							return SXERR_ABORT;` |
|       - | 5844 | `						}` |
|       3 | 5845 | `						goto done;` |
|       - | 5846 | `					}` |
|   33678 | 5847 | `					continue;` |
|       - | 5848 | `				}` |
|       - | 5849 | `				/* Extract the keyword */` |
|  100744 | 5850 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   50371 | 5851 | `			}` |
|  103426 | 5852 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5853 | `				/* Process constant declaration */` |
|      10 | 5854 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 | 5855 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5856 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5857 | `						return SXERR_ABORT;` |
|       - | 5858 | `					}` |
|     ! 0 | 5859 | `					goto done;` |
|       - | 5860 | `				}` |
|       6 | 5861 | `			}else{` |
|  103418 | 5862 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5863 | `					/* Static method or attribute,record that */` |
|    2600 | 5864 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2600 | 5865 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2600 | 5866 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5867 | `						/* Extract the keyword */` |
|    2596 | 5868 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2596 | 5869 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 5870 | `							iProtection = nKwrd;` |
|     ! 0 | 5871 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 5872 | `						}` |
|    1297 | 5873 | `					}` |
|    2600 | 5874 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5875 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5876 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 5877 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5878 | `						if( rc == SXERR_ABORT ){` |
|       - | 5879 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5880 | `							return SXERR_ABORT;` |
|       - | 5881 | `						}` |
|     ! 0 | 5882 | `						goto done;` |
|       - | 5883 | `					}` |
|    2600 | 5884 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5885 | `						/* Attribute declaration */` |
|       5 | 5886 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 5887 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 5888 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5889 | `								return SXERR_ABORT;` |
|       - | 5890 | `							}` |
|     ! 0 | 5891 | `							goto done;` |
|       - | 5892 | `						}` |
|       5 | 5893 | `						continue;` |
|       - | 5894 | `					}` |
|       - | 5895 | `					/* Extract the keyword */` |
|    2596 | 5896 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  102117 | 5897 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 5898 | `					/* Abstract method,record that */` |
|      10 | 5899 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 5900 | `					/* Mark the whole class as abstract */` |
|      10 | 5901 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 5902 | `					/* Advance the stream cursor */` |
|      10 | 5903 | `					pGen->pIn++;` |
|      10 | 5904 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      10 | 5905 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      10 | 5906 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 | 5907 | `							iProtection = nKwrd;` |
|       8 | 5908 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 | 5909 | `						}` |
|       4 | 5910 | `					}` |
|      10 | 5911 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 5912 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5913 | `							/* Static method */` |
|     ! 0 | 5914 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5915 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5916 | `					}` |
|      10 | 5917 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       8 | 5918 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5919 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5920 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 5921 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5922 | `							if( rc == SXERR_ABORT ){` |
|       - | 5923 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5924 | `								return SXERR_ABORT;` |
|       - | 5925 | `							}` |
|     ! 0 | 5926 | `							goto done;` |
|       - | 5927 | `					}` |
|      10 | 5928 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  100816 | 5929 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 5930 | `					/* final method ,record that */` |
|       5 | 5931 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 5932 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 5933 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5934 | `						/* Extract the keyword */` |
|       5 | 5935 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 5936 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 5937 | `							iProtection = nKwrd;` |
|       5 | 5938 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 5939 | `						}` |
|       2 | 5940 | `					}` |
|       5 | 5941 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 5942 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5943 | `							/* Static method */` |
|     ! 0 | 5944 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5945 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5946 | `					}` |
|       5 | 5947 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 5948 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5949 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5950 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 5951 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5952 | `							if( rc == SXERR_ABORT ){` |
|       - | 5953 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5954 | `								return SXERR_ABORT;` |
|       - | 5955 | `							}` |
|     ! 0 | 5956 | `							goto done;` |
|       - | 5957 | `					}` |
|       5 | 5958 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 5959 | `				}` |
|  103414 | 5960 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 5961 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5962 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 5963 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5964 | `						if( rc == SXERR_ABORT ){` |
|       - | 5965 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5966 | `							return SXERR_ABORT;` |
|       - | 5967 | `						}` |
|     ! 0 | 5968 | `						goto done;` |
|       - | 5969 | `				}` |
|  103414 | 5970 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 5971 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 5972 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 5973 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5974 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 5975 | `						if( rc == SXERR_ABORT ){` |
|       - | 5976 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5977 | `							return SXERR_ABORT;` |
|       - | 5978 | `						}` |
|     ! 0 | 5979 | `						goto done;` |
|       - | 5980 | `					}` |
|       - | 5981 | `					/* Attribute declaration */` |
|       7 | 5982 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 5983 | `				}else{` |
|       - | 5984 | `					/* Process method declaration */` |
|  103408 | 5985 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 5986 | `				}` |
|  103414 | 5987 | `				if( rc != SXRET_OK ){` |
|       3 | 5988 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5989 | `						return SXERR_ABORT;` |
|       - | 5990 | `					}` |
|       3 | 5991 | `					goto done;` |
|       - | 5992 | `				}` |
|       - | 5993 | `			}` |
|   51711 | 5994 | `		}else{` |
|       - | 5995 | `			/* Attribute declaration */` |
|     ! 0 | 5996 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5997 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5998 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5999 | `					return SXERR_ABORT;` |
|       - | 6000 | `				}` |
|     ! 0 | 6001 | `				goto done;` |
|       - | 6002 | `			}` |
|       - | 6003 | `		}` |
|       2 | 6004 | `	}` |
|       - | 6005 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 6006 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 6007 | `	 */` |
|       - | 6008 | `	{` |
|       - | 6009 | `		TraitUseEntry *apUse;` |
|       - | 6010 | `		sxu32 nU;` |
|   31322 | 6011 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   31362 | 6012 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      41 | 6013 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      41 | 6014 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      41 | 6015 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      41 | 6016 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 6017 | `			sxu32 nT;` |
|      41 | 6018 | `			if( !hasResolution ){` |
|       - | 6019 | `				/* No conflict resolution block: use standard trait application */` |
|      71 | 6020 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      39 | 6021 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      39 | 6022 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6023 | `						break;` |
|       - | 6024 | `					}` |
|      20 | 6025 | `				}` |
|      17 | 6026 | `			}else{` |
|       - | 6027 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 6028 | `				 * then use the block to resolve method conflicts.` |
|       - | 6029 | `				 */` |
|       - | 6030 | `				SyToken *pR;` |
|      19 | 6031 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 | 6032 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 6033 | `					ph7_class_attr *pAR;` |
|       - | 6034 | `					SyHashEntry *pER;` |
|       - | 6035 | `					SyString *pNR;` |
|      11 | 6036 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 | 6037 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 6038 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 6039 | `						pNR = &pAR->sName;` |
|     ! 0 | 6040 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 6041 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 6042 | `						}` |
|     ! 0 | 6043 | `					}` |
|      11 | 6044 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 | 6045 | `				}` |
|       - | 6046 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 | 6047 | `				pR = pUse->pResolvStart;` |
|      21 | 6048 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6049 | `					SyString sTrait,sMethod;` |
|       - | 6050 | `					ph7_class *pSrcTrait;` |
|       - | 6051 | `					ph7_class_method *pMeth;` |
|       - | 6052 | `					sxi32 nRKwrd;` |
|      33 | 6053 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6054 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6055 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6056 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6057 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6058 | `					sMethod = pR->sData;` |
|      13 | 6059 | `					pR++;` |
|      13 | 6060 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6061 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6062 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6063 | `							sTrait = sMethod;` |
|       7 | 6064 | `							pR++;` |
|       7 | 6065 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6066 | `							sMethod = pR->sData;` |
|       7 | 6067 | `							pR++;` |
|       3 | 6068 | `						}` |
|       3 | 6069 | `					}` |
|      13 | 6070 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6071 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6072 | `						continue;` |
|       - | 6073 | `					}` |
|      13 | 6074 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6075 | `					pR++;` |
|      13 | 6076 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 | 6077 | `						pSrcTrait = 0;` |
|       7 | 6078 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 | 6079 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 | 6080 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 | 6081 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 | 6082 | `								pSrcTrait = apTrait[nT];` |
|       5 | 6083 | `								break;` |
|       - | 6084 | `							}` |
|       2 | 6085 | `						}` |
|       5 | 6086 | `						if( pSrcTrait ){` |
|       5 | 6087 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 | 6088 | `							if( pMeth ){` |
|       5 | 6089 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 | 6090 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 | 6091 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 | 6092 | `								}` |
|       2 | 6093 | `							}` |
|       2 | 6094 | `						}` |
|       2 | 6095 | `					}` |
|      29 | 6096 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6097 | `				}` |
|       - | 6098 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 | 6099 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 6100 | `					ph7_class_method *pMR;` |
|       - | 6101 | `					SyHashEntry *pER;` |
|       - | 6102 | `					SyString *pNR;` |
|      11 | 6103 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 | 6104 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 | 6105 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 | 6106 | `						pNR = &pMR->sFunc.sName;` |
|      19 | 6107 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 | 6108 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 | 6109 | `						}` |
|       1 | 6110 | `					}` |
|       6 | 6111 | `				}` |
|       - | 6112 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 | 6113 | `				pR = pUse->pResolvStart;` |
|      21 | 6114 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6115 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 6116 | `					ph7_class *pSrcTrait;` |
|       - | 6117 | `					ph7_class_method *pMeth;` |
|      21 | 6118 | `					int hasQual = 0;` |
|       - | 6119 | `					sxi32 nRKwrd;` |
|      33 | 6120 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6121 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6122 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6123 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6124 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 | 6125 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6126 | `					sMethod = pR->sData;` |
|      13 | 6127 | `					pR++;` |
|      13 | 6128 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6129 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6130 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6131 | `							sTrait = sMethod;` |
|       7 | 6132 | `							hasQual = 1;` |
|       7 | 6133 | `							pR++;` |
|       7 | 6134 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6135 | `							sMethod = pR->sData;` |
|       7 | 6136 | `							pR++;` |
|       3 | 6137 | `						}` |
|       3 | 6138 | `					}` |
|      13 | 6139 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6140 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6141 | `						continue;` |
|       - | 6142 | `					}` |
|      13 | 6143 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6144 | `					pR++;` |
|      13 | 6145 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 | 6146 | `						sxi32 iNewVis = -1;` |
|       9 | 6147 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 | 6148 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 | 6149 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 | 6150 | `								iNewVis = nAK;` |
|       7 | 6151 | `								pR++;` |
|       3 | 6152 | `							}` |
|       3 | 6153 | `						}` |
|       9 | 6154 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 | 6155 | `							sAlias = pR->sData;` |
|       7 | 6156 | `							pR++;` |
|       3 | 6157 | `						}` |
|       9 | 6158 | `						pMeth = 0;` |
|       9 | 6159 | `						if( hasQual ){` |
|       3 | 6160 | `							pSrcTrait = 0;` |
|       5 | 6161 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 | 6162 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 | 6163 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 | 6164 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 | 6165 | `									pSrcTrait = apTrait[nT];` |
|       3 | 6166 | `									break;` |
|       - | 6167 | `								}` |
|       2 | 6168 | `							}` |
|       3 | 6169 | `							if( pSrcTrait ){` |
|       3 | 6170 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 | 6171 | `							}` |
|       2 | 6172 | `						}else{` |
|       7 | 6173 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 6174 | `						}` |
|       9 | 6175 | `						if( pMeth ){` |
|       9 | 6176 | `							if( sAlias.nByte > 0 ){` |
|       - | 6177 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 6178 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 6179 | `								 */` |
|       - | 6180 | `								ph7_class_method *pAlias;` |
|       - | 6181 | `								char *zAliasDup;` |
|       7 | 6182 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 | 6183 | `								if( pAlias ){` |
|       7 | 6184 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 | 6185 | `									if( iNewVis >= 0 ){` |
|       5 | 6186 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6187 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6188 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 | 6189 | `									}` |
|       7 | 6190 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 | 6191 | `									if( zAliasDup ){` |
|       7 | 6192 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 | 6193 | `									}` |
|       4 | 6194 | `								}` |
|       6 | 6195 | `							}else if( iNewVis >= 0 ){` |
|       - | 6196 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 6197 | `								ph7_class_method *pCopy;` |
|       3 | 6198 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 6199 | `								if( pCopy ){` |
|       3 | 6200 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 6201 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 6202 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6203 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6204 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 6205 | `									/* Replace the method in the class hash */` |
|       3 | 6206 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 6207 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 6208 | `								}` |
|       1 | 6209 | `							}` |
|       4 | 6210 | `						}` |
|       4 | 6211 | `						SXUNUSED(hasQual);` |
|       4 | 6212 | `					}` |
|      17 | 6213 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6214 | `				}` |
|       - | 6215 | `			}` |
|      41 | 6216 | `			SySetRelease(&pUse->aTraits);` |
|      21 | 6217 | `		}` |
|       - | 6218 | `	}` |
|       - | 6219 | `	/* Install the class */` |
|   31322 | 6220 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   31322 | 6221 | `	if( rc == SXRET_OK ){` |
|       - | 6222 | `		ph7_class **apInterface;` |
|       - | 6223 | `		sxu32 n;` |
|   31322 | 6224 | `		if( pBase ){` |
|       - | 6225 | `			/* Inherit from base class and mark as a subclass */` |
|   18100 | 6226 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    9049 | 6227 | `		}` |
|   31322 | 6228 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   33936 | 6229 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 6230 | `			/* Implements one or more interface */` |
|    2616 | 6231 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    2616 | 6232 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6233 | `				break;` |
|       - | 6234 | `			}` |
|    1309 | 6235 | `		}` |
|       - | 6236 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   31322 | 6237 | `		if( rc == SXRET_OK ){` |
|   31322 | 6238 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   31322 | 6239 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6240 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6241 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6242 | `				return SXERR_ABORT;` |
|       - | 6243 | `			}` |
|   15660 | 6244 | `		}` |
|       - | 6245 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   31322 | 6246 | `		if( rc == SXRET_OK ){` |
|   31322 | 6247 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   31322 | 6248 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6249 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6250 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6251 | `				return SXERR_ABORT;` |
|       - | 6252 | `			}` |
|   15660 | 6253 | `		}` |
|   15660 | 6254 | `	}` |
|   31322 | 6255 | `	SySetRelease(&aUseEntries);` |
|   31322 | 6256 | `	SySetRelease(&aInterfaces);` |
|   31322 | 6257 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6258 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6259 | `		return SXERR_ABORT;` |
|       - | 6260 | `	}` |
|   15660 | 6261 | `done:` |
|       - | 6262 | `	/* Point beyond the class body */` |
|   31326 | 6263 | `	pGen->pIn = &pEnd[1];` |
|   31326 | 6264 | `	pGen->pEnd = pTmp;` |
|   31326 | 6265 | `	return PH7_OK;` |
|   15664 | 6266 |  |
|       - | 6267 | `/*` |
|       - | 6268 | ` * Compile a user-defined abstract class.` |
|       - | 6269 | ` *  According to the PHP language reference manual` |
|       - | 6270 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 6271 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 6272 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 6273 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 6274 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 6275 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 6276 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 6277 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 6278 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 6279 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 6280 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 6281 | ` *   could differ.` |
|       - | 6282 | ` */` |
|      16 | 6283 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 6284 |  |
|       - | 6285 | `	sxi32 rc;` |
|      18 | 6286 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      18 | 6287 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      18 | 6288 | `	return rc;` |
|       2 | 6289 |  |
|       - | 6290 | `/*` |
|       - | 6291 | ` * Compile a user-defined final class.` |
|       - | 6292 | ` *  According to the PHP language reference manual` |
|       - | 6293 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 6294 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 6295 | ` *    final then it cannot be extended.` |
|       - | 6296 | ` */` |
|       2 | 6297 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 6298 |  |
|       - | 6299 | `	sxi32 rc;` |
|       3 | 6300 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 6301 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 6302 | `	return rc;` |
|       1 | 6303 |  |
|       - | 6304 | `/*` |
|       - | 6305 | ` * Compile a user-defined trait.` |
|       - | 6306 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 6307 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 6308 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 6309 | ` */` |
|      52 | 6310 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 | 6311 |  |
|      54 | 6312 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6313 | `	ph7_class *pClass;` |
|       - | 6314 | `	SyToken *pEnd,*pTmp;` |
|       - | 6315 | `	sxi32 iProtection;` |
|       - | 6316 | `	sxi32 iAttrflags;` |
|       - | 6317 | `	SyString *pName;` |
|       - | 6318 | `	sxi32 nKwrd;` |
|       - | 6319 | `	sxi32 rc;` |
|       - | 6320 | `	/* Jump the 'trait' keyword */` |
|      54 | 6321 | `	pGen->pIn++;` |
|      54 | 6322 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6323 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 6324 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6325 | `			return SXERR_ABORT;` |
|       - | 6326 | `		}` |
|     ! 0 | 6327 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 6328 | `			pGen->pIn++;` |
|     ! 0 | 6329 | `		}` |
|     ! 0 | 6330 | `		return SXRET_OK;` |
|       - | 6331 | `	}` |
|       - | 6332 | `	/* Extract trait name */` |
|      54 | 6333 | `	pName = &pGen->pIn->sData;` |
|      54 | 6334 | `	pGen->pIn++;` |
|       - | 6335 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 6336 | `		SyBlob sFQN;` |
|       - | 6337 | `		SyString sFQNStr;` |
|      54 | 6338 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      54 | 6339 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      54 | 6340 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      54 | 6341 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      54 | 6342 | `		SyBlobRelease(&sFQN);` |
|       - | 6343 | `	}` |
|      54 | 6344 | `	if( pClass == 0 ){` |
|     ! 0 | 6345 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6346 | `		return SXERR_ABORT;` |
|       - | 6347 | `	}` |
|       - | 6348 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      54 | 6349 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 6350 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 6351 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6352 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6353 | `			return SXERR_ABORT;` |
|       - | 6354 | `		}` |
|     ! 0 | 6355 | `		return SXRET_OK;` |
|       - | 6356 | `	}` |
|      54 | 6357 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      54 | 6358 | `	pEnd = 0;` |
|      54 | 6359 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      54 | 6360 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 6361 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 6362 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6363 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6364 | `			return SXERR_ABORT;` |
|       - | 6365 | `		}` |
|     ! 0 | 6366 | `		return SXRET_OK;` |
|       - | 6367 | `	}` |
|       - | 6368 | `	/* Swap token stream */` |
|      54 | 6369 | `	pTmp = pGen->pEnd;` |
|      54 | 6370 | `	pGen->pEnd = pEnd;` |
|       - | 6371 | `	/* Mark as trait */` |
|      54 | 6372 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 6373 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      53 | 6374 | `	for(;;){` |
|     144 | 6375 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      21 | 6376 | `			pGen->pIn++;` |
|       1 | 6377 | `		}` |
|     124 | 6378 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      54 | 6379 | `			break;` |
|       - | 6380 | `		}` |
|      71 | 6381 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6382 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6383 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6384 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 6385 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6386 | `				return SXERR_ABORT;` |
|       - | 6387 | `			}` |
|     ! 0 | 6388 | `			goto done;` |
|       - | 6389 | `		}` |
|      71 | 6390 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      71 | 6391 | `		iAttrflags = 0;` |
|      71 | 6392 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      71 | 6393 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      71 | 6394 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 6395 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 6396 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 6397 | `				for(;;){` |
|       - | 6398 | `					ph7_class *pUsedTrait;` |
|       - | 6399 | `					SyString *pUsedName;` |
|       5 | 6400 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6401 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6402 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 6403 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6404 | `							return SXERR_ABORT;` |
|       - | 6405 | `						}` |
|     ! 0 | 6406 | `						break;` |
|       - | 6407 | `					}` |
|       5 | 6408 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 6409 | `					{` |
|       - | 6410 | `						SyBlob sResolved;` |
|       5 | 6411 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 6412 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 6413 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 6414 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 6415 | `						SyBlobRelease(&sResolved);` |
|       - | 6416 | `					}` |
|       5 | 6417 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 6418 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 6419 | `					}` |
|       5 | 6420 | `					if( pUsedTrait == 0 ){` |
|       4 | 6421 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 6422 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 6423 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6424 | `							return SXERR_ABORT;` |
|       - | 6425 | `						}` |
|       2 | 6426 | `					}else{` |
|       3 | 6427 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 6428 | `					}` |
|       5 | 6429 | `					pGen->pIn++;` |
|       5 | 6430 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 6431 | `						break;` |
|       - | 6432 | `					}` |
|     ! 0 | 6433 | `					pGen->pIn++;` |
|     ! 0 | 6434 | `				}` |
|       5 | 6435 | `				continue;` |
|       - | 6436 | `			}` |
|      67 | 6437 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      63 | 6438 | `				iProtection = nKwrd;` |
|      63 | 6439 | `				pGen->pIn++;` |
|      63 | 6440 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6441 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6442 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6443 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6444 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6445 | `						return SXERR_ABORT;` |
|       - | 6446 | `					}` |
|     ! 0 | 6447 | `					goto done;` |
|       - | 6448 | `				}` |
|      63 | 6449 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 | 6450 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 | 6451 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6452 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6453 | `							return SXERR_ABORT;` |
|       - | 6454 | `						}` |
|     ! 0 | 6455 | `						goto done;` |
|       - | 6456 | `					}` |
|      11 | 6457 | `					continue;` |
|       - | 6458 | `				}` |
|      53 | 6459 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 | 6460 | `			}` |
|      57 | 6461 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 6462 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6463 | `					"Traits cannot have constants");` |
|     ! 0 | 6464 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6465 | `					return SXERR_ABORT;` |
|       - | 6466 | `				}` |
|     ! 0 | 6467 | `				goto done;` |
|     ! 0 | 6468 | `			}else{` |
|      57 | 6469 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 6470 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 6471 | `					pGen->pIn++;` |
|       5 | 6472 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 6473 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 6474 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6475 | `							iProtection = nKwrd;` |
|     ! 0 | 6476 | `							pGen->pIn++;` |
|     ! 0 | 6477 | `						}` |
|       1 | 6478 | `					}` |
|       5 | 6479 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6480 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6481 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 6482 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6483 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6484 | `							return SXERR_ABORT;` |
|       - | 6485 | `						}` |
|     ! 0 | 6486 | `						goto done;` |
|       - | 6487 | `					}` |
|       5 | 6488 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 6489 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 6490 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6491 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6492 | `								return SXERR_ABORT;` |
|       - | 6493 | `							}` |
|     ! 0 | 6494 | `							goto done;` |
|       - | 6495 | `						}` |
|       3 | 6496 | `						continue;` |
|       - | 6497 | `					}` |
|       3 | 6498 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 | 6499 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 | 6500 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 | 6501 | `					pGen->pIn++;` |
|       5 | 6502 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 6503 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6504 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6505 | `							iProtection = nKwrd;` |
|       5 | 6506 | `							pGen->pIn++;` |
|       2 | 6507 | `						}` |
|       2 | 6508 | `					}` |
|       5 | 6509 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6510 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6511 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6512 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 6513 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6514 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6515 | `							return SXERR_ABORT;` |
|       - | 6516 | `						}` |
|     ! 0 | 6517 | `						goto done;` |
|       - | 6518 | `					}` |
|       5 | 6519 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6520 | `				}` |
|      55 | 6521 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 6522 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6523 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 6524 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6525 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6526 | `						return SXERR_ABORT;` |
|       - | 6527 | `					}` |
|     ! 0 | 6528 | `					goto done;` |
|       - | 6529 | `				}` |
|      55 | 6530 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 6531 | `					pGen->pIn++;` |
|     ! 0 | 6532 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 6533 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6534 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 6535 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6536 | `							return SXERR_ABORT;` |
|       - | 6537 | `						}` |
|     ! 0 | 6538 | `						goto done;` |
|       - | 6539 | `					}` |
|     ! 0 | 6540 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6541 | `				}else{` |
|      55 | 6542 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 6543 | `				}` |
|      55 | 6544 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6545 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6546 | `						return SXERR_ABORT;` |
|       - | 6547 | `					}` |
|     ! 0 | 6548 | `					goto done;` |
|       - | 6549 | `				}` |
|       - | 6550 | `			}` |
|      28 | 6551 | `		}else{` |
|     ! 0 | 6552 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6553 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6554 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6555 | `					return SXERR_ABORT;` |
|       - | 6556 | `				}` |
|     ! 0 | 6557 | `				goto done;` |
|       - | 6558 | `			}` |
|       - | 6559 | `		}` |
|       1 | 6560 | `	}` |
|       - | 6561 | `	/* Install the trait */` |
|      54 | 6562 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      54 | 6563 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6564 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6565 | `		return SXERR_ABORT;` |
|       - | 6566 | `	}` |
|      26 | 6567 | `done:` |
|       - | 6568 | `	/* Point beyond the trait body */` |
|      54 | 6569 | `	pGen->pIn = &pEnd[1];` |
|      54 | 6570 | `	pGen->pEnd = pTmp;` |
|      54 | 6571 | `	return PH7_OK;` |
|      28 | 6572 |  |
|       - | 6573 | `/*` |
|       - | 6574 | ` * Compile a user-defined class.` |
|       - | 6575 | ` *  According to the PHP language reference manual` |
|       - | 6576 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 6577 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 6578 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 6579 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 6580 | ` *   and functions (called "methods").` |
|       - | 6581 | ` */` |
|   31306 | 6582 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 6583 |  |
|       - | 6584 | `	sxi32 rc;` |
|   31308 | 6585 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   31308 | 6586 | `	return rc;` |
|       2 | 6587 |  |
|       - | 6588 | `/*` |
|       - | 6589 | ` * Exception handling.` |
|       - | 6590 | ` *  According to the PHP language reference manual` |
|       - | 6591 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 6592 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 6593 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 6594 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 6595 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 6596 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 6597 | ` *    (or re-thrown) within a catch block.` |
|       - | 6598 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 6599 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 6600 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 6601 | ` *    been defined with set_exception_handler().` |
|       - | 6602 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 6603 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 6604 | ` */` |
|       - | 6605 | `/*` |
|       - | 6606 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 6607 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 6608 | ` * indicates failure.` |
|       - | 6609 | ` */` |
|    7760 | 6610 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 6611 |  |
|    7762 | 6612 | `	sxi32 rc = SXRET_OK;` |
|    7762 | 6613 | `	if( pRoot->pOp ){` |
|    7758 | 6614 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    3881 | 6615 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 6616 | `			/* Unexpected expression */` |
|     ! 0 | 6617 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6618 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 6619 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 6620 | `				rc = SXERR_INVALID;` |
|     ! 0 | 6621 | `			}` |
|       2 | 6622 | `		}` |
|    3882 | 6623 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 6624 | `		/* Unexpected expression */` |
|     ! 0 | 6625 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6626 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 6627 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 6628 | `			rc = SXERR_INVALID;` |
|     ! 0 | 6629 | `		}` |
|     ! 0 | 6630 | `	}` |
|    7762 | 6631 | `	return rc;` |
|       2 | 6632 |  |
|       - | 6633 | `/*` |
|       - | 6634 | ` * Compile a 'throw' statement.` |
|       - | 6635 | ` * throw: This is how you trigger an exception.` |
|       - | 6636 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 6637 | ` */` |
|    7760 | 6638 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 6639 |  |
|    7762 | 6640 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6641 | `	GenBlock *pBlock;` |
|       - | 6642 | `	sxu32 nIdx;` |
|       - | 6643 | `	sxi32 rc;` |
|    7762 | 6644 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 6645 | `	/* Compile the expression */` |
|    7762 | 6646 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    7762 | 6647 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 6648 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 6649 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6650 | `			return SXERR_ABORT;` |
|       - | 6651 | `		}` |
|     ! 0 | 6652 | `		return SXRET_OK;` |
|       - | 6653 | `	}` |
|    7762 | 6654 | `	pBlock = pGen->pCurrent;` |
|       - | 6655 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   36130 | 6656 | `	while(pBlock->pParent){` |
|   36126 | 6657 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    7758 | 6658 | `			break;` |
|       - | 6659 | `		}` |
|       - | 6660 | `		/* Point to the parent block */` |
|   28370 | 6661 | `		pBlock = pBlock->pParent;` |
|       2 | 6662 | `	}` |
|       - | 6663 | `	/* Emit the throw instruction */` |
|    7762 | 6664 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 6665 | `	/* Emit the jump */` |
|    7762 | 6666 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    7762 | 6667 | `	return SXRET_OK;` |
|    3882 | 6668 |  |
|       - | 6669 | `/*` |
|       - | 6670 | ` * Compile a 'catch' block.` |
|       - | 6671 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 6672 | ` * an object containing the exception information.` |
|       - | 6673 | ` */` |
|      56 | 6674 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 6675 |  |
|      58 | 6676 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6677 | `	ph7_exception_block sCatch;` |
|       - | 6678 | `	SySet *pInstrContainer;` |
|       - | 6679 | `	GenBlock *pCatch;` |
|       - | 6680 | `	SyToken *pToken;` |
|       - | 6681 | `	SyString *pName;` |
|       - | 6682 | `	char *zDup;` |
|       - | 6683 | `	sxi32 rc;` |
|      58 | 6684 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 6685 | `	/* Zero the structure */` |
|      58 | 6686 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 6687 | `	/* Initialize fields */` |
|      58 | 6688 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|      84 | 6689 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ \|\|` |
|      58 | 6690 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6691 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6692 | `			pToken = pGen->pIn;` |
|     ! 0 | 6693 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6694 | `				pToken--;` |
|     ! 0 | 6695 | `			}` |
|     ! 0 | 6696 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6697 | `				"Catch: Unexpected token '%z',excpecting class name",&pToken->sData);` |
|     ! 0 | 6698 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6699 | `				return SXERR_ABORT;` |
|       - | 6700 | `			}` |
|     ! 0 | 6701 | `			return SXERR_INVALID;` |
|       - | 6702 | `	}` |
|       - | 6703 | `	/* Extract the exception class */` |
|      58 | 6704 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|       - | 6705 | `	/* Duplicate class name */` |
|      58 | 6706 | `	pName = &pGen->pIn->sData;` |
|      58 | 6707 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      58 | 6708 | `	if( zDup == 0 ){` |
|     ! 0 | 6709 | `		goto Mem;` |
|       - | 6710 | `	}` |
|      58 | 6711 | `	SyStringInitFromBuf(&sCatch.sClass,zDup,pName->nByte);` |
|      58 | 6712 | `	pGen->pIn++;` |
|      84 | 6713 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      58 | 6714 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6715 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6716 | `			pToken = pGen->pIn;` |
|     ! 0 | 6717 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6718 | `				pToken--;` |
|     ! 0 | 6719 | `			}` |
|     ! 0 | 6720 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6721 | `				"Catch: Unexpected token '%z',expecting variable name",&pToken->sData);` |
|     ! 0 | 6722 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6723 | `				return SXERR_ABORT;` |
|       - | 6724 | `			}` |
|     ! 0 | 6725 | `			return SXERR_INVALID;` |
|       - | 6726 | `	}` |
|      58 | 6727 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 6728 | `	/* Duplicate instance name */` |
|      58 | 6729 | `	pName = &pGen->pIn->sData;` |
|      58 | 6730 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      58 | 6731 | `	if( zDup == 0 ){` |
|     ! 0 | 6732 | `		goto Mem;` |
|       - | 6733 | `	}` |
|      58 | 6734 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      58 | 6735 | `	pGen->pIn++;` |
|      58 | 6736 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 6737 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 6738 | `		pToken = pGen->pIn;` |
|     ! 0 | 6739 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6740 | `			pToken--;` |
|     ! 0 | 6741 | `		}` |
|     ! 0 | 6742 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6743 | `			"Catch: Unexpected token '%z',expecting right parenthesis ')'",&pToken->sData);` |
|     ! 0 | 6744 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6745 | `			return SXERR_ABORT;` |
|       - | 6746 | `		}` |
|     ! 0 | 6747 | `		return SXERR_INVALID;` |
|       - | 6748 | `	}` |
|       - | 6749 | `	/* Compile the block */` |
|      58 | 6750 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 6751 | `	/* Create the catch block */` |
|      58 | 6752 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      58 | 6753 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6754 | `		return SXERR_ABORT;` |
|       - | 6755 | `	}` |
|       - | 6756 | `	/* Swap bytecode container */` |
|      58 | 6757 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      58 | 6758 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 6759 | `	/* Compile the block */` |
|      58 | 6760 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 6761 | `	/* Fix forward jumps now the destination is resolved  */` |
|      58 | 6762 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6763 | `	/* Emit the DONE instruction */` |
|      58 | 6764 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6765 | `	/* Leave the block */` |
|      58 | 6766 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6767 | `	/* Restore the default container */` |
|      58 | 6768 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 6769 | `	/* Install the catch block */` |
|      58 | 6770 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      58 | 6771 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6772 | `		goto Mem;` |
|       - | 6773 | `	}` |
|      58 | 6774 | `	return SXRET_OK;` |
|     ! 0 | 6775 | `Mem:` |
|     ! 0 | 6776 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6777 | `	return SXERR_ABORT;` |
|      30 | 6778 |  |
|       - | 6779 | `/*` |
|       - | 6780 | ` * Compile a 'try' block.` |
|       - | 6781 | ` * A function using an exception should be in a "try" block.` |
|       - | 6782 | ` * If the exception does not trigger, the code will continue` |
|       - | 6783 | ` * as normal. However if the exception triggers, an exception` |
|       - | 6784 | ` * is "thrown".` |
|       - | 6785 | ` */` |
|      68 | 6786 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 6787 |  |
|       - | 6788 | `	ph7_exception *pException;` |
|      70 | 6789 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6790 | `	GenBlock *pTry;` |
|       - | 6791 | `	sxu32 nJmpIdx;` |
|       - | 6792 | `	sxi32 rc;` |
|       - | 6793 | `	/* Create the exception container */` |
|      70 | 6794 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|      70 | 6795 | `	if( pException == 0 ){` |
|     ! 0 | 6796 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 6797 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6798 | `		return SXERR_ABORT;` |
|       - | 6799 | `	}` |
|       - | 6800 | `	/* Zero the structure */` |
|      70 | 6801 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 6802 | `	/* Initialize fields */` |
|      70 | 6803 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|      70 | 6804 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      70 | 6805 | `	pException->iHasFinally = 0;` |
|      70 | 6806 | `	pException->iFinallyDone = 0;` |
|      70 | 6807 | `	pException->pVm = pGen->pVm;` |
|       - | 6808 | `	/* Create the try block */` |
|      70 | 6809 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      70 | 6810 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6811 | `		return SXERR_ABORT;` |
|       - | 6812 | `	}` |
|       - | 6813 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|      70 | 6814 | `	pTry->pUserData = pException;` |
|       - | 6815 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|      70 | 6816 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 6817 | `	/* Fix the jump later when the destination is resolved */` |
|      70 | 6818 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|      70 | 6819 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 6820 | `	/* Compile the block */` |
|      70 | 6821 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      70 | 6822 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6823 | `		return SXERR_ABORT;` |
|       - | 6824 | `	}` |
|       - | 6825 | `	/* Fix forward jumps now the destination is resolved */` |
|      70 | 6826 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6827 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|      70 | 6828 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 6829 | `	/* Leave the block */` |
|      70 | 6830 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6831 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|      70 | 6832 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      66 | 6833 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 6834 | `		/* Compile one or more catch blocks */` |
|      56 | 6835 | `		for(;;){` |
|     112 | 6836 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      90 | 6837 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      30 | 6838 | `					break;` |
|       - | 6839 | `			}` |
|      58 | 6840 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|      58 | 6841 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6842 | `				return SXERR_ABORT;` |
|       - | 6843 | `			}` |
|       2 | 6844 | `		}` |
|      28 | 6845 | `	}` |
|       - | 6846 | `	/* Compile optional finally block */` |
|      70 | 6847 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      36 | 6848 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 6849 | `		SySet *pInstrContainer;` |
|       - | 6850 | `		GenBlock *pFinBlock;` |
|      28 | 6851 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 6852 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      28 | 6853 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      28 | 6854 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6855 | `			return SXERR_ABORT;` |
|       - | 6856 | `		}` |
|       - | 6857 | `		/* Swap bytecode container */` |
|      28 | 6858 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      28 | 6859 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 6860 | `		/* Compile the finally body */` |
|      28 | 6861 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      28 | 6862 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6863 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 6864 | `			return SXERR_ABORT;` |
|       - | 6865 | `		}` |
|       - | 6866 | `		/* Fix forward jumps now the destination is resolved */` |
|      28 | 6867 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6868 | `		/* Emit DONE to terminate the finally block */` |
|      28 | 6869 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6870 | `		/* Leave the block */` |
|      28 | 6871 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6872 | `		/* Restore the default container */` |
|      28 | 6873 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      28 | 6874 | `		pException->iHasFinally = 1;` |
|      13 | 6875 | `	}` |
|       - | 6876 | `	/* Must have at least one catch or finally */` |
|      70 | 6877 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       3 | 6878 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 6879 | `			"Cannot use try without catch or finally");` |
|       3 | 6880 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6881 | `			return SXERR_ABORT;` |
|       - | 6882 | `		}` |
|       1 | 6883 | `	}` |
|      70 | 6884 | `	return SXRET_OK;` |
|      36 | 6885 |  |
|       - | 6886 | `/*` |
|       - | 6887 | ` * Compile a switch block.` |
|       - | 6888 | ` *  (See block-comment below for more information)` |
|       - | 6889 | ` */` |
|      98 | 6890 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 6891 |  |
|     100 | 6892 | `	sxi32 rc = SXRET_OK;` |
|     100 | 6893 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 6894 | `		/* Unexpected token */` |
|     ! 0 | 6895 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 6896 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6897 | `			return SXERR_ABORT;` |
|       - | 6898 | `		}` |
|     ! 0 | 6899 | `		pGen->pIn++;` |
|     ! 0 | 6900 | `	}` |
|     100 | 6901 | `	pGen->pIn++;` |
|       - | 6902 | `	/* First instruction to execute in this block. */` |
|     100 | 6903 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 6904 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 6905 | `	 * or the '}' token */` |
|     172 | 6906 | `	for(;;){` |
|     346 | 6907 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6908 | `			/* No more input to process */` |
|     ! 0 | 6909 | `			break;` |
|       - | 6910 | `		}` |
|     346 | 6911 | `		rc = SXRET_OK;` |
|     346 | 6912 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      68 | 6913 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      26 | 6914 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 6915 | `					/* Unexpected token */` |
|     ! 0 | 6916 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 6917 | `						&pGen->pIn->sData);` |
|     ! 0 | 6918 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6919 | `						return SXERR_ABORT;` |
|       - | 6920 | `					}` |
|       - | 6921 | `					/* FALL THROUGH */` |
|     ! 0 | 6922 | `				}` |
|      26 | 6923 | `				rc = SXERR_EOF;` |
|      26 | 6924 | `				break;` |
|       - | 6925 | `			}` |
|      23 | 6926 | `		}else{` |
|       - | 6927 | `			sxi32 nKwrd;` |
|       - | 6928 | `			/* Extract the keyword */` |
|     280 | 6929 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     280 | 6930 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      38 | 6931 | `				break;` |
|       - | 6932 | `			}` |
|     208 | 6933 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 6934 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 6935 | `					/* Unexpected token */` |
|     ! 0 | 6936 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 6937 | `						&pGen->pIn->sData);` |
|     ! 0 | 6938 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6939 | `						return SXERR_ABORT;` |
|       - | 6940 | `					}` |
|       - | 6941 | `					/* FALL THROUGH */` |
|     ! 0 | 6942 | `				}` |
|       - | 6943 | `				/* Block compiled */` |
|       3 | 6944 | `				break;` |
|       - | 6945 | `			}` |
|       - | 6946 | `		}` |
|       - | 6947 | `		/* Compile block */` |
|     248 | 6948 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     248 | 6949 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6950 | `			return SXERR_ABORT;` |
|       - | 6951 | `		}` |
|       2 | 6952 | `	}` |
|     100 | 6953 | `	return rc;` |
|      51 | 6954 |  |
|       - | 6955 | `/*` |
|       - | 6956 | ` * Compile a case eXpression.` |
|       - | 6957 | ` *  (See block-comment below for more information)` |
|       - | 6958 | ` */` |
|      80 | 6959 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 6960 |  |
|       - | 6961 | `	SySet *pInstrContainer;` |
|       - | 6962 | `	SyToken *pEnd,*pTmp;` |
|      82 | 6963 | `	sxi32 iNest = 0;` |
|       - | 6964 | `	sxi32 rc;` |
|       - | 6965 | `	/* Delimit the expression */` |
|      82 | 6966 | `	pEnd = pGen->pIn;` |
|     170 | 6967 | `	while( pEnd < pGen->pEnd ){` |
|     170 | 6968 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 6969 | `			/* Increment nesting level */` |
|       3 | 6970 | `			iNest++;` |
|     169 | 6971 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 6972 | `			/* Decrement nesting level */` |
|       3 | 6973 | `			iNest--;` |
|     167 | 6974 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      82 | 6975 | `			break;` |
|       - | 6976 | `		}` |
|      90 | 6977 | `		pEnd++;` |
|       2 | 6978 | `	}` |
|      82 | 6979 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 6980 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 6981 | `		if( rc == SXERR_ABORT ){` |
|       - | 6982 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6983 | `			return SXERR_ABORT;` |
|       - | 6984 | `		}` |
|     ! 0 | 6985 | `	}` |
|       - | 6986 | `	/* Swap token stream */` |
|      82 | 6987 | `	pTmp = pGen->pEnd;` |
|      82 | 6988 | `	pGen->pEnd = pEnd;` |
|      82 | 6989 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      82 | 6990 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      82 | 6991 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 6992 | `	/* Emit the done instruction */` |
|      82 | 6993 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      82 | 6994 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 6995 | `	/* Update token stream */` |
|      82 | 6996 | `	pGen->pIn  = pEnd;` |
|      82 | 6997 | `	pGen->pEnd = pTmp;` |
|      82 | 6998 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6999 | `		return SXERR_ABORT;` |
|       - | 7000 | `	}` |
|      82 | 7001 | `	return SXRET_OK;` |
|      42 | 7002 |  |
|       - | 7003 | `/*` |
|       - | 7004 | ` * Compile the smart switch statement.` |
|       - | 7005 | ` * According to the PHP language reference manual` |
|       - | 7006 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 7007 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 7008 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 7009 | ` *  This is exactly what the switch statement is for.` |
|       - | 7010 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 7011 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 7012 | ` *  of the outer loop, use continue 2.` |
|       - | 7013 | ` *  Note that switch/case does loose comparision.` |
|       - | 7014 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 7015 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 7016 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 7017 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 7018 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 7019 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 7020 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 7021 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 7022 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 7023 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 7024 | ` *  list for the next case.` |
|       - | 7025 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 7026 | ` *  or floating-point numbers and strings.` |
|       - | 7027 | ` */` |
|      26 | 7028 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 7029 |  |
|       - | 7030 | `	GenBlock *pSwitchBlock;` |
|       - | 7031 | `	SyToken *pTmp,*pEnd;` |
|       - | 7032 | `	ph7_switch *pSwitch;` |
|       - | 7033 | `	sxu32 nToken;` |
|       - | 7034 | `	sxu32 nLine;` |
|       - | 7035 | `	sxi32 rc;` |
|      28 | 7036 | `	nLine = pGen->pIn->nLine;` |
|       - | 7037 | `	/* Jump the 'switch' keyword */` |
|      28 | 7038 | `	pGen->pIn++;` |
|      28 | 7039 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 7040 | `		/* Syntax error */` |
|     ! 0 | 7041 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 7042 | `		if( rc == SXERR_ABORT ){` |
|       - | 7043 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7044 | `			return SXERR_ABORT;` |
|       - | 7045 | `		}` |
|     ! 0 | 7046 | `		goto Synchronize;` |
|       - | 7047 | `	}` |
|       - | 7048 | `	/* Jump the left parenthesis '(' */` |
|      28 | 7049 | `	pGen->pIn++;` |
|      28 | 7050 | `	pEnd = 0; /* cc warning */` |
|       - | 7051 | `	/* Create the loop block */` |
|      41 | 7052 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      13 | 7053 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      28 | 7054 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7055 | `		return SXERR_ABORT;` |
|       - | 7056 | `	}` |
|       - | 7057 | `	/* Delimit the condition */` |
|      28 | 7058 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      28 | 7059 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 7060 | `		/* Empty expression */` |
|     ! 0 | 7061 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 7062 | `		if( rc == SXERR_ABORT ){` |
|       - | 7063 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7064 | `			return SXERR_ABORT;` |
|       - | 7065 | `		}` |
|     ! 0 | 7066 | `	}` |
|       - | 7067 | `	/* Swap token streams */` |
|      28 | 7068 | `	pTmp = pGen->pEnd;` |
|      28 | 7069 | `	pGen->pEnd = pEnd;` |
|       - | 7070 | `	/* Compile the expression */` |
|      28 | 7071 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      28 | 7072 | `	if( rc == SXERR_ABORT ){` |
|       - | 7073 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 7074 | `		return SXERR_ABORT;` |
|       - | 7075 | `	}` |
|       - | 7076 | `	/* Update token stream */` |
|      28 | 7077 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 7078 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 7079 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 7080 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7081 | `			return SXERR_ABORT;` |
|       - | 7082 | `		}` |
|     ! 0 | 7083 | `		pGen->pIn++;` |
|     ! 0 | 7084 | `	}` |
|      28 | 7085 | `	pGen->pIn  = &pEnd[1];` |
|      28 | 7086 | `	pGen->pEnd = pTmp;` |
|      28 | 7087 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      26 | 7088 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 7089 | `			pTmp = pGen->pIn;` |
|     ! 0 | 7090 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 7091 | `				pTmp--;` |
|     ! 0 | 7092 | `			}` |
|       - | 7093 | `			/* Unexpected token */` |
|     ! 0 | 7094 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 7095 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7096 | `				return SXERR_ABORT;` |
|       - | 7097 | `			}` |
|     ! 0 | 7098 | `			goto Synchronize;` |
|       - | 7099 | `	}` |
|       - | 7100 | `	/* Set the delimiter token */` |
|      28 | 7101 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 7102 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 7103 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 7104 | `	}else{` |
|      26 | 7105 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 7106 | `	}` |
|      28 | 7107 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 7108 | `	/* Create the switch blocks container */` |
|      28 | 7109 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      28 | 7110 | `	if( pSwitch == 0 ){` |
|       - | 7111 | `		/* Abort compilation */` |
|     ! 0 | 7112 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 7113 | `		return SXERR_ABORT;` |
|       - | 7114 | `	}` |
|       - | 7115 | `	/* Zero the structure */` |
|      28 | 7116 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 7117 | `	/* Initialize fields */` |
|      28 | 7118 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 7119 | `	/* Emit the switch instruction */` |
|      28 | 7120 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 7121 | `	/* Compile case blocks */` |
|      87 | 7122 | `	for(;;){` |
|       - | 7123 | `		sxu32 nKwrd;` |
|     102 | 7124 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7125 | `			/* No more input to process */` |
|     ! 0 | 7126 | `			break;` |
|       - | 7127 | `		}` |
|     102 | 7128 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 7129 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 7130 | `				/* Unexpected token */` |
|     ! 0 | 7131 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7132 | `					&pGen->pIn->sData);` |
|     ! 0 | 7133 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7134 | `					return SXERR_ABORT;` |
|       - | 7135 | `				}` |
|       - | 7136 | `				/* FALL THROUGH */` |
|     ! 0 | 7137 | `			}` |
|       - | 7138 | `			/* Block compiled */` |
|     ! 0 | 7139 | `			break;` |
|       - | 7140 | `		}` |
|       - | 7141 | `		/* Extract the keyword */` |
|     102 | 7142 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     102 | 7143 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 7144 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 7145 | `				/* Unexpected token */` |
|     ! 0 | 7146 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7147 | `					&pGen->pIn->sData);` |
|     ! 0 | 7148 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7149 | `					return SXERR_ABORT;` |
|       - | 7150 | `				}` |
|       - | 7151 | `				/* FALL THROUGH */` |
|     ! 0 | 7152 | `			}` |
|       - | 7153 | `			/* Block compiled */` |
|       3 | 7154 | `			break;` |
|       - | 7155 | `		}` |
|     100 | 7156 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 7157 | `			/*` |
|       - | 7158 | `			 * Accroding to the PHP language reference manual` |
|       - | 7159 | `			 *  A special case is the default case. This case matches anything` |
|       - | 7160 | `			 *  that wasn't matched by the other cases.` |
|       - | 7161 | `			 */` |
|      20 | 7162 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 7163 | `				/* Default case already compiled */` |
|     ! 0 | 7164 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 7165 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7166 | `					return SXERR_ABORT;` |
|       - | 7167 | `				}` |
|     ! 0 | 7168 | `			}` |
|      20 | 7169 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 7170 | `			/* Compile the default block */` |
|      20 | 7171 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      20 | 7172 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7173 | `				return SXERR_ABORT;` |
|      20 | 7174 | `			}else if( rc == SXERR_EOF ){` |
|      18 | 7175 | `				break;` |
|       1 | 7176 | `			}` |
|      83 | 7177 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 7178 | `			ph7_case_expr sCase;` |
|       - | 7179 | `			/* Standard case block */` |
|      82 | 7180 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 7181 | `			/* initialize the structure */` |
|      82 | 7182 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 7183 | `			/* Compile the case expression */` |
|      82 | 7184 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      82 | 7185 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7186 | `				return SXERR_ABORT;` |
|       - | 7187 | `			}` |
|       - | 7188 | `			/* Compile the case block */` |
|      82 | 7189 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 7190 | `			/* Insert in the switch container */` |
|      82 | 7191 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      82 | 7192 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7193 | `				return SXERR_ABORT;` |
|      82 | 7194 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 7195 | `				break;` |
|       - | 7196 | `			}` |
|      38 | 7197 | `		}else{` |
|       - | 7198 | `			/* Unexpected token */` |
|     ! 0 | 7199 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7200 | `				&pGen->pIn->sData);` |
|     ! 0 | 7201 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7202 | `				return SXERR_ABORT;` |
|       - | 7203 | `			}` |
|     ! 0 | 7204 | `			break;` |
|       - | 7205 | `		}` |
|       2 | 7206 | `	}` |
|       - | 7207 | `	/* Fix all jumps now the destination is resolved */` |
|      28 | 7208 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 7209 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7210 | `	/* Release the loop block */` |
|      28 | 7211 | `	GenStateLeaveBlock(pGen,0);` |
|      28 | 7212 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 7213 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      28 | 7214 | `		pGen->pIn++;` |
|      13 | 7215 | `	}` |
|       - | 7216 | `	/* Statement successfully compiled */` |
|      28 | 7217 | `	return SXRET_OK;` |
|     ! 0 | 7218 | `Synchronize:` |
|       - | 7219 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 7220 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 7221 | `		pGen->pIn++;` |
|     ! 0 | 7222 | `	}` |
|     ! 0 | 7223 | `	return SXRET_OK;` |
|      15 | 7224 |  |
|       - | 7225 | `/*` |
|       - | 7226 | ` * Generate bytecode for a given expression tree.` |
|       - | 7227 | ` * If something goes wrong while generating bytecode` |
|       - | 7228 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 7229 | ` * this function takes care of generating the appropriate` |
|       - | 7230 | ` * error message.` |
|       - | 7231 | ` */` |
| 2317664 | 7232 | `static sxi32 GenStateEmitExprCode(` |
|       - | 7233 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7234 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 7235 | `	sxi32 iFlags /* Control flags */` |
|       - | 7236 | `	)` |
|       2 | 7237 |  |
|       - | 7238 | `	VmInstr *pInstr;` |
|       - | 7239 | `	sxu32 nJmpIdx;` |
| 2317666 | 7240 | `	sxi32 iP1 = 0;` |
| 2317666 | 7241 | `	sxu32 iP2 = 0;` |
| 2317666 | 7242 | `	void *p3  = 0;` |
|       - | 7243 | `	sxi32 iVmOp;` |
|       - | 7244 | `	sxi32 rc;` |
| 2317666 | 7245 | `	if( pNode->xCode ){` |
|       - | 7246 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 7247 | `		/* Compile node */` |
| 1436708 | 7248 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1436708 | 7249 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1436708 | 7250 | `		RE_SWAP_DELIMITER(pGen);` |
| 1436708 | 7251 | `		return rc;` |
|       - | 7252 | `	}` |
|  880960 | 7253 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 7254 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 7255 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 7256 | `		return SXERR_ABORT;` |
|       - | 7257 | `	}` |
|  880960 | 7258 | `	iVmOp = pNode->pOp->iVmOp;` |
|  880960 | 7259 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 7260 | `		sxu32 nJz,nJmp;` |
|       - | 7261 | `		/* Ternary operator require special handling */` |
|       - | 7262 | `		/* Phase#1: Compile the condition */` |
|    1866 | 7263 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1866 | 7264 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7265 | `			return rc;` |
|       - | 7266 | `		}` |
|    1866 | 7267 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1866 | 7268 | `		if( pNode->pLeft ){` |
|       - | 7269 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 7270 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1798 | 7271 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7272 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1798 | 7273 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1798 | 7274 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7275 | `				return rc;` |
|       - | 7276 | `			}` |
|     900 | 7277 | `		}else{` |
|       - | 7278 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 7279 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 7280 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 7281 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 7282 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7283 | `		}` |
|       - | 7284 | `		/* Phase#4: Emit the unconditional jump */` |
|    1866 | 7285 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 7286 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1866 | 7287 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1866 | 7288 | `		if( pInstr ){` |
|    1866 | 7289 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     932 | 7290 | `		}` |
|    1866 | 7291 | `		if( !pNode->pLeft ){` |
|       - | 7292 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 7293 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 7294 | `		}` |
|       - | 7295 | `		/* Phase#6: Compile the 'else' expression */` |
|    1866 | 7296 | `		if( pNode->pRight ){` |
|    1866 | 7297 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1866 | 7298 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7299 | `				return rc;` |
|       - | 7300 | `			}` |
|     932 | 7301 | `		}` |
|    1866 | 7302 | `		if( nJmp > 0 ){` |
|       - | 7303 | `			/* Phase#7: Fix the unconditional jump */` |
|    1866 | 7304 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1866 | 7305 | `			if( pInstr ){` |
|    1866 | 7306 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     932 | 7307 | `			}` |
|     932 | 7308 | `		}` |
|       - | 7309 | `		/* All done */` |
|    1866 | 7310 | `		return SXRET_OK;` |
|       - | 7311 | `	}` |
|       - | 7312 | `	/* Generate code for the left tree */` |
|  879096 | 7313 | `	if( pNode->pLeft ){` |
|  879076 | 7314 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 7315 | `			ph7_expr_node **apNode;` |
|  295318 | 7316 | `			int hasSpread = 0;` |
|       - | 7317 | `			sxi32 n;` |
|       - | 7318 | `			/* Recurse and generate bytecodes for function arguments */` |
|  295318 | 7319 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 7320 | `			/* Read-only load */` |
|  295318 | 7321 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  590194 | 7322 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  294878 | 7323 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  294878 | 7324 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7325 | `					return rc;` |
|       - | 7326 | `				}` |
|  294878 | 7327 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 7328 | `					/* Emit spread opcode to unpack this array argument */` |
|      15 | 7329 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      15 | 7330 | `					hasSpread = 1;` |
|       7 | 7331 | `				}` |
|  147440 | 7332 | `			}` |
|       - | 7333 | `			/* Total number of given arguments */` |
|  295318 | 7334 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|  295318 | 7335 | `			iP2 = hasSpread;` |
|       - | 7336 | `			/* Remove stale flags now */` |
|  295318 | 7337 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  147658 | 7338 | `		}` |
|  879076 | 7339 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  879076 | 7340 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7341 | `			return rc;` |
|       - | 7342 | `		}` |
|  879076 | 7343 | `		if( iVmOp == PH7_OP_CALL ){` |
|  295318 | 7344 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  295318 | 7345 | `			if( pInstr ){` |
|  295318 | 7346 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  294804 | 7347 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 7348 | `					sxu32 nQual;` |
|       - | 7349 | `					/* Prevent constant expansion */` |
|  294804 | 7350 | `					pInstr->iP1 = 0;` |
|       - | 7351 | `					/* Namespace-qualify the function name for CALL */` |
|  294804 | 7352 | `					nQual = GenStateNsQualifyName(pGen,nOrig);` |
|  294804 | 7353 | `					pInstr->iP2 = (sxi32)nQual;` |
|  294804 | 7354 | `					if( nQual != nOrig ){` |
|       - | 7355 | `						/* Name was compiler-qualified: flag CALL for host-function global fallback.` |
|       - | 7356 | `						 * p3 = (void*)1 tells the VM it's safe to strip the NS prefix` |
|       - | 7357 | `						 * and try the short name in hHostFunction. */` |
|      50 | 7358 | `						p3 = (void *)1;` |
|      26 | 7359 | `					}` |
|  147917 | 7360 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 7361 | `					/* Method call,flag that */` |
|     494 | 7362 | `					pInstr->iP2 = 1;` |
|     246 | 7363 | `				}` |
|  147660 | 7364 | `			}` |
|  731418 | 7365 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 7366 | `			ph7_expr_node **apNode;` |
|       - | 7367 | `			sxi32 n;` |
|       - | 7368 | `			/* Recurse and generate bytecodes for array index */` |
|   66160 | 7369 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  119380 | 7370 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   53222 | 7371 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   53222 | 7372 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7373 | `					return rc;` |
|       - | 7374 | `				}` |
|   26612 | 7375 | `			}` |
|   66160 | 7376 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   53222 | 7377 | `				iP1 = 1; /* Node have an index associated with it */` |
|   26610 | 7378 | `			}` |
|   66160 | 7379 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 7380 | `				/* Create an empty entry when the desired index is not found */` |
|   26120 | 7381 | `				iP2 = 1;` |
|   13061 | 7382 | `			}` |
|  550681 | 7383 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 7384 | `			/* POP the left node */` |
|      32 | 7385 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 7386 | `		}` |
|  439537 | 7387 | `	}` |
|  879096 | 7388 | `	rc = SXRET_OK;` |
|  879096 | 7389 | `	nJmpIdx = 0;` |
|       - | 7390 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 7391 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 7392 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  879096 | 7393 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     172 | 7394 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     172 | 7395 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     172 | 7396 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     172 | 7397 | `			int isSpecial = 0;` |
|     172 | 7398 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     120 | 7399 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     120 | 7400 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     126 | 7401 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     111 | 7402 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      56 | 7403 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      50 | 7404 | `					isSpecial = 1;` |
|      24 | 7405 | `				}` |
|      72 | 7406 | `			}` |
|     198 | 7407 | `			pInstr->iP1 = 0;` |
|     198 | 7408 | `			if( !isSpecial ){` |
|      98 | 7409 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2);` |
|      48 | 7410 | `			}` |
|       - | 7411 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 7412 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     146 | 7413 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|      98 | 7414 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|      98 | 7415 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 7416 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 | 7417 | `					return SXRET_OK;` |
|       - | 7418 | `				}` |
|      27 | 7419 | `			}` |
|      51 | 7420 | `		}` |
|      91 | 7421 | `	}` |
|       - | 7422 | `	/* Generate code for the right tree */` |
|  879036 | 7423 | `	if( pNode->pRight ){` |
|  459084 | 7424 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 7425 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    8146 | 7426 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  455012 | 7427 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 7428 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2722 | 7429 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  449580 | 7430 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 7431 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      32 | 7432 | `			iVmOp = 0; /* No binary operator to emit */` |
|      32 | 7433 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  448205 | 7434 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  200508 | 7435 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  100253 | 7436 | `		}` |
|  459084 | 7437 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  459084 | 7438 | `		if( iVmOp == PH7_OP_STORE ){` |
|  197766 | 7439 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  197766 | 7440 | `			if( pInstr ){` |
|  197766 | 7441 | `				if( pInstr->iOp == PH7_OP_LOAD_LIST ){` |
|       - | 7442 | `					/* Hide the STORE instruction */` |
|      26 | 7443 | `					iVmOp = 0;` |
|  197754 | 7444 | `				}else if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 7445 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   43880 | 7446 | `					iP2 = 1;` |
|   21941 | 7447 | `				}else{` |
|  153864 | 7448 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7449 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   26082 | 7450 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   26082 | 7451 | `						iP1 = pInstr->iP1;` |
|   13042 | 7452 | `					}else{` |
|  127784 | 7453 | `						p3 = pInstr->p3;` |
|       - | 7454 | `					}` |
|       - | 7455 | `					/* POP the last dynamic load instruction */` |
|  153864 | 7456 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 7457 | `				}` |
|   98884 | 7458 | `			}` |
|  360202 | 7459 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      46 | 7460 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      46 | 7461 | `			if( pInstr ){` |
|      46 | 7462 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7463 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 7464 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 7465 | `					 */` |
|      15 | 7466 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 7467 | `					iP1 = pInstr->iP1;` |
|      15 | 7468 | `					iP2 = pInstr->iP2;` |
|      15 | 7469 | `					p3  = pInstr->p3;` |
|       8 | 7470 | `				}else{` |
|      32 | 7471 | `					p3 = pInstr->p3;` |
|       - | 7472 | `				}` |
|      22 | 7473 | `			}` |
|      22 | 7474 | `		}` |
|  229541 | 7475 | `	}` |
|  879036 | 7476 | `	if( iVmOp > 0 ){` |
|  878952 | 7477 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   10538 | 7478 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 7479 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    7736 | 7480 | `				iP1 = 1;` |
|    3869 | 7481 | `			}` |
|  873684 | 7482 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 7483 | `			/* Namespace-qualify the class name for NEW */ {` |
|   13264 | 7484 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   13264 | 7485 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   13248 | 7486 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    6623 | 7487 | `				}` |
|   13264 | 7488 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 7489 | `					/* Prevent constant expansion for class name */` |
|   13262 | 7490 | `					pPeek->iP1 = 0;` |
|   13262 | 7491 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pPeek->iP2);` |
|    6630 | 7492 | `				}` |
|       - | 7493 | `			}` |
|   13264 | 7494 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   13264 | 7495 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 7496 | `				VmInstr *pPrev;` |
|   13248 | 7497 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   13248 | 7498 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 7499 | `					/* Pop the call instruction */` |
|   13248 | 7500 | `					iP1 = pInstr->iP1;` |
|   13248 | 7501 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    6623 | 7502 | `				}` |
|    6625 | 7503 | `			}` |
|  861785 | 7504 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 7505 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 7506 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 7507 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 7508 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 7509 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 7510 | `				int isSpecialIs = 0;` |
|      50 | 7511 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 7512 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 7513 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 7514 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 7515 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 7516 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 7517 | `						isSpecialIs = 1;` |
|       5 | 7518 | `					}` |
|      23 | 7519 | `				}` |
|      52 | 7520 | `				pInstr->iP1 = 0;` |
|      52 | 7521 | `				if( !isSpecialIs ){` |
|      38 | 7522 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2);` |
|      18 | 7523 | `				}` |
|      25 | 7524 | `			}` |
|  855133 | 7525 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 7526 | `			/* Prevent constant expansion for member/property names.` |
|       - | 7527 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 7528 | `			 * should not trigger constant lookup. */` |
|   98754 | 7529 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   98754 | 7530 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   98738 | 7531 | `				pInstr->iP1 = 0;` |
|   49368 | 7532 | `			}` |
|   98754 | 7533 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 7534 | `				/* Static member access,remember that */` |
|     112 | 7535 | `				iP1 = 1;` |
|     112 | 7536 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     112 | 7537 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      10 | 7538 | `					p3 = pInstr->p3;` |
|      10 | 7539 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       4 | 7540 | `				}` |
|      55 | 7541 | `			}` |
|   49376 | 7542 | `		}` |
|       - | 7543 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  878950 | 7544 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  439474 | 7545 | `	}` |
|  879034 | 7546 | `	if( nJmpIdx > 0 ){` |
|       - | 7547 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   10896 | 7548 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   10896 | 7549 | `		if( pInstr ){` |
|   10896 | 7550 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5447 | 7551 | `		}` |
|    5447 | 7552 | `	}` |
|  879034 | 7553 | `	return rc;` |
| 1158824 | 7554 |  |
|       - | 7555 | `/*` |
|       - | 7556 | ` * Compile a PHP expression.` |
|       - | 7557 | ` * According to the PHP language reference manual:` |
|       - | 7558 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 7559 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 7560 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 7561 | ` *  is "anything that has a value".` |
|       - | 7562 | ` * If something goes wrong while compiling the expression,this` |
|       - | 7563 | ` * function takes care of generating the appropriate error` |
|       - | 7564 | ` * message.` |
|       - | 7565 | ` */` |
|  626034 | 7566 | `static sxi32 PH7_CompileExpr(` |
|       - | 7567 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7568 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 7569 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 7570 | `	)` |
|       2 | 7571 |  |
|       - | 7572 | `	ph7_expr_node *pRoot;` |
|       - | 7573 | `	SySet sExprNode;` |
|       - | 7574 | `	SyToken *pEnd;` |
|       - | 7575 | `	sxi32 nExpr;` |
|       - | 7576 | `	sxi32 iNest;` |
|       - | 7577 | `	sxi32 rc;` |
|       - | 7578 | `	/* Initialize worker variables */` |
|  626036 | 7579 | `	nExpr = 0;` |
|  626036 | 7580 | `	pRoot = 0;` |
|  626036 | 7581 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  626036 | 7582 | `	SySetAlloc(&sExprNode,0x10);` |
|  626036 | 7583 | `	rc = SXRET_OK;` |
|       - | 7584 | `	/* Delimit the expression */` |
|  626036 | 7585 | `	pEnd = pGen->pIn;` |
|  626036 | 7586 | `	iNest = 0;` |
| 4220600 | 7587 | `	while( pEnd < pGen->pEnd ){` |
| 4002260 | 7588 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7589 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     230 | 7590 | `			iNest++;` |
| 4002146 | 7591 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     238 | 7592 | `			iNest--;` |
| 4001914 | 7593 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  407896 | 7594 | `			if( iNest <= 0 ){` |
|  407696 | 7595 | `				break;` |
|       - | 7596 | `			}` |
|     100 | 7597 | `		}` |
| 3594566 | 7598 | `		pEnd++;` |
|       2 | 7599 | `	}` |
|  626036 | 7600 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   10476 | 7601 | `		SyToken *pEnd2 = pGen->pIn;` |
|   10476 | 7602 | `		iNest = 0;` |
|       - | 7603 | `		/* Stop at the first comma */` |
|   20974 | 7604 | `		while( pEnd2 < pEnd ){` |
|   10500 | 7605 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       6 | 7606 | `				iNest++;` |
|   10498 | 7607 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       6 | 7608 | `				iNest--;` |
|   10494 | 7609 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 7610 | `				if( iNest <= 0 ){` |
|     ! 0 | 7611 | `					break;` |
|       - | 7612 | `				}` |
|       2 | 7613 | `			}` |
|   10500 | 7614 | `			pEnd2++;` |
|       2 | 7615 | `		}` |
|   10476 | 7616 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 7617 | `			pEnd = pEnd2;` |
|     ! 0 | 7618 | `		}` |
|    5237 | 7619 | `	}` |
|  626036 | 7620 | `	if( pEnd > pGen->pIn ){` |
|  626026 | 7621 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 7622 | `		/* Swap delimiter */` |
|  626026 | 7623 | `		pGen->pEnd = pEnd;` |
|       - | 7624 | `		/* Try to get an expression tree */` |
|  626026 | 7625 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  626026 | 7626 | `		if( rc == SXRET_OK && pRoot ){` |
|  625870 | 7627 | `			rc = SXRET_OK;` |
|  625870 | 7628 | `			if( xTreeValidator ){` |
|       - | 7629 | `				/* Call the upper layer validator callback */` |
|   13404 | 7630 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    6701 | 7631 | `			}` |
|  625870 | 7632 | `			if( rc != SXERR_ABORT ){` |
|       - | 7633 | `				/* Generate code for the given tree */` |
|  625870 | 7634 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  312934 | 7635 | `			}` |
|  625870 | 7636 | `			nExpr = 1;` |
|  312934 | 7637 | `		}` |
|       - | 7638 | `		/* Release the whole tree */` |
|  626026 | 7639 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 7640 | `		/* Synchronize token stream */` |
|  626026 | 7641 | `		pGen->pEnd = pTmp;` |
|  626026 | 7642 | `		pGen->pIn  = pEnd;` |
|  626026 | 7643 | `		if( rc == SXERR_ABORT ){` |
|       3 | 7644 | `			SySetRelease(&sExprNode);` |
|       3 | 7645 | `			return SXERR_ABORT;` |
|       - | 7646 | `		}` |
|  313011 | 7647 | `	}` |
|  626034 | 7648 | `	SySetRelease(&sExprNode);` |
|  626034 | 7649 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  313019 | 7650 |  |
|       - | 7651 | `/*` |
|       - | 7652 | ` * Return a pointer to the node construct handler associated` |
|       - | 7653 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 7654 | ` */` |
|  155854 | 7655 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 7656 |  |
|  155856 | 7657 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 7658 | `		/* Numeric literal: Either real or integer */` |
|   85148 | 7659 | `		return PH7_CompileNumLiteral;` |
|   70710 | 7660 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 7661 | `		/* Double quoted string */` |
|   14876 | 7662 | `		return PH7_CompileString;` |
|   55836 | 7663 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 7664 | `		/* Single quoted string */` |
|   55776 | 7665 | `		return PH7_CompileSimpleString;` |
|      62 | 7666 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 7667 | `		/* Heredoc */` |
|      28 | 7668 | `		return PH7_CompileHereDoc;` |
|      36 | 7669 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 7670 | `		/* Nowdoc */` |
|      29 | 7671 | `		return PH7_CompileNowDoc;` |
|       7 | 7672 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 7673 | `		/* Backtick quoted string */` |
|       5 | 7674 | `		return PH7_CompileBacktic;` |
|       - | 7675 | `	}` |
|       3 | 7676 | `	return 0;` |
|   77929 | 7677 |  |
|       - | 7678 | `/*` |
|       - | 7679 | ` * Compile an unset() statement.` |
|       - | 7680 | ` * unset($var, $arr[$key], ...);` |
|       - | 7681 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 7682 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 7683 | ` * parent array before extracting the element to unset.` |
|       - | 7684 | ` */` |
|    2548 | 7685 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 7686 |  |
|    2550 | 7687 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2550 | 7688 | `	sxu32 nIdx = 0;` |
|       - | 7689 | `	SyString sName;` |
|       - | 7690 | `	sxi32 rc;` |
|       - | 7691 | `	/* Jump the 'unset' keyword */` |
|    2550 | 7692 | `	pGen->pIn++;` |
|       - | 7693 | `	/* Save delimiter */` |
|    2550 | 7694 | `	pTmp = pGen->pEnd;` |
|       - | 7695 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2550 | 7696 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2550 | 7697 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 7698 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 7699 | `		SyToken *pClose;` |
|    2550 | 7700 | `		pGen->pIn++;   /* Skip '(' */` |
|    2550 | 7701 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2550 | 7702 | `		pEnd = pClose; /* Stop at ')' */` |
|    1274 | 7703 | `	}` |
|    2550 | 7704 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 7705 | `	/* Resolve the 'unset' builtin name once */` |
|    2550 | 7706 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     300 | 7707 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     300 | 7708 | `		if( pObj == 0 ){` |
|     ! 0 | 7709 | `			return SXERR_ABORT;` |
|       - | 7710 | `		}` |
|     300 | 7711 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     300 | 7712 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     149 | 7713 | `	}` |
|       - | 7714 | `	/* Compile each comma-separated argument */` |
|    8480 | 7715 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    5932 | 7716 | `		if( pGen->pIn < pNext ){` |
|    5932 | 7717 | `			pGen->pEnd = pNext;` |
|    5932 | 7718 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 7719 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,0);` |
|    5932 | 7720 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7721 | `				return SXERR_ABORT;` |
|       - | 7722 | `			}` |
|    5932 | 7723 | `			if( rc != SXERR_EMPTY ){` |
|       - | 7724 | `				/* Emit call for this single argument */` |
|    5930 | 7725 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5930 | 7726 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    5930 | 7727 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    2964 | 7728 | `			}` |
|    2965 | 7729 | `		}` |
|       - | 7730 | `		/* Jump trailing commas */` |
|    9314 | 7731 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3384 | 7732 | `			pNext++;` |
|       2 | 7733 | `		}` |
|    5932 | 7734 | `		pGen->pIn = pNext;` |
|       2 | 7735 | `	}` |
|       - | 7736 | `	/* Skip past the closing ')' if present */` |
|    2550 | 7737 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2550 | 7738 | `		pGen->pIn++;` |
|    1274 | 7739 | `	}` |
|       - | 7740 | `	/* Restore token stream */` |
|    2550 | 7741 | `	pGen->pEnd = pTmp;` |
|    2550 | 7742 | `	return SXRET_OK;` |
|    1276 | 7743 |  |
|       - | 7744 | `/*` |
|       - | 7745 | ` * PHP Language construct table.` |
|       - | 7746 | ` */` |
|       - | 7747 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 7748 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 7749 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 7750 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 7751 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 7752 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 7753 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 7754 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 7755 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 7756 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 7757 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 7758 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 7759 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 7760 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 7761 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 7762 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 7763 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 7764 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 7765 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 7766 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 7767 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 7768 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 7769 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 7770 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 7771 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 7772 | `};` |
|       - | 7773 | `/*` |
|       - | 7774 | ` * Return a pointer to the statement handler routine associated` |
|       - | 7775 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 7776 | ` */` |
|  374460 | 7777 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 7778 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 7779 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 7780 | `	)` |
|       2 | 7781 |  |
|  374462 | 7782 | `	sxu32 n = 0;` |
| 1530768 | 7783 | `	for(;;){` |
| 3061538 | 7784 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   39276 | 7785 | `			break;` |
|       - | 7786 | `		}` |
| 3022264 | 7787 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  335188 | 7788 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 7789 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 7790 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 7791 | `					/* 'static' (class context),return null */` |
|     ! 0 | 7792 | `					return 0;` |
|       - | 7793 | `				}` |
|     ! 0 | 7794 | `			}` |
|       - | 7795 | `			/* Return a pointer to the handler.` |
|       - | 7796 | `			*/` |
|  335188 | 7797 | `			return aLangConstruct[n].xConstruct;` |
|       - | 7798 | `		}` |
| 2687078 | 7799 | `		n++;` |
|       2 | 7800 | `	}` |
|   39276 | 7801 | `	if( pLookahed ){` |
|   39276 | 7802 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    7766 | 7803 | `			return PH7_CompileClassInterface;` |
|   31512 | 7804 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   31308 | 7805 | `			return PH7_CompileClass;` |
|     206 | 7806 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      54 | 7807 | `			return PH7_CompileTrait;` |
|     152 | 7808 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      19 | 7809 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      18 | 7810 | `				return PH7_CompileAbstractClass;` |
|     136 | 7811 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 7812 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 7813 | `				return PH7_CompileFinalClass;` |
|       - | 7814 | `		}` |
|      67 | 7815 | `	}` |
|       - | 7816 | `	/* Not a language construct */` |
|     136 | 7817 | `	return 0;` |
|  187232 | 7818 |  |
|       - | 7819 | `/*` |
|       - | 7820 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 7821 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 7822 | ` */` |
|     134 | 7823 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 7824 |  |
|       - | 7825 | `	int rc;` |
|     136 | 7826 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     136 | 7827 | `	if( rc == FALSE ){` |
|      40 | 7828 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      38 | 7829 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 7830 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 7831 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 7832 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 7833 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 7834 | `			*/` |
|       - | 7835 | `			){` |
|      34 | 7836 | `				rc = TRUE;` |
|      16 | 7837 | `		}` |
|      20 | 7838 | `	}` |
|     136 | 7839 | `	return rc;` |
|       2 | 7840 |  |
|       - | 7841 | `/*` |
|       - | 7842 | ` * Compile a PHP chunk.` |
|       - | 7843 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 7844 | ` * takes care of generating the appropriate error message.` |
|       - | 7845 | ` */` |
|  510078 | 7846 | `static sxi32 GenStateCompileChunk(` |
|       - | 7847 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7848 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 7849 | `	)` |
|       2 | 7850 |  |
|       - | 7851 | `	ProcLangConstruct xCons;` |
|       - | 7852 | `	sxi32 rc;` |
|  510080 | 7853 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  302126 | 7854 | `	for(;;){` |
|  604254 | 7855 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7856 | `			/* No more input to process */` |
|   11216 | 7857 | `			break;` |
|       - | 7858 | `		}` |
|  593040 | 7859 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7860 | `			/* Compile block */` |
|      12 | 7861 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 7862 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7863 | `				break;` |
|       - | 7864 | `			}` |
|       7 | 7865 | `		}else{` |
|  593030 | 7866 | `			xCons = 0;` |
|  593030 | 7867 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  374462 | 7868 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 7869 | `				/* Try to extract a language construct handler */` |
|  374462 | 7870 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  374462 | 7871 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 7872 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7873 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 7874 | `						&pGen->pIn->sData);` |
|       9 | 7875 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7876 | `						break;` |
|       - | 7877 | `					}` |
|       - | 7878 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 7879 | `					 * this erroneous statement.` |
|       - | 7880 | `					 */` |
|       9 | 7881 | `					xCons = PH7_ErrorRecover;` |
|       4 | 7882 | `				}` |
|  405800 | 7883 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   38292 | 7884 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 7885 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 7886 | `				xCons = PH7_CompileLabel;` |
|      56 | 7887 | `			}` |
|  593030 | 7888 | `			if( xCons == 0 ){` |
|       - | 7889 | `				/* Assume an expression an try to compile it */` |
|  218584 | 7890 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  218584 | 7891 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 7892 | `					/* Pop l-value */` |
|  218460 | 7893 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  109229 | 7894 | `				}` |
|  109293 | 7895 | `			}else{` |
|       - | 7896 | `				/* Go compile the sucker */` |
|  374448 | 7897 | `				rc = xCons(&(*pGen));` |
|       - | 7898 | `			}` |
|  593030 | 7899 | `			if( rc == SXERR_ABORT ){` |
|       - | 7900 | `				/* Request to abort compilation */` |
|       3 | 7901 | `				break;` |
|       - | 7902 | `			}` |
|       - | 7903 | `		}` |
|       - | 7904 | `		/* Ignore trailing semi-colons ';' */` |
|  985580 | 7905 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  392544 | 7906 | `			pGen->pIn++;` |
|       2 | 7907 | `		}` |
|  593038 | 7908 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 7909 | `			/* Compile a single statement and return */` |
|  498864 | 7910 | `			break;` |
|       - | 7911 | `		}` |
|       - | 7912 | `		/* LOOP ONE */` |
|       - | 7913 | `		/* LOOP TWO */` |
|       - | 7914 | `		/* LOOP THREE */` |
|       - | 7915 | `		/* LOOP FOUR */` |
|       2 | 7916 | `	}` |
|       - | 7917 | `	/* Return compilation status */` |
|  510080 | 7918 | `	return rc;` |
|       2 | 7919 |  |
|       - | 7920 | `/*` |
|       - | 7921 | ` * Compile a Raw PHP chunk.` |
|       - | 7922 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 7923 | ` * takes care of generating the appropriate error message.` |
|       - | 7924 | ` */` |
|   11218 | 7925 | `static sxi32 PH7_CompilePHP(` |
|       - | 7926 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7927 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 7928 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 7929 | `	)` |
|       2 | 7930 |  |
|   11220 | 7931 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 7932 | `	sxi32 rc;` |
|       - | 7933 | `	/* Reset the token set */` |
|   11220 | 7934 | `	SySetReset(&(*pTokenSet));` |
|       - | 7935 | `	/* Mark as the default token set */` |
|   11220 | 7936 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 7937 | `	/* Advance the stream cursor */` |
|   11220 | 7938 | `	pGen->pRawIn++;` |
|       - | 7939 | `	/* Tokenize the PHP chunk first */` |
|   11220 | 7940 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 7941 | `	/* Point to the head and tail of the token stream. */` |
|   11220 | 7942 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   11220 | 7943 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   11220 | 7944 | `	if( is_expr ){` |
|     ! 0 | 7945 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 7946 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 7947 | `			/* A simple expression,compile it */` |
|     ! 0 | 7948 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 7949 | `		}` |
|       - | 7950 | `		/* Emit the DONE instruction */` |
|     ! 0 | 7951 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 7952 | `		return SXRET_OK;` |
|       - | 7953 | `	}` |
|   11220 | 7954 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 7955 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 7956 | `		/*` |
|       - | 7957 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 7958 | `		 * According to the PHP reference manual:` |
|       - | 7959 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 7960 | `		 *  immediately follow` |
|       - | 7961 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 7962 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 7963 | `		 * Symisc extension:` |
|       - | 7964 | `		 *   This short syntax works with all PHP opening` |
|       - | 7965 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 7966 | `		 *   only short tag.` |
|       - | 7967 | `		 */` |
|       - | 7968 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 7969 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 7970 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 7971 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 7972 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 7973 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 7974 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 7975 | `		}` |
|       3 | 7976 | `		return SXRET_OK;` |
|       - | 7977 | `	}` |
|       - | 7978 | `	/* Compile the PHP chunk */` |
|   11218 | 7979 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 7980 | `	/* Fix exceptions jumps */` |
|   11218 | 7981 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7982 | `	/* Fix gotos now, the jump destination is resolved */` |
|   11218 | 7983 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 7984 | `		rc = SXERR_ABORT;` |
|       1 | 7985 | `	}` |
|       - | 7986 | `	/* Reset container */` |
|   11218 | 7987 | `	SySetReset(&pGen->aGoto);` |
|   11218 | 7988 | `	SySetReset(&pGen->aLabel);` |
|       - | 7989 | `	/* Compilation result */` |
|   11218 | 7990 | `	return rc;` |
|    5611 | 7991 |  |
|       - | 7992 | `/*` |
|       - | 7993 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 7994 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 7995 | ` * This is the only compile interface exported from this file.` |
|       - | 7996 | ` */` |
|   13186 | 7997 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 7998 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 7999 | `	SyString *pScript,  /* Script to compile */` |
|       - | 8000 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 8001 | `	)` |
|       2 | 8002 |  |
|       - | 8003 | `	SySet aPhpToken,aRawToken;` |
|       - | 8004 | `	ph7_gen_state *pCodeGen;` |
|       - | 8005 | `	ph7_value *pRawObj;` |
|       - | 8006 | `	sxu32 nObjIdx;` |
|       - | 8007 | `	sxi32 nRawObj;` |
|       - | 8008 | `	int is_expr;` |
|       - | 8009 | `	sxi32 rc;` |
|   13188 | 8010 | `	if( pScript->nByte < 1 ){` |
|       - | 8011 | `		/* Nothing to compile */` |
|     ! 0 | 8012 | `		return PH7_OK;` |
|       - | 8013 | `	}` |
|       - | 8014 | `	/* Initialize the tokens containers */` |
|   13188 | 8015 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13188 | 8016 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13188 | 8017 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   13188 | 8018 | `	is_expr = 0;` |
|   13188 | 8019 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 8020 | `		SyToken sTmp;` |
|       - | 8021 | `		/* PHP only: -*/` |
|    2606 | 8022 | `		sTmp.nLine = 1;` |
|    2606 | 8023 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2606 | 8024 | `		sTmp.pUserData = 0;` |
|    2606 | 8025 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2606 | 8026 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2606 | 8027 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 8028 | `			/* A simple PHP expression */` |
|     ! 0 | 8029 | `			is_expr = 1;` |
|     ! 0 | 8030 | `		}` |
|    1304 | 8031 | `	}else{` |
|       - | 8032 | `		/* Tokenize raw text */` |
|   10584 | 8033 | `		SySetAlloc(&aRawToken,32);` |
|   10584 | 8034 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 8035 | `	}` |
|   13188 | 8036 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 8037 | `	/* Process high-level tokens */` |
|   13188 | 8038 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   13188 | 8039 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   13188 | 8040 | `	rc = PH7_OK;` |
|   13188 | 8041 | `	if( is_expr ){` |
|       - | 8042 | `		/* Compile the expression */` |
|     ! 0 | 8043 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 8044 | `		goto cleanup;` |
|       - | 8045 | `	}` |
|   13188 | 8046 | `	nObjIdx = 0;` |
|       - | 8047 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 8048 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 8049 | `	 * preventing namespace bleeding across include()d files. */` |
|   13188 | 8050 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 8051 | `	/* Start the compilation process */` |
|   11888 | 8052 | `	for(;;){` |
|   34992 | 8053 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   13184 | 8054 | `			break; /* No more tokens to process */` |
|       - | 8055 | `		}` |
|   21810 | 8056 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 8057 | `			/* Compile the PHP chunk */` |
|   11220 | 8058 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   11220 | 8059 | `			if( rc == SXERR_ABORT ){` |
|       5 | 8060 | `				break;` |
|       - | 8061 | `			}` |
|   11216 | 8062 | `			continue;` |
|       - | 8063 | `		}` |
|       - | 8064 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   10592 | 8065 | `		nRawObj = 0;` |
|   21182 | 8066 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 8067 | `			/* Consume the raw chunk without any processing */` |
|   10592 | 8068 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   10592 | 8069 | `			if( pRawObj == 0 ){` |
|     ! 0 | 8070 | `				rc = SXERR_MEM;` |
|     ! 0 | 8071 | `				break;` |
|       - | 8072 | `			}` |
|       - | 8073 | `			/* Mark as constant and emit the load constant instruction */` |
|   10592 | 8074 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   10592 | 8075 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   10592 | 8076 | `			++nRawObj;` |
|   10592 | 8077 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 8078 | `		}` |
|   10592 | 8079 | `		if( nRawObj > 0 ){` |
|       - | 8080 | `			/* Emit the consume instruction */` |
|   10592 | 8081 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5295 | 8082 | `		}` |
|    6595 | 8083 | `	}` |
|    6593 | 8084 | `cleanup:` |
|   13188 | 8085 | `	SySetRelease(&aRawToken);` |
|   13188 | 8086 | `	SySetRelease(&aPhpToken);` |
|   13188 | 8087 | `	return rc;` |
|    6595 | 8088 |  |
|       - | 8089 | `/*` |
|       - | 8090 | ` * Utility routines.Initialize the code generator.` |
|       - | 8091 | ` */` |
|    2576 | 8092 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 8093 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8094 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8095 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8096 | `	)` |
|       2 | 8097 |  |
|    2578 | 8098 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8099 | `	/* Zero the structure */` |
|    2578 | 8100 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 8101 | `	/* Initial state */` |
|    2578 | 8102 | `	pGen->pVm  = &(*pVm);` |
|    2578 | 8103 | `	pGen->xErr = xErr;` |
|    2578 | 8104 | `	pGen->pErrData = pErrData;` |
|    2578 | 8105 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2578 | 8106 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2578 | 8107 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2578 | 8108 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 8109 | `	/* Error log buffer */` |
|    2578 | 8110 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 8111 | `	/* General purpose working buffer */` |
|    2578 | 8112 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 8113 | `	/* Namespace state */` |
|    2578 | 8114 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2578 | 8115 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 8116 | `	/* Create the global scope */` |
|    2578 | 8117 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 8118 | `	/* Point to the global scope */` |
|    2578 | 8119 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2578 | 8120 | `	return SXRET_OK;` |
|       2 | 8121 |  |
|       - | 8122 | `/*` |
|       - | 8123 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 8124 | ` */` |
|   15502 | 8125 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 8126 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8127 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8128 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8129 | `	)` |
|       2 | 8130 |  |
|   15504 | 8131 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8132 | `	GenBlock *pBlock,*pParent;` |
|       - | 8133 | `	/* Reset state */` |
|   15504 | 8134 | `	SySetReset(&pGen->aLabel);` |
|   15504 | 8135 | `	SySetReset(&pGen->aGoto);` |
|   15504 | 8136 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   15504 | 8137 | `	SyBlobRelease(&pGen->sWorker);` |
|   15504 | 8138 | `	SyBlobRelease(&pGen->sNamespace);` |
|   15504 | 8139 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   15504 | 8140 | `	SyHashRelease(&pGen->hUseImports);` |
|   15504 | 8141 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 8142 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 8143 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 8144 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 8145 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 8146 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 8147 | `	 * number of unique names, which is acceptable. */` |
|       - | 8148 | `	/* Point to the global scope */` |
|   15504 | 8149 | `	pBlock = pGen->pCurrent;` |
|   15504 | 8150 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 8151 | `		pParent = pBlock->pParent;` |
|     ! 0 | 8152 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 8153 | `		pBlock = pParent;` |
|     ! 0 | 8154 | `	}` |
|   15504 | 8155 | `	pGen->xErr = xErr;` |
|   15504 | 8156 | `	pGen->pErrData = pErrData;` |
|   15504 | 8157 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   15504 | 8158 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   15504 | 8159 | `	pGen->pIn = pGen->pEnd = 0;` |
|   15504 | 8160 | `	pGen->nErr = 0;` |
|   15504 | 8161 | `	return SXRET_OK;` |
|       2 | 8162 |  |
|       - | 8163 | `/*` |
|       - | 8164 | ` * Generate a compile-time error message.` |
|       - | 8165 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 8166 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 8167 | ` * abort compilation immediately.` |
|       - | 8168 | ` */` |
|     452 | 8169 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 8170 |  |
|     454 | 8171 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     454 | 8172 | `	const char *zErr = "Error";` |
|       - | 8173 | `	SyString *pFile;` |
|       - | 8174 | `	va_list ap;` |
|       - | 8175 | `	sxi32 rc;` |
|       - | 8176 | `	/* Reset the working buffer */` |
|     454 | 8177 | `	SyBlobReset(pWorker);` |
|       - | 8178 | `	/* Peek the processed file path if available */` |
|     454 | 8179 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     454 | 8180 | `	if( nErrType == E_ERROR ){` |
|       - | 8181 | `		/* Increment the error counter */` |
|     412 | 8182 | `		pGen->nErr++;` |
|     412 | 8183 | `		if( pGen->nErr > 15 ){` |
|       - | 8184 | `			/* Error count limit reached */` |
|       5 | 8185 | `			if( pGen->xErr ){` |
|       5 | 8186 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 8187 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 8188 | `				if( pFile ){` |
|       5 | 8189 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 8190 | `				}` |
|       5 | 8191 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 8192 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 8193 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 8194 | `				}` |
|       2 | 8195 | `			}` |
|       - | 8196 | `			/* Abort immediately */` |
|       5 | 8197 | `			return SXERR_ABORT;` |
|       - | 8198 | `		}` |
|     203 | 8199 | `	}` |
|     450 | 8200 | `	if( pGen->xErr == 0 ){` |
|       - | 8201 | `		/* No available error consumer,return immediately */` |
|       3 | 8202 | `		return SXRET_OK;` |
|       - | 8203 | `	}` |
|     447 | 8204 | `	switch(nErrType){` |
|     405 | 8205 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      29 | 8206 | `	case E_WARNING: zErr = "Warning";     break;` |
|       7 | 8207 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 8208 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 8209 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 8210 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 8211 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 8212 | `	default:` |
|     ! 0 | 8213 | `		break;` |
|       - | 8214 | `	}` |
|     447 | 8215 | `	rc = SXRET_OK;` |
|       - | 8216 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     447 | 8217 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     447 | 8218 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     447 | 8219 | `	va_start(ap,zFormat);` |
|     447 | 8220 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     447 | 8221 | `	va_end(ap);` |
|     447 | 8222 | `	if( pFile ){` |
|     447 | 8223 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     223 | 8224 | `	}` |
|       - | 8225 | `	/* Append a new line */` |
|     447 | 8226 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     447 | 8227 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 8228 | `		/* Consume the generated error message */` |
|     447 | 8229 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     223 | 8230 | `	}` |
|     447 | 8231 | `	return rc;` |
|     228 | 8232 |  |
|       - | 8233 |  |
