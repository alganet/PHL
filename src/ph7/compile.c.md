# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3715/4827 lines (76.96%)

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
|    2860 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    2862 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    8023 |  131 | `	for(;;){` |
|   16048 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2750 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2750 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2728 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      11 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   13322 |  140 | `		pBlock = pBlock->pParent;` |
|   13322 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1432 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  555414 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  555416 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  555416 |  162 | `	pBlock->pUserData   = pUserData;` |
|  555416 |  163 | `	pBlock->pGen        = pGen;` |
|  555416 |  164 | `	pBlock->iFlags      = iType;` |
|  555416 |  165 | `	pBlock->pParent     = 0;` |
|  555416 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  555416 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  555416 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  552812 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  552814 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  552814 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  552814 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  552814 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  552814 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  552814 |  200 | `	pGen->pCurrent = pBlock;` |
|  552814 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  267514 |  203 | `		*ppBlock = pBlock;` |
|  133756 |  204 | `	}` |
|  552814 |  205 | `	return SXRET_OK;` |
|  276408 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  552804 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  552806 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  552806 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  552806 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  552804 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  552806 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  552806 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  552806 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  552806 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  552804 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  552806 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  552806 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  552806 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  552806 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  552806 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  552806 |  244 | `	return SXRET_OK;` |
|  276404 |  245 |  |
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
|  168600 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  168602 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  168602 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  168602 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  168602 |  265 | `	return rc;` |
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
|  393776 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  393778 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  722372 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  328596 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  127990 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  200608 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   32010 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  168600 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  168600 |  298 | `		if( pInstr ){` |
|  168600 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  168600 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  168600 |  302 | `			aFix[n].nJumpType = -1;` |
|   84299 |  303 | `		}` |
|   84301 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  393778 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|  150300 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|  150302 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|  150448 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  150300 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  150432 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|  150300 |  358 | `	return SXRET_OK;` |
|   75152 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  489464 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  489466 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  489466 |  367 | `	if( pEntry == 0 ){` |
|  241286 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  248182 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  248182 |  371 | `	return SXRET_OK;` |
|  244734 |  372 |  |
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
|  241284 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  241286 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  241286 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  120642 |  387 | `	}` |
|  241286 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   85640 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   85642 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   85642 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   85642 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   85642 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   85642 |  408 | `	return pObj;` |
|   42822 |  409 |  |
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
|   86054 |  431 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  432 |  |
|   86056 |  433 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   86056 |  434 | `	sxu32 nIdx = 0;` |
|   86056 |  435 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  436 | `		ph7_value *pObj;` |
|       - |  437 | `		sxi64 iValue;` |
|   85642 |  438 | `		iValue = PH7_TokenValueToInt64(&pToken->sData);` |
|   85642 |  439 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   85642 |  440 | `		if( pObj == 0 ){` |
|     ! 0 |  441 | `			SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  442 | `			return SXERR_ABORT;` |
|       - |  443 | `		}` |
|   85642 |  444 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   42822 |  445 | `	}else{` |
|       - |  446 | `		/* Real number */` |
|       - |  447 | `		ph7_value *pObj;` |
|       - |  448 | `		/* Reserve a new constant */` |
|     416 |  449 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     416 |  450 | `		if( pObj == 0 ){` |
|     ! 0 |  451 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  452 | `			return SXERR_ABORT;` |
|       - |  453 | `		}` |
|     416 |  454 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&pToken->sData);` |
|     416 |  455 | `		PH7_MemObjToReal(pObj);` |
|       - |  456 | `	}` |
|       - |  457 | `	/* Emit the load constant instruction */` |
|   86056 |  458 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  459 | `	/* Node successfully compiled */` |
|   86056 |  460 | `	return SXRET_OK;` |
|   43029 |  461 |  |
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
|   56280 |  473 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  474 |  |
|   56282 |  475 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  476 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  477 | `	ph7_value *pObj;` |
|       - |  478 | `	sxu32 nIdx;` |
|   56282 |  479 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  480 | `	/* Delimit the string */` |
|   56282 |  481 | `	zIn  = pStr->zString;` |
|   56282 |  482 | `	zEnd = &zIn[pStr->nByte];` |
|   56282 |  483 | `	if( zIn >= zEnd ){` |
|       - |  484 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  485 | `		 * rather than reserving a new object each time. */` |
|     138 |  486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     138 |  487 | `		return SXRET_OK;` |
|       - |  488 | `	}` |
|   56146 |  489 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  490 | `		/* Already processed,emit the load constant instruction` |
|       - |  491 | `		 * and return.` |
|       - |  492 | `		 */` |
|   16664 |  493 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   16664 |  494 | `		return SXRET_OK;` |
|       - |  495 | `	}` |
|       - |  496 | `	/* Reserve a new constant */` |
|   39484 |  497 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   39484 |  498 | `	if( pObj == 0 ){` |
|     ! 0 |  499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  500 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  501 | `		return SXERR_ABORT;` |
|       - |  502 | `	}` |
|   39484 |  503 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  504 | `	/* Compile the node */` |
|   39524 |  505 | `	for(;;){` |
|   79050 |  506 | `		if( zIn >= zEnd ){` |
|       - |  507 | `			/* End of input */` |
|   39484 |  508 | `			break;` |
|       - |  509 | `		}` |
|   39568 |  510 | `		zCur = zIn;` |
|  628110 |  511 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  588544 |  512 | `			zIn++;` |
|       2 |  513 | `		}` |
|   39568 |  514 | `		if( zIn > zCur ){` |
|       - |  515 | `			/* Append raw contents*/` |
|   39548 |  516 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   19773 |  517 | `		}` |
|   39568 |  518 | `		zIn++;` |
|   39568 |  519 | `		if( zIn < zEnd ){` |
|     105 |  520 | `			if( zIn[0] == '\\' ){` |
|       - |  521 | `				/* A literal backslash */` |
|      23 |  522 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      94 |  523 | `			}else if( zIn[0] == '\'' ){` |
|       - |  524 | `				/* A single quote */` |
|      11 |  525 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |  526 | `			}else{` |
|       - |  527 | `				/* verbatim copy */` |
|      73 |  528 | `				zIn--;` |
|      73 |  529 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      73 |  530 | `				zIn++;` |
|       - |  531 | `			}` |
|      52 |  532 | `		}` |
|       - |  533 | `		/* Advance the stream cursor */` |
|   39568 |  534 | `		zIn++;` |
|       2 |  535 | `	}` |
|       - |  536 | `	/* Emit the load constant instruction */` |
|   39484 |  537 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   39484 |  538 | `	if( pStr->nByte < 1024 ){` |
|       - |  539 | `		/* Install in the literal table */` |
|   39484 |  540 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   19741 |  541 | `	}` |
|       - |  542 | `	/* Node successfully compiled */` |
|   39484 |  543 | `	return SXRET_OK;` |
|   28142 |  544 |  |
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
|    1710 |  606 | `static sxi32 GenStateProcessStringExpression(` |
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
|    1712 |  617 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  618 | `	/* Preallocate some slots */` |
|    1712 |  619 | `	SySetAlloc(&sToken,0x08);` |
|       - |  620 | `	/* Tokenize the text */` |
|    1712 |  621 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  622 | `	/* Swap delimiter */` |
|    1712 |  623 | `	pTmpIn  = pGen->pIn;` |
|    1712 |  624 | `	pTmpEnd = pGen->pEnd;` |
|    1712 |  625 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1712 |  626 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  627 | `	/* Compile the expression */` |
|    1712 |  628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  629 | `	/* Restore token stream */` |
|    1712 |  630 | `	pGen->pIn  = pTmpIn;` |
|    1712 |  631 | `	pGen->pEnd = pTmpEnd;` |
|       - |  632 | `	/* Release the token set */` |
|    1712 |  633 | `	SySetRelease(&sToken);` |
|       - |  634 | `	/* Compilation result */` |
|    1712 |  635 | `	return rc;` |
|       2 |  636 |  |
|       - |  637 | `/*` |
|       - |  638 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  639 | ` */` |
|   16358 |  640 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  641 |  |
|       - |  642 | `	ph7_value *pConstObj;` |
|   16360 |  643 | `	sxu32 nIdx = 0;` |
|       - |  644 | `	/* Reserve a new constant */` |
|   16360 |  645 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   16360 |  646 | `	if( pConstObj == 0 ){` |
|     ! 0 |  647 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  648 | `		return 0;` |
|       - |  649 | `	}` |
|   16360 |  650 | `	(*pCount)++;` |
|   16360 |  651 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  652 | `	/* Emit the load constant instruction */` |
|   16360 |  653 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   16360 |  654 | `	return pConstObj;` |
|    8181 |  655 |  |
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
|   15154 |  694 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  695 |  |
|   15156 |  696 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  697 | `	const char *zIn,*zCur,*zEnd;` |
|   15156 |  698 | `	ph7_value *pObj = 0;` |
|       - |  699 | `	sxi32 iCons;` |
|       - |  700 | `	sxi32 rc;` |
|       - |  701 | `	/* Delimit the string */` |
|   15156 |  702 | `	zIn  = pStr->zString;` |
|   15156 |  703 | `	zEnd = &zIn[pStr->nByte];` |
|   15156 |  704 | `	if( zIn >= zEnd ){` |
|       - |  705 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  706 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  707 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  708 | `		 */` |
|     226 |  709 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     226 |  710 | `		return SXRET_OK;` |
|       - |  711 | `	}` |
|   14932 |  712 | `	zCur = 0;` |
|       - |  713 | `	/* Compile the node */` |
|   14932 |  714 | `	iCons = 0;` |
|    8320 |  715 | `	for(;;){` |
|   25114 |  716 | `		zCur = zIn;` |
|  137016 |  717 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  113614 |  718 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      44 |  719 | `				break;` |
|  113530 |  720 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1628 |  721 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     814 |  722 | `					break;` |
|       - |  723 | `			}` |
|  111904 |  724 | `			zIn++;` |
|       2 |  725 | `		}` |
|   25114 |  726 | `		if( zIn > zCur ){` |
|   11774 |  727 | `			if( pObj == 0 ){` |
|   11498 |  728 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   11498 |  729 | `				if( pObj == 0 ){` |
|     ! 0 |  730 | `					return SXERR_ABORT;` |
|       - |  731 | `				}` |
|    5748 |  732 | `			}` |
|   11774 |  733 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    5886 |  734 | `		}` |
|   25114 |  735 | `		if( zIn >= zEnd ){` |
|   14932 |  736 | `			break;` |
|       - |  737 | `		}` |
|   10184 |  738 | `		if( zIn[0] == '\\' ){` |
|    8474 |  739 | `			const char *zPtr = 0;` |
|       - |  740 | `			sxu32 n;` |
|    8474 |  741 | `			zIn++;` |
|    8474 |  742 | `			if( zIn >= zEnd ){` |
|     ! 0 |  743 | `				break;` |
|       - |  744 | `			}` |
|    8474 |  745 | `			if( pObj == 0 ){` |
|    4864 |  746 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    4864 |  747 | `				if( pObj == 0 ){` |
|     ! 0 |  748 | `					return SXERR_ABORT;` |
|       - |  749 | `				}` |
|    2431 |  750 | `			}` |
|    8474 |  751 | `			n = sizeof(char); /* size of conversion */` |
|    8474 |  752 | `			switch( zIn[0] ){` |
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
|    3871 |  773 | `			case 'n':` |
|       - |  774 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    7744 |  775 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    7744 |  776 | `				break;` |
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
|    8474 |  844 | `			zIn += n;` |
|    8474 |  845 | `			continue;` |
|       - |  846 | `		}` |
|    1712 |  847 | `		if( zIn[0] == '{' ){` |
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
|    1626 |  881 | `			const char *zExpr = zIn;` |
|       - |  882 | `			/* Assemble variable name */` |
|     812 |  883 | `			for(;;){` |
|       - |  884 | `				/* Jump leading dollars */` |
|    3250 |  885 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1626 |  886 | `					zIn++;` |
|       2 |  887 | `				}` |
|     812 |  888 | `				for(;;){` |
|   10066 |  889 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7630 |  890 | `						zIn++;` |
|       2 |  891 | `					}` |
|    1626 |  892 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  893 | `						/* UTF-8 stream */` |
|     ! 0 |  894 | `						zIn++;` |
|     ! 0 |  895 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  896 | `							zIn++;` |
|     ! 0 |  897 | `						}` |
|     ! 0 |  898 | `						continue;` |
|       - |  899 | `					}` |
|    1626 |  900 | `					break;` |
|     ! 0 |  901 | `				}` |
|    1626 |  902 | `				if( zIn >= zEnd ){` |
|      96 |  903 | `					break;` |
|       - |  904 | `				}` |
|    1532 |  905 | `				if( zIn[0] == '[' ){` |
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
|    1524 |  923 | `				}else if(zIn[0] == '{' ){` |
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
|    1520 |  941 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  942 | `					/* Member access operator '->' */` |
|     ! 0 |  943 | `					zIn += 2;` |
|    1520 |  944 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  945 | `					/* Static member access operator '::' */` |
|     ! 0 |  946 | `					zIn += 2;` |
|     ! 0 |  947 | `				}else{` |
|     761 |  948 | `					break;` |
|       - |  949 | `				}` |
|     ! 0 |  950 | `			}` |
|       - |  951 | `			/* Process the expression */` |
|    1626 |  952 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1626 |  953 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  954 | `				return SXERR_ABORT;` |
|       - |  955 | `			}` |
|    1626 |  956 | `			if( rc != SXERR_EMPTY ){` |
|    1624 |  957 | `				++iCons;` |
|     811 |  958 | `			}` |
|       - |  959 | `		}` |
|       - |  960 | `		/* Invalidate the previously used constant */` |
|    1712 |  961 | `		pObj = 0;` |
|       2 |  962 | `	}/*for(;;)*/` |
|   14932 |  963 | `	if( iCons > 1 ){` |
|       - |  964 | `		/* Concatenate all compiled constants */` |
|    1286 |  965 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     642 |  966 | `	}` |
|       - |  967 | `	/* Node successfully compiled */` |
|   14932 |  968 | `	return SXRET_OK;` |
|    7579 |  969 |  |
|       - |  970 | `/*` |
|       - |  971 | ` * Compile a double quoted string.` |
|       - |  972 | ` *  See the block-comment above for more information.` |
|       - |  973 | ` */` |
|   15128 |  974 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  975 |  |
|       - |  976 | `	sxi32 rc;` |
|   15130 |  977 | `	rc = GenStateCompileString(&(*pGen));` |
|    7564 |  978 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  979 | `	/* Compilation result */` |
|   15130 |  980 | `	return rc;` |
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
|   15664 | 1012 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   15666 | 1023 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1024 | `	/* Compile the expression*/` |
|   15666 | 1025 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1026 | `	/* Restore token stream */` |
|   15666 | 1027 | `	RE_SWAP_DELIMITER(pGen);` |
|   15666 | 1028 | `	return rc;` |
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
|   22804 | 1067 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 | 1068 |  |
|       - | 1069 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1070 | `	SyToken *pKey,*pCur;` |
|   22806 | 1071 | `	sxi32 iEmitRef = 0;` |
|   22806 | 1072 | `	sxi32 nPair = 0;` |
|       - | 1073 | `	sxi32 iNest;` |
|       - | 1074 | `	sxi32 rc;` |
|   22806 | 1075 | `	xValidator = 0;` |
|   18580 | 1076 | `	for(;;){` |
|       - | 1077 | `		/* Jump leading commas */` |
|   42060 | 1078 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    4900 | 1079 | `			pGen->pIn++;` |
|       2 | 1080 | `		}` |
|   37162 | 1081 | `		pCur = pGen->pIn;` |
|   37162 | 1082 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1083 | `			/* No more entry to process */` |
|   22794 | 1084 | `			break;` |
|       - | 1085 | `		}` |
|   14370 | 1086 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1087 | `			continue;` |
|       - | 1088 | `		}` |
|       - | 1089 | `		/* Compile the key if available */` |
|   14370 | 1090 | `		pKey = pCur;` |
|   14370 | 1091 | `		iNest = 0;` |
|   39826 | 1092 | `		while( pCur < pGen->pIn ){` |
|   26648 | 1093 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1192 | 1094 | `				break;` |
|       - | 1095 | `			}` |
|   25458 | 1096 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      78 | 1097 | `				iNest++;` |
|   25420 | 1098 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1099 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1100 | `				 * parser will shortly detect any syntax error.` |
|       - | 1101 | `				 */` |
|      78 | 1102 | `				iNest--;` |
|      38 | 1103 | `			}` |
|   25458 | 1104 | `			pCur++;` |
|       2 | 1105 | `		}` |
|   14370 | 1106 | `		rc = SXERR_EMPTY;` |
|   14370 | 1107 | `		if( pCur < pGen->pIn ){` |
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
|   13770 | 1123 | `		}else if( pKey == pCur ){` |
|       - | 1124 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1125 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1126 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1127 | `		}else{` |
|       - | 1128 | `			/* Reset back the cursor and point to the entry value */` |
|   13180 | 1129 | `			pCur = pKey;` |
|       - | 1130 | `		}` |
|   14360 | 1131 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1132 | `			/* No available key,load NULL */` |
|   13182 | 1133 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    6590 | 1134 | `		}` |
|   14360 | 1135 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   14358 | 1150 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   14358 | 1151 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1152 | `			return SXERR_ABORT;` |
|       - | 1153 | `		}` |
|   14358 | 1154 | `		if( iEmitRef ){` |
|       - | 1155 | `			/* Emit the load reference instruction */` |
|      32 | 1156 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1157 | `		}` |
|   14358 | 1158 | `		xValidator = 0;` |
|   14358 | 1159 | `		iEmitRef = 0;` |
|   14358 | 1160 | `		nPair++;` |
|       2 | 1161 | `	}` |
|       - | 1162 | `	/* Emit the load map instruction */` |
|   22794 | 1163 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1164 | `	/* Node successfully compiled */` |
|   22794 | 1165 | `	return SXRET_OK;` |
|   11404 | 1166 |  |
|       - | 1167 | `/*` |
|       - | 1168 | ` * Compile the 'array' language construct.` |
|       - | 1169 | ` *	 According to the PHP language reference manual` |
|       - | 1170 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1171 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1172 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1173 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1174 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1175 | ` */` |
|   22572 | 1176 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1177 |  |
|       - | 1178 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   22574 | 1179 | `	pGen->pIn += 2;` |
|   22574 | 1180 | `	pGen->pEnd--;` |
|   11286 | 1181 | `	SXUNUSED(iCompileFlag);` |
|   22574 | 1182 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1183 |  |
|       - | 1184 | `/*` |
|       - | 1185 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - | 1186 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - | 1187 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - | 1188 | ` */` |
|     232 | 1189 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1190 |  |
|       - | 1191 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     234 | 1192 | `	pGen->pIn++;` |
|     234 | 1193 | `	pGen->pEnd--;` |
|     116 | 1194 | `	SXUNUSED(iCompileFlag);` |
|     234 | 1195 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1196 |  |
|       - | 1197 | `/*` |
|       - | 1198 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - | 1199 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1200 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1201 | ` * error message.` |
|       - | 1202 | ` * See the routine responible of compiling the list language construct` |
|       - | 1203 | ` * for more inforation.` |
|       - | 1204 | ` */` |
|     128 | 1205 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1206 |  |
|     130 | 1207 | `	sxi32 rc = SXRET_OK;` |
|     130 | 1208 | `	if( pRoot->pOp ){` |
|     ! 0 | 1209 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 | 1210 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - | 1211 | `				/* Unexpected expression */` |
|     ! 0 | 1212 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1213 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 | 1214 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 | 1215 | `					rc = SXERR_INVALID;` |
|     ! 0 | 1216 | `				}` |
|     ! 0 | 1217 | `		}` |
|     130 | 1218 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1219 | `		/* Unexpected expression */` |
|       5 | 1220 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1221 | `			"list(): Expecting a variable not an expression");` |
|       5 | 1222 | `		if( rc != SXERR_ABORT ){` |
|       5 | 1223 | `			rc = SXERR_INVALID;` |
|       2 | 1224 | `		}` |
|       2 | 1225 | `	}` |
|     130 | 1226 | `	return rc;` |
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
|       - | 1242 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - | 1243 | `struct NestedListEntry {` |
|       - | 1244 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - | 1245 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - | 1246 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - | 1247 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - | 1248 | `};` |
|       - | 1249 | `/*` |
|       - | 1250 | ` * Shared body for list() and short list [...] compilation.` |
|       - | 1251 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - | 1252 | ` * the opening delimiter and before the closing delimiter.` |
|       - | 1253 | ` */` |
|      74 | 1254 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       2 | 1255 |  |
|       - | 1256 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - | 1257 | `	SyToken *pNext;` |
|       - | 1258 | `	sxi32 nExpr;` |
|       - | 1259 | `	sxi32 rc;` |
|      76 | 1260 | `	nExpr = 0;` |
|      76 | 1261 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     230 | 1262 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     156 | 1263 | `		if( pGen->pIn < pNext ){` |
|       - | 1264 | `			/* Check for nested list() */` |
|     144 | 1265 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 | 1266 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 1267 | `				/* Record this nested list for post-processing */` |
|       3 | 1268 | `				SyToken *pListEnd = 0;` |
|       3 | 1269 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 | 1270 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 | 1271 | `				}` |
|       3 | 1272 | `				if( pListEnd ){` |
|       - | 1273 | `					struct NestedListEntry sEntry;` |
|       3 | 1274 | `					sEntry.nIndex = nExpr;` |
|       3 | 1275 | `					sEntry.pStart = pGen->pIn;` |
|       3 | 1276 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 | 1277 | `					sEntry.isShort = 0;` |
|       3 | 1278 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 | 1279 | `				}` |
|       - | 1280 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 | 1281 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     143 | 1282 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 1283 | `				/* Nested short destructuring [...] */` |
|      13 | 1284 | `				SyToken *pBracketEnd = 0;` |
|      13 | 1285 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 | 1286 | `				if( pBracketEnd ){` |
|       - | 1287 | `					struct NestedListEntry sEntry;` |
|      13 | 1288 | `					sEntry.nIndex = nExpr;` |
|      13 | 1289 | `					sEntry.pStart = pGen->pIn;` |
|      13 | 1290 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 | 1291 | `					sEntry.isShort = 1;` |
|      13 | 1292 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 | 1293 | `				}` |
|       - | 1294 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 | 1295 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 | 1296 | `			}else{` |
|       - | 1297 | `				/* Compile the expression holding the variable */` |
|     130 | 1298 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     130 | 1299 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 1300 | `					SySetRelease(&sNested);` |
|     ! 0 | 1301 | `					return SXRET_OK;` |
|       - | 1302 | `				}` |
|       - | 1303 | `			}` |
|      73 | 1304 | `		}else{` |
|       - | 1305 | `			/* Empty entry,load NULL */` |
|      13 | 1306 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - | 1307 | `		}` |
|     156 | 1308 | `		nExpr++;` |
|       - | 1309 | `		/* Advance the stream cursor */` |
|     156 | 1310 | `		pGen->pIn = &pNext[1];` |
|       2 | 1311 | `	}` |
|       - | 1312 | `	/* Emit the LOAD_LIST instruction */` |
|      76 | 1313 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - | 1314 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - | 1315 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - | 1316 | `	 * at the corresponding index and recursively destructure it.` |
|       - | 1317 | `	 */` |
|      76 | 1318 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 | 1319 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - | 1320 | `		sxu32 i;` |
|      27 | 1321 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 | 1322 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 | 1323 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 1324 | `			ph7_value *pIdx;` |
|       - | 1325 | `			sxu32 nConstIdx;` |
|       - | 1326 | `			/* DUP the source array (it's on stack top) */` |
|      15 | 1327 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - | 1328 | `			/* Push the integer index for this nested entry */` |
|      15 | 1329 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 | 1330 | `			if( pIdx == 0 ){` |
|     ! 0 | 1331 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1332 | `				SySetRelease(&sNested);` |
|     ! 0 | 1333 | `				return SXERR_ABORT;` |
|       - | 1334 | `			}` |
|      15 | 1335 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 | 1336 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - | 1337 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - | 1338 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - | 1339 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - | 1340 | `			 */` |
|      15 | 1341 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - | 1342 | `			/* Recursively compile the inner list */` |
|      15 | 1343 | `			pGen->pIn = apNested[i].pStart;` |
|      15 | 1344 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 | 1345 | `			if( apNested[i].isShort ){` |
|      13 | 1346 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 | 1347 | `			}else{` |
|       3 | 1348 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - | 1349 | `			}` |
|      15 | 1350 | `			pGen->pIn = pSavedIn;` |
|      15 | 1351 | `			pGen->pEnd = pSavedEnd;` |
|      15 | 1352 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1353 | `				SySetRelease(&sNested);` |
|     ! 0 | 1354 | `				return SXERR_ABORT;` |
|       - | 1355 | `			}` |
|       - | 1356 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 | 1357 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 | 1358 | `		}` |
|       6 | 1359 | `	}` |
|      76 | 1360 | `	SySetRelease(&sNested);` |
|       - | 1361 | `	/* Node successfully compiled */` |
|      76 | 1362 | `	return SXRET_OK;` |
|      39 | 1363 |  |
|      32 | 1364 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1365 |  |
|       - | 1366 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      34 | 1367 | `	pGen->pIn += 2;` |
|      34 | 1368 | `	pGen->pEnd--;` |
|      16 | 1369 | `	SXUNUSED(iCompileFlag);` |
|      34 | 1370 | `	return GenStateCompileListBody(pGen);` |
|       2 | 1371 |  |
|      42 | 1372 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1373 |  |
|       - | 1374 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      44 | 1375 | `	pGen->pIn++;` |
|      44 | 1376 | `	pGen->pEnd--;` |
|      21 | 1377 | `	SXUNUSED(iCompileFlag);` |
|      44 | 1378 | `	return GenStateCompileListBody(pGen);` |
|       2 | 1379 |  |
|       - | 1380 | `/* Forward declarations */` |
|       - | 1381 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - | 1382 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - | 1383 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - | 1384 | `/*` |
|       - | 1385 | ` * Compile an annoynmous function or a closure.` |
|       - | 1386 | ` * According to the PHP language reference` |
|       - | 1387 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - | 1388 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - | 1389 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - | 1390 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - | 1391 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - | 1392 | ` *  Example Anonymous function variable assignment example` |
|       - | 1393 | ` * <?php` |
|       - | 1394 | ` * $greet = function($name)` |
|       - | 1395 | ` * {` |
|       - | 1396 | ` *    printf("Hello %s\r\n", $name);` |
|       - | 1397 | ` * };` |
|       - | 1398 | ` * $greet('World');` |
|       - | 1399 | ` * $greet('PHP');` |
|       - | 1400 | ` * ?>` |
|       - | 1401 | ` * Note that the implementation of annoynmous function and closure under` |
|       - | 1402 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - | 1403 | ` */` |
|     166 | 1404 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1405 |  |
|       - | 1406 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - | 1407 | `	char zName[512];         /* Unique lambda name */` |
|       - | 1408 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - | 1409 | `							  * one thread is allowed to compile the script.` |
|       - | 1410 | `						      */` |
|       - | 1411 | `	ph7_value *pObj;` |
|       - | 1412 | `	SyString sName;` |
|       - | 1413 | `	sxu32 nIdx;` |
|       - | 1414 | `	sxu32 nLen;` |
|       - | 1415 | `	sxi32 rc;` |
|       - | 1416 |  |
|     168 | 1417 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     168 | 1418 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 | 1419 | `		pGen->pIn++;` |
|     ! 0 | 1420 | `	}` |
|       - | 1421 | `	/* Reserve a constant for the lambda */` |
|     168 | 1422 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     168 | 1423 | `	if( pObj == 0 ){` |
|     ! 0 | 1424 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1425 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1426 | `		return SXERR_ABORT;` |
|       - | 1427 | `	}` |
|       - | 1428 | `	/* Generate a unique name */` |
|     168 | 1429 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - | 1430 | `	/* Make sure the generated name is unique */` |
|     168 | 1431 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 1432 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 | 1433 | `	}` |
|     168 | 1434 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     168 | 1435 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - | 1436 | `	/* Compile the lambda body */` |
|     168 | 1437 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     168 | 1438 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1439 | `		return SXERR_ABORT;` |
|       - | 1440 | `	}` |
|     168 | 1441 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1442 | `		/* Emit the load closure instruction */` |
|      14 | 1443 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       8 | 1444 | `	}else{` |
|       - | 1445 | `		/* Emit the load constant instruction */` |
|     156 | 1446 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1447 | `	}` |
|       - | 1448 | `	/* Node successfully compiled */` |
|     168 | 1449 | `	return SXRET_OK;` |
|      85 | 1450 |  |
|       - | 1451 | `/*` |
|       - | 1452 | ` * Compile a backtick quoted string.` |
|       - | 1453 | ` */` |
|       4 | 1454 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1455 |  |
|       - | 1456 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - | 1457 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - | 1458 | `	 */` |
|       7 | 1459 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - | 1460 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 | 1461 | `		ph7_lib_version()` |
|       - | 1462 | `		);` |
|       - | 1463 | `	/* Load NULL */` |
|       5 | 1464 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1465 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1466 | `	/* Node successfully compiled */` |
|       5 | 1467 | `	return SXRET_OK;` |
|       1 | 1468 |  |
|       - | 1469 | `/*` |
|       - | 1470 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - | 1471 | ` * construct.` |
|       - | 1472 | ` */` |
|      72 | 1473 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1474 |  |
|       - | 1475 | `	SyString *pName;` |
|       - | 1476 | `	sxu32 nKeyID;` |
|       - | 1477 | `	sxi32 rc;` |
|       - | 1478 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      74 | 1479 | `	pName = &pGen->pIn->sData;` |
|      74 | 1480 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      74 | 1481 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      74 | 1482 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 | 1483 | `		SyToken *pTmp,*pNext = 0;` |
|       - | 1484 | `		/* Compile arguments one after one */` |
|       9 | 1485 | `		pTmp = pGen->pEnd;` |
|       - | 1486 | `		/* Symisc eXtension to the PHP programming language:` |
|       - | 1487 | `		 * 'echo' can be used in the context of a function which` |
|       - | 1488 | `		 *  mean that the following expression is valid:` |
|       - | 1489 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - | 1490 | `		 */` |
|       9 | 1491 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 | 1492 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 | 1493 | `			if( pGen->pIn < pNext ){` |
|       9 | 1494 | `				pGen->pEnd = pNext;` |
|       9 | 1495 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 | 1496 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1497 | `					return SXERR_ABORT;` |
|       - | 1498 | `				}` |
|       9 | 1499 | `				if( rc != SXERR_EMPTY ){` |
|       - | 1500 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - | 1501 | `					 * without the overhead of a function call.` |
|       - | 1502 | `					 * This is a very powerful optimization that improve` |
|       - | 1503 | `					 * performance greatly.` |
|       - | 1504 | `					 */` |
|       9 | 1505 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 | 1506 | `				}` |
|       4 | 1507 | `			}` |
|       - | 1508 | `			/* Jump trailing commas */` |
|       9 | 1509 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 1510 | `				pNext++;` |
|     ! 0 | 1511 | `			}` |
|       9 | 1512 | `			pGen->pIn = pNext;` |
|       1 | 1513 | `		}` |
|       - | 1514 | `		/* Restore token stream */` |
|       9 | 1515 | `		pGen->pEnd = pTmp;` |
|       5 | 1516 | `	}else{` |
|      66 | 1517 | `		sxi32 nArg = 0;` |
|      66 | 1518 | `		sxu32 nIdx = 0;` |
|      66 | 1519 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      66 | 1520 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1521 | `			return SXERR_ABORT;` |
|      66 | 1522 | `		}else if(rc != SXERR_EMPTY ){` |
|      66 | 1523 | `			nArg = 1;` |
|      32 | 1524 | `		}` |
|      66 | 1525 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1526 | `			ph7_value *pObj;` |
|       - | 1527 | `			/* Emit the call instruction */` |
|      20 | 1528 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 | 1529 | `			if( pObj == 0 ){` |
|     ! 0 | 1530 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1531 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1532 | `				return SXERR_ABORT;` |
|       - | 1533 | `			}` |
|      20 | 1534 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1535 | `			/* Install in the literal table */` |
|      20 | 1536 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       9 | 1537 | `		}` |
|       - | 1538 | `		/* Emit the call instruction */` |
|      66 | 1539 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      66 | 1540 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - | 1541 | `	}` |
|       - | 1542 | `	/* Node successfully compiled */` |
|      74 | 1543 | `	return SXRET_OK;` |
|      38 | 1544 |  |
|       - | 1545 | `/*` |
|       - | 1546 | ` * Compile a node holding a variable declaration.` |
|       - | 1547 | ` * According to the PHP language reference` |
|       - | 1548 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 1549 | ` *  The variable name is case-sensitive.` |
|       - | 1550 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 1551 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1552 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 1553 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 1554 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 1555 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 1556 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 1557 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 1558 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 1559 | ` *  the chapter on Expressions.` |
|       - | 1560 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 1561 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 1562 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 1563 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 1564 | ` *  is being assigned (the source variable).` |
|       - | 1565 | ` */` |
|  760628 | 1566 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1567 |  |
|  760630 | 1568 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1569 | `	sxi32 iVv;` |
|       - | 1570 | `	sxi32 iP1;` |
|       - | 1571 | `	void *p3;` |
|       - | 1572 | `	sxi32 rc;` |
|  760630 | 1573 | `	iVv = -1; /* Variable variable counter */` |
| 1521270 | 1574 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  760642 | 1575 | `		pGen->pIn++;` |
|  760642 | 1576 | `		iVv++;` |
|       2 | 1577 | `	}` |
|  760630 | 1578 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1579 | `		/* Invalid variable name */` |
|     ! 0 | 1580 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 | 1581 | `		if( rc == SXERR_ABORT ){` |
|       - | 1582 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1583 | `			return SXERR_ABORT;` |
|       - | 1584 | `		}` |
|     ! 0 | 1585 | `		return SXRET_OK;` |
|       - | 1586 | `	}` |
|  760630 | 1587 | `	p3  = 0;` |
|  760630 | 1588 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 1589 | `		/* Dynamic variable creation */` |
|      18 | 1590 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 | 1591 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 | 1592 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1593 | `			/* Empty expression */` |
|       3 | 1594 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1595 | `			return SXRET_OK;` |
|       - | 1596 | `		}` |
|       - | 1597 | `		/* Compile the expression holding the variable name */` |
|      16 | 1598 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 | 1599 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1600 | `			return SXERR_ABORT;` |
|      16 | 1601 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 1602 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 | 1603 | `			return SXRET_OK;` |
|       - | 1604 | `		}` |
|       7 | 1605 | `	}else{` |
|       - | 1606 | `		SyHashEntry *pEntry;` |
|       - | 1607 | `		SyString *pName;` |
|  760614 | 1608 | `		char *zName = 0;` |
|       - | 1609 | `		/* Extract variable name */` |
|  760614 | 1610 | `		pName = &pGen->pIn->sData;` |
|       - | 1611 | `		/* Advance the stream cursor */` |
|  760614 | 1612 | `		pGen->pIn++;` |
|  760614 | 1613 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  760614 | 1614 | `		if( pEntry == 0 ){` |
|       - | 1615 | `			/* Duplicate name */` |
|  109342 | 1616 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  109342 | 1617 | `			if( zName == 0 ){` |
|     ! 0 | 1618 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1619 | `				return SXERR_ABORT;` |
|       - | 1620 | `			}` |
|       - | 1621 | `			/* Install in the hashtable */` |
|  109342 | 1622 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   54672 | 1623 | `		}else{` |
|       - | 1624 | `			/* Name already available */` |
|  651274 | 1625 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1626 | `		}` |
|  760614 | 1627 | `		p3 = (void *)zName;` |
|       - | 1628 | `	}` |
|  760626 | 1629 | `	iP1 = 0;` |
|  760626 | 1630 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  292558 | 1631 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1632 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  286560 | 1633 | `			iP1 = 1;` |
|  143279 | 1634 | `		}` |
|  146278 | 1635 | `	}` |
|       - | 1636 | `	/* Emit the load instruction */` |
|  760626 | 1637 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  760638 | 1638 | `	while( iVv > 0 ){` |
|      13 | 1639 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1640 | `		iVv--;` |
|       1 | 1641 | `	}` |
|       - | 1642 | `	/* Node successfully compiled */` |
|  760626 | 1643 | `	return SXRET_OK;` |
|  380316 | 1644 |  |
|       - | 1645 | `/*` |
|       - | 1646 | ` * Load a literal.` |
|       - | 1647 | ` */` |
|  510024 | 1648 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 1649 |  |
|  510026 | 1650 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1651 | `	ph7_value *pObj;` |
|       - | 1652 | `	SyString *pStr;` |
|       - | 1653 | `	sxu32 nIdx;` |
|       - | 1654 | `	/* Extract token value */` |
|  510026 | 1655 | `	pStr = &pToken->sData;` |
|       - | 1656 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  510026 | 1657 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   92596 | 1658 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1659 | `			/* NULL constant are always indexed at 0 */` |
|   39374 | 1660 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   39374 | 1661 | `			return SXRET_OK;` |
|   53224 | 1662 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1663 | `			/* TRUE constant are always indexed at 1 */` |
|     488 | 1664 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     488 | 1665 | `			return SXRET_OK;` |
|       2 | 1666 | `		}` |
|  484051 | 1667 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   80502 | 1668 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1669 | `			/* FALSE constant are always indexed at 2 */` |
|   34382 | 1670 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   34382 | 1671 | `			return SXRET_OK;` |
|  418590 | 1672 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   71076 | 1673 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1674 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5208 | 1675 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5208 | 1676 | `			if( pObj == 0 ){` |
|     ! 0 | 1677 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1678 | `				return SXERR_ABORT;` |
|       - | 1679 | `			}` |
|    5208 | 1680 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1681 | `			/* Emit the load constant instruction */` |
|    5208 | 1682 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5208 | 1683 | `			return SXRET_OK;` |
|  390967 | 1684 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   26242 | 1685 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - | 1686 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 | 1687 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 | 1688 | `			if( pObj == 0 ){` |
|     ! 0 | 1689 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1690 | `				return SXERR_ABORT;` |
|       - | 1691 | `			}` |
|       7 | 1692 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1693 | `				SyString sNs;` |
|       7 | 1694 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 | 1695 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 | 1696 | `			}else{` |
|     ! 0 | 1697 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - | 1698 | `			}` |
|       7 | 1699 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 | 1700 | `			return SXRET_OK;` |
|  390155 | 1701 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   10986 | 1702 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  384656 | 1703 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   13650 | 1704 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 | 1705 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 1706 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 | 1707 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - | 1708 | `				/* Point to the upper block */` |
|      11 | 1709 | `				pBlock = pBlock->pParent;` |
|       1 | 1710 | `			}` |
|      11 | 1711 | `			if( pBlock == 0 ){` |
|       - | 1712 | `				/* Called in the global scope,load NULL */` |
|       5 | 1713 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1714 | `			}else{` |
|       - | 1715 | `				/* Extract the target function/method */` |
|       7 | 1716 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 | 1717 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - | 1718 | `					/* Not a class method,Load null */` |
|       3 | 1719 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1720 | `				}else{` |
|       5 | 1721 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 | 1722 | `					if( pObj == 0 ){` |
|     ! 0 | 1723 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1724 | `						return SXERR_ABORT;` |
|       - | 1725 | `					}` |
|       5 | 1726 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - | 1727 | `					/* Emit the load constant instruction */` |
|       5 | 1728 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1729 | `				}` |
|       - | 1730 | `			}` |
|      11 | 1731 | `			return SXRET_OK;` |
|       - | 1732 | `	}` |
|       - | 1733 | `	/* Query literal table */` |
|  430566 | 1734 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 1735 | `		ph7_value *pLitObj;` |
|       - | 1736 | `		/* Unknown literal,install it in the literal table */` |
|  201410 | 1737 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  201410 | 1738 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1739 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1740 | `			return SXERR_ABORT;` |
|       - | 1741 | `		}` |
|  201410 | 1742 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  201410 | 1743 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  100704 | 1744 | `	}` |
|       - | 1745 | `	/* Emit the load constant instruction */` |
|  430566 | 1746 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  430566 | 1747 | `	return SXRET_OK;` |
|  255014 | 1748 |  |
|       - | 1749 | `/*` |
|       - | 1750 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1751 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1752 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1753 | ` * Otherwise, load the simple literal directly.` |
|       - | 1754 | ` */` |
|  510048 | 1755 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 1756 |  |
|       - | 1757 | `	sxi32 rc;` |
|  510050 | 1758 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1759 | `		return SXRET_OK;` |
|       - | 1760 | `	}` |
|       - | 1761 | `	/* Check if this is a multi-token namespace path */` |
|  510050 | 1762 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 1763 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      26 | 1764 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      26 | 1765 | `		int isAbsolute = 0;` |
|      26 | 1766 | `		SyBlobReset(pWorker);` |
|       - | 1767 | `		/* Check for leading backslash (absolute path) */` |
|      26 | 1768 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      24 | 1769 | `			isAbsolute = 1;` |
|      24 | 1770 | `			pGen->pIn++; /* Skip leading backslash */` |
|      11 | 1771 | `		}` |
|       - | 1772 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      26 | 1773 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 1774 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 1775 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 | 1776 | `		}` |
|       - | 1777 | `		/* Collect all path components */` |
|     102 | 1778 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     102 | 1779 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      40 | 1780 | `				SyBlobAppend(pWorker,"\\",1);` |
|      21 | 1781 | `			}else{` |
|      64 | 1782 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 1783 | `			}` |
|     102 | 1784 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      26 | 1785 | `				pGen->pIn++;` |
|      26 | 1786 | `				break;` |
|       - | 1787 | `			}` |
|      78 | 1788 | `			pGen->pIn++;` |
|       2 | 1789 | `		}` |
|      26 | 1790 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 1791 | `			ph7_value *pObj;` |
|       - | 1792 | `			SyString sPath;` |
|       - | 1793 | `			sxu32 nIdx;` |
|      26 | 1794 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 1795 | `			/* Install in the literal table */` |
|      26 | 1796 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      13 | 1797 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      13 | 1798 | `				if( pObj == 0 ){` |
|     ! 0 | 1799 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1800 | `					return SXERR_ABORT;` |
|       - | 1801 | `				}` |
|      13 | 1802 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      13 | 1803 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       6 | 1804 | `			}` |
|       - | 1805 | `			/* Emit the load constant instruction.` |
|       - | 1806 | `			 * P1=1 means candidate for constant/function/class expansion. */` |
|      26 | 1807 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      26 | 1808 | `			return SXRET_OK;` |
|       - | 1809 | `		}` |
|     ! 0 | 1810 | `	}` |
|       - | 1811 | `	/* Single-token literal: load directly */` |
|  510026 | 1812 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  510026 | 1813 | `	return rc;` |
|  255026 | 1814 |  |
|       - | 1815 | `/*` |
|       - | 1816 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1817 | ` */` |
|  510048 | 1818 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1819 |  |
|       - | 1820 | `	sxi32 rc;` |
|  510050 | 1821 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  510050 | 1822 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1823 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1824 | `		return rc;` |
|       - | 1825 | `	}` |
|       - | 1826 | `	/* Node successfully compiled */` |
|  510050 | 1827 | `	return SXRET_OK;` |
|  255026 | 1828 |  |
|       - | 1829 | `/*` |
|       - | 1830 | ` * Recover from a compile-time error. In other words synchronize` |
|       - | 1831 | ` * the token stream cursor with the first semi-colon seen.` |
|       - | 1832 | ` */` |
|       8 | 1833 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 | 1834 |  |
|       - | 1835 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 | 1836 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 | 1837 | `		pGen->pIn++;` |
|       1 | 1838 | `	}` |
|       9 | 1839 | `	return SXRET_OK;` |
|       1 | 1840 |  |
|       - | 1841 | `/*` |
|       - | 1842 | ` * Check if the given identifier name is reserved or not.` |
|       - | 1843 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - | 1844 | ` */` |
|      36 | 1845 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 | 1846 |  |
|      38 | 1847 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      12 | 1848 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 | 1849 | `			return TRUE;` |
|      10 | 1850 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 | 1851 | `			return TRUE;` |
|       1 | 1852 | `		}` |
|      30 | 1853 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 | 1854 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 | 1855 | `			return TRUE;` |
|       - | 1856 | `		}` |
|     ! 0 | 1857 | `	}` |
|       - | 1858 | `	/* Not a reserved constant */` |
|      30 | 1859 | `	return FALSE;` |
|      20 | 1860 |  |
|       - | 1861 | `/*` |
|       - | 1862 | ` * Compile the 'const' statement.` |
|       - | 1863 | ` * According to the PHP language reference` |
|       - | 1864 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - | 1865 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - | 1866 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - | 1867 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - | 1868 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1869 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - | 1870 | ` *  Syntax` |
|       - | 1871 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - | 1872 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - | 1873 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - | 1874 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - | 1875 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - | 1876 | ` *  to get a list of all defined constants.` |
|       - | 1877 | ` *` |
|       - | 1878 | ` * Symisc eXtension.` |
|       - | 1879 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - | 1880 | ` *  would allow only simple scalar value.` |
|       - | 1881 | ` *  Example` |
|       - | 1882 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 1883 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 1884 | ` */` |
|      32 | 1885 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 | 1886 |  |
|       - | 1887 | `	SySet *pConsCode,*pInstrContainer;` |
|      34 | 1888 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1889 | `	SyString *pName;` |
|       - | 1890 | `	sxi32 rc;` |
|      34 | 1891 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      34 | 1892 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 1893 | `		/* Invalid constant name */` |
|       7 | 1894 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 | 1895 | `		if( rc == SXERR_ABORT ){` |
|       - | 1896 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1897 | `			return SXERR_ABORT;` |
|       - | 1898 | `		}` |
|       7 | 1899 | `		goto Synchronize;` |
|       - | 1900 | `	}` |
|       - | 1901 | `	/* Peek constant name */` |
|      28 | 1902 | `	pName = &pGen->pIn->sData;` |
|       - | 1903 | `	/* Make sure the constant name isn't reserved */` |
|      28 | 1904 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 1905 | `		/* Reserved constant */` |
|       9 | 1906 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 | 1907 | `		if( rc == SXERR_ABORT ){` |
|       - | 1908 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1909 | `			return SXERR_ABORT;` |
|       - | 1910 | `		}` |
|       9 | 1911 | `		goto Synchronize;` |
|       - | 1912 | `	}` |
|      20 | 1913 | `	pGen->pIn++;` |
|      20 | 1914 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 1915 | `		/* Invalid statement*/` |
|       5 | 1916 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 | 1917 | `		if( rc == SXERR_ABORT ){` |
|       - | 1918 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1919 | `			return SXERR_ABORT;` |
|       - | 1920 | `		}` |
|       5 | 1921 | `		goto Synchronize;` |
|       - | 1922 | `	}` |
|      15 | 1923 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - | 1924 | `	/* Allocate a new constant value container */` |
|      15 | 1925 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 | 1926 | `	if( pConsCode == 0 ){` |
|     ! 0 | 1927 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1928 | `		return SXERR_ABORT;` |
|       - | 1929 | `	}` |
|      15 | 1930 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 1931 | `	/* Swap bytecode container */` |
|      15 | 1932 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 | 1933 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - | 1934 | `	/* Compile constant value */` |
|      15 | 1935 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 1936 | `	/* Emit the done instruction */` |
|      15 | 1937 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 | 1938 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 | 1939 | `	if( rc == SXERR_ABORT ){` |
|       - | 1940 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1941 | `		return SXERR_ABORT;` |
|       - | 1942 | `	}` |
|      15 | 1943 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - | 1944 | `	/* Register the constant with namespace-qualified name */` |
|       - | 1945 | `	{` |
|       - | 1946 | `		SyBlob sFQN;` |
|       - | 1947 | `		SyString sFQNStr;` |
|      15 | 1948 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 | 1949 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 | 1950 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 | 1951 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 | 1952 | `		SyBlobRelease(&sFQN);` |
|       - | 1953 | `	}` |
|      15 | 1954 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1955 | `		SySetRelease(pConsCode);` |
|     ! 0 | 1956 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 | 1957 | `	}` |
|      15 | 1958 | `	return SXRET_OK;` |
|       9 | 1959 | `Synchronize:` |
|       - | 1960 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 | 1961 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 | 1962 | `		pGen->pIn++;` |
|       1 | 1963 | `	}` |
|      19 | 1964 | `	return SXRET_OK;` |
|      18 | 1965 |  |
|       - | 1966 | `/*` |
|       - | 1967 | ` * Compile the 'continue' statement.` |
|       - | 1968 | ` * According to the PHP language reference` |
|       - | 1969 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - | 1970 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - | 1971 | ` *  iteration.` |
|       - | 1972 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - | 1973 | ` *  the purposes of continue.` |
|       - | 1974 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - | 1975 | ` *  of enclosing loops it should skip to the end of.` |
|       - | 1976 | ` *  Note:` |
|       - | 1977 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - | 1978 | ` */` |
|       - | 1979 | `/*` |
|       - | 1980 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - | 1981 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - | 1982 | ` * break/continue crosses a try boundary.` |
|       - | 1983 | ` *` |
|       - | 1984 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - | 1985 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - | 1986 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - | 1987 | ` */` |
|    2722 | 1988 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 | 1989 |  |
|    2724 | 1990 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   15894 | 1991 | `	while( pBlock && pBlock != pTarget ){` |
|   13172 | 1992 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 | 1993 | `			if( pBlock->pUserData ){` |
|       - | 1994 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 | 1995 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 | 1996 | `			}else{` |
|       - | 1997 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - | 1998 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - | 1999 | `				 * exception context from a sub-execution.` |
|       - | 2000 | `				 */` |
|     ! 0 | 2001 | `				break;` |
|       - | 2002 | `			}` |
|       1 | 2003 | `		}` |
|   13172 | 2004 | `		pBlock = pBlock->pParent;` |
|       2 | 2005 | `	}` |
|    2724 | 2006 |  |
|    2642 | 2007 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 2008 |  |
|       - | 2009 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2010 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2011 | `	sxu32 nLineLocal;` |
|       - | 2012 | `	sxi32 rc;` |
|    2644 | 2013 | `	nLineLocal = pGen->pIn->nLine;` |
|    2644 | 2014 | `	iLevel = 0;` |
|       - | 2015 | `	/* Jump the 'continue' keyword */` |
|    2644 | 2016 | `	pGen->pIn++;` |
|    2644 | 2017 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 2018 | `		/* optional numeric argument which tells us how many levels` |
|       - | 2019 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 2020 | `		 */` |
|      12 | 2021 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      12 | 2022 | `		if( iLevel < 2 ){` |
|     ! 0 | 2023 | `			iLevel = 0;` |
|     ! 0 | 2024 | `		}` |
|      12 | 2025 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 2026 | `	}` |
|       - | 2027 | `	/* Point to the target loop */` |
|    2644 | 2028 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2644 | 2029 | `	if( pLoop == 0 ){` |
|       - | 2030 | `		/* Illegal continue */` |
|      11 | 2031 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 2032 | `		if( rc == SXERR_ABORT ){` |
|       - | 2033 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2034 | `			return SXERR_ABORT;` |
|       - | 2035 | `		}` |
|       6 | 2036 | `	}else{` |
|    2634 | 2037 | `		sxu32 nInstrIdx = 0;` |
|       - | 2038 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2634 | 2039 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2634 | 2040 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - | 2041 | `			/* According to the PHP language reference manual` |
|       - | 2042 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - | 2043 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - | 2044 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - | 2045 | `			 */` |
|       5 | 2046 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 | 2047 | `			if( rc == SXRET_OK ){` |
|       5 | 2048 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 | 2049 | `			}` |
|       3 | 2050 | `		}else{` |
|       - | 2051 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    2630 | 2052 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2630 | 2053 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 2054 | `				JumpFixup sJumpFix;` |
|       - | 2055 | `				/* Post-continue */` |
|       9 | 2056 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       9 | 2057 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       9 | 2058 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       4 | 2059 | `			}` |
|       - | 2060 | `		}` |
|       - | 2061 | `	}` |
|    2644 | 2062 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2063 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2064 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 2065 | `	}` |
|       - | 2066 | `	/* Statement successfully compiled */` |
|    2644 | 2067 | `	return SXRET_OK;` |
|    1323 | 2068 |  |
|       - | 2069 | `/*` |
|       - | 2070 | ` * Compile the 'break' statement.` |
|       - | 2071 | ` * According to the PHP language reference` |
|       - | 2072 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - | 2073 | ` *  structure.` |
|       - | 2074 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - | 2075 | ` *  enclosing structures are to be broken out of.` |
|       - | 2076 | ` */` |
|     106 | 2077 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 | 2078 |  |
|       - | 2079 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2080 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2081 | `	sxi32 rc;` |
|     108 | 2082 | `	iLevel = 0;` |
|       - | 2083 | `	/* Jump the 'break' keyword */` |
|     108 | 2084 | `	pGen->pIn++;` |
|     108 | 2085 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 2086 | `		/* optional numeric argument which tells us how many levels` |
|       - | 2087 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 2088 | `		 */` |
|      12 | 2089 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      12 | 2090 | `		if( iLevel < 2 ){` |
|     ! 0 | 2091 | `			iLevel = 0;` |
|     ! 0 | 2092 | `		}` |
|      12 | 2093 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 2094 | `	}` |
|       - | 2095 | `	/* Extract the target loop */` |
|     108 | 2096 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     108 | 2097 | `	if( pLoop == 0 ){` |
|       - | 2098 | `		/* Illegal break */` |
|      17 | 2099 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 | 2100 | `		if( rc == SXERR_ABORT ){` |
|       - | 2101 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2102 | `			return SXERR_ABORT;` |
|       - | 2103 | `		}` |
|       9 | 2104 | `	}else{` |
|       - | 2105 | `		sxu32 nInstrIdx;` |
|       - | 2106 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|      92 | 2107 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|      92 | 2108 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      92 | 2109 | `		if( rc == SXRET_OK ){` |
|       - | 2110 | `			/* Fix the jump later when the jump destination is resolved */` |
|      92 | 2111 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      45 | 2112 | `		}` |
|       - | 2113 | `	}` |
|     108 | 2114 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2115 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2116 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 | 2117 | `	}` |
|       - | 2118 | `	/* Statement successfully compiled */` |
|     108 | 2119 | `	return SXRET_OK;` |
|      55 | 2120 |  |
|       - | 2121 | `/*` |
|       - | 2122 | ` * Compile or record a label.` |
|       - | 2123 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - | 2124 | ` * Example` |
|       - | 2125 | ` *  goto LABEL;` |
|       - | 2126 | ` *   echo 'Foo';` |
|       - | 2127 | ` *  LABEL:` |
|       - | 2128 | ` *   echo 'Bar';` |
|       - | 2129 | ` */` |
|     112 | 2130 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 | 2131 |  |
|       - | 2132 | `	GenBlock *pBlock;` |
|       - | 2133 | `	Label sLabel;` |
|       - | 2134 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 | 2135 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 | 2136 | `	if( pBlock ){` |
|       - | 2137 | `		sxi32 rc;` |
|       7 | 2138 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 | 2139 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 | 2140 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2141 | `			return SXERR_ABORT;` |
|       - | 2142 | `		}` |
|       3 | 2143 | `	}else{` |
|     110 | 2144 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 2145 | `		char *zDup;` |
|       - | 2146 | `		/* Initialize label fields */` |
|     110 | 2147 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2148 | `		/* Duplicate label name */` |
|     110 | 2149 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 | 2150 | `		if( zDup == 0 ){` |
|     ! 0 | 2151 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2152 | `			return SXERR_ABORT;` |
|       - | 2153 | `		}` |
|     110 | 2154 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 | 2155 | `		sLabel.bRef  = FALSE;` |
|     110 | 2156 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 | 2157 | `		pBlock = pGen->pCurrent;` |
|     218 | 2158 | `		while( pBlock ){` |
|     130 | 2159 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 | 2160 | `				break;` |
|       - | 2161 | `			}` |
|       - | 2162 | `			/* Point to the upper block */` |
|     110 | 2163 | `			pBlock = pBlock->pParent;` |
|       2 | 2164 | `		}` |
|     110 | 2165 | `		if( pBlock ){` |
|      22 | 2166 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 | 2167 | `		}else{` |
|      90 | 2168 | `			sLabel.pFunc = 0;` |
|       - | 2169 | `		}` |
|       - | 2170 | `		/* Insert in label set */` |
|     110 | 2171 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - | 2172 | `	}` |
|     114 | 2173 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 | 2174 | `	return SXRET_OK;` |
|      58 | 2175 |  |
|       - | 2176 | `/*` |
|       - | 2177 | ` * Compile the so hated 'goto' statement.` |
|       - | 2178 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - | 2179 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - | 2180 | ` * a compiler it has to do this.` |
|       - | 2181 | ` * According to the PHP language reference manual` |
|       - | 2182 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - | 2183 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - | 2184 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - | 2185 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - | 2186 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - | 2187 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - | 2188 | ` *   of a multi-level break` |
|       - | 2189 | ` */` |
|     152 | 2190 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 | 2191 |  |
|       - | 2192 | `	JumpFixup sJump;` |
|       - | 2193 | `	sxi32 rc;` |
|     154 | 2194 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 | 2195 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2196 | `		/* Missing label */` |
|     ! 0 | 2197 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 | 2198 | `		if( rc == SXERR_ABORT ){` |
|       - | 2199 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2200 | `			return SXERR_ABORT;` |
|       - | 2201 | `		}` |
|     ! 0 | 2202 | `		return SXRET_OK;` |
|       - | 2203 | `	}` |
|     154 | 2204 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 | 2205 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 | 2206 | `		if( rc == SXERR_ABORT ){` |
|       - | 2207 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2208 | `			return SXERR_ABORT;` |
|       - | 2209 | `		}` |
|       3 | 2210 | `	}else{` |
|     150 | 2211 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 2212 | `		GenBlock *pBlock;` |
|       - | 2213 | `		char *zDup;` |
|       - | 2214 | `		/* Prepare the jump destination */` |
|     150 | 2215 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 | 2216 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - | 2217 | `		/* Duplicate label name */` |
|     150 | 2218 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 | 2219 | `		if( zDup == 0 ){` |
|     ! 0 | 2220 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2221 | `			return SXERR_ABORT;` |
|       - | 2222 | `		}` |
|     150 | 2223 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 | 2224 | `		pBlock = pGen->pCurrent;` |
|     312 | 2225 | `		while( pBlock ){` |
|     196 | 2226 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 | 2227 | `				break;` |
|       - | 2228 | `			}` |
|       - | 2229 | `			/* Point to the upper block */` |
|     164 | 2230 | `			pBlock = pBlock->pParent;` |
|       2 | 2231 | `		}` |
|     150 | 2232 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 | 2233 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 | 2234 | `			if( rc == SXERR_ABORT ){` |
|       - | 2235 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2236 | `				return SXERR_ABORT;` |
|       - | 2237 | `			}` |
|       3 | 2238 | `		}` |
|     150 | 2239 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 | 2240 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 | 2241 | `		}else{` |
|     124 | 2242 | `			sJump.pFunc = 0;` |
|       - | 2243 | `		}` |
|       - | 2244 | `		/* Emit the unconditional jump */` |
|     150 | 2245 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 | 2246 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 | 2247 | `		}` |
|       - | 2248 | `	}` |
|     154 | 2249 | `	pGen->pIn++; /* Jump the label name */` |
|     154 | 2250 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 | 2251 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 | 2252 | `	}` |
|       - | 2253 | `	/* Statement successfully compiled */` |
|     154 | 2254 | `	return SXRET_OK;` |
|      78 | 2255 |  |
|       - | 2256 | `/*` |
|       - | 2257 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - | 2258 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - | 2259 | ` * failure.` |
|       - | 2260 | ` */` |
|      20 | 2261 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 | 2262 |  |
|       - | 2263 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - | 2264 | `	sxu32 nRawObj;` |
|      10 | 2265 | `	sxu32 nObjIdx;` |
|       - | 2266 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - | 2267 | `	 * a PHP block.` |
|       - | 2268 | `	 */` |
|      10 | 2269 | `Consume:` |
|      21 | 2270 | `	nRawObj = nObjIdx = 0;` |
|      21 | 2271 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 | 2272 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 | 2273 | `		if( pRawObj == 0 ){` |
|     ! 0 | 2274 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2275 | `			return SXERR_ABORT;` |
|       - | 2276 | `		}` |
|       - | 2277 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 | 2278 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 | 2279 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 | 2280 | `		++nRawObj;` |
|     ! 0 | 2281 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 | 2282 | `	}` |
|      21 | 2283 | `	if( nRawObj > 0 ){` |
|       - | 2284 | `		/* Emit the consume instruction */` |
|     ! 0 | 2285 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 | 2286 | `	}` |
|      21 | 2287 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 | 2288 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - | 2289 | `		/* Reset the token set */` |
|     ! 0 | 2290 | `		SySetReset(pTokenSet);` |
|       - | 2291 | `		/* Tokenize input */` |
|     ! 0 | 2292 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 | 2293 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - | 2294 | `		/* Point to the fresh token stream */` |
|     ! 0 | 2295 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 | 2296 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - | 2297 | `		/* Advance the stream cursor */` |
|     ! 0 | 2298 | `		pGen->pRawIn++;` |
|       - | 2299 | `		/* TICKET 1433-011 */` |
|     ! 0 | 2300 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 2301 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 2302 | `			sxi32 rc;` |
|       - | 2303 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 | 2304 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 | 2305 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 | 2306 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 | 2307 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 2308 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2309 | `				return SXERR_ABORT;` |
|     ! 0 | 2310 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 | 2311 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 2312 | `			}` |
|     ! 0 | 2313 | `			goto Consume;` |
|       - | 2314 | `		}` |
|     ! 0 | 2315 | `	}else{` |
|       - | 2316 | `		/* No more chunks to process */` |
|      21 | 2317 | `		pGen->pIn = pGen->pEnd;` |
|      21 | 2318 | `		return SXERR_EOF;` |
|       - | 2319 | `	}` |
|     ! 0 | 2320 | `	return SXRET_OK;` |
|      11 | 2321 |  |
|       - | 2322 | `/*` |
|       - | 2323 | ` * Compile a PHP block.` |
|       - | 2324 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - | 2325 | ` * optionally delimited by braces {}.` |
|       - | 2326 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 2327 | ` * and this function takes care of generating the appropriate error` |
|       - | 2328 | ` * message.` |
|       - | 2329 | ` */` |
|  286696 | 2330 | `static sxi32 PH7_CompileBlock(` |
|       - | 2331 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2332 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2333 | `	)` |
|       2 | 2334 |  |
|       - | 2335 | `	sxi32 rc;` |
|       - | 2336 | `	sxu32 nLine;` |
|  286698 | 2337 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  285302 | 2338 | `		nLine = pGen->pIn->nLine;` |
|  285302 | 2339 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  285302 | 2340 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2341 | `			return SXERR_ABORT;` |
|       - | 2342 | `		}` |
|  285302 | 2343 | `		pGen->pIn++;` |
|       - | 2344 | `		/* Compile until we hit the closing braces '}' */` |
|  393871 | 2345 | `		for(;;){` |
|  787744 | 2346 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 | 2347 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 | 2348 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2349 | `			 	   return SXERR_ABORT;` |
|       - | 2350 | `				}` |
|      21 | 2351 | `				if( rc == SXERR_EOF ){` |
|       - | 2352 | `					/* No more token to process. Missing closing braces */` |
|      21 | 2353 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 | 2354 | `					break;` |
|       - | 2355 | `				}` |
|     ! 0 | 2356 | `			}` |
|  787724 | 2357 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2358 | `				/* Closing braces found,break immediately*/` |
|  285282 | 2359 | `				pGen->pIn++;` |
|  285282 | 2360 | `				break;` |
|       - | 2361 | `			}` |
|       - | 2362 | `			/* Compile a single statement */` |
|  502444 | 2363 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  502444 | 2364 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2365 | `				return SXERR_ABORT;` |
|       - | 2366 | `			}` |
|       2 | 2367 | `		}` |
|  285302 | 2368 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  144048 | 2369 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 | 2370 | `		pGen->pIn++;` |
|     ! 0 | 2371 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 | 2372 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2373 | `			return SXERR_ABORT;` |
|       - | 2374 | `		}` |
|       - | 2375 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 | 2376 | `		for(;;){` |
|     ! 0 | 2377 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2378 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 | 2379 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2380 | `			 	   return SXERR_ABORT;` |
|       - | 2381 | `				}` |
|     ! 0 | 2382 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - | 2383 | `					/* No more token to process */` |
|     ! 0 | 2384 | `					if( rc == SXERR_EOF ){` |
|     ! 0 | 2385 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - | 2386 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 | 2387 | `					}` |
|     ! 0 | 2388 | `					break;` |
|       - | 2389 | `				}` |
|     ! 0 | 2390 | `			}` |
|     ! 0 | 2391 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 2392 | `				sxi32 nKwrd;` |
|       - | 2393 | `				/* Keyword found */` |
|     ! 0 | 2394 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 | 2395 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 | 2396 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - | 2397 | `						/* Delimiter keyword found,break */` |
|     ! 0 | 2398 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 | 2399 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 | 2400 | `						}` |
|     ! 0 | 2401 | `						break;` |
|       - | 2402 | `				}` |
|     ! 0 | 2403 | `			}` |
|       - | 2404 | `			/* Compile a single statement */` |
|     ! 0 | 2405 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 | 2406 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2407 | `				return SXERR_ABORT;` |
|       - | 2408 | `			}` |
|     ! 0 | 2409 | `		}` |
|     ! 0 | 2410 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 | 2411 | `	}else{` |
|       - | 2412 | `		/* Compile a single statement */` |
|    1398 | 2413 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1398 | 2414 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2415 | `			return SXERR_ABORT;` |
|       - | 2416 | `		}` |
|       - | 2417 | `	}` |
|       - | 2418 | `	/* Jump trailing semi-colons ';' */` |
|  286698 | 2419 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2420 | `		pGen->pIn++;` |
|     ! 0 | 2421 | `	}` |
|  286698 | 2422 | `	return SXRET_OK;` |
|  143350 | 2423 |  |
|       - | 2424 | `/*` |
|       - | 2425 | ` * Compile the gentle 'while' statement.` |
|       - | 2426 | ` * According to the PHP language reference` |
|       - | 2427 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - | 2428 | ` *  The basic form of a while statement is:` |
|       - | 2429 | ` *  while (expr)` |
|       - | 2430 | ` *   statement` |
|       - | 2431 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - | 2432 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - | 2433 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - | 2434 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - | 2435 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - | 2436 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - | 2437 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - | 2438 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - | 2439 | ` *  while (expr):` |
|       - | 2440 | ` *    statement` |
|       - | 2441 | ` *   endwhile;` |
|       - | 2442 | ` */` |
|   10510 | 2443 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 2444 |  |
|   10512 | 2445 | `	GenBlock *pWhileBlock = 0;` |
|   10512 | 2446 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2447 | `	sxu32 nFalseJump;` |
|       - | 2448 | `	sxu32 nLine;` |
|       - | 2449 | `	sxi32 rc;` |
|   10512 | 2450 | `	nLine = pGen->pIn->nLine;` |
|       - | 2451 | `	/* Jump the 'while' keyword */` |
|   10512 | 2452 | `	pGen->pIn++;` |
|   10512 | 2453 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2454 | `		/* Syntax error */` |
|     ! 0 | 2455 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2456 | `		if( rc == SXERR_ABORT ){` |
|       - | 2457 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2458 | `			return SXERR_ABORT;` |
|       - | 2459 | `		}` |
|     ! 0 | 2460 | `		goto Synchronize;` |
|       - | 2461 | `	}` |
|       - | 2462 | `	/* Jump the left parenthesis '(' */` |
|   10512 | 2463 | `	pGen->pIn++;` |
|       - | 2464 | `	/* Create the loop block */` |
|   10512 | 2465 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   10512 | 2466 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2467 | `		return SXERR_ABORT;` |
|       - | 2468 | `	}` |
|       - | 2469 | `	/* Delimit the condition */` |
|   10512 | 2470 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10512 | 2471 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2472 | `		/* Empty expression */` |
|       3 | 2473 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2474 | `		if( rc == SXERR_ABORT ){` |
|       - | 2475 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2476 | `			return SXERR_ABORT;` |
|       - | 2477 | `		}` |
|       1 | 2478 | `	}` |
|       - | 2479 | `	/* Swap token streams */` |
|   10512 | 2480 | `	pTmp = pGen->pEnd;` |
|   10512 | 2481 | `	pGen->pEnd = pEnd;` |
|       - | 2482 | `	/* Compile the expression */` |
|   10512 | 2483 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10512 | 2484 | `	if( rc == SXERR_ABORT ){` |
|       - | 2485 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2486 | `		return SXERR_ABORT;` |
|       - | 2487 | `	}` |
|       - | 2488 | `	/* Update token stream */` |
|   10512 | 2489 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2490 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2491 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2492 | `			return SXERR_ABORT;` |
|       - | 2493 | `		}` |
|     ! 0 | 2494 | `		pGen->pIn++;` |
|     ! 0 | 2495 | `	}` |
|       - | 2496 | `	/* Synchronize pointers */` |
|   10512 | 2497 | `	pGen->pIn  = &pEnd[1];` |
|   10512 | 2498 | `	pGen->pEnd = pTmp;` |
|       - | 2499 | `	/* Emit the false jump */` |
|   10512 | 2500 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2501 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10512 | 2502 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2503 | `	/* Compile the loop body */` |
|   10512 | 2504 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   10512 | 2505 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2506 | `		return SXERR_ABORT;` |
|       - | 2507 | `	}` |
|       - | 2508 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10512 | 2509 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2510 | `	/* Fix all jumps now the destination is resolved */` |
|   10512 | 2511 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2512 | `	/* Release the loop block */` |
|   10512 | 2513 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2514 | `	/* Statement successfully compiled */` |
|   10512 | 2515 | `	return SXRET_OK;` |
|     ! 0 | 2516 | `Synchronize:` |
|       - | 2517 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2518 | `	 * compiling this erroneous block.` |
|       - | 2519 | `	 */` |
|     ! 0 | 2520 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2521 | `		pGen->pIn++;` |
|     ! 0 | 2522 | `	}` |
|     ! 0 | 2523 | `	return SXRET_OK;` |
|    5257 | 2524 |  |
|       - | 2525 | `/*` |
|       - | 2526 | ` * Compile the ugly do..while() statement.` |
|       - | 2527 | ` * According to the PHP language reference` |
|       - | 2528 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - | 2529 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - | 2530 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - | 2531 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - | 2532 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - | 2533 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - | 2534 | ` *  would end immediately).` |
|       - | 2535 | ` *  There is just one syntax for do-while loops:` |
|       - | 2536 | ` *  <?php` |
|       - | 2537 | ` *  $i = 0;` |
|       - | 2538 | ` *  do {` |
|       - | 2539 | ` *   echo $i;` |
|       - | 2540 | ` *  } while ($i > 0);` |
|       - | 2541 | ` * ?>` |
|       - | 2542 | ` */` |
|       2 | 2543 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 | 2544 |  |
|       3 | 2545 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 | 2546 | `	GenBlock *pDoBlock = 0;` |
|       - | 2547 | `	sxu32 nLine;` |
|       - | 2548 | `	sxi32 rc;` |
|       3 | 2549 | `	nLine = pGen->pIn->nLine;` |
|       - | 2550 | `	/* Jump the 'do' keyword */` |
|       3 | 2551 | `	pGen->pIn++;` |
|       - | 2552 | `	/* Create the loop block */` |
|       3 | 2553 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 | 2554 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2555 | `		return SXERR_ABORT;` |
|       - | 2556 | `	}` |
|       - | 2557 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 | 2558 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 | 2559 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 | 2560 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2561 | `		return SXERR_ABORT;` |
|       - | 2562 | `	}` |
|       3 | 2563 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2564 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 | 2565 | `	}` |
|       3 | 2566 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 | 2567 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - | 2568 | `			/* Missing 'while' statement */` |
|       3 | 2569 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 | 2570 | `			if( rc == SXERR_ABORT ){` |
|       - | 2571 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2572 | `				return SXERR_ABORT;` |
|       - | 2573 | `			}` |
|       3 | 2574 | `			goto Synchronize;` |
|       - | 2575 | `	}` |
|       - | 2576 | `	/* Jump the 'while' keyword */` |
|     ! 0 | 2577 | `	pGen->pIn++;` |
|     ! 0 | 2578 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2579 | `		/* Syntax error */` |
|     ! 0 | 2580 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2581 | `		if( rc == SXERR_ABORT ){` |
|       - | 2582 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2583 | `			return SXERR_ABORT;` |
|       - | 2584 | `		}` |
|     ! 0 | 2585 | `		goto Synchronize;` |
|       - | 2586 | `	}` |
|       - | 2587 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 | 2588 | `	pGen->pIn++;` |
|       - | 2589 | `	/* Delimit the condition */` |
|     ! 0 | 2590 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 | 2591 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2592 | `		/* Empty expression */` |
|     ! 0 | 2593 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 | 2594 | `		if( rc == SXERR_ABORT ){` |
|       - | 2595 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2596 | `			return SXERR_ABORT;` |
|       - | 2597 | `		}` |
|     ! 0 | 2598 | `		goto Synchronize;` |
|       - | 2599 | `	}` |
|       - | 2600 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 | 2601 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - | 2602 | `		JumpFixup *aPost;` |
|       - | 2603 | `		VmInstr *pInstr;` |
|       - | 2604 | `		sxu32 nJumpDest;` |
|       - | 2605 | `		sxu32 n;` |
|     ! 0 | 2606 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 | 2607 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 | 2608 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 | 2609 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 | 2610 | `			if( pInstr ){` |
|       - | 2611 | `				/* Fix */` |
|     ! 0 | 2612 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 | 2613 | `			}` |
|     ! 0 | 2614 | `		}` |
|     ! 0 | 2615 | `	}` |
|       - | 2616 | `	/* Swap token streams */` |
|     ! 0 | 2617 | `	pTmp = pGen->pEnd;` |
|     ! 0 | 2618 | `	pGen->pEnd = pEnd;` |
|       - | 2619 | `	/* Compile the expression */` |
|     ! 0 | 2620 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 2621 | `	if( rc == SXERR_ABORT ){` |
|       - | 2622 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2623 | `		return SXERR_ABORT;` |
|       - | 2624 | `	}` |
|       - | 2625 | `	/* Update token stream */` |
|     ! 0 | 2626 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2627 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2628 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2629 | `			return SXERR_ABORT;` |
|       - | 2630 | `		}` |
|     ! 0 | 2631 | `		pGen->pIn++;` |
|     ! 0 | 2632 | `	}` |
|     ! 0 | 2633 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 | 2634 | `	pGen->pEnd = pTmp;` |
|       - | 2635 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 | 2636 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - | 2637 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 | 2638 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2639 | `	/* Release the loop block */` |
|     ! 0 | 2640 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2641 | `	/* Statement successfully compiled */` |
|     ! 0 | 2642 | `	return SXRET_OK;` |
|       1 | 2643 | `Synchronize:` |
|       - | 2644 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2645 | `	 * compiling this erroneous block.` |
|       - | 2646 | `	 */` |
|       3 | 2647 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2648 | `		pGen->pIn++;` |
|     ! 0 | 2649 | `	}` |
|       3 | 2650 | `	return SXRET_OK;` |
|       2 | 2651 |  |
|       - | 2652 | `/*` |
|       - | 2653 | ` * Compile the complex and powerful 'for' statement.` |
|       - | 2654 | ` * According to the PHP language reference` |
|       - | 2655 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - | 2656 | ` *  The syntax of a for loop is:` |
|       - | 2657 | ` *  for (expr1; expr2; expr3)` |
|       - | 2658 | ` *   statement` |
|       - | 2659 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - | 2660 | ` *  the beginning of the loop.` |
|       - | 2661 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - | 2662 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - | 2663 | ` *  to FALSE, the execution of the loop ends.` |
|       - | 2664 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - | 2665 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - | 2666 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - | 2667 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - | 2668 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - | 2669 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - | 2670 | ` *  of using the for truth expression.` |
|       - | 2671 | ` */` |
|   10494 | 2672 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 2673 |  |
|   10496 | 2674 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   10496 | 2675 | `	GenBlock *pForBlock = 0;` |
|       - | 2676 | `	sxu32 nFalseJump;` |
|       - | 2677 | `	sxu32 nLine;` |
|       - | 2678 | `	sxi32 rc;` |
|   10496 | 2679 | `	nLine = pGen->pIn->nLine;` |
|       - | 2680 | `	/* Jump the 'for' keyword */` |
|   10496 | 2681 | `	pGen->pIn++;` |
|   10496 | 2682 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2683 | `		/* Syntax error */` |
|     ! 0 | 2684 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2685 | `		if( rc == SXERR_ABORT ){` |
|       - | 2686 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2687 | `			return SXERR_ABORT;` |
|       - | 2688 | `		}` |
|     ! 0 | 2689 | `		return SXRET_OK;` |
|       - | 2690 | `	}` |
|       - | 2691 | `	/* Jump the left parenthesis '(' */` |
|   10496 | 2692 | `	pGen->pIn++;` |
|       - | 2693 | `	/* Delimit the init-expr;condition;post-expr */` |
|   10496 | 2694 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10496 | 2695 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2696 | `		/* Empty expression */` |
|     ! 0 | 2697 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 | 2698 | `		if( rc == SXERR_ABORT ){` |
|       - | 2699 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2700 | `			return SXERR_ABORT;` |
|       - | 2701 | `		}` |
|       - | 2702 | `		/* Synchronize */` |
|     ! 0 | 2703 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2704 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2705 | `			pGen->pIn++;` |
|     ! 0 | 2706 | `		}` |
|     ! 0 | 2707 | `		return SXRET_OK;` |
|       - | 2708 | `	}` |
|       - | 2709 | `	/* Swap token streams */` |
|   10496 | 2710 | `	pTmp = pGen->pEnd;` |
|   10496 | 2711 | `	pGen->pEnd = pEnd;` |
|       - | 2712 | `	/* Compile initialization expressions if available */` |
|   10496 | 2713 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2714 | `	/* Pop operand lvalues */` |
|   10496 | 2715 | `	if( rc == SXERR_ABORT ){` |
|       - | 2716 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2717 | `		return SXERR_ABORT;` |
|   10496 | 2718 | `	}else if( rc != SXERR_EMPTY ){` |
|   10494 | 2719 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5246 | 2720 | `	}` |
|   10496 | 2721 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2722 | `		/* Syntax error */` |
|     ! 0 | 2723 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2724 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 | 2725 | `		if( rc == SXERR_ABORT ){` |
|       - | 2726 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2727 | `			return SXERR_ABORT;` |
|       - | 2728 | `		}` |
|     ! 0 | 2729 | `		return SXRET_OK;` |
|       - | 2730 | `	}` |
|       - | 2731 | `	/* Jump the trailing ';' */` |
|   10496 | 2732 | `	pGen->pIn++;` |
|       - | 2733 | `	/* Create the loop block */` |
|   10496 | 2734 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   10496 | 2735 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2736 | `		return SXERR_ABORT;` |
|       - | 2737 | `	}` |
|       - | 2738 | `	/* Deffer continue jumps */` |
|   10496 | 2739 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 2740 | `	/* Compile the condition */` |
|   10496 | 2741 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10496 | 2742 | `	if( rc == SXERR_ABORT ){` |
|       - | 2743 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2744 | `		return SXERR_ABORT;` |
|   10496 | 2745 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 2746 | `		/* Emit the false jump */` |
|   10494 | 2747 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2748 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10494 | 2749 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5246 | 2750 | `	}` |
|   10496 | 2751 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2752 | `		/* Syntax error */` |
|       5 | 2753 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2754 | `			"for: Expected ';' after conditionals expressions");` |
|       5 | 2755 | `		if( rc == SXERR_ABORT ){` |
|       - | 2756 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2757 | `			return SXERR_ABORT;` |
|       - | 2758 | `		}` |
|       5 | 2759 | `		return SXRET_OK;` |
|       - | 2760 | `	}` |
|       - | 2761 | `	/* Jump the trailing ';' */` |
|   10492 | 2762 | `	pGen->pIn++;` |
|       - | 2763 | `	/* Save the post condition stream */` |
|   10492 | 2764 | `	pPostStart = pGen->pIn;` |
|       - | 2765 | `	/* Compile the loop body */` |
|   10492 | 2766 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   10492 | 2767 | `	pGen->pEnd = pTmp;` |
|   10492 | 2768 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   10492 | 2769 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2770 | `		return SXERR_ABORT;` |
|       - | 2771 | `	}` |
|       - | 2772 | `	/* Fix post-continue jumps */` |
|   10492 | 2773 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 2774 | `		JumpFixup *aPost;` |
|       - | 2775 | `		VmInstr *pInstr;` |
|       - | 2776 | `		sxu32 nJumpDest;` |
|       - | 2777 | `		sxu32 n;` |
|       9 | 2778 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|       9 | 2779 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      17 | 2780 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|       9 | 2781 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       9 | 2782 | `			if( pInstr ){` |
|       - | 2783 | `				/* Fix jump */` |
|       9 | 2784 | `				pInstr->iP2 = nJumpDest;` |
|       4 | 2785 | `			}` |
|       5 | 2786 | `		}` |
|       4 | 2787 | `	}` |
|       - | 2788 | `	/* compile the post-expressions if available */` |
|   10492 | 2789 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2790 | `		pPostStart++;` |
|     ! 0 | 2791 | `	}` |
|   10492 | 2792 | `	if( pPostStart < pEnd ){` |
|       - | 2793 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   10492 | 2794 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   10492 | 2795 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10492 | 2796 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 2797 | `			/* Syntax error */` |
|     ! 0 | 2798 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 2799 | `			if( rc == SXERR_ABORT ){` |
|       - | 2800 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2801 | `				return SXERR_ABORT;` |
|       - | 2802 | `			}` |
|     ! 0 | 2803 | `			return SXRET_OK;` |
|       - | 2804 | `		}` |
|   10492 | 2805 | `		RE_SWAP_DELIMITER(pGen);` |
|   10492 | 2806 | `		if( rc == SXERR_ABORT ){` |
|       - | 2807 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2808 | `			return SXERR_ABORT;` |
|   10492 | 2809 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 2810 | `			/* Pop operand lvalue */` |
|   10492 | 2811 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5245 | 2812 | `		}` |
|    5245 | 2813 | `	}` |
|       - | 2814 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10492 | 2815 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 2816 | `	/* Fix all jumps now the destination is resolved */` |
|   10492 | 2817 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2818 | `	/* Release the loop block */` |
|   10492 | 2819 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2820 | `	/* Statement successfully compiled */` |
|   10492 | 2821 | `	return SXRET_OK;` |
|    5249 | 2822 |  |
|       - | 2823 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 2824 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 2825 | ` * are allowed.` |
|       - | 2826 | ` */` |
|    5600 | 2827 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 2828 |  |
|    5602 | 2829 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    5602 | 2830 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 2831 | `		/* Unexpected expression */` |
|     ! 0 | 2832 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 2833 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 2834 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 2835 | `			rc = SXERR_INVALID;` |
|     ! 0 | 2836 | `		}` |
|     ! 0 | 2837 | `	}` |
|    5602 | 2838 | `	return rc;` |
|       2 | 2839 |  |
|       - | 2840 | `/*` |
|       - | 2841 | ` * Compile the 'foreach' statement.` |
|       - | 2842 | ` * According to the PHP language reference` |
|       - | 2843 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - | 2844 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - | 2845 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - | 2846 | ` *  is a minor but useful extension of the first:` |
|       - | 2847 | ` *  foreach (array_expression as $value)` |
|       - | 2848 | ` *    statement` |
|       - | 2849 | ` *  foreach (array_expression as $key => $value)` |
|       - | 2850 | ` *   statement` |
|       - | 2851 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - | 2852 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - | 2853 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - | 2854 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - | 2855 | ` *  to the variable $key on each loop.` |
|       - | 2856 | ` *  Note:` |
|       - | 2857 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - | 2858 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - | 2859 | ` *  Note:` |
|       - | 2860 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - | 2861 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - | 2862 | ` *  or after the foreach without resetting it.` |
|       - | 2863 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - | 2864 | ` *  of copying the value.` |
|       - | 2865 | ` */` |
|    2850 | 2866 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 2867 |  |
|    2852 | 2868 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    2852 | 2869 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    2852 | 2870 | `	GenBlock *pForeachBlock = 0;` |
|       - | 2871 | `	ph7_foreach_info *pInfo;` |
|       - | 2872 | `	sxu32 nFalseJump;` |
|       - | 2873 | `	VmInstr *pInstr;` |
|       - | 2874 | `	sxu32 nLine;` |
|       - | 2875 | `	sxi32 rc;` |
|    2852 | 2876 | `	nLine = pGen->pIn->nLine;` |
|       - | 2877 | `	/* Jump the 'foreach' keyword */` |
|    2852 | 2878 | `	pGen->pIn++;` |
|    2852 | 2879 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2880 | `		/* Syntax error */` |
|     ! 0 | 2881 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 2882 | `		if( rc == SXERR_ABORT ){` |
|       - | 2883 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2884 | `			return SXERR_ABORT;` |
|       - | 2885 | `		}` |
|     ! 0 | 2886 | `		goto Synchronize;` |
|       - | 2887 | `	}` |
|       - | 2888 | `	/* Jump the left parenthesis '(' */` |
|    2852 | 2889 | `	pGen->pIn++;` |
|       - | 2890 | `	/* Create the loop block */` |
|    2852 | 2891 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    2852 | 2892 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2893 | `		return SXERR_ABORT;` |
|       - | 2894 | `	}` |
|       - | 2895 | `	/* Delimit the expression */` |
|    2852 | 2896 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    2852 | 2897 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2898 | `		/* Empty expression */` |
|     ! 0 | 2899 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 2900 | `		if( rc == SXERR_ABORT ){` |
|       - | 2901 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2902 | `			return SXERR_ABORT;` |
|       - | 2903 | `		}` |
|       - | 2904 | `		/* Synchronize */` |
|     ! 0 | 2905 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2906 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2907 | `			pGen->pIn++;` |
|     ! 0 | 2908 | `		}` |
|     ! 0 | 2909 | `		return SXRET_OK;` |
|       - | 2910 | `	}` |
|       - | 2911 | `	/* Compile the array expression */` |
|    2852 | 2912 | `	pCur = pGen->pIn;` |
|   19066 | 2913 | `	while( pCur < pEnd ){` |
|   19066 | 2914 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    2862 | 2915 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    2862 | 2916 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 2917 | `				/* Break with the first 'as' found */` |
|    2852 | 2918 | `				break;` |
|       - | 2919 | `			}` |
|       5 | 2920 | `		}` |
|       - | 2921 | `		/* Advance the stream cursor */` |
|   16216 | 2922 | `		pCur++;` |
|       2 | 2923 | `	}` |
|    2852 | 2924 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 2925 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2926 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 2927 | `		if( rc == SXERR_ABORT ){` |
|       - | 2928 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2929 | `			return SXERR_ABORT;` |
|       - | 2930 | `		}` |
|     ! 0 | 2931 | `		goto Synchronize;` |
|       - | 2932 | `	}` |
|       - | 2933 | `	/* Swap token streams */` |
|    2852 | 2934 | `	pTmp = pGen->pEnd;` |
|    2852 | 2935 | `	pGen->pEnd = pCur;` |
|    2852 | 2936 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    2852 | 2937 | `	if( rc == SXERR_ABORT ){` |
|       - | 2938 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2939 | `		return SXERR_ABORT;` |
|       - | 2940 | `	}` |
|       - | 2941 | `	/* Update token stream */` |
|    2852 | 2942 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 2943 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2944 | `		if( rc == SXERR_ABORT ){` |
|       - | 2945 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2946 | `			return SXERR_ABORT;` |
|       - | 2947 | `		}` |
|     ! 0 | 2948 | `		pGen->pIn++;` |
|     ! 0 | 2949 | `	}` |
|    2852 | 2950 | `	pCur++; /* Jump the 'as' keyword */` |
|    2852 | 2951 | `	pGen->pIn = pCur;` |
|    2852 | 2952 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2953 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 2954 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2955 | `			return SXERR_ABORT;` |
|       - | 2956 | `		}` |
|     ! 0 | 2957 | `	}` |
|       - | 2958 | `	/* Create the foreach context */` |
|    2852 | 2959 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    2852 | 2960 | `	if( pInfo == 0 ){` |
|     ! 0 | 2961 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 2962 | `		return SXERR_ABORT;` |
|       - | 2963 | `	}` |
|       - | 2964 | `	/* Zero the structure */` |
|    2852 | 2965 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 2966 | `	/* Initialize structure fields */` |
|    2852 | 2967 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 2968 | `	/* Check if we have a key field */` |
|    8602 | 2969 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    5752 | 2970 | `		pCur++;` |
|       2 | 2971 | `	}` |
|    2852 | 2972 | `	if( pCur < pEnd ){` |
|       - | 2973 | `		/* Compile the expression holding the key name */` |
|    2762 | 2974 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 2975 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 2976 | `			if( rc == SXERR_ABORT ){` |
|       - | 2977 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2978 | `				return SXERR_ABORT;` |
|       - | 2979 | `			}` |
|     ! 0 | 2980 | `		}else{` |
|    2762 | 2981 | `			pGen->pEnd = pCur;` |
|    2762 | 2982 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2762 | 2983 | `			if( rc == SXERR_ABORT ){` |
|       - | 2984 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2985 | `				return SXERR_ABORT;` |
|       - | 2986 | `			}` |
|    2762 | 2987 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2762 | 2988 | `			if( pInstr->p3 ){` |
|       - | 2989 | `				/* Record key name */` |
|    2762 | 2990 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1380 | 2991 | `			}` |
|    2762 | 2992 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 2993 | `		}` |
|    2762 | 2994 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1380 | 2995 | `	}` |
|    2852 | 2996 | `	pGen->pEnd = pEnd;` |
|    2852 | 2997 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2998 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 2999 | `		if( rc == SXERR_ABORT ){` |
|       - | 3000 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3001 | `			return SXERR_ABORT;` |
|       - | 3002 | `		}` |
|     ! 0 | 3003 | `		goto Synchronize;` |
|       - | 3004 | `	}` |
|    2852 | 3005 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 | 3006 | `		pGen->pIn++;` |
|       - | 3007 | `		/* Pass by reference  */` |
|      11 | 3008 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 | 3009 | `	}` |
|       - | 3010 | `	/* Check if the value target is list() */` |
|    2852 | 3011 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 3012 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 3013 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - | 3014 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - | 3015 | `		 */` |
|       - | 3016 | `		static int iForeachListCnt = 0;` |
|       - | 3017 | `		char zTmp[128];` |
|       - | 3018 | `		sxu32 nLen;` |
|       - | 3019 | `		char *zDup;` |
|      10 | 3020 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 | 3021 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 | 3022 | `		if( zDup == 0 ){` |
|     ! 0 | 3023 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3024 | `			return SXERR_ABORT;` |
|       - | 3025 | `		}` |
|      10 | 3026 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 3027 | `		/* Save list() token boundaries */` |
|      10 | 3028 | `		pListStart = pGen->pIn;` |
|       - | 3029 | `		/* Advance past list(...) — validate parentheses */` |
|      10 | 3030 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 | 3031 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 | 3032 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - | 3033 | `				"foreach: Expected '(' after 'list'");` |
|       3 | 3034 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3035 | `				return SXERR_ABORT;` |
|       - | 3036 | `			}` |
|       3 | 3037 | `			goto Synchronize;` |
|       - | 3038 | `		}` |
|       7 | 3039 | `		pGen->pIn++; /* Jump '(' */` |
|       7 | 3040 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 | 3041 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 3042 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3043 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 | 3044 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3045 | `				return SXERR_ABORT;` |
|       - | 3046 | `			}` |
|     ! 0 | 3047 | `			goto Synchronize;` |
|       - | 3048 | `		}` |
|       7 | 3049 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 | 3050 | `		pListEnd = pGen->pIn;` |
|       7 | 3051 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    2847 | 3052 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 3053 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - | 3054 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - | 3055 | `		 */` |
|       - | 3056 | `		static int iForeachShortListCnt = 0;` |
|       - | 3057 | `		char zTmp[128];` |
|       - | 3058 | `		sxu32 nLen;` |
|       - | 3059 | `		char *zDup;` |
|       3 | 3060 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 | 3061 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 | 3062 | `		if( zDup == 0 ){` |
|     ! 0 | 3063 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3064 | `			return SXERR_ABORT;` |
|       - | 3065 | `		}` |
|       3 | 3066 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 3067 | `		/* Save [...] token boundaries */` |
|       3 | 3068 | `		pListStart = pGen->pIn;` |
|       - | 3069 | `		/* Advance past [...] */` |
|       3 | 3070 | `		pGen->pIn++; /* Jump '[' */` |
|       3 | 3071 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 | 3072 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 3073 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3074 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 | 3075 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3076 | `				return SXERR_ABORT;` |
|       - | 3077 | `			}` |
|     ! 0 | 3078 | `			goto Synchronize;` |
|       - | 3079 | `		}` |
|       3 | 3080 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 | 3081 | `		pListEnd = pGen->pIn;` |
|       3 | 3082 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 | 3083 | `	}else{` |
|       - | 3084 | `		/* Compile the expression holding the value name */` |
|    2842 | 3085 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2842 | 3086 | `		if( rc == SXERR_ABORT ){` |
|       - | 3087 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3088 | `			return SXERR_ABORT;` |
|       - | 3089 | `		}` |
|    2842 | 3090 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2842 | 3091 | `		if( pInstr->p3 ){` |
|       - | 3092 | `			/* Record value name */` |
|    2842 | 3093 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1420 | 3094 | `		}` |
|       - | 3095 | `	}` |
|       - | 3096 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    2850 | 3097 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 3098 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2850 | 3099 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 3100 | `	/* Record the first instruction to execute */` |
|    2850 | 3101 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3102 | `	/* Emit the FOREACH_STEP instruction */` |
|    2850 | 3103 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 3104 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2850 | 3105 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 3106 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    2850 | 3107 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - | 3108 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - | 3109 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - | 3110 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - | 3111 | `		 */` |
|       9 | 3112 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - | 3113 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - | 3114 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - | 3115 | `		 * picks up the delimiter and the variable names inside.` |
|       - | 3116 | `		 */` |
|       9 | 3117 | `		pSavedIn = pGen->pIn;` |
|       9 | 3118 | `		pSavedEnd = pGen->pEnd;` |
|       9 | 3119 | `		pGen->pIn = pListStart;` |
|       9 | 3120 | `		pGen->pEnd = pListEnd;` |
|       9 | 3121 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 | 3122 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 | 3123 | `		}else{` |
|       7 | 3124 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - | 3125 | `		}` |
|       9 | 3126 | `		pGen->pIn = pSavedIn;` |
|       9 | 3127 | `		pGen->pEnd = pSavedEnd;` |
|       9 | 3128 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3129 | `			return SXERR_ABORT;` |
|       - | 3130 | `		}` |
|       - | 3131 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 | 3132 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 | 3133 | `	}` |
|       - | 3134 | `	/* Compile the loop body */` |
|    2850 | 3135 | `	pGen->pIn = &pEnd[1];` |
|    2850 | 3136 | `	pGen->pEnd = pTmp;` |
|    2850 | 3137 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    2850 | 3138 | `	if( rc == SXERR_ABORT ){` |
|       - | 3139 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3140 | `		return SXERR_ABORT;` |
|       - | 3141 | `	}` |
|       - | 3142 | `	/* Emit the unconditional jump to the start of the loop */` |
|    2850 | 3143 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 3144 | `	/* Fix all jumps now the destination is resolved */` |
|    2850 | 3145 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3146 | `	/* Release the loop block */` |
|    2850 | 3147 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3148 | `	/* Statement successfully compiled */` |
|    2850 | 3149 | `	return SXRET_OK;` |
|       1 | 3150 | `Synchronize:` |
|       - | 3151 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 3152 | `	 * compiling this erroneous block.` |
|       - | 3153 | `	 */` |
|       3 | 3154 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3155 | `		pGen->pIn++;` |
|     ! 0 | 3156 | `	}` |
|       3 | 3157 | `	return SXRET_OK;` |
|    1427 | 3158 |  |
|       - | 3159 | `/*` |
|       - | 3160 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 3161 | ` * According to the PHP language reference` |
|       - | 3162 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 3163 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 3164 | ` *  that is similar to that of C:` |
|       - | 3165 | ` *  if (expr)` |
|       - | 3166 | ` *   statement` |
|       - | 3167 | ` *  else construct:` |
|       - | 3168 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 3169 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 3170 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 3171 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 3172 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 3173 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 3174 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 3175 | ` *  elseif` |
|       - | 3176 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 3177 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 3178 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 3179 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 3180 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 3181 | ` *   <?php` |
|       - | 3182 | ` *    if ($a > $b) {` |
|       - | 3183 | ` *     echo "a is bigger than b";` |
|       - | 3184 | ` *    } elseif ($a == $b) {` |
|       - | 3185 | ` *     echo "a is equal to b";` |
|       - | 3186 | ` *    } else {` |
|       - | 3187 | ` *     echo "a is smaller than b";` |
|       - | 3188 | ` *    }` |
|       - | 3189 | ` *    ?>` |
|       - | 3190 | ` */` |
|  104510 | 3191 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 3192 |  |
|  104512 | 3193 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  104512 | 3194 | `	GenBlock *pCondBlock = 0;` |
|       - | 3195 | `	sxu32 nJumpIdx;` |
|       - | 3196 | `	sxu32 nKeyID;` |
|       - | 3197 | `	sxi32 rc;` |
|       - | 3198 | `	/* Jump the 'if' keyword */` |
|  104512 | 3199 | `	pGen->pIn++;` |
|  104512 | 3200 | `	pToken = pGen->pIn;` |
|       - | 3201 | `	/* Create the conditional block */` |
|  104512 | 3202 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  104512 | 3203 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3204 | `		return SXERR_ABORT;` |
|       - | 3205 | `	}` |
|       - | 3206 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   57471 | 3207 | `	for(;;){` |
|  114944 | 3208 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3209 | `			/* Syntax error */` |
|     ! 0 | 3210 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3211 | `				pToken--;` |
|     ! 0 | 3212 | `			}` |
|     ! 0 | 3213 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 3214 | `			if( rc == SXERR_ABORT ){` |
|       - | 3215 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3216 | `				return SXERR_ABORT;` |
|       - | 3217 | `			}` |
|     ! 0 | 3218 | `			goto Synchronize;` |
|       - | 3219 | `		}` |
|       - | 3220 | `		/* Jump the left parenthesis '(' */` |
|  114944 | 3221 | `		pToken++;` |
|       - | 3222 | `		/* Delimit the condition */` |
|  114944 | 3223 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  114944 | 3224 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 3225 | `			/* Syntax error */` |
|     ! 0 | 3226 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3227 | `				pToken--;` |
|     ! 0 | 3228 | `			}` |
|     ! 0 | 3229 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 3230 | `			if( rc == SXERR_ABORT ){` |
|       - | 3231 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3232 | `				return SXERR_ABORT;` |
|       - | 3233 | `			}` |
|     ! 0 | 3234 | `			goto Synchronize;` |
|       - | 3235 | `		}` |
|       - | 3236 | `		/* Swap token streams */` |
|  114944 | 3237 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 3238 | `		/* Compile the condition */` |
|  114944 | 3239 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3240 | `		/* Update token stream */` |
|  114944 | 3241 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 3242 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3243 | `			pGen->pIn++;` |
|     ! 0 | 3244 | `		}` |
|  114944 | 3245 | `		pGen->pIn  = &pEnd[1];` |
|  114944 | 3246 | `		pGen->pEnd = pTmp;` |
|  114944 | 3247 | `		if( rc == SXERR_ABORT ){` |
|       - | 3248 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3249 | `			return SXERR_ABORT;` |
|       - | 3250 | `		}` |
|       - | 3251 | `		/* Emit the false jump */` |
|  114944 | 3252 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 3253 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  114944 | 3254 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 3255 | `		/* Compile the body */` |
|  114944 | 3256 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  114944 | 3257 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3258 | `			return SXERR_ABORT;` |
|       - | 3259 | `		}` |
|  114944 | 3260 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   30944 | 3261 | `			break;` |
|       - | 3262 | `		}` |
|       - | 3263 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   53060 | 3264 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   53060 | 3265 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   34100 | 3266 | `			break;` |
|       - | 3267 | `		}` |
|       - | 3268 | `		/* Emit the unconditional jump */` |
|   18962 | 3269 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 3270 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   18962 | 3271 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   18962 | 3272 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   13734 | 3273 | `			pToken = &pGen->pIn[1];` |
|   13734 | 3274 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5234 | 3275 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4266 | 3276 | `					break;` |
|       - | 3277 | `			}` |
|    5206 | 3278 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2602 | 3279 | `		}` |
|   10434 | 3280 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 3281 | `		/* Synchronize cursors */` |
|   10434 | 3282 | `		pToken = pGen->pIn;` |
|       - | 3283 | `		/* Fix the false jump */` |
|   10434 | 3284 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 3285 | `	} /* For(;;) */` |
|       - | 3286 | `	/* Fix the false jump */` |
|  104512 | 3287 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  104512 | 3288 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   42626 | 3289 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 3290 | `			/* Compile the else block */` |
|    8530 | 3291 | `			pGen->pIn++;` |
|    8530 | 3292 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    8530 | 3293 | `			if( rc == SXERR_ABORT ){` |
|       - | 3294 |  |
|     ! 0 | 3295 | `				return SXERR_ABORT;` |
|       - | 3296 | `			}` |
|    4264 | 3297 | `	}` |
|  104512 | 3298 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3299 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  104512 | 3300 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 3301 | `	/* Release the conditional block */` |
|  104512 | 3302 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3303 | `	/* Statement successfully compiled */` |
|  104512 | 3304 | `	return SXRET_OK;` |
|     ! 0 | 3305 | `Synchronize:` |
|       - | 3306 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 3307 | `	 */` |
|     ! 0 | 3308 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3309 | `		pGen->pIn++;` |
|     ! 0 | 3310 | `	}` |
|     ! 0 | 3311 | `	return SXRET_OK;` |
|   52257 | 3312 |  |
|       - | 3313 | `/*` |
|       - | 3314 | ` * Compile the global construct.` |
|       - | 3315 | ` * According to the PHP language reference` |
|       - | 3316 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 3317 | ` *  to be used in that function.` |
|       - | 3318 | ` *  Example #1 Using global` |
|       - | 3319 | ` *  <?php` |
|       - | 3320 | ` *   $a = 1;` |
|       - | 3321 | ` *   $b = 2;` |
|       - | 3322 | ` *   function Sum()` |
|       - | 3323 | ` *   {` |
|       - | 3324 | ` *    global $a, $b;` |
|       - | 3325 | ` *    $b = $a + $b;` |
|       - | 3326 | ` *   }` |
|       - | 3327 | ` *   Sum();` |
|       - | 3328 | ` *   echo $b;` |
|       - | 3329 | ` *  ?>` |
|       - | 3330 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 3331 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 3332 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 3333 | ` */` |
|      26 | 3334 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 | 3335 |  |
|      28 | 3336 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3337 | `	sxi32 nExpr;` |
|       - | 3338 | `	sxi32 rc;` |
|       - | 3339 | `	/* Jump the 'global' keyword */` |
|      28 | 3340 | `	pGen->pIn++;` |
|      28 | 3341 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 3342 | `		/* Nothing to process */` |
|     ! 0 | 3343 | `		return SXRET_OK;` |
|       - | 3344 | `	}` |
|      28 | 3345 | `	pTmp = pGen->pEnd;` |
|      28 | 3346 | `	nExpr = 0;` |
|      56 | 3347 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      30 | 3348 | `		if( pGen->pIn < pNext ){` |
|      30 | 3349 | `			pGen->pEnd = pNext;` |
|      30 | 3350 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3351 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 3352 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 3353 | `					return SXERR_ABORT;` |
|       - | 3354 | `				}` |
|     ! 0 | 3355 | `			}else{` |
|      30 | 3356 | `				pGen->pIn++;` |
|      30 | 3357 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3358 | `					/* Emit a warning */` |
|     ! 0 | 3359 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 3360 | `				}else{` |
|      30 | 3361 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 | 3362 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3363 | `						return SXERR_ABORT;` |
|      30 | 3364 | `					}else if(rc != SXERR_EMPTY ){` |
|      30 | 3365 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      30 | 3366 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - | 3367 | `							/* Variable name, not a constant */` |
|      30 | 3368 | `							pLast->iP1 = 0;` |
|      14 | 3369 | `						}` |
|      30 | 3370 | `						nExpr++;` |
|      14 | 3371 | `					}` |
|       - | 3372 | `				}` |
|       - | 3373 | `			}` |
|      14 | 3374 | `		}` |
|       - | 3375 | `		/* Next expression in the stream */` |
|      30 | 3376 | `		pGen->pIn = pNext;` |
|       - | 3377 | `		/* Jump trailing commas */` |
|      32 | 3378 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3379 | `			pGen->pIn++;` |
|       1 | 3380 | `		}` |
|       2 | 3381 | `	}` |
|       - | 3382 | `	/* Restore token stream */` |
|      28 | 3383 | `	pGen->pEnd = pTmp;` |
|      28 | 3384 | `	if( nExpr > 0 ){` |
|       - | 3385 | `		/* Emit the uplink instruction */` |
|      28 | 3386 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      13 | 3387 | `	}` |
|      28 | 3388 | `	return SXRET_OK;` |
|      15 | 3389 |  |
|       - | 3390 | `/*` |
|       - | 3391 | ` * Compile the return statement.` |
|       - | 3392 | ` * According to the PHP language reference` |
|       - | 3393 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 3394 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 3395 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 3396 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 3397 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 3398 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 3399 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 3400 | ` *  from within the main script file, then script execution end.` |
|       - | 3401 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 3402 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 3403 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 3404 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 3405 | ` */` |
|  151550 | 3406 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 3407 |  |
|  151552 | 3408 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3409 | `	sxi32 rc;` |
|       - | 3410 | `	/* Jump the 'return' keyword */` |
|  151552 | 3411 | `	pGen->pIn++;` |
|  151552 | 3412 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3413 | `		/* Compile the expression */` |
|  151530 | 3414 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  151530 | 3415 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3416 | `			return SXERR_ABORT;` |
|  151530 | 3417 | `		}else if(rc != SXERR_EMPTY ){` |
|  151530 | 3418 | `			nRet = 1;` |
|   75764 | 3419 | `		}` |
|   75764 | 3420 | `	}` |
|       - | 3421 | `	/* Emit the done instruction */` |
|  151552 | 3422 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  151552 | 3423 | `	return SXRET_OK;` |
|   75777 | 3424 |  |
|       - | 3425 | `/*` |
|       - | 3426 | ` * Compile a yield expression.` |
|       - | 3427 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - | 3428 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - | 3429 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - | 3430 | ` */` |
|      32 | 3431 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 3432 |  |
|       - | 3433 | `	SyToken *pTmp, *pSplit;` |
|      34 | 3434 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      34 | 3435 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - | 3436 | `	sxi32 rc;` |
|      16 | 3437 | `	(void)iCompileFlag;` |
|       - | 3438 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      34 | 3439 | `	pGen->pIn++;` |
|       - | 3440 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - | 3441 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      34 | 3442 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3443 | `		/* Bare yield — no value */` |
|     ! 0 | 3444 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 | 3445 | `		return SXRET_OK;` |
|       - | 3446 | `	}` |
|       - | 3447 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      34 | 3448 | `	pSplit = 0;` |
|       - | 3449 | `	{` |
|      34 | 3450 | `		SyToken *pCur = pGen->pIn;` |
|      34 | 3451 | `		sxi32 nNest = 0;` |
|      78 | 3452 | `		while( pCur < pGen->pEnd ){` |
|      52 | 3453 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 | 3454 | `				nNest++;` |
|      52 | 3455 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 | 3456 | `				nNest--;` |
|      52 | 3457 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 | 3458 | `				pSplit = pCur;` |
|       7 | 3459 | `				break;` |
|       - | 3460 | `			}` |
|      46 | 3461 | `			pCur++;` |
|       2 | 3462 | `		}` |
|       - | 3463 | `	}` |
|      34 | 3464 | `	pTmp = pGen->pEnd;` |
|      34 | 3465 | `	if( pSplit ){` |
|       - | 3466 | `		/* yield $key => $value */` |
|       7 | 3467 | `		pGen->pEnd = pSplit;` |
|       7 | 3468 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 3469 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 3470 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       7 | 3471 | `		pGen->pEnd = pTmp;` |
|       7 | 3472 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 3473 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 3474 | `		iP1 = 1;` |
|       7 | 3475 | `		iP2 = 1;` |
|       4 | 3476 | `	}else{` |
|       - | 3477 | `		/* yield $value */` |
|      28 | 3478 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      28 | 3479 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      28 | 3480 | `		if( rc != SXERR_EMPTY ){` |
|      28 | 3481 | `			iP1 = 1;` |
|      13 | 3482 | `		}` |
|       - | 3483 | `	}` |
|      34 | 3484 | `	pGen->pEnd = pTmp;` |
|      34 | 3485 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      34 | 3486 | `	return SXRET_OK;` |
|      18 | 3487 |  |
|       - | 3488 | `/*` |
|       - | 3489 | ` * Compile the die/exit language construct.` |
|       - | 3490 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 3491 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 3492 | ` */` |
|      88 | 3493 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 | 3494 |  |
|      90 | 3495 | `	sxi32 nExpr = 0;` |
|       - | 3496 | `	sxi32 rc;` |
|       - | 3497 | `	/* Jump the die/exit keyword */` |
|      90 | 3498 | `	pGen->pIn++;` |
|      90 | 3499 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3500 | `		/* Compile the expression */` |
|      90 | 3501 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 | 3502 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3503 | `			return SXERR_ABORT;` |
|      90 | 3504 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 | 3505 | `			nExpr = 1;` |
|      44 | 3506 | `		}` |
|      44 | 3507 | `	}` |
|       - | 3508 | `	/* Emit the HALT instruction */` |
|      90 | 3509 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 | 3510 | `	return SXRET_OK;` |
|      46 | 3511 |  |
|       - | 3512 | `/*` |
|       - | 3513 | ` * Compile the 'echo' language construct.` |
|       - | 3514 | ` */` |
|   10662 | 3515 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 3516 |  |
|   10664 | 3517 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3518 | `	sxi32 rc;` |
|       - | 3519 | `	/* Jump the 'echo' keyword */` |
|   10664 | 3520 | `	pGen->pIn++;` |
|       - | 3521 | `	/* Compile arguments one after one */` |
|   10664 | 3522 | `	pTmp = pGen->pEnd;` |
|   21714 | 3523 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   11052 | 3524 | `		if( pGen->pIn < pNext ){` |
|   11052 | 3525 | `			pGen->pEnd = pNext;` |
|   11052 | 3526 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   11052 | 3527 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3528 | `				return SXERR_ABORT;` |
|   11052 | 3529 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3530 | `				/* Emit the consume instruction */` |
|   11028 | 3531 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    5513 | 3532 | `			}` |
|    5525 | 3533 | `		}` |
|       - | 3534 | `		/* Jump trailing commas */` |
|   11440 | 3535 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     390 | 3536 | `			pNext++;` |
|       2 | 3537 | `		}` |
|   11052 | 3538 | `		pGen->pIn = pNext;` |
|       2 | 3539 | `	}` |
|       - | 3540 | `	/* Restore token stream */` |
|   10664 | 3541 | `	pGen->pEnd = pTmp;` |
|   10664 | 3542 | `	return SXRET_OK;` |
|    5333 | 3543 |  |
|       - | 3544 | `/*` |
|       - | 3545 | ` * Compile the static statement.` |
|       - | 3546 | ` * According to the PHP language reference` |
|       - | 3547 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 3548 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 3549 | ` *  when program execution leaves this scope.` |
|       - | 3550 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 3551 | ` * Symisc eXtension.` |
|       - | 3552 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 3553 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3554 | ` *  Example` |
|       - | 3555 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 3556 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 3557 | ` */` |
|       2 | 3558 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 | 3559 |  |
|       - | 3560 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 3561 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 3562 | `	GenBlock *pBlock;` |
|       - | 3563 | `	SyString *pName;` |
|       - | 3564 | `	char *zDup;` |
|       - | 3565 | `	sxu32 nLine;` |
|       - | 3566 | `	sxi32 rc;` |
|       - | 3567 | `	/* Jump the static keyword */` |
|       3 | 3568 | `	nLine = pGen->pIn->nLine;` |
|       3 | 3569 | `	pGen->pIn++;` |
|       - | 3570 | `	/* Extract the enclosing function if any */` |
|       3 | 3571 | `	pBlock = pGen->pCurrent;` |
|       5 | 3572 | `	while( pBlock ){` |
|       5 | 3573 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 | 3574 | `			break;` |
|       - | 3575 | `		}` |
|       - | 3576 | `		/* Point to the upper block */` |
|       3 | 3577 | `		pBlock = pBlock->pParent;` |
|       1 | 3578 | `	}` |
|       3 | 3579 | `	if( pBlock == 0 ){` |
|       - | 3580 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 3581 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3582 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 | 3583 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3584 | `				return SXERR_ABORT;` |
|       - | 3585 | `			}` |
|     ! 0 | 3586 | `			goto Synchronize;` |
|       - | 3587 | `		}` |
|       - | 3588 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 3589 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 3590 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3591 | `			return SXERR_ABORT;` |
|     ! 0 | 3592 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 3593 | `			/* Emit the POP instruction */` |
|     ! 0 | 3594 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 3595 | `		}` |
|     ! 0 | 3596 | `		return SXRET_OK;` |
|       - | 3597 | `	}` |
|       3 | 3598 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 3599 | `	/* Make sure we are dealing with a valid statement */` |
|       3 | 3600 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 | 3601 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 | 3602 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 | 3603 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3604 | `				return SXERR_ABORT;` |
|       - | 3605 | `			}` |
|       3 | 3606 | `			goto Synchronize;` |
|       - | 3607 | `	}` |
|     ! 0 | 3608 | `	pGen->pIn++;` |
|       - | 3609 | `	/* Extract variable name */` |
|     ! 0 | 3610 | `	pName = &pGen->pIn->sData;` |
|     ! 0 | 3611 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 | 3612 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 3613 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3614 | `		goto Synchronize;` |
|       - | 3615 | `	}` |
|       - | 3616 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 | 3617 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 | 3618 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 3619 | `	/* Duplicate variable name */` |
|     ! 0 | 3620 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 | 3621 | `	if( zDup == 0 ){` |
|     ! 0 | 3622 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3623 | `		return SXERR_ABORT;` |
|       - | 3624 | `	}` |
|     ! 0 | 3625 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 3626 | `	/* Check if we have an expression to compile */` |
|     ! 0 | 3627 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 3628 | `		SySet *pInstrContainer;` |
|       - | 3629 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 3630 | `		 * Static variable can take any complex expression including function` |
|       - | 3631 | `		 * call as their initialization value.` |
|       - | 3632 | `		 * Example:` |
|       - | 3633 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 3634 | `		 */` |
|     ! 0 | 3635 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 3636 | `		/* Swap bytecode container */` |
|     ! 0 | 3637 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 | 3638 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 3639 | `		/* Compile the expression */` |
|     ! 0 | 3640 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3641 | `		/* Emit the done instruction */` |
|     ! 0 | 3642 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 3643 | `		/* Restore default bytecode container */` |
|     ! 0 | 3644 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 3645 | `	}` |
|       - | 3646 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 | 3647 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 | 3648 | `	return SXRET_OK;` |
|       1 | 3649 | `Synchronize:` |
|       - | 3650 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 3651 | `	 * statement.` |
|       - | 3652 | `	 */` |
|       5 | 3653 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 | 3654 | `		pGen->pIn++;` |
|       1 | 3655 | `	}` |
|       3 | 3656 | `	return SXRET_OK;` |
|       2 | 3657 |  |
|       - | 3658 | `/*` |
|       - | 3659 | ` * Compile the var statement.` |
|       - | 3660 | ` * Symisc Extension:` |
|       - | 3661 | ` *      var statement can be used outside of a class definition.` |
|       - | 3662 | ` */` |
|       4 | 3663 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 3664 |  |
|       - | 3665 | `	sxu32 nLine;` |
|       - | 3666 | `	sxi32 rc;` |
|       5 | 3667 | `	nLine = pGen->pIn->nLine;` |
|       - | 3668 | `	/* Jump the 'var' keyword */` |
|       5 | 3669 | `	pGen->pIn++;` |
|       5 | 3670 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 3671 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - | 3672 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 | 3673 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 | 3674 | `			pGen->pIn++;` |
|     ! 0 | 3675 | `		}` |
|     ! 0 | 3676 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3677 | `			return SXERR_ABORT;` |
|       - | 3678 | `		}` |
|     ! 0 | 3679 | `	}else{` |
|       - | 3680 | `		/* Compile the expression */` |
|       5 | 3681 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 | 3682 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3683 | `			return SXERR_ABORT;` |
|       5 | 3684 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 | 3685 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 3686 | `		}` |
|       - | 3687 | `	}` |
|       5 | 3688 | `	return SXRET_OK;` |
|       3 | 3689 |  |
|       - | 3690 | `/*` |
|       - | 3691 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 3692 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 3693 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 3694 | ` */` |
|       - | 3695 | `/*` |
|       - | 3696 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - | 3697 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 3698 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 3699 | ` * qualified name and updates the instruction's operand index.` |
|       - | 3700 | ` *` |
|       - | 3701 | ` * Resolution order:` |
|       - | 3702 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - | 3703 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - | 3704 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - | 3705 | ` *` |
|       - | 3706 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - | 3707 | ` * came from an import (step 1) and 0 otherwise.` |
|       - | 3708 | ` * Returns the (possibly new) literal index.` |
|       - | 3709 | ` */` |
|  311242 | 3710 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       2 | 3711 |  |
|       - | 3712 | `	ph7_value *pLit;` |
|       - | 3713 | `	const char *zLit;` |
|       - | 3714 | `	SyString sQualified;` |
|       - | 3715 | `	sxu32 nLit;` |
|       - | 3716 | `	sxu32 k;` |
|       - | 3717 | `	sxu32 nNewIdx;` |
|       - | 3718 | `	int hasNsSep;` |
|       - | 3719 | `	SyHashEntry *pImport;` |
|       - | 3720 | `	ph7_value *pNew;` |
|  311244 | 3721 | `	if( pFromImport ){` |
|  297720 | 3722 | `		*pFromImport = 0;` |
|  148859 | 3723 | `	}` |
|  311244 | 3724 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  311244 | 3725 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 3726 | `		return nOrigIdx;` |
|       - | 3727 | `	}` |
|  311244 | 3728 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  311244 | 3729 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 3730 | `	/* Skip if already qualified (contains backslash) */` |
|  311244 | 3731 | `	hasNsSep = 0;` |
| 3347748 | 3732 | `	for( k = 0; k < nLit; k++ ){` |
| 3036538 | 3733 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1518254 | 3734 | `	}` |
|  311244 | 3735 | `	if( hasNsSep ){` |
|      34 | 3736 | `		return nOrigIdx;` |
|       - | 3737 | `	}` |
|       - | 3738 | `	/* Check use imports first (works even outside namespaces) */` |
|  311212 | 3739 | `	SyBlobReset(&pGen->sWorker);` |
|  311212 | 3740 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  311212 | 3741 | `	if( pImport ){` |
|      38 | 3742 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 | 3743 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 | 3744 | `		if( pFromImport ){` |
|      18 | 3745 | `			*pFromImport = 1;` |
|       8 | 3746 | `		}` |
|      20 | 3747 | `	}else{` |
|  311176 | 3748 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  311106 | 3749 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - | 3750 | `		}` |
|       - | 3751 | `		/* Prepend current namespace */` |
|      72 | 3752 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      72 | 3753 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      72 | 3754 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 3755 | `	}` |
|       - | 3756 | `	/* Look up or create a new literal for the qualified name */` |
|     108 | 3757 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     108 | 3758 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      44 | 3759 | `		return nNewIdx; /* Already interned */` |
|       - | 3760 | `	}` |
|      66 | 3761 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      66 | 3762 | `	if( pNew == 0 ){` |
|     ! 0 | 3763 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 3764 | `	}` |
|      66 | 3765 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      66 | 3766 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      66 | 3767 | `	return nNewIdx;` |
|  155623 | 3768 |  |
|       - | 3769 | `/*` |
|       - | 3770 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 3771 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 3772 | ` */` |
|   26178 | 3773 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3774 |  |
|       - | 3775 | `	SyHashEntry *pImport;` |
|       - | 3776 | `	/* Check use imports first */` |
|   26180 | 3777 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   26180 | 3778 | `	if( pImport ){` |
|       7 | 3779 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       7 | 3780 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       7 | 3781 | `		return;` |
|       - | 3782 | `	}` |
|       - | 3783 | `	/* Prepend current namespace if active */` |
|   26174 | 3784 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 3785 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 3786 | `		SyBlobAppend(pOut,"\\",1);` |
|       1 | 3787 | `	}` |
|   26174 | 3788 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   13091 | 3789 |  |
|       - | 3790 | `/*` |
|       - | 3791 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 3792 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 3793 | ` * The caller must release pOut when done.` |
|       - | 3794 | ` */` |
|   44750 | 3795 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3796 |  |
|   44752 | 3797 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      46 | 3798 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      46 | 3799 | `		SyBlobAppend(pOut,"\\",1);` |
|      22 | 3800 | `	}` |
|   44752 | 3801 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   44752 | 3802 |  |
|       - | 3803 | `/*` |
|       - | 3804 | ` * Compile a namespace statement` |
|       - | 3805 | ` * According to the PHP language reference manual` |
|       - | 3806 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 3807 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 3808 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 3809 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 3810 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 3811 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 3812 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 3813 | ` *  programming world.` |
|       - | 3814 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 3815 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 3816 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 3817 | ` *  classes/functions/constants.` |
|       - | 3818 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 3819 | ` *  readability of source code.` |
|       - | 3820 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 3821 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 3822 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 3823 | ` *       class MyClass {}` |
|       - | 3824 | ` *       function myfunction() {}` |
|       - | 3825 | ` *       const MYCONST = 1;` |
|       - | 3826 | ` *       $a = new MyClass;` |
|       - | 3827 | ` *       $c = new \my\name\MyClass;` |
|       - | 3828 | ` *       $a = strlen('hi');` |
|       - | 3829 | ` *       $d = namespace\MYCONST;` |
|       - | 3830 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 3831 | ` *       echo constant($d);` |
|       - | 3832 | ` * NOTE` |
|       - | 3833 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3834 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3835 | ` */` |
|       - | 3836 | `/*` |
|       - | 3837 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 3838 | ` */` |
|       6 | 3839 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 | 3840 |  |
|       7 | 3841 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|     ! 0 | 3842 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|     ! 0 | 3843 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|     ! 0 | 3844 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|     ! 0 | 3845 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|     ! 0 | 3846 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|     ! 0 | 3847 | `	return "token";` |
|       4 | 3848 |  |
|      94 | 3849 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       2 | 3850 |  |
|       - | 3851 | `	sxu32 nLine;` |
|       - | 3852 | `	sxi32 rc;` |
|      96 | 3853 | `	nLine = pGen->pIn->nLine;` |
|      96 | 3854 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 3855 | `	/* Reset namespace and clear previous use imports */` |
|      96 | 3856 | `	SyBlobReset(&pGen->sNamespace);` |
|      96 | 3857 | `	SyHashRelease(&pGen->hUseImports);` |
|      96 | 3858 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      96 | 3859 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      96 | 3860 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      96 | 3861 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      96 | 3862 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      96 | 3863 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3864 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 | 3865 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3866 | `		return SXRET_OK;` |
|       - | 3867 | `	}` |
|      96 | 3868 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - | 3869 | `		/* namespace; — switch to global namespace */` |
|     ! 0 | 3870 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3871 | `		return SXRET_OK;` |
|       - | 3872 | `	}` |
|      96 | 3873 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - | 3874 | `		/* namespace { } — global namespace block */` |
|     ! 0 | 3875 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3876 | `		return SXRET_OK;` |
|       - | 3877 | `	}` |
|       - | 3878 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     228 | 3879 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     134 | 3880 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 3881 | `			/* Append backslash separator */` |
|      21 | 3882 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      21 | 3883 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      10 | 3884 | `			}` |
|      11 | 3885 | `		}else{` |
|       - | 3886 | `			/* Append identifier */` |
|     114 | 3887 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 3888 | `		}` |
|     134 | 3889 | `		pGen->pIn++;` |
|       2 | 3890 | `	}` |
|       - | 3891 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - | 3892 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - | 3893 | `	{` |
|      96 | 3894 | `		char *zNsDup = 0;` |
|      96 | 3895 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     140 | 3896 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      92 | 3897 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      46 | 3898 | `		}` |
|      96 | 3899 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - | 3900 | `	}` |
|      96 | 3901 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 | 3902 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3903 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 3904 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 | 3905 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3906 | `			return SXERR_ABORT;` |
|       - | 3907 | `		}` |
|       2 | 3908 | `	}` |
|      96 | 3909 | `	return SXRET_OK;` |
|      49 | 3910 |  |
|       - | 3911 | `/*` |
|       - | 3912 | ` * Compile the 'use' statement` |
|       - | 3913 | ` * According to the PHP language reference manual` |
|       - | 3914 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 3915 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 3916 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 3917 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 3918 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 3919 | ` *  a function or constant is not supported.` |
|       - | 3920 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 3921 | ` * NOTE` |
|       - | 3922 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3923 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3924 | ` */` |
|      64 | 3925 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       2 | 3926 |  |
|       - | 3927 | `	sxu32 nLine;` |
|       - | 3928 | `	sxi32 rc;` |
|       - | 3929 | `	SyBlob sPath;` |
|       - | 3930 | `	SyString sAlias;` |
|       - | 3931 | `	SyToken *pLast;` |
|       - | 3932 | `	char *zDup;` |
|       - | 3933 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - | 3934 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - | 3935 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      66 | 3936 | `	nLine = pGen->pIn->nLine;` |
|      66 | 3937 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - | 3938 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      66 | 3939 | `	iUseType = 0;` |
|      66 | 3940 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 | 3941 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 | 3942 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 | 3943 | `			iUseType = 1;` |
|      16 | 3944 | `			pGen->pIn++;` |
|      23 | 3945 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 | 3946 | `			iUseType = 2;` |
|      16 | 3947 | `			pGen->pIn++;` |
|       7 | 3948 | `		}` |
|      14 | 3949 | `	}` |
|       - | 3950 | `	/* Select target hash tables based on import type */` |
|      66 | 3951 | `	switch( iUseType ){` |
|       7 | 3952 | `		case 1:` |
|      16 | 3953 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 | 3954 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 | 3955 | `			break;` |
|       7 | 3956 | `		case 2:` |
|      16 | 3957 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 | 3958 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 | 3959 | `			break;` |
|      18 | 3960 | `		default:` |
|      38 | 3961 | `			pGenHash = &pGen->hUseImports;` |
|      38 | 3962 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      36 | 3963 | `			break;` |
|       - | 3964 | `	}` |
|      66 | 3965 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 3966 | `	/* Process one or more use declarations separated by commas */` |
|      33 | 3967 | `	for(;;){` |
|      68 | 3968 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3969 | `			break;` |
|       - | 3970 | `		}` |
|      68 | 3971 | `		SyBlobReset(&sPath);` |
|      68 | 3972 | `		pLast = 0;` |
|       - | 3973 | `		/* Collect the full namespace path */` |
|     250 | 3974 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     184 | 3975 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     124 | 3976 | `				pLast = pGen->pIn;` |
|     124 | 3977 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      62 | 3978 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 | 3979 | `				}` |
|     124 | 3980 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      61 | 3981 | `			}` |
|     184 | 3982 | `			pGen->pIn++;` |
|       2 | 3983 | `		}` |
|      68 | 3984 | `		if( pLast == 0 ){` |
|       - | 3985 | `			/* Empty path */` |
|       5 | 3986 | `			break;` |
|       - | 3987 | `		}` |
|       - | 3988 | `		/* Default alias is the last component of the path */` |
|      64 | 3989 | `		sAlias = pLast->sData;` |
|       - | 3990 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      62 | 3991 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      41 | 3992 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 | 3993 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 | 3994 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 | 3995 | `				sAlias = pGen->pIn->sData;` |
|      18 | 3996 | `				pGen->pIn++;` |
|       8 | 3997 | `			}` |
|       8 | 3998 | `		}` |
|       - | 3999 | `		/* Check for duplicate import alias (per-type) */` |
|      64 | 4000 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 | 4001 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 4002 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 | 4003 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 | 4004 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4005 | `				SyBlobRelease(&sPath);` |
|     ! 0 | 4006 | `				return SXERR_ABORT;` |
|       - | 4007 | `			}` |
|       2 | 4008 | `		}` |
|       - | 4009 | `		/* Register the import: alias -> FQN.` |
|       - | 4010 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - | 4011 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - | 4012 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      95 | 4013 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      62 | 4014 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      64 | 4015 | `		if( zDup ){` |
|      64 | 4016 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      64 | 4017 | `			if( pVmHash ){` |
|       - | 4018 | `				/* Class imports: populate VM table directly (class resolution` |
|       - | 4019 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      36 | 4020 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      36 | 4021 | `				if( zAliasDup ){` |
|      36 | 4022 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      17 | 4023 | `				}` |
|      17 | 4024 | `			}` |
|      64 | 4025 | `			if( iUseType == 2 ){` |
|       - | 4026 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - | 4027 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 | 4028 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 | 4029 | `				if( zAliasDup ){` |
|       - | 4030 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - | 4031 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - | 4032 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 | 4033 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 | 4034 | `					if( azPair ){` |
|      16 | 4035 | `						azPair[0] = zAliasDup;` |
|      16 | 4036 | `						azPair[1] = zDup;` |
|      16 | 4037 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 | 4038 | `					}` |
|       7 | 4039 | `				}` |
|       7 | 4040 | `			}` |
|      31 | 4041 | `		}` |
|       - | 4042 | `		/* Check for comma (multiple use declarations) */` |
|      64 | 4043 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 4044 | `			pGen->pIn++;` |
|       2 | 4045 | `		}else{` |
|      32 | 4046 | `			break;` |
|       - | 4047 | `		}` |
|       1 | 4048 | `	}` |
|      66 | 4049 | `	SyBlobRelease(&sPath);` |
|      66 | 4050 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 4051 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 4052 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 4053 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4054 | `			return SXERR_ABORT;` |
|       - | 4055 | `		}` |
|       1 | 4056 | `	}` |
|      66 | 4057 | `	return SXRET_OK;` |
|      34 | 4058 |  |
|       - | 4059 | `/*` |
|       - | 4060 | ` * Compile the stupid 'declare' language construct.` |
|       - | 4061 | ` *` |
|       - | 4062 | ` * According to the PHP language reference manual.` |
|       - | 4063 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 4064 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 4065 | ` *  declare (directive)` |
|       - | 4066 | ` *   statement` |
|       - | 4067 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 4068 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 4069 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 4070 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 4071 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 4072 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 4073 | ` * <?php` |
|       - | 4074 | ` * // these are the same:` |
|       - | 4075 | ` * // you can use this:` |
|       - | 4076 | ` * declare(ticks=1) {` |
|       - | 4077 | ` *   // entire script here` |
|       - | 4078 | ` * }` |
|       - | 4079 | ` * // or you can use this:` |
|       - | 4080 | ` * declare(ticks=1);` |
|       - | 4081 | ` * // entire script here` |
|       - | 4082 | ` * ?>` |
|       - | 4083 | ` *` |
|       - | 4084 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 4085 | ` */` |
|       8 | 4086 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 4087 |  |
|       9 | 4088 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 4089 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 4090 | `	sxi32 rc;` |
|       9 | 4091 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 4092 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 4093 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 4094 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4095 | `			return SXERR_ABORT;` |
|       - | 4096 | `		}` |
|       5 | 4097 | `		goto Synchro;` |
|       - | 4098 | `	}` |
|       5 | 4099 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 4100 | `	/* Delimit the directive */` |
|       5 | 4101 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 4102 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 4103 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 4104 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4105 | `			return SXERR_ABORT;` |
|       - | 4106 | `		}` |
|     ! 0 | 4107 | `		return SXRET_OK;` |
|       - | 4108 | `	}` |
|       - | 4109 | `	/* Update the cursor */` |
|       5 | 4110 | `	pGen->pIn = &pEnd[1];` |
|       5 | 4111 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 4112 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 4113 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4114 | `			return SXERR_ABORT;` |
|       - | 4115 | `		}` |
|     ! 0 | 4116 | `	}` |
|       - | 4117 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 4118 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 4119 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 4120 | `		ph7_lib_version()` |
|       - | 4121 | `		);` |
|       - | 4122 | `	/*All done */` |
|       5 | 4123 | `	return SXRET_OK;` |
|       2 | 4124 | `Synchro:` |
|       - | 4125 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 4126 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 4127 | `		pGen->pIn++;` |
|       1 | 4128 | `	}` |
|       5 | 4129 | `	return SXRET_OK;` |
|       5 | 4130 |  |
|       - | 4131 | `/*` |
|       - | 4132 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 4133 | ` * as follows:` |
|       - | 4134 | ` * function makecoffee($type = "cappuccino")` |
|       - | 4135 | ` * {` |
|       - | 4136 | ` *   return "Making a cup of $type.\n";` |
|       - | 4137 | ` * }` |
|       - | 4138 | ` * Symisc eXtension.` |
|       - | 4139 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 4140 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 4141 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 4142 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 4143 | ` *      {` |
|       - | 4144 | ` *       var_dump($a);` |
|       - | 4145 | ` *      }` |
|       - | 4146 | ` *     //call test without args` |
|       - | 4147 | ` *      test();` |
|       - | 4148 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 4149 | ` *      Example:` |
|       - | 4150 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 4151 | ` * 3 -) Function overloading!!` |
|       - | 4152 | ` *      Example:` |
|       - | 4153 | ` *      function foo($a) {` |
|       - | 4154 | ` *   	  return $a.PHP_EOL;` |
|       - | 4155 | ` *	    }` |
|       - | 4156 | ` *	    function foo($a, $b) {` |
|       - | 4157 | ` *   	  return $a + $b;` |
|       - | 4158 | ` *	    }` |
|       - | 4159 | ` *	    echo foo(5); // Prints "5"` |
|       - | 4160 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 4161 | ` *      // Same arg` |
|       - | 4162 | ` *	   function foo(string $a)` |
|       - | 4163 | ` *	   {` |
|       - | 4164 | ` *	     echo "a is a string\n";` |
|       - | 4165 | ` *	     var_dump($a);` |
|       - | 4166 | ` *	   }` |
|       - | 4167 | ` *	  function foo(int $a)` |
|       - | 4168 | ` *	  {` |
|       - | 4169 | ` *	    echo "a is integer\n";` |
|       - | 4170 | ` *	    var_dump($a);` |
|       - | 4171 | ` *	  }` |
|       - | 4172 | ` *	  function foo(array $a)` |
|       - | 4173 | ` *	  {` |
|       - | 4174 | ` * 	    echo "a is an array\n";` |
|       - | 4175 | ` * 	    var_dump($a);` |
|       - | 4176 | ` *	  }` |
|       - | 4177 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 4178 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 4179 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 4180 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 4181 | ` * introduced by the PH7 engine.` |
|       - | 4182 | ` */` |
|   41642 | 4183 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 4184 |  |
|       - | 4185 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 4186 | `	SySet *pInstrContainer;` |
|       - | 4187 | `	sxi32 rc;` |
|       - | 4188 | `	/* Swap token stream */` |
|   41644 | 4189 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   41644 | 4190 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   41644 | 4191 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 4192 | `	/* Compile the expression holding the argument value */` |
|   41644 | 4193 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4194 | `	/* Emit the done instruction */` |
|   41644 | 4195 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   41644 | 4196 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   41644 | 4197 | `	RE_SWAP_DELIMITER(pGen);` |
|   41644 | 4198 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4199 | `		return SXERR_ABORT;` |
|       - | 4200 | `	}` |
|   41644 | 4201 | `	return SXRET_OK;` |
|   20823 | 4202 |  |
|       - | 4203 | `/*` |
|       - | 4204 | ` * Collect function arguments one after one.` |
|       - | 4205 | ` * According to the PHP language reference manual.` |
|       - | 4206 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 4207 | ` * list of expressions.` |
|       - | 4208 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 4209 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 4210 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 4211 | ` * for more information.` |
|       - | 4212 | ` * Example #1 Passing arrays to functions` |
|       - | 4213 | ` * <?php` |
|       - | 4214 | ` * function takes_array($input)` |
|       - | 4215 | ` * {` |
|       - | 4216 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 4217 | ` * }` |
|       - | 4218 | ` * ?>` |
|       - | 4219 | ` * Making arguments be passed by reference` |
|       - | 4220 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 4221 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 4222 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 4223 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 4224 | ` * to the argument name in the function definition:` |
|       - | 4225 | ` * Example #2 Passing function parameters by reference` |
|       - | 4226 | ` * <?php` |
|       - | 4227 | ` * function add_some_extra(&$string)` |
|       - | 4228 | ` * {` |
|       - | 4229 | ` *   $string .= 'and something extra.';` |
|       - | 4230 | ` * }` |
|       - | 4231 | ` * $str = 'This is a string, ';` |
|       - | 4232 | ` * add_some_extra($str);` |
|       - | 4233 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 4234 | ` * ?>` |
|       - | 4235 | ` *` |
|       - | 4236 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 4237 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 4238 | ` * on these extension.` |
|       - | 4239 | ` */` |
|   49996 | 4240 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 4241 |  |
|       - | 4242 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 4243 | `	SyToken *pIn;  /* Token stream */` |
|       - | 4244 | `	SyBlob sSig;         /* Function signature */` |
|       - | 4245 | `	char *zDup;          /* Copy of argument name */` |
|       - | 4246 | `	sxi32 rc;` |
|       - | 4247 |  |
|   49998 | 4248 | `	pIn = pGen->pIn;` |
|   49998 | 4249 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 4250 | `	/* Process arguments one after one */` |
|   63256 | 4251 | `	for(;;){` |
|  126514 | 4252 | `		if( pIn >= pEnd ){` |
|       - | 4253 | `			/* No more arguments to process */` |
|   49996 | 4254 | `			break;` |
|       - | 4255 | `		}` |
|   76520 | 4256 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   76520 | 4257 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 4258 | `		/* Detect nullable prefix '?' on type hints */` |
|   76520 | 4259 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      11 | 4260 | `			sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|      11 | 4261 | `			pIn++;` |
|       5 | 4262 | `		}` |
|       - | 4263 | `		/* Skip leading namespace separator '\' on FQN type hints like \Throwable */` |
|   76520 | 4264 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       5 | 4265 | `			pIn++;` |
|       2 | 4266 | `		}` |
|   76520 | 4267 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|   52072 | 4268 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   46864 | 4269 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   46864 | 4270 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 4271 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   46864 | 4272 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 4273 | `					sArg.nType = MEMOBJ_BOOL;` |
|   46864 | 4274 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   13020 | 4275 | `					sArg.nType = MEMOBJ_INT;` |
|   40355 | 4276 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   33844 | 4277 | `					sArg.nType = MEMOBJ_STRING;` |
|   16924 | 4278 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 4279 | `					sArg.nType = MEMOBJ_REAL;` |
|     ! 0 | 4280 | `				}else{` |
|       4 | 4281 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 4282 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 4283 | `						&pIn->sData);` |
|       - | 4284 | `				}` |
|   23433 | 4285 | `			}else{` |
|    5210 | 4286 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 4287 | `				char *zDupLocal;` |
|       - | 4288 | `				/* Argument must be a class instance,record that*/` |
|    5210 | 4289 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    5210 | 4290 | `				if( zDupLocal ){` |
|    5210 | 4291 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    5210 | 4292 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2604 | 4293 | `				}` |
|       - | 4294 | `			}` |
|   52072 | 4295 | `			pIn++;` |
|   26035 | 4296 | `		}` |
|   76520 | 4297 | `		if( pIn >= pEnd ){` |
|     ! 0 | 4298 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 4299 | `			return rc;` |
|       - | 4300 | `		}` |
|   76520 | 4301 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 4302 | `			/* Pass by reference,record that */` |
|    2628 | 4303 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    2628 | 4304 | `			pIn++;` |
|    1313 | 4305 | `		}` |
|   76520 | 4306 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - | 4307 | `			/* Variadic parameter: ...$args */` |
|      23 | 4308 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      23 | 4309 | `			pIn++;` |
|      11 | 4310 | `		}` |
|   76520 | 4311 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4312 | `			/* Invalid argument */` |
|     ! 0 | 4313 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 4314 | `			return rc;` |
|       - | 4315 | `		}` |
|   76520 | 4316 | `		pIn++; /* Jump the dollar sign */` |
|       - | 4317 | `		/* Copy argument name */` |
|   76520 | 4318 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   76520 | 4319 | `		if( zDup == 0 ){` |
|     ! 0 | 4320 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 4321 | `			return SXERR_ABORT;` |
|       - | 4322 | `		}` |
|   76520 | 4323 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   76520 | 4324 | `		pIn++;` |
|   76520 | 4325 | `		if( pIn < pEnd ){` |
|   47354 | 4326 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 4327 | `				SyToken *pDefend;` |
|   41646 | 4328 | `				sxi32 iNest = 0;` |
|   41646 | 4329 | `				pIn++; /* Jump the equal sign */` |
|   41646 | 4330 | `				pDefend = pIn;` |
|       - | 4331 | `				/* Process the default value associated with this argument */` |
|   88492 | 4332 | `				while( pDefend < pEnd ){` |
|   67664 | 4333 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   20818 | 4334 | `						break;` |
|       - | 4335 | `					}` |
|   46848 | 4336 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 4337 | `						/* Increment nesting level */` |
|    2604 | 4338 | `						iNest++;` |
|   45547 | 4339 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 4340 | `						/* Decrement nesting level */` |
|    2604 | 4341 | `						iNest--;` |
|    1301 | 4342 | `					}` |
|   46848 | 4343 | `					pDefend++;` |
|       2 | 4344 | `				}` |
|   41646 | 4345 | `				if( pIn >= pDefend ){` |
|       3 | 4346 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 4347 | `					return rc;` |
|       - | 4348 | `				}` |
|       - | 4349 | `				/* Process default value */` |
|   41644 | 4350 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   41644 | 4351 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 4352 | `					return rc;` |
|       - | 4353 | `				}` |
|       - | 4354 | `				/* Point beyond the default value */` |
|   41644 | 4355 | `				pIn = pDefend;` |
|   20821 | 4356 | `			}` |
|   47352 | 4357 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 4358 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 4359 | `				return rc;` |
|       - | 4360 | `			}` |
|   47352 | 4361 | `			pIn++; /* Jump the trailing comma */` |
|   23675 | 4362 | `		}` |
|       - | 4363 | `		/* Append argument signature */` |
|   76518 | 4364 | `		if( sArg.nType > 0 ){` |
|   52070 | 4365 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 4366 | `				/* Class name */` |
|    5210 | 4367 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2606 | 4368 | `			}else{` |
|       - | 4369 | `				int c;` |
|   46862 | 4370 | `				c = 'n'; /* cc warning */` |
|       - | 4371 | `				/* Type leading character */` |
|   46862 | 4372 | `				switch(sArg.nType){` |
|     ! 0 | 4373 | `				case MEMOBJ_HASHMAP:` |
|       - | 4374 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 4375 | `					c = 'h';` |
|     ! 0 | 4376 | `					break;` |
|    6509 | 4377 | `				case MEMOBJ_INT:` |
|       - | 4378 | `					/* Integer */` |
|   13020 | 4379 | `					c = 'i';` |
|   13020 | 4380 | `					break;` |
|     ! 0 | 4381 | `				case MEMOBJ_BOOL:` |
|       - | 4382 | `					/* Bool */` |
|     ! 0 | 4383 | `					c = 'b';` |
|     ! 0 | 4384 | `					break;` |
|     ! 0 | 4385 | `				case MEMOBJ_REAL:` |
|       - | 4386 | `					/* Float */` |
|     ! 0 | 4387 | `					c = 'f';` |
|     ! 0 | 4388 | `					break;` |
|   16921 | 4389 | `				case MEMOBJ_STRING:` |
|       - | 4390 | `					/* String */` |
|   33844 | 4391 | `					c = 's';` |
|   33842 | 4392 | `					break;` |
|     ! 0 | 4393 | `				default:` |
|     ! 0 | 4394 | `					break;` |
|       - | 4395 | `				}` |
|   46862 | 4396 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 4397 | `			}` |
|   26036 | 4398 | `		}else{` |
|       - | 4399 | `			/* No type is associated with this parameter which mean` |
|       - | 4400 | `			 * that this function is not condidate for overloading.` |
|       - | 4401 | `			 */` |
|   24450 | 4402 | `			SyBlobRelease(&sSig);` |
|       - | 4403 | `		}` |
|       - | 4404 | `		/* Save in the argument set */` |
|   76518 | 4405 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 4406 | `	}` |
|   49996 | 4407 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 4408 | `		/* Save function signature */` |
|   31248 | 4409 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   15623 | 4410 | `	}` |
|   49996 | 4411 | `	return SXRET_OK;` |
|   25000 | 4412 |  |
|       - | 4413 | `/*` |
|       - | 4414 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 4415 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 4416 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 4417 | ` */` |
|  138970 | 4418 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 4419 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 4420 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 4421 | `	)` |
|       2 | 4422 |  |
|       - | 4423 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 4424 | `	GenBlock *pBlock;` |
|       - | 4425 | `	sxu32 nGotoOfft;` |
|       - | 4426 | `	sxi32 rc;` |
|       - | 4427 | `	/* Attach the new function */` |
|  138972 | 4428 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  138972 | 4429 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4430 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 4431 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4432 | `		return SXERR_ABORT;` |
|       - | 4433 | `	}` |
|  138972 | 4434 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 4435 | `	/* Swap bytecode containers */` |
|  138972 | 4436 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  138972 | 4437 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 4438 | `	/* Compile the body */` |
|  138972 | 4439 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 4440 | `	/* Fix exception jumps now the destination is resolved */` |
|  138972 | 4441 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4442 | `	/* Emit the final return if not yet done */` |
|  138972 | 4443 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4444 | `	/* Fix gotos jumps now the destination is resolved */` |
|  138972 | 4445 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 4446 | `		rc = SXERR_ABORT;` |
|     ! 0 | 4447 | `	}` |
|  138972 | 4448 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 4449 | `	/* Restore the default container */` |
|  138972 | 4450 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4451 | `	/* Leave function block */` |
|  138972 | 4452 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  138972 | 4453 | `	if( rc == SXERR_ABORT ){` |
|       - | 4454 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4455 | `		return SXERR_ABORT;` |
|       - | 4456 | `	}` |
|       - | 4457 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - | 4458 | `	{` |
|  138972 | 4459 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - | 4460 | `		sxu32 i;` |
| 2885522 | 4461 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 2746568 | 4462 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      18 | 4463 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      18 | 4464 | `				break;` |
|       - | 4465 | `			}` |
| 1373277 | 4466 | `		}` |
|       - | 4467 | `	}` |
|       - | 4468 | `	/* All done, function body compiled */` |
|  138972 | 4469 | `	return SXRET_OK;` |
|   69487 | 4470 |  |
|       - | 4471 | `/*` |
|       - | 4472 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 4473 | ` * According to the PHP language reference manual.` |
|       - | 4474 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 4475 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 4476 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 4477 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4478 | ` *  Functions need not be defined before they are referenced.` |
|       - | 4479 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 4480 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 4481 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 4482 | ` *  calls with over 32-64 recursion levels.` |
|       - | 4483 | ` *` |
|       - | 4484 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 4485 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 4486 | ` * on these extension.` |
|       - | 4487 | ` */` |
|       - | 4488 | `/*` |
|       - | 4489 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - | 4490 | ` */` |
|       6 | 4491 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       1 | 4492 |  |
|       - | 4493 | `	sxu32 i;` |
|      31 | 4494 | `	for( i = 0; i < n; i++ ){` |
|      25 | 4495 | `		int a = zA[i], b = zB[i];` |
|      25 | 4496 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      25 | 4497 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      25 | 4498 | `		if( a != b ) return a - b;` |
|      13 | 4499 | `	}` |
|       7 | 4500 | `	return 0;` |
|       4 | 4501 |  |
|       - | 4502 | `/*` |
|       - | 4503 | ` * Helper: set the return type to a class/self/parent/static sentinel.` |
|       - | 4504 | ` */` |
|       2 | 4505 | `static void GenStateSetReturnClass(ph7_gen_state *pGen, ph7_vm_func *pFunc, const char *zName, sxu32 nByte)` |
|       1 | 4506 |  |
|       3 | 4507 | `	char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator, zName, nByte);` |
|       3 | 4508 | `	if( zDup ){` |
|       3 | 4509 | `		pFunc->nReturnType = SXU32_HIGH;` |
|       3 | 4510 | `		SyStringInitFromBuf(&pFunc->sReturnClass, zDup, nByte);` |
|       1 | 4511 | `	}` |
|       3 | 4512 |  |
|       - | 4513 | `/*` |
|       - | 4514 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - | 4515 | `` * pGen->pIn should point to the token after `)`.`` |
|       - | 4516 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - | 4517 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - | 4518 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, and nullable `: ?type`.`` |
|       - | 4519 | ` */` |
|  159834 | 4520 | `static void GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 | 4521 |  |
|  159836 | 4522 | `	SyToken *pCur = pGen->pIn;` |
|  159836 | 4523 | `	pFunc->nReturnType = 0;` |
|  159836 | 4524 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  159836 | 4525 | `	if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_COLON) == 0 ){` |
|  159782 | 4526 | `		return; /* No return type */` |
|       - | 4527 | `	}` |
|      55 | 4528 | `	pCur++; /* Skip ':' */` |
|      55 | 4529 | `	if( pCur >= pGen->pEnd ){` |
|     ! 0 | 4530 | `		pGen->pIn = pCur;` |
|     ! 0 | 4531 | `		return;` |
|       - | 4532 | `	}` |
|       - | 4533 | `	/* Handle nullable prefix '?' (tokenized as PH7_TK_OP with '?' operator) */` |
|      55 | 4534 | `	if( (pCur->nType & PH7_TK_OP) && pCur->sData.nByte == 1 && pCur->sData.zString[0] == '?' ){` |
|       7 | 4535 | `		pCur++;` |
|       7 | 4536 | `		if( pCur >= pGen->pEnd ){` |
|     ! 0 | 4537 | `			pGen->pIn = pCur;` |
|     ! 0 | 4538 | `			return;` |
|       - | 4539 | `		}` |
|       3 | 4540 | `	}` |
|      55 | 4541 | `	if( pCur->nType & PH7_TK_KEYWORD ){` |
|      49 | 4542 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pCur->pUserData));` |
|      49 | 4543 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|       3 | 4544 | `			pFunc->nReturnType = MEMOBJ_HASHMAP;` |
|      48 | 4545 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       3 | 4546 | `			pFunc->nReturnType = MEMOBJ_BOOL;` |
|      46 | 4547 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|      17 | 4548 | `			pFunc->nReturnType = MEMOBJ_INT;` |
|      37 | 4549 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|      25 | 4550 | `			pFunc->nReturnType = MEMOBJ_STRING;` |
|      17 | 4551 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       3 | 4552 | `			pFunc->nReturnType = MEMOBJ_REAL;` |
|       4 | 4553 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT \|\| nKey == PH7_TKWRD_STATIC ){` |
|       - | 4554 | `			/* self/parent/static — store as class sentinel */` |
|       3 | 4555 | `			GenStateSetReturnClass(pGen, pFunc, pCur->sData.zString, pCur->sData.nByte);` |
|       1 | 4556 | `		}` |
|      49 | 4557 | `		pCur++;` |
|      31 | 4558 | `	}else if( pCur->nType & PH7_TK_ID ){` |
|       7 | 4559 | `		SyString *pType = &pCur->sData;` |
|       7 | 4560 | `		if( pType->nByte == 4 && SyMemcmpNoCase(pType->zString, "void", 4) == 0 ){` |
|       7 | 4561 | `			pFunc->nReturnType = MEMOBJ_VOID;` |
|       4 | 4562 | `		}else{` |
|       - | 4563 | `			/* Class/interface name */` |
|     ! 0 | 4564 | `			GenStateSetReturnClass(pGen, pFunc, pType->zString, pType->nByte);` |
|       - | 4565 | `		}` |
|       7 | 4566 | `		pCur++;` |
|       3 | 4567 | `	}` |
|      55 | 4568 | `	pGen->pIn = pCur;` |
|   79919 | 4569 |  |
|       - | 4570 |  |
|   34490 | 4571 | `static sxi32 GenStateCompileFunc(` |
|       - | 4572 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4573 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 4574 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 4575 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 4576 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 4577 | `	)` |
|       2 | 4578 |  |
|       - | 4579 | `	ph7_vm_func *pFunc;` |
|       - | 4580 | `	SyToken *pEnd;` |
|       - | 4581 | `	sxu32 nLine;` |
|       - | 4582 | `	char *zName;` |
|       - | 4583 | `	sxi32 rc;` |
|       - | 4584 | `	/* Extract line number */` |
|   34492 | 4585 | `	nLine = pGen->pIn->nLine;` |
|       - | 4586 | `	/* Jump the left parenthesis '(' */` |
|   34492 | 4587 | `	pGen->pIn++;` |
|       - | 4588 | `	/* Delimit the function signature */` |
|   34492 | 4589 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   34492 | 4590 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4591 | `		/* Syntax error */` |
|       7 | 4592 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 4593 | `		if( rc == SXERR_ABORT ){` |
|       - | 4594 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4595 | `			return SXERR_ABORT;` |
|       - | 4596 | `		}` |
|       7 | 4597 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 4598 | `		return SXRET_OK;` |
|       - | 4599 | `	}` |
|       - | 4600 | `	/* Create the function state */` |
|   34486 | 4601 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   34486 | 4602 | `	if( pFunc == 0 ){` |
|     ! 0 | 4603 | `		goto OutOfMem;` |
|       - | 4604 | `	}` |
|       - | 4605 | `	/* Build the function name, prepending namespace if active */` |
|   34493 | 4606 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 4607 | `		SyBlob sFQN;` |
|       - | 4608 | `		sxu32 nLen;` |
|      16 | 4609 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 | 4610 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 | 4611 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 | 4612 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 | 4613 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 | 4614 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 | 4615 | `		SyBlobRelease(&sFQN);` |
|      16 | 4616 | `		if( zName == 0 ){` |
|     ! 0 | 4617 | `			goto OutOfMem;` |
|       - | 4618 | `		}` |
|      16 | 4619 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 | 4620 | `	}else{` |
|   34472 | 4621 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   34472 | 4622 | `		if( zName == 0 ){` |
|     ! 0 | 4623 | `			goto OutOfMem;` |
|       - | 4624 | `		}` |
|   34472 | 4625 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 4626 | `	}` |
|   34486 | 4627 | `	if( pGen->pIn < pEnd ){` |
|       - | 4628 | `		/* Collect function arguments */` |
|   23898 | 4629 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   23898 | 4630 | `		if( rc == SXERR_ABORT ){` |
|       - | 4631 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4632 | `			return SXERR_ABORT;` |
|       - | 4633 | `		}` |
|   11948 | 4634 | `	}` |
|       - | 4635 | `	/* Point past ')' and parse optional return type ': type' */` |
|   34486 | 4636 | `	pGen->pIn = &pEnd[1];` |
|   34486 | 4637 | `	GenStateParseReturnType(pGen, pFunc);` |
|   34486 | 4638 | `	if( bHandleClosure ){` |
|       - | 4639 | `		ph7_vm_func_closure_env sEnv;` |
|     168 | 4640 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     166 | 4641 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      91 | 4642 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      14 | 4643 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 4644 | `				/* Closure,record environment variable */` |
|      14 | 4645 | `				pGen->pIn++;` |
|      14 | 4646 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 4647 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 4648 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4649 | `						return SXERR_ABORT;` |
|       - | 4650 | `					}` |
|     ! 0 | 4651 | `				}` |
|      14 | 4652 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 4653 | `				/* Compile until we hit the first closing parenthesis */` |
|      28 | 4654 | `				while( pGen->pIn < pGen->pEnd ){` |
|      28 | 4655 | `					int iFlagsLocal = 0;` |
|      28 | 4656 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      14 | 4657 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      14 | 4658 | `						break;` |
|       - | 4659 | `					}` |
|      16 | 4660 | `					nLineLocal = pGen->pIn->nLine;` |
|      16 | 4661 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 4662 | `						/* Pass by reference,record that */` |
|     ! 0 | 4663 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 4664 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 4665 | `							);` |
|     ! 0 | 4666 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 4667 | `						pGen->pIn++;` |
|     ! 0 | 4668 | `					}` |
|      14 | 4669 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      16 | 4670 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 4671 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 4672 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 4673 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4674 | `								return SXERR_ABORT;` |
|       - | 4675 | `							}` |
|       - | 4676 | `							/* Find the closing parenthesis */` |
|     ! 0 | 4677 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 4678 | `								pGen->pIn++;` |
|     ! 0 | 4679 | `							}` |
|     ! 0 | 4680 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 4681 | `								pGen->pIn++;` |
|     ! 0 | 4682 | `							}` |
|     ! 0 | 4683 | `							break;` |
|       - | 4684 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 4685 | `					}else{` |
|       - | 4686 | `						SyString *pNameLocal;` |
|       - | 4687 | `						char *zDup;` |
|       - | 4688 | `						/* Duplicate variable name */` |
|      16 | 4689 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      16 | 4690 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      16 | 4691 | `						if( zDup ){` |
|       - | 4692 | `							/* Zero the structure */` |
|      16 | 4693 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 | 4694 | `							sEnv.iFlags = iFlagsLocal;` |
|      16 | 4695 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 | 4696 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      16 | 4697 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 4698 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 4699 | `									got_this = 1;` |
|     ! 0 | 4700 | `							}` |
|       - | 4701 | `							/* Save imported variable */` |
|      16 | 4702 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       9 | 4703 | `						}else{` |
|     ! 0 | 4704 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4705 | `							 return SXERR_ABORT;` |
|       - | 4706 | `						}` |
|       - | 4707 | `					}` |
|      16 | 4708 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      18 | 4709 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4710 | `						/* Ignore trailing commas */` |
|       3 | 4711 | `						pGen->pIn++;` |
|       1 | 4712 | `					}` |
|       2 | 4713 | `				}` |
|      14 | 4714 | `				if( !got_this ){` |
|       - | 4715 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 4716 | `					 * available to the closure environment.` |
|       - | 4717 | `					 */` |
|      14 | 4718 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      14 | 4719 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      14 | 4720 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      14 | 4721 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      14 | 4722 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       6 | 4723 | `				}` |
|      14 | 4724 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 4725 | `					/* Mark as closure */` |
|      14 | 4726 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       6 | 4727 | `				}` |
|       6 | 4728 | `		}` |
|      83 | 4729 | `	}` |
|       - | 4730 | `	/* Compile the body */` |
|   34486 | 4731 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   34486 | 4732 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4733 | `		return SXERR_ABORT;` |
|       - | 4734 | `	}` |
|   34486 | 4735 | `	if( ppFunc ){` |
|     168 | 4736 | `		*ppFunc = pFunc;` |
|      83 | 4737 | `	}` |
|   34486 | 4738 | `	rc = SXRET_OK;` |
|   34486 | 4739 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 4740 | `		/* Finally register the function */` |
|   34474 | 4741 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   17236 | 4742 | `	}` |
|   34486 | 4743 | `	if( rc == SXRET_OK ){` |
|   34486 | 4744 | `		return SXRET_OK;` |
|       - | 4745 | `	}` |
|       - | 4746 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 4747 | `OutOfMem:` |
|       - | 4748 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 4749 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 4750 | `	 */` |
|     ! 0 | 4751 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 4752 | `	return SXERR_ABORT;` |
|   17247 | 4753 |  |
|       - | 4754 | `/*` |
|       - | 4755 | ` * Compile a standard PHP function.` |
|       - | 4756 | ` *  Refer to the block-comment above for more information.` |
|       - | 4757 | ` */` |
|   34330 | 4758 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 4759 |  |
|       - | 4760 | `	SyString *pName;` |
|       - | 4761 | `	sxi32 iFlags;` |
|       - | 4762 | `	sxu32 nLine;` |
|       - | 4763 | `	sxi32 rc;` |
|       - | 4764 |  |
|   34332 | 4765 | `	nLine = pGen->pIn->nLine;` |
|   34332 | 4766 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   34332 | 4767 | `	iFlags = 0;` |
|   34332 | 4768 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4769 | `		/* Return by reference,remember that */` |
|       7 | 4770 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4771 | `		/* Jump the '&' token */` |
|       7 | 4772 | `		pGen->pIn++;` |
|       3 | 4773 | `	}` |
|   34332 | 4774 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4775 | `		/* Invalid function name */` |
|       5 | 4776 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 4777 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4778 | `			return SXERR_ABORT;` |
|       - | 4779 | `		}` |
|       - | 4780 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 4781 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 4782 | `			pGen->pIn++;` |
|       1 | 4783 | `		}` |
|       5 | 4784 | `		return SXRET_OK;` |
|       - | 4785 | `	}` |
|   34328 | 4786 | `	pName = &pGen->pIn->sData;` |
|   34328 | 4787 | `	nLine = pGen->pIn->nLine;` |
|       - | 4788 | `	/* Jump the function name */` |
|   34328 | 4789 | `	pGen->pIn++;` |
|   34328 | 4790 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4791 | `		/* Syntax error */` |
|       3 | 4792 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 4793 | `		if( rc == SXERR_ABORT ){` |
|       - | 4794 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4795 | `			return SXERR_ABORT;` |
|       - | 4796 | `		}` |
|       - | 4797 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 4798 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4799 | `			pGen->pIn++;` |
|     ! 0 | 4800 | `		}` |
|       3 | 4801 | `		return SXRET_OK;` |
|       - | 4802 | `	}` |
|       - | 4803 | `	/* Compile function body */` |
|   34326 | 4804 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   34326 | 4805 | `	return rc;` |
|   17167 | 4806 |  |
|       - | 4807 | `/*` |
|       - | 4808 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 4809 | ` * According to the PHP language reference manual` |
|       - | 4810 | ` *  Visibility:` |
|       - | 4811 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 4812 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 4813 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 4814 | ` *  Members declared protected can be accessed only within the class` |
|       - | 4815 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 4816 | ` *  may only be accessed by the class that defines the member.` |
|       - | 4817 | ` */` |
|  159400 | 4818 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 4819 |  |
|  159402 | 4820 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    7868 | 4821 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  151536 | 4822 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   18244 | 4823 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 4824 | `	}` |
|       - | 4825 | `	/* Assume public by default */` |
|  133294 | 4826 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   79702 | 4827 |  |
|       - | 4828 | `/*` |
|       - | 4829 | ` * Compile a class constant.` |
|       - | 4830 | ` * According to the PHP language reference manual` |
|       - | 4831 | ` *  Class Constants` |
|       - | 4832 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 4833 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 4834 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 4835 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 4836 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 4837 | ` *   It's also possible for interfaces to have constants.` |
|       - | 4838 | ` * Symisc eXtension.` |
|       - | 4839 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 4840 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4841 | ` *  Example:` |
|       - | 4842 | ` *   class Test{` |
|       - | 4843 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4844 | ` *   };` |
|       - | 4845 | ` *   var_dump(TEST::MyConst);` |
|       - | 4846 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4847 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4848 | ` */` |
|      10 | 4849 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4850 |  |
|      12 | 4851 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4852 | `	SySet *pInstrContainer;` |
|       - | 4853 | `	ph7_class_attr *pCons;` |
|       - | 4854 | `	SyString *pName;` |
|       - | 4855 | `	sxi32 rc;` |
|       - | 4856 | `	/* Extract visibility level */` |
|      12 | 4857 | `	iProtection = GetProtectionLevel(iProtection);` |
|      12 | 4858 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       5 | 4859 | `loop:` |
|       - | 4860 | `	/* Mark as constant */` |
|      12 | 4861 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      12 | 4862 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4863 | `		/* Invalid constant name */` |
|     ! 0 | 4864 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 4865 | `		if( rc == SXERR_ABORT ){` |
|       - | 4866 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4867 | `			return SXERR_ABORT;` |
|       - | 4868 | `		}` |
|     ! 0 | 4869 | `		goto Synchronize;` |
|       - | 4870 | `	}` |
|       - | 4871 | `	/* Peek constant name */` |
|      12 | 4872 | `	pName = &pGen->pIn->sData;` |
|       - | 4873 | `	/* Make sure the constant name isn't reserved */` |
|      12 | 4874 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 4875 | `		/* Reserved constant name */` |
|     ! 0 | 4876 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 4877 | `		if( rc == SXERR_ABORT ){` |
|       - | 4878 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4879 | `			return SXERR_ABORT;` |
|       - | 4880 | `		}` |
|     ! 0 | 4881 | `		goto Synchronize;` |
|       - | 4882 | `	}` |
|       - | 4883 | `	/* Advance the stream cursor */` |
|      12 | 4884 | `	pGen->pIn++;` |
|      12 | 4885 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 4886 | `		/* Invalid declaration */` |
|     ! 0 | 4887 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 4888 | `		if( rc == SXERR_ABORT ){` |
|       - | 4889 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4890 | `			return SXERR_ABORT;` |
|       - | 4891 | `		}` |
|     ! 0 | 4892 | `		goto Synchronize;` |
|       - | 4893 | `	}` |
|      12 | 4894 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 4895 | `	/* Allocate a new class attribute */` |
|      12 | 4896 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      12 | 4897 | `	if( pCons == 0 ){` |
|     ! 0 | 4898 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4899 | `		return SXERR_ABORT;` |
|       - | 4900 | `	}` |
|       - | 4901 | `	/* Swap bytecode container */` |
|      12 | 4902 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      12 | 4903 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 4904 | `	/* Compile constant value.` |
|       - | 4905 | `	 */` |
|      12 | 4906 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      12 | 4907 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 4908 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 4909 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4910 | `			return SXERR_ABORT;` |
|       - | 4911 | `		}` |
|       1 | 4912 | `	}` |
|       - | 4913 | `	/* Emit the done instruction */` |
|      12 | 4914 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      12 | 4915 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      12 | 4916 | `	if( rc == SXERR_ABORT ){` |
|       - | 4917 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4918 | `		return SXERR_ABORT;` |
|       - | 4919 | `	}` |
|       - | 4920 | `	/* All done,install the constant */` |
|      12 | 4921 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      12 | 4922 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4923 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4924 | `		return SXERR_ABORT;` |
|       - | 4925 | `	}` |
|      12 | 4926 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4927 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 4928 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4929 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 4930 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4931 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4932 | `				pTok--;` |
|     ! 0 | 4933 | `			}` |
|     ! 0 | 4934 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4935 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 4936 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4937 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4938 | `				return SXERR_ABORT;` |
|       - | 4939 | `			}` |
|     ! 0 | 4940 | `		}else{` |
|     ! 0 | 4941 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 4942 | `				goto loop;` |
|       - | 4943 | `			}` |
|       - | 4944 | `		}` |
|     ! 0 | 4945 | `	}` |
|      12 | 4946 | `	return SXRET_OK;` |
|     ! 0 | 4947 | `Synchronize:` |
|       - | 4948 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4949 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 4950 | `		pGen->pIn++;` |
|     ! 0 | 4951 | `	}` |
|     ! 0 | 4952 | `	return SXERR_CORRUPT;` |
|       7 | 4953 |  |
|       - | 4954 | `/*` |
|       - | 4955 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 4956 | ` * According to the PHP language reference manual` |
|       - | 4957 | ` *  Properties` |
|       - | 4958 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 4959 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 4960 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 4961 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 4962 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 4963 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 4964 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 4965 | ` * Symisc eXtension.` |
|       - | 4966 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 4967 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4968 | ` *  Example:` |
|       - | 4969 | ` *   class Test{` |
|       - | 4970 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4971 | ` *   };` |
|       - | 4972 | ` *   var_dump(TEST::myVar);` |
|       - | 4973 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4974 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4975 | ` */` |
|   34038 | 4976 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4977 |  |
|   34040 | 4978 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4979 | `	ph7_class_attr *pAttr;` |
|       - | 4980 | `	SyString *pName;` |
|       - | 4981 | `	sxi32 rc;` |
|       - | 4982 | `	/* Extract visibility level */` |
|   34040 | 4983 | `	iProtection = GetProtectionLevel(iProtection);` |
|   17019 | 4984 | `loop:` |
|   34040 | 4985 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   34040 | 4986 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 4987 | `		/* Invalid attribute name */` |
|     ! 0 | 4988 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 4989 | `		if( rc == SXERR_ABORT ){` |
|       - | 4990 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4991 | `			return SXERR_ABORT;` |
|       - | 4992 | `		}` |
|     ! 0 | 4993 | `		goto Synchronize;` |
|       - | 4994 | `	}` |
|       - | 4995 | `	/* Peek attribute name */` |
|   34040 | 4996 | `	pName = &pGen->pIn->sData;` |
|       - | 4997 | `	/* Advance the stream cursor */` |
|   34040 | 4998 | `	pGen->pIn++;` |
|   34040 | 4999 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 5000 | `		/* Invalid declaration */` |
|       3 | 5001 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 5002 | `		if( rc == SXERR_ABORT ){` |
|       - | 5003 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5004 | `			return SXERR_ABORT;` |
|       - | 5005 | `		}` |
|       3 | 5006 | `		goto Synchronize;` |
|       - | 5007 | `	}` |
|       - | 5008 | `	/* Allocate a new class attribute */` |
|   34038 | 5009 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   34038 | 5010 | `	if( pAttr == 0 ){` |
|     ! 0 | 5011 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5012 | `		return SXERR_ABORT;` |
|       - | 5013 | `	}` |
|   34038 | 5014 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 5015 | `		SySet *pInstrContainer;` |
|   10570 | 5016 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 5017 | `		/* Swap bytecode container */` |
|   10570 | 5018 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   10570 | 5019 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 5020 | `		/* Compile attribute value.` |
|       - | 5021 | `		 */` |
|   10570 | 5022 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   10570 | 5023 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 5024 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 5025 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5026 | `				return SXERR_ABORT;` |
|       - | 5027 | `			}` |
|     ! 0 | 5028 | `		}` |
|       - | 5029 | `		/* Emit the done instruction */` |
|   10570 | 5030 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   10570 | 5031 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5284 | 5032 | `	}` |
|       - | 5033 | `	/* All done,install the attribute */` |
|   34038 | 5034 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   34038 | 5035 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5036 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5037 | `		return SXERR_ABORT;` |
|       - | 5038 | `	}` |
|   34038 | 5039 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 5040 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 5041 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 5042 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 5043 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 5044 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 5045 | `				pTok--;` |
|     ! 0 | 5046 | `			}` |
|     ! 0 | 5047 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5048 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5049 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 5050 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5051 | `				return SXERR_ABORT;` |
|       - | 5052 | `			}` |
|     ! 0 | 5053 | `		}else{` |
|     ! 0 | 5054 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 5055 | `				goto loop;` |
|       - | 5056 | `			}` |
|       - | 5057 | `		}` |
|     ! 0 | 5058 | `	}` |
|   34038 | 5059 | `	return SXRET_OK;` |
|       1 | 5060 | `Synchronize:` |
|       - | 5061 | `	/* Synchronize with the first semi-colon */` |
|       5 | 5062 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 5063 | `		pGen->pIn++;` |
|       1 | 5064 | `	}` |
|       3 | 5065 | `	return SXERR_CORRUPT;` |
|   17021 | 5066 |  |
|       - | 5067 | `/*` |
|       - | 5068 | ` * Compile a class method.` |
|       - | 5069 | ` *` |
|       - | 5070 | ` * Refer to the official documentation for more information` |
|       - | 5071 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 5072 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 5073 | ` * overloading and many more.` |
|       - | 5074 | ` */` |
|  125352 | 5075 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 5076 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 5077 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 5078 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 5079 | `	int doBody,          /* TRUE to process method body */` |
|       - | 5080 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 5081 | `	)` |
|       2 | 5082 |  |
|  125354 | 5083 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5084 | `	ph7_class_method *pMeth;` |
|       - | 5085 | `	sxi32 iFuncFlags;` |
|       - | 5086 | `	SyString *pName;` |
|       - | 5087 | `	SyToken *pEnd;` |
|       - | 5088 | `	sxi32 rc;` |
|       - | 5089 | `	/* Extract visibility level */` |
|  125354 | 5090 | `	iProtection = GetProtectionLevel(iProtection);` |
|  125354 | 5091 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  125354 | 5092 | `	iFuncFlags = 0;` |
|  125354 | 5093 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5094 | `		/* Invalid method name */` |
|     ! 0 | 5095 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 5096 | `		if( rc == SXERR_ABORT ){` |
|       - | 5097 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5098 | `			return SXERR_ABORT;` |
|       - | 5099 | `		}` |
|     ! 0 | 5100 | `		goto Synchronize;` |
|       - | 5101 | `	}` |
|  125354 | 5102 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 5103 | `		/* Return by reference,remember that */` |
|     ! 0 | 5104 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 5105 | `		/* Jump the '&' token */` |
|     ! 0 | 5106 | `		pGen->pIn++;` |
|     ! 0 | 5107 | `	}` |
|  125354 | 5108 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5109 | `		/* Invalid method name */` |
|     ! 0 | 5110 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 5111 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5112 | `			return SXERR_ABORT;` |
|       - | 5113 | `		}` |
|     ! 0 | 5114 | `		goto Synchronize;` |
|       - | 5115 | `	}` |
|       - | 5116 | `	/* Peek method name */` |
|  125354 | 5117 | `	pName = &pGen->pIn->sData;` |
|  125354 | 5118 | `	nLine = pGen->pIn->nLine;` |
|       - | 5119 | `	/* Jump the method name */` |
|  125354 | 5120 | `	pGen->pIn++;` |
|  125354 | 5121 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 5122 | `		/* Abstract method */` |
|   20866 | 5123 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 5124 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5125 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 5126 | `				&pClass->sName,pName);` |
|     ! 0 | 5127 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5128 | `				return SXERR_ABORT;` |
|       - | 5129 | `			}` |
|     ! 0 | 5130 | `		}` |
|       - | 5131 | `		/* Assemble method signature only */` |
|   20866 | 5132 | `		doBody = FALSE;` |
|   10432 | 5133 | `	}` |
|  125354 | 5134 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 5135 | `		/* Syntax error */` |
|     ! 0 | 5136 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 5137 | `		if( rc == SXERR_ABORT ){` |
|       - | 5138 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5139 | `			return SXERR_ABORT;` |
|       - | 5140 | `		}` |
|     ! 0 | 5141 | `		goto Synchronize;` |
|       - | 5142 | `	}` |
|       - | 5143 | `	/* Allocate a new class_method instance */` |
|  125354 | 5144 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  125354 | 5145 | `	if( pMeth == 0 ){` |
|     ! 0 | 5146 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5147 | `		return SXERR_ABORT;` |
|       - | 5148 | `	}` |
|       - | 5149 | `	/* Jump the left parenthesis '(' */` |
|  125354 | 5150 | `	pGen->pIn++;` |
|  125354 | 5151 | `	pEnd = 0; /* cc warning */` |
|       - | 5152 | `	/* Delimit the method signature */` |
|  125354 | 5153 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  125354 | 5154 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5155 | `		/* Syntax error */` |
|       3 | 5156 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 5157 | `		if( rc == SXERR_ABORT ){` |
|       - | 5158 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5159 | `			return SXERR_ABORT;` |
|       - | 5160 | `		}` |
|       3 | 5161 | `		goto Synchronize;` |
|       - | 5162 | `	}` |
|  125352 | 5163 | `	if( pGen->pIn < pEnd ){` |
|       - | 5164 | `		/* Collect method arguments */` |
|   26102 | 5165 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   26102 | 5166 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5167 | `			return SXERR_ABORT;` |
|       - | 5168 | `		}` |
|   13050 | 5169 | `	}` |
|       - | 5170 | `	/* Point past ')' and parse optional return type ': type' */` |
|  125352 | 5171 | `	pGen->pIn = &pEnd[1];` |
|  125352 | 5172 | `	GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  125352 | 5173 | `	if( doBody ){` |
|       - | 5174 | `		/* Compile method body */` |
|  104488 | 5175 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  104488 | 5176 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5177 | `			return SXERR_ABORT;` |
|       - | 5178 | `		}` |
|   52245 | 5179 | `	}else{` |
|       - | 5180 | `		/* Only method signature is allowed */` |
|   20866 | 5181 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 5182 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5183 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 5184 | `				if( rc == SXERR_ABORT ){` |
|       - | 5185 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5186 | `					return SXERR_ABORT;` |
|       - | 5187 | `				}` |
|     ! 0 | 5188 | `				return SXERR_CORRUPT;` |
|       - | 5189 | `			}` |
|       - | 5190 | `	}` |
|       - | 5191 | `	/* All done,install the method */` |
|  125352 | 5192 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  125352 | 5193 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5194 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5195 | `		return SXERR_ABORT;` |
|       - | 5196 | `	}` |
|  125352 | 5197 | `	return SXRET_OK;` |
|       1 | 5198 | `Synchronize:` |
|       - | 5199 | `	/* Synchronize with the first semi-colon */` |
|       7 | 5200 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 5201 | `		pGen->pIn++;` |
|       1 | 5202 | `	}` |
|       3 | 5203 | `	return SXERR_CORRUPT;` |
|   62678 | 5204 |  |
|       - | 5205 | `/*` |
|       - | 5206 | ` * Compile an object interface.` |
|       - | 5207 | ` *  According to the PHP language reference manual` |
|       - | 5208 | ` *   Object Interfaces:` |
|       - | 5209 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 5210 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 5211 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 5212 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 5213 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 5214 | ` */` |
|    7842 | 5215 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 5216 |  |
|    7844 | 5217 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5218 | `	ph7_class *pClass,*pBase;` |
|       - | 5219 | `	SyToken *pEnd,*pTmp;` |
|       - | 5220 | `	SyString *pName;` |
|       - | 5221 | `	sxi32 nKwrd;` |
|       - | 5222 | `	sxi32 rc;` |
|       - | 5223 | `	/* Jump the 'interface' keyword */` |
|    7844 | 5224 | `	pGen->pIn++;` |
|       - | 5225 | `	/* Extract interface name */` |
|    7844 | 5226 | `	pName = &pGen->pIn->sData;` |
|       - | 5227 | `	/* Advance the stream cursor */` |
|    7844 | 5228 | `	pGen->pIn++;` |
|       - | 5229 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5230 | `		SyBlob sFQN;` |
|       - | 5231 | `		SyString sFQNStr;` |
|    7844 | 5232 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    7844 | 5233 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    7844 | 5234 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    7844 | 5235 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    7844 | 5236 | `		SyBlobRelease(&sFQN);` |
|       - | 5237 | `	}` |
|    7844 | 5238 | `	if( pClass == 0 ){` |
|     ! 0 | 5239 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5240 | `		return SXERR_ABORT;` |
|       - | 5241 | `	}` |
|       - | 5242 | `	/* Mark as an interface */` |
|    7844 | 5243 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 5244 | `	/* Assume no base class is given */` |
|    7844 | 5245 | `	pBase = 0;` |
|    7844 | 5246 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 5247 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 5248 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 5249 | `			SyString *pBaseName;` |
|       - | 5250 | `			/* Extract base interface */` |
|       3 | 5251 | `			pGen->pIn++;` |
|       3 | 5252 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5253 | `				/* Syntax error */` |
|     ! 0 | 5254 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5255 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 5256 | `					pName);` |
|     ! 0 | 5257 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5258 | `				if( rc == SXERR_ABORT ){` |
|       - | 5259 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5260 | `					return SXERR_ABORT;` |
|       - | 5261 | `				}` |
|     ! 0 | 5262 | `				return SXRET_OK;` |
|       - | 5263 | `			}` |
|       3 | 5264 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5265 | `			{` |
|       - | 5266 | `				SyBlob sResolved;` |
|       3 | 5267 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 5268 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 | 5269 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 5270 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 5271 | `				SyBlobRelease(&sResolved);` |
|       - | 5272 | `			}` |
|       - | 5273 | `			/* Only interfaces is allowed */` |
|       3 | 5274 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5275 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5276 | `			}` |
|       3 | 5277 | `			if( pBase == 0 ){` |
|       - | 5278 | `				/* Inexistant interface */` |
|     ! 0 | 5279 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 5280 | `				if( rc == SXERR_ABORT ){` |
|       - | 5281 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5282 | `					return SXERR_ABORT;` |
|       - | 5283 | `				}` |
|     ! 0 | 5284 | `			}` |
|       - | 5285 | `			/* Advance the stream cursor */` |
|       3 | 5286 | `			pGen->pIn++;` |
|       1 | 5287 | `		}` |
|       1 | 5288 | `	}` |
|    7844 | 5289 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5290 | `		/* Syntax error */` |
|     ! 0 | 5291 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 5292 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5293 | `		if( rc == SXERR_ABORT ){` |
|       - | 5294 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5295 | `			return SXERR_ABORT;` |
|       - | 5296 | `		}` |
|     ! 0 | 5297 | `		return SXRET_OK;` |
|       - | 5298 | `	}` |
|    7844 | 5299 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    7844 | 5300 | `	pEnd = 0; /* cc warning */` |
|       - | 5301 | `	/* Delimit the interface body */` |
|    7844 | 5302 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    7844 | 5303 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5304 | `		/* Syntax error */` |
|     ! 0 | 5305 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 5306 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5307 | `		if( rc == SXERR_ABORT ){` |
|       - | 5308 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5309 | `			return SXERR_ABORT;` |
|       - | 5310 | `		}` |
|     ! 0 | 5311 | `		return SXRET_OK;` |
|       - | 5312 | `	}` |
|       - | 5313 | `	/* Swap token stream */` |
|    7844 | 5314 | `	pTmp = pGen->pEnd;` |
|    7844 | 5315 | `	pGen->pEnd = pEnd;` |
|       - | 5316 | `	/* Start the parse process` |
|       - | 5317 | `	 * Note (According to the PHP reference manual):` |
|       - | 5318 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 5319 | `	 *  Only 'public' visibility is allowed.` |
|       - | 5320 | `	 */` |
|   14348 | 5321 | `	for(;;){` |
|       - | 5322 | `		/* Jump leading/trailing semi-colons */` |
|   49552 | 5323 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   20856 | 5324 | `			pGen->pIn++;` |
|       2 | 5325 | `		}` |
|   28698 | 5326 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5327 | `			/* End of interface body */` |
|    7844 | 5328 | `			break;` |
|       - | 5329 | `		}` |
|   20856 | 5330 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5331 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5332 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 5333 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5334 | `			if( rc == SXERR_ABORT ){` |
|       - | 5335 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5336 | `				return SXERR_ABORT;` |
|       - | 5337 | `			}` |
|     ! 0 | 5338 | `			goto done;` |
|       - | 5339 | `		}` |
|       - | 5340 | `		/* Extract the current keyword */` |
|   20856 | 5341 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   20856 | 5342 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 5343 | `			/* Emit a warning and switch to public visibility */` |
|     ! 0 | 5344 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"interface: Access type must be public");` |
|     ! 0 | 5345 | `			nKwrd = PH7_TKWRD_PUBLIC;` |
|     ! 0 | 5346 | `		}` |
|   20856 | 5347 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5348 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5349 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5350 | `			if( rc == SXERR_ABORT ){` |
|       - | 5351 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5352 | `				return SXERR_ABORT;` |
|       - | 5353 | `			}` |
|     ! 0 | 5354 | `			goto done;` |
|       - | 5355 | `		}` |
|   20856 | 5356 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 5357 | `			/* Advance the stream cursor */` |
|   20852 | 5358 | `			pGen->pIn++;` |
|   20852 | 5359 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5360 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5361 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5362 | `				if( rc == SXERR_ABORT ){` |
|       - | 5363 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5364 | `					return SXERR_ABORT;` |
|       - | 5365 | `				}` |
|     ! 0 | 5366 | `				goto done;` |
|       - | 5367 | `			}` |
|   20852 | 5368 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   20852 | 5369 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5370 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5371 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5372 | `				if( rc == SXERR_ABORT ){` |
|       - | 5373 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5374 | `					return SXERR_ABORT;` |
|       - | 5375 | `				}` |
|     ! 0 | 5376 | `				goto done;` |
|       - | 5377 | `			}` |
|   10425 | 5378 | `		}` |
|   20856 | 5379 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5380 | `			/* Parse constant */` |
|       3 | 5381 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 5382 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5383 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5384 | `					return SXERR_ABORT;` |
|       - | 5385 | `				}` |
|     ! 0 | 5386 | `				goto done;` |
|       - | 5387 | `			}` |
|       2 | 5388 | `		}else{` |
|   20854 | 5389 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   20854 | 5390 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5391 | `				/* Static method,record that */` |
|     ! 0 | 5392 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 5393 | `				/* Advance the stream cursor */` |
|     ! 0 | 5394 | `				pGen->pIn++;` |
|     ! 0 | 5395 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 5396 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5397 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5398 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5399 | `						if( rc == SXERR_ABORT ){` |
|       - | 5400 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5401 | `							return SXERR_ABORT;` |
|       - | 5402 | `						}` |
|     ! 0 | 5403 | `						goto done;` |
|       - | 5404 | `				}` |
|     ! 0 | 5405 | `			}` |
|       - | 5406 | `			/* Process method signature (no body for interface methods) */` |
|   20854 | 5407 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   20854 | 5408 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5409 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5410 | `					return SXERR_ABORT;` |
|       - | 5411 | `				}` |
|     ! 0 | 5412 | `				goto done;` |
|       - | 5413 | `			}` |
|       - | 5414 | `		}` |
|       2 | 5415 | `	}` |
|       - | 5416 | `	/* Install the interface */` |
|    7844 | 5417 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    7844 | 5418 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 5419 | `		/* Inherit from the base interface */` |
|       3 | 5420 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 5421 | `	}` |
|    7844 | 5422 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5423 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5424 | `		return SXERR_ABORT;` |
|       - | 5425 | `	}` |
|    3921 | 5426 | `done:` |
|       - | 5427 | `	/* Point beyond the interface body */` |
|    7844 | 5428 | `	pGen->pIn  = &pEnd[1];` |
|    7844 | 5429 | `	pGen->pEnd = pTmp;` |
|    7844 | 5430 | `	return PH7_OK;` |
|    3923 | 5431 |  |
|       - | 5432 | `/*` |
|       - | 5433 | ` * Compile a user-defined class.` |
|       - | 5434 | ` * According to the PHP language reference manual` |
|       - | 5435 | ` *  class` |
|       - | 5436 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 5437 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 5438 | ` *  of the properties and methods belonging to the class.` |
|       - | 5439 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 5440 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 5441 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 5442 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 5443 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 5444 | ` *  (called "methods").` |
|       - | 5445 | ` */` |
|       - | 5446 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 5447 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 5448 | `struct TraitUseEntry {` |
|       - | 5449 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 5450 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 5451 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 5452 | `};` |
|       - | 5453 | `/*` |
|       - | 5454 | ` * Validate that methods implementing interface contracts have compatible` |
|       - | 5455 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - | 5456 | ` */` |
|   36838 | 5457 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5458 |  |
|       - | 5459 | `	ph7_class **apIface;` |
|       - | 5460 | `	sxu32 nIface,i;` |
|       - | 5461 | `	sxi32 rc;` |
|   36840 | 5462 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 | 5463 | `		return SXRET_OK;` |
|       - | 5464 | `	}` |
|   36840 | 5465 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   36840 | 5466 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   39480 | 5467 | `	for(i = 0; i < nIface; i++){` |
|    2642 | 5468 | `		ph7_class *pIface = apIface[i];` |
|       - | 5469 | `		SyHashEntry *pEntry;` |
|    2642 | 5470 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   15730 | 5471 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   13090 | 5472 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - | 5473 | `			ph7_class_method *pImplMeth;` |
|   13090 | 5474 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - | 5475 | `			/* Find the implementing method in the class */` |
|   13090 | 5476 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   13090 | 5477 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 | 5478 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - | 5479 | `			}` |
|       - | 5480 | `			/* Check visibility: interface methods must be implemented as public */` |
|   13076 | 5481 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 | 5482 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5483 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 | 5484 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 | 5485 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5486 | `					return SXERR_ABORT;` |
|       - | 5487 | `				}` |
|       1 | 5488 | `			}` |
|       - | 5489 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - | 5490 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - | 5491 | `			 */` |
|       - | 5492 | `			{` |
|   13076 | 5493 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   13076 | 5494 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   13076 | 5495 | `				int sigError = 0;` |
|   13076 | 5496 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 | 5497 | `					sigError = 1;` |
|   13075 | 5498 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - | 5499 | `					/* Extra parameters must all have default values */` |
|       5 | 5500 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - | 5501 | `					sxu32 k;` |
|       7 | 5502 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 | 5503 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 | 5504 | `							sigError = 1;` |
|       3 | 5505 | `							break;` |
|       - | 5506 | `						}` |
|       2 | 5507 | `					}` |
|       2 | 5508 | `				}` |
|   13076 | 5509 | `				if( sigError ){` |
|       - | 5510 | `					SyBlob sImplSig, sIfaceSig;` |
|       - | 5511 | `					ph7_vm_func_arg *aArgs;` |
|       - | 5512 | `					sxu32 j;` |
|       5 | 5513 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 | 5514 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - | 5515 | `					/* Build implementing method signature */` |
|       5 | 5516 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 | 5517 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 | 5518 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 | 5519 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 | 5520 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5521 | `					}` |
|       - | 5522 | `					/* Build interface method signature */` |
|       5 | 5523 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 | 5524 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 | 5525 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 | 5526 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 | 5527 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5528 | `					}` |
|       7 | 5529 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5530 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 | 5531 | `						&pClass->sName,pMName,` |
|       4 | 5532 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 | 5533 | `						&pIface->sName,pMName,` |
|       4 | 5534 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 | 5535 | `					SyBlobRelease(&sImplSig);` |
|       5 | 5536 | `					SyBlobRelease(&sIfaceSig);` |
|       5 | 5537 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5538 | `						return SXERR_ABORT;` |
|       - | 5539 | `					}` |
|       2 | 5540 | `				}` |
|       - | 5541 | `			}` |
|       2 | 5542 | `		}` |
|    1322 | 5543 | `	}` |
|   36840 | 5544 | `	return SXRET_OK;` |
|   18421 | 5545 |  |
|       - | 5546 | `/*` |
|       - | 5547 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - | 5548 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - | 5549 | ` */` |
|   36838 | 5550 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5551 |  |
|       - | 5552 | `	ph7_class_method *pMeth;` |
|       - | 5553 | `	SyHashEntry *pEntry;` |
|       - | 5554 | `	sxu32 nAbstract;` |
|       - | 5555 | `	SyBlob sMsg;` |
|       - | 5556 | `	sxi32 rc;` |
|       - | 5557 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   36840 | 5558 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      20 | 5559 | `		return SXRET_OK;` |
|       - | 5560 | `	}` |
|       - | 5561 | `	/* Count abstract methods */` |
|   36822 | 5562 | `	nAbstract = 0;` |
|   36822 | 5563 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  349562 | 5564 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  312742 | 5565 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  312742 | 5566 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 | 5567 | `			nAbstract++;` |
|       8 | 5568 | `		}` |
|       2 | 5569 | `	}` |
|   36822 | 5570 | `	if( nAbstract == 0 ){` |
|   36808 | 5571 | `		return SXRET_OK;` |
|       - | 5572 | `	}` |
|       - | 5573 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 | 5574 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 | 5575 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - | 5576 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 | 5577 | `		&pClass->sName,nAbstract,` |
|       7 | 5578 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 | 5579 | `		(nAbstract > 1 ? "s" : ""));` |
|       - | 5580 | `	/* Second pass: list methods with origins */` |
|       - | 5581 | `	{` |
|      15 | 5582 | `		sxu32 nListed = 0;` |
|      15 | 5583 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 | 5584 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 | 5585 | `			ph7_class *pOrigin = 0;` |
|       - | 5586 | `			SyString *pMName;` |
|      19 | 5587 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 | 5588 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 | 5589 | `				continue;` |
|       - | 5590 | `			}` |
|      17 | 5591 | `			pMName = &pMeth->sFunc.sName;` |
|      17 | 5592 | `			if( nListed > 0 ){` |
|       3 | 5593 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 | 5594 | `			}` |
|       - | 5595 | `			/* Find the origin of this abstract method.` |
|       - | 5596 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - | 5597 | `			 * inheritance chains) take precedence for interface-declared` |
|       - | 5598 | `			 * methods. Abstract class methods only win when the class` |
|       - | 5599 | `			 * itself declared the abstract method (not inherited from` |
|       - | 5600 | `			 * an interface). Trait methods are adopted into the using` |
|       - | 5601 | `			 * class's namespace.` |
|       - | 5602 | `			 */` |
|       - | 5603 | `			{` |
|       - | 5604 | `				ph7_class **apIface;` |
|       - | 5605 | `				ph7_class **apTrait;` |
|       - | 5606 | `				ph7_class *pWalk;` |
|       - | 5607 | `				sxu32 i;` |
|       - | 5608 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - | 5609 | `				 * (one that was written in the class body, not inherited from an` |
|       - | 5610 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - | 5611 | `				 */` |
|      17 | 5612 | `				if( pClass->pBase ){` |
|       9 | 5613 | `					pWalk = pClass->pBase;` |
|      17 | 5614 | `					while( pWalk ){` |
|       - | 5615 | `						ph7_class_method *pParentMeth;` |
|      11 | 5616 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 | 5617 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - | 5618 | `							/* Exclude methods that came from an interface anywhere` |
|       - | 5619 | `							 * in this class's ancestor chain.` |
|       - | 5620 | `							 */` |
|      11 | 5621 | `							int fromIface = 0;` |
|      11 | 5622 | `							ph7_class *pAnc = pWalk;` |
|      15 | 5623 | `							while( pAnc ){` |
|       - | 5624 | `								ph7_class **apPI;` |
|       - | 5625 | `								sxu32 j;` |
|      13 | 5626 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 | 5627 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 | 5628 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 | 5629 | `										fromIface = 1;` |
|       9 | 5630 | `										break;` |
|       - | 5631 | `									}` |
|     ! 0 | 5632 | `								}` |
|      13 | 5633 | `								if( fromIface ) break;` |
|       5 | 5634 | `								pAnc = pAnc->pBase;` |
|       1 | 5635 | `							}` |
|      11 | 5636 | `							if( !fromIface ){` |
|       3 | 5637 | `								pOrigin = pWalk;` |
|       3 | 5638 | `								break;` |
|       - | 5639 | `							}` |
|       4 | 5640 | `						}` |
|       9 | 5641 | `						pWalk = pWalk->pBase;` |
|       1 | 5642 | `					}` |
|       4 | 5643 | `				}` |
|       - | 5644 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - | 5645 | `				 * each interface's own parent chain for the deepest origin.` |
|       - | 5646 | `				 */` |
|      17 | 5647 | `				if( !pOrigin ){` |
|      15 | 5648 | `					pWalk = pClass;` |
|      37 | 5649 | `					while( pWalk && !pOrigin ){` |
|      23 | 5650 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 | 5651 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 | 5652 | `							ph7_class *pIface = apIface[i];` |
|      13 | 5653 | `							ph7_class *pDeepest = 0;` |
|      25 | 5654 | `							while( pIface ){` |
|      13 | 5655 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 | 5656 | `									pDeepest = pIface;` |
|       6 | 5657 | `								}` |
|      13 | 5658 | `								pIface = pIface->pBase;` |
|       1 | 5659 | `							}` |
|      13 | 5660 | `							if( pDeepest ){` |
|      13 | 5661 | `								pOrigin = pDeepest;` |
|      13 | 5662 | `								break;` |
|       - | 5663 | `							}` |
|     ! 0 | 5664 | `						}` |
|      23 | 5665 | `						pWalk = pWalk->pBase;` |
|       1 | 5666 | `					}` |
|       7 | 5667 | `				}` |
|       - | 5668 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 | 5669 | `				if( !pOrigin ){` |
|       3 | 5670 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 | 5671 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 | 5672 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 | 5673 | `							pOrigin = pClass;` |
|       3 | 5674 | `							break;` |
|       - | 5675 | `						}` |
|     ! 0 | 5676 | `					}` |
|       1 | 5677 | `				}` |
|       - | 5678 | `			}` |
|      17 | 5679 | `			if( pOrigin ){` |
|      17 | 5680 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 | 5681 | `			}else{` |
|       - | 5682 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 | 5683 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - | 5684 | `			}` |
|      17 | 5685 | `			nListed++;` |
|       1 | 5686 | `		}` |
|       - | 5687 | `	}` |
|      15 | 5688 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 | 5689 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 | 5690 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 | 5691 | `	SyBlobRelease(&sMsg);` |
|      15 | 5692 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5693 | `		return SXERR_ABORT;` |
|       - | 5694 | `	}` |
|      15 | 5695 | `	return SXRET_OK;` |
|   18421 | 5696 |  |
|   36842 | 5697 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 5698 |  |
|   36844 | 5699 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5700 | `	ph7_class *pClass,*pBase;` |
|       - | 5701 | `	SyToken *pEnd,*pTmp;` |
|       - | 5702 | `	sxi32 iProtection;` |
|       - | 5703 | `	SySet aInterfaces;` |
|       - | 5704 | `	SySet aUseEntries;` |
|       - | 5705 | `	sxi32 iAttrflags;` |
|       - | 5706 | `	SyString *pName;` |
|       - | 5707 | `	sxi32 nKwrd;` |
|       - | 5708 | `	sxi32 rc;` |
|       - | 5709 | `	/* Jump the 'class' keyword */` |
|   36844 | 5710 | `	pGen->pIn++;` |
|   36844 | 5711 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5712 | `		/* Syntax error */` |
|     ! 0 | 5713 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 5714 | `		if( rc == SXERR_ABORT ){` |
|       - | 5715 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5716 | `			return SXERR_ABORT;` |
|       - | 5717 | `		}` |
|       - | 5718 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 5719 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 5720 | `			pGen->pIn++;` |
|     ! 0 | 5721 | `		}` |
|     ! 0 | 5722 | `		return SXRET_OK;` |
|       - | 5723 | `	}` |
|       - | 5724 | `	/* Extract class name */` |
|   36844 | 5725 | `	pName = &pGen->pIn->sData;` |
|       - | 5726 | `	/* Advance the stream cursor */` |
|   36844 | 5727 | `	pGen->pIn++;` |
|       - | 5728 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5729 | `		SyBlob sFQN;` |
|       - | 5730 | `		SyString sFQNStr;` |
|   36844 | 5731 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   36844 | 5732 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   36844 | 5733 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   36844 | 5734 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   36844 | 5735 | `		SyBlobRelease(&sFQN);` |
|       - | 5736 | `	}` |
|   36844 | 5737 | `	if( pClass == 0 ){` |
|     ! 0 | 5738 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5739 | `		return SXERR_ABORT;` |
|       - | 5740 | `	}` |
|       - | 5741 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   36844 | 5742 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   36844 | 5743 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 5744 | `	/* Assume a standalone class */` |
|   36844 | 5745 | `	pBase = 0;` |
|   36844 | 5746 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5747 | `		SyString *pBaseName;` |
|   26124 | 5748 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   26124 | 5749 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   23486 | 5750 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   23486 | 5751 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5752 | `				/* Syntax error */` |
|     ! 0 | 5753 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5754 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 5755 | `					pName);` |
|     ! 0 | 5756 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5757 | `				if( rc == SXERR_ABORT ){` |
|       - | 5758 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5759 | `					return SXERR_ABORT;` |
|       - | 5760 | `				}` |
|     ! 0 | 5761 | `				return SXRET_OK;` |
|       - | 5762 | `			}` |
|       - | 5763 | `			/* Extract base class name and resolve through namespace/imports */` |
|   23486 | 5764 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5765 | `			{` |
|       - | 5766 | `				SyBlob sResolved;` |
|   23486 | 5767 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   23486 | 5768 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   35228 | 5769 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   23484 | 5770 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   23486 | 5771 | `				SyBlobRelease(&sResolved);` |
|       - | 5772 | `			}` |
|       - | 5773 | `			/* Interfaces are not allowed */` |
|   23486 | 5774 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 5775 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5776 | `			}` |
|   23486 | 5777 | `			if( pBase == 0 ){` |
|       - | 5778 | `				/* Inexistant base class */` |
|     ! 0 | 5779 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 5780 | `				if( rc == SXERR_ABORT ){` |
|       - | 5781 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5782 | `					return SXERR_ABORT;` |
|       - | 5783 | `				}` |
|     ! 0 | 5784 | `			}else{` |
|   23486 | 5785 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 5786 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 5787 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 5788 | `					if( rc == SXERR_ABORT ){` |
|       - | 5789 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5790 | `						return SXERR_ABORT;` |
|       - | 5791 | `					}` |
|     ! 0 | 5792 | `				}` |
|       - | 5793 | `			}` |
|       - | 5794 | `			/* Advance the stream cursor */` |
|   23486 | 5795 | `			pGen->pIn++;` |
|   11742 | 5796 | `		}` |
|   26124 | 5797 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 5798 | `			ph7_class *pInterface;` |
|       - | 5799 | `			SyString *pIntName;` |
|       - | 5800 | `			/* Interface implementation */` |
|    2642 | 5801 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    1320 | 5802 | `			for(;;){` |
|    2642 | 5803 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5804 | `					/* Syntax error */` |
|     ! 0 | 5805 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5806 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 5807 | `						pName);` |
|     ! 0 | 5808 | `					if( rc == SXERR_ABORT ){` |
|       - | 5809 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5810 | `						return SXERR_ABORT;` |
|       - | 5811 | `					}` |
|     ! 0 | 5812 | `					break;` |
|       - | 5813 | `				}` |
|       - | 5814 | `				/* Extract interface name and resolve through namespace/imports */` |
|    2642 | 5815 | `				pIntName = &pGen->pIn->sData;` |
|       - | 5816 | `				{` |
|       - | 5817 | `					SyBlob sResolved;` |
|    2642 | 5818 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    2642 | 5819 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|    5282 | 5820 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    2640 | 5821 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    2642 | 5822 | `					SyBlobRelease(&sResolved);` |
|       - | 5823 | `				}` |
|       - | 5824 | `				/* Only interfaces are allowed */` |
|    2642 | 5825 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5826 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 5827 | `				}` |
|    2642 | 5828 | `				if( pInterface == 0 ){` |
|       - | 5829 | `					/* Inexistant interface */` |
|     ! 0 | 5830 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 5831 | `					if( rc == SXERR_ABORT ){` |
|       - | 5832 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5833 | `						return SXERR_ABORT;` |
|       - | 5834 | `					}` |
|     ! 0 | 5835 | `				}else{` |
|       - | 5836 | `					/* Register interface */` |
|    2642 | 5837 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 5838 | `				}` |
|       - | 5839 | `				/* Advance the stream cursor */` |
|    2642 | 5840 | `				pGen->pIn++;` |
|    2642 | 5841 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    1322 | 5842 | `					break;` |
|       - | 5843 | `				}` |
|     ! 0 | 5844 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 5845 | `			}` |
|    1320 | 5846 | `		}` |
|   13061 | 5847 | `	}` |
|   36844 | 5848 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5849 | `		/* Syntax error */` |
|     ! 0 | 5850 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 5851 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5852 | `		if( rc == SXERR_ABORT ){` |
|       - | 5853 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5854 | `			return SXERR_ABORT;` |
|       - | 5855 | `		}` |
|     ! 0 | 5856 | `		return SXRET_OK;` |
|       - | 5857 | `	}` |
|   36844 | 5858 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   36844 | 5859 | `	pEnd = 0; /* cc warning */` |
|       - | 5860 | `	/* Delimit the class body */` |
|   36844 | 5861 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   36844 | 5862 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5863 | `		/* Syntax error */` |
|     ! 0 | 5864 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 5865 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5866 | `		if( rc == SXERR_ABORT ){` |
|       - | 5867 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5868 | `			return SXERR_ABORT;` |
|       - | 5869 | `		}` |
|     ! 0 | 5870 | `		return SXRET_OK;` |
|       - | 5871 | `	}` |
|       - | 5872 | `	/* Swap token stream */` |
|   36844 | 5873 | `	pTmp = pGen->pEnd;` |
|   36844 | 5874 | `	pGen->pEnd = pEnd;` |
|       - | 5875 | `	/* Set the inherited flags */` |
|   36844 | 5876 | `	pClass->iFlags = iFlags;` |
|       - | 5877 | `	/* Start the parse process */` |
|   70650 | 5878 | `	for(;;){` |
|       - | 5879 | `		/* Jump leading/trailing semi-colons */` |
|  209432 | 5880 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   34074 | 5881 | `			pGen->pIn++;` |
|       2 | 5882 | `		}` |
|  175360 | 5883 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5884 | `			/* End of class body */` |
|   36840 | 5885 | `			break;` |
|       - | 5886 | `		}` |
|  138522 | 5887 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5888 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5889 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5890 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5891 | `			if( rc == SXERR_ABORT ){` |
|       - | 5892 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5893 | `				return SXERR_ABORT;` |
|       - | 5894 | `			}` |
|     ! 0 | 5895 | `			goto done;` |
|       - | 5896 | `		}` |
|       - | 5897 | `		/* Assume public visibility */` |
|  138522 | 5898 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  138522 | 5899 | `		iAttrflags = 0;` |
|  138522 | 5900 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 5901 | `			/* Extract the current keyword */` |
|  138522 | 5902 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  138522 | 5903 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5904 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 5905 | `				TraitUseEntry sUse;` |
|      41 | 5906 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      41 | 5907 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      41 | 5908 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      28 | 5909 | `				for(;;){` |
|       - | 5910 | `					ph7_class *pTrait;` |
|       - | 5911 | `					SyString *pTraitName;` |
|      49 | 5912 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5913 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5914 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 5915 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5916 | `							return SXERR_ABORT;` |
|       - | 5917 | `						}` |
|     ! 0 | 5918 | `						break;` |
|       - | 5919 | `					}` |
|      49 | 5920 | `					pTraitName = &pGen->pIn->sData;` |
|       - | 5921 | `					/* Resolve trait name through namespace/imports */ {` |
|       - | 5922 | `						SyBlob sResolved;` |
|      49 | 5923 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      49 | 5924 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      97 | 5925 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      48 | 5926 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      49 | 5927 | `						SyBlobRelease(&sResolved);` |
|       - | 5928 | `					}` |
|       - | 5929 | `					/* Only traits are allowed */` |
|      49 | 5930 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5931 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 5932 | `					}` |
|      49 | 5933 | `					if( pTrait == 0 ){` |
|     ! 0 | 5934 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5935 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 | 5936 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5937 | `							return SXERR_ABORT;` |
|       - | 5938 | `						}` |
|     ! 0 | 5939 | `					}else{` |
|      49 | 5940 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 5941 | `					}` |
|      49 | 5942 | `					pGen->pIn++; /* Advance past trait name */` |
|      49 | 5943 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      21 | 5944 | `						break;` |
|       - | 5945 | `					}` |
|       9 | 5946 | `					pGen->pIn++; /* Jump the comma */` |
|       1 | 5947 | `				}` |
|       - | 5948 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      41 | 5949 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 5950 | `					SyToken *pBlock;` |
|       9 | 5951 | `					pGen->pIn++; /* Jump '{' */` |
|       9 | 5952 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 | 5953 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 | 5954 | `					sUse.pResolvEnd = pBlock;` |
|       9 | 5955 | `					if( pBlock < pGen->pEnd ){` |
|       9 | 5956 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 | 5957 | `					}else{` |
|     ! 0 | 5958 | `						pGen->pIn = pGen->pEnd;` |
|       - | 5959 | `					}` |
|       4 | 5960 | `				}` |
|      41 | 5961 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 5962 | `				/* The semicolon will be consumed by the outer loop */` |
|      41 | 5963 | `				continue;` |
|       - | 5964 | `			}` |
|  138482 | 5965 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  135774 | 5966 | `				iProtection = nKwrd;` |
|  135774 | 5967 | `				pGen->pIn++; /* Jump the visibility token */` |
|  135774 | 5968 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5969 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5970 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5971 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5972 | `					if( rc == SXERR_ABORT ){` |
|       - | 5973 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5974 | `						return SXERR_ABORT;` |
|       - | 5975 | `					}` |
|     ! 0 | 5976 | `					goto done;` |
|       - | 5977 | `				}` |
|  135774 | 5978 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5979 | `					/* Attribute declaration */` |
|   34018 | 5980 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   34018 | 5981 | `					if( rc != SXRET_OK ){` |
|       3 | 5982 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5983 | `							return SXERR_ABORT;` |
|       - | 5984 | `						}` |
|       3 | 5985 | `						goto done;` |
|       - | 5986 | `					}` |
|   34016 | 5987 | `					continue;` |
|       - | 5988 | `				}` |
|       - | 5989 | `				/* Extract the keyword */` |
|  101758 | 5990 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   50878 | 5991 | `			}` |
|  104466 | 5992 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5993 | `				/* Process constant declaration */` |
|      10 | 5994 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 | 5995 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5996 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5997 | `						return SXERR_ABORT;` |
|       - | 5998 | `					}` |
|     ! 0 | 5999 | `					goto done;` |
|       - | 6000 | `				}` |
|       6 | 6001 | `			}else{` |
|  104458 | 6002 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 6003 | `					/* Static method or attribute,record that */` |
|    2626 | 6004 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2626 | 6005 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2626 | 6006 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 6007 | `						/* Extract the keyword */` |
|    2622 | 6008 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2622 | 6009 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6010 | `							iProtection = nKwrd;` |
|     ! 0 | 6011 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 6012 | `						}` |
|    1310 | 6013 | `					}` |
|    2626 | 6014 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6015 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6016 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 6017 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6018 | `						if( rc == SXERR_ABORT ){` |
|       - | 6019 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6020 | `							return SXERR_ABORT;` |
|       - | 6021 | `						}` |
|     ! 0 | 6022 | `						goto done;` |
|       - | 6023 | `					}` |
|    2626 | 6024 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 6025 | `						/* Attribute declaration */` |
|       5 | 6026 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 6027 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6028 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6029 | `								return SXERR_ABORT;` |
|       - | 6030 | `							}` |
|     ! 0 | 6031 | `							goto done;` |
|       - | 6032 | `						}` |
|       5 | 6033 | `						continue;` |
|       - | 6034 | `					}` |
|       - | 6035 | `					/* Extract the keyword */` |
|    2622 | 6036 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  103144 | 6037 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 6038 | `					/* Abstract method,record that */` |
|      10 | 6039 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 6040 | `					/* Mark the whole class as abstract */` |
|      10 | 6041 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 6042 | `					/* Advance the stream cursor */` |
|      10 | 6043 | `					pGen->pIn++;` |
|      10 | 6044 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      10 | 6045 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      10 | 6046 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 | 6047 | `							iProtection = nKwrd;` |
|       8 | 6048 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 | 6049 | `						}` |
|       4 | 6050 | `					}` |
|      10 | 6051 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 6052 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 6053 | `							/* Static method */` |
|     ! 0 | 6054 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 6055 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 6056 | `					}` |
|      10 | 6057 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       8 | 6058 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6059 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6060 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 6061 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 6062 | `							if( rc == SXERR_ABORT ){` |
|       - | 6063 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 6064 | `								return SXERR_ABORT;` |
|       - | 6065 | `							}` |
|     ! 0 | 6066 | `							goto done;` |
|       - | 6067 | `					}` |
|      10 | 6068 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  101830 | 6069 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 6070 | `					/* final method ,record that */` |
|       5 | 6071 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 6072 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 6073 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 6074 | `						/* Extract the keyword */` |
|       5 | 6075 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6076 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6077 | `							iProtection = nKwrd;` |
|       5 | 6078 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 6079 | `						}` |
|       2 | 6080 | `					}` |
|       5 | 6081 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 6082 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 6083 | `							/* Static method */` |
|     ! 0 | 6084 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 6085 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 6086 | `					}` |
|       5 | 6087 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6088 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6089 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6090 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 6091 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 6092 | `							if( rc == SXERR_ABORT ){` |
|       - | 6093 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 6094 | `								return SXERR_ABORT;` |
|       - | 6095 | `							}` |
|     ! 0 | 6096 | `							goto done;` |
|       - | 6097 | `					}` |
|       5 | 6098 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6099 | `				}` |
|  104454 | 6100 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 6101 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6102 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 6103 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6104 | `						if( rc == SXERR_ABORT ){` |
|       - | 6105 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6106 | `							return SXERR_ABORT;` |
|       - | 6107 | `						}` |
|     ! 0 | 6108 | `						goto done;` |
|       - | 6109 | `				}` |
|  104454 | 6110 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 6111 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 6112 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 6113 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6114 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 6115 | `						if( rc == SXERR_ABORT ){` |
|       - | 6116 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6117 | `							return SXERR_ABORT;` |
|       - | 6118 | `						}` |
|     ! 0 | 6119 | `						goto done;` |
|       - | 6120 | `					}` |
|       - | 6121 | `					/* Attribute declaration */` |
|       7 | 6122 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 6123 | `				}else{` |
|       - | 6124 | `					/* Process method declaration */` |
|  104448 | 6125 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 6126 | `				}` |
|  104454 | 6127 | `				if( rc != SXRET_OK ){` |
|       3 | 6128 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6129 | `						return SXERR_ABORT;` |
|       - | 6130 | `					}` |
|       3 | 6131 | `					goto done;` |
|       - | 6132 | `				}` |
|       - | 6133 | `			}` |
|   52231 | 6134 | `		}else{` |
|       - | 6135 | `			/* Attribute declaration */` |
|     ! 0 | 6136 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6137 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6138 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6139 | `					return SXERR_ABORT;` |
|       - | 6140 | `				}` |
|     ! 0 | 6141 | `				goto done;` |
|       - | 6142 | `			}` |
|       - | 6143 | `		}` |
|       2 | 6144 | `	}` |
|       - | 6145 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 6146 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 6147 | `	 */` |
|       - | 6148 | `	{` |
|       - | 6149 | `		TraitUseEntry *apUse;` |
|       - | 6150 | `		sxu32 nU;` |
|   36840 | 6151 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   36880 | 6152 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      41 | 6153 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      41 | 6154 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      41 | 6155 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      41 | 6156 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 6157 | `			sxu32 nT;` |
|      41 | 6158 | `			if( !hasResolution ){` |
|       - | 6159 | `				/* No conflict resolution block: use standard trait application */` |
|      71 | 6160 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      39 | 6161 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      39 | 6162 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6163 | `						break;` |
|       - | 6164 | `					}` |
|      20 | 6165 | `				}` |
|      17 | 6166 | `			}else{` |
|       - | 6167 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 6168 | `				 * then use the block to resolve method conflicts.` |
|       - | 6169 | `				 */` |
|       - | 6170 | `				SyToken *pR;` |
|      19 | 6171 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 | 6172 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 6173 | `					ph7_class_attr *pAR;` |
|       - | 6174 | `					SyHashEntry *pER;` |
|       - | 6175 | `					SyString *pNR;` |
|      11 | 6176 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 | 6177 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 6178 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 6179 | `						pNR = &pAR->sName;` |
|     ! 0 | 6180 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 6181 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 6182 | `						}` |
|     ! 0 | 6183 | `					}` |
|      11 | 6184 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 | 6185 | `				}` |
|       - | 6186 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 | 6187 | `				pR = pUse->pResolvStart;` |
|      21 | 6188 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6189 | `					SyString sTrait,sMethod;` |
|       - | 6190 | `					ph7_class *pSrcTrait;` |
|       - | 6191 | `					ph7_class_method *pMeth;` |
|       - | 6192 | `					sxi32 nRKwrd;` |
|      33 | 6193 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6194 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6195 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6196 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6197 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6198 | `					sMethod = pR->sData;` |
|      13 | 6199 | `					pR++;` |
|      13 | 6200 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6201 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6202 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6203 | `							sTrait = sMethod;` |
|       7 | 6204 | `							pR++;` |
|       7 | 6205 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6206 | `							sMethod = pR->sData;` |
|       7 | 6207 | `							pR++;` |
|       3 | 6208 | `						}` |
|       3 | 6209 | `					}` |
|      13 | 6210 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6211 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6212 | `						continue;` |
|       - | 6213 | `					}` |
|      13 | 6214 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6215 | `					pR++;` |
|      13 | 6216 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 | 6217 | `						pSrcTrait = 0;` |
|       7 | 6218 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 | 6219 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 | 6220 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 | 6221 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 | 6222 | `								pSrcTrait = apTrait[nT];` |
|       5 | 6223 | `								break;` |
|       - | 6224 | `							}` |
|       2 | 6225 | `						}` |
|       5 | 6226 | `						if( pSrcTrait ){` |
|       5 | 6227 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 | 6228 | `							if( pMeth ){` |
|       5 | 6229 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 | 6230 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 | 6231 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 | 6232 | `								}` |
|       2 | 6233 | `							}` |
|       2 | 6234 | `						}` |
|       2 | 6235 | `					}` |
|      29 | 6236 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6237 | `				}` |
|       - | 6238 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 | 6239 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 6240 | `					ph7_class_method *pMR;` |
|       - | 6241 | `					SyHashEntry *pER;` |
|       - | 6242 | `					SyString *pNR;` |
|      11 | 6243 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 | 6244 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 | 6245 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 | 6246 | `						pNR = &pMR->sFunc.sName;` |
|      19 | 6247 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 | 6248 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 | 6249 | `						}` |
|       1 | 6250 | `					}` |
|       6 | 6251 | `				}` |
|       - | 6252 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 | 6253 | `				pR = pUse->pResolvStart;` |
|      21 | 6254 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6255 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 6256 | `					ph7_class *pSrcTrait;` |
|       - | 6257 | `					ph7_class_method *pMeth;` |
|      21 | 6258 | `					int hasQual = 0;` |
|       - | 6259 | `					sxi32 nRKwrd;` |
|      33 | 6260 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6261 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6262 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6263 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6264 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 | 6265 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6266 | `					sMethod = pR->sData;` |
|      13 | 6267 | `					pR++;` |
|      13 | 6268 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6269 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6270 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6271 | `							sTrait = sMethod;` |
|       7 | 6272 | `							hasQual = 1;` |
|       7 | 6273 | `							pR++;` |
|       7 | 6274 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6275 | `							sMethod = pR->sData;` |
|       7 | 6276 | `							pR++;` |
|       3 | 6277 | `						}` |
|       3 | 6278 | `					}` |
|      13 | 6279 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6280 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6281 | `						continue;` |
|       - | 6282 | `					}` |
|      13 | 6283 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6284 | `					pR++;` |
|      13 | 6285 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 | 6286 | `						sxi32 iNewVis = -1;` |
|       9 | 6287 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 | 6288 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 | 6289 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 | 6290 | `								iNewVis = nAK;` |
|       7 | 6291 | `								pR++;` |
|       3 | 6292 | `							}` |
|       3 | 6293 | `						}` |
|       9 | 6294 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 | 6295 | `							sAlias = pR->sData;` |
|       7 | 6296 | `							pR++;` |
|       3 | 6297 | `						}` |
|       9 | 6298 | `						pMeth = 0;` |
|       9 | 6299 | `						if( hasQual ){` |
|       3 | 6300 | `							pSrcTrait = 0;` |
|       5 | 6301 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 | 6302 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 | 6303 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 | 6304 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 | 6305 | `									pSrcTrait = apTrait[nT];` |
|       3 | 6306 | `									break;` |
|       - | 6307 | `								}` |
|       2 | 6308 | `							}` |
|       3 | 6309 | `							if( pSrcTrait ){` |
|       3 | 6310 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 | 6311 | `							}` |
|       2 | 6312 | `						}else{` |
|       7 | 6313 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 6314 | `						}` |
|       9 | 6315 | `						if( pMeth ){` |
|       9 | 6316 | `							if( sAlias.nByte > 0 ){` |
|       - | 6317 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 6318 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 6319 | `								 */` |
|       - | 6320 | `								ph7_class_method *pAlias;` |
|       - | 6321 | `								char *zAliasDup;` |
|       7 | 6322 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 | 6323 | `								if( pAlias ){` |
|       7 | 6324 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 | 6325 | `									if( iNewVis >= 0 ){` |
|       5 | 6326 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6327 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6328 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 | 6329 | `									}` |
|       7 | 6330 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 | 6331 | `									if( zAliasDup ){` |
|       7 | 6332 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 | 6333 | `									}` |
|       4 | 6334 | `								}` |
|       6 | 6335 | `							}else if( iNewVis >= 0 ){` |
|       - | 6336 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 6337 | `								ph7_class_method *pCopy;` |
|       3 | 6338 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 6339 | `								if( pCopy ){` |
|       3 | 6340 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 6341 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 6342 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6343 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6344 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 6345 | `									/* Replace the method in the class hash */` |
|       3 | 6346 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 6347 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 6348 | `								}` |
|       1 | 6349 | `							}` |
|       4 | 6350 | `						}` |
|       4 | 6351 | `						SXUNUSED(hasQual);` |
|       4 | 6352 | `					}` |
|      17 | 6353 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6354 | `				}` |
|       - | 6355 | `			}` |
|      41 | 6356 | `			SySetRelease(&pUse->aTraits);` |
|      21 | 6357 | `		}` |
|       - | 6358 | `	}` |
|       - | 6359 | `	/* Install the class */` |
|   36840 | 6360 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   36840 | 6361 | `	if( rc == SXRET_OK ){` |
|       - | 6362 | `		ph7_class **apInterface;` |
|       - | 6363 | `		sxu32 n;` |
|   36840 | 6364 | `		if( pBase ){` |
|       - | 6365 | `			/* Inherit from base class and mark as a subclass */` |
|   23486 | 6366 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   11742 | 6367 | `		}` |
|   36840 | 6368 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   39480 | 6369 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 6370 | `			/* Implements one or more interface */` |
|    2642 | 6371 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    2642 | 6372 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6373 | `				break;` |
|       - | 6374 | `			}` |
|    1322 | 6375 | `		}` |
|       - | 6376 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   36840 | 6377 | `		if( rc == SXRET_OK ){` |
|   36840 | 6378 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   36840 | 6379 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6380 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6381 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6382 | `				return SXERR_ABORT;` |
|       - | 6383 | `			}` |
|   18419 | 6384 | `		}` |
|       - | 6385 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   36840 | 6386 | `		if( rc == SXRET_OK ){` |
|   36840 | 6387 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   36840 | 6388 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6389 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6390 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6391 | `				return SXERR_ABORT;` |
|       - | 6392 | `			}` |
|   18419 | 6393 | `		}` |
|   18419 | 6394 | `	}` |
|   36840 | 6395 | `	SySetRelease(&aUseEntries);` |
|   36840 | 6396 | `	SySetRelease(&aInterfaces);` |
|   36840 | 6397 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6398 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6399 | `		return SXERR_ABORT;` |
|       - | 6400 | `	}` |
|   18419 | 6401 | `done:` |
|       - | 6402 | `	/* Point beyond the class body */` |
|   36844 | 6403 | `	pGen->pIn = &pEnd[1];` |
|   36844 | 6404 | `	pGen->pEnd = pTmp;` |
|   36844 | 6405 | `	return PH7_OK;` |
|   18423 | 6406 |  |
|       - | 6407 | `/*` |
|       - | 6408 | ` * Compile a user-defined abstract class.` |
|       - | 6409 | ` *  According to the PHP language reference manual` |
|       - | 6410 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 6411 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 6412 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 6413 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 6414 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 6415 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 6416 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 6417 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 6418 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 6419 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 6420 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 6421 | ` *   could differ.` |
|       - | 6422 | ` */` |
|      16 | 6423 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 6424 |  |
|       - | 6425 | `	sxi32 rc;` |
|      18 | 6426 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      18 | 6427 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      18 | 6428 | `	return rc;` |
|       2 | 6429 |  |
|       - | 6430 | `/*` |
|       - | 6431 | ` * Compile a user-defined final class.` |
|       - | 6432 | ` *  According to the PHP language reference manual` |
|       - | 6433 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 6434 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 6435 | ` *    final then it cannot be extended.` |
|       - | 6436 | ` */` |
|       2 | 6437 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 6438 |  |
|       - | 6439 | `	sxi32 rc;` |
|       3 | 6440 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 6441 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 6442 | `	return rc;` |
|       1 | 6443 |  |
|       - | 6444 | `/*` |
|       - | 6445 | ` * Compile a user-defined trait.` |
|       - | 6446 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 6447 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 6448 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 6449 | ` */` |
|      52 | 6450 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 | 6451 |  |
|      54 | 6452 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6453 | `	ph7_class *pClass;` |
|       - | 6454 | `	SyToken *pEnd,*pTmp;` |
|       - | 6455 | `	sxi32 iProtection;` |
|       - | 6456 | `	sxi32 iAttrflags;` |
|       - | 6457 | `	SyString *pName;` |
|       - | 6458 | `	sxi32 nKwrd;` |
|       - | 6459 | `	sxi32 rc;` |
|       - | 6460 | `	/* Jump the 'trait' keyword */` |
|      54 | 6461 | `	pGen->pIn++;` |
|      54 | 6462 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6463 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 6464 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6465 | `			return SXERR_ABORT;` |
|       - | 6466 | `		}` |
|     ! 0 | 6467 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 6468 | `			pGen->pIn++;` |
|     ! 0 | 6469 | `		}` |
|     ! 0 | 6470 | `		return SXRET_OK;` |
|       - | 6471 | `	}` |
|       - | 6472 | `	/* Extract trait name */` |
|      54 | 6473 | `	pName = &pGen->pIn->sData;` |
|      54 | 6474 | `	pGen->pIn++;` |
|       - | 6475 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 6476 | `		SyBlob sFQN;` |
|       - | 6477 | `		SyString sFQNStr;` |
|      54 | 6478 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      54 | 6479 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      54 | 6480 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      54 | 6481 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      54 | 6482 | `		SyBlobRelease(&sFQN);` |
|       - | 6483 | `	}` |
|      54 | 6484 | `	if( pClass == 0 ){` |
|     ! 0 | 6485 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6486 | `		return SXERR_ABORT;` |
|       - | 6487 | `	}` |
|       - | 6488 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      54 | 6489 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 6490 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 6491 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6492 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6493 | `			return SXERR_ABORT;` |
|       - | 6494 | `		}` |
|     ! 0 | 6495 | `		return SXRET_OK;` |
|       - | 6496 | `	}` |
|      54 | 6497 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      54 | 6498 | `	pEnd = 0;` |
|      54 | 6499 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      54 | 6500 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 6501 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 6502 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6503 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6504 | `			return SXERR_ABORT;` |
|       - | 6505 | `		}` |
|     ! 0 | 6506 | `		return SXRET_OK;` |
|       - | 6507 | `	}` |
|       - | 6508 | `	/* Swap token stream */` |
|      54 | 6509 | `	pTmp = pGen->pEnd;` |
|      54 | 6510 | `	pGen->pEnd = pEnd;` |
|       - | 6511 | `	/* Mark as trait */` |
|      54 | 6512 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 6513 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      53 | 6514 | `	for(;;){` |
|     144 | 6515 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      21 | 6516 | `			pGen->pIn++;` |
|       1 | 6517 | `		}` |
|     124 | 6518 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      54 | 6519 | `			break;` |
|       - | 6520 | `		}` |
|      71 | 6521 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6522 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6523 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6524 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 6525 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6526 | `				return SXERR_ABORT;` |
|       - | 6527 | `			}` |
|     ! 0 | 6528 | `			goto done;` |
|       - | 6529 | `		}` |
|      71 | 6530 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      71 | 6531 | `		iAttrflags = 0;` |
|      71 | 6532 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      71 | 6533 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      71 | 6534 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 6535 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 6536 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 6537 | `				for(;;){` |
|       - | 6538 | `					ph7_class *pUsedTrait;` |
|       - | 6539 | `					SyString *pUsedName;` |
|       5 | 6540 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6541 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6542 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 6543 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6544 | `							return SXERR_ABORT;` |
|       - | 6545 | `						}` |
|     ! 0 | 6546 | `						break;` |
|       - | 6547 | `					}` |
|       5 | 6548 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 6549 | `					{` |
|       - | 6550 | `						SyBlob sResolved;` |
|       5 | 6551 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 6552 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 6553 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 6554 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 6555 | `						SyBlobRelease(&sResolved);` |
|       - | 6556 | `					}` |
|       5 | 6557 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 6558 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 6559 | `					}` |
|       5 | 6560 | `					if( pUsedTrait == 0 ){` |
|       4 | 6561 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 6562 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 6563 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6564 | `							return SXERR_ABORT;` |
|       - | 6565 | `						}` |
|       2 | 6566 | `					}else{` |
|       3 | 6567 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 6568 | `					}` |
|       5 | 6569 | `					pGen->pIn++;` |
|       5 | 6570 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 6571 | `						break;` |
|       - | 6572 | `					}` |
|     ! 0 | 6573 | `					pGen->pIn++;` |
|     ! 0 | 6574 | `				}` |
|       5 | 6575 | `				continue;` |
|       - | 6576 | `			}` |
|      67 | 6577 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      63 | 6578 | `				iProtection = nKwrd;` |
|      63 | 6579 | `				pGen->pIn++;` |
|      63 | 6580 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6581 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6582 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6583 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6584 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6585 | `						return SXERR_ABORT;` |
|       - | 6586 | `					}` |
|     ! 0 | 6587 | `					goto done;` |
|       - | 6588 | `				}` |
|      63 | 6589 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 | 6590 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 | 6591 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6592 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6593 | `							return SXERR_ABORT;` |
|       - | 6594 | `						}` |
|     ! 0 | 6595 | `						goto done;` |
|       - | 6596 | `					}` |
|      11 | 6597 | `					continue;` |
|       - | 6598 | `				}` |
|      53 | 6599 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 | 6600 | `			}` |
|      57 | 6601 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 6602 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6603 | `					"Traits cannot have constants");` |
|     ! 0 | 6604 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6605 | `					return SXERR_ABORT;` |
|       - | 6606 | `				}` |
|     ! 0 | 6607 | `				goto done;` |
|     ! 0 | 6608 | `			}else{` |
|      57 | 6609 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 6610 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 6611 | `					pGen->pIn++;` |
|       5 | 6612 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 6613 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 6614 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6615 | `							iProtection = nKwrd;` |
|     ! 0 | 6616 | `							pGen->pIn++;` |
|     ! 0 | 6617 | `						}` |
|       1 | 6618 | `					}` |
|       5 | 6619 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6620 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6621 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 6622 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6623 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6624 | `							return SXERR_ABORT;` |
|       - | 6625 | `						}` |
|     ! 0 | 6626 | `						goto done;` |
|       - | 6627 | `					}` |
|       5 | 6628 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 6629 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 6630 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6631 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6632 | `								return SXERR_ABORT;` |
|       - | 6633 | `							}` |
|     ! 0 | 6634 | `							goto done;` |
|       - | 6635 | `						}` |
|       3 | 6636 | `						continue;` |
|       - | 6637 | `					}` |
|       3 | 6638 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 | 6639 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 | 6640 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 | 6641 | `					pGen->pIn++;` |
|       5 | 6642 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 6643 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6644 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6645 | `							iProtection = nKwrd;` |
|       5 | 6646 | `							pGen->pIn++;` |
|       2 | 6647 | `						}` |
|       2 | 6648 | `					}` |
|       5 | 6649 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6650 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6651 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6652 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 6653 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6654 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6655 | `							return SXERR_ABORT;` |
|       - | 6656 | `						}` |
|     ! 0 | 6657 | `						goto done;` |
|       - | 6658 | `					}` |
|       5 | 6659 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6660 | `				}` |
|      55 | 6661 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 6662 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6663 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 6664 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6665 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6666 | `						return SXERR_ABORT;` |
|       - | 6667 | `					}` |
|     ! 0 | 6668 | `					goto done;` |
|       - | 6669 | `				}` |
|      55 | 6670 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 6671 | `					pGen->pIn++;` |
|     ! 0 | 6672 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 6673 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6674 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 6675 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6676 | `							return SXERR_ABORT;` |
|       - | 6677 | `						}` |
|     ! 0 | 6678 | `						goto done;` |
|       - | 6679 | `					}` |
|     ! 0 | 6680 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6681 | `				}else{` |
|      55 | 6682 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 6683 | `				}` |
|      55 | 6684 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6685 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6686 | `						return SXERR_ABORT;` |
|       - | 6687 | `					}` |
|     ! 0 | 6688 | `					goto done;` |
|       - | 6689 | `				}` |
|       - | 6690 | `			}` |
|      28 | 6691 | `		}else{` |
|     ! 0 | 6692 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6693 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6694 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6695 | `					return SXERR_ABORT;` |
|       - | 6696 | `				}` |
|     ! 0 | 6697 | `				goto done;` |
|       - | 6698 | `			}` |
|       - | 6699 | `		}` |
|       1 | 6700 | `	}` |
|       - | 6701 | `	/* Install the trait */` |
|      54 | 6702 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      54 | 6703 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6704 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6705 | `		return SXERR_ABORT;` |
|       - | 6706 | `	}` |
|      26 | 6707 | `done:` |
|       - | 6708 | `	/* Point beyond the trait body */` |
|      54 | 6709 | `	pGen->pIn = &pEnd[1];` |
|      54 | 6710 | `	pGen->pEnd = pTmp;` |
|      54 | 6711 | `	return PH7_OK;` |
|      28 | 6712 |  |
|       - | 6713 | `/*` |
|       - | 6714 | ` * Compile a user-defined class.` |
|       - | 6715 | ` *  According to the PHP language reference manual` |
|       - | 6716 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 6717 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 6718 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 6719 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 6720 | ` *   and functions (called "methods").` |
|       - | 6721 | ` */` |
|   36824 | 6722 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 6723 |  |
|       - | 6724 | `	sxi32 rc;` |
|   36826 | 6725 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   36826 | 6726 | `	return rc;` |
|       2 | 6727 |  |
|       - | 6728 | `/*` |
|       - | 6729 | ` * Exception handling.` |
|       - | 6730 | ` *  According to the PHP language reference manual` |
|       - | 6731 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 6732 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 6733 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 6734 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 6735 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 6736 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 6737 | ` *    (or re-thrown) within a catch block.` |
|       - | 6738 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 6739 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 6740 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 6741 | ` *    been defined with set_exception_handler().` |
|       - | 6742 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 6743 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 6744 | ` */` |
|       - | 6745 | `/*` |
|       - | 6746 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 6747 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 6748 | ` * indicates failure.` |
|       - | 6749 | ` */` |
|    7838 | 6750 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 6751 |  |
|    7840 | 6752 | `	sxi32 rc = SXRET_OK;` |
|    7840 | 6753 | `	if( pRoot->pOp ){` |
|    7836 | 6754 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    3920 | 6755 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 6756 | `			/* Unexpected expression */` |
|     ! 0 | 6757 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6758 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 6759 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 6760 | `				rc = SXERR_INVALID;` |
|     ! 0 | 6761 | `			}` |
|       2 | 6762 | `		}` |
|    3921 | 6763 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 6764 | `		/* Unexpected expression */` |
|     ! 0 | 6765 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6766 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 6767 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 6768 | `			rc = SXERR_INVALID;` |
|     ! 0 | 6769 | `		}` |
|     ! 0 | 6770 | `	}` |
|    7840 | 6771 | `	return rc;` |
|       2 | 6772 |  |
|       - | 6773 | `/*` |
|       - | 6774 | ` * Compile a 'throw' statement.` |
|       - | 6775 | ` * throw: This is how you trigger an exception.` |
|       - | 6776 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 6777 | ` */` |
|    7838 | 6778 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 6779 |  |
|    7840 | 6780 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6781 | `	GenBlock *pBlock;` |
|       - | 6782 | `	sxu32 nIdx;` |
|       - | 6783 | `	sxi32 rc;` |
|    7840 | 6784 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 6785 | `	/* Compile the expression */` |
|    7840 | 6786 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    7840 | 6787 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 6788 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 6789 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6790 | `			return SXERR_ABORT;` |
|       - | 6791 | `		}` |
|     ! 0 | 6792 | `		return SXRET_OK;` |
|       - | 6793 | `	}` |
|    7840 | 6794 | `	pBlock = pGen->pCurrent;` |
|       - | 6795 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   36494 | 6796 | `	while(pBlock->pParent){` |
|   36490 | 6797 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    7836 | 6798 | `			break;` |
|       - | 6799 | `		}` |
|       - | 6800 | `		/* Point to the parent block */` |
|   28656 | 6801 | `		pBlock = pBlock->pParent;` |
|       2 | 6802 | `	}` |
|       - | 6803 | `	/* Emit the throw instruction */` |
|    7840 | 6804 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 6805 | `	/* Emit the jump */` |
|    7840 | 6806 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    7840 | 6807 | `	return SXRET_OK;` |
|    3921 | 6808 |  |
|       - | 6809 | `/*` |
|       - | 6810 | ` * Compile a 'catch' block.` |
|       - | 6811 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 6812 | ` * an object containing the exception information.` |
|       - | 6813 | ` */` |
|      56 | 6814 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 6815 |  |
|      58 | 6816 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6817 | `	ph7_exception_block sCatch;` |
|       - | 6818 | `	SySet *pInstrContainer;` |
|       - | 6819 | `	GenBlock *pCatch;` |
|       - | 6820 | `	SyToken *pToken;` |
|       - | 6821 | `	SyString *pName;` |
|       - | 6822 | `	char *zDup;` |
|       - | 6823 | `	sxi32 rc;` |
|      58 | 6824 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 6825 | `	/* Zero the structure */` |
|      58 | 6826 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 6827 | `	/* Initialize fields */` |
|      58 | 6828 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|      84 | 6829 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ \|\|` |
|      58 | 6830 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6831 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6832 | `			pToken = pGen->pIn;` |
|     ! 0 | 6833 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6834 | `				pToken--;` |
|     ! 0 | 6835 | `			}` |
|     ! 0 | 6836 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6837 | `				"Catch: Unexpected token '%z',excpecting class name",&pToken->sData);` |
|     ! 0 | 6838 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6839 | `				return SXERR_ABORT;` |
|       - | 6840 | `			}` |
|     ! 0 | 6841 | `			return SXERR_INVALID;` |
|       - | 6842 | `	}` |
|       - | 6843 | `	/* Extract the exception class */` |
|      58 | 6844 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|       - | 6845 | `	/* Duplicate class name */` |
|      58 | 6846 | `	pName = &pGen->pIn->sData;` |
|      58 | 6847 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      58 | 6848 | `	if( zDup == 0 ){` |
|     ! 0 | 6849 | `		goto Mem;` |
|       - | 6850 | `	}` |
|      58 | 6851 | `	SyStringInitFromBuf(&sCatch.sClass,zDup,pName->nByte);` |
|      58 | 6852 | `	pGen->pIn++;` |
|      84 | 6853 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      58 | 6854 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6855 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6856 | `			pToken = pGen->pIn;` |
|     ! 0 | 6857 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6858 | `				pToken--;` |
|     ! 0 | 6859 | `			}` |
|     ! 0 | 6860 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6861 | `				"Catch: Unexpected token '%z',expecting variable name",&pToken->sData);` |
|     ! 0 | 6862 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6863 | `				return SXERR_ABORT;` |
|       - | 6864 | `			}` |
|     ! 0 | 6865 | `			return SXERR_INVALID;` |
|       - | 6866 | `	}` |
|      58 | 6867 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 6868 | `	/* Duplicate instance name */` |
|      58 | 6869 | `	pName = &pGen->pIn->sData;` |
|      58 | 6870 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      58 | 6871 | `	if( zDup == 0 ){` |
|     ! 0 | 6872 | `		goto Mem;` |
|       - | 6873 | `	}` |
|      58 | 6874 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      58 | 6875 | `	pGen->pIn++;` |
|      58 | 6876 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 6877 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 6878 | `		pToken = pGen->pIn;` |
|     ! 0 | 6879 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6880 | `			pToken--;` |
|     ! 0 | 6881 | `		}` |
|     ! 0 | 6882 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6883 | `			"Catch: Unexpected token '%z',expecting right parenthesis ')'",&pToken->sData);` |
|     ! 0 | 6884 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6885 | `			return SXERR_ABORT;` |
|       - | 6886 | `		}` |
|     ! 0 | 6887 | `		return SXERR_INVALID;` |
|       - | 6888 | `	}` |
|       - | 6889 | `	/* Compile the block */` |
|      58 | 6890 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 6891 | `	/* Create the catch block */` |
|      58 | 6892 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      58 | 6893 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6894 | `		return SXERR_ABORT;` |
|       - | 6895 | `	}` |
|       - | 6896 | `	/* Swap bytecode container */` |
|      58 | 6897 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      58 | 6898 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 6899 | `	/* Compile the block */` |
|      58 | 6900 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 6901 | `	/* Fix forward jumps now the destination is resolved  */` |
|      58 | 6902 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6903 | `	/* Emit the DONE instruction */` |
|      58 | 6904 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6905 | `	/* Leave the block */` |
|      58 | 6906 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6907 | `	/* Restore the default container */` |
|      58 | 6908 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 6909 | `	/* Install the catch block */` |
|      58 | 6910 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      58 | 6911 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6912 | `		goto Mem;` |
|       - | 6913 | `	}` |
|      58 | 6914 | `	return SXRET_OK;` |
|     ! 0 | 6915 | `Mem:` |
|     ! 0 | 6916 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6917 | `	return SXERR_ABORT;` |
|      30 | 6918 |  |
|       - | 6919 | `/*` |
|       - | 6920 | ` * Compile a 'try' block.` |
|       - | 6921 | ` * A function using an exception should be in a "try" block.` |
|       - | 6922 | ` * If the exception does not trigger, the code will continue` |
|       - | 6923 | ` * as normal. However if the exception triggers, an exception` |
|       - | 6924 | ` * is "thrown".` |
|       - | 6925 | ` */` |
|      68 | 6926 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 6927 |  |
|       - | 6928 | `	ph7_exception *pException;` |
|      70 | 6929 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6930 | `	GenBlock *pTry;` |
|       - | 6931 | `	sxu32 nJmpIdx;` |
|       - | 6932 | `	sxi32 rc;` |
|       - | 6933 | `	/* Create the exception container */` |
|      70 | 6934 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|      70 | 6935 | `	if( pException == 0 ){` |
|     ! 0 | 6936 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 6937 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6938 | `		return SXERR_ABORT;` |
|       - | 6939 | `	}` |
|       - | 6940 | `	/* Zero the structure */` |
|      70 | 6941 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 6942 | `	/* Initialize fields */` |
|      70 | 6943 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|      70 | 6944 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      70 | 6945 | `	pException->iHasFinally = 0;` |
|      70 | 6946 | `	pException->iFinallyDone = 0;` |
|      70 | 6947 | `	pException->pVm = pGen->pVm;` |
|       - | 6948 | `	/* Create the try block */` |
|      70 | 6949 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      70 | 6950 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6951 | `		return SXERR_ABORT;` |
|       - | 6952 | `	}` |
|       - | 6953 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|      70 | 6954 | `	pTry->pUserData = pException;` |
|       - | 6955 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|      70 | 6956 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 6957 | `	/* Fix the jump later when the destination is resolved */` |
|      70 | 6958 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|      70 | 6959 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 6960 | `	/* Compile the block */` |
|      70 | 6961 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      70 | 6962 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6963 | `		return SXERR_ABORT;` |
|       - | 6964 | `	}` |
|       - | 6965 | `	/* Fix forward jumps now the destination is resolved */` |
|      70 | 6966 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6967 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|      70 | 6968 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 6969 | `	/* Leave the block */` |
|      70 | 6970 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6971 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|      70 | 6972 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      66 | 6973 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 6974 | `		/* Compile one or more catch blocks */` |
|      56 | 6975 | `		for(;;){` |
|     112 | 6976 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      90 | 6977 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      30 | 6978 | `					break;` |
|       - | 6979 | `			}` |
|      58 | 6980 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|      58 | 6981 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6982 | `				return SXERR_ABORT;` |
|       - | 6983 | `			}` |
|       2 | 6984 | `		}` |
|      28 | 6985 | `	}` |
|       - | 6986 | `	/* Compile optional finally block */` |
|      70 | 6987 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      36 | 6988 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 6989 | `		SySet *pInstrContainer;` |
|       - | 6990 | `		GenBlock *pFinBlock;` |
|      28 | 6991 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 6992 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      28 | 6993 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      28 | 6994 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6995 | `			return SXERR_ABORT;` |
|       - | 6996 | `		}` |
|       - | 6997 | `		/* Swap bytecode container */` |
|      28 | 6998 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      28 | 6999 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 7000 | `		/* Compile the finally body */` |
|      28 | 7001 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      28 | 7002 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7003 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 7004 | `			return SXERR_ABORT;` |
|       - | 7005 | `		}` |
|       - | 7006 | `		/* Fix forward jumps now the destination is resolved */` |
|      28 | 7007 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7008 | `		/* Emit DONE to terminate the finally block */` |
|      28 | 7009 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 7010 | `		/* Leave the block */` |
|      28 | 7011 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 7012 | `		/* Restore the default container */` |
|      28 | 7013 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      28 | 7014 | `		pException->iHasFinally = 1;` |
|      13 | 7015 | `	}` |
|       - | 7016 | `	/* Must have at least one catch or finally */` |
|      70 | 7017 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       3 | 7018 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 7019 | `			"Cannot use try without catch or finally");` |
|       3 | 7020 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7021 | `			return SXERR_ABORT;` |
|       - | 7022 | `		}` |
|       1 | 7023 | `	}` |
|      70 | 7024 | `	return SXRET_OK;` |
|      36 | 7025 |  |
|       - | 7026 | `/*` |
|       - | 7027 | ` * Compile a switch block.` |
|       - | 7028 | ` *  (See block-comment below for more information)` |
|       - | 7029 | ` */` |
|      98 | 7030 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 7031 |  |
|     100 | 7032 | `	sxi32 rc = SXRET_OK;` |
|     100 | 7033 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 7034 | `		/* Unexpected token */` |
|     ! 0 | 7035 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 7036 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7037 | `			return SXERR_ABORT;` |
|       - | 7038 | `		}` |
|     ! 0 | 7039 | `		pGen->pIn++;` |
|     ! 0 | 7040 | `	}` |
|     100 | 7041 | `	pGen->pIn++;` |
|       - | 7042 | `	/* First instruction to execute in this block. */` |
|     100 | 7043 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 7044 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 7045 | `	 * or the '}' token */` |
|     172 | 7046 | `	for(;;){` |
|     346 | 7047 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7048 | `			/* No more input to process */` |
|     ! 0 | 7049 | `			break;` |
|       - | 7050 | `		}` |
|     346 | 7051 | `		rc = SXRET_OK;` |
|     346 | 7052 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      68 | 7053 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      26 | 7054 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 7055 | `					/* Unexpected token */` |
|     ! 0 | 7056 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 7057 | `						&pGen->pIn->sData);` |
|     ! 0 | 7058 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7059 | `						return SXERR_ABORT;` |
|       - | 7060 | `					}` |
|       - | 7061 | `					/* FALL THROUGH */` |
|     ! 0 | 7062 | `				}` |
|      26 | 7063 | `				rc = SXERR_EOF;` |
|      26 | 7064 | `				break;` |
|       - | 7065 | `			}` |
|      23 | 7066 | `		}else{` |
|       - | 7067 | `			sxi32 nKwrd;` |
|       - | 7068 | `			/* Extract the keyword */` |
|     280 | 7069 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     280 | 7070 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      38 | 7071 | `				break;` |
|       - | 7072 | `			}` |
|     208 | 7073 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 7074 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 7075 | `					/* Unexpected token */` |
|     ! 0 | 7076 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 7077 | `						&pGen->pIn->sData);` |
|     ! 0 | 7078 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7079 | `						return SXERR_ABORT;` |
|       - | 7080 | `					}` |
|       - | 7081 | `					/* FALL THROUGH */` |
|     ! 0 | 7082 | `				}` |
|       - | 7083 | `				/* Block compiled */` |
|       3 | 7084 | `				break;` |
|       - | 7085 | `			}` |
|       - | 7086 | `		}` |
|       - | 7087 | `		/* Compile block */` |
|     248 | 7088 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     248 | 7089 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7090 | `			return SXERR_ABORT;` |
|       - | 7091 | `		}` |
|       2 | 7092 | `	}` |
|     100 | 7093 | `	return rc;` |
|      51 | 7094 |  |
|       - | 7095 | `/*` |
|       - | 7096 | ` * Compile a case eXpression.` |
|       - | 7097 | ` *  (See block-comment below for more information)` |
|       - | 7098 | ` */` |
|      80 | 7099 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 7100 |  |
|       - | 7101 | `	SySet *pInstrContainer;` |
|       - | 7102 | `	SyToken *pEnd,*pTmp;` |
|      82 | 7103 | `	sxi32 iNest = 0;` |
|       - | 7104 | `	sxi32 rc;` |
|       - | 7105 | `	/* Delimit the expression */` |
|      82 | 7106 | `	pEnd = pGen->pIn;` |
|     170 | 7107 | `	while( pEnd < pGen->pEnd ){` |
|     170 | 7108 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 7109 | `			/* Increment nesting level */` |
|       3 | 7110 | `			iNest++;` |
|     169 | 7111 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 7112 | `			/* Decrement nesting level */` |
|       3 | 7113 | `			iNest--;` |
|     167 | 7114 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      82 | 7115 | `			break;` |
|       - | 7116 | `		}` |
|      90 | 7117 | `		pEnd++;` |
|       2 | 7118 | `	}` |
|      82 | 7119 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 7120 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 7121 | `		if( rc == SXERR_ABORT ){` |
|       - | 7122 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7123 | `			return SXERR_ABORT;` |
|       - | 7124 | `		}` |
|     ! 0 | 7125 | `	}` |
|       - | 7126 | `	/* Swap token stream */` |
|      82 | 7127 | `	pTmp = pGen->pEnd;` |
|      82 | 7128 | `	pGen->pEnd = pEnd;` |
|      82 | 7129 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      82 | 7130 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      82 | 7131 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 7132 | `	/* Emit the done instruction */` |
|      82 | 7133 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      82 | 7134 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 7135 | `	/* Update token stream */` |
|      82 | 7136 | `	pGen->pIn  = pEnd;` |
|      82 | 7137 | `	pGen->pEnd = pTmp;` |
|      82 | 7138 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 7139 | `		return SXERR_ABORT;` |
|       - | 7140 | `	}` |
|      82 | 7141 | `	return SXRET_OK;` |
|      42 | 7142 |  |
|       - | 7143 | `/*` |
|       - | 7144 | ` * Compile the smart switch statement.` |
|       - | 7145 | ` * According to the PHP language reference manual` |
|       - | 7146 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 7147 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 7148 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 7149 | ` *  This is exactly what the switch statement is for.` |
|       - | 7150 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 7151 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 7152 | ` *  of the outer loop, use continue 2.` |
|       - | 7153 | ` *  Note that switch/case does loose comparision.` |
|       - | 7154 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 7155 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 7156 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 7157 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 7158 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 7159 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 7160 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 7161 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 7162 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 7163 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 7164 | ` *  list for the next case.` |
|       - | 7165 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 7166 | ` *  or floating-point numbers and strings.` |
|       - | 7167 | ` */` |
|      26 | 7168 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 7169 |  |
|       - | 7170 | `	GenBlock *pSwitchBlock;` |
|       - | 7171 | `	SyToken *pTmp,*pEnd;` |
|       - | 7172 | `	ph7_switch *pSwitch;` |
|       - | 7173 | `	sxu32 nToken;` |
|       - | 7174 | `	sxu32 nLine;` |
|       - | 7175 | `	sxi32 rc;` |
|      28 | 7176 | `	nLine = pGen->pIn->nLine;` |
|       - | 7177 | `	/* Jump the 'switch' keyword */` |
|      28 | 7178 | `	pGen->pIn++;` |
|      28 | 7179 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 7180 | `		/* Syntax error */` |
|     ! 0 | 7181 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 7182 | `		if( rc == SXERR_ABORT ){` |
|       - | 7183 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7184 | `			return SXERR_ABORT;` |
|       - | 7185 | `		}` |
|     ! 0 | 7186 | `		goto Synchronize;` |
|       - | 7187 | `	}` |
|       - | 7188 | `	/* Jump the left parenthesis '(' */` |
|      28 | 7189 | `	pGen->pIn++;` |
|      28 | 7190 | `	pEnd = 0; /* cc warning */` |
|       - | 7191 | `	/* Create the loop block */` |
|      41 | 7192 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      13 | 7193 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      28 | 7194 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7195 | `		return SXERR_ABORT;` |
|       - | 7196 | `	}` |
|       - | 7197 | `	/* Delimit the condition */` |
|      28 | 7198 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      28 | 7199 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 7200 | `		/* Empty expression */` |
|     ! 0 | 7201 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 7202 | `		if( rc == SXERR_ABORT ){` |
|       - | 7203 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7204 | `			return SXERR_ABORT;` |
|       - | 7205 | `		}` |
|     ! 0 | 7206 | `	}` |
|       - | 7207 | `	/* Swap token streams */` |
|      28 | 7208 | `	pTmp = pGen->pEnd;` |
|      28 | 7209 | `	pGen->pEnd = pEnd;` |
|       - | 7210 | `	/* Compile the expression */` |
|      28 | 7211 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      28 | 7212 | `	if( rc == SXERR_ABORT ){` |
|       - | 7213 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 7214 | `		return SXERR_ABORT;` |
|       - | 7215 | `	}` |
|       - | 7216 | `	/* Update token stream */` |
|      28 | 7217 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 7218 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 7219 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 7220 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7221 | `			return SXERR_ABORT;` |
|       - | 7222 | `		}` |
|     ! 0 | 7223 | `		pGen->pIn++;` |
|     ! 0 | 7224 | `	}` |
|      28 | 7225 | `	pGen->pIn  = &pEnd[1];` |
|      28 | 7226 | `	pGen->pEnd = pTmp;` |
|      28 | 7227 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      26 | 7228 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 7229 | `			pTmp = pGen->pIn;` |
|     ! 0 | 7230 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 7231 | `				pTmp--;` |
|     ! 0 | 7232 | `			}` |
|       - | 7233 | `			/* Unexpected token */` |
|     ! 0 | 7234 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 7235 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7236 | `				return SXERR_ABORT;` |
|       - | 7237 | `			}` |
|     ! 0 | 7238 | `			goto Synchronize;` |
|       - | 7239 | `	}` |
|       - | 7240 | `	/* Set the delimiter token */` |
|      28 | 7241 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 7242 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 7243 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 7244 | `	}else{` |
|      26 | 7245 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 7246 | `	}` |
|      28 | 7247 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 7248 | `	/* Create the switch blocks container */` |
|      28 | 7249 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      28 | 7250 | `	if( pSwitch == 0 ){` |
|       - | 7251 | `		/* Abort compilation */` |
|     ! 0 | 7252 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 7253 | `		return SXERR_ABORT;` |
|       - | 7254 | `	}` |
|       - | 7255 | `	/* Zero the structure */` |
|      28 | 7256 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 7257 | `	/* Initialize fields */` |
|      28 | 7258 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 7259 | `	/* Emit the switch instruction */` |
|      28 | 7260 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 7261 | `	/* Compile case blocks */` |
|      87 | 7262 | `	for(;;){` |
|       - | 7263 | `		sxu32 nKwrd;` |
|     102 | 7264 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7265 | `			/* No more input to process */` |
|     ! 0 | 7266 | `			break;` |
|       - | 7267 | `		}` |
|     102 | 7268 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 7269 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 7270 | `				/* Unexpected token */` |
|     ! 0 | 7271 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7272 | `					&pGen->pIn->sData);` |
|     ! 0 | 7273 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7274 | `					return SXERR_ABORT;` |
|       - | 7275 | `				}` |
|       - | 7276 | `				/* FALL THROUGH */` |
|     ! 0 | 7277 | `			}` |
|       - | 7278 | `			/* Block compiled */` |
|     ! 0 | 7279 | `			break;` |
|       - | 7280 | `		}` |
|       - | 7281 | `		/* Extract the keyword */` |
|     102 | 7282 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     102 | 7283 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 7284 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 7285 | `				/* Unexpected token */` |
|     ! 0 | 7286 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7287 | `					&pGen->pIn->sData);` |
|     ! 0 | 7288 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7289 | `					return SXERR_ABORT;` |
|       - | 7290 | `				}` |
|       - | 7291 | `				/* FALL THROUGH */` |
|     ! 0 | 7292 | `			}` |
|       - | 7293 | `			/* Block compiled */` |
|       3 | 7294 | `			break;` |
|       - | 7295 | `		}` |
|     100 | 7296 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 7297 | `			/*` |
|       - | 7298 | `			 * Accroding to the PHP language reference manual` |
|       - | 7299 | `			 *  A special case is the default case. This case matches anything` |
|       - | 7300 | `			 *  that wasn't matched by the other cases.` |
|       - | 7301 | `			 */` |
|      20 | 7302 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 7303 | `				/* Default case already compiled */` |
|     ! 0 | 7304 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 7305 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7306 | `					return SXERR_ABORT;` |
|       - | 7307 | `				}` |
|     ! 0 | 7308 | `			}` |
|      20 | 7309 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 7310 | `			/* Compile the default block */` |
|      20 | 7311 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      20 | 7312 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7313 | `				return SXERR_ABORT;` |
|      20 | 7314 | `			}else if( rc == SXERR_EOF ){` |
|      18 | 7315 | `				break;` |
|       1 | 7316 | `			}` |
|      83 | 7317 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 7318 | `			ph7_case_expr sCase;` |
|       - | 7319 | `			/* Standard case block */` |
|      82 | 7320 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 7321 | `			/* initialize the structure */` |
|      82 | 7322 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 7323 | `			/* Compile the case expression */` |
|      82 | 7324 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      82 | 7325 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7326 | `				return SXERR_ABORT;` |
|       - | 7327 | `			}` |
|       - | 7328 | `			/* Compile the case block */` |
|      82 | 7329 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 7330 | `			/* Insert in the switch container */` |
|      82 | 7331 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      82 | 7332 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7333 | `				return SXERR_ABORT;` |
|      82 | 7334 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 7335 | `				break;` |
|       - | 7336 | `			}` |
|      38 | 7337 | `		}else{` |
|       - | 7338 | `			/* Unexpected token */` |
|     ! 0 | 7339 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7340 | `				&pGen->pIn->sData);` |
|     ! 0 | 7341 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7342 | `				return SXERR_ABORT;` |
|       - | 7343 | `			}` |
|     ! 0 | 7344 | `			break;` |
|       - | 7345 | `		}` |
|       2 | 7346 | `	}` |
|       - | 7347 | `	/* Fix all jumps now the destination is resolved */` |
|      28 | 7348 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 7349 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7350 | `	/* Release the loop block */` |
|      28 | 7351 | `	GenStateLeaveBlock(pGen,0);` |
|      28 | 7352 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 7353 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      28 | 7354 | `		pGen->pIn++;` |
|      13 | 7355 | `	}` |
|       - | 7356 | `	/* Statement successfully compiled */` |
|      28 | 7357 | `	return SXRET_OK;` |
|     ! 0 | 7358 | `Synchronize:` |
|       - | 7359 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 7360 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 7361 | `		pGen->pIn++;` |
|     ! 0 | 7362 | `	}` |
|     ! 0 | 7363 | `	return SXRET_OK;` |
|      15 | 7364 |  |
|       - | 7365 | `/*` |
|       - | 7366 | ` * Generate bytecode for a given expression tree.` |
|       - | 7367 | ` * If something goes wrong while generating bytecode` |
|       - | 7368 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 7369 | ` * this function takes care of generating the appropriate` |
|       - | 7370 | ` * error message.` |
|       - | 7371 | ` */` |
| 2341160 | 7372 | `static sxi32 GenStateEmitExprCode(` |
|       - | 7373 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7374 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 7375 | `	sxi32 iFlags /* Control flags */` |
|       - | 7376 | `	)` |
|       2 | 7377 |  |
|       - | 7378 | `	VmInstr *pInstr;` |
|       - | 7379 | `	sxu32 nJmpIdx;` |
| 2341162 | 7380 | `	sxi32 iP1 = 0;` |
| 2341162 | 7381 | `	sxu32 iP2 = 0;` |
| 2341162 | 7382 | `	void *p3  = 0;` |
|       - | 7383 | `	sxi32 iVmOp;` |
|       - | 7384 | `	sxi32 rc;` |
| 2341162 | 7385 | `	if( pNode->xCode ){` |
|       - | 7386 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 7387 | `		/* Compile node */` |
| 1451324 | 7388 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1451324 | 7389 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1451324 | 7390 | `		RE_SWAP_DELIMITER(pGen);` |
| 1451324 | 7391 | `		return rc;` |
|       - | 7392 | `	}` |
|  889840 | 7393 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 7394 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 7395 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 7396 | `		return SXERR_ABORT;` |
|       - | 7397 | `	}` |
|  889840 | 7398 | `	iVmOp = pNode->pOp->iVmOp;` |
|  889840 | 7399 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 7400 | `		sxu32 nJz,nJmp;` |
|       - | 7401 | `		/* Ternary operator require special handling */` |
|       - | 7402 | `		/* Phase#1: Compile the condition */` |
|    1882 | 7403 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1882 | 7404 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7405 | `			return rc;` |
|       - | 7406 | `		}` |
|    1882 | 7407 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1882 | 7408 | `		if( pNode->pLeft ){` |
|       - | 7409 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 7410 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1814 | 7411 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7412 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1814 | 7413 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1814 | 7414 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7415 | `				return rc;` |
|       - | 7416 | `			}` |
|     908 | 7417 | `		}else{` |
|       - | 7418 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 7419 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 7420 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 7421 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 7422 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7423 | `		}` |
|       - | 7424 | `		/* Phase#4: Emit the unconditional jump */` |
|    1882 | 7425 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 7426 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1882 | 7427 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1882 | 7428 | `		if( pInstr ){` |
|    1882 | 7429 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     940 | 7430 | `		}` |
|    1882 | 7431 | `		if( !pNode->pLeft ){` |
|       - | 7432 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 7433 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 7434 | `		}` |
|       - | 7435 | `		/* Phase#6: Compile the 'else' expression */` |
|    1882 | 7436 | `		if( pNode->pRight ){` |
|    1882 | 7437 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1882 | 7438 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7439 | `				return rc;` |
|       - | 7440 | `			}` |
|     940 | 7441 | `		}` |
|    1882 | 7442 | `		if( nJmp > 0 ){` |
|       - | 7443 | `			/* Phase#7: Fix the unconditional jump */` |
|    1882 | 7444 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1882 | 7445 | `			if( pInstr ){` |
|    1882 | 7446 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     940 | 7447 | `			}` |
|     940 | 7448 | `		}` |
|       - | 7449 | `		/* All done */` |
|    1882 | 7450 | `		return SXRET_OK;` |
|       - | 7451 | `	}` |
|       - | 7452 | `	/* Generate code for the left tree */` |
|  887960 | 7453 | `	if( pNode->pLeft ){` |
|  887940 | 7454 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 7455 | `			ph7_expr_node **apNode;` |
|  298234 | 7456 | `			int hasSpread = 0;` |
|       - | 7457 | `			sxi32 n;` |
|       - | 7458 | `			/* Recurse and generate bytecodes for function arguments */` |
|  298234 | 7459 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 7460 | `			/* Read-only load */` |
|  298234 | 7461 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  596010 | 7462 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  297778 | 7463 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  297778 | 7464 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7465 | `					return rc;` |
|       - | 7466 | `				}` |
|  297778 | 7467 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 7468 | `					/* Emit spread opcode to unpack this array argument */` |
|      15 | 7469 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      15 | 7470 | `					hasSpread = 1;` |
|       7 | 7471 | `				}` |
|  148890 | 7472 | `			}` |
|       - | 7473 | `			/* Total number of given arguments */` |
|  298234 | 7474 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|  298234 | 7475 | `			iP2 = hasSpread;` |
|       - | 7476 | `			/* Remove stale flags now */` |
|  298234 | 7477 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  149116 | 7478 | `		}` |
|  887940 | 7479 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  887940 | 7480 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7481 | `			return rc;` |
|       - | 7482 | `		}` |
|  887940 | 7483 | `		if( iVmOp == PH7_OP_CALL ){` |
|  298234 | 7484 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  298234 | 7485 | `			if( pInstr ){` |
|  298234 | 7486 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  297720 | 7487 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 7488 | `					sxu32 nQual;` |
|       - | 7489 | `					/* Prevent constant expansion */` |
|  297720 | 7490 | `					pInstr->iP1 = 0;` |
|       - | 7491 | `					/* Namespace-qualify the function name for CALL.` |
|       - | 7492 | `					 * Only check function imports — class imports must NOT` |
|       - | 7493 | ``					 * affect function resolution.  For `new Foo()`, the CALL`` |
|       - | 7494 | `					 * handler fires before NEW; we store the original literal` |
|       - | 7495 | `					 * index in the CALL instruction's iP2 so the NEW handler` |
|       - | 7496 | `					 * can recover the unqualified name and re-qualify with` |
|       - | 7497 | `					 * class imports. */ {` |
|  297720 | 7498 | `						int fromImport = 0;` |
|  297720 | 7499 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  297720 | 7500 | `						pInstr->iP2 = (sxi32)nQual;` |
|  297720 | 7501 | `						if( nQual != nOrig ){` |
|       - | 7502 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 7503 | `							 * NEW handler can recover the unqualified name. */` |
|      62 | 7504 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      62 | 7505 | `							if( !fromImport ){` |
|      52 | 7506 | `								p3 = (void *)1;` |
|      25 | 7507 | `							}` |
|      32 | 7508 | `						}` |
|       - | 7509 | `					}` |
|  149375 | 7510 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 7511 | `					/* Method call,flag that */` |
|     494 | 7512 | `					pInstr->iP2 = 1;` |
|     246 | 7513 | `				}` |
|  149118 | 7514 | `			}` |
|  738824 | 7515 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 7516 | `			ph7_expr_node **apNode;` |
|       - | 7517 | `			sxi32 n;` |
|       - | 7518 | `			/* Recurse and generate bytecodes for array index */` |
|   66810 | 7519 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  120550 | 7520 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   53742 | 7521 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   53742 | 7522 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7523 | `					return rc;` |
|       - | 7524 | `				}` |
|   26872 | 7525 | `			}` |
|   66810 | 7526 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   53742 | 7527 | `				iP1 = 1; /* Node have an index associated with it */` |
|   26870 | 7528 | `			}` |
|   66810 | 7529 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 7530 | `				/* Create an empty entry when the desired index is not found */` |
|   26380 | 7531 | `				iP2 = 1;` |
|   13191 | 7532 | `			}` |
|  556304 | 7533 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 7534 | `			/* POP the left node */` |
|      32 | 7535 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 7536 | `		}` |
|  443969 | 7537 | `	}` |
|  887960 | 7538 | `	rc = SXRET_OK;` |
|  887960 | 7539 | `	nJmpIdx = 0;` |
|       - | 7540 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 7541 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 7542 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  887960 | 7543 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     172 | 7544 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     172 | 7545 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     172 | 7546 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     172 | 7547 | `			int isSpecial = 0;` |
|     172 | 7548 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     120 | 7549 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     120 | 7550 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     126 | 7551 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     111 | 7552 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      56 | 7553 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      50 | 7554 | `					isSpecial = 1;` |
|      24 | 7555 | `				}` |
|      72 | 7556 | `			}` |
|     198 | 7557 | `			pInstr->iP1 = 0;` |
|     198 | 7558 | `			if( !isSpecial ){` |
|      98 | 7559 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      48 | 7560 | `			}` |
|       - | 7561 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 7562 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     146 | 7563 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|      98 | 7564 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|      98 | 7565 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 7566 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 | 7567 | `					return SXRET_OK;` |
|       - | 7568 | `				}` |
|      27 | 7569 | `			}` |
|      51 | 7570 | `		}` |
|      91 | 7571 | `	}` |
|       - | 7572 | `	/* Generate code for the right tree */` |
|  887900 | 7573 | `	if( pNode->pRight ){` |
|  463796 | 7574 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 7575 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    8226 | 7576 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  459684 | 7577 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 7578 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2748 | 7579 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  454199 | 7580 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 7581 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      32 | 7582 | `			iVmOp = 0; /* No binary operator to emit */` |
|      32 | 7583 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  452811 | 7584 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  202494 | 7585 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  101246 | 7586 | `		}` |
|  463796 | 7587 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  463796 | 7588 | `		if( iVmOp == PH7_OP_STORE ){` |
|  199726 | 7589 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  199700 | 7590 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 7591 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 7592 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 7593 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 7594 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 7595 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 7596 | `				 */` |
|      54 | 7597 | `				iVmOp = 0;` |
|  199700 | 7598 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  199674 | 7599 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 7600 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   44322 | 7601 | `					iP2 = 1;` |
|   22162 | 7602 | `				}else{` |
|  155354 | 7603 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7604 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   26342 | 7605 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   26342 | 7606 | `						iP1 = pInstr->iP1;` |
|   13172 | 7607 | `					}else{` |
|  129014 | 7608 | `						p3 = pInstr->p3;` |
|       - | 7609 | `					}` |
|       - | 7610 | `					/* POP the last dynamic load instruction */` |
|  155354 | 7611 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 7612 | `				}` |
|   99838 | 7613 | `			}` |
|  363934 | 7614 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      46 | 7615 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      46 | 7616 | `			if( pInstr ){` |
|      46 | 7617 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7618 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 7619 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 7620 | `					 */` |
|      15 | 7621 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 7622 | `					iP1 = pInstr->iP1;` |
|      15 | 7623 | `					iP2 = pInstr->iP2;` |
|      15 | 7624 | `					p3  = pInstr->p3;` |
|       8 | 7625 | `				}else{` |
|      32 | 7626 | `					p3 = pInstr->p3;` |
|       - | 7627 | `				}` |
|      22 | 7628 | `			}` |
|      22 | 7629 | `		}` |
|  231897 | 7630 | `	}` |
|  887900 | 7631 | `	if( iVmOp > 0 ){` |
|  887788 | 7632 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   10642 | 7633 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 7634 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    7814 | 7635 | `				iP1 = 1;` |
|    3908 | 7636 | `			}` |
|  882468 | 7637 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 7638 | `			/* Namespace-qualify the class name for NEW */ {` |
|   13396 | 7639 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   13396 | 7640 | `				VmInstr *pCallInstr = 0;` |
|   13396 | 7641 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   13380 | 7642 | `					pCallInstr = pPeek;` |
|   13380 | 7643 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    6689 | 7644 | `				}` |
|   13396 | 7645 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 7646 | `					sxu32 nLitForClass;` |
|       - | 7647 | `					/* If the CALL handler already qualified the name using` |
|       - | 7648 | `					 * function imports, recover the original unqualified` |
|       - | 7649 | `					 * literal so we can re-qualify with class imports. */` |
|   13394 | 7650 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      26 | 7651 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      14 | 7652 | `					}else{` |
|   13370 | 7653 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 7654 | `					}` |
|   13394 | 7655 | `					pPeek->iP1 = 0;` |
|   13394 | 7656 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    6696 | 7657 | `				}` |
|       - | 7658 | `			}` |
|   13396 | 7659 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   13396 | 7660 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 7661 | `				VmInstr *pPrev;` |
|   13380 | 7662 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   13380 | 7663 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 7664 | `					/* Pop the call instruction */` |
|   13380 | 7665 | `					iP1 = pInstr->iP1;` |
|   13380 | 7666 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    6689 | 7667 | `				}` |
|    6691 | 7668 | `			}` |
|  870451 | 7669 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 7670 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 7671 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 7672 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 7673 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 7674 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 7675 | `				int isSpecialIs = 0;` |
|      50 | 7676 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 7677 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 7678 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 7679 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 7680 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 7681 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 7682 | `						isSpecialIs = 1;` |
|       5 | 7683 | `					}` |
|      23 | 7684 | `				}` |
|      52 | 7685 | `				pInstr->iP1 = 0;` |
|      52 | 7686 | `				if( !isSpecialIs ){` |
|      38 | 7687 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      18 | 7688 | `				}` |
|      25 | 7689 | `			}` |
|  863733 | 7690 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 7691 | `			/* Prevent constant expansion for member/property names.` |
|       - | 7692 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 7693 | `			 * should not trigger constant lookup. */` |
|   99742 | 7694 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   99742 | 7695 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   99726 | 7696 | `				pInstr->iP1 = 0;` |
|   49862 | 7697 | `			}` |
|   99742 | 7698 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 7699 | `				/* Static member access,remember that */` |
|     112 | 7700 | `				iP1 = 1;` |
|     112 | 7701 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     112 | 7702 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      10 | 7703 | `					p3 = pInstr->p3;` |
|      10 | 7704 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       4 | 7705 | `				}` |
|      55 | 7706 | `			}` |
|   49870 | 7707 | `		}` |
|       - | 7708 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  887786 | 7709 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  443892 | 7710 | `	}` |
|  887898 | 7711 | `	if( nJmpIdx > 0 ){` |
|       - | 7712 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   11002 | 7713 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   11002 | 7714 | `		if( pInstr ){` |
|   11002 | 7715 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5500 | 7716 | `		}` |
|    5500 | 7717 | `	}` |
|  887898 | 7718 | `	return rc;` |
| 1170572 | 7719 |  |
|       - | 7720 | `/*` |
|       - | 7721 | ` * Compile a PHP expression.` |
|       - | 7722 | ` * According to the PHP language reference manual:` |
|       - | 7723 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 7724 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 7725 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 7726 | ` *  is "anything that has a value".` |
|       - | 7727 | ` * If something goes wrong while compiling the expression,this` |
|       - | 7728 | ` * function takes care of generating the appropriate error` |
|       - | 7729 | ` * message.` |
|       - | 7730 | ` */` |
|  632486 | 7731 | `static sxi32 PH7_CompileExpr(` |
|       - | 7732 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7733 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 7734 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 7735 | `	)` |
|       2 | 7736 |  |
|       - | 7737 | `	ph7_expr_node *pRoot;` |
|       - | 7738 | `	SySet sExprNode;` |
|       - | 7739 | `	SyToken *pEnd;` |
|       - | 7740 | `	sxi32 nExpr;` |
|       - | 7741 | `	sxi32 iNest;` |
|       - | 7742 | `	sxi32 rc;` |
|       - | 7743 | `	/* Initialize worker variables */` |
|  632488 | 7744 | `	nExpr = 0;` |
|  632488 | 7745 | `	pRoot = 0;` |
|  632488 | 7746 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  632488 | 7747 | `	SySetAlloc(&sExprNode,0x10);` |
|  632488 | 7748 | `	rc = SXRET_OK;` |
|       - | 7749 | `	/* Delimit the expression */` |
|  632488 | 7750 | `	pEnd = pGen->pIn;` |
|  632488 | 7751 | `	iNest = 0;` |
| 4263724 | 7752 | `	while( pEnd < pGen->pEnd ){` |
| 4042994 | 7753 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7754 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     230 | 7755 | `			iNest++;` |
| 4042880 | 7756 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     238 | 7757 | `			iNest--;` |
| 4042648 | 7758 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  411958 | 7759 | `			if( iNest <= 0 ){` |
|  411758 | 7760 | `				break;` |
|       - | 7761 | `			}` |
|     100 | 7762 | `		}` |
| 3631238 | 7763 | `		pEnd++;` |
|       2 | 7764 | `	}` |
|  632488 | 7765 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   10580 | 7766 | `		SyToken *pEnd2 = pGen->pIn;` |
|   10580 | 7767 | `		iNest = 0;` |
|       - | 7768 | `		/* Stop at the first comma */` |
|   21182 | 7769 | `		while( pEnd2 < pEnd ){` |
|   10604 | 7770 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       6 | 7771 | `				iNest++;` |
|   10602 | 7772 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       6 | 7773 | `				iNest--;` |
|   10598 | 7774 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 7775 | `				if( iNest <= 0 ){` |
|     ! 0 | 7776 | `					break;` |
|       - | 7777 | `				}` |
|       2 | 7778 | `			}` |
|   10604 | 7779 | `			pEnd2++;` |
|       2 | 7780 | `		}` |
|   10580 | 7781 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 7782 | `			pEnd = pEnd2;` |
|     ! 0 | 7783 | `		}` |
|    5289 | 7784 | `	}` |
|  632488 | 7785 | `	if( pEnd > pGen->pIn ){` |
|  632478 | 7786 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 7787 | `		/* Swap delimiter */` |
|  632478 | 7788 | `		pGen->pEnd = pEnd;` |
|       - | 7789 | `		/* Try to get an expression tree */` |
|  632478 | 7790 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  632478 | 7791 | `		if( rc == SXRET_OK && pRoot ){` |
|  632322 | 7792 | `			rc = SXRET_OK;` |
|  632322 | 7793 | `			if( xTreeValidator ){` |
|       - | 7794 | `				/* Call the upper layer validator callback */` |
|   13598 | 7795 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    6798 | 7796 | `			}` |
|  632322 | 7797 | `			if( rc != SXERR_ABORT ){` |
|       - | 7798 | `				/* Generate code for the given tree */` |
|  632322 | 7799 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  316160 | 7800 | `			}` |
|  632322 | 7801 | `			nExpr = 1;` |
|  316160 | 7802 | `		}` |
|       - | 7803 | `		/* Release the whole tree */` |
|  632478 | 7804 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 7805 | `		/* Synchronize token stream */` |
|  632478 | 7806 | `		pGen->pEnd = pTmp;` |
|  632478 | 7807 | `		pGen->pIn  = pEnd;` |
|  632478 | 7808 | `		if( rc == SXERR_ABORT ){` |
|       3 | 7809 | `			SySetRelease(&sExprNode);` |
|       3 | 7810 | `			return SXERR_ABORT;` |
|       - | 7811 | `		}` |
|  316237 | 7812 | `	}` |
|  632486 | 7813 | `	SySetRelease(&sExprNode);` |
|  632486 | 7814 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  316245 | 7815 |  |
|       - | 7816 | `/*` |
|       - | 7817 | ` * Return a pointer to the node construct handler associated` |
|       - | 7818 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 7819 | ` */` |
|  157594 | 7820 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 7821 |  |
|  157596 | 7822 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 7823 | `		/* Numeric literal: Either real or integer */` |
|   86122 | 7824 | `		return PH7_CompileNumLiteral;` |
|   71476 | 7825 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 7826 | `		/* Double quoted string */` |
|   15136 | 7827 | `		return PH7_CompileString;` |
|   56342 | 7828 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 7829 | `		/* Single quoted string */` |
|   56282 | 7830 | `		return PH7_CompileSimpleString;` |
|      62 | 7831 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 7832 | `		/* Heredoc */` |
|      28 | 7833 | `		return PH7_CompileHereDoc;` |
|      36 | 7834 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 7835 | `		/* Nowdoc */` |
|      29 | 7836 | `		return PH7_CompileNowDoc;` |
|       7 | 7837 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 7838 | `		/* Backtick quoted string */` |
|       5 | 7839 | `		return PH7_CompileBacktic;` |
|       - | 7840 | `	}` |
|       3 | 7841 | `	return 0;` |
|   78799 | 7842 |  |
|       - | 7843 | `/*` |
|       - | 7844 | ` * Compile an unset() statement.` |
|       - | 7845 | ` * unset($var, $arr[$key], ...);` |
|       - | 7846 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 7847 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 7848 | ` * parent array before extracting the element to unset.` |
|       - | 7849 | ` */` |
|    2562 | 7850 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 7851 |  |
|    2564 | 7852 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2564 | 7853 | `	sxu32 nIdx = 0;` |
|       - | 7854 | `	SyString sName;` |
|       - | 7855 | `	sxi32 rc;` |
|       - | 7856 | `	/* Jump the 'unset' keyword */` |
|    2564 | 7857 | `	pGen->pIn++;` |
|       - | 7858 | `	/* Save delimiter */` |
|    2564 | 7859 | `	pTmp = pGen->pEnd;` |
|       - | 7860 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2564 | 7861 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2564 | 7862 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 7863 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 7864 | `		SyToken *pClose;` |
|    2564 | 7865 | `		pGen->pIn++;   /* Skip '(' */` |
|    2564 | 7866 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2564 | 7867 | `		pEnd = pClose; /* Stop at ')' */` |
|    1281 | 7868 | `	}` |
|    2564 | 7869 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 7870 | `	/* Resolve the 'unset' builtin name once */` |
|    2564 | 7871 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     302 | 7872 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     302 | 7873 | `		if( pObj == 0 ){` |
|     ! 0 | 7874 | `			return SXERR_ABORT;` |
|       - | 7875 | `		}` |
|     302 | 7876 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     302 | 7877 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     150 | 7878 | `	}` |
|       - | 7879 | `	/* Compile each comma-separated argument */` |
|    8560 | 7880 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    5998 | 7881 | `		if( pGen->pIn < pNext ){` |
|    5998 | 7882 | `			pGen->pEnd = pNext;` |
|    5998 | 7883 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 7884 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,0);` |
|    5998 | 7885 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7886 | `				return SXERR_ABORT;` |
|       - | 7887 | `			}` |
|    5998 | 7888 | `			if( rc != SXERR_EMPTY ){` |
|       - | 7889 | `				/* Emit call for this single argument */` |
|    5996 | 7890 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5996 | 7891 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    5996 | 7892 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    2997 | 7893 | `			}` |
|    2998 | 7894 | `		}` |
|       - | 7895 | `		/* Jump trailing commas */` |
|    9432 | 7896 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3436 | 7897 | `			pNext++;` |
|       2 | 7898 | `		}` |
|    5998 | 7899 | `		pGen->pIn = pNext;` |
|       2 | 7900 | `	}` |
|       - | 7901 | `	/* Skip past the closing ')' if present */` |
|    2564 | 7902 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2564 | 7903 | `		pGen->pIn++;` |
|    1281 | 7904 | `	}` |
|       - | 7905 | `	/* Restore token stream */` |
|    2564 | 7906 | `	pGen->pEnd = pTmp;` |
|    2564 | 7907 | `	return SXRET_OK;` |
|    1283 | 7908 |  |
|       - | 7909 | `/*` |
|       - | 7910 | ` * PHP Language construct table.` |
|       - | 7911 | ` */` |
|       - | 7912 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 7913 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 7914 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 7915 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 7916 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 7917 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 7918 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 7919 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 7920 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 7921 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 7922 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 7923 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 7924 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 7925 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 7926 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 7927 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 7928 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 7929 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 7930 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 7931 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 7932 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 7933 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 7934 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 7935 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 7936 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 7937 | `};` |
|       - | 7938 | `/*` |
|       - | 7939 | ` * Return a pointer to the statement handler routine associated` |
|       - | 7940 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 7941 | ` */` |
|  383490 | 7942 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 7943 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 7944 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 7945 | `	)` |
|       2 | 7946 |  |
|  383492 | 7947 | `	sxu32 n = 0;` |
| 1611603 | 7948 | `	for(;;){` |
| 3223208 | 7949 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   44872 | 7950 | `			break;` |
|       - | 7951 | `		}` |
| 3178338 | 7952 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  338622 | 7953 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 7954 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 7955 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 7956 | `					/* 'static' (class context),return null */` |
|     ! 0 | 7957 | `					return 0;` |
|       - | 7958 | `				}` |
|     ! 0 | 7959 | `			}` |
|       - | 7960 | `			/* Return a pointer to the handler.` |
|       - | 7961 | `			*/` |
|  338622 | 7962 | `			return aLangConstruct[n].xConstruct;` |
|       - | 7963 | `		}` |
| 2839718 | 7964 | `		n++;` |
|       2 | 7965 | `	}` |
|   44872 | 7966 | `	if( pLookahed ){` |
|   44872 | 7967 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    7844 | 7968 | `			return PH7_CompileClassInterface;` |
|   37030 | 7969 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   36826 | 7970 | `			return PH7_CompileClass;` |
|     206 | 7971 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      54 | 7972 | `			return PH7_CompileTrait;` |
|     152 | 7973 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      19 | 7974 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      18 | 7975 | `				return PH7_CompileAbstractClass;` |
|     136 | 7976 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 7977 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 7978 | `				return PH7_CompileFinalClass;` |
|       - | 7979 | `		}` |
|      67 | 7980 | `	}` |
|       - | 7981 | `	/* Not a language construct */` |
|     136 | 7982 | `	return 0;` |
|  191747 | 7983 |  |
|       - | 7984 | `/*` |
|       - | 7985 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 7986 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 7987 | ` */` |
|     134 | 7988 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 7989 |  |
|       - | 7990 | `	int rc;` |
|     136 | 7991 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     136 | 7992 | `	if( rc == FALSE ){` |
|      40 | 7993 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      38 | 7994 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 7995 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 7996 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 7997 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 7998 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 7999 | `			*/` |
|       - | 8000 | `			){` |
|      34 | 8001 | `				rc = TRUE;` |
|      16 | 8002 | `		}` |
|      20 | 8003 | `	}` |
|     136 | 8004 | `	return rc;` |
|       2 | 8005 |  |
|       - | 8006 | `/*` |
|       - | 8007 | ` * Compile a PHP chunk.` |
|       - | 8008 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 8009 | ` * takes care of generating the appropriate error message.` |
|       - | 8010 | ` */` |
|  515168 | 8011 | `static sxi32 GenStateCompileChunk(` |
|       - | 8012 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 8013 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 8014 | `	)` |
|       2 | 8015 |  |
|       - | 8016 | `	ProcLangConstruct xCons;` |
|       - | 8017 | `	sxi32 rc;` |
|  515170 | 8018 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  307774 | 8019 | `	for(;;){` |
|  615550 | 8020 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 8021 | `			/* No more input to process */` |
|   11330 | 8022 | `			break;` |
|       - | 8023 | `		}` |
|  604222 | 8024 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 8025 | `			/* Compile block */` |
|      12 | 8026 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 8027 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8028 | `				break;` |
|       - | 8029 | `			}` |
|       7 | 8030 | `		}else{` |
|  604212 | 8031 | `			xCons = 0;` |
|  604212 | 8032 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  383492 | 8033 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 8034 | `				/* Try to extract a language construct handler */` |
|  383492 | 8035 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  383492 | 8036 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 8037 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 8038 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 8039 | `						&pGen->pIn->sData);` |
|       9 | 8040 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 8041 | `						break;` |
|       - | 8042 | `					}` |
|       - | 8043 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 8044 | `					 * this erroneous statement.` |
|       - | 8045 | `					 */` |
|       9 | 8046 | `					xCons = PH7_ErrorRecover;` |
|       4 | 8047 | `				}` |
|  412467 | 8048 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   38666 | 8049 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 8050 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 8051 | `				xCons = PH7_CompileLabel;` |
|      56 | 8052 | `			}` |
|  604212 | 8053 | `			if( xCons == 0 ){` |
|       - | 8054 | `				/* Assume an expression an try to compile it */` |
|  220736 | 8055 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  220736 | 8056 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 8057 | `					/* Pop l-value */` |
|  220612 | 8058 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  110305 | 8059 | `				}` |
|  110369 | 8060 | `			}else{` |
|       - | 8061 | `				/* Go compile the sucker */` |
|  383478 | 8062 | `				rc = xCons(&(*pGen));` |
|       - | 8063 | `			}` |
|  604212 | 8064 | `			if( rc == SXERR_ABORT ){` |
|       - | 8065 | `				/* Request to abort compilation */` |
|       3 | 8066 | `				break;` |
|       - | 8067 | `			}` |
|       - | 8068 | `		}` |
|       - | 8069 | `		/* Ignore trailing semi-colons ';' */` |
| 1000752 | 8070 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  396534 | 8071 | `			pGen->pIn++;` |
|       2 | 8072 | `		}` |
|  604220 | 8073 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 8074 | `			/* Compile a single statement and return */` |
|  503840 | 8075 | `			break;` |
|       - | 8076 | `		}` |
|       - | 8077 | `		/* LOOP ONE */` |
|       - | 8078 | `		/* LOOP TWO */` |
|       - | 8079 | `		/* LOOP THREE */` |
|       - | 8080 | `		/* LOOP FOUR */` |
|       2 | 8081 | `	}` |
|       - | 8082 | `	/* Return compilation status */` |
|  515170 | 8083 | `	return rc;` |
|       2 | 8084 |  |
|       - | 8085 | `/*` |
|       - | 8086 | ` * Compile a Raw PHP chunk.` |
|       - | 8087 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 8088 | ` * takes care of generating the appropriate error message.` |
|       - | 8089 | ` */` |
|   11332 | 8090 | `static sxi32 PH7_CompilePHP(` |
|       - | 8091 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 8092 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 8093 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 8094 | `	)` |
|       2 | 8095 |  |
|   11334 | 8096 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 8097 | `	sxi32 rc;` |
|       - | 8098 | `	/* Reset the token set */` |
|   11334 | 8099 | `	SySetReset(&(*pTokenSet));` |
|       - | 8100 | `	/* Mark as the default token set */` |
|   11334 | 8101 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 8102 | `	/* Advance the stream cursor */` |
|   11334 | 8103 | `	pGen->pRawIn++;` |
|       - | 8104 | `	/* Tokenize the PHP chunk first */` |
|   11334 | 8105 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 8106 | `	/* Point to the head and tail of the token stream. */` |
|   11334 | 8107 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   11334 | 8108 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   11334 | 8109 | `	if( is_expr ){` |
|     ! 0 | 8110 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 8111 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 8112 | `			/* A simple expression,compile it */` |
|     ! 0 | 8113 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 8114 | `		}` |
|       - | 8115 | `		/* Emit the DONE instruction */` |
|     ! 0 | 8116 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 8117 | `		return SXRET_OK;` |
|       - | 8118 | `	}` |
|   11334 | 8119 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 8120 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 8121 | `		/*` |
|       - | 8122 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 8123 | `		 * According to the PHP reference manual:` |
|       - | 8124 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 8125 | `		 *  immediately follow` |
|       - | 8126 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 8127 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 8128 | `		 * Symisc extension:` |
|       - | 8129 | `		 *   This short syntax works with all PHP opening` |
|       - | 8130 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 8131 | `		 *   only short tag.` |
|       - | 8132 | `		 */` |
|       - | 8133 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 8134 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 8135 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 8136 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 8137 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 8138 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 8139 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 8140 | `		}` |
|       3 | 8141 | `		return SXRET_OK;` |
|       - | 8142 | `	}` |
|       - | 8143 | `	/* Compile the PHP chunk */` |
|   11332 | 8144 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 8145 | `	/* Fix exceptions jumps */` |
|   11332 | 8146 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 8147 | `	/* Fix gotos now, the jump destination is resolved */` |
|   11332 | 8148 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 8149 | `		rc = SXERR_ABORT;` |
|       1 | 8150 | `	}` |
|       - | 8151 | `	/* Reset container */` |
|   11332 | 8152 | `	SySetReset(&pGen->aGoto);` |
|   11332 | 8153 | `	SySetReset(&pGen->aLabel);` |
|       - | 8154 | `	/* Compilation result */` |
|   11332 | 8155 | `	return rc;` |
|    5668 | 8156 |  |
|       - | 8157 | `/*` |
|       - | 8158 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 8159 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 8160 | ` * This is the only compile interface exported from this file.` |
|       - | 8161 | ` */` |
|   13356 | 8162 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 8163 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 8164 | `	SyString *pScript,  /* Script to compile */` |
|       - | 8165 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 8166 | `	)` |
|       2 | 8167 |  |
|       - | 8168 | `	SySet aPhpToken,aRawToken;` |
|       - | 8169 | `	ph7_gen_state *pCodeGen;` |
|       - | 8170 | `	ph7_value *pRawObj;` |
|       - | 8171 | `	sxu32 nObjIdx;` |
|       - | 8172 | `	sxi32 nRawObj;` |
|       - | 8173 | `	int is_expr;` |
|       - | 8174 | `	sxi32 rc;` |
|   13358 | 8175 | `	if( pScript->nByte < 1 ){` |
|       - | 8176 | `		/* Nothing to compile */` |
|     ! 0 | 8177 | `		return PH7_OK;` |
|       - | 8178 | `	}` |
|       - | 8179 | `	/* Initialize the tokens containers */` |
|   13358 | 8180 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13358 | 8181 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13358 | 8182 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   13358 | 8183 | `	is_expr = 0;` |
|   13358 | 8184 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 8185 | `		SyToken sTmp;` |
|       - | 8186 | `		/* PHP only: -*/` |
|    2632 | 8187 | `		sTmp.nLine = 1;` |
|    2632 | 8188 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2632 | 8189 | `		sTmp.pUserData = 0;` |
|    2632 | 8190 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2632 | 8191 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2632 | 8192 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 8193 | `			/* A simple PHP expression */` |
|     ! 0 | 8194 | `			is_expr = 1;` |
|     ! 0 | 8195 | `		}` |
|    1317 | 8196 | `	}else{` |
|       - | 8197 | `		/* Tokenize raw text */` |
|   10728 | 8198 | `		SySetAlloc(&aRawToken,32);` |
|   10728 | 8199 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 8200 | `	}` |
|   13358 | 8201 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 8202 | `	/* Process high-level tokens */` |
|   13358 | 8203 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   13358 | 8204 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   13358 | 8205 | `	rc = PH7_OK;` |
|   13358 | 8206 | `	if( is_expr ){` |
|       - | 8207 | `		/* Compile the expression */` |
|     ! 0 | 8208 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 8209 | `		goto cleanup;` |
|       - | 8210 | `	}` |
|   13358 | 8211 | `	nObjIdx = 0;` |
|       - | 8212 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 8213 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 8214 | `	 * preventing namespace bleeding across include()d files. */` |
|   13358 | 8215 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 8216 | `	/* Start the compilation process */` |
|   12045 | 8217 | `	for(;;){` |
|   35420 | 8218 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   13354 | 8219 | `			break; /* No more tokens to process */` |
|       - | 8220 | `		}` |
|   22068 | 8221 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 8222 | `			/* Compile the PHP chunk */` |
|   11334 | 8223 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   11334 | 8224 | `			if( rc == SXERR_ABORT ){` |
|       5 | 8225 | `				break;` |
|       - | 8226 | `			}` |
|   11330 | 8227 | `			continue;` |
|       - | 8228 | `		}` |
|       - | 8229 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   10736 | 8230 | `		nRawObj = 0;` |
|   21470 | 8231 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 8232 | `			/* Consume the raw chunk without any processing */` |
|   10736 | 8233 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   10736 | 8234 | `			if( pRawObj == 0 ){` |
|     ! 0 | 8235 | `				rc = SXERR_MEM;` |
|     ! 0 | 8236 | `				break;` |
|       - | 8237 | `			}` |
|       - | 8238 | `			/* Mark as constant and emit the load constant instruction */` |
|   10736 | 8239 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   10736 | 8240 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   10736 | 8241 | `			++nRawObj;` |
|   10736 | 8242 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 8243 | `		}` |
|   10736 | 8244 | `		if( nRawObj > 0 ){` |
|       - | 8245 | `			/* Emit the consume instruction */` |
|   10736 | 8246 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5367 | 8247 | `		}` |
|    6680 | 8248 | `	}` |
|    6678 | 8249 | `cleanup:` |
|   13358 | 8250 | `	SySetRelease(&aRawToken);` |
|   13358 | 8251 | `	SySetRelease(&aPhpToken);` |
|   13358 | 8252 | `	return rc;` |
|    6680 | 8253 |  |
|       - | 8254 | `/*` |
|       - | 8255 | ` * Utility routines.Initialize the code generator.` |
|       - | 8256 | ` */` |
|    2602 | 8257 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 8258 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8259 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8260 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8261 | `	)` |
|       2 | 8262 |  |
|    2604 | 8263 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8264 | `	/* Zero the structure */` |
|    2604 | 8265 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 8266 | `	/* Initial state */` |
|    2604 | 8267 | `	pGen->pVm  = &(*pVm);` |
|    2604 | 8268 | `	pGen->xErr = xErr;` |
|    2604 | 8269 | `	pGen->pErrData = pErrData;` |
|    2604 | 8270 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2604 | 8271 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2604 | 8272 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2604 | 8273 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 8274 | `	/* Error log buffer */` |
|    2604 | 8275 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 8276 | `	/* General purpose working buffer */` |
|    2604 | 8277 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 8278 | `	/* Namespace state */` |
|    2604 | 8279 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2604 | 8280 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    2604 | 8281 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    2604 | 8282 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 8283 | `	/* Create the global scope */` |
|    2604 | 8284 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 8285 | `	/* Point to the global scope */` |
|    2604 | 8286 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2604 | 8287 | `	return SXRET_OK;` |
|       2 | 8288 |  |
|       - | 8289 | `/*` |
|       - | 8290 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 8291 | ` */` |
|   15696 | 8292 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 8293 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8294 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8295 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8296 | `	)` |
|       2 | 8297 |  |
|   15698 | 8298 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8299 | `	GenBlock *pBlock,*pParent;` |
|       - | 8300 | `	/* Reset state */` |
|   15698 | 8301 | `	SySetReset(&pGen->aLabel);` |
|   15698 | 8302 | `	SySetReset(&pGen->aGoto);` |
|   15698 | 8303 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   15698 | 8304 | `	SyBlobRelease(&pGen->sWorker);` |
|   15698 | 8305 | `	SyBlobRelease(&pGen->sNamespace);` |
|   15698 | 8306 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   15698 | 8307 | `	SyHashRelease(&pGen->hUseImports);` |
|   15698 | 8308 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   15698 | 8309 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   15698 | 8310 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   15698 | 8311 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   15698 | 8312 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 8313 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 8314 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 8315 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 8316 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 8317 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 8318 | `	 * number of unique names, which is acceptable. */` |
|       - | 8319 | `	/* Point to the global scope */` |
|   15698 | 8320 | `	pBlock = pGen->pCurrent;` |
|   15698 | 8321 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 8322 | `		pParent = pBlock->pParent;` |
|     ! 0 | 8323 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 8324 | `		pBlock = pParent;` |
|     ! 0 | 8325 | `	}` |
|   15698 | 8326 | `	pGen->xErr = xErr;` |
|   15698 | 8327 | `	pGen->pErrData = pErrData;` |
|   15698 | 8328 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   15698 | 8329 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   15698 | 8330 | `	pGen->pIn = pGen->pEnd = 0;` |
|   15698 | 8331 | `	pGen->nErr = 0;` |
|   15698 | 8332 | `	return SXRET_OK;` |
|       2 | 8333 |  |
|       - | 8334 | `/*` |
|       - | 8335 | ` * Generate a compile-time error message.` |
|       - | 8336 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 8337 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 8338 | ` * abort compilation immediately.` |
|       - | 8339 | ` */` |
|     454 | 8340 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 8341 |  |
|     456 | 8342 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     456 | 8343 | `	const char *zErr = "Error";` |
|       - | 8344 | `	SyString *pFile;` |
|       - | 8345 | `	va_list ap;` |
|       - | 8346 | `	sxi32 rc;` |
|       - | 8347 | `	/* Reset the working buffer */` |
|     456 | 8348 | `	SyBlobReset(pWorker);` |
|       - | 8349 | `	/* Peek the processed file path if available */` |
|     456 | 8350 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     456 | 8351 | `	if( nErrType == E_ERROR ){` |
|       - | 8352 | `		/* Increment the error counter */` |
|     414 | 8353 | `		pGen->nErr++;` |
|     414 | 8354 | `		if( pGen->nErr > 15 ){` |
|       - | 8355 | `			/* Error count limit reached */` |
|       5 | 8356 | `			if( pGen->xErr ){` |
|       5 | 8357 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 8358 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 8359 | `				if( pFile ){` |
|       5 | 8360 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 8361 | `				}` |
|       5 | 8362 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 8363 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 8364 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 8365 | `				}` |
|       2 | 8366 | `			}` |
|       - | 8367 | `			/* Abort immediately */` |
|       5 | 8368 | `			return SXERR_ABORT;` |
|       - | 8369 | `		}` |
|     204 | 8370 | `	}` |
|     452 | 8371 | `	if( pGen->xErr == 0 ){` |
|       - | 8372 | `		/* No available error consumer,return immediately */` |
|       3 | 8373 | `		return SXRET_OK;` |
|       - | 8374 | `	}` |
|     449 | 8375 | `	switch(nErrType){` |
|     407 | 8376 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      29 | 8377 | `	case E_WARNING: zErr = "Warning";     break;` |
|       7 | 8378 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 8379 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 8380 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 8381 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 8382 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 8383 | `	default:` |
|     ! 0 | 8384 | `		break;` |
|       - | 8385 | `	}` |
|     449 | 8386 | `	rc = SXRET_OK;` |
|       - | 8387 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     449 | 8388 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     449 | 8389 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     449 | 8390 | `	va_start(ap,zFormat);` |
|     449 | 8391 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     449 | 8392 | `	va_end(ap);` |
|     449 | 8393 | `	if( pFile ){` |
|     449 | 8394 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     224 | 8395 | `	}` |
|       - | 8396 | `	/* Append a new line */` |
|     449 | 8397 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     449 | 8398 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 8399 | `		/* Consume the generated error message */` |
|     449 | 8400 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     224 | 8401 | `	}` |
|     449 | 8402 | `	return rc;` |
|     229 | 8403 |  |
|       - | 8404 |  |
