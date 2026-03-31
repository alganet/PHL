# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3475/4573 lines (75.99%)

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
|    2794 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    2796 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    7860 |  131 | `	for(;;){` |
|   15722 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2684 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2684 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2662 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      11 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   13062 |  140 | `		pBlock = pBlock->pParent;` |
|   13062 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1399 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  446972 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  446974 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  446974 |  162 | `	pBlock->pUserData   = pUserData;` |
|  446974 |  163 | `	pBlock->pGen        = pGen;` |
|  446974 |  164 | `	pBlock->iFlags      = iType;` |
|  446974 |  165 | `	pBlock->pParent     = 0;` |
|  446974 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  446974 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  446974 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  444422 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  444424 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  444424 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  444424 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  444424 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  444424 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  444424 |  200 | `	pGen->pCurrent = pBlock;` |
|  444424 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  213470 |  203 | `		*ppBlock = pBlock;` |
|  106734 |  204 | `	}` |
|  444424 |  205 | `	return SXRET_OK;` |
|  222213 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  444414 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  444416 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  444416 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  444416 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  444414 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  444416 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  444416 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  444416 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  444416 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  444414 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  444416 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  444416 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  444416 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  444416 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  444416 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  444416 |  244 | `	return SXRET_OK;` |
|  222209 |  245 |  |
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
|  165110 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  165112 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  165112 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  165112 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  165112 |  265 | `	return rc;` |
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
|  336958 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  336960 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  658852 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  321894 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  125404 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  196492 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   31384 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  165110 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  165110 |  298 | `		if( pInstr ){` |
|  165110 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  165110 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  165110 |  302 | `			aFix[n].nJumpType = -1;` |
|   82554 |  303 | `		}` |
|   82556 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  336960 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|   98478 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|   98480 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|   98626 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|   98478 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|   98610 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|   98478 |  358 | `	return SXRET_OK;` |
|   49241 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  426998 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  427000 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  427000 |  367 | `	if( pEntry == 0 ){` |
|  187190 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  239812 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  239812 |  371 | `	return SXRET_OK;` |
|  213501 |  372 |  |
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
|  187188 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  187190 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  187190 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   93594 |  387 | `	}` |
|  187190 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   83318 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   83320 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   83320 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   83320 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   83320 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   83320 |  408 | `	return pObj;` |
|   41661 |  409 |  |
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
|   83718 |  431 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  432 |  |
|   83720 |  433 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   83720 |  434 | `	sxu32 nIdx = 0;` |
|   83720 |  435 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  436 | `		ph7_value *pObj;` |
|       - |  437 | `		sxi64 iValue;` |
|   83320 |  438 | `		iValue = PH7_TokenValueToInt64(&pToken->sData);` |
|   83320 |  439 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   83320 |  440 | `		if( pObj == 0 ){` |
|     ! 0 |  441 | `			SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  442 | `			return SXERR_ABORT;` |
|       - |  443 | `		}` |
|   83320 |  444 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   41661 |  445 | `	}else{` |
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
|   83720 |  458 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  459 | `	/* Node successfully compiled */` |
|   83720 |  460 | `	return SXRET_OK;` |
|   41861 |  461 |  |
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
|   54590 |  473 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  474 |  |
|   54592 |  475 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  476 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  477 | `	ph7_value *pObj;` |
|       - |  478 | `	sxu32 nIdx;` |
|   54592 |  479 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  480 | `	/* Delimit the string */` |
|   54592 |  481 | `	zIn  = pStr->zString;` |
|   54592 |  482 | `	zEnd = &zIn[pStr->nByte];` |
|   54592 |  483 | `	if( zIn >= zEnd ){` |
|       - |  484 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  485 | `		 * rather than reserving a new object each time. */` |
|     112 |  486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     112 |  487 | `		return SXRET_OK;` |
|       - |  488 | `	}` |
|   54482 |  489 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  490 | `		/* Already processed,emit the load constant instruction` |
|       - |  491 | `		 * and return.` |
|       - |  492 | `		 */` |
|   16082 |  493 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   16082 |  494 | `		return SXRET_OK;` |
|       - |  495 | `	}` |
|       - |  496 | `	/* Reserve a new constant */` |
|   38402 |  497 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   38402 |  498 | `	if( pObj == 0 ){` |
|     ! 0 |  499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  500 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  501 | `		return SXERR_ABORT;` |
|       - |  502 | `	}` |
|   38402 |  503 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  504 | `	/* Compile the node */` |
|   38417 |  505 | `	for(;;){` |
|   76836 |  506 | `		if( zIn >= zEnd ){` |
|       - |  507 | `			/* End of input */` |
|   38402 |  508 | `			break;` |
|       - |  509 | `		}` |
|   38436 |  510 | `		zCur = zIn;` |
|  607592 |  511 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  569158 |  512 | `			zIn++;` |
|       2 |  513 | `		}` |
|   38436 |  514 | `		if( zIn > zCur ){` |
|       - |  515 | `			/* Append raw contents*/` |
|   38418 |  516 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   19208 |  517 | `		}` |
|   38436 |  518 | `		zIn++;` |
|   38436 |  519 | `		if( zIn < zEnd ){` |
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
|   38436 |  534 | `		zIn++;` |
|       2 |  535 | `	}` |
|       - |  536 | `	/* Emit the load constant instruction */` |
|   38402 |  537 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   38402 |  538 | `	if( pStr->nByte < 1024 ){` |
|       - |  539 | `		/* Install in the literal table */` |
|   38402 |  540 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   19200 |  541 | `	}` |
|       - |  542 | `	/* Node successfully compiled */` |
|   38402 |  543 | `	return SXRET_OK;` |
|   27297 |  544 |  |
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
|    1566 |  606 | `static sxi32 GenStateProcessStringExpression(` |
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
|    1568 |  617 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  618 | `	/* Preallocate some slots */` |
|    1568 |  619 | `	SySetAlloc(&sToken,0x08);` |
|       - |  620 | `	/* Tokenize the text */` |
|    1568 |  621 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  622 | `	/* Swap delimiter */` |
|    1568 |  623 | `	pTmpIn  = pGen->pIn;` |
|    1568 |  624 | `	pTmpEnd = pGen->pEnd;` |
|    1568 |  625 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1568 |  626 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  627 | `	/* Compile the expression */` |
|    1568 |  628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  629 | `	/* Restore token stream */` |
|    1568 |  630 | `	pGen->pIn  = pTmpIn;` |
|    1568 |  631 | `	pGen->pEnd = pTmpEnd;` |
|       - |  632 | `	/* Release the token set */` |
|    1568 |  633 | `	SySetRelease(&sToken);` |
|       - |  634 | `	/* Compilation result */` |
|    1568 |  635 | `	return rc;` |
|       2 |  636 |  |
|       - |  637 | `/*` |
|       - |  638 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  639 | ` */` |
|   14874 |  640 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  641 |  |
|       - |  642 | `	ph7_value *pConstObj;` |
|   14876 |  643 | `	sxu32 nIdx = 0;` |
|       - |  644 | `	/* Reserve a new constant */` |
|   14876 |  645 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   14876 |  646 | `	if( pConstObj == 0 ){` |
|     ! 0 |  647 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  648 | `		return 0;` |
|       - |  649 | `	}` |
|   14876 |  650 | `	(*pCount)++;` |
|   14876 |  651 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  652 | `	/* Emit the load constant instruction */` |
|   14876 |  653 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   14876 |  654 | `	return pConstObj;` |
|    7439 |  655 |  |
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
|   13756 |  694 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  695 |  |
|   13758 |  696 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  697 | `	const char *zIn,*zCur,*zEnd;` |
|   13758 |  698 | `	ph7_value *pObj = 0;` |
|       - |  699 | `	sxi32 iCons;` |
|       - |  700 | `	sxi32 rc;` |
|       - |  701 | `	/* Delimit the string */` |
|   13758 |  702 | `	zIn  = pStr->zString;` |
|   13758 |  703 | `	zEnd = &zIn[pStr->nByte];` |
|   13758 |  704 | `	if( zIn >= zEnd ){` |
|       - |  705 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  706 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  707 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  708 | `		 */` |
|     224 |  709 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     224 |  710 | `		return SXRET_OK;` |
|       - |  711 | `	}` |
|   13536 |  712 | `	zCur = 0;` |
|       - |  713 | `	/* Compile the node */` |
|   13536 |  714 | `	iCons = 0;` |
|    7550 |  715 | `	for(;;){` |
|   22754 |  716 | `		zCur = zIn;` |
|  129030 |  717 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  107844 |  718 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      44 |  719 | `				break;` |
|  107760 |  720 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1484 |  721 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     742 |  722 | `					break;` |
|       - |  723 | `			}` |
|  106278 |  724 | `			zIn++;` |
|       2 |  725 | `		}` |
|   22754 |  726 | `		if( zIn > zCur ){` |
|   10984 |  727 | `			if( pObj == 0 ){` |
|   10714 |  728 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   10714 |  729 | `				if( pObj == 0 ){` |
|     ! 0 |  730 | `					return SXERR_ABORT;` |
|       - |  731 | `				}` |
|    5356 |  732 | `			}` |
|   10984 |  733 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    5491 |  734 | `		}` |
|   22754 |  735 | `		if( zIn >= zEnd ){` |
|   13536 |  736 | `			break;` |
|       - |  737 | `		}` |
|    9220 |  738 | `		if( zIn[0] == '\\' ){` |
|    7654 |  739 | `			const char *zPtr = 0;` |
|       - |  740 | `			sxu32 n;` |
|    7654 |  741 | `			zIn++;` |
|    7654 |  742 | `			if( zIn >= zEnd ){` |
|     ! 0 |  743 | `				break;` |
|       - |  744 | `			}` |
|    7654 |  745 | `			if( pObj == 0 ){` |
|    4164 |  746 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    4164 |  747 | `				if( pObj == 0 ){` |
|     ! 0 |  748 | `					return SXERR_ABORT;` |
|       - |  749 | `				}` |
|    2081 |  750 | `			}` |
|    7654 |  751 | `			n = sizeof(char); /* size of conversion */` |
|    7654 |  752 | `			switch( zIn[0] ){` |
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
|    3464 |  773 | `			case 'n':` |
|       - |  774 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    6930 |  775 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    6930 |  776 | `				break;` |
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
|    7654 |  844 | `			zIn += n;` |
|    7654 |  845 | `			continue;` |
|       - |  846 | `		}` |
|    1568 |  847 | `		if( zIn[0] == '{' ){` |
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
|    1482 |  881 | `			const char *zExpr = zIn;` |
|       - |  882 | `			/* Assemble variable name */` |
|     740 |  883 | `			for(;;){` |
|       - |  884 | `				/* Jump leading dollars */` |
|    2962 |  885 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1482 |  886 | `					zIn++;` |
|       2 |  887 | `				}` |
|     740 |  888 | `				for(;;){` |
|    9504 |  889 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7284 |  890 | `						zIn++;` |
|       2 |  891 | `					}` |
|    1482 |  892 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  893 | `						/* UTF-8 stream */` |
|     ! 0 |  894 | `						zIn++;` |
|     ! 0 |  895 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  896 | `							zIn++;` |
|     ! 0 |  897 | `						}` |
|     ! 0 |  898 | `						continue;` |
|       - |  899 | `					}` |
|    1482 |  900 | `					break;` |
|     ! 0 |  901 | `				}` |
|    1482 |  902 | `				if( zIn >= zEnd ){` |
|      84 |  903 | `					break;` |
|       - |  904 | `				}` |
|    1400 |  905 | `				if( zIn[0] == '[' ){` |
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
|    1392 |  923 | `				}else if(zIn[0] == '{' ){` |
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
|    1388 |  941 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  942 | `					/* Member access operator '->' */` |
|     ! 0 |  943 | `					zIn += 2;` |
|    1388 |  944 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  945 | `					/* Static member access operator '::' */` |
|     ! 0 |  946 | `					zIn += 2;` |
|     ! 0 |  947 | `				}else{` |
|     695 |  948 | `					break;` |
|       - |  949 | `				}` |
|     ! 0 |  950 | `			}` |
|       - |  951 | `			/* Process the expression */` |
|    1482 |  952 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1482 |  953 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  954 | `				return SXERR_ABORT;` |
|       - |  955 | `			}` |
|    1482 |  956 | `			if( rc != SXERR_EMPTY ){` |
|    1480 |  957 | `				++iCons;` |
|     739 |  958 | `			}` |
|       - |  959 | `		}` |
|       - |  960 | `		/* Invalidate the previously used constant */` |
|    1568 |  961 | `		pObj = 0;` |
|       2 |  962 | `	}/*for(;;)*/` |
|   13536 |  963 | `	if( iCons > 1 ){` |
|       - |  964 | `		/* Concatenate all compiled constants */` |
|    1194 |  965 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     596 |  966 | `	}` |
|       - |  967 | `	/* Node successfully compiled */` |
|   13536 |  968 | `	return SXRET_OK;` |
|    6880 |  969 |  |
|       - |  970 | `/*` |
|       - |  971 | ` * Compile a double quoted string.` |
|       - |  972 | ` *  See the block-comment above for more information.` |
|       - |  973 | ` */` |
|   13730 |  974 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  975 |  |
|       - |  976 | `	sxi32 rc;` |
|   13732 |  977 | `	rc = GenStateCompileString(&(*pGen));` |
|    6865 |  978 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  979 | `	/* Compilation result */` |
|   13732 |  980 | `	return rc;` |
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
|   15194 | 1012 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   15196 | 1023 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1024 | `	/* Compile the expression*/` |
|   15196 | 1025 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1026 | `	/* Restore token stream */` |
|   15196 | 1027 | `	RE_SWAP_DELIMITER(pGen);` |
|   15196 | 1028 | `	return rc;` |
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
|   22300 | 1067 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 | 1068 |  |
|       - | 1069 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1070 | `	SyToken *pKey,*pCur;` |
|   22302 | 1071 | `	sxi32 iEmitRef = 0;` |
|   22302 | 1072 | `	sxi32 nPair = 0;` |
|       - | 1073 | `	sxi32 iNest;` |
|       - | 1074 | `	sxi32 rc;` |
|   22302 | 1075 | `	xValidator = 0;` |
|   18131 | 1076 | `	for(;;){` |
|       - | 1077 | `		/* Jump leading commas */` |
|   41008 | 1078 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    4746 | 1079 | `			pGen->pIn++;` |
|       2 | 1080 | `		}` |
|   36264 | 1081 | `		pCur = pGen->pIn;` |
|   36264 | 1082 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1083 | `			/* No more entry to process */` |
|   22290 | 1084 | `			break;` |
|       - | 1085 | `		}` |
|   13976 | 1086 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1087 | `			continue;` |
|       - | 1088 | `		}` |
|       - | 1089 | `		/* Compile the key if available */` |
|   13976 | 1090 | `		pKey = pCur;` |
|   13976 | 1091 | `		iNest = 0;` |
|   38754 | 1092 | `		while( pCur < pGen->pIn ){` |
|   25958 | 1093 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1180 | 1094 | `				break;` |
|       - | 1095 | `			}` |
|   24780 | 1096 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      78 | 1097 | `				iNest++;` |
|   24742 | 1098 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1099 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1100 | `				 * parser will shortly detect any syntax error.` |
|       - | 1101 | `				 */` |
|      78 | 1102 | `				iNest--;` |
|      38 | 1103 | `			}` |
|   24780 | 1104 | `			pCur++;` |
|       2 | 1105 | `		}` |
|   13976 | 1106 | `		rc = SXERR_EMPTY;` |
|   13976 | 1107 | `		if( pCur < pGen->pIn ){` |
|    1180 | 1108 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - | 1109 | `				/* Missing value */` |
|      11 | 1110 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 | 1111 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1112 | `					return SXERR_ABORT;` |
|       - | 1113 | `				}` |
|      11 | 1114 | `				return SXRET_OK;` |
|       - | 1115 | `			}` |
|       - | 1116 | `			/* Compile the expression holding the key */` |
|    1170 | 1117 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - | 1118 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1170 | 1119 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1120 | `				return SXERR_ABORT;` |
|       - | 1121 | `			}` |
|    1170 | 1122 | `			pCur++; /* Jump the '=>' operator */` |
|   13382 | 1123 | `		}else if( pKey == pCur ){` |
|       - | 1124 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1125 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1126 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1127 | `		}else{` |
|       - | 1128 | `			/* Reset back the cursor and point to the entry value */` |
|   12798 | 1129 | `			pCur = pKey;` |
|       - | 1130 | `		}` |
|   13966 | 1131 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1132 | `			/* No available key,load NULL */` |
|   12800 | 1133 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    6399 | 1134 | `		}` |
|   13966 | 1135 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   13964 | 1150 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   13964 | 1151 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1152 | `			return SXERR_ABORT;` |
|       - | 1153 | `		}` |
|   13964 | 1154 | `		if( iEmitRef ){` |
|       - | 1155 | `			/* Emit the load reference instruction */` |
|      32 | 1156 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1157 | `		}` |
|   13964 | 1158 | `		xValidator = 0;` |
|   13964 | 1159 | `		iEmitRef = 0;` |
|   13964 | 1160 | `		nPair++;` |
|       2 | 1161 | `	}` |
|       - | 1162 | `	/* Emit the load map instruction */` |
|   22290 | 1163 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1164 | `	/* Node successfully compiled */` |
|   22290 | 1165 | `	return SXRET_OK;` |
|   11152 | 1166 |  |
|       - | 1167 | `/*` |
|       - | 1168 | ` * Compile the 'array' language construct.` |
|       - | 1169 | ` *	 According to the PHP language reference manual` |
|       - | 1170 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1171 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1172 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1173 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1174 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1175 | ` */` |
|   22152 | 1176 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1177 |  |
|       - | 1178 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   22154 | 1179 | `	pGen->pIn += 2;` |
|   22154 | 1180 | `	pGen->pEnd--;` |
|   11076 | 1181 | `	SXUNUSED(iCompileFlag);` |
|   22154 | 1182 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1183 |  |
|       - | 1184 | `/*` |
|       - | 1185 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - | 1186 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - | 1187 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - | 1188 | ` */` |
|     148 | 1189 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1190 |  |
|       - | 1191 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     150 | 1192 | `	pGen->pIn++;` |
|     150 | 1193 | `	pGen->pEnd--;` |
|      74 | 1194 | `	SXUNUSED(iCompileFlag);` |
|     150 | 1195 | `	return GenStateCompileArrayBody(pGen);` |
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
|     132 | 1362 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
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
|     134 | 1375 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     134 | 1376 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 | 1377 | `		pGen->pIn++;` |
|     ! 0 | 1378 | `	}` |
|       - | 1379 | `	/* Reserve a constant for the lambda */` |
|     134 | 1380 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     134 | 1381 | `	if( pObj == 0 ){` |
|     ! 0 | 1382 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1383 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1384 | `		return SXERR_ABORT;` |
|       - | 1385 | `	}` |
|       - | 1386 | `	/* Generate a unique name */` |
|     134 | 1387 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - | 1388 | `	/* Make sure the generated name is unique */` |
|     134 | 1389 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 1390 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 | 1391 | `	}` |
|     134 | 1392 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     134 | 1393 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - | 1394 | `	/* Compile the lambda body */` |
|     134 | 1395 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     134 | 1396 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1397 | `		return SXERR_ABORT;` |
|       - | 1398 | `	}` |
|     134 | 1399 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1400 | `		/* Emit the load closure instruction */` |
|      12 | 1401 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       7 | 1402 | `	}else{` |
|       - | 1403 | `		/* Emit the load constant instruction */` |
|     124 | 1404 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1405 | `	}` |
|       - | 1406 | `	/* Node successfully compiled */` |
|     134 | 1407 | `	return SXRET_OK;` |
|      68 | 1408 |  |
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
|      70 | 1431 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1432 |  |
|       - | 1433 | `	SyString *pName;` |
|       - | 1434 | `	sxu32 nKeyID;` |
|       - | 1435 | `	sxi32 rc;` |
|       - | 1436 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      72 | 1437 | `	pName = &pGen->pIn->sData;` |
|      72 | 1438 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      72 | 1439 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      72 | 1440 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
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
|      64 | 1475 | `		sxi32 nArg = 0;` |
|      64 | 1476 | `		sxu32 nIdx = 0;` |
|      64 | 1477 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      64 | 1478 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1479 | `			return SXERR_ABORT;` |
|      64 | 1480 | `		}else if(rc != SXERR_EMPTY ){` |
|      64 | 1481 | `			nArg = 1;` |
|      31 | 1482 | `		}` |
|      64 | 1483 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1484 | `			ph7_value *pObj;` |
|       - | 1485 | `			/* Emit the call instruction */` |
|      18 | 1486 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      18 | 1487 | `			if( pObj == 0 ){` |
|     ! 0 | 1488 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1489 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1490 | `				return SXERR_ABORT;` |
|       - | 1491 | `			}` |
|      18 | 1492 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1493 | `			/* Install in the literal table */` |
|      18 | 1494 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       8 | 1495 | `		}` |
|       - | 1496 | `		/* Emit the call instruction */` |
|      64 | 1497 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      64 | 1498 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - | 1499 | `	}` |
|       - | 1500 | `	/* Node successfully compiled */` |
|      72 | 1501 | `	return SXRET_OK;` |
|      37 | 1502 |  |
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
|  685316 | 1524 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1525 |  |
|  685318 | 1526 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1527 | `	sxi32 iVv;` |
|       - | 1528 | `	sxi32 iP1;` |
|       - | 1529 | `	void *p3;` |
|       - | 1530 | `	sxi32 rc;` |
|  685318 | 1531 | `	iVv = -1; /* Variable variable counter */` |
| 1370646 | 1532 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  685330 | 1533 | `		pGen->pIn++;` |
|  685330 | 1534 | `		iVv++;` |
|       2 | 1535 | `	}` |
|  685318 | 1536 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1537 | `		/* Invalid variable name */` |
|     ! 0 | 1538 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 | 1539 | `		if( rc == SXERR_ABORT ){` |
|       - | 1540 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1541 | `			return SXERR_ABORT;` |
|       - | 1542 | `		}` |
|     ! 0 | 1543 | `		return SXRET_OK;` |
|       - | 1544 | `	}` |
|  685318 | 1545 | `	p3  = 0;` |
|  685318 | 1546 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
|  685302 | 1566 | `		char *zName = 0;` |
|       - | 1567 | `		/* Extract variable name */` |
|  685302 | 1568 | `		pName = &pGen->pIn->sData;` |
|       - | 1569 | `		/* Advance the stream cursor */` |
|  685302 | 1570 | `		pGen->pIn++;` |
|  685302 | 1571 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  685302 | 1572 | `		if( pEntry == 0 ){` |
|       - | 1573 | `			/* Duplicate name */` |
|  101634 | 1574 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  101634 | 1575 | `			if( zName == 0 ){` |
|     ! 0 | 1576 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1577 | `				return SXERR_ABORT;` |
|       - | 1578 | `			}` |
|       - | 1579 | `			/* Install in the hashtable */` |
|  101634 | 1580 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   50818 | 1581 | `		}else{` |
|       - | 1582 | `			/* Name already available */` |
|  583670 | 1583 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1584 | `		}` |
|  685302 | 1585 | `		p3 = (void *)zName;` |
|       - | 1586 | `	}` |
|  685314 | 1587 | `	iP1 = 0;` |
|  685314 | 1588 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  227366 | 1589 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1590 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  221680 | 1591 | `			iP1 = 1;` |
|  110839 | 1592 | `		}` |
|  113682 | 1593 | `	}` |
|       - | 1594 | `	/* Emit the load instruction */` |
|  685314 | 1595 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  685326 | 1596 | `	while( iVv > 0 ){` |
|      13 | 1597 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1598 | `		iVv--;` |
|       1 | 1599 | `	}` |
|       - | 1600 | `	/* Node successfully compiled */` |
|  685314 | 1601 | `	return SXRET_OK;` |
|  342660 | 1602 |  |
|       - | 1603 | `/*` |
|       - | 1604 | ` * Load a literal.` |
|       - | 1605 | ` */` |
|  440036 | 1606 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 1607 |  |
|  440038 | 1608 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1609 | `	ph7_value *pObj;` |
|       - | 1610 | `	SyString *pStr;` |
|       - | 1611 | `	sxu32 nIdx;` |
|       - | 1612 | `	/* Extract token value */` |
|  440038 | 1613 | `	pStr = &pToken->sData;` |
|       - | 1614 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  440038 | 1615 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   82962 | 1616 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1617 | `			/* NULL constant are always indexed at 0 */` |
|   30890 | 1618 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   30890 | 1619 | `			return SXRET_OK;` |
|   52074 | 1620 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1621 | `			/* TRUE constant are always indexed at 1 */` |
|     464 | 1622 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     464 | 1623 | `			return SXRET_OK;` |
|       2 | 1624 | `		}` |
|  422185 | 1625 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   78604 | 1626 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1627 | `			/* FALSE constant are always indexed at 2 */` |
|   33678 | 1628 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   33678 | 1629 | `			return SXRET_OK;` |
|  358205 | 1630 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   69606 | 1631 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1632 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5104 | 1633 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5104 | 1634 | `			if( pObj == 0 ){` |
|     ! 0 | 1635 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1636 | `				return SXERR_ABORT;` |
|       - | 1637 | `			}` |
|    5104 | 1638 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1639 | `			/* Emit the load constant instruction */` |
|    5104 | 1640 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5104 | 1641 | `			return SXRET_OK;` |
|  327336 | 1642 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   18072 | 1643 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
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
|  326495 | 1659 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    8178 | 1660 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  322400 | 1661 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    8230 | 1662 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|  369894 | 1692 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 1693 | `		ph7_value *pLitObj;` |
|       - | 1694 | `		/* Unknown literal,install it in the literal table */` |
|  148400 | 1695 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  148400 | 1696 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1697 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1698 | `			return SXERR_ABORT;` |
|       - | 1699 | `		}` |
|  148400 | 1700 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  148400 | 1701 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   74199 | 1702 | `	}` |
|       - | 1703 | `	/* Emit the load constant instruction */` |
|  369894 | 1704 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  369894 | 1705 | `	return SXRET_OK;` |
|  220020 | 1706 |  |
|       - | 1707 | `/*` |
|       - | 1708 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1709 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1710 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1711 | ` * Otherwise, load the simple literal directly.` |
|       - | 1712 | ` */` |
|  440056 | 1713 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 1714 |  |
|       - | 1715 | `	sxi32 rc;` |
|  440058 | 1716 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1717 | `		return SXRET_OK;` |
|       - | 1718 | `	}` |
|       - | 1719 | `	/* Check if this is a multi-token namespace path */` |
|  440058 | 1720 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 1721 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      21 | 1722 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      21 | 1723 | `		int isAbsolute = 0;` |
|      21 | 1724 | `		SyBlobReset(pWorker);` |
|       - | 1725 | `		/* Check for leading backslash (absolute path) */` |
|      21 | 1726 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      19 | 1727 | `			isAbsolute = 1;` |
|      19 | 1728 | `			pGen->pIn++; /* Skip leading backslash */` |
|       9 | 1729 | `		}` |
|       - | 1730 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      21 | 1731 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 1732 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 1733 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 | 1734 | `		}` |
|       - | 1735 | `		/* Collect all path components */` |
|      81 | 1736 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      81 | 1737 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      31 | 1738 | `				SyBlobAppend(pWorker,"\\",1);` |
|      16 | 1739 | `			}else{` |
|      51 | 1740 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 1741 | `			}` |
|      81 | 1742 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      21 | 1743 | `				pGen->pIn++;` |
|      21 | 1744 | `				break;` |
|       - | 1745 | `			}` |
|      61 | 1746 | `			pGen->pIn++;` |
|       1 | 1747 | `		}` |
|      21 | 1748 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 1749 | `			ph7_value *pObj;` |
|       - | 1750 | `			SyString sPath;` |
|       - | 1751 | `			sxu32 nIdx;` |
|      21 | 1752 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 1753 | `			/* Install in the literal table */` |
|      21 | 1754 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      11 | 1755 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      11 | 1756 | `				if( pObj == 0 ){` |
|     ! 0 | 1757 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1758 | `					return SXERR_ABORT;` |
|       - | 1759 | `				}` |
|      11 | 1760 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      11 | 1761 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       5 | 1762 | `			}` |
|       - | 1763 | `			/* Emit the load constant instruction.` |
|       - | 1764 | `			 * P1=1 means candidate for constant/function/class expansion. */` |
|      21 | 1765 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      21 | 1766 | `			return SXRET_OK;` |
|       - | 1767 | `		}` |
|     ! 0 | 1768 | `	}` |
|       - | 1769 | `	/* Single-token literal: load directly */` |
|  440038 | 1770 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  440038 | 1771 | `	return rc;` |
|  220030 | 1772 |  |
|       - | 1773 | `/*` |
|       - | 1774 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1775 | ` */` |
|  440056 | 1776 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1777 |  |
|       - | 1778 | `	sxi32 rc;` |
|  440058 | 1779 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  440058 | 1780 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1781 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1782 | `		return rc;` |
|       - | 1783 | `	}` |
|       - | 1784 | `	/* Node successfully compiled */` |
|  440058 | 1785 | `	return SXRET_OK;` |
|  220030 | 1786 |  |
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
|    2656 | 1938 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 | 1939 |  |
|    2658 | 1940 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   15568 | 1941 | `	while( pBlock && pBlock != pTarget ){` |
|   12912 | 1942 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
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
|   12912 | 1954 | `		pBlock = pBlock->pParent;` |
|       2 | 1955 | `	}` |
|    2658 | 1956 |  |
|    2590 | 1957 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 1958 |  |
|       - | 1959 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1960 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1961 | `	sxu32 nLineLocal;` |
|       - | 1962 | `	sxi32 rc;` |
|    2592 | 1963 | `	nLineLocal = pGen->pIn->nLine;` |
|    2592 | 1964 | `	iLevel = 0;` |
|       - | 1965 | `	/* Jump the 'continue' keyword */` |
|    2592 | 1966 | `	pGen->pIn++;` |
|    2592 | 1967 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|    2592 | 1978 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2592 | 1979 | `	if( pLoop == 0 ){` |
|       - | 1980 | `		/* Illegal continue */` |
|      11 | 1981 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 1982 | `		if( rc == SXERR_ABORT ){` |
|       - | 1983 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1984 | `			return SXERR_ABORT;` |
|       - | 1985 | `		}` |
|       6 | 1986 | `	}else{` |
|    2582 | 1987 | `		sxu32 nInstrIdx = 0;` |
|       - | 1988 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2582 | 1989 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2582 | 1990 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
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
|    2578 | 2002 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2578 | 2003 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 2004 | `				JumpFixup sJumpFix;` |
|       - | 2005 | `				/* Post-continue */` |
|      10 | 2006 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      10 | 2007 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      10 | 2008 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       4 | 2009 | `			}` |
|       - | 2010 | `		}` |
|       - | 2011 | `	}` |
|    2592 | 2012 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2013 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2014 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 2015 | `	}` |
|       - | 2016 | `	/* Statement successfully compiled */` |
|    2592 | 2017 | `	return SXRET_OK;` |
|    1297 | 2018 |  |
|       - | 2019 | `/*` |
|       - | 2020 | ` * Compile the 'break' statement.` |
|       - | 2021 | ` * According to the PHP language reference` |
|       - | 2022 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - | 2023 | ` *  structure.` |
|       - | 2024 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - | 2025 | ` *  enclosing structures are to be broken out of.` |
|       - | 2026 | ` */` |
|      92 | 2027 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 | 2028 |  |
|       - | 2029 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2030 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2031 | `	sxi32 rc;` |
|      94 | 2032 | `	iLevel = 0;` |
|       - | 2033 | `	/* Jump the 'break' keyword */` |
|      94 | 2034 | `	pGen->pIn++;` |
|      94 | 2035 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|      94 | 2046 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|      94 | 2047 | `	if( pLoop == 0 ){` |
|       - | 2048 | `		/* Illegal break */` |
|      17 | 2049 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 | 2050 | `		if( rc == SXERR_ABORT ){` |
|       - | 2051 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2052 | `			return SXERR_ABORT;` |
|       - | 2053 | `		}` |
|       9 | 2054 | `	}else{` |
|       - | 2055 | `		sxu32 nInstrIdx;` |
|       - | 2056 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|      78 | 2057 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|      78 | 2058 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      78 | 2059 | `		if( rc == SXRET_OK ){` |
|       - | 2060 | `			/* Fix the jump later when the jump destination is resolved */` |
|      78 | 2061 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      38 | 2062 | `		}` |
|       - | 2063 | `	}` |
|      94 | 2064 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2065 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2066 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 | 2067 | `	}` |
|       - | 2068 | `	/* Statement successfully compiled */` |
|      94 | 2069 | `	return SXRET_OK;` |
|      48 | 2070 |  |
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
|  232264 | 2280 | `static sxi32 PH7_CompileBlock(` |
|       - | 2281 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2282 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2283 | `	)` |
|       2 | 2284 |  |
|       - | 2285 | `	sxi32 rc;` |
|       - | 2286 | `	sxu32 nLine;` |
|  232266 | 2287 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  230956 | 2288 | `		nLine = pGen->pIn->nLine;` |
|  230956 | 2289 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  230956 | 2290 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2291 | `			return SXERR_ABORT;` |
|       - | 2292 | `		}` |
|  230956 | 2293 | `		pGen->pIn++;` |
|       - | 2294 | `		/* Compile until we hit the closing braces '}' */` |
|  337314 | 2295 | `		for(;;){` |
|  674630 | 2296 | `			if( pGen->pIn >= pGen->pEnd ){` |
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
|  674610 | 2307 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2308 | `				/* Closing braces found,break immediately*/` |
|  230936 | 2309 | `				pGen->pIn++;` |
|  230936 | 2310 | `				break;` |
|       - | 2311 | `			}` |
|       - | 2312 | `			/* Compile a single statement */` |
|  443676 | 2313 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  443676 | 2314 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2315 | `				return SXERR_ABORT;` |
|       - | 2316 | `			}` |
|       2 | 2317 | `		}` |
|  230956 | 2318 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  116789 | 2319 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|    1312 | 2363 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1312 | 2364 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2365 | `			return SXERR_ABORT;` |
|       - | 2366 | `		}` |
|       - | 2367 | `	}` |
|       - | 2368 | `	/* Jump trailing semi-colons ';' */` |
|  232266 | 2369 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2370 | `		pGen->pIn++;` |
|     ! 0 | 2371 | `	}` |
|  232266 | 2372 | `	return SXRET_OK;` |
|  116134 | 2373 |  |
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
|   10276 | 2393 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 2394 |  |
|   10278 | 2395 | `	GenBlock *pWhileBlock = 0;` |
|   10278 | 2396 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2397 | `	sxu32 nFalseJump;` |
|       - | 2398 | `	sxu32 nLine;` |
|       - | 2399 | `	sxi32 rc;` |
|   10278 | 2400 | `	nLine = pGen->pIn->nLine;` |
|       - | 2401 | `	/* Jump the 'while' keyword */` |
|   10278 | 2402 | `	pGen->pIn++;` |
|   10278 | 2403 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2404 | `		/* Syntax error */` |
|     ! 0 | 2405 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2406 | `		if( rc == SXERR_ABORT ){` |
|       - | 2407 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2408 | `			return SXERR_ABORT;` |
|       - | 2409 | `		}` |
|     ! 0 | 2410 | `		goto Synchronize;` |
|       - | 2411 | `	}` |
|       - | 2412 | `	/* Jump the left parenthesis '(' */` |
|   10278 | 2413 | `	pGen->pIn++;` |
|       - | 2414 | `	/* Create the loop block */` |
|   10278 | 2415 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   10278 | 2416 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2417 | `		return SXERR_ABORT;` |
|       - | 2418 | `	}` |
|       - | 2419 | `	/* Delimit the condition */` |
|   10278 | 2420 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10278 | 2421 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2422 | `		/* Empty expression */` |
|       3 | 2423 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2424 | `		if( rc == SXERR_ABORT ){` |
|       - | 2425 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2426 | `			return SXERR_ABORT;` |
|       - | 2427 | `		}` |
|       1 | 2428 | `	}` |
|       - | 2429 | `	/* Swap token streams */` |
|   10278 | 2430 | `	pTmp = pGen->pEnd;` |
|   10278 | 2431 | `	pGen->pEnd = pEnd;` |
|       - | 2432 | `	/* Compile the expression */` |
|   10278 | 2433 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10278 | 2434 | `	if( rc == SXERR_ABORT ){` |
|       - | 2435 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2436 | `		return SXERR_ABORT;` |
|       - | 2437 | `	}` |
|       - | 2438 | `	/* Update token stream */` |
|   10278 | 2439 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2440 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2441 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2442 | `			return SXERR_ABORT;` |
|       - | 2443 | `		}` |
|     ! 0 | 2444 | `		pGen->pIn++;` |
|     ! 0 | 2445 | `	}` |
|       - | 2446 | `	/* Synchronize pointers */` |
|   10278 | 2447 | `	pGen->pIn  = &pEnd[1];` |
|   10278 | 2448 | `	pGen->pEnd = pTmp;` |
|       - | 2449 | `	/* Emit the false jump */` |
|   10278 | 2450 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2451 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10278 | 2452 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2453 | `	/* Compile the loop body */` |
|   10278 | 2454 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   10278 | 2455 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2456 | `		return SXERR_ABORT;` |
|       - | 2457 | `	}` |
|       - | 2458 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10278 | 2459 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2460 | `	/* Fix all jumps now the destination is resolved */` |
|   10278 | 2461 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2462 | `	/* Release the loop block */` |
|   10278 | 2463 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2464 | `	/* Statement successfully compiled */` |
|   10278 | 2465 | `	return SXRET_OK;` |
|     ! 0 | 2466 | `Synchronize:` |
|       - | 2467 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2468 | `	 * compiling this erroneous block.` |
|       - | 2469 | `	 */` |
|     ! 0 | 2470 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2471 | `		pGen->pIn++;` |
|     ! 0 | 2472 | `	}` |
|     ! 0 | 2473 | `	return SXRET_OK;` |
|    5140 | 2474 |  |
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
|   10280 | 2622 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 2623 |  |
|   10282 | 2624 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   10282 | 2625 | `	GenBlock *pForBlock = 0;` |
|       - | 2626 | `	sxu32 nFalseJump;` |
|       - | 2627 | `	sxu32 nLine;` |
|       - | 2628 | `	sxi32 rc;` |
|   10282 | 2629 | `	nLine = pGen->pIn->nLine;` |
|       - | 2630 | `	/* Jump the 'for' keyword */` |
|   10282 | 2631 | `	pGen->pIn++;` |
|   10282 | 2632 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2633 | `		/* Syntax error */` |
|     ! 0 | 2634 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2635 | `		if( rc == SXERR_ABORT ){` |
|       - | 2636 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2637 | `			return SXERR_ABORT;` |
|       - | 2638 | `		}` |
|     ! 0 | 2639 | `		return SXRET_OK;` |
|       - | 2640 | `	}` |
|       - | 2641 | `	/* Jump the left parenthesis '(' */` |
|   10282 | 2642 | `	pGen->pIn++;` |
|       - | 2643 | `	/* Delimit the init-expr;condition;post-expr */` |
|   10282 | 2644 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10282 | 2645 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|   10282 | 2660 | `	pTmp = pGen->pEnd;` |
|   10282 | 2661 | `	pGen->pEnd = pEnd;` |
|       - | 2662 | `	/* Compile initialization expressions if available */` |
|   10282 | 2663 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2664 | `	/* Pop operand lvalues */` |
|   10282 | 2665 | `	if( rc == SXERR_ABORT ){` |
|       - | 2666 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2667 | `		return SXERR_ABORT;` |
|   10282 | 2668 | `	}else if( rc != SXERR_EMPTY ){` |
|   10280 | 2669 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5139 | 2670 | `	}` |
|   10282 | 2671 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   10282 | 2682 | `	pGen->pIn++;` |
|       - | 2683 | `	/* Create the loop block */` |
|   10282 | 2684 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   10282 | 2685 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2686 | `		return SXERR_ABORT;` |
|       - | 2687 | `	}` |
|       - | 2688 | `	/* Deffer continue jumps */` |
|   10282 | 2689 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 2690 | `	/* Compile the condition */` |
|   10282 | 2691 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10282 | 2692 | `	if( rc == SXERR_ABORT ){` |
|       - | 2693 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2694 | `		return SXERR_ABORT;` |
|   10282 | 2695 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 2696 | `		/* Emit the false jump */` |
|   10280 | 2697 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2698 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10280 | 2699 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5139 | 2700 | `	}` |
|   10282 | 2701 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   10278 | 2712 | `	pGen->pIn++;` |
|       - | 2713 | `	/* Save the post condition stream */` |
|   10278 | 2714 | `	pPostStart = pGen->pIn;` |
|       - | 2715 | `	/* Compile the loop body */` |
|   10278 | 2716 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   10278 | 2717 | `	pGen->pEnd = pTmp;` |
|   10278 | 2718 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   10278 | 2719 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2720 | `		return SXERR_ABORT;` |
|       - | 2721 | `	}` |
|       - | 2722 | `	/* Fix post-continue jumps */` |
|   10278 | 2723 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 2724 | `		JumpFixup *aPost;` |
|       - | 2725 | `		VmInstr *pInstr;` |
|       - | 2726 | `		sxu32 nJumpDest;` |
|       - | 2727 | `		sxu32 n;` |
|      10 | 2728 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      10 | 2729 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      18 | 2730 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      10 | 2731 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      10 | 2732 | `			if( pInstr ){` |
|       - | 2733 | `				/* Fix jump */` |
|      10 | 2734 | `				pInstr->iP2 = nJumpDest;` |
|       4 | 2735 | `			}` |
|       6 | 2736 | `		}` |
|       4 | 2737 | `	}` |
|       - | 2738 | `	/* compile the post-expressions if available */` |
|   10278 | 2739 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2740 | `		pPostStart++;` |
|     ! 0 | 2741 | `	}` |
|   10278 | 2742 | `	if( pPostStart < pEnd ){` |
|       - | 2743 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   10278 | 2744 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   10278 | 2745 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10278 | 2746 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 2747 | `			/* Syntax error */` |
|     ! 0 | 2748 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 2749 | `			if( rc == SXERR_ABORT ){` |
|       - | 2750 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2751 | `				return SXERR_ABORT;` |
|       - | 2752 | `			}` |
|     ! 0 | 2753 | `			return SXRET_OK;` |
|       - | 2754 | `		}` |
|   10278 | 2755 | `		RE_SWAP_DELIMITER(pGen);` |
|   10278 | 2756 | `		if( rc == SXERR_ABORT ){` |
|       - | 2757 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2758 | `			return SXERR_ABORT;` |
|   10278 | 2759 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 2760 | `			/* Pop operand lvalue */` |
|   10278 | 2761 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5138 | 2762 | `		}` |
|    5138 | 2763 | `	}` |
|       - | 2764 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10278 | 2765 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 2766 | `	/* Fix all jumps now the destination is resolved */` |
|   10278 | 2767 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2768 | `	/* Release the loop block */` |
|   10278 | 2769 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2770 | `	/* Statement successfully compiled */` |
|   10278 | 2771 | `	return SXRET_OK;` |
|    5142 | 2772 |  |
|       - | 2773 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 2774 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 2775 | ` * are allowed.` |
|       - | 2776 | ` */` |
|    5460 | 2777 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 2778 |  |
|    5462 | 2779 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    5462 | 2780 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 2781 | `		/* Unexpected expression */` |
|     ! 0 | 2782 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 2783 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 2784 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 2785 | `			rc = SXERR_INVALID;` |
|     ! 0 | 2786 | `		}` |
|     ! 0 | 2787 | `	}` |
|    5462 | 2788 | `	return rc;` |
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
|    2764 | 2816 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 2817 |  |
|    2766 | 2818 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    2766 | 2819 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    2766 | 2820 | `	GenBlock *pForeachBlock = 0;` |
|       - | 2821 | `	ph7_foreach_info *pInfo;` |
|       - | 2822 | `	sxu32 nFalseJump;` |
|       - | 2823 | `	VmInstr *pInstr;` |
|       - | 2824 | `	sxu32 nLine;` |
|       - | 2825 | `	sxi32 rc;` |
|    2766 | 2826 | `	nLine = pGen->pIn->nLine;` |
|       - | 2827 | `	/* Jump the 'foreach' keyword */` |
|    2766 | 2828 | `	pGen->pIn++;` |
|    2766 | 2829 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2830 | `		/* Syntax error */` |
|     ! 0 | 2831 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 2832 | `		if( rc == SXERR_ABORT ){` |
|       - | 2833 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2834 | `			return SXERR_ABORT;` |
|       - | 2835 | `		}` |
|     ! 0 | 2836 | `		goto Synchronize;` |
|       - | 2837 | `	}` |
|       - | 2838 | `	/* Jump the left parenthesis '(' */` |
|    2766 | 2839 | `	pGen->pIn++;` |
|       - | 2840 | `	/* Create the loop block */` |
|    2766 | 2841 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    2766 | 2842 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2843 | `		return SXERR_ABORT;` |
|       - | 2844 | `	}` |
|       - | 2845 | `	/* Delimit the expression */` |
|    2766 | 2846 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    2766 | 2847 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|    2766 | 2862 | `	pCur = pGen->pIn;` |
|   18566 | 2863 | `	while( pCur < pEnd ){` |
|   18566 | 2864 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    2776 | 2865 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    2776 | 2866 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 2867 | `				/* Break with the first 'as' found */` |
|    2766 | 2868 | `				break;` |
|       - | 2869 | `			}` |
|       5 | 2870 | `		}` |
|       - | 2871 | `		/* Advance the stream cursor */` |
|   15802 | 2872 | `		pCur++;` |
|       2 | 2873 | `	}` |
|    2766 | 2874 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 2875 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2876 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 2877 | `		if( rc == SXERR_ABORT ){` |
|       - | 2878 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2879 | `			return SXERR_ABORT;` |
|       - | 2880 | `		}` |
|     ! 0 | 2881 | `		goto Synchronize;` |
|       - | 2882 | `	}` |
|       - | 2883 | `	/* Swap token streams */` |
|    2766 | 2884 | `	pTmp = pGen->pEnd;` |
|    2766 | 2885 | `	pGen->pEnd = pCur;` |
|    2766 | 2886 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    2766 | 2887 | `	if( rc == SXERR_ABORT ){` |
|       - | 2888 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2889 | `		return SXERR_ABORT;` |
|       - | 2890 | `	}` |
|       - | 2891 | `	/* Update token stream */` |
|    2766 | 2892 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 2893 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2894 | `		if( rc == SXERR_ABORT ){` |
|       - | 2895 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2896 | `			return SXERR_ABORT;` |
|       - | 2897 | `		}` |
|     ! 0 | 2898 | `		pGen->pIn++;` |
|     ! 0 | 2899 | `	}` |
|    2766 | 2900 | `	pCur++; /* Jump the 'as' keyword */` |
|    2766 | 2901 | `	pGen->pIn = pCur;` |
|    2766 | 2902 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2903 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 2904 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2905 | `			return SXERR_ABORT;` |
|       - | 2906 | `		}` |
|     ! 0 | 2907 | `	}` |
|       - | 2908 | `	/* Create the foreach context */` |
|    2766 | 2909 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    2766 | 2910 | `	if( pInfo == 0 ){` |
|     ! 0 | 2911 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 2912 | `		return SXERR_ABORT;` |
|       - | 2913 | `	}` |
|       - | 2914 | `	/* Zero the structure */` |
|    2766 | 2915 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 2916 | `	/* Initialize structure fields */` |
|    2766 | 2917 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 2918 | `	/* Check if we have a key field */` |
|    8334 | 2919 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    5570 | 2920 | `		pCur++;` |
|       2 | 2921 | `	}` |
|    2766 | 2922 | `	if( pCur < pEnd ){` |
|       - | 2923 | `		/* Compile the expression holding the key name */` |
|    2706 | 2924 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 2925 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 2926 | `			if( rc == SXERR_ABORT ){` |
|       - | 2927 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2928 | `				return SXERR_ABORT;` |
|       - | 2929 | `			}` |
|     ! 0 | 2930 | `		}else{` |
|    2706 | 2931 | `			pGen->pEnd = pCur;` |
|    2706 | 2932 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2706 | 2933 | `			if( rc == SXERR_ABORT ){` |
|       - | 2934 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2935 | `				return SXERR_ABORT;` |
|       - | 2936 | `			}` |
|    2706 | 2937 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2706 | 2938 | `			if( pInstr->p3 ){` |
|       - | 2939 | `				/* Record key name */` |
|    2706 | 2940 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1352 | 2941 | `			}` |
|    2706 | 2942 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 2943 | `		}` |
|    2706 | 2944 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1352 | 2945 | `	}` |
|    2766 | 2946 | `	pGen->pEnd = pEnd;` |
|    2766 | 2947 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2948 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 2949 | `		if( rc == SXERR_ABORT ){` |
|       - | 2950 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2951 | `			return SXERR_ABORT;` |
|       - | 2952 | `		}` |
|     ! 0 | 2953 | `		goto Synchronize;` |
|       - | 2954 | `	}` |
|    2766 | 2955 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      12 | 2956 | `		pGen->pIn++;` |
|       - | 2957 | `		/* Pass by reference  */` |
|      12 | 2958 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 | 2959 | `	}` |
|       - | 2960 | `	/* Check if the value target is list() */` |
|    2766 | 2961 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 2962 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 2963 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - | 2964 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - | 2965 | `		 */` |
|       - | 2966 | `		static int iForeachListCnt = 0;` |
|       - | 2967 | `		char zTmp[128];` |
|       - | 2968 | `		sxu32 nLen;` |
|       - | 2969 | `		char *zDup;` |
|       9 | 2970 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       9 | 2971 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       9 | 2972 | `		if( zDup == 0 ){` |
|     ! 0 | 2973 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2974 | `			return SXERR_ABORT;` |
|       - | 2975 | `		}` |
|       9 | 2976 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 2977 | `		/* Save list() token boundaries */` |
|       9 | 2978 | `		pListStart = pGen->pIn;` |
|       - | 2979 | `		/* Advance past list(...) — validate parentheses */` |
|       9 | 2980 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       9 | 2981 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
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
|    2758 | 3004 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2758 | 3005 | `		if( rc == SXERR_ABORT ){` |
|       - | 3006 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3007 | `			return SXERR_ABORT;` |
|       - | 3008 | `		}` |
|    2758 | 3009 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2758 | 3010 | `		if( pInstr->p3 ){` |
|       - | 3011 | `			/* Record value name */` |
|    2758 | 3012 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1378 | 3013 | `		}` |
|       - | 3014 | `	}` |
|       - | 3015 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    2764 | 3016 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 3017 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2764 | 3018 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 3019 | `	/* Record the first instruction to execute */` |
|    2764 | 3020 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3021 | `	/* Emit the FOREACH_STEP instruction */` |
|    2764 | 3022 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 3023 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2764 | 3024 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 3025 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    2764 | 3026 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
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
|    2764 | 3050 | `	pGen->pIn = &pEnd[1];` |
|    2764 | 3051 | `	pGen->pEnd = pTmp;` |
|    2764 | 3052 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    2764 | 3053 | `	if( rc == SXERR_ABORT ){` |
|       - | 3054 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3055 | `		return SXERR_ABORT;` |
|       - | 3056 | `	}` |
|       - | 3057 | `	/* Emit the unconditional jump to the start of the loop */` |
|    2764 | 3058 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 3059 | `	/* Fix all jumps now the destination is resolved */` |
|    2764 | 3060 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3061 | `	/* Release the loop block */` |
|    2764 | 3062 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3063 | `	/* Statement successfully compiled */` |
|    2764 | 3064 | `	return SXRET_OK;` |
|       1 | 3065 | `Synchronize:` |
|       - | 3066 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 3067 | `	 * compiling this erroneous block.` |
|       - | 3068 | `	 */` |
|       3 | 3069 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3070 | `		pGen->pIn++;` |
|     ! 0 | 3071 | `	}` |
|       3 | 3072 | `	return SXRET_OK;` |
|    1384 | 3073 |  |
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
|  102392 | 3106 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 3107 |  |
|  102394 | 3108 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  102394 | 3109 | `	GenBlock *pCondBlock = 0;` |
|       - | 3110 | `	sxu32 nJumpIdx;` |
|       - | 3111 | `	sxu32 nKeyID;` |
|       - | 3112 | `	sxi32 rc;` |
|       - | 3113 | `	/* Jump the 'if' keyword */` |
|  102394 | 3114 | `	pGen->pIn++;` |
|  102394 | 3115 | `	pToken = pGen->pIn;` |
|       - | 3116 | `	/* Create the conditional block */` |
|  102394 | 3117 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  102394 | 3118 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3119 | `		return SXERR_ABORT;` |
|       - | 3120 | `	}` |
|       - | 3121 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   56308 | 3122 | `	for(;;){` |
|  112618 | 3123 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
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
|  112618 | 3136 | `		pToken++;` |
|       - | 3137 | `		/* Delimit the condition */` |
|  112618 | 3138 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  112618 | 3139 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
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
|  112618 | 3152 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 3153 | `		/* Compile the condition */` |
|  112618 | 3154 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3155 | `		/* Update token stream */` |
|  112618 | 3156 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 3157 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3158 | `			pGen->pIn++;` |
|     ! 0 | 3159 | `		}` |
|  112618 | 3160 | `		pGen->pIn  = &pEnd[1];` |
|  112618 | 3161 | `		pGen->pEnd = pTmp;` |
|  112618 | 3162 | `		if( rc == SXERR_ABORT ){` |
|       - | 3163 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3164 | `			return SXERR_ABORT;` |
|       - | 3165 | `		}` |
|       - | 3166 | `		/* Emit the false jump */` |
|  112618 | 3167 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 3168 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  112618 | 3169 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 3170 | `		/* Compile the body */` |
|  112618 | 3171 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  112618 | 3172 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3173 | `			return SXERR_ABORT;` |
|       - | 3174 | `		}` |
|  112618 | 3175 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   30307 | 3176 | `			break;` |
|       - | 3177 | `		}` |
|       - | 3178 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   52008 | 3179 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   52008 | 3180 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   33414 | 3181 | `			break;` |
|       - | 3182 | `		}` |
|       - | 3183 | `		/* Emit the unconditional jump */` |
|   18596 | 3184 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 3185 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   18596 | 3186 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   18596 | 3187 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   13472 | 3188 | `			pToken = &pGen->pIn[1];` |
|   13472 | 3189 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5130 | 3190 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4187 | 3191 | `					break;` |
|       - | 3192 | `			}` |
|    5102 | 3193 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2550 | 3194 | `		}` |
|   10226 | 3195 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 3196 | `		/* Synchronize cursors */` |
|   10226 | 3197 | `		pToken = pGen->pIn;` |
|       - | 3198 | `		/* Fix the false jump */` |
|   10226 | 3199 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 3200 | `	} /* For(;;) */` |
|       - | 3201 | `	/* Fix the false jump */` |
|  102394 | 3202 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  102394 | 3203 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   41782 | 3204 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 3205 | `			/* Compile the else block */` |
|    8372 | 3206 | `			pGen->pIn++;` |
|    8372 | 3207 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    8372 | 3208 | `			if( rc == SXERR_ABORT ){` |
|       - | 3209 |  |
|     ! 0 | 3210 | `				return SXERR_ABORT;` |
|       - | 3211 | `			}` |
|    4185 | 3212 | `	}` |
|  102394 | 3213 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3214 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  102394 | 3215 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 3216 | `	/* Release the conditional block */` |
|  102394 | 3217 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3218 | `	/* Statement successfully compiled */` |
|  102394 | 3219 | `	return SXRET_OK;` |
|     ! 0 | 3220 | `Synchronize:` |
|       - | 3221 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 3222 | `	 */` |
|     ! 0 | 3223 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3224 | `		pGen->pIn++;` |
|     ! 0 | 3225 | `	}` |
|     ! 0 | 3226 | `	return SXRET_OK;` |
|   51198 | 3227 |  |
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
|  107626 | 3321 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 3322 |  |
|  107628 | 3323 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3324 | `	sxi32 rc;` |
|       - | 3325 | `	/* Jump the 'return' keyword */` |
|  107628 | 3326 | `	pGen->pIn++;` |
|  107628 | 3327 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3328 | `		/* Compile the expression */` |
|  107606 | 3329 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  107606 | 3330 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3331 | `			return SXERR_ABORT;` |
|  107606 | 3332 | `		}else if(rc != SXERR_EMPTY ){` |
|  107606 | 3333 | `			nRet = 1;` |
|   53802 | 3334 | `		}` |
|   53802 | 3335 | `	}` |
|       - | 3336 | `	/* Emit the done instruction */` |
|  107628 | 3337 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  107628 | 3338 | `	return SXRET_OK;` |
|   53815 | 3339 |  |
|       - | 3340 | `/*` |
|       - | 3341 | ` * Compile the die/exit language construct.` |
|       - | 3342 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 3343 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 3344 | ` */` |
|      88 | 3345 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 | 3346 |  |
|      90 | 3347 | `	sxi32 nExpr = 0;` |
|       - | 3348 | `	sxi32 rc;` |
|       - | 3349 | `	/* Jump the die/exit keyword */` |
|      90 | 3350 | `	pGen->pIn++;` |
|      90 | 3351 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3352 | `		/* Compile the expression */` |
|      90 | 3353 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 | 3354 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3355 | `			return SXERR_ABORT;` |
|      90 | 3356 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 | 3357 | `			nExpr = 1;` |
|      44 | 3358 | `		}` |
|      44 | 3359 | `	}` |
|       - | 3360 | `	/* Emit the HALT instruction */` |
|      90 | 3361 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 | 3362 | `	return SXRET_OK;` |
|      46 | 3363 |  |
|       - | 3364 | `/*` |
|       - | 3365 | ` * Compile the 'echo' language construct.` |
|       - | 3366 | ` */` |
|    9724 | 3367 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 3368 |  |
|    9726 | 3369 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3370 | `	sxi32 rc;` |
|       - | 3371 | `	/* Jump the 'echo' keyword */` |
|    9726 | 3372 | `	pGen->pIn++;` |
|       - | 3373 | `	/* Compile arguments one after one */` |
|    9726 | 3374 | `	pTmp = pGen->pEnd;` |
|   19838 | 3375 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   10114 | 3376 | `		if( pGen->pIn < pNext ){` |
|   10114 | 3377 | `			pGen->pEnd = pNext;` |
|   10114 | 3378 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   10114 | 3379 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3380 | `				return SXERR_ABORT;` |
|   10114 | 3381 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3382 | `				/* Emit the consume instruction */` |
|   10090 | 3383 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    5044 | 3384 | `			}` |
|    5056 | 3385 | `		}` |
|       - | 3386 | `		/* Jump trailing commas */` |
|   10502 | 3387 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     390 | 3388 | `			pNext++;` |
|       2 | 3389 | `		}` |
|   10114 | 3390 | `		pGen->pIn = pNext;` |
|       2 | 3391 | `	}` |
|       - | 3392 | `	/* Restore token stream */` |
|    9726 | 3393 | `	pGen->pEnd = pTmp;` |
|    9726 | 3394 | `	return SXRET_OK;` |
|    4864 | 3395 |  |
|       - | 3396 | `/*` |
|       - | 3397 | ` * Compile the static statement.` |
|       - | 3398 | ` * According to the PHP language reference` |
|       - | 3399 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 3400 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 3401 | ` *  when program execution leaves this scope.` |
|       - | 3402 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 3403 | ` * Symisc eXtension.` |
|       - | 3404 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 3405 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3406 | ` *  Example` |
|       - | 3407 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 3408 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 3409 | ` */` |
|       2 | 3410 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 | 3411 |  |
|       - | 3412 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 3413 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 3414 | `	GenBlock *pBlock;` |
|       - | 3415 | `	SyString *pName;` |
|       - | 3416 | `	char *zDup;` |
|       - | 3417 | `	sxu32 nLine;` |
|       - | 3418 | `	sxi32 rc;` |
|       - | 3419 | `	/* Jump the static keyword */` |
|       3 | 3420 | `	nLine = pGen->pIn->nLine;` |
|       3 | 3421 | `	pGen->pIn++;` |
|       - | 3422 | `	/* Extract the enclosing function if any */` |
|       3 | 3423 | `	pBlock = pGen->pCurrent;` |
|       5 | 3424 | `	while( pBlock ){` |
|       5 | 3425 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 | 3426 | `			break;` |
|       - | 3427 | `		}` |
|       - | 3428 | `		/* Point to the upper block */` |
|       3 | 3429 | `		pBlock = pBlock->pParent;` |
|       1 | 3430 | `	}` |
|       3 | 3431 | `	if( pBlock == 0 ){` |
|       - | 3432 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 3433 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3434 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 | 3435 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3436 | `				return SXERR_ABORT;` |
|       - | 3437 | `			}` |
|     ! 0 | 3438 | `			goto Synchronize;` |
|       - | 3439 | `		}` |
|       - | 3440 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 3441 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 3442 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3443 | `			return SXERR_ABORT;` |
|     ! 0 | 3444 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 3445 | `			/* Emit the POP instruction */` |
|     ! 0 | 3446 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 3447 | `		}` |
|     ! 0 | 3448 | `		return SXRET_OK;` |
|       - | 3449 | `	}` |
|       3 | 3450 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 3451 | `	/* Make sure we are dealing with a valid statement */` |
|       3 | 3452 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 | 3453 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 | 3454 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 | 3455 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3456 | `				return SXERR_ABORT;` |
|       - | 3457 | `			}` |
|       3 | 3458 | `			goto Synchronize;` |
|       - | 3459 | `	}` |
|     ! 0 | 3460 | `	pGen->pIn++;` |
|       - | 3461 | `	/* Extract variable name */` |
|     ! 0 | 3462 | `	pName = &pGen->pIn->sData;` |
|     ! 0 | 3463 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 | 3464 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 3465 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3466 | `		goto Synchronize;` |
|       - | 3467 | `	}` |
|       - | 3468 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 | 3469 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 | 3470 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 3471 | `	/* Duplicate variable name */` |
|     ! 0 | 3472 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 | 3473 | `	if( zDup == 0 ){` |
|     ! 0 | 3474 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3475 | `		return SXERR_ABORT;` |
|       - | 3476 | `	}` |
|     ! 0 | 3477 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 3478 | `	/* Check if we have an expression to compile */` |
|     ! 0 | 3479 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 3480 | `		SySet *pInstrContainer;` |
|       - | 3481 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 3482 | `		 * Static variable can take any complex expression including function` |
|       - | 3483 | `		 * call as their initialization value.` |
|       - | 3484 | `		 * Example:` |
|       - | 3485 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 3486 | `		 */` |
|     ! 0 | 3487 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 3488 | `		/* Swap bytecode container */` |
|     ! 0 | 3489 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 | 3490 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 3491 | `		/* Compile the expression */` |
|     ! 0 | 3492 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3493 | `		/* Emit the done instruction */` |
|     ! 0 | 3494 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 3495 | `		/* Restore default bytecode container */` |
|     ! 0 | 3496 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 3497 | `	}` |
|       - | 3498 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 | 3499 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 | 3500 | `	return SXRET_OK;` |
|       1 | 3501 | `Synchronize:` |
|       - | 3502 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 3503 | `	 * statement.` |
|       - | 3504 | `	 */` |
|       5 | 3505 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 | 3506 | `		pGen->pIn++;` |
|       1 | 3507 | `	}` |
|       3 | 3508 | `	return SXRET_OK;` |
|       2 | 3509 |  |
|       - | 3510 | `/*` |
|       - | 3511 | ` * Compile the var statement.` |
|       - | 3512 | ` * Symisc Extension:` |
|       - | 3513 | ` *      var statement can be used outside of a class definition.` |
|       - | 3514 | ` */` |
|       4 | 3515 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 3516 |  |
|       - | 3517 | `	sxu32 nLine;` |
|       - | 3518 | `	sxi32 rc;` |
|       5 | 3519 | `	nLine = pGen->pIn->nLine;` |
|       - | 3520 | `	/* Jump the 'var' keyword */` |
|       5 | 3521 | `	pGen->pIn++;` |
|       5 | 3522 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 3523 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - | 3524 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 | 3525 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 | 3526 | `			pGen->pIn++;` |
|     ! 0 | 3527 | `		}` |
|     ! 0 | 3528 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3529 | `			return SXERR_ABORT;` |
|       - | 3530 | `		}` |
|     ! 0 | 3531 | `	}else{` |
|       - | 3532 | `		/* Compile the expression */` |
|       5 | 3533 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 | 3534 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3535 | `			return SXERR_ABORT;` |
|       5 | 3536 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 | 3537 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 3538 | `		}` |
|       - | 3539 | `	}` |
|       5 | 3540 | `	return SXRET_OK;` |
|       3 | 3541 |  |
|       - | 3542 | `/*` |
|       - | 3543 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 3544 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 3545 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 3546 | ` */` |
|       - | 3547 | `/*` |
|       - | 3548 | ` * Namespace-qualify a name for CALL/NEW instructions.` |
|       - | 3549 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 3550 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 3551 | ` * qualified name and updates the instruction's operand index.` |
|       - | 3552 | ` *` |
|       - | 3553 | ` * Resolution: use imports -> current NS prefix.` |
|       - | 3554 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 3555 | ` * Returns the (possibly new) literal index.` |
|       - | 3556 | ` */` |
|  253108 | 3557 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx)` |
|       2 | 3558 |  |
|       - | 3559 | `	ph7_value *pLit;` |
|       - | 3560 | `	const char *zLit;` |
|       - | 3561 | `	SyString sQualified;` |
|       - | 3562 | `	sxu32 nLit;` |
|       - | 3563 | `	sxu32 k;` |
|       - | 3564 | `	sxu32 nNewIdx;` |
|       - | 3565 | `	int hasNsSep;` |
|       - | 3566 | `	SyHashEntry *pImport;` |
|       - | 3567 | `	ph7_value *pNew;` |
|  253110 | 3568 | `	if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  253004 | 3569 | `		return nOrigIdx; /* Not in a namespace */` |
|       - | 3570 | `	}` |
|     107 | 3571 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|     107 | 3572 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 3573 | `		return nOrigIdx;` |
|       - | 3574 | `	}` |
|     107 | 3575 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|     107 | 3576 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 3577 | `	/* Skip if already qualified (contains backslash) */` |
|     107 | 3578 | `	hasNsSep = 0;` |
|     521 | 3579 | `	for( k = 0; k < nLit; k++ ){` |
|     465 | 3580 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|     208 | 3581 | `	}` |
|     107 | 3582 | `	if( hasNsSep ){` |
|      51 | 3583 | `		return nOrigIdx;` |
|       - | 3584 | `	}` |
|       - | 3585 | `	/* Build the qualified name into sWorker */` |
|      57 | 3586 | `	SyBlobReset(&pGen->sWorker);` |
|       - | 3587 | `	/* Check use imports first */` |
|      57 | 3588 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zLit,nLit);` |
|      57 | 3589 | `	if( pImport ){` |
|      15 | 3590 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      15 | 3591 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       8 | 3592 | `	}else{` |
|       - | 3593 | `		/* Prepend current namespace */` |
|      43 | 3594 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      43 | 3595 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      43 | 3596 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 3597 | `	}` |
|       - | 3598 | `	/* Look up or create a new literal for the qualified name */` |
|      57 | 3599 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      57 | 3600 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      17 | 3601 | `		return nNewIdx; /* Already interned */` |
|       - | 3602 | `	}` |
|      41 | 3603 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      41 | 3604 | `	if( pNew == 0 ){` |
|     ! 0 | 3605 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 3606 | `	}` |
|      41 | 3607 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      41 | 3608 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      41 | 3609 | `	return nNewIdx;` |
|  126556 | 3610 |  |
|       - | 3611 | `/*` |
|       - | 3612 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 3613 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 3614 | ` */` |
|   15450 | 3615 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3616 |  |
|       - | 3617 | `	SyHashEntry *pImport;` |
|       - | 3618 | `	/* Check use imports first */` |
|   15452 | 3619 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   15452 | 3620 | `	if( pImport ){` |
|       7 | 3621 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       7 | 3622 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       7 | 3623 | `		return;` |
|       - | 3624 | `	}` |
|       - | 3625 | `	/* Prepend current namespace if active */` |
|   15446 | 3626 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 3627 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 3628 | `		SyBlobAppend(pOut,"\\",1);` |
|       1 | 3629 | `	}` |
|   15446 | 3630 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    7727 | 3631 |  |
|       - | 3632 | `/*` |
|       - | 3633 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 3634 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 3635 | ` * The caller must release pOut when done.` |
|       - | 3636 | ` */` |
|   31062 | 3637 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3638 |  |
|   31064 | 3639 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      33 | 3640 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      33 | 3641 | `		SyBlobAppend(pOut,"\\",1);` |
|      16 | 3642 | `	}` |
|   31064 | 3643 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   31064 | 3644 |  |
|       - | 3645 | `/*` |
|       - | 3646 | ` * Compile a namespace statement` |
|       - | 3647 | ` * According to the PHP language reference manual` |
|       - | 3648 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 3649 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 3650 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 3651 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 3652 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 3653 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 3654 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 3655 | ` *  programming world.` |
|       - | 3656 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 3657 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 3658 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 3659 | ` *  classes/functions/constants.` |
|       - | 3660 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 3661 | ` *  readability of source code.` |
|       - | 3662 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 3663 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 3664 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 3665 | ` *       class MyClass {}` |
|       - | 3666 | ` *       function myfunction() {}` |
|       - | 3667 | ` *       const MYCONST = 1;` |
|       - | 3668 | ` *       $a = new MyClass;` |
|       - | 3669 | ` *       $c = new \my\name\MyClass;` |
|       - | 3670 | ` *       $a = strlen('hi');` |
|       - | 3671 | ` *       $d = namespace\MYCONST;` |
|       - | 3672 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 3673 | ` *       echo constant($d);` |
|       - | 3674 | ` * NOTE` |
|       - | 3675 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3676 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3677 | ` */` |
|       - | 3678 | `/*` |
|       - | 3679 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 3680 | ` */` |
|       6 | 3681 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 | 3682 |  |
|       7 | 3683 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|     ! 0 | 3684 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|     ! 0 | 3685 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|     ! 0 | 3686 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|     ! 0 | 3687 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|     ! 0 | 3688 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|     ! 0 | 3689 | `	return "token";` |
|       4 | 3690 |  |
|      56 | 3691 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       1 | 3692 |  |
|       - | 3693 | `	sxu32 nLine;` |
|       - | 3694 | `	sxi32 rc;` |
|      57 | 3695 | `	nLine = pGen->pIn->nLine;` |
|      57 | 3696 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 3697 | `	/* Reset namespace and clear previous use imports */` |
|      57 | 3698 | `	SyBlobReset(&pGen->sNamespace);` |
|      57 | 3699 | `	SyHashRelease(&pGen->hUseImports);` |
|      57 | 3700 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      57 | 3701 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3702 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 | 3703 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3704 | `		return SXRET_OK;` |
|       - | 3705 | `	}` |
|      57 | 3706 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - | 3707 | `		/* namespace; — switch to global namespace */` |
|     ! 0 | 3708 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3709 | `		return SXRET_OK;` |
|       - | 3710 | `	}` |
|      57 | 3711 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - | 3712 | `		/* namespace { } — global namespace block */` |
|     ! 0 | 3713 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3714 | `		return SXRET_OK;` |
|       - | 3715 | `	}` |
|       - | 3716 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     143 | 3717 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      87 | 3718 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 3719 | `			/* Append backslash separator */` |
|      17 | 3720 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      17 | 3721 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       8 | 3722 | `			}` |
|       9 | 3723 | `		}else{` |
|       - | 3724 | `			/* Append identifier */` |
|      71 | 3725 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 3726 | `		}` |
|      87 | 3727 | `		pGen->pIn++;` |
|       1 | 3728 | `	}` |
|       - | 3729 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - | 3730 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - | 3731 | `	{` |
|      57 | 3732 | `		char *zNsDup = 0;` |
|      57 | 3733 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      82 | 3734 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      54 | 3735 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      27 | 3736 | `		}` |
|      57 | 3737 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - | 3738 | `	}` |
|      57 | 3739 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 | 3740 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3741 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 3742 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 | 3743 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3744 | `			return SXERR_ABORT;` |
|       - | 3745 | `		}` |
|       2 | 3746 | `	}` |
|      57 | 3747 | `	return SXRET_OK;` |
|      29 | 3748 |  |
|       - | 3749 | `/*` |
|       - | 3750 | ` * Compile the 'use' statement` |
|       - | 3751 | ` * According to the PHP language reference manual` |
|       - | 3752 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 3753 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 3754 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 3755 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 3756 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 3757 | ` *  a function or constant is not supported.` |
|       - | 3758 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 3759 | ` * NOTE` |
|       - | 3760 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3761 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3762 | ` */` |
|      30 | 3763 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       1 | 3764 |  |
|       - | 3765 | `	sxu32 nLine;` |
|       - | 3766 | `	sxi32 rc;` |
|       - | 3767 | `	SyBlob sPath;` |
|       - | 3768 | `	SyString sAlias;` |
|       - | 3769 | `	SyToken *pLast;` |
|       - | 3770 | `	char *zDup;` |
|      31 | 3771 | `	nLine = pGen->pIn->nLine;` |
|      31 | 3772 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|      31 | 3773 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 3774 | `	/* Process one or more use declarations separated by commas */` |
|      16 | 3775 | `	for(;;){` |
|      33 | 3776 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3777 | `			break;` |
|       - | 3778 | `		}` |
|      33 | 3779 | `		SyBlobReset(&sPath);` |
|      33 | 3780 | `		pLast = 0;` |
|       - | 3781 | `		/* Collect the full namespace path */` |
|     133 | 3782 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     101 | 3783 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      65 | 3784 | `				pLast = pGen->pIn;` |
|      65 | 3785 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      37 | 3786 | `					SyBlobAppend(&sPath,"\\",1);` |
|      18 | 3787 | `				}` |
|      65 | 3788 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      32 | 3789 | `			}` |
|     101 | 3790 | `			pGen->pIn++;` |
|       1 | 3791 | `		}` |
|      33 | 3792 | `		if( pLast == 0 ){` |
|       - | 3793 | `			/* Empty path */` |
|       5 | 3794 | `			break;` |
|       - | 3795 | `		}` |
|       - | 3796 | `		/* Default alias is the last component of the path */` |
|      29 | 3797 | `		sAlias = pLast->sData;` |
|       - | 3798 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      28 | 3799 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      18 | 3800 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       7 | 3801 | `			pGen->pIn++; /* Jump 'as' */` |
|       7 | 3802 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       7 | 3803 | `				sAlias = pGen->pIn->sData;` |
|       7 | 3804 | `				pGen->pIn++;` |
|       3 | 3805 | `			}` |
|       3 | 3806 | `		}` |
|       - | 3807 | `		/* Check for duplicate import alias */` |
|      29 | 3808 | `		if( SyHashGet(&pGen->hUseImports,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 | 3809 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3810 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 | 3811 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 | 3812 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3813 | `				SyBlobRelease(&sPath);` |
|     ! 0 | 3814 | `				return SXERR_ABORT;` |
|       - | 3815 | `			}` |
|       2 | 3816 | `		}` |
|       - | 3817 | `		/* Register the import: alias -> FQN.` |
|       - | 3818 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - | 3819 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - | 3820 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      43 | 3821 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      28 | 3822 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      29 | 3823 | `		if( zDup ){` |
|       - | 3824 | `			char *zAliasDup;` |
|      29 | 3825 | `			SyHashInsert(&pGen->hUseImports,sAlias.zString,sAlias.nByte,zDup);` |
|       - | 3826 | `			/* Duplicate the alias key for the VM hash (token pointers may not survive to runtime) */` |
|      29 | 3827 | `			zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      29 | 3828 | `			if( zAliasDup ){` |
|      29 | 3829 | `				SyHashInsert(&pGen->pVm->hUseImports,zAliasDup,sAlias.nByte,zDup);` |
|      14 | 3830 | `			}` |
|      14 | 3831 | `		}` |
|       - | 3832 | `		/* Check for comma (multiple use declarations) */` |
|      29 | 3833 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3834 | `			pGen->pIn++;` |
|       2 | 3835 | `		}else{` |
|      14 | 3836 | `			break;` |
|       - | 3837 | `		}` |
|       1 | 3838 | `	}` |
|      31 | 3839 | `	SyBlobRelease(&sPath);` |
|      31 | 3840 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 3841 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 3842 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 3843 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3844 | `			return SXERR_ABORT;` |
|       - | 3845 | `		}` |
|       1 | 3846 | `	}` |
|      31 | 3847 | `	return SXRET_OK;` |
|      16 | 3848 |  |
|       - | 3849 | `/*` |
|       - | 3850 | ` * Compile the stupid 'declare' language construct.` |
|       - | 3851 | ` *` |
|       - | 3852 | ` * According to the PHP language reference manual.` |
|       - | 3853 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 3854 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 3855 | ` *  declare (directive)` |
|       - | 3856 | ` *   statement` |
|       - | 3857 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 3858 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 3859 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 3860 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 3861 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 3862 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 3863 | ` * <?php` |
|       - | 3864 | ` * // these are the same:` |
|       - | 3865 | ` * // you can use this:` |
|       - | 3866 | ` * declare(ticks=1) {` |
|       - | 3867 | ` *   // entire script here` |
|       - | 3868 | ` * }` |
|       - | 3869 | ` * // or you can use this:` |
|       - | 3870 | ` * declare(ticks=1);` |
|       - | 3871 | ` * // entire script here` |
|       - | 3872 | ` * ?>` |
|       - | 3873 | ` *` |
|       - | 3874 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 3875 | ` */` |
|       8 | 3876 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 3877 |  |
|       9 | 3878 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 3879 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 3880 | `	sxi32 rc;` |
|       9 | 3881 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 3882 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 3883 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 3884 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3885 | `			return SXERR_ABORT;` |
|       - | 3886 | `		}` |
|       5 | 3887 | `		goto Synchro;` |
|       - | 3888 | `	}` |
|       5 | 3889 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 3890 | `	/* Delimit the directive */` |
|       5 | 3891 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 3892 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 3893 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 3894 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3895 | `			return SXERR_ABORT;` |
|       - | 3896 | `		}` |
|     ! 0 | 3897 | `		return SXRET_OK;` |
|       - | 3898 | `	}` |
|       - | 3899 | `	/* Update the cursor */` |
|       5 | 3900 | `	pGen->pIn = &pEnd[1];` |
|       5 | 3901 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 3902 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 3903 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3904 | `			return SXERR_ABORT;` |
|       - | 3905 | `		}` |
|     ! 0 | 3906 | `	}` |
|       - | 3907 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 3908 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 3909 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 3910 | `		ph7_lib_version()` |
|       - | 3911 | `		);` |
|       - | 3912 | `	/*All done */` |
|       5 | 3913 | `	return SXRET_OK;` |
|       2 | 3914 | `Synchro:` |
|       - | 3915 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 3916 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 3917 | `		pGen->pIn++;` |
|       1 | 3918 | `	}` |
|       5 | 3919 | `	return SXRET_OK;` |
|       5 | 3920 |  |
|       - | 3921 | `/*` |
|       - | 3922 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 3923 | ` * as follows:` |
|       - | 3924 | ` * function makecoffee($type = "cappuccino")` |
|       - | 3925 | ` * {` |
|       - | 3926 | ` *   return "Making a cup of $type.\n";` |
|       - | 3927 | ` * }` |
|       - | 3928 | ` * Symisc eXtension.` |
|       - | 3929 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 3930 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 3931 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 3932 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 3933 | ` *      {` |
|       - | 3934 | ` *       var_dump($a);` |
|       - | 3935 | ` *      }` |
|       - | 3936 | ` *     //call test without args` |
|       - | 3937 | ` *      test();` |
|       - | 3938 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 3939 | ` *      Example:` |
|       - | 3940 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 3941 | ` * 3 -) Function overloading!!` |
|       - | 3942 | ` *      Example:` |
|       - | 3943 | ` *      function foo($a) {` |
|       - | 3944 | ` *   	  return $a.PHP_EOL;` |
|       - | 3945 | ` *	    }` |
|       - | 3946 | ` *	    function foo($a, $b) {` |
|       - | 3947 | ` *   	  return $a + $b;` |
|       - | 3948 | ` *	    }` |
|       - | 3949 | ` *	    echo foo(5); // Prints "5"` |
|       - | 3950 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 3951 | ` *      // Same arg` |
|       - | 3952 | ` *	   function foo(string $a)` |
|       - | 3953 | ` *	   {` |
|       - | 3954 | ` *	     echo "a is a string\n";` |
|       - | 3955 | ` *	     var_dump($a);` |
|       - | 3956 | ` *	   }` |
|       - | 3957 | ` *	  function foo(int $a)` |
|       - | 3958 | ` *	  {` |
|       - | 3959 | ` *	    echo "a is integer\n";` |
|       - | 3960 | ` *	    var_dump($a);` |
|       - | 3961 | ` *	  }` |
|       - | 3962 | ` *	  function foo(array $a)` |
|       - | 3963 | ` *	  {` |
|       - | 3964 | ` * 	    echo "a is an array\n";` |
|       - | 3965 | ` * 	    var_dump($a);` |
|       - | 3966 | ` *	  }` |
|       - | 3967 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 3968 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 3969 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 3970 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 3971 | ` * introduced by the PH7 engine.` |
|       - | 3972 | ` */` |
|   33160 | 3973 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 3974 |  |
|       - | 3975 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 3976 | `	SySet *pInstrContainer;` |
|       - | 3977 | `	sxi32 rc;` |
|       - | 3978 | `	/* Swap token stream */` |
|   33162 | 3979 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   33162 | 3980 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   33162 | 3981 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 3982 | `	/* Compile the expression holding the argument value */` |
|   33162 | 3983 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3984 | `	/* Emit the done instruction */` |
|   33162 | 3985 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   33162 | 3986 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   33162 | 3987 | `	RE_SWAP_DELIMITER(pGen);` |
|   33162 | 3988 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3989 | `		return SXERR_ABORT;` |
|       - | 3990 | `	}` |
|   33162 | 3991 | `	return SXRET_OK;` |
|   16582 | 3992 |  |
|       - | 3993 | `/*` |
|       - | 3994 | ` * Collect function arguments one after one.` |
|       - | 3995 | ` * According to the PHP language reference manual.` |
|       - | 3996 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 3997 | ` * list of expressions.` |
|       - | 3998 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 3999 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 4000 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 4001 | ` * for more information.` |
|       - | 4002 | ` * Example #1 Passing arrays to functions` |
|       - | 4003 | ` * <?php` |
|       - | 4004 | ` * function takes_array($input)` |
|       - | 4005 | ` * {` |
|       - | 4006 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 4007 | ` * }` |
|       - | 4008 | ` * ?>` |
|       - | 4009 | ` * Making arguments be passed by reference` |
|       - | 4010 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 4011 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 4012 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 4013 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 4014 | ` * to the argument name in the function definition:` |
|       - | 4015 | ` * Example #2 Passing function parameters by reference` |
|       - | 4016 | ` * <?php` |
|       - | 4017 | ` * function add_some_extra(&$string)` |
|       - | 4018 | ` * {` |
|       - | 4019 | ` *   $string .= 'and something extra.';` |
|       - | 4020 | ` * }` |
|       - | 4021 | ` * $str = 'This is a string, ';` |
|       - | 4022 | ` * add_some_extra($str);` |
|       - | 4023 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 4024 | ` * ?>` |
|       - | 4025 | ` *` |
|       - | 4026 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 4027 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 4028 | ` * on these extension.` |
|       - | 4029 | ` */` |
|   36176 | 4030 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 4031 |  |
|       - | 4032 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 4033 | `	SyToken *pIn;  /* Token stream */` |
|       - | 4034 | `	SyBlob sSig;         /* Function signature */` |
|       - | 4035 | `	char *zDup;          /* Copy of argument name */` |
|       - | 4036 | `	sxi32 rc;` |
|       - | 4037 |  |
|   36178 | 4038 | `	pIn = pGen->pIn;` |
|   36178 | 4039 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 4040 | `	/* Process arguments one after one */` |
|   49161 | 4041 | `	for(;;){` |
|   98324 | 4042 | `		if( pIn >= pEnd ){` |
|       - | 4043 | `			/* No more arguments to process */` |
|   36176 | 4044 | `			break;` |
|       - | 4045 | `		}` |
|   62150 | 4046 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   62150 | 4047 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   62150 | 4048 | `		if( pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   51004 | 4049 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   45904 | 4050 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   45904 | 4051 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 4052 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   45904 | 4053 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 4054 | `					sArg.nType = MEMOBJ_BOOL;` |
|   45904 | 4055 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   12752 | 4056 | `					sArg.nType = MEMOBJ_INT;` |
|   39529 | 4057 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   33152 | 4058 | `					sArg.nType = MEMOBJ_STRING;` |
|   16578 | 4059 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 4060 | `					sArg.nType = MEMOBJ_REAL;` |
|     ! 0 | 4061 | `				}else{` |
|       4 | 4062 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 4063 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 4064 | `						&pIn->sData);` |
|       - | 4065 | `				}` |
|   22953 | 4066 | `			}else{` |
|    5102 | 4067 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 4068 | `				char *zDupLocal;` |
|       - | 4069 | `				/* Argument must be a class instance,record that*/` |
|    5102 | 4070 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    5102 | 4071 | `				if( zDupLocal ){` |
|    5102 | 4072 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    5102 | 4073 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2550 | 4074 | `				}` |
|       - | 4075 | `			}` |
|   51004 | 4076 | `			pIn++;` |
|   25501 | 4077 | `		}` |
|   62150 | 4078 | `		if( pIn >= pEnd ){` |
|     ! 0 | 4079 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 4080 | `			return rc;` |
|       - | 4081 | `		}` |
|   62150 | 4082 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 4083 | `			/* Pass by reference,record that */` |
|    2574 | 4084 | `			sArg.iFlags = VM_FUNC_ARG_BY_REF;` |
|    2574 | 4085 | `			pIn++;` |
|    1286 | 4086 | `		}` |
|   62150 | 4087 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4088 | `			/* Invalid argument */` |
|     ! 0 | 4089 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 4090 | `			return rc;` |
|       - | 4091 | `		}` |
|   62150 | 4092 | `		pIn++; /* Jump the dollar sign */` |
|       - | 4093 | `		/* Copy argument name */` |
|   62150 | 4094 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   62150 | 4095 | `		if( zDup == 0 ){` |
|     ! 0 | 4096 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 4097 | `			return SXERR_ABORT;` |
|       - | 4098 | `		}` |
|   62150 | 4099 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   62150 | 4100 | `		pIn++;` |
|   62150 | 4101 | `		if( pIn < pEnd ){` |
|   38738 | 4102 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 4103 | `				SyToken *pDefend;` |
|   33164 | 4104 | `				sxi32 iNest = 0;` |
|   33164 | 4105 | `				pIn++; /* Jump the equal sign */` |
|   33164 | 4106 | `				pDefend = pIn;` |
|       - | 4107 | `				/* Process the default value associated with this argument */` |
|   71424 | 4108 | `				while( pDefend < pEnd ){` |
|   58662 | 4109 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   20402 | 4110 | `						break;` |
|       - | 4111 | `					}` |
|   38262 | 4112 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 4113 | `						/* Increment nesting level */` |
|    2552 | 4114 | `						iNest++;` |
|   36987 | 4115 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 4116 | `						/* Decrement nesting level */` |
|    2552 | 4117 | `						iNest--;` |
|    1275 | 4118 | `					}` |
|   38262 | 4119 | `					pDefend++;` |
|       2 | 4120 | `				}` |
|   33164 | 4121 | `				if( pIn >= pDefend ){` |
|       3 | 4122 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 4123 | `					return rc;` |
|       - | 4124 | `				}` |
|       - | 4125 | `				/* Process default value */` |
|   33162 | 4126 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   33162 | 4127 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 4128 | `					return rc;` |
|       - | 4129 | `				}` |
|       - | 4130 | `				/* Point beyond the default value */` |
|   33162 | 4131 | `				pIn = pDefend;` |
|   16580 | 4132 | `			}` |
|   38736 | 4133 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 4134 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 4135 | `				return rc;` |
|       - | 4136 | `			}` |
|   38736 | 4137 | `			pIn++; /* Jump the trailing comma */` |
|   19367 | 4138 | `		}` |
|       - | 4139 | `		/* Append argument signature */` |
|   62148 | 4140 | `		if( sArg.nType > 0 ){` |
|   51002 | 4141 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 4142 | `				/* Class name */` |
|    5102 | 4143 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2552 | 4144 | `			}else{` |
|       - | 4145 | `				int c;` |
|   45902 | 4146 | `				c = 'n'; /* cc warning */` |
|       - | 4147 | `				/* Type leading character */` |
|   45902 | 4148 | `				switch(sArg.nType){` |
|     ! 0 | 4149 | `				case MEMOBJ_HASHMAP:` |
|       - | 4150 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 4151 | `					c = 'h';` |
|     ! 0 | 4152 | `					break;` |
|    6375 | 4153 | `				case MEMOBJ_INT:` |
|       - | 4154 | `					/* Integer */` |
|   12752 | 4155 | `					c = 'i';` |
|   12752 | 4156 | `					break;` |
|     ! 0 | 4157 | `				case MEMOBJ_BOOL:` |
|       - | 4158 | `					/* Bool */` |
|     ! 0 | 4159 | `					c = 'b';` |
|     ! 0 | 4160 | `					break;` |
|     ! 0 | 4161 | `				case MEMOBJ_REAL:` |
|       - | 4162 | `					/* Float */` |
|     ! 0 | 4163 | `					c = 'f';` |
|     ! 0 | 4164 | `					break;` |
|   16575 | 4165 | `				case MEMOBJ_STRING:` |
|       - | 4166 | `					/* String */` |
|   33152 | 4167 | `					c = 's';` |
|   33150 | 4168 | `					break;` |
|     ! 0 | 4169 | `				default:` |
|     ! 0 | 4170 | `					break;` |
|       - | 4171 | `				}` |
|   45902 | 4172 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 4173 | `			}` |
|   25502 | 4174 | `		}else{` |
|       - | 4175 | `			/* No type is associated with this parameter which mean` |
|       - | 4176 | `			 * that this function is not condidate for overloading.` |
|       - | 4177 | `			 */` |
|   11148 | 4178 | `			SyBlobRelease(&sSig);` |
|       - | 4179 | `		}` |
|       - | 4180 | `		/* Save in the argument set */` |
|   62148 | 4181 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 4182 | `	}` |
|   36176 | 4183 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 4184 | `		/* Save function signature */` |
|   30602 | 4185 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   15300 | 4186 | `	}` |
|   36176 | 4187 | `	return SXRET_OK;` |
|   18090 | 4188 |  |
|       - | 4189 | `/*` |
|       - | 4190 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 4191 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 4192 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 4193 | ` */` |
|   87596 | 4194 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 4195 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 4196 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 4197 | `	)` |
|       2 | 4198 |  |
|       - | 4199 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 4200 | `	GenBlock *pBlock;` |
|       - | 4201 | `	sxu32 nGotoOfft;` |
|       - | 4202 | `	sxi32 rc;` |
|       - | 4203 | `	/* Attach the new function */` |
|   87598 | 4204 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   87598 | 4205 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4206 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 4207 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4208 | `		return SXERR_ABORT;` |
|       - | 4209 | `	}` |
|   87598 | 4210 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 4211 | `	/* Swap bytecode containers */` |
|   87598 | 4212 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   87598 | 4213 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 4214 | `	/* Compile the body */` |
|   87598 | 4215 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 4216 | `	/* Fix exception jumps now the destination is resolved */` |
|   87598 | 4217 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4218 | `	/* Emit the final return if not yet done */` |
|   87598 | 4219 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4220 | `	/* Fix gotos jumps now the destination is resolved */` |
|   87598 | 4221 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 4222 | `		rc = SXERR_ABORT;` |
|     ! 0 | 4223 | `	}` |
|   87598 | 4224 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 4225 | `	/* Restore the default container */` |
|   87598 | 4226 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4227 | `	/* Leave function block */` |
|   87598 | 4228 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   87598 | 4229 | `	if( rc == SXERR_ABORT ){` |
|       - | 4230 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4231 | `		return SXERR_ABORT;` |
|       - | 4232 | `	}` |
|       - | 4233 | `	/* All done, function body compiled */` |
|   87598 | 4234 | `	return SXRET_OK;` |
|   43800 | 4235 |  |
|       - | 4236 | `/*` |
|       - | 4237 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 4238 | ` * According to the PHP language reference manual.` |
|       - | 4239 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 4240 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 4241 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 4242 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4243 | ` *  Functions need not be defined before they are referenced.` |
|       - | 4244 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 4245 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 4246 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 4247 | ` *  calls with over 32-64 recursion levels.` |
|       - | 4248 | ` *` |
|       - | 4249 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 4250 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 4251 | ` * on these extension.` |
|       - | 4252 | ` */` |
|   33668 | 4253 | `static sxi32 GenStateCompileFunc(` |
|       - | 4254 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4255 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 4256 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 4257 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 4258 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 4259 | `	)` |
|       2 | 4260 |  |
|       - | 4261 | `	ph7_vm_func *pFunc;` |
|       - | 4262 | `	SyToken *pEnd;` |
|       - | 4263 | `	sxu32 nLine;` |
|       - | 4264 | `	char *zName;` |
|       - | 4265 | `	sxi32 rc;` |
|       - | 4266 | `	/* Extract line number */` |
|   33670 | 4267 | `	nLine = pGen->pIn->nLine;` |
|       - | 4268 | `	/* Jump the left parenthesis '(' */` |
|   33670 | 4269 | `	pGen->pIn++;` |
|       - | 4270 | `	/* Delimit the function signature */` |
|   33670 | 4271 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   33670 | 4272 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4273 | `		/* Syntax error */` |
|       7 | 4274 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 4275 | `		if( rc == SXERR_ABORT ){` |
|       - | 4276 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4277 | `			return SXERR_ABORT;` |
|       - | 4278 | `		}` |
|       7 | 4279 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 4280 | `		return SXRET_OK;` |
|       - | 4281 | `	}` |
|       - | 4282 | `	/* Create the function state */` |
|   33664 | 4283 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   33664 | 4284 | `	if( pFunc == 0 ){` |
|     ! 0 | 4285 | `		goto OutOfMem;` |
|       - | 4286 | `	}` |
|       - | 4287 | `	/* Build the function name, prepending namespace if active */` |
|   33668 | 4288 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 4289 | `		SyBlob sFQN;` |
|       - | 4290 | `		sxu32 nLen;` |
|       9 | 4291 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       9 | 4292 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       9 | 4293 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       9 | 4294 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       9 | 4295 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       9 | 4296 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       9 | 4297 | `		SyBlobRelease(&sFQN);` |
|       9 | 4298 | `		if( zName == 0 ){` |
|     ! 0 | 4299 | `			goto OutOfMem;` |
|       - | 4300 | `		}` |
|       9 | 4301 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       5 | 4302 | `	}else{` |
|   33656 | 4303 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   33656 | 4304 | `		if( zName == 0 ){` |
|     ! 0 | 4305 | `			goto OutOfMem;` |
|       - | 4306 | `		}` |
|   33656 | 4307 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 4308 | `	}` |
|   33664 | 4309 | `	if( pGen->pIn < pEnd ){` |
|       - | 4310 | `		/* Collect function arguments */` |
|   23348 | 4311 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   23348 | 4312 | `		if( rc == SXERR_ABORT ){` |
|       - | 4313 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4314 | `			return SXERR_ABORT;` |
|       - | 4315 | `		}` |
|   11673 | 4316 | `	}` |
|       - | 4317 | `	/* Compile function body */` |
|   33664 | 4318 | `	pGen->pIn = &pEnd[1];` |
|   33664 | 4319 | `	if( bHandleClosure ){` |
|       - | 4320 | `		ph7_vm_func_closure_env sEnv;` |
|     134 | 4321 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     132 | 4322 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      73 | 4323 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      12 | 4324 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 4325 | `				/* Closure,record environment variable */` |
|      12 | 4326 | `				pGen->pIn++;` |
|      12 | 4327 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 4328 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 4329 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4330 | `						return SXERR_ABORT;` |
|       - | 4331 | `					}` |
|     ! 0 | 4332 | `				}` |
|      12 | 4333 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 4334 | `				/* Compile until we hit the first closing parenthesis */` |
|      22 | 4335 | `				while( pGen->pIn < pGen->pEnd ){` |
|      22 | 4336 | `					int iFlagsLocal = 0;` |
|      22 | 4337 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      12 | 4338 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      12 | 4339 | `						break;` |
|       - | 4340 | `					}` |
|      12 | 4341 | `					nLineLocal = pGen->pIn->nLine;` |
|      12 | 4342 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 4343 | `						/* Pass by reference,record that */` |
|     ! 0 | 4344 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 4345 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 4346 | `							);` |
|     ! 0 | 4347 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 4348 | `						pGen->pIn++;` |
|     ! 0 | 4349 | `					}` |
|      10 | 4350 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      12 | 4351 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 4352 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 4353 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 4354 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4355 | `								return SXERR_ABORT;` |
|       - | 4356 | `							}` |
|       - | 4357 | `							/* Find the closing parenthesis */` |
|     ! 0 | 4358 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 4359 | `								pGen->pIn++;` |
|     ! 0 | 4360 | `							}` |
|     ! 0 | 4361 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 4362 | `								pGen->pIn++;` |
|     ! 0 | 4363 | `							}` |
|     ! 0 | 4364 | `							break;` |
|       - | 4365 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 4366 | `					}else{` |
|       - | 4367 | `						SyString *pNameLocal;` |
|       - | 4368 | `						char *zDup;` |
|       - | 4369 | `						/* Duplicate variable name */` |
|      12 | 4370 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      12 | 4371 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      12 | 4372 | `						if( zDup ){` |
|       - | 4373 | `							/* Zero the structure */` |
|      12 | 4374 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      12 | 4375 | `							sEnv.iFlags = iFlagsLocal;` |
|      12 | 4376 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      12 | 4377 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      12 | 4378 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 4379 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 4380 | `									got_this = 1;` |
|     ! 0 | 4381 | `							}` |
|       - | 4382 | `							/* Save imported variable */` |
|      12 | 4383 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       7 | 4384 | `						}else{` |
|     ! 0 | 4385 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4386 | `							 return SXERR_ABORT;` |
|       - | 4387 | `						}` |
|       - | 4388 | `					}` |
|      12 | 4389 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      12 | 4390 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4391 | `						/* Ignore trailing commas */` |
|     ! 0 | 4392 | `						pGen->pIn++;` |
|     ! 0 | 4393 | `					}` |
|       2 | 4394 | `				}` |
|      12 | 4395 | `				if( !got_this ){` |
|       - | 4396 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 4397 | `					 * available to the closure environment.` |
|       - | 4398 | `					 */` |
|      12 | 4399 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      12 | 4400 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      12 | 4401 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      12 | 4402 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      12 | 4403 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       5 | 4404 | `				}` |
|      12 | 4405 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 4406 | `					/* Mark as closure */` |
|      12 | 4407 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       5 | 4408 | `				}` |
|       5 | 4409 | `		}` |
|      66 | 4410 | `	}` |
|       - | 4411 | `	/* Compile the body */` |
|   33664 | 4412 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   33664 | 4413 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4414 | `		return SXERR_ABORT;` |
|       - | 4415 | `	}` |
|   33664 | 4416 | `	if( ppFunc ){` |
|     134 | 4417 | `		*ppFunc = pFunc;` |
|      66 | 4418 | `	}` |
|   33664 | 4419 | `	rc = SXRET_OK;` |
|   33664 | 4420 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 4421 | `		/* Finally register the function */` |
|   33654 | 4422 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   16826 | 4423 | `	}` |
|   33664 | 4424 | `	if( rc == SXRET_OK ){` |
|   33664 | 4425 | `		return SXRET_OK;` |
|       - | 4426 | `	}` |
|       - | 4427 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 4428 | `OutOfMem:` |
|       - | 4429 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 4430 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 4431 | `	 */` |
|     ! 0 | 4432 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 4433 | `	return SXERR_ABORT;` |
|   16836 | 4434 |  |
|       - | 4435 | `/*` |
|       - | 4436 | ` * Compile a standard PHP function.` |
|       - | 4437 | ` *  Refer to the block-comment above for more information.` |
|       - | 4438 | ` */` |
|   33542 | 4439 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 4440 |  |
|       - | 4441 | `	SyString *pName;` |
|       - | 4442 | `	sxi32 iFlags;` |
|       - | 4443 | `	sxu32 nLine;` |
|       - | 4444 | `	sxi32 rc;` |
|       - | 4445 |  |
|   33544 | 4446 | `	nLine = pGen->pIn->nLine;` |
|   33544 | 4447 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   33544 | 4448 | `	iFlags = 0;` |
|   33544 | 4449 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4450 | `		/* Return by reference,remember that */` |
|       7 | 4451 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4452 | `		/* Jump the '&' token */` |
|       7 | 4453 | `		pGen->pIn++;` |
|       3 | 4454 | `	}` |
|   33544 | 4455 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4456 | `		/* Invalid function name */` |
|       5 | 4457 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 4458 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4459 | `			return SXERR_ABORT;` |
|       - | 4460 | `		}` |
|       - | 4461 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 4462 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 4463 | `			pGen->pIn++;` |
|       1 | 4464 | `		}` |
|       5 | 4465 | `		return SXRET_OK;` |
|       - | 4466 | `	}` |
|   33540 | 4467 | `	pName = &pGen->pIn->sData;` |
|   33540 | 4468 | `	nLine = pGen->pIn->nLine;` |
|       - | 4469 | `	/* Jump the function name */` |
|   33540 | 4470 | `	pGen->pIn++;` |
|   33540 | 4471 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4472 | `		/* Syntax error */` |
|       3 | 4473 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 4474 | `		if( rc == SXERR_ABORT ){` |
|       - | 4475 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4476 | `			return SXERR_ABORT;` |
|       - | 4477 | `		}` |
|       - | 4478 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 4479 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4480 | `			pGen->pIn++;` |
|     ! 0 | 4481 | `		}` |
|       3 | 4482 | `		return SXRET_OK;` |
|       - | 4483 | `	}` |
|       - | 4484 | `	/* Compile function body */` |
|   33538 | 4485 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   33538 | 4486 | `	return rc;` |
|   16773 | 4487 |  |
|       - | 4488 | `/*` |
|       - | 4489 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 4490 | ` * According to the PHP language reference manual` |
|       - | 4491 | ` *  Visibility:` |
|       - | 4492 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 4493 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 4494 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 4495 | ` *  Members declared protected can be accessed only within the class` |
|       - | 4496 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 4497 | ` *  may only be accessed by the class that defines the member.` |
|       - | 4498 | ` */` |
|  100102 | 4499 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 4500 |  |
|  100104 | 4501 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|      62 | 4502 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  100044 | 4503 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   17880 | 4504 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 4505 | `	}` |
|       - | 4506 | `	/* Assume public by default */` |
|   82166 | 4507 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   50053 | 4508 |  |
|       - | 4509 | `/*` |
|       - | 4510 | ` * Compile a class constant.` |
|       - | 4511 | ` * According to the PHP language reference manual` |
|       - | 4512 | ` *  Class Constants` |
|       - | 4513 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 4514 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 4515 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 4516 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 4517 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 4518 | ` *   It's also possible for interfaces to have constants.` |
|       - | 4519 | ` * Symisc eXtension.` |
|       - | 4520 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 4521 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4522 | ` *  Example:` |
|       - | 4523 | ` *   class Test{` |
|       - | 4524 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4525 | ` *   };` |
|       - | 4526 | ` *   var_dump(TEST::MyConst);` |
|       - | 4527 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4528 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4529 | ` */` |
|      10 | 4530 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4531 |  |
|      12 | 4532 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4533 | `	SySet *pInstrContainer;` |
|       - | 4534 | `	ph7_class_attr *pCons;` |
|       - | 4535 | `	SyString *pName;` |
|       - | 4536 | `	sxi32 rc;` |
|       - | 4537 | `	/* Extract visibility level */` |
|      12 | 4538 | `	iProtection = GetProtectionLevel(iProtection);` |
|      12 | 4539 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       5 | 4540 | `loop:` |
|       - | 4541 | `	/* Mark as constant */` |
|      12 | 4542 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      12 | 4543 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4544 | `		/* Invalid constant name */` |
|     ! 0 | 4545 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 4546 | `		if( rc == SXERR_ABORT ){` |
|       - | 4547 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4548 | `			return SXERR_ABORT;` |
|       - | 4549 | `		}` |
|     ! 0 | 4550 | `		goto Synchronize;` |
|       - | 4551 | `	}` |
|       - | 4552 | `	/* Peek constant name */` |
|      12 | 4553 | `	pName = &pGen->pIn->sData;` |
|       - | 4554 | `	/* Make sure the constant name isn't reserved */` |
|      12 | 4555 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 4556 | `		/* Reserved constant name */` |
|     ! 0 | 4557 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 4558 | `		if( rc == SXERR_ABORT ){` |
|       - | 4559 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4560 | `			return SXERR_ABORT;` |
|       - | 4561 | `		}` |
|     ! 0 | 4562 | `		goto Synchronize;` |
|       - | 4563 | `	}` |
|       - | 4564 | `	/* Advance the stream cursor */` |
|      12 | 4565 | `	pGen->pIn++;` |
|      12 | 4566 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 4567 | `		/* Invalid declaration */` |
|     ! 0 | 4568 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 4569 | `		if( rc == SXERR_ABORT ){` |
|       - | 4570 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4571 | `			return SXERR_ABORT;` |
|       - | 4572 | `		}` |
|     ! 0 | 4573 | `		goto Synchronize;` |
|       - | 4574 | `	}` |
|      12 | 4575 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 4576 | `	/* Allocate a new class attribute */` |
|      12 | 4577 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      12 | 4578 | `	if( pCons == 0 ){` |
|     ! 0 | 4579 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4580 | `		return SXERR_ABORT;` |
|       - | 4581 | `	}` |
|       - | 4582 | `	/* Swap bytecode container */` |
|      12 | 4583 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      12 | 4584 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 4585 | `	/* Compile constant value.` |
|       - | 4586 | `	 */` |
|      12 | 4587 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      12 | 4588 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 4589 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 4590 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4591 | `			return SXERR_ABORT;` |
|       - | 4592 | `		}` |
|       1 | 4593 | `	}` |
|       - | 4594 | `	/* Emit the done instruction */` |
|      12 | 4595 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      12 | 4596 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      12 | 4597 | `	if( rc == SXERR_ABORT ){` |
|       - | 4598 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4599 | `		return SXERR_ABORT;` |
|       - | 4600 | `	}` |
|       - | 4601 | `	/* All done,install the constant */` |
|      12 | 4602 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      12 | 4603 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4604 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4605 | `		return SXERR_ABORT;` |
|       - | 4606 | `	}` |
|      12 | 4607 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4608 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 4609 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4610 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 4611 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4612 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4613 | `				pTok--;` |
|     ! 0 | 4614 | `			}` |
|     ! 0 | 4615 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4616 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 4617 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4618 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4619 | `				return SXERR_ABORT;` |
|       - | 4620 | `			}` |
|     ! 0 | 4621 | `		}else{` |
|     ! 0 | 4622 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 4623 | `				goto loop;` |
|       - | 4624 | `			}` |
|       - | 4625 | `		}` |
|     ! 0 | 4626 | `	}` |
|      12 | 4627 | `	return SXRET_OK;` |
|     ! 0 | 4628 | `Synchronize:` |
|       - | 4629 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4630 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 4631 | `		pGen->pIn++;` |
|     ! 0 | 4632 | `	}` |
|     ! 0 | 4633 | `	return SXERR_CORRUPT;` |
|       7 | 4634 |  |
|       - | 4635 | `/*` |
|       - | 4636 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 4637 | ` * According to the PHP language reference manual` |
|       - | 4638 | ` *  Properties` |
|       - | 4639 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 4640 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 4641 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 4642 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 4643 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 4644 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 4645 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 4646 | ` * Symisc eXtension.` |
|       - | 4647 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 4648 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4649 | ` *  Example:` |
|       - | 4650 | ` *   class Test{` |
|       - | 4651 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4652 | ` *   };` |
|       - | 4653 | ` *   var_dump(TEST::myVar);` |
|       - | 4654 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4655 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4656 | ` */` |
|   25712 | 4657 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4658 |  |
|   25714 | 4659 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4660 | `	ph7_class_attr *pAttr;` |
|       - | 4661 | `	SyString *pName;` |
|       - | 4662 | `	sxi32 rc;` |
|       - | 4663 | `	/* Extract visibility level */` |
|   25714 | 4664 | `	iProtection = GetProtectionLevel(iProtection);` |
|   12856 | 4665 | `loop:` |
|   25714 | 4666 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   25714 | 4667 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 4668 | `		/* Invalid attribute name */` |
|     ! 0 | 4669 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 4670 | `		if( rc == SXERR_ABORT ){` |
|       - | 4671 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4672 | `			return SXERR_ABORT;` |
|       - | 4673 | `		}` |
|     ! 0 | 4674 | `		goto Synchronize;` |
|       - | 4675 | `	}` |
|       - | 4676 | `	/* Peek attribute name */` |
|   25714 | 4677 | `	pName = &pGen->pIn->sData;` |
|       - | 4678 | `	/* Advance the stream cursor */` |
|   25714 | 4679 | `	pGen->pIn++;` |
|   25714 | 4680 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 4681 | `		/* Invalid declaration */` |
|       3 | 4682 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 4683 | `		if( rc == SXERR_ABORT ){` |
|       - | 4684 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4685 | `			return SXERR_ABORT;` |
|       - | 4686 | `		}` |
|       3 | 4687 | `		goto Synchronize;` |
|       - | 4688 | `	}` |
|       - | 4689 | `	/* Allocate a new class attribute */` |
|   25712 | 4690 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   25712 | 4691 | `	if( pAttr == 0 ){` |
|     ! 0 | 4692 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 4693 | `		return SXERR_ABORT;` |
|       - | 4694 | `	}` |
|   25712 | 4695 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 4696 | `		SySet *pInstrContainer;` |
|   10362 | 4697 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 4698 | `		/* Swap bytecode container */` |
|   10362 | 4699 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   10362 | 4700 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 4701 | `		/* Compile attribute value.` |
|       - | 4702 | `		 */` |
|   10362 | 4703 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   10362 | 4704 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 4705 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 4706 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4707 | `				return SXERR_ABORT;` |
|       - | 4708 | `			}` |
|     ! 0 | 4709 | `		}` |
|       - | 4710 | `		/* Emit the done instruction */` |
|   10362 | 4711 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   10362 | 4712 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5180 | 4713 | `	}` |
|       - | 4714 | `	/* All done,install the attribute */` |
|   25712 | 4715 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   25712 | 4716 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4717 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4718 | `		return SXERR_ABORT;` |
|       - | 4719 | `	}` |
|   25712 | 4720 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4721 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 4722 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4723 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 4724 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4725 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4726 | `				pTok--;` |
|     ! 0 | 4727 | `			}` |
|     ! 0 | 4728 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4729 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4730 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4731 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4732 | `				return SXERR_ABORT;` |
|       - | 4733 | `			}` |
|     ! 0 | 4734 | `		}else{` |
|     ! 0 | 4735 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 4736 | `				goto loop;` |
|       - | 4737 | `			}` |
|       - | 4738 | `		}` |
|     ! 0 | 4739 | `	}` |
|   25712 | 4740 | `	return SXRET_OK;` |
|       1 | 4741 | `Synchronize:` |
|       - | 4742 | `	/* Synchronize with the first semi-colon */` |
|       5 | 4743 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 4744 | `		pGen->pIn++;` |
|       1 | 4745 | `	}` |
|       3 | 4746 | `	return SXERR_CORRUPT;` |
|   12858 | 4747 |  |
|       - | 4748 | `/*` |
|       - | 4749 | ` * Compile a class method.` |
|       - | 4750 | ` *` |
|       - | 4751 | ` * Refer to the official documentation for more information` |
|       - | 4752 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 4753 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 4754 | ` * overloading and many more.` |
|       - | 4755 | ` */` |
|   74380 | 4756 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 4757 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4758 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 4759 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 4760 | `	int doBody,          /* TRUE to process method body */` |
|       - | 4761 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 4762 | `	)` |
|       2 | 4763 |  |
|   74382 | 4764 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4765 | `	ph7_class_method *pMeth;` |
|       - | 4766 | `	sxi32 iFuncFlags;` |
|       - | 4767 | `	SyString *pName;` |
|       - | 4768 | `	SyToken *pEnd;` |
|       - | 4769 | `	sxi32 rc;` |
|       - | 4770 | `	/* Extract visibility level */` |
|   74382 | 4771 | `	iProtection = GetProtectionLevel(iProtection);` |
|   74382 | 4772 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   74382 | 4773 | `	iFuncFlags = 0;` |
|   74382 | 4774 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4775 | `		/* Invalid method name */` |
|     ! 0 | 4776 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4777 | `		if( rc == SXERR_ABORT ){` |
|       - | 4778 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4779 | `			return SXERR_ABORT;` |
|       - | 4780 | `		}` |
|     ! 0 | 4781 | `		goto Synchronize;` |
|       - | 4782 | `	}` |
|   74382 | 4783 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4784 | `		/* Return by reference,remember that */` |
|     ! 0 | 4785 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4786 | `		/* Jump the '&' token */` |
|     ! 0 | 4787 | `		pGen->pIn++;` |
|     ! 0 | 4788 | `	}` |
|   74382 | 4789 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID)) == 0 ){` |
|       - | 4790 | `		/* Invalid method name */` |
|     ! 0 | 4791 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4792 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4793 | `			return SXERR_ABORT;` |
|       - | 4794 | `		}` |
|     ! 0 | 4795 | `		goto Synchronize;` |
|       - | 4796 | `	}` |
|       - | 4797 | `	/* Peek method name */` |
|   74382 | 4798 | `	pName = &pGen->pIn->sData;` |
|   74382 | 4799 | `	nLine = pGen->pIn->nLine;` |
|       - | 4800 | `	/* Jump the method name */` |
|   74382 | 4801 | `	pGen->pIn++;` |
|   74382 | 4802 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 4803 | `		/* Abstract method */` |
|   20446 | 4804 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 4805 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4806 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 4807 | `				&pClass->sName,pName);` |
|     ! 0 | 4808 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4809 | `				return SXERR_ABORT;` |
|       - | 4810 | `			}` |
|     ! 0 | 4811 | `		}` |
|       - | 4812 | `		/* Assemble method signature only */` |
|   20446 | 4813 | `		doBody = FALSE;` |
|   10222 | 4814 | `	}` |
|   74382 | 4815 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4816 | `		/* Syntax error */` |
|     ! 0 | 4817 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 4818 | `		if( rc == SXERR_ABORT ){` |
|       - | 4819 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4820 | `			return SXERR_ABORT;` |
|       - | 4821 | `		}` |
|     ! 0 | 4822 | `		goto Synchronize;` |
|       - | 4823 | `	}` |
|       - | 4824 | `	/* Allocate a new class_method instance */` |
|   74382 | 4825 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   74382 | 4826 | `	if( pMeth == 0 ){` |
|     ! 0 | 4827 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4828 | `		return SXERR_ABORT;` |
|       - | 4829 | `	}` |
|       - | 4830 | `	/* Jump the left parenthesis '(' */` |
|   74382 | 4831 | `	pGen->pIn++;` |
|   74382 | 4832 | `	pEnd = 0; /* cc warning */` |
|       - | 4833 | `	/* Delimit the method signature */` |
|   74382 | 4834 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   74382 | 4835 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4836 | `		/* Syntax error */` |
|       3 | 4837 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 4838 | `		if( rc == SXERR_ABORT ){` |
|       - | 4839 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4840 | `			return SXERR_ABORT;` |
|       - | 4841 | `		}` |
|       3 | 4842 | `		goto Synchronize;` |
|       - | 4843 | `	}` |
|   74380 | 4844 | `	if( pGen->pIn < pEnd ){` |
|       - | 4845 | `		/* Collect method arguments */` |
|   12832 | 4846 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   12832 | 4847 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4848 | `			return SXERR_ABORT;` |
|       - | 4849 | `		}` |
|    6415 | 4850 | `	}` |
|       - | 4851 | `	/* Point beyond method signature */` |
|   74380 | 4852 | `	pGen->pIn = &pEnd[1];` |
|   74380 | 4853 | `	if( doBody ){` |
|       - | 4854 | `		/* Compile method body */` |
|   53936 | 4855 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   53936 | 4856 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4857 | `			return SXERR_ABORT;` |
|       - | 4858 | `		}` |
|   26969 | 4859 | `	}else{` |
|       - | 4860 | `		/* Only method signature is allowed */` |
|   20446 | 4861 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 4862 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4863 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 4864 | `				if( rc == SXERR_ABORT ){` |
|       - | 4865 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4866 | `					return SXERR_ABORT;` |
|       - | 4867 | `				}` |
|     ! 0 | 4868 | `				return SXERR_CORRUPT;` |
|       - | 4869 | `			}` |
|       - | 4870 | `	}` |
|       - | 4871 | `	/* All done,install the method */` |
|   74380 | 4872 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   74380 | 4873 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4874 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4875 | `		return SXERR_ABORT;` |
|       - | 4876 | `	}` |
|   74380 | 4877 | `	return SXRET_OK;` |
|       1 | 4878 | `Synchronize:` |
|       - | 4879 | `	/* Synchronize with the first semi-colon */` |
|       7 | 4880 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 4881 | `		pGen->pIn++;` |
|       1 | 4882 | `	}` |
|       3 | 4883 | `	return SXERR_CORRUPT;` |
|   37192 | 4884 |  |
|       - | 4885 | `/*` |
|       - | 4886 | ` * Compile an object interface.` |
|       - | 4887 | ` *  According to the PHP language reference manual` |
|       - | 4888 | ` *   Object Interfaces:` |
|       - | 4889 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 4890 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 4891 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 4892 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 4893 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 4894 | ` */` |
|    7682 | 4895 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 4896 |  |
|    7684 | 4897 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4898 | `	ph7_class *pClass,*pBase;` |
|       - | 4899 | `	SyToken *pEnd,*pTmp;` |
|       - | 4900 | `	SyString *pName;` |
|       - | 4901 | `	sxi32 nKwrd;` |
|       - | 4902 | `	sxi32 rc;` |
|       - | 4903 | `	/* Jump the 'interface' keyword */` |
|    7684 | 4904 | `	pGen->pIn++;` |
|       - | 4905 | `	/* Extract interface name */` |
|    7684 | 4906 | `	pName = &pGen->pIn->sData;` |
|       - | 4907 | `	/* Advance the stream cursor */` |
|    7684 | 4908 | `	pGen->pIn++;` |
|       - | 4909 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 4910 | `		SyBlob sFQN;` |
|       - | 4911 | `		SyString sFQNStr;` |
|    7684 | 4912 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    7684 | 4913 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    7684 | 4914 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    7684 | 4915 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    7684 | 4916 | `		SyBlobRelease(&sFQN);` |
|       - | 4917 | `	}` |
|    7684 | 4918 | `	if( pClass == 0 ){` |
|     ! 0 | 4919 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4920 | `		return SXERR_ABORT;` |
|       - | 4921 | `	}` |
|       - | 4922 | `	/* Mark as an interface */` |
|    7684 | 4923 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 4924 | `	/* Assume no base class is given */` |
|    7684 | 4925 | `	pBase = 0;` |
|    7684 | 4926 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 4927 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 4928 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 4929 | `			SyString *pBaseName;` |
|       - | 4930 | `			/* Extract base interface */` |
|       3 | 4931 | `			pGen->pIn++;` |
|       3 | 4932 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4933 | `				/* Syntax error */` |
|     ! 0 | 4934 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4935 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 4936 | `					pName);` |
|     ! 0 | 4937 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4938 | `				if( rc == SXERR_ABORT ){` |
|       - | 4939 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4940 | `					return SXERR_ABORT;` |
|       - | 4941 | `				}` |
|     ! 0 | 4942 | `				return SXRET_OK;` |
|       - | 4943 | `			}` |
|       3 | 4944 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 4945 | `			{` |
|       - | 4946 | `				SyBlob sResolved;` |
|       3 | 4947 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 4948 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 | 4949 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 4950 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 4951 | `				SyBlobRelease(&sResolved);` |
|       - | 4952 | `			}` |
|       - | 4953 | `			/* Only interfaces is allowed */` |
|       3 | 4954 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 4955 | `				pBase = pBase->pNextName;` |
|     ! 0 | 4956 | `			}` |
|       3 | 4957 | `			if( pBase == 0 ){` |
|       - | 4958 | `				/* Inexistant interface */` |
|     ! 0 | 4959 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 4960 | `				if( rc == SXERR_ABORT ){` |
|       - | 4961 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4962 | `					return SXERR_ABORT;` |
|       - | 4963 | `				}` |
|     ! 0 | 4964 | `			}` |
|       - | 4965 | `			/* Advance the stream cursor */` |
|       3 | 4966 | `			pGen->pIn++;` |
|       1 | 4967 | `		}` |
|       1 | 4968 | `	}` |
|    7684 | 4969 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 4970 | `		/* Syntax error */` |
|     ! 0 | 4971 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 4972 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4973 | `		if( rc == SXERR_ABORT ){` |
|       - | 4974 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4975 | `			return SXERR_ABORT;` |
|       - | 4976 | `		}` |
|     ! 0 | 4977 | `		return SXRET_OK;` |
|       - | 4978 | `	}` |
|    7684 | 4979 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    7684 | 4980 | `	pEnd = 0; /* cc warning */` |
|       - | 4981 | `	/* Delimit the interface body */` |
|    7684 | 4982 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    7684 | 4983 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4984 | `		/* Syntax error */` |
|     ! 0 | 4985 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 4986 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4987 | `		if( rc == SXERR_ABORT ){` |
|       - | 4988 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4989 | `			return SXERR_ABORT;` |
|       - | 4990 | `		}` |
|     ! 0 | 4991 | `		return SXRET_OK;` |
|       - | 4992 | `	}` |
|       - | 4993 | `	/* Swap token stream */` |
|    7684 | 4994 | `	pTmp = pGen->pEnd;` |
|    7684 | 4995 | `	pGen->pEnd = pEnd;` |
|       - | 4996 | `	/* Start the parse process` |
|       - | 4997 | `	 * Note (According to the PHP reference manual):` |
|       - | 4998 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 4999 | `	 *  Only 'public' visibility is allowed.` |
|       - | 5000 | `	 */` |
|   14059 | 5001 | `	for(;;){` |
|       - | 5002 | `		/* Jump leading/trailing semi-colons */` |
|   48556 | 5003 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   20438 | 5004 | `			pGen->pIn++;` |
|       2 | 5005 | `		}` |
|   28120 | 5006 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5007 | `			/* End of interface body */` |
|    7684 | 5008 | `			break;` |
|       - | 5009 | `		}` |
|   20438 | 5010 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5011 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5012 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 5013 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5014 | `			if( rc == SXERR_ABORT ){` |
|       - | 5015 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5016 | `				return SXERR_ABORT;` |
|       - | 5017 | `			}` |
|     ! 0 | 5018 | `			goto done;` |
|       - | 5019 | `		}` |
|       - | 5020 | `		/* Extract the current keyword */` |
|   20438 | 5021 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   20438 | 5022 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 5023 | `			/* Emit a warning and switch to public visibility */` |
|     ! 0 | 5024 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"interface: Access type must be public");` |
|     ! 0 | 5025 | `			nKwrd = PH7_TKWRD_PUBLIC;` |
|     ! 0 | 5026 | `		}` |
|   20438 | 5027 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5028 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5029 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5030 | `			if( rc == SXERR_ABORT ){` |
|       - | 5031 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5032 | `				return SXERR_ABORT;` |
|       - | 5033 | `			}` |
|     ! 0 | 5034 | `			goto done;` |
|       - | 5035 | `		}` |
|   20438 | 5036 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 5037 | `			/* Advance the stream cursor */` |
|   20434 | 5038 | `			pGen->pIn++;` |
|   20434 | 5039 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5040 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5041 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5042 | `				if( rc == SXERR_ABORT ){` |
|       - | 5043 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5044 | `					return SXERR_ABORT;` |
|       - | 5045 | `				}` |
|     ! 0 | 5046 | `				goto done;` |
|       - | 5047 | `			}` |
|   20434 | 5048 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   20434 | 5049 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5050 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5051 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5052 | `				if( rc == SXERR_ABORT ){` |
|       - | 5053 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5054 | `					return SXERR_ABORT;` |
|       - | 5055 | `				}` |
|     ! 0 | 5056 | `				goto done;` |
|       - | 5057 | `			}` |
|   10216 | 5058 | `		}` |
|   20438 | 5059 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5060 | `			/* Parse constant */` |
|       3 | 5061 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 5062 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5063 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5064 | `					return SXERR_ABORT;` |
|       - | 5065 | `				}` |
|     ! 0 | 5066 | `				goto done;` |
|       - | 5067 | `			}` |
|       2 | 5068 | `		}else{` |
|   20436 | 5069 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   20436 | 5070 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5071 | `				/* Static method,record that */` |
|     ! 0 | 5072 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 5073 | `				/* Advance the stream cursor */` |
|     ! 0 | 5074 | `				pGen->pIn++;` |
|     ! 0 | 5075 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 5076 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5077 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5078 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5079 | `						if( rc == SXERR_ABORT ){` |
|       - | 5080 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5081 | `							return SXERR_ABORT;` |
|       - | 5082 | `						}` |
|     ! 0 | 5083 | `						goto done;` |
|       - | 5084 | `				}` |
|     ! 0 | 5085 | `			}` |
|       - | 5086 | `			/* Process method signature (no body for interface methods) */` |
|   20436 | 5087 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   20436 | 5088 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5089 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5090 | `					return SXERR_ABORT;` |
|       - | 5091 | `				}` |
|     ! 0 | 5092 | `				goto done;` |
|       - | 5093 | `			}` |
|       - | 5094 | `		}` |
|       2 | 5095 | `	}` |
|       - | 5096 | `	/* Install the interface */` |
|    7684 | 5097 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    7684 | 5098 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 5099 | `		/* Inherit from the base interface */` |
|       3 | 5100 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 5101 | `	}` |
|    7684 | 5102 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5103 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5104 | `		return SXERR_ABORT;` |
|       - | 5105 | `	}` |
|    3841 | 5106 | `done:` |
|       - | 5107 | `	/* Point beyond the interface body */` |
|    7684 | 5108 | `	pGen->pIn  = &pEnd[1];` |
|    7684 | 5109 | `	pGen->pEnd = pTmp;` |
|    7684 | 5110 | `	return PH7_OK;` |
|    3843 | 5111 |  |
|       - | 5112 | `/*` |
|       - | 5113 | ` * Compile a user-defined class.` |
|       - | 5114 | ` * According to the PHP language reference manual` |
|       - | 5115 | ` *  class` |
|       - | 5116 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 5117 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 5118 | ` *  of the properties and methods belonging to the class.` |
|       - | 5119 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 5120 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 5121 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 5122 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 5123 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 5124 | ` *  (called "methods").` |
|       - | 5125 | ` */` |
|       - | 5126 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 5127 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 5128 | `struct TraitUseEntry {` |
|       - | 5129 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 5130 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 5131 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 5132 | `};` |
|       - | 5133 | `/*` |
|       - | 5134 | ` * Validate that methods implementing interface contracts have compatible` |
|       - | 5135 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - | 5136 | ` */` |
|   23326 | 5137 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5138 |  |
|       - | 5139 | `	ph7_class **apIface;` |
|       - | 5140 | `	sxu32 nIface,i;` |
|       - | 5141 | `	sxi32 rc;` |
|   23328 | 5142 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 | 5143 | `		return SXRET_OK;` |
|       - | 5144 | `	}` |
|   23328 | 5145 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   23328 | 5146 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   23364 | 5147 | `	for(i = 0; i < nIface; i++){` |
|      38 | 5148 | `		ph7_class *pIface = apIface[i];` |
|       - | 5149 | `		SyHashEntry *pEntry;` |
|      38 | 5150 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|     114 | 5151 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|      78 | 5152 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - | 5153 | `			ph7_class_method *pImplMeth;` |
|      78 | 5154 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - | 5155 | `			/* Find the implementing method in the class */` |
|      78 | 5156 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|      78 | 5157 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 | 5158 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - | 5159 | `			}` |
|       - | 5160 | `			/* Check visibility: interface methods must be implemented as public */` |
|      64 | 5161 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 | 5162 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5163 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 | 5164 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 | 5165 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5166 | `					return SXERR_ABORT;` |
|       - | 5167 | `				}` |
|       1 | 5168 | `			}` |
|       - | 5169 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - | 5170 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - | 5171 | `			 */` |
|       - | 5172 | `			{` |
|      64 | 5173 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|      64 | 5174 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|      64 | 5175 | `				int sigError = 0;` |
|      64 | 5176 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 | 5177 | `					sigError = 1;` |
|      63 | 5178 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - | 5179 | `					/* Extra parameters must all have default values */` |
|       5 | 5180 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - | 5181 | `					sxu32 k;` |
|       7 | 5182 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 | 5183 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 | 5184 | `							sigError = 1;` |
|       3 | 5185 | `							break;` |
|       - | 5186 | `						}` |
|       2 | 5187 | `					}` |
|       2 | 5188 | `				}` |
|      64 | 5189 | `				if( sigError ){` |
|       - | 5190 | `					SyBlob sImplSig, sIfaceSig;` |
|       - | 5191 | `					ph7_vm_func_arg *aArgs;` |
|       - | 5192 | `					sxu32 j;` |
|       5 | 5193 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 | 5194 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - | 5195 | `					/* Build implementing method signature */` |
|       5 | 5196 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 | 5197 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 | 5198 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 | 5199 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 | 5200 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5201 | `					}` |
|       - | 5202 | `					/* Build interface method signature */` |
|       5 | 5203 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 | 5204 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 | 5205 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 | 5206 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 | 5207 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5208 | `					}` |
|       7 | 5209 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5210 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 | 5211 | `						&pClass->sName,pMName,` |
|       4 | 5212 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 | 5213 | `						&pIface->sName,pMName,` |
|       4 | 5214 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 | 5215 | `					SyBlobRelease(&sImplSig);` |
|       5 | 5216 | `					SyBlobRelease(&sIfaceSig);` |
|       5 | 5217 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5218 | `						return SXERR_ABORT;` |
|       - | 5219 | `					}` |
|       2 | 5220 | `				}` |
|       - | 5221 | `			}` |
|       2 | 5222 | `		}` |
|      20 | 5223 | `	}` |
|   23328 | 5224 | `	return SXRET_OK;` |
|   11665 | 5225 |  |
|       - | 5226 | `/*` |
|       - | 5227 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - | 5228 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - | 5229 | ` */` |
|   23326 | 5230 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5231 |  |
|       - | 5232 | `	ph7_class_method *pMeth;` |
|       - | 5233 | `	SyHashEntry *pEntry;` |
|       - | 5234 | `	sxu32 nAbstract;` |
|       - | 5235 | `	SyBlob sMsg;` |
|       - | 5236 | `	sxi32 rc;` |
|       - | 5237 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   23328 | 5238 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      18 | 5239 | `		return SXRET_OK;` |
|       - | 5240 | `	}` |
|       - | 5241 | `	/* Count abstract methods */` |
|   23312 | 5242 | `	nAbstract = 0;` |
|   23312 | 5243 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  212468 | 5244 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  189158 | 5245 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  189158 | 5246 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 | 5247 | `			nAbstract++;` |
|       8 | 5248 | `		}` |
|       2 | 5249 | `	}` |
|   23312 | 5250 | `	if( nAbstract == 0 ){` |
|   23298 | 5251 | `		return SXRET_OK;` |
|       - | 5252 | `	}` |
|       - | 5253 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 | 5254 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 | 5255 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - | 5256 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 | 5257 | `		&pClass->sName,nAbstract,` |
|       7 | 5258 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 | 5259 | `		(nAbstract > 1 ? "s" : ""));` |
|       - | 5260 | `	/* Second pass: list methods with origins */` |
|       - | 5261 | `	{` |
|      15 | 5262 | `		sxu32 nListed = 0;` |
|      15 | 5263 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 | 5264 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 | 5265 | `			ph7_class *pOrigin = 0;` |
|       - | 5266 | `			SyString *pMName;` |
|      19 | 5267 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 | 5268 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 | 5269 | `				continue;` |
|       - | 5270 | `			}` |
|      17 | 5271 | `			pMName = &pMeth->sFunc.sName;` |
|      17 | 5272 | `			if( nListed > 0 ){` |
|       3 | 5273 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 | 5274 | `			}` |
|       - | 5275 | `			/* Find the origin of this abstract method.` |
|       - | 5276 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - | 5277 | `			 * inheritance chains) take precedence for interface-declared` |
|       - | 5278 | `			 * methods. Abstract class methods only win when the class` |
|       - | 5279 | `			 * itself declared the abstract method (not inherited from` |
|       - | 5280 | `			 * an interface). Trait methods are adopted into the using` |
|       - | 5281 | `			 * class's namespace.` |
|       - | 5282 | `			 */` |
|       - | 5283 | `			{` |
|       - | 5284 | `				ph7_class **apIface;` |
|       - | 5285 | `				ph7_class **apTrait;` |
|       - | 5286 | `				ph7_class *pWalk;` |
|       - | 5287 | `				sxu32 i;` |
|       - | 5288 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - | 5289 | `				 * (one that was written in the class body, not inherited from an` |
|       - | 5290 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - | 5291 | `				 */` |
|      17 | 5292 | `				if( pClass->pBase ){` |
|       9 | 5293 | `					pWalk = pClass->pBase;` |
|      17 | 5294 | `					while( pWalk ){` |
|       - | 5295 | `						ph7_class_method *pParentMeth;` |
|      11 | 5296 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 | 5297 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - | 5298 | `							/* Exclude methods that came from an interface anywhere` |
|       - | 5299 | `							 * in this class's ancestor chain.` |
|       - | 5300 | `							 */` |
|      11 | 5301 | `							int fromIface = 0;` |
|      11 | 5302 | `							ph7_class *pAnc = pWalk;` |
|      15 | 5303 | `							while( pAnc ){` |
|       - | 5304 | `								ph7_class **apPI;` |
|       - | 5305 | `								sxu32 j;` |
|      13 | 5306 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 | 5307 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 | 5308 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 | 5309 | `										fromIface = 1;` |
|       9 | 5310 | `										break;` |
|       - | 5311 | `									}` |
|     ! 0 | 5312 | `								}` |
|      13 | 5313 | `								if( fromIface ) break;` |
|       5 | 5314 | `								pAnc = pAnc->pBase;` |
|       1 | 5315 | `							}` |
|      11 | 5316 | `							if( !fromIface ){` |
|       3 | 5317 | `								pOrigin = pWalk;` |
|       3 | 5318 | `								break;` |
|       - | 5319 | `							}` |
|       4 | 5320 | `						}` |
|       9 | 5321 | `						pWalk = pWalk->pBase;` |
|       1 | 5322 | `					}` |
|       4 | 5323 | `				}` |
|       - | 5324 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - | 5325 | `				 * each interface's own parent chain for the deepest origin.` |
|       - | 5326 | `				 */` |
|      17 | 5327 | `				if( !pOrigin ){` |
|      15 | 5328 | `					pWalk = pClass;` |
|      37 | 5329 | `					while( pWalk && !pOrigin ){` |
|      23 | 5330 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 | 5331 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 | 5332 | `							ph7_class *pIface = apIface[i];` |
|      13 | 5333 | `							ph7_class *pDeepest = 0;` |
|      25 | 5334 | `							while( pIface ){` |
|      13 | 5335 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 | 5336 | `									pDeepest = pIface;` |
|       6 | 5337 | `								}` |
|      13 | 5338 | `								pIface = pIface->pBase;` |
|       1 | 5339 | `							}` |
|      13 | 5340 | `							if( pDeepest ){` |
|      13 | 5341 | `								pOrigin = pDeepest;` |
|      13 | 5342 | `								break;` |
|       - | 5343 | `							}` |
|     ! 0 | 5344 | `						}` |
|      23 | 5345 | `						pWalk = pWalk->pBase;` |
|       1 | 5346 | `					}` |
|       7 | 5347 | `				}` |
|       - | 5348 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 | 5349 | `				if( !pOrigin ){` |
|       3 | 5350 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 | 5351 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 | 5352 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 | 5353 | `							pOrigin = pClass;` |
|       3 | 5354 | `							break;` |
|       - | 5355 | `						}` |
|     ! 0 | 5356 | `					}` |
|       1 | 5357 | `				}` |
|       - | 5358 | `			}` |
|      17 | 5359 | `			if( pOrigin ){` |
|      17 | 5360 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 | 5361 | `			}else{` |
|       - | 5362 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 | 5363 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - | 5364 | `			}` |
|      17 | 5365 | `			nListed++;` |
|       1 | 5366 | `		}` |
|       - | 5367 | `	}` |
|      15 | 5368 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 | 5369 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 | 5370 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 | 5371 | `	SyBlobRelease(&sMsg);` |
|      15 | 5372 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5373 | `		return SXERR_ABORT;` |
|       - | 5374 | `	}` |
|      15 | 5375 | `	return SXRET_OK;` |
|   11665 | 5376 |  |
|   23330 | 5377 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 5378 |  |
|   23332 | 5379 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5380 | `	ph7_class *pClass,*pBase;` |
|       - | 5381 | `	SyToken *pEnd,*pTmp;` |
|       - | 5382 | `	sxi32 iProtection;` |
|       - | 5383 | `	SySet aInterfaces;` |
|       - | 5384 | `	SySet aUseEntries;` |
|       - | 5385 | `	sxi32 iAttrflags;` |
|       - | 5386 | `	SyString *pName;` |
|       - | 5387 | `	sxi32 nKwrd;` |
|       - | 5388 | `	sxi32 rc;` |
|       - | 5389 | `	/* Jump the 'class' keyword */` |
|   23332 | 5390 | `	pGen->pIn++;` |
|   23332 | 5391 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5392 | `		/* Syntax error */` |
|     ! 0 | 5393 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 5394 | `		if( rc == SXERR_ABORT ){` |
|       - | 5395 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5396 | `			return SXERR_ABORT;` |
|       - | 5397 | `		}` |
|       - | 5398 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 5399 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 5400 | `			pGen->pIn++;` |
|     ! 0 | 5401 | `		}` |
|     ! 0 | 5402 | `		return SXRET_OK;` |
|       - | 5403 | `	}` |
|       - | 5404 | `	/* Extract class name */` |
|   23332 | 5405 | `	pName = &pGen->pIn->sData;` |
|       - | 5406 | `	/* Advance the stream cursor */` |
|   23332 | 5407 | `	pGen->pIn++;` |
|       - | 5408 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5409 | `		SyBlob sFQN;` |
|       - | 5410 | `		SyString sFQNStr;` |
|   23332 | 5411 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   23332 | 5412 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   23332 | 5413 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   23332 | 5414 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   23332 | 5415 | `		SyBlobRelease(&sFQN);` |
|       - | 5416 | `	}` |
|   23332 | 5417 | `	if( pClass == 0 ){` |
|     ! 0 | 5418 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5419 | `		return SXERR_ABORT;` |
|       - | 5420 | `	}` |
|       - | 5421 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   23332 | 5422 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   23332 | 5423 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 5424 | `	/* Assume a standalone class */` |
|   23332 | 5425 | `	pBase = 0;` |
|   23332 | 5426 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5427 | `		SyString *pBaseName;` |
|   15396 | 5428 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   15396 | 5429 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   15362 | 5430 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   15362 | 5431 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5432 | `				/* Syntax error */` |
|     ! 0 | 5433 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5434 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 5435 | `					pName);` |
|     ! 0 | 5436 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5437 | `				if( rc == SXERR_ABORT ){` |
|       - | 5438 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5439 | `					return SXERR_ABORT;` |
|       - | 5440 | `				}` |
|     ! 0 | 5441 | `				return SXRET_OK;` |
|       - | 5442 | `			}` |
|       - | 5443 | `			/* Extract base class name and resolve through namespace/imports */` |
|   15362 | 5444 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5445 | `			{` |
|       - | 5446 | `				SyBlob sResolved;` |
|   15362 | 5447 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   15362 | 5448 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   23042 | 5449 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   15360 | 5450 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   15362 | 5451 | `				SyBlobRelease(&sResolved);` |
|       - | 5452 | `			}` |
|       - | 5453 | `			/* Interfaces are not allowed */` |
|   15362 | 5454 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 5455 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5456 | `			}` |
|   15362 | 5457 | `			if( pBase == 0 ){` |
|       - | 5458 | `				/* Inexistant base class */` |
|     ! 0 | 5459 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 5460 | `				if( rc == SXERR_ABORT ){` |
|       - | 5461 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5462 | `					return SXERR_ABORT;` |
|       - | 5463 | `				}` |
|     ! 0 | 5464 | `			}else{` |
|   15362 | 5465 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 5466 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 5467 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 5468 | `					if( rc == SXERR_ABORT ){` |
|       - | 5469 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5470 | `						return SXERR_ABORT;` |
|       - | 5471 | `					}` |
|     ! 0 | 5472 | `				}` |
|       - | 5473 | `			}` |
|       - | 5474 | `			/* Advance the stream cursor */` |
|   15362 | 5475 | `			pGen->pIn++;` |
|    7680 | 5476 | `		}` |
|   15396 | 5477 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 5478 | `			ph7_class *pInterface;` |
|       - | 5479 | `			SyString *pIntName;` |
|       - | 5480 | `			/* Interface implementation */` |
|      38 | 5481 | `			pGen->pIn++; /* Advance the stream cursor */` |
|      18 | 5482 | `			for(;;){` |
|      38 | 5483 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5484 | `					/* Syntax error */` |
|     ! 0 | 5485 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5486 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 5487 | `						pName);` |
|     ! 0 | 5488 | `					if( rc == SXERR_ABORT ){` |
|       - | 5489 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5490 | `						return SXERR_ABORT;` |
|       - | 5491 | `					}` |
|     ! 0 | 5492 | `					break;` |
|       - | 5493 | `				}` |
|       - | 5494 | `				/* Extract interface name and resolve through namespace/imports */` |
|      38 | 5495 | `				pIntName = &pGen->pIn->sData;` |
|       - | 5496 | `				{` |
|       - | 5497 | `					SyBlob sResolved;` |
|      38 | 5498 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      38 | 5499 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|      74 | 5500 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|      36 | 5501 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      38 | 5502 | `					SyBlobRelease(&sResolved);` |
|       - | 5503 | `				}` |
|       - | 5504 | `				/* Only interfaces are allowed */` |
|      38 | 5505 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5506 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 5507 | `				}` |
|      38 | 5508 | `				if( pInterface == 0 ){` |
|       - | 5509 | `					/* Inexistant interface */` |
|     ! 0 | 5510 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 5511 | `					if( rc == SXERR_ABORT ){` |
|       - | 5512 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5513 | `						return SXERR_ABORT;` |
|       - | 5514 | `					}` |
|     ! 0 | 5515 | `				}else{` |
|       - | 5516 | `					/* Register interface */` |
|      38 | 5517 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 5518 | `				}` |
|       - | 5519 | `				/* Advance the stream cursor */` |
|      38 | 5520 | `				pGen->pIn++;` |
|      38 | 5521 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      20 | 5522 | `					break;` |
|       - | 5523 | `				}` |
|     ! 0 | 5524 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 5525 | `			}` |
|      18 | 5526 | `		}` |
|    7697 | 5527 | `	}` |
|   23332 | 5528 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5529 | `		/* Syntax error */` |
|     ! 0 | 5530 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 5531 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5532 | `		if( rc == SXERR_ABORT ){` |
|       - | 5533 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5534 | `			return SXERR_ABORT;` |
|       - | 5535 | `		}` |
|     ! 0 | 5536 | `		return SXRET_OK;` |
|       - | 5537 | `	}` |
|   23332 | 5538 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   23332 | 5539 | `	pEnd = 0; /* cc warning */` |
|       - | 5540 | `	/* Delimit the class body */` |
|   23332 | 5541 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   23332 | 5542 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5543 | `		/* Syntax error */` |
|     ! 0 | 5544 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 5545 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5546 | `		if( rc == SXERR_ABORT ){` |
|       - | 5547 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5548 | `			return SXERR_ABORT;` |
|       - | 5549 | `		}` |
|     ! 0 | 5550 | `		return SXRET_OK;` |
|       - | 5551 | `	}` |
|       - | 5552 | `	/* Swap token stream */` |
|   23332 | 5553 | `	pTmp = pGen->pEnd;` |
|   23332 | 5554 | `	pGen->pEnd = pEnd;` |
|       - | 5555 | `	/* Set the inherited flags */` |
|   23332 | 5556 | `	pClass->iFlags = iFlags;` |
|       - | 5557 | `	/* Start the parse process */` |
|   38617 | 5558 | `	for(;;){` |
|       - | 5559 | `		/* Jump leading/trailing semi-colons */` |
|  128712 | 5560 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   25746 | 5561 | `			pGen->pIn++;` |
|       2 | 5562 | `		}` |
|  102968 | 5563 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5564 | `			/* End of class body */` |
|   23328 | 5565 | `			break;` |
|       - | 5566 | `		}` |
|   79642 | 5567 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5568 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5569 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5570 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5571 | `			if( rc == SXERR_ABORT ){` |
|       - | 5572 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5573 | `				return SXERR_ABORT;` |
|       - | 5574 | `			}` |
|     ! 0 | 5575 | `			goto done;` |
|       - | 5576 | `		}` |
|       - | 5577 | `		/* Assume public visibility */` |
|   79642 | 5578 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   79642 | 5579 | `		iAttrflags = 0;` |
|   79642 | 5580 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 5581 | `			/* Extract the current keyword */` |
|   79642 | 5582 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   79642 | 5583 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5584 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 5585 | `				TraitUseEntry sUse;` |
|      41 | 5586 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      41 | 5587 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      41 | 5588 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      28 | 5589 | `				for(;;){` |
|       - | 5590 | `					ph7_class *pTrait;` |
|       - | 5591 | `					SyString *pTraitName;` |
|      49 | 5592 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5593 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5594 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 5595 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5596 | `							return SXERR_ABORT;` |
|       - | 5597 | `						}` |
|     ! 0 | 5598 | `						break;` |
|       - | 5599 | `					}` |
|      49 | 5600 | `					pTraitName = &pGen->pIn->sData;` |
|       - | 5601 | `					/* Resolve trait name through namespace/imports */ {` |
|       - | 5602 | `						SyBlob sResolved;` |
|      49 | 5603 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      49 | 5604 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      97 | 5605 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      48 | 5606 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      49 | 5607 | `						SyBlobRelease(&sResolved);` |
|       - | 5608 | `					}` |
|       - | 5609 | `					/* Only traits are allowed */` |
|      49 | 5610 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5611 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 5612 | `					}` |
|      49 | 5613 | `					if( pTrait == 0 ){` |
|     ! 0 | 5614 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5615 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 | 5616 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5617 | `							return SXERR_ABORT;` |
|       - | 5618 | `						}` |
|     ! 0 | 5619 | `					}else{` |
|      49 | 5620 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 5621 | `					}` |
|      49 | 5622 | `					pGen->pIn++; /* Advance past trait name */` |
|      49 | 5623 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      21 | 5624 | `						break;` |
|       - | 5625 | `					}` |
|       9 | 5626 | `					pGen->pIn++; /* Jump the comma */` |
|       1 | 5627 | `				}` |
|       - | 5628 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      41 | 5629 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 5630 | `					SyToken *pBlock;` |
|       9 | 5631 | `					pGen->pIn++; /* Jump '{' */` |
|       9 | 5632 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 | 5633 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 | 5634 | `					sUse.pResolvEnd = pBlock;` |
|       9 | 5635 | `					if( pBlock < pGen->pEnd ){` |
|       9 | 5636 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 | 5637 | `					}else{` |
|     ! 0 | 5638 | `						pGen->pIn = pGen->pEnd;` |
|       - | 5639 | `					}` |
|       4 | 5640 | `				}` |
|      41 | 5641 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 5642 | `				/* The semicolon will be consumed by the outer loop */` |
|      41 | 5643 | `				continue;` |
|       - | 5644 | `			}` |
|   79602 | 5645 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|   76958 | 5646 | `				iProtection = nKwrd;` |
|   76958 | 5647 | `				pGen->pIn++; /* Jump the visibility token */` |
|   76958 | 5648 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5649 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5650 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5651 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5652 | `					if( rc == SXERR_ABORT ){` |
|       - | 5653 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5654 | `						return SXERR_ABORT;` |
|       - | 5655 | `					}` |
|     ! 0 | 5656 | `					goto done;` |
|       - | 5657 | `				}` |
|   76958 | 5658 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5659 | `					/* Attribute declaration */` |
|   25692 | 5660 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   25692 | 5661 | `					if( rc != SXRET_OK ){` |
|       3 | 5662 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5663 | `							return SXERR_ABORT;` |
|       - | 5664 | `						}` |
|       3 | 5665 | `						goto done;` |
|       - | 5666 | `					}` |
|   25690 | 5667 | `					continue;` |
|       - | 5668 | `				}` |
|       - | 5669 | `				/* Extract the keyword */` |
|   51268 | 5670 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   25633 | 5671 | `			}` |
|   53912 | 5672 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5673 | `				/* Process constant declaration */` |
|      10 | 5674 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 | 5675 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5676 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5677 | `						return SXERR_ABORT;` |
|       - | 5678 | `					}` |
|     ! 0 | 5679 | `					goto done;` |
|       - | 5680 | `				}` |
|       6 | 5681 | `			}else{` |
|   53904 | 5682 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5683 | `					/* Static method or attribute,record that */` |
|      23 | 5684 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      23 | 5685 | `					pGen->pIn++; /* Jump the static keyword */` |
|      23 | 5686 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5687 | `						/* Extract the keyword */` |
|      19 | 5688 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      19 | 5689 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 5690 | `							iProtection = nKwrd;` |
|     ! 0 | 5691 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 5692 | `						}` |
|       9 | 5693 | `					}` |
|      23 | 5694 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5695 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5696 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 5697 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5698 | `						if( rc == SXERR_ABORT ){` |
|       - | 5699 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5700 | `							return SXERR_ABORT;` |
|       - | 5701 | `						}` |
|     ! 0 | 5702 | `						goto done;` |
|       - | 5703 | `					}` |
|      23 | 5704 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5705 | `						/* Attribute declaration */` |
|       5 | 5706 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 5707 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 5708 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5709 | `								return SXERR_ABORT;` |
|       - | 5710 | `							}` |
|     ! 0 | 5711 | `							goto done;` |
|       - | 5712 | `						}` |
|       5 | 5713 | `						continue;` |
|       - | 5714 | `					}` |
|       - | 5715 | `					/* Extract the keyword */` |
|      19 | 5716 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   53891 | 5717 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 5718 | `					/* Abstract method,record that */` |
|       8 | 5719 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 5720 | `					/* Mark the whole class as abstract */` |
|       8 | 5721 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 5722 | `					/* Advance the stream cursor */` |
|       8 | 5723 | `					pGen->pIn++;` |
|       8 | 5724 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       8 | 5725 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       8 | 5726 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 | 5727 | `							iProtection = nKwrd;` |
|       6 | 5728 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 5729 | `						}` |
|       3 | 5730 | `					}` |
|       8 | 5731 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       6 | 5732 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5733 | `							/* Static method */` |
|     ! 0 | 5734 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5735 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5736 | `					}` |
|       8 | 5737 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       6 | 5738 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5739 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5740 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 5741 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5742 | `							if( rc == SXERR_ABORT ){` |
|       - | 5743 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5744 | `								return SXERR_ABORT;` |
|       - | 5745 | `							}` |
|     ! 0 | 5746 | `							goto done;` |
|       - | 5747 | `					}` |
|       8 | 5748 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   53879 | 5749 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 5750 | `					/* final method ,record that */` |
|       5 | 5751 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 5752 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 5753 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5754 | `						/* Extract the keyword */` |
|       5 | 5755 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 5756 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 5757 | `							iProtection = nKwrd;` |
|       5 | 5758 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 5759 | `						}` |
|       2 | 5760 | `					}` |
|       5 | 5761 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 5762 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5763 | `							/* Static method */` |
|     ! 0 | 5764 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5765 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5766 | `					}` |
|       5 | 5767 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 5768 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5769 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5770 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 5771 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5772 | `							if( rc == SXERR_ABORT ){` |
|       - | 5773 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5774 | `								return SXERR_ABORT;` |
|       - | 5775 | `							}` |
|     ! 0 | 5776 | `							goto done;` |
|       - | 5777 | `					}` |
|       5 | 5778 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 5779 | `				}` |
|   53900 | 5780 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 5781 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5782 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 5783 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5784 | `						if( rc == SXERR_ABORT ){` |
|       - | 5785 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5786 | `							return SXERR_ABORT;` |
|       - | 5787 | `						}` |
|     ! 0 | 5788 | `						goto done;` |
|       - | 5789 | `				}` |
|   53900 | 5790 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 5791 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 5792 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 5793 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5794 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 5795 | `						if( rc == SXERR_ABORT ){` |
|       - | 5796 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5797 | `							return SXERR_ABORT;` |
|       - | 5798 | `						}` |
|     ! 0 | 5799 | `						goto done;` |
|       - | 5800 | `					}` |
|       - | 5801 | `					/* Attribute declaration */` |
|       7 | 5802 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 5803 | `				}else{` |
|       - | 5804 | `					/* Process method declaration */` |
|   53894 | 5805 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 5806 | `				}` |
|   53900 | 5807 | `				if( rc != SXRET_OK ){` |
|       3 | 5808 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5809 | `						return SXERR_ABORT;` |
|       - | 5810 | `					}` |
|       3 | 5811 | `					goto done;` |
|       - | 5812 | `				}` |
|       - | 5813 | `			}` |
|   26954 | 5814 | `		}else{` |
|       - | 5815 | `			/* Attribute declaration */` |
|     ! 0 | 5816 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5817 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5818 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5819 | `					return SXERR_ABORT;` |
|       - | 5820 | `				}` |
|     ! 0 | 5821 | `				goto done;` |
|       - | 5822 | `			}` |
|       - | 5823 | `		}` |
|       2 | 5824 | `	}` |
|       - | 5825 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 5826 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 5827 | `	 */` |
|       - | 5828 | `	{` |
|       - | 5829 | `		TraitUseEntry *apUse;` |
|       - | 5830 | `		sxu32 nU;` |
|   23328 | 5831 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   23368 | 5832 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      41 | 5833 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      41 | 5834 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      41 | 5835 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      41 | 5836 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 5837 | `			sxu32 nT;` |
|      41 | 5838 | `			if( !hasResolution ){` |
|       - | 5839 | `				/* No conflict resolution block: use standard trait application */` |
|      71 | 5840 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      39 | 5841 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      39 | 5842 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 5843 | `						break;` |
|       - | 5844 | `					}` |
|      20 | 5845 | `				}` |
|      17 | 5846 | `			}else{` |
|       - | 5847 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 5848 | `				 * then use the block to resolve method conflicts.` |
|       - | 5849 | `				 */` |
|       - | 5850 | `				SyToken *pR;` |
|      19 | 5851 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 | 5852 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 5853 | `					ph7_class_attr *pAR;` |
|       - | 5854 | `					SyHashEntry *pER;` |
|       - | 5855 | `					SyString *pNR;` |
|      11 | 5856 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 | 5857 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 5858 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 5859 | `						pNR = &pAR->sName;` |
|     ! 0 | 5860 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 5861 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 5862 | `						}` |
|     ! 0 | 5863 | `					}` |
|      11 | 5864 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 | 5865 | `				}` |
|       - | 5866 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 | 5867 | `				pR = pUse->pResolvStart;` |
|      21 | 5868 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 5869 | `					SyString sTrait,sMethod;` |
|       - | 5870 | `					ph7_class *pSrcTrait;` |
|       - | 5871 | `					ph7_class_method *pMeth;` |
|       - | 5872 | `					sxi32 nRKwrd;` |
|      33 | 5873 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 5874 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 5875 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 5876 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 5877 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 5878 | `					sMethod = pR->sData;` |
|      13 | 5879 | `					pR++;` |
|      13 | 5880 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 5881 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 5882 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 5883 | `							sTrait = sMethod;` |
|       7 | 5884 | `							pR++;` |
|       7 | 5885 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 5886 | `							sMethod = pR->sData;` |
|       7 | 5887 | `							pR++;` |
|       3 | 5888 | `						}` |
|       3 | 5889 | `					}` |
|      13 | 5890 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5891 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 5892 | `						continue;` |
|       - | 5893 | `					}` |
|      13 | 5894 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 5895 | `					pR++;` |
|      13 | 5896 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 | 5897 | `						pSrcTrait = 0;` |
|       7 | 5898 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 | 5899 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 | 5900 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 | 5901 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 | 5902 | `								pSrcTrait = apTrait[nT];` |
|       5 | 5903 | `								break;` |
|       - | 5904 | `							}` |
|       2 | 5905 | `						}` |
|       5 | 5906 | `						if( pSrcTrait ){` |
|       5 | 5907 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 | 5908 | `							if( pMeth ){` |
|       5 | 5909 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 | 5910 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 | 5911 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 | 5912 | `								}` |
|       2 | 5913 | `							}` |
|       2 | 5914 | `						}` |
|       2 | 5915 | `					}` |
|      29 | 5916 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 5917 | `				}` |
|       - | 5918 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 | 5919 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 5920 | `					ph7_class_method *pMR;` |
|       - | 5921 | `					SyHashEntry *pER;` |
|       - | 5922 | `					SyString *pNR;` |
|      11 | 5923 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 | 5924 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 | 5925 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 | 5926 | `						pNR = &pMR->sFunc.sName;` |
|      19 | 5927 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 | 5928 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 | 5929 | `						}` |
|       1 | 5930 | `					}` |
|       6 | 5931 | `				}` |
|       - | 5932 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 | 5933 | `				pR = pUse->pResolvStart;` |
|      21 | 5934 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 5935 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 5936 | `					ph7_class *pSrcTrait;` |
|       - | 5937 | `					ph7_class_method *pMeth;` |
|      21 | 5938 | `					int hasQual = 0;` |
|       - | 5939 | `					sxi32 nRKwrd;` |
|      33 | 5940 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 5941 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 5942 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 5943 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 5944 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 | 5945 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 5946 | `					sMethod = pR->sData;` |
|      13 | 5947 | `					pR++;` |
|      13 | 5948 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 5949 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 5950 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 5951 | `							sTrait = sMethod;` |
|       7 | 5952 | `							hasQual = 1;` |
|       7 | 5953 | `							pR++;` |
|       7 | 5954 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 5955 | `							sMethod = pR->sData;` |
|       7 | 5956 | `							pR++;` |
|       3 | 5957 | `						}` |
|       3 | 5958 | `					}` |
|      13 | 5959 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5960 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 5961 | `						continue;` |
|       - | 5962 | `					}` |
|      13 | 5963 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 5964 | `					pR++;` |
|      13 | 5965 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 | 5966 | `						sxi32 iNewVis = -1;` |
|       9 | 5967 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 | 5968 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 | 5969 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 | 5970 | `								iNewVis = nAK;` |
|       7 | 5971 | `								pR++;` |
|       3 | 5972 | `							}` |
|       3 | 5973 | `						}` |
|       9 | 5974 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 | 5975 | `							sAlias = pR->sData;` |
|       7 | 5976 | `							pR++;` |
|       3 | 5977 | `						}` |
|       9 | 5978 | `						pMeth = 0;` |
|       9 | 5979 | `						if( hasQual ){` |
|       3 | 5980 | `							pSrcTrait = 0;` |
|       5 | 5981 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 | 5982 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 | 5983 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 | 5984 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 | 5985 | `									pSrcTrait = apTrait[nT];` |
|       3 | 5986 | `									break;` |
|       - | 5987 | `								}` |
|       2 | 5988 | `							}` |
|       3 | 5989 | `							if( pSrcTrait ){` |
|       3 | 5990 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 | 5991 | `							}` |
|       2 | 5992 | `						}else{` |
|       7 | 5993 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 5994 | `						}` |
|       9 | 5995 | `						if( pMeth ){` |
|       9 | 5996 | `							if( sAlias.nByte > 0 ){` |
|       - | 5997 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 5998 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 5999 | `								 */` |
|       - | 6000 | `								ph7_class_method *pAlias;` |
|       - | 6001 | `								char *zAliasDup;` |
|       7 | 6002 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 | 6003 | `								if( pAlias ){` |
|       7 | 6004 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 | 6005 | `									if( iNewVis >= 0 ){` |
|       5 | 6006 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6007 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6008 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 | 6009 | `									}` |
|       7 | 6010 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 | 6011 | `									if( zAliasDup ){` |
|       7 | 6012 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 | 6013 | `									}` |
|       4 | 6014 | `								}` |
|       6 | 6015 | `							}else if( iNewVis >= 0 ){` |
|       - | 6016 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 6017 | `								ph7_class_method *pCopy;` |
|       3 | 6018 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 6019 | `								if( pCopy ){` |
|       3 | 6020 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 6021 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 6022 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6023 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6024 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 6025 | `									/* Replace the method in the class hash */` |
|       3 | 6026 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 6027 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 6028 | `								}` |
|       1 | 6029 | `							}` |
|       4 | 6030 | `						}` |
|       4 | 6031 | `						SXUNUSED(hasQual);` |
|       4 | 6032 | `					}` |
|      17 | 6033 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6034 | `				}` |
|       - | 6035 | `			}` |
|      41 | 6036 | `			SySetRelease(&pUse->aTraits);` |
|      21 | 6037 | `		}` |
|       - | 6038 | `	}` |
|       - | 6039 | `	/* Install the class */` |
|   23328 | 6040 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   23328 | 6041 | `	if( rc == SXRET_OK ){` |
|       - | 6042 | `		ph7_class **apInterface;` |
|       - | 6043 | `		sxu32 n;` |
|   23328 | 6044 | `		if( pBase ){` |
|       - | 6045 | `			/* Inherit from base class and mark as a subclass */` |
|   15362 | 6046 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    7680 | 6047 | `		}` |
|   23328 | 6048 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   23364 | 6049 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 6050 | `			/* Implements one or more interface */` |
|      38 | 6051 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|      38 | 6052 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6053 | `				break;` |
|       - | 6054 | `			}` |
|      20 | 6055 | `		}` |
|       - | 6056 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   23328 | 6057 | `		if( rc == SXRET_OK ){` |
|   23328 | 6058 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   23328 | 6059 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6060 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6061 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6062 | `				return SXERR_ABORT;` |
|       - | 6063 | `			}` |
|   11663 | 6064 | `		}` |
|       - | 6065 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   23328 | 6066 | `		if( rc == SXRET_OK ){` |
|   23328 | 6067 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   23328 | 6068 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6069 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6070 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6071 | `				return SXERR_ABORT;` |
|       - | 6072 | `			}` |
|   11663 | 6073 | `		}` |
|   11663 | 6074 | `	}` |
|   23328 | 6075 | `	SySetRelease(&aUseEntries);` |
|   23328 | 6076 | `	SySetRelease(&aInterfaces);` |
|   23328 | 6077 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6078 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6079 | `		return SXERR_ABORT;` |
|       - | 6080 | `	}` |
|   11663 | 6081 | `done:` |
|       - | 6082 | `	/* Point beyond the class body */` |
|   23332 | 6083 | `	pGen->pIn = &pEnd[1];` |
|   23332 | 6084 | `	pGen->pEnd = pTmp;` |
|   23332 | 6085 | `	return PH7_OK;` |
|   11667 | 6086 |  |
|       - | 6087 | `/*` |
|       - | 6088 | ` * Compile a user-defined abstract class.` |
|       - | 6089 | ` *  According to the PHP language reference manual` |
|       - | 6090 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 6091 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 6092 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 6093 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 6094 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 6095 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 6096 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 6097 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 6098 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 6099 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 6100 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 6101 | ` *   could differ.` |
|       - | 6102 | ` */` |
|      14 | 6103 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 6104 |  |
|       - | 6105 | `	sxi32 rc;` |
|      16 | 6106 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      16 | 6107 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      16 | 6108 | `	return rc;` |
|       2 | 6109 |  |
|       - | 6110 | `/*` |
|       - | 6111 | ` * Compile a user-defined final class.` |
|       - | 6112 | ` *  According to the PHP language reference manual` |
|       - | 6113 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 6114 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 6115 | ` *    final then it cannot be extended.` |
|       - | 6116 | ` */` |
|       2 | 6117 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 6118 |  |
|       - | 6119 | `	sxi32 rc;` |
|       3 | 6120 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 6121 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 6122 | `	return rc;` |
|       1 | 6123 |  |
|       - | 6124 | `/*` |
|       - | 6125 | ` * Compile a user-defined trait.` |
|       - | 6126 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 6127 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 6128 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 6129 | ` */` |
|      50 | 6130 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       1 | 6131 |  |
|      51 | 6132 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6133 | `	ph7_class *pClass;` |
|       - | 6134 | `	SyToken *pEnd,*pTmp;` |
|       - | 6135 | `	sxi32 iProtection;` |
|       - | 6136 | `	sxi32 iAttrflags;` |
|       - | 6137 | `	SyString *pName;` |
|       - | 6138 | `	sxi32 nKwrd;` |
|       - | 6139 | `	sxi32 rc;` |
|       - | 6140 | `	/* Jump the 'trait' keyword */` |
|      51 | 6141 | `	pGen->pIn++;` |
|      51 | 6142 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6143 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 6144 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6145 | `			return SXERR_ABORT;` |
|       - | 6146 | `		}` |
|     ! 0 | 6147 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 6148 | `			pGen->pIn++;` |
|     ! 0 | 6149 | `		}` |
|     ! 0 | 6150 | `		return SXRET_OK;` |
|       - | 6151 | `	}` |
|       - | 6152 | `	/* Extract trait name */` |
|      51 | 6153 | `	pName = &pGen->pIn->sData;` |
|      51 | 6154 | `	pGen->pIn++;` |
|       - | 6155 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 6156 | `		SyBlob sFQN;` |
|       - | 6157 | `		SyString sFQNStr;` |
|      51 | 6158 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      51 | 6159 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      51 | 6160 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      51 | 6161 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      51 | 6162 | `		SyBlobRelease(&sFQN);` |
|       - | 6163 | `	}` |
|      51 | 6164 | `	if( pClass == 0 ){` |
|     ! 0 | 6165 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6166 | `		return SXERR_ABORT;` |
|       - | 6167 | `	}` |
|       - | 6168 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      51 | 6169 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 6170 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 6171 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6172 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6173 | `			return SXERR_ABORT;` |
|       - | 6174 | `		}` |
|     ! 0 | 6175 | `		return SXRET_OK;` |
|       - | 6176 | `	}` |
|      51 | 6177 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      51 | 6178 | `	pEnd = 0;` |
|      51 | 6179 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      51 | 6180 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 6181 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 6182 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6183 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6184 | `			return SXERR_ABORT;` |
|       - | 6185 | `		}` |
|     ! 0 | 6186 | `		return SXRET_OK;` |
|       - | 6187 | `	}` |
|       - | 6188 | `	/* Swap token stream */` |
|      51 | 6189 | `	pTmp = pGen->pEnd;` |
|      51 | 6190 | `	pGen->pEnd = pEnd;` |
|       - | 6191 | `	/* Mark as trait */` |
|      51 | 6192 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 6193 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      52 | 6194 | `	for(;;){` |
|     141 | 6195 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      21 | 6196 | `			pGen->pIn++;` |
|       1 | 6197 | `		}` |
|     121 | 6198 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      51 | 6199 | `			break;` |
|       - | 6200 | `		}` |
|      71 | 6201 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6202 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6203 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6204 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 6205 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6206 | `				return SXERR_ABORT;` |
|       - | 6207 | `			}` |
|     ! 0 | 6208 | `			goto done;` |
|       - | 6209 | `		}` |
|      71 | 6210 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      71 | 6211 | `		iAttrflags = 0;` |
|      71 | 6212 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      71 | 6213 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      71 | 6214 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 6215 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 6216 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 6217 | `				for(;;){` |
|       - | 6218 | `					ph7_class *pUsedTrait;` |
|       - | 6219 | `					SyString *pUsedName;` |
|       5 | 6220 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6221 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6222 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 6223 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6224 | `							return SXERR_ABORT;` |
|       - | 6225 | `						}` |
|     ! 0 | 6226 | `						break;` |
|       - | 6227 | `					}` |
|       5 | 6228 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 6229 | `					{` |
|       - | 6230 | `						SyBlob sResolved;` |
|       5 | 6231 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 6232 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 6233 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 6234 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 6235 | `						SyBlobRelease(&sResolved);` |
|       - | 6236 | `					}` |
|       5 | 6237 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 6238 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 6239 | `					}` |
|       5 | 6240 | `					if( pUsedTrait == 0 ){` |
|       4 | 6241 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 6242 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 6243 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6244 | `							return SXERR_ABORT;` |
|       - | 6245 | `						}` |
|       2 | 6246 | `					}else{` |
|       3 | 6247 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 6248 | `					}` |
|       5 | 6249 | `					pGen->pIn++;` |
|       5 | 6250 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 6251 | `						break;` |
|       - | 6252 | `					}` |
|     ! 0 | 6253 | `					pGen->pIn++;` |
|     ! 0 | 6254 | `				}` |
|       5 | 6255 | `				continue;` |
|       - | 6256 | `			}` |
|      67 | 6257 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      63 | 6258 | `				iProtection = nKwrd;` |
|      63 | 6259 | `				pGen->pIn++;` |
|      63 | 6260 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6261 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6262 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6263 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6264 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6265 | `						return SXERR_ABORT;` |
|       - | 6266 | `					}` |
|     ! 0 | 6267 | `					goto done;` |
|       - | 6268 | `				}` |
|      63 | 6269 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 | 6270 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 | 6271 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6272 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6273 | `							return SXERR_ABORT;` |
|       - | 6274 | `						}` |
|     ! 0 | 6275 | `						goto done;` |
|       - | 6276 | `					}` |
|      11 | 6277 | `					continue;` |
|       - | 6278 | `				}` |
|      53 | 6279 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 | 6280 | `			}` |
|      57 | 6281 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 6282 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6283 | `					"Traits cannot have constants");` |
|     ! 0 | 6284 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6285 | `					return SXERR_ABORT;` |
|       - | 6286 | `				}` |
|     ! 0 | 6287 | `				goto done;` |
|     ! 0 | 6288 | `			}else{` |
|      57 | 6289 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 6290 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 6291 | `					pGen->pIn++;` |
|       5 | 6292 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 6293 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 6294 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6295 | `							iProtection = nKwrd;` |
|     ! 0 | 6296 | `							pGen->pIn++;` |
|     ! 0 | 6297 | `						}` |
|       1 | 6298 | `					}` |
|       5 | 6299 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6300 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6301 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 6302 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6303 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6304 | `							return SXERR_ABORT;` |
|       - | 6305 | `						}` |
|     ! 0 | 6306 | `						goto done;` |
|       - | 6307 | `					}` |
|       5 | 6308 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 6309 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 6310 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6311 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6312 | `								return SXERR_ABORT;` |
|       - | 6313 | `							}` |
|     ! 0 | 6314 | `							goto done;` |
|       - | 6315 | `						}` |
|       3 | 6316 | `						continue;` |
|       - | 6317 | `					}` |
|       3 | 6318 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 | 6319 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 | 6320 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 | 6321 | `					pGen->pIn++;` |
|       5 | 6322 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 6323 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6324 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6325 | `							iProtection = nKwrd;` |
|       5 | 6326 | `							pGen->pIn++;` |
|       2 | 6327 | `						}` |
|       2 | 6328 | `					}` |
|       5 | 6329 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6330 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6331 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6332 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 6333 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6334 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6335 | `							return SXERR_ABORT;` |
|       - | 6336 | `						}` |
|     ! 0 | 6337 | `						goto done;` |
|       - | 6338 | `					}` |
|       5 | 6339 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6340 | `				}` |
|      55 | 6341 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 6342 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6343 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 6344 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6345 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6346 | `						return SXERR_ABORT;` |
|       - | 6347 | `					}` |
|     ! 0 | 6348 | `					goto done;` |
|       - | 6349 | `				}` |
|      55 | 6350 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 6351 | `					pGen->pIn++;` |
|     ! 0 | 6352 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 6353 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6354 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 6355 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6356 | `							return SXERR_ABORT;` |
|       - | 6357 | `						}` |
|     ! 0 | 6358 | `						goto done;` |
|       - | 6359 | `					}` |
|     ! 0 | 6360 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6361 | `				}else{` |
|      55 | 6362 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 6363 | `				}` |
|      55 | 6364 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6365 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6366 | `						return SXERR_ABORT;` |
|       - | 6367 | `					}` |
|     ! 0 | 6368 | `					goto done;` |
|       - | 6369 | `				}` |
|       - | 6370 | `			}` |
|      28 | 6371 | `		}else{` |
|     ! 0 | 6372 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6373 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6374 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6375 | `					return SXERR_ABORT;` |
|       - | 6376 | `				}` |
|     ! 0 | 6377 | `				goto done;` |
|       - | 6378 | `			}` |
|       - | 6379 | `		}` |
|       1 | 6380 | `	}` |
|       - | 6381 | `	/* Install the trait */` |
|      51 | 6382 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      51 | 6383 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6384 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6385 | `		return SXERR_ABORT;` |
|       - | 6386 | `	}` |
|      25 | 6387 | `done:` |
|       - | 6388 | `	/* Point beyond the trait body */` |
|      51 | 6389 | `	pGen->pIn = &pEnd[1];` |
|      51 | 6390 | `	pGen->pEnd = pTmp;` |
|      51 | 6391 | `	return PH7_OK;` |
|      26 | 6392 |  |
|       - | 6393 | `/*` |
|       - | 6394 | ` * Compile a user-defined class.` |
|       - | 6395 | ` *  According to the PHP language reference manual` |
|       - | 6396 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 6397 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 6398 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 6399 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 6400 | ` *   and functions (called "methods").` |
|       - | 6401 | ` */` |
|   23314 | 6402 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 6403 |  |
|       - | 6404 | `	sxi32 rc;` |
|   23316 | 6405 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   23316 | 6406 | `	return rc;` |
|       2 | 6407 |  |
|       - | 6408 | `/*` |
|       - | 6409 | ` * Exception handling.` |
|       - | 6410 | ` *  According to the PHP language reference manual` |
|       - | 6411 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 6412 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 6413 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 6414 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 6415 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 6416 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 6417 | ` *    (or re-thrown) within a catch block.` |
|       - | 6418 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 6419 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 6420 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 6421 | ` *    been defined with set_exception_handler().` |
|       - | 6422 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 6423 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 6424 | ` */` |
|       - | 6425 | `/*` |
|       - | 6426 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 6427 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 6428 | ` * indicates failure.` |
|       - | 6429 | ` */` |
|    7680 | 6430 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 6431 |  |
|    7682 | 6432 | `	sxi32 rc = SXRET_OK;` |
|    7682 | 6433 | `	if( pRoot->pOp ){` |
|    7678 | 6434 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    3841 | 6435 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 6436 | `			/* Unexpected expression */` |
|     ! 0 | 6437 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6438 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 6439 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 6440 | `				rc = SXERR_INVALID;` |
|     ! 0 | 6441 | `			}` |
|       2 | 6442 | `		}` |
|    3842 | 6443 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 6444 | `		/* Unexpected expression */` |
|     ! 0 | 6445 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6446 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 6447 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 6448 | `			rc = SXERR_INVALID;` |
|     ! 0 | 6449 | `		}` |
|     ! 0 | 6450 | `	}` |
|    7682 | 6451 | `	return rc;` |
|       2 | 6452 |  |
|       - | 6453 | `/*` |
|       - | 6454 | ` * Compile a 'throw' statement.` |
|       - | 6455 | ` * throw: This is how you trigger an exception.` |
|       - | 6456 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 6457 | ` */` |
|    7680 | 6458 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 6459 |  |
|    7682 | 6460 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6461 | `	GenBlock *pBlock;` |
|       - | 6462 | `	sxu32 nIdx;` |
|       - | 6463 | `	sxi32 rc;` |
|    7682 | 6464 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 6465 | `	/* Compile the expression */` |
|    7682 | 6466 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    7682 | 6467 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 6468 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 6469 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6470 | `			return SXERR_ABORT;` |
|       - | 6471 | `		}` |
|     ! 0 | 6472 | `		return SXRET_OK;` |
|       - | 6473 | `	}` |
|    7682 | 6474 | `	pBlock = pGen->pCurrent;` |
|       - | 6475 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   35762 | 6476 | `	while(pBlock->pParent){` |
|   35758 | 6477 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    7678 | 6478 | `			break;` |
|       - | 6479 | `		}` |
|       - | 6480 | `		/* Point to the parent block */` |
|   28082 | 6481 | `		pBlock = pBlock->pParent;` |
|       2 | 6482 | `	}` |
|       - | 6483 | `	/* Emit the throw instruction */` |
|    7682 | 6484 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 6485 | `	/* Emit the jump */` |
|    7682 | 6486 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    7682 | 6487 | `	return SXRET_OK;` |
|    3842 | 6488 |  |
|       - | 6489 | `/*` |
|       - | 6490 | ` * Compile a 'catch' block.` |
|       - | 6491 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 6492 | ` * an object containing the exception information.` |
|       - | 6493 | ` */` |
|      50 | 6494 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 6495 |  |
|      52 | 6496 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6497 | `	ph7_exception_block sCatch;` |
|       - | 6498 | `	SySet *pInstrContainer;` |
|       - | 6499 | `	GenBlock *pCatch;` |
|       - | 6500 | `	SyToken *pToken;` |
|       - | 6501 | `	SyString *pName;` |
|       - | 6502 | `	char *zDup;` |
|       - | 6503 | `	sxi32 rc;` |
|      52 | 6504 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 6505 | `	/* Zero the structure */` |
|      52 | 6506 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 6507 | `	/* Initialize fields */` |
|      52 | 6508 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|      75 | 6509 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ \|\|` |
|      52 | 6510 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6511 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6512 | `			pToken = pGen->pIn;` |
|     ! 0 | 6513 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6514 | `				pToken--;` |
|     ! 0 | 6515 | `			}` |
|     ! 0 | 6516 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6517 | `				"Catch: Unexpected token '%z',excpecting class name",&pToken->sData);` |
|     ! 0 | 6518 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6519 | `				return SXERR_ABORT;` |
|       - | 6520 | `			}` |
|     ! 0 | 6521 | `			return SXERR_INVALID;` |
|       - | 6522 | `	}` |
|       - | 6523 | `	/* Extract the exception class */` |
|      52 | 6524 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|       - | 6525 | `	/* Duplicate class name */` |
|      52 | 6526 | `	pName = &pGen->pIn->sData;` |
|      52 | 6527 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      52 | 6528 | `	if( zDup == 0 ){` |
|     ! 0 | 6529 | `		goto Mem;` |
|       - | 6530 | `	}` |
|      52 | 6531 | `	SyStringInitFromBuf(&sCatch.sClass,zDup,pName->nByte);` |
|      52 | 6532 | `	pGen->pIn++;` |
|      75 | 6533 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      52 | 6534 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6535 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6536 | `			pToken = pGen->pIn;` |
|     ! 0 | 6537 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6538 | `				pToken--;` |
|     ! 0 | 6539 | `			}` |
|     ! 0 | 6540 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6541 | `				"Catch: Unexpected token '%z',expecting variable name",&pToken->sData);` |
|     ! 0 | 6542 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6543 | `				return SXERR_ABORT;` |
|       - | 6544 | `			}` |
|     ! 0 | 6545 | `			return SXERR_INVALID;` |
|       - | 6546 | `	}` |
|      52 | 6547 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 6548 | `	/* Duplicate instance name */` |
|      52 | 6549 | `	pName = &pGen->pIn->sData;` |
|      52 | 6550 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      52 | 6551 | `	if( zDup == 0 ){` |
|     ! 0 | 6552 | `		goto Mem;` |
|       - | 6553 | `	}` |
|      52 | 6554 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      52 | 6555 | `	pGen->pIn++;` |
|      52 | 6556 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 6557 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 6558 | `		pToken = pGen->pIn;` |
|     ! 0 | 6559 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6560 | `			pToken--;` |
|     ! 0 | 6561 | `		}` |
|     ! 0 | 6562 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6563 | `			"Catch: Unexpected token '%z',expecting right parenthesis ')'",&pToken->sData);` |
|     ! 0 | 6564 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6565 | `			return SXERR_ABORT;` |
|       - | 6566 | `		}` |
|     ! 0 | 6567 | `		return SXERR_INVALID;` |
|       - | 6568 | `	}` |
|       - | 6569 | `	/* Compile the block */` |
|      52 | 6570 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 6571 | `	/* Create the catch block */` |
|      52 | 6572 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      52 | 6573 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6574 | `		return SXERR_ABORT;` |
|       - | 6575 | `	}` |
|       - | 6576 | `	/* Swap bytecode container */` |
|      52 | 6577 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      52 | 6578 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 6579 | `	/* Compile the block */` |
|      52 | 6580 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 6581 | `	/* Fix forward jumps now the destination is resolved  */` |
|      52 | 6582 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6583 | `	/* Emit the DONE instruction */` |
|      52 | 6584 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6585 | `	/* Leave the block */` |
|      52 | 6586 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6587 | `	/* Restore the default container */` |
|      52 | 6588 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 6589 | `	/* Install the catch block */` |
|      52 | 6590 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      52 | 6591 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6592 | `		goto Mem;` |
|       - | 6593 | `	}` |
|      52 | 6594 | `	return SXRET_OK;` |
|     ! 0 | 6595 | `Mem:` |
|     ! 0 | 6596 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6597 | `	return SXERR_ABORT;` |
|      27 | 6598 |  |
|       - | 6599 | `/*` |
|       - | 6600 | ` * Compile a 'try' block.` |
|       - | 6601 | ` * A function using an exception should be in a "try" block.` |
|       - | 6602 | ` * If the exception does not trigger, the code will continue` |
|       - | 6603 | ` * as normal. However if the exception triggers, an exception` |
|       - | 6604 | ` * is "thrown".` |
|       - | 6605 | ` */` |
|      62 | 6606 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 6607 |  |
|       - | 6608 | `	ph7_exception *pException;` |
|      64 | 6609 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6610 | `	GenBlock *pTry;` |
|       - | 6611 | `	sxu32 nJmpIdx;` |
|       - | 6612 | `	sxi32 rc;` |
|       - | 6613 | `	/* Create the exception container */` |
|      64 | 6614 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|      64 | 6615 | `	if( pException == 0 ){` |
|     ! 0 | 6616 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 6617 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6618 | `		return SXERR_ABORT;` |
|       - | 6619 | `	}` |
|       - | 6620 | `	/* Zero the structure */` |
|      64 | 6621 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 6622 | `	/* Initialize fields */` |
|      64 | 6623 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|      64 | 6624 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      64 | 6625 | `	pException->iHasFinally = 0;` |
|      64 | 6626 | `	pException->iFinallyDone = 0;` |
|      64 | 6627 | `	pException->pVm = pGen->pVm;` |
|       - | 6628 | `	/* Create the try block */` |
|      64 | 6629 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      64 | 6630 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6631 | `		return SXERR_ABORT;` |
|       - | 6632 | `	}` |
|       - | 6633 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|      64 | 6634 | `	pTry->pUserData = pException;` |
|       - | 6635 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|      64 | 6636 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 6637 | `	/* Fix the jump later when the destination is resolved */` |
|      64 | 6638 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|      64 | 6639 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 6640 | `	/* Compile the block */` |
|      64 | 6641 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      64 | 6642 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6643 | `		return SXERR_ABORT;` |
|       - | 6644 | `	}` |
|       - | 6645 | `	/* Fix forward jumps now the destination is resolved */` |
|      64 | 6646 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6647 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|      64 | 6648 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 6649 | `	/* Leave the block */` |
|      64 | 6650 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6651 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|      64 | 6652 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      60 | 6653 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 6654 | `		/* Compile one or more catch blocks */` |
|      50 | 6655 | `		for(;;){` |
|     100 | 6656 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      78 | 6657 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      27 | 6658 | `					break;` |
|       - | 6659 | `			}` |
|      52 | 6660 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|      52 | 6661 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6662 | `				return SXERR_ABORT;` |
|       - | 6663 | `			}` |
|       2 | 6664 | `		}` |
|      25 | 6665 | `	}` |
|       - | 6666 | `	/* Compile optional finally block */` |
|      64 | 6667 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      30 | 6668 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 6669 | `		SySet *pInstrContainer;` |
|       - | 6670 | `		GenBlock *pFinBlock;` |
|      27 | 6671 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 6672 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      27 | 6673 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      27 | 6674 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6675 | `			return SXERR_ABORT;` |
|       - | 6676 | `		}` |
|       - | 6677 | `		/* Swap bytecode container */` |
|      27 | 6678 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      27 | 6679 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 6680 | `		/* Compile the finally body */` |
|      27 | 6681 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      27 | 6682 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6683 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 6684 | `			return SXERR_ABORT;` |
|       - | 6685 | `		}` |
|       - | 6686 | `		/* Fix forward jumps now the destination is resolved */` |
|      27 | 6687 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6688 | `		/* Emit DONE to terminate the finally block */` |
|      27 | 6689 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6690 | `		/* Leave the block */` |
|      27 | 6691 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6692 | `		/* Restore the default container */` |
|      27 | 6693 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      27 | 6694 | `		pException->iHasFinally = 1;` |
|      13 | 6695 | `	}` |
|       - | 6696 | `	/* Must have at least one catch or finally */` |
|      64 | 6697 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       3 | 6698 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 6699 | `			"Cannot use try without catch or finally");` |
|       3 | 6700 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6701 | `			return SXERR_ABORT;` |
|       - | 6702 | `		}` |
|       1 | 6703 | `	}` |
|      64 | 6704 | `	return SXRET_OK;` |
|      33 | 6705 |  |
|       - | 6706 | `/*` |
|       - | 6707 | ` * Compile a switch block.` |
|       - | 6708 | ` *  (See block-comment below for more information)` |
|       - | 6709 | ` */` |
|      84 | 6710 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 6711 |  |
|      86 | 6712 | `	sxi32 rc = SXRET_OK;` |
|      86 | 6713 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 6714 | `		/* Unexpected token */` |
|     ! 0 | 6715 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 6716 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6717 | `			return SXERR_ABORT;` |
|       - | 6718 | `		}` |
|     ! 0 | 6719 | `		pGen->pIn++;` |
|     ! 0 | 6720 | `	}` |
|      86 | 6721 | `	pGen->pIn++;` |
|       - | 6722 | `	/* First instruction to execute in this block. */` |
|      86 | 6723 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 6724 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 6725 | `	 * or the '}' token */` |
|     151 | 6726 | `	for(;;){` |
|     304 | 6727 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6728 | `			/* No more input to process */` |
|     ! 0 | 6729 | `			break;` |
|       - | 6730 | `		}` |
|     304 | 6731 | `		rc = SXRET_OK;` |
|     304 | 6732 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      62 | 6733 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      20 | 6734 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 6735 | `					/* Unexpected token */` |
|     ! 0 | 6736 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 6737 | `						&pGen->pIn->sData);` |
|     ! 0 | 6738 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6739 | `						return SXERR_ABORT;` |
|       - | 6740 | `					}` |
|       - | 6741 | `					/* FALL THROUGH */` |
|     ! 0 | 6742 | `				}` |
|      20 | 6743 | `				rc = SXERR_EOF;` |
|      20 | 6744 | `				break;` |
|       - | 6745 | `			}` |
|      23 | 6746 | `		}else{` |
|       - | 6747 | `			sxi32 nKwrd;` |
|       - | 6748 | `			/* Extract the keyword */` |
|     244 | 6749 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     244 | 6750 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      34 | 6751 | `				break;` |
|       - | 6752 | `			}` |
|     180 | 6753 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 6754 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 6755 | `					/* Unexpected token */` |
|     ! 0 | 6756 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 6757 | `						&pGen->pIn->sData);` |
|     ! 0 | 6758 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6759 | `						return SXERR_ABORT;` |
|       - | 6760 | `					}` |
|       - | 6761 | `					/* FALL THROUGH */` |
|     ! 0 | 6762 | `				}` |
|       - | 6763 | `				/* Block compiled */` |
|       3 | 6764 | `				break;` |
|       - | 6765 | `			}` |
|       - | 6766 | `		}` |
|       - | 6767 | `		/* Compile block */` |
|     220 | 6768 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     220 | 6769 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6770 | `			return SXERR_ABORT;` |
|       - | 6771 | `		}` |
|       2 | 6772 | `	}` |
|      86 | 6773 | `	return rc;` |
|      44 | 6774 |  |
|       - | 6775 | `/*` |
|       - | 6776 | ` * Compile a case eXpression.` |
|       - | 6777 | ` *  (See block-comment below for more information)` |
|       - | 6778 | ` */` |
|      70 | 6779 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 6780 |  |
|       - | 6781 | `	SySet *pInstrContainer;` |
|       - | 6782 | `	SyToken *pEnd,*pTmp;` |
|      72 | 6783 | `	sxi32 iNest = 0;` |
|       - | 6784 | `	sxi32 rc;` |
|       - | 6785 | `	/* Delimit the expression */` |
|      72 | 6786 | `	pEnd = pGen->pIn;` |
|     150 | 6787 | `	while( pEnd < pGen->pEnd ){` |
|     150 | 6788 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 6789 | `			/* Increment nesting level */` |
|       3 | 6790 | `			iNest++;` |
|     149 | 6791 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 6792 | `			/* Decrement nesting level */` |
|       3 | 6793 | `			iNest--;` |
|     147 | 6794 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      72 | 6795 | `			break;` |
|       - | 6796 | `		}` |
|      80 | 6797 | `		pEnd++;` |
|       2 | 6798 | `	}` |
|      72 | 6799 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 6800 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 6801 | `		if( rc == SXERR_ABORT ){` |
|       - | 6802 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6803 | `			return SXERR_ABORT;` |
|       - | 6804 | `		}` |
|     ! 0 | 6805 | `	}` |
|       - | 6806 | `	/* Swap token stream */` |
|      72 | 6807 | `	pTmp = pGen->pEnd;` |
|      72 | 6808 | `	pGen->pEnd = pEnd;` |
|      72 | 6809 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      72 | 6810 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      72 | 6811 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 6812 | `	/* Emit the done instruction */` |
|      72 | 6813 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      72 | 6814 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 6815 | `	/* Update token stream */` |
|      72 | 6816 | `	pGen->pIn  = pEnd;` |
|      72 | 6817 | `	pGen->pEnd = pTmp;` |
|      72 | 6818 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6819 | `		return SXERR_ABORT;` |
|       - | 6820 | `	}` |
|      72 | 6821 | `	return SXRET_OK;` |
|      37 | 6822 |  |
|       - | 6823 | `/*` |
|       - | 6824 | ` * Compile the smart switch statement.` |
|       - | 6825 | ` * According to the PHP language reference manual` |
|       - | 6826 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 6827 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 6828 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 6829 | ` *  This is exactly what the switch statement is for.` |
|       - | 6830 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 6831 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 6832 | ` *  of the outer loop, use continue 2.` |
|       - | 6833 | ` *  Note that switch/case does loose comparision.` |
|       - | 6834 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 6835 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 6836 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 6837 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 6838 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 6839 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 6840 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 6841 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 6842 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 6843 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 6844 | ` *  list for the next case.` |
|       - | 6845 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 6846 | ` *  or floating-point numbers and strings.` |
|       - | 6847 | ` */` |
|      20 | 6848 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 6849 |  |
|       - | 6850 | `	GenBlock *pSwitchBlock;` |
|       - | 6851 | `	SyToken *pTmp,*pEnd;` |
|       - | 6852 | `	ph7_switch *pSwitch;` |
|       - | 6853 | `	sxu32 nToken;` |
|       - | 6854 | `	sxu32 nLine;` |
|       - | 6855 | `	sxi32 rc;` |
|      22 | 6856 | `	nLine = pGen->pIn->nLine;` |
|       - | 6857 | `	/* Jump the 'switch' keyword */` |
|      22 | 6858 | `	pGen->pIn++;` |
|      22 | 6859 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 6860 | `		/* Syntax error */` |
|     ! 0 | 6861 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 6862 | `		if( rc == SXERR_ABORT ){` |
|       - | 6863 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6864 | `			return SXERR_ABORT;` |
|       - | 6865 | `		}` |
|     ! 0 | 6866 | `		goto Synchronize;` |
|       - | 6867 | `	}` |
|       - | 6868 | `	/* Jump the left parenthesis '(' */` |
|      22 | 6869 | `	pGen->pIn++;` |
|      22 | 6870 | `	pEnd = 0; /* cc warning */` |
|       - | 6871 | `	/* Create the loop block */` |
|      32 | 6872 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      10 | 6873 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      22 | 6874 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6875 | `		return SXERR_ABORT;` |
|       - | 6876 | `	}` |
|       - | 6877 | `	/* Delimit the condition */` |
|      22 | 6878 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      22 | 6879 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 6880 | `		/* Empty expression */` |
|     ! 0 | 6881 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 6882 | `		if( rc == SXERR_ABORT ){` |
|       - | 6883 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6884 | `			return SXERR_ABORT;` |
|       - | 6885 | `		}` |
|     ! 0 | 6886 | `	}` |
|       - | 6887 | `	/* Swap token streams */` |
|      22 | 6888 | `	pTmp = pGen->pEnd;` |
|      22 | 6889 | `	pGen->pEnd = pEnd;` |
|       - | 6890 | `	/* Compile the expression */` |
|      22 | 6891 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      22 | 6892 | `	if( rc == SXERR_ABORT ){` |
|       - | 6893 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 6894 | `		return SXERR_ABORT;` |
|       - | 6895 | `	}` |
|       - | 6896 | `	/* Update token stream */` |
|      22 | 6897 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 6898 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6899 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 6900 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6901 | `			return SXERR_ABORT;` |
|       - | 6902 | `		}` |
|     ! 0 | 6903 | `		pGen->pIn++;` |
|     ! 0 | 6904 | `	}` |
|      22 | 6905 | `	pGen->pIn  = &pEnd[1];` |
|      22 | 6906 | `	pGen->pEnd = pTmp;` |
|      22 | 6907 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      20 | 6908 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 6909 | `			pTmp = pGen->pIn;` |
|     ! 0 | 6910 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 6911 | `				pTmp--;` |
|     ! 0 | 6912 | `			}` |
|       - | 6913 | `			/* Unexpected token */` |
|     ! 0 | 6914 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 6915 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6916 | `				return SXERR_ABORT;` |
|       - | 6917 | `			}` |
|     ! 0 | 6918 | `			goto Synchronize;` |
|       - | 6919 | `	}` |
|       - | 6920 | `	/* Set the delimiter token */` |
|      22 | 6921 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 6922 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 6923 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 6924 | `	}else{` |
|      20 | 6925 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 6926 | `	}` |
|      22 | 6927 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 6928 | `	/* Create the switch blocks container */` |
|      22 | 6929 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      22 | 6930 | `	if( pSwitch == 0 ){` |
|       - | 6931 | `		/* Abort compilation */` |
|     ! 0 | 6932 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6933 | `		return SXERR_ABORT;` |
|       - | 6934 | `	}` |
|       - | 6935 | `	/* Zero the structure */` |
|      22 | 6936 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 6937 | `	/* Initialize fields */` |
|      22 | 6938 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 6939 | `	/* Emit the switch instruction */` |
|      22 | 6940 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 6941 | `	/* Compile case blocks */` |
|      76 | 6942 | `	for(;;){` |
|       - | 6943 | `		sxu32 nKwrd;` |
|      88 | 6944 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6945 | `			/* No more input to process */` |
|     ! 0 | 6946 | `			break;` |
|       - | 6947 | `		}` |
|      88 | 6948 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6949 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 6950 | `				/* Unexpected token */` |
|     ! 0 | 6951 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 6952 | `					&pGen->pIn->sData);` |
|     ! 0 | 6953 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6954 | `					return SXERR_ABORT;` |
|       - | 6955 | `				}` |
|       - | 6956 | `				/* FALL THROUGH */` |
|     ! 0 | 6957 | `			}` |
|       - | 6958 | `			/* Block compiled */` |
|     ! 0 | 6959 | `			break;` |
|       - | 6960 | `		}` |
|       - | 6961 | `		/* Extract the keyword */` |
|      88 | 6962 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      88 | 6963 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 6964 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 6965 | `				/* Unexpected token */` |
|     ! 0 | 6966 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 6967 | `					&pGen->pIn->sData);` |
|     ! 0 | 6968 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6969 | `					return SXERR_ABORT;` |
|       - | 6970 | `				}` |
|       - | 6971 | `				/* FALL THROUGH */` |
|     ! 0 | 6972 | `			}` |
|       - | 6973 | `			/* Block compiled */` |
|       3 | 6974 | `			break;` |
|       - | 6975 | `		}` |
|      86 | 6976 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 6977 | `			/*` |
|       - | 6978 | `			 * Accroding to the PHP language reference manual` |
|       - | 6979 | `			 *  A special case is the default case. This case matches anything` |
|       - | 6980 | `			 *  that wasn't matched by the other cases.` |
|       - | 6981 | `			 */` |
|      16 | 6982 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 6983 | `				/* Default case already compiled */` |
|     ! 0 | 6984 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 6985 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6986 | `					return SXERR_ABORT;` |
|       - | 6987 | `				}` |
|     ! 0 | 6988 | `			}` |
|      16 | 6989 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 6990 | `			/* Compile the default block */` |
|      16 | 6991 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      16 | 6992 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 6993 | `				return SXERR_ABORT;` |
|      16 | 6994 | `			}else if( rc == SXERR_EOF ){` |
|      14 | 6995 | `				break;` |
|       1 | 6996 | `			}` |
|      73 | 6997 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 6998 | `			ph7_case_expr sCase;` |
|       - | 6999 | `			/* Standard case block */` |
|      72 | 7000 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 7001 | `			/* initialize the structure */` |
|      72 | 7002 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 7003 | `			/* Compile the case expression */` |
|      72 | 7004 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      72 | 7005 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7006 | `				return SXERR_ABORT;` |
|       - | 7007 | `			}` |
|       - | 7008 | `			/* Compile the case block */` |
|      72 | 7009 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 7010 | `			/* Insert in the switch container */` |
|      72 | 7011 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      72 | 7012 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7013 | `				return SXERR_ABORT;` |
|      72 | 7014 | `			}else if( rc == SXERR_EOF ){` |
|       7 | 7015 | `				break;` |
|       - | 7016 | `			}` |
|      34 | 7017 | `		}else{` |
|       - | 7018 | `			/* Unexpected token */` |
|     ! 0 | 7019 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7020 | `				&pGen->pIn->sData);` |
|     ! 0 | 7021 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7022 | `				return SXERR_ABORT;` |
|       - | 7023 | `			}` |
|     ! 0 | 7024 | `			break;` |
|       - | 7025 | `		}` |
|       2 | 7026 | `	}` |
|       - | 7027 | `	/* Fix all jumps now the destination is resolved */` |
|      22 | 7028 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      22 | 7029 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7030 | `	/* Release the loop block */` |
|      22 | 7031 | `	GenStateLeaveBlock(pGen,0);` |
|      22 | 7032 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 7033 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      22 | 7034 | `		pGen->pIn++;` |
|      10 | 7035 | `	}` |
|       - | 7036 | `	/* Statement successfully compiled */` |
|      22 | 7037 | `	return SXRET_OK;` |
|     ! 0 | 7038 | `Synchronize:` |
|       - | 7039 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 7040 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 7041 | `		pGen->pIn++;` |
|     ! 0 | 7042 | `	}` |
|     ! 0 | 7043 | `	return SXRET_OK;` |
|      12 | 7044 |  |
|       - | 7045 | `/*` |
|       - | 7046 | ` * Generate bytecode for a given expression tree.` |
|       - | 7047 | ` * If something goes wrong while generating bytecode` |
|       - | 7048 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 7049 | ` * this function takes care of generating the appropriate` |
|       - | 7050 | ` * error message.` |
|       - | 7051 | ` */` |
| 2118184 | 7052 | `static sxi32 GenStateEmitExprCode(` |
|       - | 7053 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7054 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 7055 | `	sxi32 iFlags /* Control flags */` |
|       - | 7056 | `	)` |
|       2 | 7057 |  |
|       - | 7058 | `	VmInstr *pInstr;` |
|       - | 7059 | `	sxu32 nJmpIdx;` |
| 2118186 | 7060 | `	sxi32 iP1 = 0;` |
| 2118186 | 7061 | `	sxu32 iP2 = 0;` |
| 2118186 | 7062 | `	void *p3  = 0;` |
|       - | 7063 | `	sxi32 iVmOp;` |
|       - | 7064 | `	sxi32 rc;` |
| 2118186 | 7065 | `	if( pNode->xCode ){` |
|       - | 7066 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 7067 | `		/* Compile node */` |
| 1299996 | 7068 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1299996 | 7069 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1299996 | 7070 | `		RE_SWAP_DELIMITER(pGen);` |
| 1299996 | 7071 | `		return rc;` |
|       - | 7072 | `	}` |
|  818192 | 7073 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 7074 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 7075 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 7076 | `		return SXERR_ABORT;` |
|       - | 7077 | `	}` |
|  818192 | 7078 | `	iVmOp = pNode->pOp->iVmOp;` |
|  818192 | 7079 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 7080 | `		sxu32 nJz,nJmp;` |
|       - | 7081 | `		/* Ternary operator require special handling */` |
|       - | 7082 | `		/* Phase#1: Compile the condition */` |
|    1754 | 7083 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1754 | 7084 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7085 | `			return rc;` |
|       - | 7086 | `		}` |
|    1754 | 7087 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1754 | 7088 | `		if( pNode->pLeft ){` |
|       - | 7089 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 7090 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1686 | 7091 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7092 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1686 | 7093 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1686 | 7094 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7095 | `				return rc;` |
|       - | 7096 | `			}` |
|     844 | 7097 | `		}else{` |
|       - | 7098 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 7099 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 7100 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 7101 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 7102 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7103 | `		}` |
|       - | 7104 | `		/* Phase#4: Emit the unconditional jump */` |
|    1754 | 7105 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 7106 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1754 | 7107 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1754 | 7108 | `		if( pInstr ){` |
|    1754 | 7109 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     876 | 7110 | `		}` |
|    1754 | 7111 | `		if( !pNode->pLeft ){` |
|       - | 7112 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 7113 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 7114 | `		}` |
|       - | 7115 | `		/* Phase#6: Compile the 'else' expression */` |
|    1754 | 7116 | `		if( pNode->pRight ){` |
|    1754 | 7117 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1754 | 7118 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7119 | `				return rc;` |
|       - | 7120 | `			}` |
|     876 | 7121 | `		}` |
|    1754 | 7122 | `		if( nJmp > 0 ){` |
|       - | 7123 | `			/* Phase#7: Fix the unconditional jump */` |
|    1754 | 7124 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1754 | 7125 | `			if( pInstr ){` |
|    1754 | 7126 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     876 | 7127 | `			}` |
|     876 | 7128 | `		}` |
|       - | 7129 | `		/* All done */` |
|    1754 | 7130 | `		return SXRET_OK;` |
|       - | 7131 | `	}` |
|       - | 7132 | `	/* Generate code for the left tree */` |
|  816440 | 7133 | `	if( pNode->pLeft ){` |
|  816422 | 7134 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 7135 | `			ph7_expr_node **apNode;` |
|       - | 7136 | `			sxi32 n;` |
|       - | 7137 | `			/* Recurse and generate bytecodes for function arguments */` |
|  240274 | 7138 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 7139 | `			/* Read-only load */` |
|  240274 | 7140 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  469908 | 7141 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  229636 | 7142 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  229636 | 7143 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7144 | `					return rc;` |
|       - | 7145 | `				}` |
|  114819 | 7146 | `			}` |
|       - | 7147 | `			/* Total number of given arguments */` |
|  240274 | 7148 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 7149 | `			/* Remove stale flags now */` |
|  240274 | 7150 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  120136 | 7151 | `		}` |
|  816422 | 7152 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  816422 | 7153 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7154 | `			return rc;` |
|       - | 7155 | `		}` |
|  816422 | 7156 | `		if( iVmOp == PH7_OP_CALL ){` |
|  240274 | 7157 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  240274 | 7158 | `			if( pInstr ){` |
|  240274 | 7159 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  239950 | 7160 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 7161 | `					sxu32 nQual;` |
|       - | 7162 | `					/* Prevent constant expansion */` |
|  239950 | 7163 | `					pInstr->iP1 = 0;` |
|       - | 7164 | `					/* Namespace-qualify the function name for CALL */` |
|  239950 | 7165 | `					nQual = GenStateNsQualifyName(pGen,nOrig);` |
|  239950 | 7166 | `					pInstr->iP2 = (sxi32)nQual;` |
|  239950 | 7167 | `					if( nQual != nOrig ){` |
|       - | 7168 | `						/* Name was compiler-qualified: flag CALL for host-function global fallback.` |
|       - | 7169 | `						 * p3 = (void*)1 tells the VM it's safe to strip the NS prefix` |
|       - | 7170 | `						 * and try the short name in hHostFunction. */` |
|      49 | 7171 | `						p3 = (void *)1;` |
|      26 | 7172 | `					}` |
|  120300 | 7173 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 7174 | `					/* Method call,flag that */` |
|     310 | 7175 | `					pInstr->iP2 = 1;` |
|     154 | 7176 | `				}` |
|  120138 | 7177 | `			}` |
|  696286 | 7178 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 7179 | `			ph7_expr_node **apNode;` |
|       - | 7180 | `			sxi32 n;` |
|       - | 7181 | `			/* Recurse and generate bytecodes for array index */` |
|   65400 | 7182 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  117996 | 7183 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   52598 | 7184 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   52598 | 7185 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7186 | `					return rc;` |
|       - | 7187 | `				}` |
|   26300 | 7188 | `			}` |
|   65400 | 7189 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   52598 | 7190 | `				iP1 = 1; /* Node have an index associated with it */` |
|   26298 | 7191 | `			}` |
|   65400 | 7192 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 7193 | `				/* Create an empty entry when the desired index is not found */` |
|   25854 | 7194 | `				iP2 = 1;` |
|   12928 | 7195 | `			}` |
|  543451 | 7196 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 7197 | `			/* POP the left node */` |
|      32 | 7198 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 7199 | `		}` |
|  408210 | 7200 | `	}` |
|  816440 | 7201 | `	rc = SXRET_OK;` |
|  816440 | 7202 | `	nJmpIdx = 0;` |
|       - | 7203 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 7204 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 7205 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  816440 | 7206 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     100 | 7207 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     100 | 7208 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     100 | 7209 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     100 | 7210 | `			int isSpecial = 0;` |
|     100 | 7211 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|      60 | 7212 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|      60 | 7213 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|      64 | 7214 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      52 | 7215 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      26 | 7216 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      40 | 7217 | `					isSpecial = 1;` |
|      19 | 7218 | `				}` |
|      39 | 7219 | `			}` |
|     120 | 7220 | `			pInstr->iP1 = 0;` |
|     120 | 7221 | `			if( !isSpecial ){` |
|      42 | 7222 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2);` |
|      20 | 7223 | `			}` |
|      39 | 7224 | `		}` |
|      73 | 7225 | `	}` |
|       - | 7226 | `	/* Generate code for the right tree */` |
|  816424 | 7227 | `	if( pNode->pRight ){` |
|  452934 | 7228 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 7229 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    8030 | 7230 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  448920 | 7231 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 7232 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2696 | 7233 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  443559 | 7234 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  198070 | 7235 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|   99034 | 7236 | `		}` |
|  452934 | 7237 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  452934 | 7238 | `		if( iVmOp == PH7_OP_STORE ){` |
|  195400 | 7239 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  195400 | 7240 | `			if( pInstr ){` |
|  195400 | 7241 | `				if( pInstr->iOp == PH7_OP_LOAD_LIST ){` |
|       - | 7242 | `					/* Hide the STORE instruction */` |
|      26 | 7243 | `					iVmOp = 0;` |
|  195388 | 7244 | `				}else if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 7245 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   43438 | 7246 | `					iP2 = 1;` |
|   21720 | 7247 | `				}else{` |
|  151940 | 7248 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7249 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   25816 | 7250 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   25816 | 7251 | `						iP1 = pInstr->iP1;` |
|   12909 | 7252 | `					}else{` |
|  126126 | 7253 | `						p3 = pInstr->p3;` |
|       - | 7254 | `					}` |
|       - | 7255 | `					/* POP the last dynamic load instruction */` |
|  151940 | 7256 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 7257 | `				}` |
|   97701 | 7258 | `			}` |
|  355235 | 7259 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      46 | 7260 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      46 | 7261 | `			if( pInstr ){` |
|      46 | 7262 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7263 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 7264 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 7265 | `					 */` |
|      15 | 7266 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 7267 | `					iP1 = pInstr->iP1;` |
|      15 | 7268 | `					iP2 = pInstr->iP2;` |
|      15 | 7269 | `					p3  = pInstr->p3;` |
|       8 | 7270 | `				}else{` |
|      32 | 7271 | `					p3 = pInstr->p3;` |
|       - | 7272 | `				}` |
|      22 | 7273 | `			}` |
|      22 | 7274 | `		}` |
|  226466 | 7275 | `	}` |
|  816424 | 7276 | `	if( iVmOp > 0 ){` |
|  816370 | 7277 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   10422 | 7278 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 7279 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    7658 | 7280 | `				iP1 = 1;` |
|    3830 | 7281 | `			}` |
|  811160 | 7282 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 7283 | `			/* Namespace-qualify the class name for NEW */ {` |
|   13088 | 7284 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   13088 | 7285 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   13076 | 7286 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    6537 | 7287 | `				}` |
|   13088 | 7288 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 7289 | `					/* Prevent constant expansion for class name */` |
|   13086 | 7290 | `					pPeek->iP1 = 0;` |
|   13086 | 7291 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pPeek->iP2);` |
|    6542 | 7292 | `				}` |
|       - | 7293 | `			}` |
|   13088 | 7294 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   13088 | 7295 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 7296 | `				VmInstr *pPrev;` |
|   13076 | 7297 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   13076 | 7298 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 7299 | `					/* Pop the call instruction */` |
|   13076 | 7300 | `					iP1 = pInstr->iP1;` |
|   13076 | 7301 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    6537 | 7302 | `				}` |
|    6539 | 7303 | `			}` |
|  799407 | 7304 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 7305 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 7306 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 7307 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 7308 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 7309 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 7310 | `				int isSpecialIs = 0;` |
|      50 | 7311 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 7312 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 7313 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 7314 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 7315 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 7316 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 7317 | `						isSpecialIs = 1;` |
|       5 | 7318 | `					}` |
|      23 | 7319 | `				}` |
|      52 | 7320 | `				pInstr->iP1 = 0;` |
|      52 | 7321 | `				if( !isSpecialIs ){` |
|      38 | 7322 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2);` |
|      18 | 7323 | `				}` |
|      25 | 7324 | `			}` |
|  792843 | 7325 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 7326 | `			/* Prevent constant expansion for member/property names.` |
|       - | 7327 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 7328 | `			 * should not trigger constant lookup. */` |
|   97580 | 7329 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   97580 | 7330 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   97564 | 7331 | `				pInstr->iP1 = 0;` |
|   48781 | 7332 | `			}` |
|   97580 | 7333 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 7334 | `				/* Static member access,remember that */` |
|      84 | 7335 | `				iP1 = 1;` |
|      84 | 7336 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      84 | 7337 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      10 | 7338 | `					p3 = pInstr->p3;` |
|      10 | 7339 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       4 | 7340 | `				}` |
|      41 | 7341 | `			}` |
|   48789 | 7342 | `		}` |
|       - | 7343 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  816368 | 7344 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  816368 | 7345 | `		if( nJmpIdx > 0 ){` |
|       - | 7346 | `			/* Fix short-circuited jumps now the destination is resolved */` |
|   10724 | 7347 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   10724 | 7348 | `			if( pInstr ){` |
|   10724 | 7349 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5361 | 7350 | `			}` |
|    5361 | 7351 | `		}` |
|  408183 | 7352 | `	}` |
|  816422 | 7353 | `	return rc;` |
| 1059085 | 7354 |  |
|       - | 7355 | `/*` |
|       - | 7356 | ` * Compile a PHP expression.` |
|       - | 7357 | ` * According to the PHP language reference manual:` |
|       - | 7358 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 7359 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 7360 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 7361 | ` *  is "anything that has a value".` |
|       - | 7362 | ` * If something goes wrong while compiling the expression,this` |
|       - | 7363 | ` * function takes care of generating the appropriate error` |
|       - | 7364 | ` * message.` |
|       - | 7365 | ` */` |
|  561564 | 7366 | `static sxi32 PH7_CompileExpr(` |
|       - | 7367 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7368 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 7369 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 7370 | `	)` |
|       2 | 7371 |  |
|       - | 7372 | `	ph7_expr_node *pRoot;` |
|       - | 7373 | `	SySet sExprNode;` |
|       - | 7374 | `	SyToken *pEnd;` |
|       - | 7375 | `	sxi32 nExpr;` |
|       - | 7376 | `	sxi32 iNest;` |
|       - | 7377 | `	sxi32 rc;` |
|       - | 7378 | `	/* Initialize worker variables */` |
|  561566 | 7379 | `	nExpr = 0;` |
|  561566 | 7380 | `	pRoot = 0;` |
|  561566 | 7381 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  561566 | 7382 | `	SySetAlloc(&sExprNode,0x10);` |
|  561566 | 7383 | `	rc = SXRET_OK;` |
|       - | 7384 | `	/* Delimit the expression */` |
|  561566 | 7385 | `	pEnd = pGen->pIn;` |
|  561566 | 7386 | `	iNest = 0;` |
| 3817364 | 7387 | `	while( pEnd < pGen->pEnd ){` |
| 3610036 | 7388 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7389 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     184 | 7390 | `			iNest++;` |
| 3609945 | 7391 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     192 | 7392 | `			iNest--;` |
| 3609759 | 7393 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  354380 | 7394 | `			if( iNest <= 0 ){` |
|  354238 | 7395 | `				break;` |
|       - | 7396 | `			}` |
|      71 | 7397 | `		}` |
| 3255800 | 7398 | `		pEnd++;` |
|       2 | 7399 | `	}` |
|  561566 | 7400 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   10372 | 7401 | `		SyToken *pEnd2 = pGen->pIn;` |
|   10372 | 7402 | `		iNest = 0;` |
|       - | 7403 | `		/* Stop at the first comma */` |
|   20766 | 7404 | `		while( pEnd2 < pEnd ){` |
|   10396 | 7405 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       6 | 7406 | `				iNest++;` |
|   10394 | 7407 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       6 | 7408 | `				iNest--;` |
|   10390 | 7409 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 7410 | `				if( iNest <= 0 ){` |
|     ! 0 | 7411 | `					break;` |
|       - | 7412 | `				}` |
|       2 | 7413 | `			}` |
|   10396 | 7414 | `			pEnd2++;` |
|       2 | 7415 | `		}` |
|   10372 | 7416 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 7417 | `			pEnd = pEnd2;` |
|     ! 0 | 7418 | `		}` |
|    5185 | 7419 | `	}` |
|  561566 | 7420 | `	if( pEnd > pGen->pIn ){` |
|  561556 | 7421 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 7422 | `		/* Swap delimiter */` |
|  561556 | 7423 | `		pGen->pEnd = pEnd;` |
|       - | 7424 | `		/* Try to get an expression tree */` |
|  561556 | 7425 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  561556 | 7426 | `		if( rc == SXRET_OK && pRoot ){` |
|  561398 | 7427 | `			rc = SXRET_OK;` |
|  561398 | 7428 | `			if( xTreeValidator ){` |
|       - | 7429 | `				/* Call the upper layer validator callback */` |
|   13236 | 7430 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    6617 | 7431 | `			}` |
|  561398 | 7432 | `			if( rc != SXERR_ABORT ){` |
|       - | 7433 | `				/* Generate code for the given tree */` |
|  561398 | 7434 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  280698 | 7435 | `			}` |
|  561398 | 7436 | `			nExpr = 1;` |
|  280698 | 7437 | `		}` |
|       - | 7438 | `		/* Release the whole tree */` |
|  561556 | 7439 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 7440 | `		/* Synchronize token stream */` |
|  561556 | 7441 | `		pGen->pEnd = pTmp;` |
|  561556 | 7442 | `		pGen->pIn  = pEnd;` |
|  561556 | 7443 | `		if( rc == SXERR_ABORT ){` |
|       3 | 7444 | `			SySetRelease(&sExprNode);` |
|       3 | 7445 | `			return SXERR_ABORT;` |
|       - | 7446 | `		}` |
|  280776 | 7447 | `	}` |
|  561564 | 7448 | `	SySetRelease(&sExprNode);` |
|  561564 | 7449 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  280784 | 7450 |  |
|       - | 7451 | `/*` |
|       - | 7452 | ` * Return a pointer to the node construct handler associated` |
|       - | 7453 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 7454 | ` */` |
|  152172 | 7455 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 7456 |  |
|  152174 | 7457 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 7458 | `		/* Numeric literal: Either real or integer */` |
|   83788 | 7459 | `		return PH7_CompileNumLiteral;` |
|   68388 | 7460 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 7461 | `		/* Double quoted string */` |
|   13738 | 7462 | `		return PH7_CompileString;` |
|   54652 | 7463 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 7464 | `		/* Single quoted string */` |
|   54592 | 7465 | `		return PH7_CompileSimpleString;` |
|      62 | 7466 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 7467 | `		/* Heredoc */` |
|      28 | 7468 | `		return PH7_CompileHereDoc;` |
|      36 | 7469 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 7470 | `		/* Nowdoc */` |
|      29 | 7471 | `		return PH7_CompileNowDoc;` |
|       7 | 7472 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 7473 | `		/* Backtick quoted string */` |
|       5 | 7474 | `		return PH7_CompileBacktic;` |
|       - | 7475 | `	}` |
|       3 | 7476 | `	return 0;` |
|   76088 | 7477 |  |
|       - | 7478 | `/*` |
|       - | 7479 | ` * Compile an unset() statement.` |
|       - | 7480 | ` * unset($var, $arr[$key], ...);` |
|       - | 7481 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 7482 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 7483 | ` * parent array before extracting the element to unset.` |
|       - | 7484 | ` */` |
|    2488 | 7485 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 7486 |  |
|    2490 | 7487 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2490 | 7488 | `	sxu32 nIdx = 0;` |
|       - | 7489 | `	SyString sName;` |
|       - | 7490 | `	sxi32 rc;` |
|       - | 7491 | `	/* Jump the 'unset' keyword */` |
|    2490 | 7492 | `	pGen->pIn++;` |
|       - | 7493 | `	/* Save delimiter */` |
|    2490 | 7494 | `	pTmp = pGen->pEnd;` |
|       - | 7495 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2490 | 7496 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2490 | 7497 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 7498 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 7499 | `		SyToken *pClose;` |
|    2490 | 7500 | `		pGen->pIn++;   /* Skip '(' */` |
|    2490 | 7501 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2490 | 7502 | `		pEnd = pClose; /* Stop at ')' */` |
|    1244 | 7503 | `	}` |
|    2490 | 7504 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 7505 | `	/* Resolve the 'unset' builtin name once */` |
|    2490 | 7506 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     326 | 7507 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     326 | 7508 | `		if( pObj == 0 ){` |
|     ! 0 | 7509 | `			return SXERR_ABORT;` |
|       - | 7510 | `		}` |
|     326 | 7511 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     326 | 7512 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     162 | 7513 | `	}` |
|       - | 7514 | `	/* Compile each comma-separated argument */` |
|    8174 | 7515 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    5686 | 7516 | `		if( pGen->pIn < pNext ){` |
|    5686 | 7517 | `			pGen->pEnd = pNext;` |
|    5686 | 7518 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 7519 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,0);` |
|    5686 | 7520 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7521 | `				return SXERR_ABORT;` |
|       - | 7522 | `			}` |
|    5686 | 7523 | `			if( rc != SXERR_EMPTY ){` |
|       - | 7524 | `				/* Emit call for this single argument */` |
|    5684 | 7525 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5684 | 7526 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    5684 | 7527 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    2841 | 7528 | `			}` |
|    2842 | 7529 | `		}` |
|       - | 7530 | `		/* Jump trailing commas */` |
|    8882 | 7531 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3198 | 7532 | `			pNext++;` |
|       2 | 7533 | `		}` |
|    5686 | 7534 | `		pGen->pIn = pNext;` |
|       2 | 7535 | `	}` |
|       - | 7536 | `	/* Skip past the closing ')' if present */` |
|    2490 | 7537 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2490 | 7538 | `		pGen->pIn++;` |
|    1244 | 7539 | `	}` |
|       - | 7540 | `	/* Restore token stream */` |
|    2490 | 7541 | `	pGen->pEnd = pTmp;` |
|    2490 | 7542 | `	return SXRET_OK;` |
|    1246 | 7543 |  |
|       - | 7544 | `/*` |
|       - | 7545 | ` * PHP Language construct table.` |
|       - | 7546 | ` */` |
|       - | 7547 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 7548 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 7549 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 7550 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 7551 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 7552 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 7553 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 7554 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 7555 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 7556 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 7557 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 7558 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 7559 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 7560 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 7561 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 7562 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 7563 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 7564 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 7565 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 7566 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 7567 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 7568 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 7569 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 7570 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 7571 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 7572 | `};` |
|       - | 7573 | `/*` |
|       - | 7574 | ` * Return a pointer to the statement handler routine associated` |
|       - | 7575 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 7576 | ` */` |
|  321090 | 7577 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 7578 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 7579 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 7580 | `	)` |
|       2 | 7581 |  |
|  321092 | 7582 | `	sxu32 n = 0;` |
| 1233238 | 7583 | `	for(;;){` |
| 2466478 | 7584 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   31162 | 7585 | `			break;` |
|       - | 7586 | `		}` |
| 2435318 | 7587 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  289932 | 7588 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 7589 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 7590 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 7591 | `					/* 'static' (class context),return null */` |
|     ! 0 | 7592 | `					return 0;` |
|       - | 7593 | `				}` |
|     ! 0 | 7594 | `			}` |
|       - | 7595 | `			/* Return a pointer to the handler.` |
|       - | 7596 | `			*/` |
|  289932 | 7597 | `			return aLangConstruct[n].xConstruct;` |
|       - | 7598 | `		}` |
| 2145388 | 7599 | `		n++;` |
|       2 | 7600 | `	}` |
|   31162 | 7601 | `	if( pLookahed ){` |
|   31162 | 7602 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    7684 | 7603 | `			return PH7_CompileClassInterface;` |
|   23480 | 7604 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   23316 | 7605 | `			return PH7_CompileClass;` |
|     166 | 7606 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      51 | 7607 | `			return PH7_CompileTrait;` |
|     114 | 7608 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      17 | 7609 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      16 | 7610 | `				return PH7_CompileAbstractClass;` |
|     100 | 7611 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 7612 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 7613 | `				return PH7_CompileFinalClass;` |
|       - | 7614 | `		}` |
|      49 | 7615 | `	}` |
|       - | 7616 | `	/* Not a language construct */` |
|     100 | 7617 | `	return 0;` |
|  160547 | 7618 |  |
|       - | 7619 | `/*` |
|       - | 7620 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 7621 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 7622 | ` */` |
|      98 | 7623 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 7624 |  |
|       - | 7625 | `	int rc;` |
|     100 | 7626 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     100 | 7627 | `	if( rc == FALSE ){` |
|      14 | 7628 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|       - | 7629 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 7630 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 7631 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 7632 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 7633 | `			*/` |
|       - | 7634 | `			){` |
|       6 | 7635 | `				rc = TRUE;` |
|       2 | 7636 | `		}` |
|       6 | 7637 | `	}` |
|     100 | 7638 | `	return rc;` |
|       2 | 7639 |  |
|       - | 7640 | `/*` |
|       - | 7641 | ` * Compile a PHP chunk.` |
|       - | 7642 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 7643 | ` * takes care of generating the appropriate error message.` |
|       - | 7644 | ` */` |
|  455866 | 7645 | `static sxi32 GenStateCompileChunk(` |
|       - | 7646 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7647 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 7648 | `	)` |
|       2 | 7649 |  |
|       - | 7650 | `	ProcLangConstruct xCons;` |
|       - | 7651 | `	sxi32 rc;` |
|  455868 | 7652 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  269970 | 7653 | `	for(;;){` |
|  539942 | 7654 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7655 | `			/* No more input to process */` |
|   10882 | 7656 | `			break;` |
|       - | 7657 | `		}` |
|  529062 | 7658 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7659 | `			/* Compile block */` |
|      12 | 7660 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 7661 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7662 | `				break;` |
|       - | 7663 | `			}` |
|       7 | 7664 | `		}else{` |
|  529052 | 7665 | `			xCons = 0;` |
|  529052 | 7666 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  321092 | 7667 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 7668 | `				/* Try to extract a language construct handler */` |
|  321092 | 7669 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  321092 | 7670 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 7671 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7672 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 7673 | `						&pGen->pIn->sData);` |
|       9 | 7674 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7675 | `						break;` |
|       - | 7676 | `					}` |
|       - | 7677 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 7678 | `					 * this erroneous statement.` |
|       - | 7679 | `					 */` |
|       9 | 7680 | `					xCons = PH7_ErrorRecover;` |
|       4 | 7681 | `				}` |
|  368507 | 7682 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   29958 | 7683 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 7684 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 7685 | `				xCons = PH7_CompileLabel;` |
|      56 | 7686 | `			}` |
|  529052 | 7687 | `			if( xCons == 0 ){` |
|       - | 7688 | `				/* Assume an expression an try to compile it */` |
|  207940 | 7689 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  207940 | 7690 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 7691 | `					/* Pop l-value */` |
|  207814 | 7692 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  103906 | 7693 | `				}` |
|  103971 | 7694 | `			}else{` |
|       - | 7695 | `				/* Go compile the sucker */` |
|  321114 | 7696 | `				rc = xCons(&(*pGen));` |
|       - | 7697 | `			}` |
|  529052 | 7698 | `			if( rc == SXERR_ABORT ){` |
|       - | 7699 | `				/* Request to abort compilation */` |
|       3 | 7700 | `				break;` |
|       - | 7701 | `			}` |
|       - | 7702 | `		}` |
|       - | 7703 | `		/* Ignore trailing semi-colons ';' */` |
|  867558 | 7704 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  338500 | 7705 | `			pGen->pIn++;` |
|       2 | 7706 | `		}` |
|  529060 | 7707 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 7708 | `			/* Compile a single statement and return */` |
|  444986 | 7709 | `			break;` |
|       - | 7710 | `		}` |
|       - | 7711 | `		/* LOOP ONE */` |
|       - | 7712 | `		/* LOOP TWO */` |
|       - | 7713 | `		/* LOOP THREE */` |
|       - | 7714 | `		/* LOOP FOUR */` |
|       2 | 7715 | `	}` |
|       - | 7716 | `	/* Return compilation status */` |
|  455868 | 7717 | `	return rc;` |
|       2 | 7718 |  |
|       - | 7719 | `/*` |
|       - | 7720 | ` * Compile a Raw PHP chunk.` |
|       - | 7721 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 7722 | ` * takes care of generating the appropriate error message.` |
|       - | 7723 | ` */` |
|   10884 | 7724 | `static sxi32 PH7_CompilePHP(` |
|       - | 7725 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7726 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 7727 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 7728 | `	)` |
|       2 | 7729 |  |
|   10886 | 7730 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 7731 | `	sxi32 rc;` |
|       - | 7732 | `	/* Reset the token set */` |
|   10886 | 7733 | `	SySetReset(&(*pTokenSet));` |
|       - | 7734 | `	/* Mark as the default token set */` |
|   10886 | 7735 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 7736 | `	/* Advance the stream cursor */` |
|   10886 | 7737 | `	pGen->pRawIn++;` |
|       - | 7738 | `	/* Tokenize the PHP chunk first */` |
|   10886 | 7739 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 7740 | `	/* Point to the head and tail of the token stream. */` |
|   10886 | 7741 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   10886 | 7742 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   10886 | 7743 | `	if( is_expr ){` |
|     ! 0 | 7744 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 7745 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 7746 | `			/* A simple expression,compile it */` |
|     ! 0 | 7747 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 7748 | `		}` |
|       - | 7749 | `		/* Emit the DONE instruction */` |
|     ! 0 | 7750 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 7751 | `		return SXRET_OK;` |
|       - | 7752 | `	}` |
|   10886 | 7753 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 7754 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 7755 | `		/*` |
|       - | 7756 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 7757 | `		 * According to the PHP reference manual:` |
|       - | 7758 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 7759 | `		 *  immediately follow` |
|       - | 7760 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 7761 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 7762 | `		 * Symisc extension:` |
|       - | 7763 | `		 *   This short syntax works with all PHP opening` |
|       - | 7764 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 7765 | `		 *   only short tag.` |
|       - | 7766 | `		 */` |
|       - | 7767 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 7768 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 7769 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 7770 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 7771 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 7772 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 7773 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 7774 | `		}` |
|       3 | 7775 | `		return SXRET_OK;` |
|       - | 7776 | `	}` |
|       - | 7777 | `	/* Compile the PHP chunk */` |
|   10884 | 7778 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 7779 | `	/* Fix exceptions jumps */` |
|   10884 | 7780 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7781 | `	/* Fix gotos now, the jump destination is resolved */` |
|   10884 | 7782 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 7783 | `		rc = SXERR_ABORT;` |
|       1 | 7784 | `	}` |
|       - | 7785 | `	/* Reset container */` |
|   10884 | 7786 | `	SySetReset(&pGen->aGoto);` |
|   10884 | 7787 | `	SySetReset(&pGen->aLabel);` |
|       - | 7788 | `	/* Compilation result */` |
|   10884 | 7789 | `	return rc;` |
|    5444 | 7790 |  |
|       - | 7791 | `/*` |
|       - | 7792 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 7793 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 7794 | ` * This is the only compile interface exported from this file.` |
|       - | 7795 | ` */` |
|   12750 | 7796 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 7797 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 7798 | `	SyString *pScript,  /* Script to compile */` |
|       - | 7799 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 7800 | `	)` |
|       2 | 7801 |  |
|       - | 7802 | `	SySet aPhpToken,aRawToken;` |
|       - | 7803 | `	ph7_gen_state *pCodeGen;` |
|       - | 7804 | `	ph7_value *pRawObj;` |
|       - | 7805 | `	sxu32 nObjIdx;` |
|       - | 7806 | `	sxi32 nRawObj;` |
|       - | 7807 | `	int is_expr;` |
|       - | 7808 | `	sxi32 rc;` |
|   12752 | 7809 | `	if( pScript->nByte < 1 ){` |
|       - | 7810 | `		/* Nothing to compile */` |
|     ! 0 | 7811 | `		return PH7_OK;` |
|       - | 7812 | `	}` |
|       - | 7813 | `	/* Initialize the tokens containers */` |
|   12752 | 7814 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   12752 | 7815 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   12752 | 7816 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   12752 | 7817 | `	is_expr = 0;` |
|   12752 | 7818 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 7819 | `		SyToken sTmp;` |
|       - | 7820 | `		/* PHP only: -*/` |
|    2574 | 7821 | `		sTmp.nLine = 1;` |
|    2574 | 7822 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2574 | 7823 | `		sTmp.pUserData = 0;` |
|    2574 | 7824 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2574 | 7825 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2574 | 7826 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 7827 | `			/* A simple PHP expression */` |
|     ! 0 | 7828 | `			is_expr = 1;` |
|     ! 0 | 7829 | `		}` |
|    1288 | 7830 | `	}else{` |
|       - | 7831 | `		/* Tokenize raw text */` |
|   10180 | 7832 | `		SySetAlloc(&aRawToken,32);` |
|   10180 | 7833 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 7834 | `	}` |
|   12752 | 7835 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 7836 | `	/* Process high-level tokens */` |
|   12752 | 7837 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   12752 | 7838 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   12752 | 7839 | `	rc = PH7_OK;` |
|   12752 | 7840 | `	if( is_expr ){` |
|       - | 7841 | `		/* Compile the expression */` |
|     ! 0 | 7842 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 7843 | `		goto cleanup;` |
|       - | 7844 | `	}` |
|   12752 | 7845 | `	nObjIdx = 0;` |
|       - | 7846 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 7847 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 7848 | `	 * preventing namespace bleeding across include()d files. */` |
|   12752 | 7849 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 7850 | `	/* Start the compilation process */` |
|   11468 | 7851 | `	for(;;){` |
|   33818 | 7852 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   12748 | 7853 | `			break; /* No more tokens to process */` |
|       - | 7854 | `		}` |
|   21072 | 7855 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 7856 | `			/* Compile the PHP chunk */` |
|   10886 | 7857 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   10886 | 7858 | `			if( rc == SXERR_ABORT ){` |
|       5 | 7859 | `				break;` |
|       - | 7860 | `			}` |
|   10882 | 7861 | `			continue;` |
|       - | 7862 | `		}` |
|       - | 7863 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   10188 | 7864 | `		nRawObj = 0;` |
|   20374 | 7865 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 7866 | `			/* Consume the raw chunk without any processing */` |
|   10188 | 7867 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   10188 | 7868 | `			if( pRawObj == 0 ){` |
|     ! 0 | 7869 | `				rc = SXERR_MEM;` |
|     ! 0 | 7870 | `				break;` |
|       - | 7871 | `			}` |
|       - | 7872 | `			/* Mark as constant and emit the load constant instruction */` |
|   10188 | 7873 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   10188 | 7874 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   10188 | 7875 | `			++nRawObj;` |
|   10188 | 7876 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 7877 | `		}` |
|   10188 | 7878 | `		if( nRawObj > 0 ){` |
|       - | 7879 | `			/* Emit the consume instruction */` |
|   10188 | 7880 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5093 | 7881 | `		}` |
|    6377 | 7882 | `	}` |
|    6375 | 7883 | `cleanup:` |
|   12752 | 7884 | `	SySetRelease(&aRawToken);` |
|   12752 | 7885 | `	SySetRelease(&aPhpToken);` |
|   12752 | 7886 | `	return rc;` |
|    6377 | 7887 |  |
|       - | 7888 | `/*` |
|       - | 7889 | ` * Utility routines.Initialize the code generator.` |
|       - | 7890 | ` */` |
|    2550 | 7891 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 7892 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 7893 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 7894 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 7895 | `	)` |
|       2 | 7896 |  |
|    2552 | 7897 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 7898 | `	/* Zero the structure */` |
|    2552 | 7899 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 7900 | `	/* Initial state */` |
|    2552 | 7901 | `	pGen->pVm  = &(*pVm);` |
|    2552 | 7902 | `	pGen->xErr = xErr;` |
|    2552 | 7903 | `	pGen->pErrData = pErrData;` |
|    2552 | 7904 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2552 | 7905 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2552 | 7906 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2552 | 7907 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 7908 | `	/* Error log buffer */` |
|    2552 | 7909 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 7910 | `	/* General purpose working buffer */` |
|    2552 | 7911 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 7912 | `	/* Namespace state */` |
|    2552 | 7913 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2552 | 7914 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 7915 | `	/* Create the global scope */` |
|    2552 | 7916 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 7917 | `	/* Point to the global scope */` |
|    2552 | 7918 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2552 | 7919 | `	return SXRET_OK;` |
|       2 | 7920 |  |
|       - | 7921 | `/*` |
|       - | 7922 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 7923 | ` */` |
|   15038 | 7924 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 7925 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 7926 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 7927 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 7928 | `	)` |
|       2 | 7929 |  |
|   15040 | 7930 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 7931 | `	GenBlock *pBlock,*pParent;` |
|       - | 7932 | `	/* Reset state */` |
|   15040 | 7933 | `	SySetReset(&pGen->aLabel);` |
|   15040 | 7934 | `	SySetReset(&pGen->aGoto);` |
|   15040 | 7935 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   15040 | 7936 | `	SyBlobRelease(&pGen->sWorker);` |
|   15040 | 7937 | `	SyBlobRelease(&pGen->sNamespace);` |
|   15040 | 7938 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   15040 | 7939 | `	SyHashRelease(&pGen->hUseImports);` |
|   15040 | 7940 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 7941 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 7942 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 7943 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 7944 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 7945 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 7946 | `	 * number of unique names, which is acceptable. */` |
|       - | 7947 | `	/* Point to the global scope */` |
|   15040 | 7948 | `	pBlock = pGen->pCurrent;` |
|   15040 | 7949 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 7950 | `		pParent = pBlock->pParent;` |
|     ! 0 | 7951 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 7952 | `		pBlock = pParent;` |
|     ! 0 | 7953 | `	}` |
|   15040 | 7954 | `	pGen->xErr = xErr;` |
|   15040 | 7955 | `	pGen->pErrData = pErrData;` |
|   15040 | 7956 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   15040 | 7957 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   15040 | 7958 | `	pGen->pIn = pGen->pEnd = 0;` |
|   15040 | 7959 | `	pGen->nErr = 0;` |
|   15040 | 7960 | `	return SXRET_OK;` |
|       2 | 7961 |  |
|       - | 7962 | `/*` |
|       - | 7963 | ` * Generate a compile-time error message.` |
|       - | 7964 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 7965 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 7966 | ` * abort compilation immediately.` |
|       - | 7967 | ` */` |
|     454 | 7968 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 7969 |  |
|     456 | 7970 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     456 | 7971 | `	const char *zErr = "Error";` |
|       - | 7972 | `	SyString *pFile;` |
|       - | 7973 | `	va_list ap;` |
|       - | 7974 | `	sxi32 rc;` |
|       - | 7975 | `	/* Reset the working buffer */` |
|     456 | 7976 | `	SyBlobReset(pWorker);` |
|       - | 7977 | `	/* Peek the processed file path if available */` |
|     456 | 7978 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     456 | 7979 | `	if( nErrType == E_ERROR ){` |
|       - | 7980 | `		/* Increment the error counter */` |
|     414 | 7981 | `		pGen->nErr++;` |
|     414 | 7982 | `		if( pGen->nErr > 15 ){` |
|       - | 7983 | `			/* Error count limit reached */` |
|       5 | 7984 | `			if( pGen->xErr ){` |
|       5 | 7985 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 7986 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 7987 | `				if( pFile ){` |
|       5 | 7988 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 7989 | `				}` |
|       5 | 7990 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 7991 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 7992 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 7993 | `				}` |
|       2 | 7994 | `			}` |
|       - | 7995 | `			/* Abort immediately */` |
|       5 | 7996 | `			return SXERR_ABORT;` |
|       - | 7997 | `		}` |
|     204 | 7998 | `	}` |
|     452 | 7999 | `	if( pGen->xErr == 0 ){` |
|       - | 8000 | `		/* No available error consumer,return immediately */` |
|       3 | 8001 | `		return SXRET_OK;` |
|       - | 8002 | `	}` |
|     449 | 8003 | `	switch(nErrType){` |
|     407 | 8004 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      29 | 8005 | `	case E_WARNING: zErr = "Warning";     break;` |
|       7 | 8006 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 8007 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 8008 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 8009 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 8010 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 8011 | `	default:` |
|     ! 0 | 8012 | `		break;` |
|       - | 8013 | `	}` |
|     449 | 8014 | `	rc = SXRET_OK;` |
|       - | 8015 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     449 | 8016 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     449 | 8017 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     449 | 8018 | `	va_start(ap,zFormat);` |
|     449 | 8019 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     449 | 8020 | `	va_end(ap);` |
|     449 | 8021 | `	if( pFile ){` |
|     449 | 8022 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     224 | 8023 | `	}` |
|       - | 8024 | `	/* Append a new line */` |
|     449 | 8025 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     449 | 8026 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 8027 | `		/* Consume the generated error message */` |
|     449 | 8028 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     224 | 8029 | `	}` |
|     449 | 8030 | `	return rc;` |
|     229 | 8031 |  |
|       - | 8032 |  |
