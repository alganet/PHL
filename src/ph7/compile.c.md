# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3674/4779 lines (76.88%)

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
|    2850 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    2852 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    7993 |  131 | `	for(;;){` |
|   15988 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2740 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2740 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2718 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      11 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   13272 |  140 | `		pBlock = pBlock->pParent;` |
|   13272 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1427 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  553308 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  553310 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  553310 |  162 | `	pBlock->pUserData   = pUserData;` |
|  553310 |  163 | `	pBlock->pGen        = pGen;` |
|  553310 |  164 | `	pBlock->iFlags      = iType;` |
|  553310 |  165 | `	pBlock->pParent     = 0;` |
|  553310 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  553310 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  553310 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  550716 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  550718 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  550718 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  550718 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  550718 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  550718 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  550718 |  200 | `	pGen->pCurrent = pBlock;` |
|  550718 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  266500 |  203 | `		*ppBlock = pBlock;` |
|  133249 |  204 | `	}` |
|  550718 |  205 | `	return SXRET_OK;` |
|  275360 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  550708 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  550710 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  550710 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  550710 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  550708 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  550710 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  550710 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  550710 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  550710 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  550708 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  550710 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  550710 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  550710 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  550710 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  550710 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  550710 |  244 | `	return SXRET_OK;` |
|  275356 |  245 |  |
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
|  167964 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  167966 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  167966 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  167966 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  167966 |  265 | `	return rc;` |
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
|  392288 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  392290 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  719646 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  327358 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  127508 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  199852 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   31890 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  167964 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  167964 |  298 | `		if( pInstr ){` |
|  167964 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  167964 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  167964 |  302 | `			aFix[n].nJumpType = -1;` |
|   83981 |  303 | `		}` |
|   83983 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  392290 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|  149728 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|  149730 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|  149876 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  149728 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  149860 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|  149728 |  358 | `	return SXRET_OK;` |
|   74866 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  487654 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  487656 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  487656 |  367 | `	if( pEntry == 0 ){` |
|  240370 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  247288 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  247288 |  371 | `	return SXRET_OK;` |
|  243829 |  372 |  |
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
|  240368 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  240370 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  240370 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  120184 |  387 | `	}` |
|  240370 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   85254 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   85256 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   85256 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   85256 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   85256 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   85256 |  408 | `	return pObj;` |
|   42629 |  409 |  |
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
|   85668 |  431 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  432 |  |
|   85670 |  433 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   85670 |  434 | `	sxu32 nIdx = 0;` |
|   85670 |  435 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  436 | `		ph7_value *pObj;` |
|       - |  437 | `		sxi64 iValue;` |
|   85256 |  438 | `		iValue = PH7_TokenValueToInt64(&pToken->sData);` |
|   85256 |  439 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   85256 |  440 | `		if( pObj == 0 ){` |
|     ! 0 |  441 | `			SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  442 | `			return SXERR_ABORT;` |
|       - |  443 | `		}` |
|   85256 |  444 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   42629 |  445 | `	}else{` |
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
|   85670 |  458 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  459 | `	/* Node successfully compiled */` |
|   85670 |  460 | `	return SXRET_OK;` |
|   42836 |  461 |  |
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
|   56092 |  473 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  474 |  |
|   56094 |  475 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  476 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  477 | `	ph7_value *pObj;` |
|       - |  478 | `	sxu32 nIdx;` |
|   56094 |  479 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  480 | `	/* Delimit the string */` |
|   56094 |  481 | `	zIn  = pStr->zString;` |
|   56094 |  482 | `	zEnd = &zIn[pStr->nByte];` |
|   56094 |  483 | `	if( zIn >= zEnd ){` |
|       - |  484 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  485 | `		 * rather than reserving a new object each time. */` |
|     138 |  486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     138 |  487 | `		return SXRET_OK;` |
|       - |  488 | `	}` |
|   55958 |  489 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  490 | `		/* Already processed,emit the load constant instruction` |
|       - |  491 | `		 * and return.` |
|       - |  492 | `		 */` |
|   16624 |  493 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   16624 |  494 | `		return SXRET_OK;` |
|       - |  495 | `	}` |
|       - |  496 | `	/* Reserve a new constant */` |
|   39336 |  497 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   39336 |  498 | `	if( pObj == 0 ){` |
|     ! 0 |  499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  500 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  501 | `		return SXERR_ABORT;` |
|       - |  502 | `	}` |
|   39336 |  503 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  504 | `	/* Compile the node */` |
|   39376 |  505 | `	for(;;){` |
|   78754 |  506 | `		if( zIn >= zEnd ){` |
|       - |  507 | `			/* End of input */` |
|   39336 |  508 | `			break;` |
|       - |  509 | `		}` |
|   39420 |  510 | `		zCur = zIn;` |
|  625788 |  511 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  586370 |  512 | `			zIn++;` |
|       2 |  513 | `		}` |
|   39420 |  514 | `		if( zIn > zCur ){` |
|       - |  515 | `			/* Append raw contents*/` |
|   39400 |  516 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   19699 |  517 | `		}` |
|   39420 |  518 | `		zIn++;` |
|   39420 |  519 | `		if( zIn < zEnd ){` |
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
|   39420 |  534 | `		zIn++;` |
|       2 |  535 | `	}` |
|       - |  536 | `	/* Emit the load constant instruction */` |
|   39336 |  537 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   39336 |  538 | `	if( pStr->nByte < 1024 ){` |
|       - |  539 | `		/* Install in the literal table */` |
|   39336 |  540 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   19667 |  541 | `	}` |
|       - |  542 | `	/* Node successfully compiled */` |
|   39336 |  543 | `	return SXRET_OK;` |
|   28048 |  544 |  |
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
|   16290 |  640 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  641 |  |
|       - |  642 | `	ph7_value *pConstObj;` |
|   16292 |  643 | `	sxu32 nIdx = 0;` |
|       - |  644 | `	/* Reserve a new constant */` |
|   16292 |  645 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   16292 |  646 | `	if( pConstObj == 0 ){` |
|     ! 0 |  647 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  648 | `		return 0;` |
|       - |  649 | `	}` |
|   16292 |  650 | `	(*pCount)++;` |
|   16292 |  651 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  652 | `	/* Emit the load constant instruction */` |
|   16292 |  653 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   16292 |  654 | `	return pConstObj;` |
|    8147 |  655 |  |
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
|   15120 |  694 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  695 |  |
|   15122 |  696 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  697 | `	const char *zIn,*zCur,*zEnd;` |
|   15122 |  698 | `	ph7_value *pObj = 0;` |
|       - |  699 | `	sxi32 iCons;` |
|       - |  700 | `	sxi32 rc;` |
|       - |  701 | `	/* Delimit the string */` |
|   15122 |  702 | `	zIn  = pStr->zString;` |
|   15122 |  703 | `	zEnd = &zIn[pStr->nByte];` |
|   15122 |  704 | `	if( zIn >= zEnd ){` |
|       - |  705 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  706 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  707 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  708 | `		 */` |
|     226 |  709 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     226 |  710 | `		return SXRET_OK;` |
|       - |  711 | `	}` |
|   14898 |  712 | `	zCur = 0;` |
|       - |  713 | `	/* Compile the node */` |
|   14898 |  714 | `	iCons = 0;` |
|    8273 |  715 | `	for(;;){` |
|   24994 |  716 | `		zCur = zIn;` |
|  136788 |  717 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  113446 |  718 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      44 |  719 | `				break;` |
|  113362 |  720 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1568 |  721 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     784 |  722 | `					break;` |
|       - |  723 | `			}` |
|  111796 |  724 | `			zIn++;` |
|       2 |  725 | `		}` |
|   24994 |  726 | `		if( zIn > zCur ){` |
|   11732 |  727 | `			if( pObj == 0 ){` |
|   11456 |  728 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   11456 |  729 | `				if( pObj == 0 ){` |
|     ! 0 |  730 | `					return SXERR_ABORT;` |
|       - |  731 | `				}` |
|    5727 |  732 | `			}` |
|   11732 |  733 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    5865 |  734 | `		}` |
|   24994 |  735 | `		if( zIn >= zEnd ){` |
|   14898 |  736 | `			break;` |
|       - |  737 | `		}` |
|   10098 |  738 | `		if( zIn[0] == '\\' ){` |
|    8448 |  739 | `			const char *zPtr = 0;` |
|       - |  740 | `			sxu32 n;` |
|    8448 |  741 | `			zIn++;` |
|    8448 |  742 | `			if( zIn >= zEnd ){` |
|     ! 0 |  743 | `				break;` |
|       - |  744 | `			}` |
|    8448 |  745 | `			if( pObj == 0 ){` |
|    4838 |  746 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    4838 |  747 | `				if( pObj == 0 ){` |
|     ! 0 |  748 | `					return SXERR_ABORT;` |
|       - |  749 | `				}` |
|    2418 |  750 | `			}` |
|    8448 |  751 | `			n = sizeof(char); /* size of conversion */` |
|    8448 |  752 | `			switch( zIn[0] ){` |
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
|    3858 |  773 | `			case 'n':` |
|       - |  774 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    7718 |  775 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    7718 |  776 | `				break;` |
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
|    8448 |  844 | `			zIn += n;` |
|    8448 |  845 | `			continue;` |
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
|   14898 |  963 | `	if( iCons > 1 ){` |
|       - |  964 | `		/* Concatenate all compiled constants */` |
|    1260 |  965 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     629 |  966 | `	}` |
|       - |  967 | `	/* Node successfully compiled */` |
|   14898 |  968 | `	return SXRET_OK;` |
|    7562 |  969 |  |
|       - |  970 | `/*` |
|       - |  971 | ` * Compile a double quoted string.` |
|       - |  972 | ` *  See the block-comment above for more information.` |
|       - |  973 | ` */` |
|   15094 |  974 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  975 |  |
|       - |  976 | `	sxi32 rc;` |
|   15096 |  977 | `	rc = GenStateCompileString(&(*pGen));` |
|    7547 |  978 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  979 | `	/* Compilation result */` |
|   15096 |  980 | `	return rc;` |
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
|   15458 | 1012 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   15460 | 1023 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1024 | `	/* Compile the expression*/` |
|   15460 | 1025 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1026 | `	/* Restore token stream */` |
|   15460 | 1027 | `	RE_SWAP_DELIMITER(pGen);` |
|   15460 | 1028 | `	return rc;` |
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
|   22678 | 1067 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 | 1068 |  |
|       - | 1069 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1070 | `	SyToken *pKey,*pCur;` |
|   22680 | 1071 | `	sxi32 iEmitRef = 0;` |
|   22680 | 1072 | `	sxi32 nPair = 0;` |
|       - | 1073 | `	sxi32 iNest;` |
|       - | 1074 | `	sxi32 rc;` |
|   22680 | 1075 | `	xValidator = 0;` |
|   18446 | 1076 | `	for(;;){` |
|       - | 1077 | `		/* Jump leading commas */` |
|   41726 | 1078 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    4834 | 1079 | `			pGen->pIn++;` |
|       2 | 1080 | `		}` |
|   36894 | 1081 | `		pCur = pGen->pIn;` |
|   36894 | 1082 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1083 | `			/* No more entry to process */` |
|   22668 | 1084 | `			break;` |
|       - | 1085 | `		}` |
|   14228 | 1086 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1087 | `			continue;` |
|       - | 1088 | `		}` |
|       - | 1089 | `		/* Compile the key if available */` |
|   14228 | 1090 | `		pKey = pCur;` |
|   14228 | 1091 | `		iNest = 0;` |
|   39430 | 1092 | `		while( pCur < pGen->pIn ){` |
|   26394 | 1093 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1192 | 1094 | `				break;` |
|       - | 1095 | `			}` |
|   25204 | 1096 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      78 | 1097 | `				iNest++;` |
|   25166 | 1098 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1099 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1100 | `				 * parser will shortly detect any syntax error.` |
|       - | 1101 | `				 */` |
|      78 | 1102 | `				iNest--;` |
|      38 | 1103 | `			}` |
|   25204 | 1104 | `			pCur++;` |
|       2 | 1105 | `		}` |
|   14228 | 1106 | `		rc = SXERR_EMPTY;` |
|   14228 | 1107 | `		if( pCur < pGen->pIn ){` |
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
|   13628 | 1123 | `		}else if( pKey == pCur ){` |
|       - | 1124 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1125 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1126 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1127 | `		}else{` |
|       - | 1128 | `			/* Reset back the cursor and point to the entry value */` |
|   13038 | 1129 | `			pCur = pKey;` |
|       - | 1130 | `		}` |
|   14218 | 1131 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1132 | `			/* No available key,load NULL */` |
|   13040 | 1133 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    6519 | 1134 | `		}` |
|   14218 | 1135 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   14216 | 1150 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   14216 | 1151 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1152 | `			return SXERR_ABORT;` |
|       - | 1153 | `		}` |
|   14216 | 1154 | `		if( iEmitRef ){` |
|       - | 1155 | `			/* Emit the load reference instruction */` |
|      32 | 1156 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1157 | `		}` |
|   14216 | 1158 | `		xValidator = 0;` |
|   14216 | 1159 | `		iEmitRef = 0;` |
|   14216 | 1160 | `		nPair++;` |
|       2 | 1161 | `	}` |
|       - | 1162 | `	/* Emit the load map instruction */` |
|   22668 | 1163 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1164 | `	/* Node successfully compiled */` |
|   22668 | 1165 | `	return SXRET_OK;` |
|   11341 | 1166 |  |
|       - | 1167 | `/*` |
|       - | 1168 | ` * Compile the 'array' language construct.` |
|       - | 1169 | ` *	 According to the PHP language reference manual` |
|       - | 1170 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1171 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1172 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1173 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1174 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1175 | ` */` |
|   22490 | 1176 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1177 |  |
|       - | 1178 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   22492 | 1179 | `	pGen->pIn += 2;` |
|   22492 | 1180 | `	pGen->pEnd--;` |
|   11245 | 1181 | `	SXUNUSED(iCompileFlag);` |
|   22492 | 1182 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1183 |  |
|       - | 1184 | `/*` |
|       - | 1185 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - | 1186 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - | 1187 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - | 1188 | ` */` |
|     188 | 1189 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1190 |  |
|       - | 1191 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     190 | 1192 | `	pGen->pIn++;` |
|     190 | 1193 | `	pGen->pEnd--;` |
|      94 | 1194 | `	SXUNUSED(iCompileFlag);` |
|     190 | 1195 | `	return GenStateCompileArrayBody(pGen);` |
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
|       - | 1342 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - | 1343 | `/*` |
|       - | 1344 | ` * Compile an annoynmous function or a closure.` |
|       - | 1345 | ` * According to the PHP language reference` |
|       - | 1346 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - | 1347 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - | 1348 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - | 1349 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - | 1350 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - | 1351 | ` *  Example Anonymous function variable assignment example` |
|       - | 1352 | ` * <?php` |
|       - | 1353 | ` * $greet = function($name)` |
|       - | 1354 | ` * {` |
|       - | 1355 | ` *    printf("Hello %s\r\n", $name);` |
|       - | 1356 | ` * };` |
|       - | 1357 | ` * $greet('World');` |
|       - | 1358 | ` * $greet('PHP');` |
|       - | 1359 | ` * ?>` |
|       - | 1360 | ` * Note that the implementation of annoynmous function and closure under` |
|       - | 1361 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - | 1362 | ` */` |
|     166 | 1363 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1364 |  |
|       - | 1365 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - | 1366 | `	char zName[512];         /* Unique lambda name */` |
|       - | 1367 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - | 1368 | `							  * one thread is allowed to compile the script.` |
|       - | 1369 | `						      */` |
|       - | 1370 | `	ph7_value *pObj;` |
|       - | 1371 | `	SyString sName;` |
|       - | 1372 | `	sxu32 nIdx;` |
|       - | 1373 | `	sxu32 nLen;` |
|       - | 1374 | `	sxi32 rc;` |
|       - | 1375 |  |
|     168 | 1376 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     168 | 1377 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 | 1378 | `		pGen->pIn++;` |
|     ! 0 | 1379 | `	}` |
|       - | 1380 | `	/* Reserve a constant for the lambda */` |
|     168 | 1381 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     168 | 1382 | `	if( pObj == 0 ){` |
|     ! 0 | 1383 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1384 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1385 | `		return SXERR_ABORT;` |
|       - | 1386 | `	}` |
|       - | 1387 | `	/* Generate a unique name */` |
|     168 | 1388 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - | 1389 | `	/* Make sure the generated name is unique */` |
|     168 | 1390 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 1391 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 | 1392 | `	}` |
|     168 | 1393 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     168 | 1394 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - | 1395 | `	/* Compile the lambda body */` |
|     168 | 1396 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     168 | 1397 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1398 | `		return SXERR_ABORT;` |
|       - | 1399 | `	}` |
|     168 | 1400 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1401 | `		/* Emit the load closure instruction */` |
|      14 | 1402 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       8 | 1403 | `	}else{` |
|       - | 1404 | `		/* Emit the load constant instruction */` |
|     156 | 1405 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1406 | `	}` |
|       - | 1407 | `	/* Node successfully compiled */` |
|     168 | 1408 | `	return SXRET_OK;` |
|      85 | 1409 |  |
|       - | 1410 | `/*` |
|       - | 1411 | ` * Compile a backtick quoted string.` |
|       - | 1412 | ` */` |
|       4 | 1413 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1414 |  |
|       - | 1415 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - | 1416 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - | 1417 | `	 */` |
|       7 | 1418 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - | 1419 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 | 1420 | `		ph7_lib_version()` |
|       - | 1421 | `		);` |
|       - | 1422 | `	/* Load NULL */` |
|       5 | 1423 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1424 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1425 | `	/* Node successfully compiled */` |
|       5 | 1426 | `	return SXRET_OK;` |
|       1 | 1427 |  |
|       - | 1428 | `/*` |
|       - | 1429 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - | 1430 | ` * construct.` |
|       - | 1431 | ` */` |
|      72 | 1432 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1433 |  |
|       - | 1434 | `	SyString *pName;` |
|       - | 1435 | `	sxu32 nKeyID;` |
|       - | 1436 | `	sxi32 rc;` |
|       - | 1437 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      74 | 1438 | `	pName = &pGen->pIn->sData;` |
|      74 | 1439 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      74 | 1440 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      74 | 1441 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 | 1442 | `		SyToken *pTmp,*pNext = 0;` |
|       - | 1443 | `		/* Compile arguments one after one */` |
|       9 | 1444 | `		pTmp = pGen->pEnd;` |
|       - | 1445 | `		/* Symisc eXtension to the PHP programming language:` |
|       - | 1446 | `		 * 'echo' can be used in the context of a function which` |
|       - | 1447 | `		 *  mean that the following expression is valid:` |
|       - | 1448 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - | 1449 | `		 */` |
|       9 | 1450 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 | 1451 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 | 1452 | `			if( pGen->pIn < pNext ){` |
|       9 | 1453 | `				pGen->pEnd = pNext;` |
|       9 | 1454 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 | 1455 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1456 | `					return SXERR_ABORT;` |
|       - | 1457 | `				}` |
|       9 | 1458 | `				if( rc != SXERR_EMPTY ){` |
|       - | 1459 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - | 1460 | `					 * without the overhead of a function call.` |
|       - | 1461 | `					 * This is a very powerful optimization that improve` |
|       - | 1462 | `					 * performance greatly.` |
|       - | 1463 | `					 */` |
|       9 | 1464 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 | 1465 | `				}` |
|       4 | 1466 | `			}` |
|       - | 1467 | `			/* Jump trailing commas */` |
|       9 | 1468 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 1469 | `				pNext++;` |
|     ! 0 | 1470 | `			}` |
|       9 | 1471 | `			pGen->pIn = pNext;` |
|       1 | 1472 | `		}` |
|       - | 1473 | `		/* Restore token stream */` |
|       9 | 1474 | `		pGen->pEnd = pTmp;` |
|       5 | 1475 | `	}else{` |
|      66 | 1476 | `		sxi32 nArg = 0;` |
|      66 | 1477 | `		sxu32 nIdx = 0;` |
|      66 | 1478 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      66 | 1479 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1480 | `			return SXERR_ABORT;` |
|      66 | 1481 | `		}else if(rc != SXERR_EMPTY ){` |
|      66 | 1482 | `			nArg = 1;` |
|      32 | 1483 | `		}` |
|      66 | 1484 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1485 | `			ph7_value *pObj;` |
|       - | 1486 | `			/* Emit the call instruction */` |
|      20 | 1487 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 | 1488 | `			if( pObj == 0 ){` |
|     ! 0 | 1489 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1490 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1491 | `				return SXERR_ABORT;` |
|       - | 1492 | `			}` |
|      20 | 1493 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1494 | `			/* Install in the literal table */` |
|      20 | 1495 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       9 | 1496 | `		}` |
|       - | 1497 | `		/* Emit the call instruction */` |
|      66 | 1498 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      66 | 1499 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - | 1500 | `	}` |
|       - | 1501 | `	/* Node successfully compiled */` |
|      74 | 1502 | `	return SXRET_OK;` |
|      38 | 1503 |  |
|       - | 1504 | `/*` |
|       - | 1505 | ` * Compile a node holding a variable declaration.` |
|       - | 1506 | ` * According to the PHP language reference` |
|       - | 1507 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 1508 | ` *  The variable name is case-sensitive.` |
|       - | 1509 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 1510 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1511 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 1512 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 1513 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 1514 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 1515 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 1516 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 1517 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 1518 | ` *  the chapter on Expressions.` |
|       - | 1519 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 1520 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 1521 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 1522 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 1523 | ` *  is being assigned (the source variable).` |
|       - | 1524 | ` */` |
|  757610 | 1525 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1526 |  |
|  757612 | 1527 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1528 | `	sxi32 iVv;` |
|       - | 1529 | `	sxi32 iP1;` |
|       - | 1530 | `	void *p3;` |
|       - | 1531 | `	sxi32 rc;` |
|  757612 | 1532 | `	iVv = -1; /* Variable variable counter */` |
| 1515234 | 1533 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  757624 | 1534 | `		pGen->pIn++;` |
|  757624 | 1535 | `		iVv++;` |
|       2 | 1536 | `	}` |
|  757612 | 1537 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1538 | `		/* Invalid variable name */` |
|     ! 0 | 1539 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 | 1540 | `		if( rc == SXERR_ABORT ){` |
|       - | 1541 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1542 | `			return SXERR_ABORT;` |
|       - | 1543 | `		}` |
|     ! 0 | 1544 | `		return SXRET_OK;` |
|       - | 1545 | `	}` |
|  757612 | 1546 | `	p3  = 0;` |
|  757612 | 1547 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 1548 | `		/* Dynamic variable creation */` |
|      18 | 1549 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 | 1550 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 | 1551 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1552 | `			/* Empty expression */` |
|       3 | 1553 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1554 | `			return SXRET_OK;` |
|       - | 1555 | `		}` |
|       - | 1556 | `		/* Compile the expression holding the variable name */` |
|      16 | 1557 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 | 1558 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1559 | `			return SXERR_ABORT;` |
|      16 | 1560 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 1561 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 | 1562 | `			return SXRET_OK;` |
|       - | 1563 | `		}` |
|       7 | 1564 | `	}else{` |
|       - | 1565 | `		SyHashEntry *pEntry;` |
|       - | 1566 | `		SyString *pName;` |
|  757596 | 1567 | `		char *zName = 0;` |
|       - | 1568 | `		/* Extract variable name */` |
|  757596 | 1569 | `		pName = &pGen->pIn->sData;` |
|       - | 1570 | `		/* Advance the stream cursor */` |
|  757596 | 1571 | `		pGen->pIn++;` |
|  757596 | 1572 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  757596 | 1573 | `		if( pEntry == 0 ){` |
|       - | 1574 | `			/* Duplicate name */` |
|  108932 | 1575 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  108932 | 1576 | `			if( zName == 0 ){` |
|     ! 0 | 1577 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1578 | `				return SXERR_ABORT;` |
|       - | 1579 | `			}` |
|       - | 1580 | `			/* Install in the hashtable */` |
|  108932 | 1581 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   54467 | 1582 | `		}else{` |
|       - | 1583 | `			/* Name already available */` |
|  648666 | 1584 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1585 | `		}` |
|  757596 | 1586 | `		p3 = (void *)zName;` |
|       - | 1587 | `	}` |
|  757608 | 1588 | `	iP1 = 0;` |
|  757608 | 1589 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  291420 | 1590 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1591 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  285488 | 1592 | `			iP1 = 1;` |
|  142743 | 1593 | `		}` |
|  145709 | 1594 | `	}` |
|       - | 1595 | `	/* Emit the load instruction */` |
|  757608 | 1596 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  757620 | 1597 | `	while( iVv > 0 ){` |
|      13 | 1598 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1599 | `		iVv--;` |
|       1 | 1600 | `	}` |
|       - | 1601 | `	/* Node successfully compiled */` |
|  757608 | 1602 | `	return SXRET_OK;` |
|  378807 | 1603 |  |
|       - | 1604 | `/*` |
|       - | 1605 | ` * Load a literal.` |
|       - | 1606 | ` */` |
|  508116 | 1607 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 1608 |  |
|  508118 | 1609 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1610 | `	ph7_value *pObj;` |
|       - | 1611 | `	SyString *pStr;` |
|       - | 1612 | `	sxu32 nIdx;` |
|       - | 1613 | `	/* Extract token value */` |
|  508118 | 1614 | `	pStr = &pToken->sData;` |
|       - | 1615 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  508118 | 1616 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   92246 | 1617 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1618 | `			/* NULL constant are always indexed at 0 */` |
|   39224 | 1619 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   39224 | 1620 | `			return SXRET_OK;` |
|   53024 | 1621 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1622 | `			/* TRUE constant are always indexed at 1 */` |
|     488 | 1623 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     488 | 1624 | `			return SXRET_OK;` |
|       2 | 1625 | `		}` |
|  482242 | 1626 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   80200 | 1627 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1628 | `			/* FALSE constant are always indexed at 2 */` |
|   34252 | 1629 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   34252 | 1630 | `			return SXRET_OK;` |
|  417027 | 1631 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   70806 | 1632 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1633 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5188 | 1634 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5188 | 1635 | `			if( pObj == 0 ){` |
|     ! 0 | 1636 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1637 | `				return SXERR_ABORT;` |
|       - | 1638 | `			}` |
|    5188 | 1639 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1640 | `			/* Emit the load constant instruction */` |
|    5188 | 1641 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5188 | 1642 | `			return SXRET_OK;` |
|  389509 | 1643 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   26142 | 1644 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - | 1645 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 | 1646 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 | 1647 | `			if( pObj == 0 ){` |
|     ! 0 | 1648 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1649 | `				return SXERR_ABORT;` |
|       - | 1650 | `			}` |
|       7 | 1651 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1652 | `				SyString sNs;` |
|       7 | 1653 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 | 1654 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 | 1655 | `			}else{` |
|     ! 0 | 1656 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - | 1657 | `			}` |
|       7 | 1658 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 | 1659 | `			return SXRET_OK;` |
|  388702 | 1660 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   10946 | 1661 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  383223 | 1662 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   13600 | 1663 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 | 1664 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 1665 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 | 1666 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - | 1667 | `				/* Point to the upper block */` |
|      11 | 1668 | `				pBlock = pBlock->pParent;` |
|       1 | 1669 | `			}` |
|      11 | 1670 | `			if( pBlock == 0 ){` |
|       - | 1671 | `				/* Called in the global scope,load NULL */` |
|       5 | 1672 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1673 | `			}else{` |
|       - | 1674 | `				/* Extract the target function/method */` |
|       7 | 1675 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 | 1676 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - | 1677 | `					/* Not a class method,Load null */` |
|       3 | 1678 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1679 | `				}else{` |
|       5 | 1680 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 | 1681 | `					if( pObj == 0 ){` |
|     ! 0 | 1682 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1683 | `						return SXERR_ABORT;` |
|       - | 1684 | `					}` |
|       5 | 1685 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - | 1686 | `					/* Emit the load constant instruction */` |
|       5 | 1687 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1688 | `				}` |
|       - | 1689 | `			}` |
|      11 | 1690 | `			return SXRET_OK;` |
|       - | 1691 | `	}` |
|       - | 1692 | `	/* Query literal table */` |
|  428958 | 1693 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 1694 | `		ph7_value *pLitObj;` |
|       - | 1695 | `		/* Unknown literal,install it in the literal table */` |
|  200644 | 1696 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  200644 | 1697 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1698 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1699 | `			return SXERR_ABORT;` |
|       - | 1700 | `		}` |
|  200644 | 1701 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  200644 | 1702 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  100321 | 1703 | `	}` |
|       - | 1704 | `	/* Emit the load constant instruction */` |
|  428958 | 1705 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  428958 | 1706 | `	return SXRET_OK;` |
|  254060 | 1707 |  |
|       - | 1708 | `/*` |
|       - | 1709 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1710 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1711 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1712 | ` * Otherwise, load the simple literal directly.` |
|       - | 1713 | ` */` |
|  508140 | 1714 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 1715 |  |
|       - | 1716 | `	sxi32 rc;` |
|  508142 | 1717 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1718 | `		return SXRET_OK;` |
|       - | 1719 | `	}` |
|       - | 1720 | `	/* Check if this is a multi-token namespace path */` |
|  508142 | 1721 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 1722 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      26 | 1723 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      26 | 1724 | `		int isAbsolute = 0;` |
|      26 | 1725 | `		SyBlobReset(pWorker);` |
|       - | 1726 | `		/* Check for leading backslash (absolute path) */` |
|      26 | 1727 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      24 | 1728 | `			isAbsolute = 1;` |
|      24 | 1729 | `			pGen->pIn++; /* Skip leading backslash */` |
|      11 | 1730 | `		}` |
|       - | 1731 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      26 | 1732 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 1733 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 1734 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 | 1735 | `		}` |
|       - | 1736 | `		/* Collect all path components */` |
|     102 | 1737 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     102 | 1738 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      40 | 1739 | `				SyBlobAppend(pWorker,"\\",1);` |
|      21 | 1740 | `			}else{` |
|      64 | 1741 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 1742 | `			}` |
|     102 | 1743 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      26 | 1744 | `				pGen->pIn++;` |
|      26 | 1745 | `				break;` |
|       - | 1746 | `			}` |
|      78 | 1747 | `			pGen->pIn++;` |
|       2 | 1748 | `		}` |
|      26 | 1749 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 1750 | `			ph7_value *pObj;` |
|       - | 1751 | `			SyString sPath;` |
|       - | 1752 | `			sxu32 nIdx;` |
|      26 | 1753 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 1754 | `			/* Install in the literal table */` |
|      26 | 1755 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      13 | 1756 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      13 | 1757 | `				if( pObj == 0 ){` |
|     ! 0 | 1758 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1759 | `					return SXERR_ABORT;` |
|       - | 1760 | `				}` |
|      13 | 1761 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      13 | 1762 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       6 | 1763 | `			}` |
|       - | 1764 | `			/* Emit the load constant instruction.` |
|       - | 1765 | `			 * P1=1 means candidate for constant/function/class expansion. */` |
|      26 | 1766 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      26 | 1767 | `			return SXRET_OK;` |
|       - | 1768 | `		}` |
|     ! 0 | 1769 | `	}` |
|       - | 1770 | `	/* Single-token literal: load directly */` |
|  508118 | 1771 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  508118 | 1772 | `	return rc;` |
|  254072 | 1773 |  |
|       - | 1774 | `/*` |
|       - | 1775 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1776 | ` */` |
|  508140 | 1777 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1778 |  |
|       - | 1779 | `	sxi32 rc;` |
|  508142 | 1780 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  508142 | 1781 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1782 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1783 | `		return rc;` |
|       - | 1784 | `	}` |
|       - | 1785 | `	/* Node successfully compiled */` |
|  508142 | 1786 | `	return SXRET_OK;` |
|  254072 | 1787 |  |
|       - | 1788 | `/*` |
|       - | 1789 | ` * Recover from a compile-time error. In other words synchronize` |
|       - | 1790 | ` * the token stream cursor with the first semi-colon seen.` |
|       - | 1791 | ` */` |
|       8 | 1792 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 | 1793 |  |
|       - | 1794 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 | 1795 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 | 1796 | `		pGen->pIn++;` |
|       1 | 1797 | `	}` |
|       9 | 1798 | `	return SXRET_OK;` |
|       1 | 1799 |  |
|       - | 1800 | `/*` |
|       - | 1801 | ` * Check if the given identifier name is reserved or not.` |
|       - | 1802 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - | 1803 | ` */` |
|      36 | 1804 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 | 1805 |  |
|      38 | 1806 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      12 | 1807 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 | 1808 | `			return TRUE;` |
|      10 | 1809 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 | 1810 | `			return TRUE;` |
|       1 | 1811 | `		}` |
|      30 | 1812 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 | 1813 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 | 1814 | `			return TRUE;` |
|       - | 1815 | `		}` |
|     ! 0 | 1816 | `	}` |
|       - | 1817 | `	/* Not a reserved constant */` |
|      30 | 1818 | `	return FALSE;` |
|      20 | 1819 |  |
|       - | 1820 | `/*` |
|       - | 1821 | ` * Compile the 'const' statement.` |
|       - | 1822 | ` * According to the PHP language reference` |
|       - | 1823 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - | 1824 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - | 1825 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - | 1826 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - | 1827 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1828 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - | 1829 | ` *  Syntax` |
|       - | 1830 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - | 1831 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - | 1832 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - | 1833 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - | 1834 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - | 1835 | ` *  to get a list of all defined constants.` |
|       - | 1836 | ` *` |
|       - | 1837 | ` * Symisc eXtension.` |
|       - | 1838 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - | 1839 | ` *  would allow only simple scalar value.` |
|       - | 1840 | ` *  Example` |
|       - | 1841 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 1842 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 1843 | ` */` |
|      32 | 1844 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 | 1845 |  |
|       - | 1846 | `	SySet *pConsCode,*pInstrContainer;` |
|      34 | 1847 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1848 | `	SyString *pName;` |
|       - | 1849 | `	sxi32 rc;` |
|      34 | 1850 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      34 | 1851 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 1852 | `		/* Invalid constant name */` |
|       7 | 1853 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 | 1854 | `		if( rc == SXERR_ABORT ){` |
|       - | 1855 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1856 | `			return SXERR_ABORT;` |
|       - | 1857 | `		}` |
|       7 | 1858 | `		goto Synchronize;` |
|       - | 1859 | `	}` |
|       - | 1860 | `	/* Peek constant name */` |
|      28 | 1861 | `	pName = &pGen->pIn->sData;` |
|       - | 1862 | `	/* Make sure the constant name isn't reserved */` |
|      28 | 1863 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 1864 | `		/* Reserved constant */` |
|       9 | 1865 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 | 1866 | `		if( rc == SXERR_ABORT ){` |
|       - | 1867 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1868 | `			return SXERR_ABORT;` |
|       - | 1869 | `		}` |
|       9 | 1870 | `		goto Synchronize;` |
|       - | 1871 | `	}` |
|      20 | 1872 | `	pGen->pIn++;` |
|      20 | 1873 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 1874 | `		/* Invalid statement*/` |
|       5 | 1875 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 | 1876 | `		if( rc == SXERR_ABORT ){` |
|       - | 1877 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1878 | `			return SXERR_ABORT;` |
|       - | 1879 | `		}` |
|       5 | 1880 | `		goto Synchronize;` |
|       - | 1881 | `	}` |
|      15 | 1882 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - | 1883 | `	/* Allocate a new constant value container */` |
|      15 | 1884 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 | 1885 | `	if( pConsCode == 0 ){` |
|     ! 0 | 1886 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1887 | `		return SXERR_ABORT;` |
|       - | 1888 | `	}` |
|      15 | 1889 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 1890 | `	/* Swap bytecode container */` |
|      15 | 1891 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 | 1892 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - | 1893 | `	/* Compile constant value */` |
|      15 | 1894 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 1895 | `	/* Emit the done instruction */` |
|      15 | 1896 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 | 1897 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 | 1898 | `	if( rc == SXERR_ABORT ){` |
|       - | 1899 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1900 | `		return SXERR_ABORT;` |
|       - | 1901 | `	}` |
|      15 | 1902 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - | 1903 | `	/* Register the constant with namespace-qualified name */` |
|       - | 1904 | `	{` |
|       - | 1905 | `		SyBlob sFQN;` |
|       - | 1906 | `		SyString sFQNStr;` |
|      15 | 1907 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 | 1908 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 | 1909 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 | 1910 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 | 1911 | `		SyBlobRelease(&sFQN);` |
|       - | 1912 | `	}` |
|      15 | 1913 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1914 | `		SySetRelease(pConsCode);` |
|     ! 0 | 1915 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 | 1916 | `	}` |
|      15 | 1917 | `	return SXRET_OK;` |
|       9 | 1918 | `Synchronize:` |
|       - | 1919 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 | 1920 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 | 1921 | `		pGen->pIn++;` |
|       1 | 1922 | `	}` |
|      19 | 1923 | `	return SXRET_OK;` |
|      18 | 1924 |  |
|       - | 1925 | `/*` |
|       - | 1926 | ` * Compile the 'continue' statement.` |
|       - | 1927 | ` * According to the PHP language reference` |
|       - | 1928 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - | 1929 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - | 1930 | ` *  iteration.` |
|       - | 1931 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - | 1932 | ` *  the purposes of continue.` |
|       - | 1933 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - | 1934 | ` *  of enclosing loops it should skip to the end of.` |
|       - | 1935 | ` *  Note:` |
|       - | 1936 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - | 1937 | ` */` |
|       - | 1938 | `/*` |
|       - | 1939 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - | 1940 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - | 1941 | ` * break/continue crosses a try boundary.` |
|       - | 1942 | ` *` |
|       - | 1943 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - | 1944 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - | 1945 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - | 1946 | ` */` |
|    2712 | 1947 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 | 1948 |  |
|    2714 | 1949 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   15834 | 1950 | `	while( pBlock && pBlock != pTarget ){` |
|   13122 | 1951 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 | 1952 | `			if( pBlock->pUserData ){` |
|       - | 1953 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 | 1954 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 | 1955 | `			}else{` |
|       - | 1956 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - | 1957 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - | 1958 | `				 * exception context from a sub-execution.` |
|       - | 1959 | `				 */` |
|     ! 0 | 1960 | `				break;` |
|       - | 1961 | `			}` |
|       1 | 1962 | `		}` |
|   13122 | 1963 | `		pBlock = pBlock->pParent;` |
|       2 | 1964 | `	}` |
|    2714 | 1965 |  |
|    2632 | 1966 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 1967 |  |
|       - | 1968 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1969 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1970 | `	sxu32 nLineLocal;` |
|       - | 1971 | `	sxi32 rc;` |
|    2634 | 1972 | `	nLineLocal = pGen->pIn->nLine;` |
|    2634 | 1973 | `	iLevel = 0;` |
|       - | 1974 | `	/* Jump the 'continue' keyword */` |
|    2634 | 1975 | `	pGen->pIn++;` |
|    2634 | 1976 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 1977 | `		/* optional numeric argument which tells us how many levels` |
|       - | 1978 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 1979 | `		 */` |
|      12 | 1980 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      12 | 1981 | `		if( iLevel < 2 ){` |
|     ! 0 | 1982 | `			iLevel = 0;` |
|     ! 0 | 1983 | `		}` |
|      12 | 1984 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 1985 | `	}` |
|       - | 1986 | `	/* Point to the target loop */` |
|    2634 | 1987 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2634 | 1988 | `	if( pLoop == 0 ){` |
|       - | 1989 | `		/* Illegal continue */` |
|      11 | 1990 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 1991 | `		if( rc == SXERR_ABORT ){` |
|       - | 1992 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1993 | `			return SXERR_ABORT;` |
|       - | 1994 | `		}` |
|       6 | 1995 | `	}else{` |
|    2624 | 1996 | `		sxu32 nInstrIdx = 0;` |
|       - | 1997 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2624 | 1998 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2624 | 1999 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - | 2000 | `			/* According to the PHP language reference manual` |
|       - | 2001 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - | 2002 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - | 2003 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - | 2004 | `			 */` |
|       5 | 2005 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 | 2006 | `			if( rc == SXRET_OK ){` |
|       5 | 2007 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 | 2008 | `			}` |
|       3 | 2009 | `		}else{` |
|       - | 2010 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    2620 | 2011 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2620 | 2012 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 2013 | `				JumpFixup sJumpFix;` |
|       - | 2014 | `				/* Post-continue */` |
|       9 | 2015 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       9 | 2016 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       9 | 2017 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       4 | 2018 | `			}` |
|       - | 2019 | `		}` |
|       - | 2020 | `	}` |
|    2634 | 2021 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2022 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2023 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 2024 | `	}` |
|       - | 2025 | `	/* Statement successfully compiled */` |
|    2634 | 2026 | `	return SXRET_OK;` |
|    1318 | 2027 |  |
|       - | 2028 | `/*` |
|       - | 2029 | ` * Compile the 'break' statement.` |
|       - | 2030 | ` * According to the PHP language reference` |
|       - | 2031 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - | 2032 | ` *  structure.` |
|       - | 2033 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - | 2034 | ` *  enclosing structures are to be broken out of.` |
|       - | 2035 | ` */` |
|     106 | 2036 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 | 2037 |  |
|       - | 2038 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2039 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2040 | `	sxi32 rc;` |
|     108 | 2041 | `	iLevel = 0;` |
|       - | 2042 | `	/* Jump the 'break' keyword */` |
|     108 | 2043 | `	pGen->pIn++;` |
|     108 | 2044 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 2045 | `		/* optional numeric argument which tells us how many levels` |
|       - | 2046 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 2047 | `		 */` |
|      12 | 2048 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      12 | 2049 | `		if( iLevel < 2 ){` |
|     ! 0 | 2050 | `			iLevel = 0;` |
|     ! 0 | 2051 | `		}` |
|      12 | 2052 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 2053 | `	}` |
|       - | 2054 | `	/* Extract the target loop */` |
|     108 | 2055 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     108 | 2056 | `	if( pLoop == 0 ){` |
|       - | 2057 | `		/* Illegal break */` |
|      17 | 2058 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 | 2059 | `		if( rc == SXERR_ABORT ){` |
|       - | 2060 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2061 | `			return SXERR_ABORT;` |
|       - | 2062 | `		}` |
|       9 | 2063 | `	}else{` |
|       - | 2064 | `		sxu32 nInstrIdx;` |
|       - | 2065 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|      92 | 2066 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|      92 | 2067 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      92 | 2068 | `		if( rc == SXRET_OK ){` |
|       - | 2069 | `			/* Fix the jump later when the jump destination is resolved */` |
|      92 | 2070 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      45 | 2071 | `		}` |
|       - | 2072 | `	}` |
|     108 | 2073 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2074 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2075 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 | 2076 | `	}` |
|       - | 2077 | `	/* Statement successfully compiled */` |
|     108 | 2078 | `	return SXRET_OK;` |
|      55 | 2079 |  |
|       - | 2080 | `/*` |
|       - | 2081 | ` * Compile or record a label.` |
|       - | 2082 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - | 2083 | ` * Example` |
|       - | 2084 | ` *  goto LABEL;` |
|       - | 2085 | ` *   echo 'Foo';` |
|       - | 2086 | ` *  LABEL:` |
|       - | 2087 | ` *   echo 'Bar';` |
|       - | 2088 | ` */` |
|     112 | 2089 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 | 2090 |  |
|       - | 2091 | `	GenBlock *pBlock;` |
|       - | 2092 | `	Label sLabel;` |
|       - | 2093 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 | 2094 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 | 2095 | `	if( pBlock ){` |
|       - | 2096 | `		sxi32 rc;` |
|       7 | 2097 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 | 2098 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 | 2099 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2100 | `			return SXERR_ABORT;` |
|       - | 2101 | `		}` |
|       3 | 2102 | `	}else{` |
|     110 | 2103 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 2104 | `		char *zDup;` |
|       - | 2105 | `		/* Initialize label fields */` |
|     110 | 2106 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2107 | `		/* Duplicate label name */` |
|     110 | 2108 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 | 2109 | `		if( zDup == 0 ){` |
|     ! 0 | 2110 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2111 | `			return SXERR_ABORT;` |
|       - | 2112 | `		}` |
|     110 | 2113 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 | 2114 | `		sLabel.bRef  = FALSE;` |
|     110 | 2115 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 | 2116 | `		pBlock = pGen->pCurrent;` |
|     218 | 2117 | `		while( pBlock ){` |
|     130 | 2118 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 | 2119 | `				break;` |
|       - | 2120 | `			}` |
|       - | 2121 | `			/* Point to the upper block */` |
|     110 | 2122 | `			pBlock = pBlock->pParent;` |
|       2 | 2123 | `		}` |
|     110 | 2124 | `		if( pBlock ){` |
|      22 | 2125 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 | 2126 | `		}else{` |
|      90 | 2127 | `			sLabel.pFunc = 0;` |
|       - | 2128 | `		}` |
|       - | 2129 | `		/* Insert in label set */` |
|     110 | 2130 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - | 2131 | `	}` |
|     114 | 2132 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 | 2133 | `	return SXRET_OK;` |
|      58 | 2134 |  |
|       - | 2135 | `/*` |
|       - | 2136 | ` * Compile the so hated 'goto' statement.` |
|       - | 2137 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - | 2138 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - | 2139 | ` * a compiler it has to do this.` |
|       - | 2140 | ` * According to the PHP language reference manual` |
|       - | 2141 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - | 2142 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - | 2143 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - | 2144 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - | 2145 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - | 2146 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - | 2147 | ` *   of a multi-level break` |
|       - | 2148 | ` */` |
|     152 | 2149 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 | 2150 |  |
|       - | 2151 | `	JumpFixup sJump;` |
|       - | 2152 | `	sxi32 rc;` |
|     154 | 2153 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 | 2154 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2155 | `		/* Missing label */` |
|     ! 0 | 2156 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 | 2157 | `		if( rc == SXERR_ABORT ){` |
|       - | 2158 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2159 | `			return SXERR_ABORT;` |
|       - | 2160 | `		}` |
|     ! 0 | 2161 | `		return SXRET_OK;` |
|       - | 2162 | `	}` |
|     154 | 2163 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 | 2164 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 | 2165 | `		if( rc == SXERR_ABORT ){` |
|       - | 2166 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2167 | `			return SXERR_ABORT;` |
|       - | 2168 | `		}` |
|       3 | 2169 | `	}else{` |
|     150 | 2170 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 2171 | `		GenBlock *pBlock;` |
|       - | 2172 | `		char *zDup;` |
|       - | 2173 | `		/* Prepare the jump destination */` |
|     150 | 2174 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 | 2175 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - | 2176 | `		/* Duplicate label name */` |
|     150 | 2177 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 | 2178 | `		if( zDup == 0 ){` |
|     ! 0 | 2179 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2180 | `			return SXERR_ABORT;` |
|       - | 2181 | `		}` |
|     150 | 2182 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 | 2183 | `		pBlock = pGen->pCurrent;` |
|     312 | 2184 | `		while( pBlock ){` |
|     196 | 2185 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 | 2186 | `				break;` |
|       - | 2187 | `			}` |
|       - | 2188 | `			/* Point to the upper block */` |
|     164 | 2189 | `			pBlock = pBlock->pParent;` |
|       2 | 2190 | `		}` |
|     150 | 2191 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 | 2192 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 | 2193 | `			if( rc == SXERR_ABORT ){` |
|       - | 2194 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2195 | `				return SXERR_ABORT;` |
|       - | 2196 | `			}` |
|       3 | 2197 | `		}` |
|     150 | 2198 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 | 2199 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 | 2200 | `		}else{` |
|     124 | 2201 | `			sJump.pFunc = 0;` |
|       - | 2202 | `		}` |
|       - | 2203 | `		/* Emit the unconditional jump */` |
|     150 | 2204 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 | 2205 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 | 2206 | `		}` |
|       - | 2207 | `	}` |
|     154 | 2208 | `	pGen->pIn++; /* Jump the label name */` |
|     154 | 2209 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 | 2210 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 | 2211 | `	}` |
|       - | 2212 | `	/* Statement successfully compiled */` |
|     154 | 2213 | `	return SXRET_OK;` |
|      78 | 2214 |  |
|       - | 2215 | `/*` |
|       - | 2216 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - | 2217 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - | 2218 | ` * failure.` |
|       - | 2219 | ` */` |
|      20 | 2220 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 | 2221 |  |
|       - | 2222 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - | 2223 | `	sxu32 nRawObj;` |
|      10 | 2224 | `	sxu32 nObjIdx;` |
|       - | 2225 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - | 2226 | `	 * a PHP block.` |
|       - | 2227 | `	 */` |
|      10 | 2228 | `Consume:` |
|      21 | 2229 | `	nRawObj = nObjIdx = 0;` |
|      21 | 2230 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 | 2231 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 | 2232 | `		if( pRawObj == 0 ){` |
|     ! 0 | 2233 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2234 | `			return SXERR_ABORT;` |
|       - | 2235 | `		}` |
|       - | 2236 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 | 2237 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 | 2238 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 | 2239 | `		++nRawObj;` |
|     ! 0 | 2240 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 | 2241 | `	}` |
|      21 | 2242 | `	if( nRawObj > 0 ){` |
|       - | 2243 | `		/* Emit the consume instruction */` |
|     ! 0 | 2244 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 | 2245 | `	}` |
|      21 | 2246 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 | 2247 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - | 2248 | `		/* Reset the token set */` |
|     ! 0 | 2249 | `		SySetReset(pTokenSet);` |
|       - | 2250 | `		/* Tokenize input */` |
|     ! 0 | 2251 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 | 2252 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - | 2253 | `		/* Point to the fresh token stream */` |
|     ! 0 | 2254 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 | 2255 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - | 2256 | `		/* Advance the stream cursor */` |
|     ! 0 | 2257 | `		pGen->pRawIn++;` |
|       - | 2258 | `		/* TICKET 1433-011 */` |
|     ! 0 | 2259 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 2260 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 2261 | `			sxi32 rc;` |
|       - | 2262 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 | 2263 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 | 2264 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 | 2265 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 | 2266 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 2267 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2268 | `				return SXERR_ABORT;` |
|     ! 0 | 2269 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 | 2270 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 2271 | `			}` |
|     ! 0 | 2272 | `			goto Consume;` |
|       - | 2273 | `		}` |
|     ! 0 | 2274 | `	}else{` |
|       - | 2275 | `		/* No more chunks to process */` |
|      21 | 2276 | `		pGen->pIn = pGen->pEnd;` |
|      21 | 2277 | `		return SXERR_EOF;` |
|       - | 2278 | `	}` |
|     ! 0 | 2279 | `	return SXRET_OK;` |
|      11 | 2280 |  |
|       - | 2281 | `/*` |
|       - | 2282 | ` * Compile a PHP block.` |
|       - | 2283 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - | 2284 | ` * optionally delimited by braces {}.` |
|       - | 2285 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 2286 | ` * and this function takes care of generating the appropriate error` |
|       - | 2287 | ` * message.` |
|       - | 2288 | ` */` |
|  285612 | 2289 | `static sxi32 PH7_CompileBlock(` |
|       - | 2290 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2291 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2292 | `	)` |
|       2 | 2293 |  |
|       - | 2294 | `	sxi32 rc;` |
|       - | 2295 | `	sxu32 nLine;` |
|  285614 | 2296 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  284220 | 2297 | `		nLine = pGen->pIn->nLine;` |
|  284220 | 2298 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  284220 | 2299 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2300 | `			return SXERR_ABORT;` |
|       - | 2301 | `		}` |
|  284220 | 2302 | `		pGen->pIn++;` |
|       - | 2303 | `		/* Compile until we hit the closing braces '}' */` |
|  392374 | 2304 | `		for(;;){` |
|  784750 | 2305 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 | 2306 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 | 2307 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2308 | `			 	   return SXERR_ABORT;` |
|       - | 2309 | `				}` |
|      21 | 2310 | `				if( rc == SXERR_EOF ){` |
|       - | 2311 | `					/* No more token to process. Missing closing braces */` |
|      21 | 2312 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 | 2313 | `					break;` |
|       - | 2314 | `				}` |
|     ! 0 | 2315 | `			}` |
|  784730 | 2316 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2317 | `				/* Closing braces found,break immediately*/` |
|  284200 | 2318 | `				pGen->pIn++;` |
|  284200 | 2319 | `				break;` |
|       - | 2320 | `			}` |
|       - | 2321 | `			/* Compile a single statement */` |
|  500532 | 2322 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  500532 | 2323 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2324 | `				return SXERR_ABORT;` |
|       - | 2325 | `			}` |
|       2 | 2326 | `		}` |
|  284220 | 2327 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  143505 | 2328 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 | 2329 | `		pGen->pIn++;` |
|     ! 0 | 2330 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 | 2331 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2332 | `			return SXERR_ABORT;` |
|       - | 2333 | `		}` |
|       - | 2334 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 | 2335 | `		for(;;){` |
|     ! 0 | 2336 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2337 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 | 2338 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2339 | `			 	   return SXERR_ABORT;` |
|       - | 2340 | `				}` |
|     ! 0 | 2341 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - | 2342 | `					/* No more token to process */` |
|     ! 0 | 2343 | `					if( rc == SXERR_EOF ){` |
|     ! 0 | 2344 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - | 2345 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 | 2346 | `					}` |
|     ! 0 | 2347 | `					break;` |
|       - | 2348 | `				}` |
|     ! 0 | 2349 | `			}` |
|     ! 0 | 2350 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 2351 | `				sxi32 nKwrd;` |
|       - | 2352 | `				/* Keyword found */` |
|     ! 0 | 2353 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 | 2354 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 | 2355 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - | 2356 | `						/* Delimiter keyword found,break */` |
|     ! 0 | 2357 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 | 2358 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 | 2359 | `						}` |
|     ! 0 | 2360 | `						break;` |
|       - | 2361 | `				}` |
|     ! 0 | 2362 | `			}` |
|       - | 2363 | `			/* Compile a single statement */` |
|     ! 0 | 2364 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 | 2365 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2366 | `				return SXERR_ABORT;` |
|       - | 2367 | `			}` |
|     ! 0 | 2368 | `		}` |
|     ! 0 | 2369 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 | 2370 | `	}else{` |
|       - | 2371 | `		/* Compile a single statement */` |
|    1396 | 2372 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1396 | 2373 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2374 | `			return SXERR_ABORT;` |
|       - | 2375 | `		}` |
|       - | 2376 | `	}` |
|       - | 2377 | `	/* Jump trailing semi-colons ';' */` |
|  285614 | 2378 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2379 | `		pGen->pIn++;` |
|     ! 0 | 2380 | `	}` |
|  285614 | 2381 | `	return SXRET_OK;` |
|  142808 | 2382 |  |
|       - | 2383 | `/*` |
|       - | 2384 | ` * Compile the gentle 'while' statement.` |
|       - | 2385 | ` * According to the PHP language reference` |
|       - | 2386 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - | 2387 | ` *  The basic form of a while statement is:` |
|       - | 2388 | ` *  while (expr)` |
|       - | 2389 | ` *   statement` |
|       - | 2390 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - | 2391 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - | 2392 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - | 2393 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - | 2394 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - | 2395 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - | 2396 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - | 2397 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - | 2398 | ` *  while (expr):` |
|       - | 2399 | ` *    statement` |
|       - | 2400 | ` *   endwhile;` |
|       - | 2401 | ` */` |
|   10470 | 2402 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 2403 |  |
|   10472 | 2404 | `	GenBlock *pWhileBlock = 0;` |
|   10472 | 2405 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2406 | `	sxu32 nFalseJump;` |
|       - | 2407 | `	sxu32 nLine;` |
|       - | 2408 | `	sxi32 rc;` |
|   10472 | 2409 | `	nLine = pGen->pIn->nLine;` |
|       - | 2410 | `	/* Jump the 'while' keyword */` |
|   10472 | 2411 | `	pGen->pIn++;` |
|   10472 | 2412 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2413 | `		/* Syntax error */` |
|     ! 0 | 2414 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2415 | `		if( rc == SXERR_ABORT ){` |
|       - | 2416 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2417 | `			return SXERR_ABORT;` |
|       - | 2418 | `		}` |
|     ! 0 | 2419 | `		goto Synchronize;` |
|       - | 2420 | `	}` |
|       - | 2421 | `	/* Jump the left parenthesis '(' */` |
|   10472 | 2422 | `	pGen->pIn++;` |
|       - | 2423 | `	/* Create the loop block */` |
|   10472 | 2424 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   10472 | 2425 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2426 | `		return SXERR_ABORT;` |
|       - | 2427 | `	}` |
|       - | 2428 | `	/* Delimit the condition */` |
|   10472 | 2429 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10472 | 2430 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2431 | `		/* Empty expression */` |
|       3 | 2432 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2433 | `		if( rc == SXERR_ABORT ){` |
|       - | 2434 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2435 | `			return SXERR_ABORT;` |
|       - | 2436 | `		}` |
|       1 | 2437 | `	}` |
|       - | 2438 | `	/* Swap token streams */` |
|   10472 | 2439 | `	pTmp = pGen->pEnd;` |
|   10472 | 2440 | `	pGen->pEnd = pEnd;` |
|       - | 2441 | `	/* Compile the expression */` |
|   10472 | 2442 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10472 | 2443 | `	if( rc == SXERR_ABORT ){` |
|       - | 2444 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2445 | `		return SXERR_ABORT;` |
|       - | 2446 | `	}` |
|       - | 2447 | `	/* Update token stream */` |
|   10472 | 2448 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2449 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2450 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2451 | `			return SXERR_ABORT;` |
|       - | 2452 | `		}` |
|     ! 0 | 2453 | `		pGen->pIn++;` |
|     ! 0 | 2454 | `	}` |
|       - | 2455 | `	/* Synchronize pointers */` |
|   10472 | 2456 | `	pGen->pIn  = &pEnd[1];` |
|   10472 | 2457 | `	pGen->pEnd = pTmp;` |
|       - | 2458 | `	/* Emit the false jump */` |
|   10472 | 2459 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2460 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10472 | 2461 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2462 | `	/* Compile the loop body */` |
|   10472 | 2463 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   10472 | 2464 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2465 | `		return SXERR_ABORT;` |
|       - | 2466 | `	}` |
|       - | 2467 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10472 | 2468 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2469 | `	/* Fix all jumps now the destination is resolved */` |
|   10472 | 2470 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2471 | `	/* Release the loop block */` |
|   10472 | 2472 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2473 | `	/* Statement successfully compiled */` |
|   10472 | 2474 | `	return SXRET_OK;` |
|     ! 0 | 2475 | `Synchronize:` |
|       - | 2476 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2477 | `	 * compiling this erroneous block.` |
|       - | 2478 | `	 */` |
|     ! 0 | 2479 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2480 | `		pGen->pIn++;` |
|     ! 0 | 2481 | `	}` |
|     ! 0 | 2482 | `	return SXRET_OK;` |
|    5237 | 2483 |  |
|       - | 2484 | `/*` |
|       - | 2485 | ` * Compile the ugly do..while() statement.` |
|       - | 2486 | ` * According to the PHP language reference` |
|       - | 2487 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - | 2488 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - | 2489 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - | 2490 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - | 2491 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - | 2492 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - | 2493 | ` *  would end immediately).` |
|       - | 2494 | ` *  There is just one syntax for do-while loops:` |
|       - | 2495 | ` *  <?php` |
|       - | 2496 | ` *  $i = 0;` |
|       - | 2497 | ` *  do {` |
|       - | 2498 | ` *   echo $i;` |
|       - | 2499 | ` *  } while ($i > 0);` |
|       - | 2500 | ` * ?>` |
|       - | 2501 | ` */` |
|       2 | 2502 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 | 2503 |  |
|       3 | 2504 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 | 2505 | `	GenBlock *pDoBlock = 0;` |
|       - | 2506 | `	sxu32 nLine;` |
|       - | 2507 | `	sxi32 rc;` |
|       3 | 2508 | `	nLine = pGen->pIn->nLine;` |
|       - | 2509 | `	/* Jump the 'do' keyword */` |
|       3 | 2510 | `	pGen->pIn++;` |
|       - | 2511 | `	/* Create the loop block */` |
|       3 | 2512 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 | 2513 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2514 | `		return SXERR_ABORT;` |
|       - | 2515 | `	}` |
|       - | 2516 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 | 2517 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 | 2518 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 | 2519 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2520 | `		return SXERR_ABORT;` |
|       - | 2521 | `	}` |
|       3 | 2522 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2523 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 | 2524 | `	}` |
|       3 | 2525 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 | 2526 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - | 2527 | `			/* Missing 'while' statement */` |
|       3 | 2528 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 | 2529 | `			if( rc == SXERR_ABORT ){` |
|       - | 2530 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2531 | `				return SXERR_ABORT;` |
|       - | 2532 | `			}` |
|       3 | 2533 | `			goto Synchronize;` |
|       - | 2534 | `	}` |
|       - | 2535 | `	/* Jump the 'while' keyword */` |
|     ! 0 | 2536 | `	pGen->pIn++;` |
|     ! 0 | 2537 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2538 | `		/* Syntax error */` |
|     ! 0 | 2539 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2540 | `		if( rc == SXERR_ABORT ){` |
|       - | 2541 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2542 | `			return SXERR_ABORT;` |
|       - | 2543 | `		}` |
|     ! 0 | 2544 | `		goto Synchronize;` |
|       - | 2545 | `	}` |
|       - | 2546 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 | 2547 | `	pGen->pIn++;` |
|       - | 2548 | `	/* Delimit the condition */` |
|     ! 0 | 2549 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 | 2550 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2551 | `		/* Empty expression */` |
|     ! 0 | 2552 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 | 2553 | `		if( rc == SXERR_ABORT ){` |
|       - | 2554 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2555 | `			return SXERR_ABORT;` |
|       - | 2556 | `		}` |
|     ! 0 | 2557 | `		goto Synchronize;` |
|       - | 2558 | `	}` |
|       - | 2559 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 | 2560 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - | 2561 | `		JumpFixup *aPost;` |
|       - | 2562 | `		VmInstr *pInstr;` |
|       - | 2563 | `		sxu32 nJumpDest;` |
|       - | 2564 | `		sxu32 n;` |
|     ! 0 | 2565 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 | 2566 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 | 2567 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 | 2568 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 | 2569 | `			if( pInstr ){` |
|       - | 2570 | `				/* Fix */` |
|     ! 0 | 2571 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 | 2572 | `			}` |
|     ! 0 | 2573 | `		}` |
|     ! 0 | 2574 | `	}` |
|       - | 2575 | `	/* Swap token streams */` |
|     ! 0 | 2576 | `	pTmp = pGen->pEnd;` |
|     ! 0 | 2577 | `	pGen->pEnd = pEnd;` |
|       - | 2578 | `	/* Compile the expression */` |
|     ! 0 | 2579 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 2580 | `	if( rc == SXERR_ABORT ){` |
|       - | 2581 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2582 | `		return SXERR_ABORT;` |
|       - | 2583 | `	}` |
|       - | 2584 | `	/* Update token stream */` |
|     ! 0 | 2585 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2586 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2587 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2588 | `			return SXERR_ABORT;` |
|       - | 2589 | `		}` |
|     ! 0 | 2590 | `		pGen->pIn++;` |
|     ! 0 | 2591 | `	}` |
|     ! 0 | 2592 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 | 2593 | `	pGen->pEnd = pTmp;` |
|       - | 2594 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 | 2595 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - | 2596 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 | 2597 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2598 | `	/* Release the loop block */` |
|     ! 0 | 2599 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2600 | `	/* Statement successfully compiled */` |
|     ! 0 | 2601 | `	return SXRET_OK;` |
|       1 | 2602 | `Synchronize:` |
|       - | 2603 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2604 | `	 * compiling this erroneous block.` |
|       - | 2605 | `	 */` |
|       3 | 2606 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2607 | `		pGen->pIn++;` |
|     ! 0 | 2608 | `	}` |
|       3 | 2609 | `	return SXRET_OK;` |
|       2 | 2610 |  |
|       - | 2611 | `/*` |
|       - | 2612 | ` * Compile the complex and powerful 'for' statement.` |
|       - | 2613 | ` * According to the PHP language reference` |
|       - | 2614 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - | 2615 | ` *  The syntax of a for loop is:` |
|       - | 2616 | ` *  for (expr1; expr2; expr3)` |
|       - | 2617 | ` *   statement` |
|       - | 2618 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - | 2619 | ` *  the beginning of the loop.` |
|       - | 2620 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - | 2621 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - | 2622 | ` *  to FALSE, the execution of the loop ends.` |
|       - | 2623 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - | 2624 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - | 2625 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - | 2626 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - | 2627 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - | 2628 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - | 2629 | ` *  of using the for truth expression.` |
|       - | 2630 | ` */` |
|   10454 | 2631 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 2632 |  |
|   10456 | 2633 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   10456 | 2634 | `	GenBlock *pForBlock = 0;` |
|       - | 2635 | `	sxu32 nFalseJump;` |
|       - | 2636 | `	sxu32 nLine;` |
|       - | 2637 | `	sxi32 rc;` |
|   10456 | 2638 | `	nLine = pGen->pIn->nLine;` |
|       - | 2639 | `	/* Jump the 'for' keyword */` |
|   10456 | 2640 | `	pGen->pIn++;` |
|   10456 | 2641 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2642 | `		/* Syntax error */` |
|     ! 0 | 2643 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2644 | `		if( rc == SXERR_ABORT ){` |
|       - | 2645 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2646 | `			return SXERR_ABORT;` |
|       - | 2647 | `		}` |
|     ! 0 | 2648 | `		return SXRET_OK;` |
|       - | 2649 | `	}` |
|       - | 2650 | `	/* Jump the left parenthesis '(' */` |
|   10456 | 2651 | `	pGen->pIn++;` |
|       - | 2652 | `	/* Delimit the init-expr;condition;post-expr */` |
|   10456 | 2653 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10456 | 2654 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2655 | `		/* Empty expression */` |
|     ! 0 | 2656 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 | 2657 | `		if( rc == SXERR_ABORT ){` |
|       - | 2658 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2659 | `			return SXERR_ABORT;` |
|       - | 2660 | `		}` |
|       - | 2661 | `		/* Synchronize */` |
|     ! 0 | 2662 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2663 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2664 | `			pGen->pIn++;` |
|     ! 0 | 2665 | `		}` |
|     ! 0 | 2666 | `		return SXRET_OK;` |
|       - | 2667 | `	}` |
|       - | 2668 | `	/* Swap token streams */` |
|   10456 | 2669 | `	pTmp = pGen->pEnd;` |
|   10456 | 2670 | `	pGen->pEnd = pEnd;` |
|       - | 2671 | `	/* Compile initialization expressions if available */` |
|   10456 | 2672 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2673 | `	/* Pop operand lvalues */` |
|   10456 | 2674 | `	if( rc == SXERR_ABORT ){` |
|       - | 2675 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2676 | `		return SXERR_ABORT;` |
|   10456 | 2677 | `	}else if( rc != SXERR_EMPTY ){` |
|   10454 | 2678 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5226 | 2679 | `	}` |
|   10456 | 2680 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2681 | `		/* Syntax error */` |
|     ! 0 | 2682 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2683 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 | 2684 | `		if( rc == SXERR_ABORT ){` |
|       - | 2685 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2686 | `			return SXERR_ABORT;` |
|       - | 2687 | `		}` |
|     ! 0 | 2688 | `		return SXRET_OK;` |
|       - | 2689 | `	}` |
|       - | 2690 | `	/* Jump the trailing ';' */` |
|   10456 | 2691 | `	pGen->pIn++;` |
|       - | 2692 | `	/* Create the loop block */` |
|   10456 | 2693 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   10456 | 2694 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2695 | `		return SXERR_ABORT;` |
|       - | 2696 | `	}` |
|       - | 2697 | `	/* Deffer continue jumps */` |
|   10456 | 2698 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 2699 | `	/* Compile the condition */` |
|   10456 | 2700 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10456 | 2701 | `	if( rc == SXERR_ABORT ){` |
|       - | 2702 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2703 | `		return SXERR_ABORT;` |
|   10456 | 2704 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 2705 | `		/* Emit the false jump */` |
|   10454 | 2706 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2707 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10454 | 2708 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5226 | 2709 | `	}` |
|   10456 | 2710 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2711 | `		/* Syntax error */` |
|       5 | 2712 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2713 | `			"for: Expected ';' after conditionals expressions");` |
|       5 | 2714 | `		if( rc == SXERR_ABORT ){` |
|       - | 2715 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2716 | `			return SXERR_ABORT;` |
|       - | 2717 | `		}` |
|       5 | 2718 | `		return SXRET_OK;` |
|       - | 2719 | `	}` |
|       - | 2720 | `	/* Jump the trailing ';' */` |
|   10452 | 2721 | `	pGen->pIn++;` |
|       - | 2722 | `	/* Save the post condition stream */` |
|   10452 | 2723 | `	pPostStart = pGen->pIn;` |
|       - | 2724 | `	/* Compile the loop body */` |
|   10452 | 2725 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   10452 | 2726 | `	pGen->pEnd = pTmp;` |
|   10452 | 2727 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   10452 | 2728 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2729 | `		return SXERR_ABORT;` |
|       - | 2730 | `	}` |
|       - | 2731 | `	/* Fix post-continue jumps */` |
|   10452 | 2732 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 2733 | `		JumpFixup *aPost;` |
|       - | 2734 | `		VmInstr *pInstr;` |
|       - | 2735 | `		sxu32 nJumpDest;` |
|       - | 2736 | `		sxu32 n;` |
|       9 | 2737 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|       9 | 2738 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      17 | 2739 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|       9 | 2740 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       9 | 2741 | `			if( pInstr ){` |
|       - | 2742 | `				/* Fix jump */` |
|       9 | 2743 | `				pInstr->iP2 = nJumpDest;` |
|       4 | 2744 | `			}` |
|       5 | 2745 | `		}` |
|       4 | 2746 | `	}` |
|       - | 2747 | `	/* compile the post-expressions if available */` |
|   10452 | 2748 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2749 | `		pPostStart++;` |
|     ! 0 | 2750 | `	}` |
|   10452 | 2751 | `	if( pPostStart < pEnd ){` |
|       - | 2752 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   10452 | 2753 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   10452 | 2754 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10452 | 2755 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 2756 | `			/* Syntax error */` |
|     ! 0 | 2757 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 2758 | `			if( rc == SXERR_ABORT ){` |
|       - | 2759 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2760 | `				return SXERR_ABORT;` |
|       - | 2761 | `			}` |
|     ! 0 | 2762 | `			return SXRET_OK;` |
|       - | 2763 | `		}` |
|   10452 | 2764 | `		RE_SWAP_DELIMITER(pGen);` |
|   10452 | 2765 | `		if( rc == SXERR_ABORT ){` |
|       - | 2766 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2767 | `			return SXERR_ABORT;` |
|   10452 | 2768 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 2769 | `			/* Pop operand lvalue */` |
|   10452 | 2770 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5225 | 2771 | `		}` |
|    5225 | 2772 | `	}` |
|       - | 2773 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10452 | 2774 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 2775 | `	/* Fix all jumps now the destination is resolved */` |
|   10452 | 2776 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2777 | `	/* Release the loop block */` |
|   10452 | 2778 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2779 | `	/* Statement successfully compiled */` |
|   10452 | 2780 | `	return SXRET_OK;` |
|    5229 | 2781 |  |
|       - | 2782 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 2783 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 2784 | ` * are allowed.` |
|       - | 2785 | ` */` |
|    5580 | 2786 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 2787 |  |
|    5582 | 2788 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    5582 | 2789 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 2790 | `		/* Unexpected expression */` |
|     ! 0 | 2791 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 2792 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 2793 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 2794 | `			rc = SXERR_INVALID;` |
|     ! 0 | 2795 | `		}` |
|     ! 0 | 2796 | `	}` |
|    5582 | 2797 | `	return rc;` |
|       2 | 2798 |  |
|       - | 2799 | `/*` |
|       - | 2800 | ` * Compile the 'foreach' statement.` |
|       - | 2801 | ` * According to the PHP language reference` |
|       - | 2802 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - | 2803 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - | 2804 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - | 2805 | ` *  is a minor but useful extension of the first:` |
|       - | 2806 | ` *  foreach (array_expression as $value)` |
|       - | 2807 | ` *    statement` |
|       - | 2808 | ` *  foreach (array_expression as $key => $value)` |
|       - | 2809 | ` *   statement` |
|       - | 2810 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - | 2811 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - | 2812 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - | 2813 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - | 2814 | ` *  to the variable $key on each loop.` |
|       - | 2815 | ` *  Note:` |
|       - | 2816 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - | 2817 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - | 2818 | ` *  Note:` |
|       - | 2819 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - | 2820 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - | 2821 | ` *  or after the foreach without resetting it.` |
|       - | 2822 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - | 2823 | ` *  of copying the value.` |
|       - | 2824 | ` */` |
|    2838 | 2825 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 2826 |  |
|    2840 | 2827 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    2840 | 2828 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    2840 | 2829 | `	GenBlock *pForeachBlock = 0;` |
|       - | 2830 | `	ph7_foreach_info *pInfo;` |
|       - | 2831 | `	sxu32 nFalseJump;` |
|       - | 2832 | `	VmInstr *pInstr;` |
|       - | 2833 | `	sxu32 nLine;` |
|       - | 2834 | `	sxi32 rc;` |
|    2840 | 2835 | `	nLine = pGen->pIn->nLine;` |
|       - | 2836 | `	/* Jump the 'foreach' keyword */` |
|    2840 | 2837 | `	pGen->pIn++;` |
|    2840 | 2838 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2839 | `		/* Syntax error */` |
|     ! 0 | 2840 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 2841 | `		if( rc == SXERR_ABORT ){` |
|       - | 2842 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2843 | `			return SXERR_ABORT;` |
|       - | 2844 | `		}` |
|     ! 0 | 2845 | `		goto Synchronize;` |
|       - | 2846 | `	}` |
|       - | 2847 | `	/* Jump the left parenthesis '(' */` |
|    2840 | 2848 | `	pGen->pIn++;` |
|       - | 2849 | `	/* Create the loop block */` |
|    2840 | 2850 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    2840 | 2851 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2852 | `		return SXERR_ABORT;` |
|       - | 2853 | `	}` |
|       - | 2854 | `	/* Delimit the expression */` |
|    2840 | 2855 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    2840 | 2856 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2857 | `		/* Empty expression */` |
|     ! 0 | 2858 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 2859 | `		if( rc == SXERR_ABORT ){` |
|       - | 2860 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2861 | `			return SXERR_ABORT;` |
|       - | 2862 | `		}` |
|       - | 2863 | `		/* Synchronize */` |
|     ! 0 | 2864 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2865 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2866 | `			pGen->pIn++;` |
|     ! 0 | 2867 | `		}` |
|     ! 0 | 2868 | `		return SXRET_OK;` |
|       - | 2869 | `	}` |
|       - | 2870 | `	/* Compile the array expression */` |
|    2840 | 2871 | `	pCur = pGen->pIn;` |
|   18990 | 2872 | `	while( pCur < pEnd ){` |
|   18990 | 2873 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    2850 | 2874 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    2850 | 2875 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 2876 | `				/* Break with the first 'as' found */` |
|    2840 | 2877 | `				break;` |
|       - | 2878 | `			}` |
|       5 | 2879 | `		}` |
|       - | 2880 | `		/* Advance the stream cursor */` |
|   16152 | 2881 | `		pCur++;` |
|       2 | 2882 | `	}` |
|    2840 | 2883 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 2884 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2885 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 2886 | `		if( rc == SXERR_ABORT ){` |
|       - | 2887 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2888 | `			return SXERR_ABORT;` |
|       - | 2889 | `		}` |
|     ! 0 | 2890 | `		goto Synchronize;` |
|       - | 2891 | `	}` |
|       - | 2892 | `	/* Swap token streams */` |
|    2840 | 2893 | `	pTmp = pGen->pEnd;` |
|    2840 | 2894 | `	pGen->pEnd = pCur;` |
|    2840 | 2895 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    2840 | 2896 | `	if( rc == SXERR_ABORT ){` |
|       - | 2897 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2898 | `		return SXERR_ABORT;` |
|       - | 2899 | `	}` |
|       - | 2900 | `	/* Update token stream */` |
|    2840 | 2901 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 2902 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2903 | `		if( rc == SXERR_ABORT ){` |
|       - | 2904 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2905 | `			return SXERR_ABORT;` |
|       - | 2906 | `		}` |
|     ! 0 | 2907 | `		pGen->pIn++;` |
|     ! 0 | 2908 | `	}` |
|    2840 | 2909 | `	pCur++; /* Jump the 'as' keyword */` |
|    2840 | 2910 | `	pGen->pIn = pCur;` |
|    2840 | 2911 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2912 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 2913 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2914 | `			return SXERR_ABORT;` |
|       - | 2915 | `		}` |
|     ! 0 | 2916 | `	}` |
|       - | 2917 | `	/* Create the foreach context */` |
|    2840 | 2918 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    2840 | 2919 | `	if( pInfo == 0 ){` |
|     ! 0 | 2920 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 2921 | `		return SXERR_ABORT;` |
|       - | 2922 | `	}` |
|       - | 2923 | `	/* Zero the structure */` |
|    2840 | 2924 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 2925 | `	/* Initialize structure fields */` |
|    2840 | 2926 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 2927 | `	/* Check if we have a key field */` |
|    8556 | 2928 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    5718 | 2929 | `		pCur++;` |
|       2 | 2930 | `	}` |
|    2840 | 2931 | `	if( pCur < pEnd ){` |
|       - | 2932 | `		/* Compile the expression holding the key name */` |
|    2752 | 2933 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 2934 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 2935 | `			if( rc == SXERR_ABORT ){` |
|       - | 2936 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2937 | `				return SXERR_ABORT;` |
|       - | 2938 | `			}` |
|     ! 0 | 2939 | `		}else{` |
|    2752 | 2940 | `			pGen->pEnd = pCur;` |
|    2752 | 2941 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2752 | 2942 | `			if( rc == SXERR_ABORT ){` |
|       - | 2943 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2944 | `				return SXERR_ABORT;` |
|       - | 2945 | `			}` |
|    2752 | 2946 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2752 | 2947 | `			if( pInstr->p3 ){` |
|       - | 2948 | `				/* Record key name */` |
|    2752 | 2949 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1375 | 2950 | `			}` |
|    2752 | 2951 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 2952 | `		}` |
|    2752 | 2953 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1375 | 2954 | `	}` |
|    2840 | 2955 | `	pGen->pEnd = pEnd;` |
|    2840 | 2956 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2957 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 2958 | `		if( rc == SXERR_ABORT ){` |
|       - | 2959 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2960 | `			return SXERR_ABORT;` |
|       - | 2961 | `		}` |
|     ! 0 | 2962 | `		goto Synchronize;` |
|       - | 2963 | `	}` |
|    2840 | 2964 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 | 2965 | `		pGen->pIn++;` |
|       - | 2966 | `		/* Pass by reference  */` |
|      11 | 2967 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 | 2968 | `	}` |
|       - | 2969 | `	/* Check if the value target is list() */` |
|    2840 | 2970 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 2971 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 2972 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - | 2973 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - | 2974 | `		 */` |
|       - | 2975 | `		static int iForeachListCnt = 0;` |
|       - | 2976 | `		char zTmp[128];` |
|       - | 2977 | `		sxu32 nLen;` |
|       - | 2978 | `		char *zDup;` |
|      10 | 2979 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 | 2980 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 | 2981 | `		if( zDup == 0 ){` |
|     ! 0 | 2982 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2983 | `			return SXERR_ABORT;` |
|       - | 2984 | `		}` |
|      10 | 2985 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 2986 | `		/* Save list() token boundaries */` |
|      10 | 2987 | `		pListStart = pGen->pIn;` |
|       - | 2988 | `		/* Advance past list(...) — validate parentheses */` |
|      10 | 2989 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 | 2990 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 | 2991 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - | 2992 | `				"foreach: Expected '(' after 'list'");` |
|       3 | 2993 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2994 | `				return SXERR_ABORT;` |
|       - | 2995 | `			}` |
|       3 | 2996 | `			goto Synchronize;` |
|       - | 2997 | `		}` |
|       7 | 2998 | `		pGen->pIn++; /* Jump '(' */` |
|       7 | 2999 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 | 3000 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 3001 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3002 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 | 3003 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3004 | `				return SXERR_ABORT;` |
|       - | 3005 | `			}` |
|     ! 0 | 3006 | `			goto Synchronize;` |
|       - | 3007 | `		}` |
|       7 | 3008 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 | 3009 | `		pListEnd = pGen->pIn;` |
|       7 | 3010 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       4 | 3011 | `	}else{` |
|       - | 3012 | `		/* Compile the expression holding the value name */` |
|    2832 | 3013 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2832 | 3014 | `		if( rc == SXERR_ABORT ){` |
|       - | 3015 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3016 | `			return SXERR_ABORT;` |
|       - | 3017 | `		}` |
|    2832 | 3018 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2832 | 3019 | `		if( pInstr->p3 ){` |
|       - | 3020 | `			/* Record value name */` |
|    2832 | 3021 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1415 | 3022 | `		}` |
|       - | 3023 | `	}` |
|       - | 3024 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    2838 | 3025 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 3026 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2838 | 3027 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 3028 | `	/* Record the first instruction to execute */` |
|    2838 | 3029 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3030 | `	/* Emit the FOREACH_STEP instruction */` |
|    2838 | 3031 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 3032 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2838 | 3033 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 3034 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    2838 | 3035 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - | 3036 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - | 3037 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - | 3038 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - | 3039 | `		 */` |
|       7 | 3040 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - | 3041 | `		/* Compile list(...) body directly — this pushes variables and emits LOAD_LIST.` |
|       - | 3042 | `		 * We position the tokens at the list keyword so PH7_CompileList picks up` |
|       - | 3043 | `		 * the opening '(' and the variable names inside.` |
|       - | 3044 | `		 */` |
|       7 | 3045 | `		pSavedIn = pGen->pIn;` |
|       7 | 3046 | `		pSavedEnd = pGen->pEnd;` |
|       7 | 3047 | `		pGen->pIn = pListStart;` |
|       7 | 3048 | `		pGen->pEnd = pListEnd;` |
|       7 | 3049 | `		rc = PH7_CompileList(&(*pGen),0);` |
|       7 | 3050 | `		pGen->pIn = pSavedIn;` |
|       7 | 3051 | `		pGen->pEnd = pSavedEnd;` |
|       7 | 3052 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3053 | `			return SXERR_ABORT;` |
|       - | 3054 | `		}` |
|       - | 3055 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       7 | 3056 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       3 | 3057 | `	}` |
|       - | 3058 | `	/* Compile the loop body */` |
|    2838 | 3059 | `	pGen->pIn = &pEnd[1];` |
|    2838 | 3060 | `	pGen->pEnd = pTmp;` |
|    2838 | 3061 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    2838 | 3062 | `	if( rc == SXERR_ABORT ){` |
|       - | 3063 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3064 | `		return SXERR_ABORT;` |
|       - | 3065 | `	}` |
|       - | 3066 | `	/* Emit the unconditional jump to the start of the loop */` |
|    2838 | 3067 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 3068 | `	/* Fix all jumps now the destination is resolved */` |
|    2838 | 3069 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3070 | `	/* Release the loop block */` |
|    2838 | 3071 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3072 | `	/* Statement successfully compiled */` |
|    2838 | 3073 | `	return SXRET_OK;` |
|       1 | 3074 | `Synchronize:` |
|       - | 3075 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 3076 | `	 * compiling this erroneous block.` |
|       - | 3077 | `	 */` |
|       3 | 3078 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3079 | `		pGen->pIn++;` |
|     ! 0 | 3080 | `	}` |
|       3 | 3081 | `	return SXRET_OK;` |
|    1421 | 3082 |  |
|       - | 3083 | `/*` |
|       - | 3084 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 3085 | ` * According to the PHP language reference` |
|       - | 3086 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 3087 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 3088 | ` *  that is similar to that of C:` |
|       - | 3089 | ` *  if (expr)` |
|       - | 3090 | ` *   statement` |
|       - | 3091 | ` *  else construct:` |
|       - | 3092 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 3093 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 3094 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 3095 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 3096 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 3097 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 3098 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 3099 | ` *  elseif` |
|       - | 3100 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 3101 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 3102 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 3103 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 3104 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 3105 | ` *   <?php` |
|       - | 3106 | ` *    if ($a > $b) {` |
|       - | 3107 | ` *     echo "a is bigger than b";` |
|       - | 3108 | ` *    } elseif ($a == $b) {` |
|       - | 3109 | ` *     echo "a is equal to b";` |
|       - | 3110 | ` *    } else {` |
|       - | 3111 | ` *     echo "a is smaller than b";` |
|       - | 3112 | ` *    }` |
|       - | 3113 | ` *    ?>` |
|       - | 3114 | ` */` |
|  104118 | 3115 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 3116 |  |
|  104120 | 3117 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  104120 | 3118 | `	GenBlock *pCondBlock = 0;` |
|       - | 3119 | `	sxu32 nJumpIdx;` |
|       - | 3120 | `	sxu32 nKeyID;` |
|       - | 3121 | `	sxi32 rc;` |
|       - | 3122 | `	/* Jump the 'if' keyword */` |
|  104120 | 3123 | `	pGen->pIn++;` |
|  104120 | 3124 | `	pToken = pGen->pIn;` |
|       - | 3125 | `	/* Create the conditional block */` |
|  104120 | 3126 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  104120 | 3127 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3128 | `		return SXERR_ABORT;` |
|       - | 3129 | `	}` |
|       - | 3130 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   57255 | 3131 | `	for(;;){` |
|  114512 | 3132 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3133 | `			/* Syntax error */` |
|     ! 0 | 3134 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3135 | `				pToken--;` |
|     ! 0 | 3136 | `			}` |
|     ! 0 | 3137 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 3138 | `			if( rc == SXERR_ABORT ){` |
|       - | 3139 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3140 | `				return SXERR_ABORT;` |
|       - | 3141 | `			}` |
|     ! 0 | 3142 | `			goto Synchronize;` |
|       - | 3143 | `		}` |
|       - | 3144 | `		/* Jump the left parenthesis '(' */` |
|  114512 | 3145 | `		pToken++;` |
|       - | 3146 | `		/* Delimit the condition */` |
|  114512 | 3147 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  114512 | 3148 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 3149 | `			/* Syntax error */` |
|     ! 0 | 3150 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3151 | `				pToken--;` |
|     ! 0 | 3152 | `			}` |
|     ! 0 | 3153 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 3154 | `			if( rc == SXERR_ABORT ){` |
|       - | 3155 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3156 | `				return SXERR_ABORT;` |
|       - | 3157 | `			}` |
|     ! 0 | 3158 | `			goto Synchronize;` |
|       - | 3159 | `		}` |
|       - | 3160 | `		/* Swap token streams */` |
|  114512 | 3161 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 3162 | `		/* Compile the condition */` |
|  114512 | 3163 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3164 | `		/* Update token stream */` |
|  114512 | 3165 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 3166 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3167 | `			pGen->pIn++;` |
|     ! 0 | 3168 | `		}` |
|  114512 | 3169 | `		pGen->pIn  = &pEnd[1];` |
|  114512 | 3170 | `		pGen->pEnd = pTmp;` |
|  114512 | 3171 | `		if( rc == SXERR_ABORT ){` |
|       - | 3172 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3173 | `			return SXERR_ABORT;` |
|       - | 3174 | `		}` |
|       - | 3175 | `		/* Emit the false jump */` |
|  114512 | 3176 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 3177 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  114512 | 3178 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 3179 | `		/* Compile the body */` |
|  114512 | 3180 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  114512 | 3181 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3182 | `			return SXERR_ABORT;` |
|       - | 3183 | `		}` |
|  114512 | 3184 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   30828 | 3185 | `			break;` |
|       - | 3186 | `		}` |
|       - | 3187 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   52860 | 3188 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   52860 | 3189 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   33970 | 3190 | `			break;` |
|       - | 3191 | `		}` |
|       - | 3192 | `		/* Emit the unconditional jump */` |
|   18892 | 3193 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 3194 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   18892 | 3195 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   18892 | 3196 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   13684 | 3197 | `			pToken = &pGen->pIn[1];` |
|   13684 | 3198 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5214 | 3199 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4251 | 3200 | `					break;` |
|       - | 3201 | `			}` |
|    5186 | 3202 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2592 | 3203 | `		}` |
|   10394 | 3204 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 3205 | `		/* Synchronize cursors */` |
|   10394 | 3206 | `		pToken = pGen->pIn;` |
|       - | 3207 | `		/* Fix the false jump */` |
|   10394 | 3208 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 3209 | `	} /* For(;;) */` |
|       - | 3210 | `	/* Fix the false jump */` |
|  104120 | 3211 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  104120 | 3212 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   42466 | 3213 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 3214 | `			/* Compile the else block */` |
|    8500 | 3215 | `			pGen->pIn++;` |
|    8500 | 3216 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    8500 | 3217 | `			if( rc == SXERR_ABORT ){` |
|       - | 3218 |  |
|     ! 0 | 3219 | `				return SXERR_ABORT;` |
|       - | 3220 | `			}` |
|    4249 | 3221 | `	}` |
|  104120 | 3222 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3223 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  104120 | 3224 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 3225 | `	/* Release the conditional block */` |
|  104120 | 3226 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3227 | `	/* Statement successfully compiled */` |
|  104120 | 3228 | `	return SXRET_OK;` |
|     ! 0 | 3229 | `Synchronize:` |
|       - | 3230 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 3231 | `	 */` |
|     ! 0 | 3232 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3233 | `		pGen->pIn++;` |
|     ! 0 | 3234 | `	}` |
|     ! 0 | 3235 | `	return SXRET_OK;` |
|   52061 | 3236 |  |
|       - | 3237 | `/*` |
|       - | 3238 | ` * Compile the global construct.` |
|       - | 3239 | ` * According to the PHP language reference` |
|       - | 3240 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 3241 | ` *  to be used in that function.` |
|       - | 3242 | ` *  Example #1 Using global` |
|       - | 3243 | ` *  <?php` |
|       - | 3244 | ` *   $a = 1;` |
|       - | 3245 | ` *   $b = 2;` |
|       - | 3246 | ` *   function Sum()` |
|       - | 3247 | ` *   {` |
|       - | 3248 | ` *    global $a, $b;` |
|       - | 3249 | ` *    $b = $a + $b;` |
|       - | 3250 | ` *   }` |
|       - | 3251 | ` *   Sum();` |
|       - | 3252 | ` *   echo $b;` |
|       - | 3253 | ` *  ?>` |
|       - | 3254 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 3255 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 3256 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 3257 | ` */` |
|      26 | 3258 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 | 3259 |  |
|      28 | 3260 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3261 | `	sxi32 nExpr;` |
|       - | 3262 | `	sxi32 rc;` |
|       - | 3263 | `	/* Jump the 'global' keyword */` |
|      28 | 3264 | `	pGen->pIn++;` |
|      28 | 3265 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 3266 | `		/* Nothing to process */` |
|     ! 0 | 3267 | `		return SXRET_OK;` |
|       - | 3268 | `	}` |
|      28 | 3269 | `	pTmp = pGen->pEnd;` |
|      28 | 3270 | `	nExpr = 0;` |
|      56 | 3271 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      30 | 3272 | `		if( pGen->pIn < pNext ){` |
|      30 | 3273 | `			pGen->pEnd = pNext;` |
|      30 | 3274 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3275 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 3276 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 3277 | `					return SXERR_ABORT;` |
|       - | 3278 | `				}` |
|     ! 0 | 3279 | `			}else{` |
|      30 | 3280 | `				pGen->pIn++;` |
|      30 | 3281 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3282 | `					/* Emit a warning */` |
|     ! 0 | 3283 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 3284 | `				}else{` |
|      30 | 3285 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 | 3286 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3287 | `						return SXERR_ABORT;` |
|      30 | 3288 | `					}else if(rc != SXERR_EMPTY ){` |
|      30 | 3289 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      30 | 3290 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - | 3291 | `							/* Variable name, not a constant */` |
|      30 | 3292 | `							pLast->iP1 = 0;` |
|      14 | 3293 | `						}` |
|      30 | 3294 | `						nExpr++;` |
|      14 | 3295 | `					}` |
|       - | 3296 | `				}` |
|       - | 3297 | `			}` |
|      14 | 3298 | `		}` |
|       - | 3299 | `		/* Next expression in the stream */` |
|      30 | 3300 | `		pGen->pIn = pNext;` |
|       - | 3301 | `		/* Jump trailing commas */` |
|      32 | 3302 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3303 | `			pGen->pIn++;` |
|       1 | 3304 | `		}` |
|       2 | 3305 | `	}` |
|       - | 3306 | `	/* Restore token stream */` |
|      28 | 3307 | `	pGen->pEnd = pTmp;` |
|      28 | 3308 | `	if( nExpr > 0 ){` |
|       - | 3309 | `		/* Emit the uplink instruction */` |
|      28 | 3310 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      13 | 3311 | `	}` |
|      28 | 3312 | `	return SXRET_OK;` |
|      15 | 3313 |  |
|       - | 3314 | `/*` |
|       - | 3315 | ` * Compile the return statement.` |
|       - | 3316 | ` * According to the PHP language reference` |
|       - | 3317 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 3318 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 3319 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 3320 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 3321 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 3322 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 3323 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 3324 | ` *  from within the main script file, then script execution end.` |
|       - | 3325 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 3326 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 3327 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 3328 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 3329 | ` */` |
|  150970 | 3330 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 3331 |  |
|  150972 | 3332 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3333 | `	sxi32 rc;` |
|       - | 3334 | `	/* Jump the 'return' keyword */` |
|  150972 | 3335 | `	pGen->pIn++;` |
|  150972 | 3336 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3337 | `		/* Compile the expression */` |
|  150950 | 3338 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  150950 | 3339 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3340 | `			return SXERR_ABORT;` |
|  150950 | 3341 | `		}else if(rc != SXERR_EMPTY ){` |
|  150950 | 3342 | `			nRet = 1;` |
|   75474 | 3343 | `		}` |
|   75474 | 3344 | `	}` |
|       - | 3345 | `	/* Emit the done instruction */` |
|  150972 | 3346 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  150972 | 3347 | `	return SXRET_OK;` |
|   75487 | 3348 |  |
|       - | 3349 | `/*` |
|       - | 3350 | ` * Compile a yield expression.` |
|       - | 3351 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - | 3352 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - | 3353 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - | 3354 | ` */` |
|      32 | 3355 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 3356 |  |
|       - | 3357 | `	SyToken *pTmp, *pSplit;` |
|      34 | 3358 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      34 | 3359 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - | 3360 | `	sxi32 rc;` |
|      16 | 3361 | `	(void)iCompileFlag;` |
|       - | 3362 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      34 | 3363 | `	pGen->pIn++;` |
|       - | 3364 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - | 3365 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      34 | 3366 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3367 | `		/* Bare yield — no value */` |
|     ! 0 | 3368 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 | 3369 | `		return SXRET_OK;` |
|       - | 3370 | `	}` |
|       - | 3371 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      34 | 3372 | `	pSplit = 0;` |
|       - | 3373 | `	{` |
|      34 | 3374 | `		SyToken *pCur = pGen->pIn;` |
|      34 | 3375 | `		sxi32 nNest = 0;` |
|      78 | 3376 | `		while( pCur < pGen->pEnd ){` |
|      52 | 3377 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 | 3378 | `				nNest++;` |
|      52 | 3379 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 | 3380 | `				nNest--;` |
|      52 | 3381 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 | 3382 | `				pSplit = pCur;` |
|       7 | 3383 | `				break;` |
|       - | 3384 | `			}` |
|      46 | 3385 | `			pCur++;` |
|       2 | 3386 | `		}` |
|       - | 3387 | `	}` |
|      34 | 3388 | `	pTmp = pGen->pEnd;` |
|      34 | 3389 | `	if( pSplit ){` |
|       - | 3390 | `		/* yield $key => $value */` |
|       7 | 3391 | `		pGen->pEnd = pSplit;` |
|       7 | 3392 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 3393 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 3394 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       7 | 3395 | `		pGen->pEnd = pTmp;` |
|       7 | 3396 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 3397 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 3398 | `		iP1 = 1;` |
|       7 | 3399 | `		iP2 = 1;` |
|       4 | 3400 | `	}else{` |
|       - | 3401 | `		/* yield $value */` |
|      28 | 3402 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      28 | 3403 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      28 | 3404 | `		if( rc != SXERR_EMPTY ){` |
|      28 | 3405 | `			iP1 = 1;` |
|      13 | 3406 | `		}` |
|       - | 3407 | `	}` |
|      34 | 3408 | `	pGen->pEnd = pTmp;` |
|      34 | 3409 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      34 | 3410 | `	return SXRET_OK;` |
|      18 | 3411 |  |
|       - | 3412 | `/*` |
|       - | 3413 | ` * Compile the die/exit language construct.` |
|       - | 3414 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 3415 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 3416 | ` */` |
|      88 | 3417 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 | 3418 |  |
|      90 | 3419 | `	sxi32 nExpr = 0;` |
|       - | 3420 | `	sxi32 rc;` |
|       - | 3421 | `	/* Jump the die/exit keyword */` |
|      90 | 3422 | `	pGen->pIn++;` |
|      90 | 3423 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3424 | `		/* Compile the expression */` |
|      90 | 3425 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 | 3426 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3427 | `			return SXERR_ABORT;` |
|      90 | 3428 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 | 3429 | `			nExpr = 1;` |
|      44 | 3430 | `		}` |
|      44 | 3431 | `	}` |
|       - | 3432 | `	/* Emit the HALT instruction */` |
|      90 | 3433 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 | 3434 | `	return SXRET_OK;` |
|      46 | 3435 |  |
|       - | 3436 | `/*` |
|       - | 3437 | ` * Compile the 'echo' language construct.` |
|       - | 3438 | ` */` |
|   10630 | 3439 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 3440 |  |
|   10632 | 3441 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3442 | `	sxi32 rc;` |
|       - | 3443 | `	/* Jump the 'echo' keyword */` |
|   10632 | 3444 | `	pGen->pIn++;` |
|       - | 3445 | `	/* Compile arguments one after one */` |
|   10632 | 3446 | `	pTmp = pGen->pEnd;` |
|   21650 | 3447 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   11020 | 3448 | `		if( pGen->pIn < pNext ){` |
|   11020 | 3449 | `			pGen->pEnd = pNext;` |
|   11020 | 3450 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   11020 | 3451 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3452 | `				return SXERR_ABORT;` |
|   11020 | 3453 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3454 | `				/* Emit the consume instruction */` |
|   10996 | 3455 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    5497 | 3456 | `			}` |
|    5509 | 3457 | `		}` |
|       - | 3458 | `		/* Jump trailing commas */` |
|   11408 | 3459 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     390 | 3460 | `			pNext++;` |
|       2 | 3461 | `		}` |
|   11020 | 3462 | `		pGen->pIn = pNext;` |
|       2 | 3463 | `	}` |
|       - | 3464 | `	/* Restore token stream */` |
|   10632 | 3465 | `	pGen->pEnd = pTmp;` |
|   10632 | 3466 | `	return SXRET_OK;` |
|    5317 | 3467 |  |
|       - | 3468 | `/*` |
|       - | 3469 | ` * Compile the static statement.` |
|       - | 3470 | ` * According to the PHP language reference` |
|       - | 3471 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 3472 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 3473 | ` *  when program execution leaves this scope.` |
|       - | 3474 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 3475 | ` * Symisc eXtension.` |
|       - | 3476 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 3477 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3478 | ` *  Example` |
|       - | 3479 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 3480 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 3481 | ` */` |
|       2 | 3482 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 | 3483 |  |
|       - | 3484 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 3485 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 3486 | `	GenBlock *pBlock;` |
|       - | 3487 | `	SyString *pName;` |
|       - | 3488 | `	char *zDup;` |
|       - | 3489 | `	sxu32 nLine;` |
|       - | 3490 | `	sxi32 rc;` |
|       - | 3491 | `	/* Jump the static keyword */` |
|       3 | 3492 | `	nLine = pGen->pIn->nLine;` |
|       3 | 3493 | `	pGen->pIn++;` |
|       - | 3494 | `	/* Extract the enclosing function if any */` |
|       3 | 3495 | `	pBlock = pGen->pCurrent;` |
|       5 | 3496 | `	while( pBlock ){` |
|       5 | 3497 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 | 3498 | `			break;` |
|       - | 3499 | `		}` |
|       - | 3500 | `		/* Point to the upper block */` |
|       3 | 3501 | `		pBlock = pBlock->pParent;` |
|       1 | 3502 | `	}` |
|       3 | 3503 | `	if( pBlock == 0 ){` |
|       - | 3504 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 3505 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3506 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 | 3507 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3508 | `				return SXERR_ABORT;` |
|       - | 3509 | `			}` |
|     ! 0 | 3510 | `			goto Synchronize;` |
|       - | 3511 | `		}` |
|       - | 3512 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 3513 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 3514 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3515 | `			return SXERR_ABORT;` |
|     ! 0 | 3516 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 3517 | `			/* Emit the POP instruction */` |
|     ! 0 | 3518 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 3519 | `		}` |
|     ! 0 | 3520 | `		return SXRET_OK;` |
|       - | 3521 | `	}` |
|       3 | 3522 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 3523 | `	/* Make sure we are dealing with a valid statement */` |
|       3 | 3524 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 | 3525 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 | 3526 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 | 3527 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3528 | `				return SXERR_ABORT;` |
|       - | 3529 | `			}` |
|       3 | 3530 | `			goto Synchronize;` |
|       - | 3531 | `	}` |
|     ! 0 | 3532 | `	pGen->pIn++;` |
|       - | 3533 | `	/* Extract variable name */` |
|     ! 0 | 3534 | `	pName = &pGen->pIn->sData;` |
|     ! 0 | 3535 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 | 3536 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 3537 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3538 | `		goto Synchronize;` |
|       - | 3539 | `	}` |
|       - | 3540 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 | 3541 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 | 3542 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 3543 | `	/* Duplicate variable name */` |
|     ! 0 | 3544 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 | 3545 | `	if( zDup == 0 ){` |
|     ! 0 | 3546 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3547 | `		return SXERR_ABORT;` |
|       - | 3548 | `	}` |
|     ! 0 | 3549 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 3550 | `	/* Check if we have an expression to compile */` |
|     ! 0 | 3551 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 3552 | `		SySet *pInstrContainer;` |
|       - | 3553 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 3554 | `		 * Static variable can take any complex expression including function` |
|       - | 3555 | `		 * call as their initialization value.` |
|       - | 3556 | `		 * Example:` |
|       - | 3557 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 3558 | `		 */` |
|     ! 0 | 3559 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 3560 | `		/* Swap bytecode container */` |
|     ! 0 | 3561 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 | 3562 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 3563 | `		/* Compile the expression */` |
|     ! 0 | 3564 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3565 | `		/* Emit the done instruction */` |
|     ! 0 | 3566 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 3567 | `		/* Restore default bytecode container */` |
|     ! 0 | 3568 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 3569 | `	}` |
|       - | 3570 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 | 3571 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 | 3572 | `	return SXRET_OK;` |
|       1 | 3573 | `Synchronize:` |
|       - | 3574 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 3575 | `	 * statement.` |
|       - | 3576 | `	 */` |
|       5 | 3577 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 | 3578 | `		pGen->pIn++;` |
|       1 | 3579 | `	}` |
|       3 | 3580 | `	return SXRET_OK;` |
|       2 | 3581 |  |
|       - | 3582 | `/*` |
|       - | 3583 | ` * Compile the var statement.` |
|       - | 3584 | ` * Symisc Extension:` |
|       - | 3585 | ` *      var statement can be used outside of a class definition.` |
|       - | 3586 | ` */` |
|       4 | 3587 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 3588 |  |
|       - | 3589 | `	sxu32 nLine;` |
|       - | 3590 | `	sxi32 rc;` |
|       5 | 3591 | `	nLine = pGen->pIn->nLine;` |
|       - | 3592 | `	/* Jump the 'var' keyword */` |
|       5 | 3593 | `	pGen->pIn++;` |
|       5 | 3594 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 3595 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - | 3596 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 | 3597 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 | 3598 | `			pGen->pIn++;` |
|     ! 0 | 3599 | `		}` |
|     ! 0 | 3600 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3601 | `			return SXERR_ABORT;` |
|       - | 3602 | `		}` |
|     ! 0 | 3603 | `	}else{` |
|       - | 3604 | `		/* Compile the expression */` |
|       5 | 3605 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 | 3606 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3607 | `			return SXERR_ABORT;` |
|       5 | 3608 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 | 3609 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 3610 | `		}` |
|       - | 3611 | `	}` |
|       5 | 3612 | `	return SXRET_OK;` |
|       3 | 3613 |  |
|       - | 3614 | `/*` |
|       - | 3615 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 3616 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 3617 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 3618 | ` */` |
|       - | 3619 | `/*` |
|       - | 3620 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - | 3621 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 3622 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 3623 | ` * qualified name and updates the instruction's operand index.` |
|       - | 3624 | ` *` |
|       - | 3625 | ` * Resolution order:` |
|       - | 3626 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - | 3627 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - | 3628 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - | 3629 | ` *` |
|       - | 3630 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - | 3631 | ` * came from an import (step 1) and 0 otherwise.` |
|       - | 3632 | ` * Returns the (possibly new) literal index.` |
|       - | 3633 | ` */` |
|  310086 | 3634 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       2 | 3635 |  |
|       - | 3636 | `	ph7_value *pLit;` |
|       - | 3637 | `	const char *zLit;` |
|       - | 3638 | `	SyString sQualified;` |
|       - | 3639 | `	sxu32 nLit;` |
|       - | 3640 | `	sxu32 k;` |
|       - | 3641 | `	sxu32 nNewIdx;` |
|       - | 3642 | `	int hasNsSep;` |
|       - | 3643 | `	SyHashEntry *pImport;` |
|       - | 3644 | `	ph7_value *pNew;` |
|  310088 | 3645 | `	if( pFromImport ){` |
|  296614 | 3646 | `		*pFromImport = 0;` |
|  148306 | 3647 | `	}` |
|  310088 | 3648 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  310088 | 3649 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 3650 | `		return nOrigIdx;` |
|       - | 3651 | `	}` |
|  310088 | 3652 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  310088 | 3653 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 3654 | `	/* Skip if already qualified (contains backslash) */` |
|  310088 | 3655 | `	hasNsSep = 0;` |
| 3335312 | 3656 | `	for( k = 0; k < nLit; k++ ){` |
| 3025258 | 3657 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1512614 | 3658 | `	}` |
|  310088 | 3659 | `	if( hasNsSep ){` |
|      34 | 3660 | `		return nOrigIdx;` |
|       - | 3661 | `	}` |
|       - | 3662 | `	/* Check use imports first (works even outside namespaces) */` |
|  310056 | 3663 | `	SyBlobReset(&pGen->sWorker);` |
|  310056 | 3664 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  310056 | 3665 | `	if( pImport ){` |
|      38 | 3666 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 | 3667 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 | 3668 | `		if( pFromImport ){` |
|      18 | 3669 | `			*pFromImport = 1;` |
|       8 | 3670 | `		}` |
|      20 | 3671 | `	}else{` |
|  310020 | 3672 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  309950 | 3673 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - | 3674 | `		}` |
|       - | 3675 | `		/* Prepend current namespace */` |
|      72 | 3676 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      72 | 3677 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      72 | 3678 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 3679 | `	}` |
|       - | 3680 | `	/* Look up or create a new literal for the qualified name */` |
|     108 | 3681 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     108 | 3682 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      44 | 3683 | `		return nNewIdx; /* Already interned */` |
|       - | 3684 | `	}` |
|      66 | 3685 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      66 | 3686 | `	if( pNew == 0 ){` |
|     ! 0 | 3687 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 3688 | `	}` |
|      66 | 3689 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      66 | 3690 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      66 | 3691 | `	return nNewIdx;` |
|  155045 | 3692 |  |
|       - | 3693 | `/*` |
|       - | 3694 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 3695 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 3696 | ` */` |
|   26078 | 3697 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3698 |  |
|       - | 3699 | `	SyHashEntry *pImport;` |
|       - | 3700 | `	/* Check use imports first */` |
|   26080 | 3701 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   26080 | 3702 | `	if( pImport ){` |
|       7 | 3703 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       7 | 3704 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       7 | 3705 | `		return;` |
|       - | 3706 | `	}` |
|       - | 3707 | `	/* Prepend current namespace if active */` |
|   26074 | 3708 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 3709 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 3710 | `		SyBlobAppend(pOut,"\\",1);` |
|       1 | 3711 | `	}` |
|   26074 | 3712 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   13041 | 3713 |  |
|       - | 3714 | `/*` |
|       - | 3715 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 3716 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 3717 | ` * The caller must release pOut when done.` |
|       - | 3718 | ` */` |
|   44580 | 3719 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3720 |  |
|   44582 | 3721 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      46 | 3722 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      46 | 3723 | `		SyBlobAppend(pOut,"\\",1);` |
|      22 | 3724 | `	}` |
|   44582 | 3725 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   44582 | 3726 |  |
|       - | 3727 | `/*` |
|       - | 3728 | ` * Compile a namespace statement` |
|       - | 3729 | ` * According to the PHP language reference manual` |
|       - | 3730 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 3731 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 3732 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 3733 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 3734 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 3735 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 3736 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 3737 | ` *  programming world.` |
|       - | 3738 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 3739 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 3740 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 3741 | ` *  classes/functions/constants.` |
|       - | 3742 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 3743 | ` *  readability of source code.` |
|       - | 3744 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 3745 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 3746 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 3747 | ` *       class MyClass {}` |
|       - | 3748 | ` *       function myfunction() {}` |
|       - | 3749 | ` *       const MYCONST = 1;` |
|       - | 3750 | ` *       $a = new MyClass;` |
|       - | 3751 | ` *       $c = new \my\name\MyClass;` |
|       - | 3752 | ` *       $a = strlen('hi');` |
|       - | 3753 | ` *       $d = namespace\MYCONST;` |
|       - | 3754 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 3755 | ` *       echo constant($d);` |
|       - | 3756 | ` * NOTE` |
|       - | 3757 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3758 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3759 | ` */` |
|       - | 3760 | `/*` |
|       - | 3761 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 3762 | ` */` |
|       6 | 3763 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 | 3764 |  |
|       7 | 3765 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|     ! 0 | 3766 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|     ! 0 | 3767 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|     ! 0 | 3768 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|     ! 0 | 3769 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|     ! 0 | 3770 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|     ! 0 | 3771 | `	return "token";` |
|       4 | 3772 |  |
|      94 | 3773 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       2 | 3774 |  |
|       - | 3775 | `	sxu32 nLine;` |
|       - | 3776 | `	sxi32 rc;` |
|      96 | 3777 | `	nLine = pGen->pIn->nLine;` |
|      96 | 3778 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 3779 | `	/* Reset namespace and clear previous use imports */` |
|      96 | 3780 | `	SyBlobReset(&pGen->sNamespace);` |
|      96 | 3781 | `	SyHashRelease(&pGen->hUseImports);` |
|      96 | 3782 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      96 | 3783 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      96 | 3784 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      96 | 3785 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      96 | 3786 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      96 | 3787 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3788 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 | 3789 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3790 | `		return SXRET_OK;` |
|       - | 3791 | `	}` |
|      96 | 3792 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - | 3793 | `		/* namespace; — switch to global namespace */` |
|     ! 0 | 3794 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3795 | `		return SXRET_OK;` |
|       - | 3796 | `	}` |
|      96 | 3797 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - | 3798 | `		/* namespace { } — global namespace block */` |
|     ! 0 | 3799 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3800 | `		return SXRET_OK;` |
|       - | 3801 | `	}` |
|       - | 3802 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     228 | 3803 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     134 | 3804 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 3805 | `			/* Append backslash separator */` |
|      21 | 3806 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      21 | 3807 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      10 | 3808 | `			}` |
|      11 | 3809 | `		}else{` |
|       - | 3810 | `			/* Append identifier */` |
|     114 | 3811 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 3812 | `		}` |
|     134 | 3813 | `		pGen->pIn++;` |
|       2 | 3814 | `	}` |
|       - | 3815 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - | 3816 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - | 3817 | `	{` |
|      96 | 3818 | `		char *zNsDup = 0;` |
|      96 | 3819 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     140 | 3820 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      92 | 3821 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      46 | 3822 | `		}` |
|      96 | 3823 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - | 3824 | `	}` |
|      96 | 3825 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 | 3826 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3827 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 3828 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 | 3829 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3830 | `			return SXERR_ABORT;` |
|       - | 3831 | `		}` |
|       2 | 3832 | `	}` |
|      96 | 3833 | `	return SXRET_OK;` |
|      49 | 3834 |  |
|       - | 3835 | `/*` |
|       - | 3836 | ` * Compile the 'use' statement` |
|       - | 3837 | ` * According to the PHP language reference manual` |
|       - | 3838 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 3839 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 3840 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 3841 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 3842 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 3843 | ` *  a function or constant is not supported.` |
|       - | 3844 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 3845 | ` * NOTE` |
|       - | 3846 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3847 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3848 | ` */` |
|      64 | 3849 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       2 | 3850 |  |
|       - | 3851 | `	sxu32 nLine;` |
|       - | 3852 | `	sxi32 rc;` |
|       - | 3853 | `	SyBlob sPath;` |
|       - | 3854 | `	SyString sAlias;` |
|       - | 3855 | `	SyToken *pLast;` |
|       - | 3856 | `	char *zDup;` |
|       - | 3857 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - | 3858 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - | 3859 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      66 | 3860 | `	nLine = pGen->pIn->nLine;` |
|      66 | 3861 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - | 3862 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      66 | 3863 | `	iUseType = 0;` |
|      66 | 3864 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 | 3865 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 | 3866 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 | 3867 | `			iUseType = 1;` |
|      16 | 3868 | `			pGen->pIn++;` |
|      23 | 3869 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 | 3870 | `			iUseType = 2;` |
|      16 | 3871 | `			pGen->pIn++;` |
|       7 | 3872 | `		}` |
|      14 | 3873 | `	}` |
|       - | 3874 | `	/* Select target hash tables based on import type */` |
|      66 | 3875 | `	switch( iUseType ){` |
|       7 | 3876 | `		case 1:` |
|      16 | 3877 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 | 3878 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 | 3879 | `			break;` |
|       7 | 3880 | `		case 2:` |
|      16 | 3881 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 | 3882 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 | 3883 | `			break;` |
|      18 | 3884 | `		default:` |
|      38 | 3885 | `			pGenHash = &pGen->hUseImports;` |
|      38 | 3886 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      36 | 3887 | `			break;` |
|       - | 3888 | `	}` |
|      66 | 3889 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 3890 | `	/* Process one or more use declarations separated by commas */` |
|      33 | 3891 | `	for(;;){` |
|      68 | 3892 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3893 | `			break;` |
|       - | 3894 | `		}` |
|      68 | 3895 | `		SyBlobReset(&sPath);` |
|      68 | 3896 | `		pLast = 0;` |
|       - | 3897 | `		/* Collect the full namespace path */` |
|     250 | 3898 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     184 | 3899 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     124 | 3900 | `				pLast = pGen->pIn;` |
|     124 | 3901 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      62 | 3902 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 | 3903 | `				}` |
|     124 | 3904 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      61 | 3905 | `			}` |
|     184 | 3906 | `			pGen->pIn++;` |
|       2 | 3907 | `		}` |
|      68 | 3908 | `		if( pLast == 0 ){` |
|       - | 3909 | `			/* Empty path */` |
|       5 | 3910 | `			break;` |
|       - | 3911 | `		}` |
|       - | 3912 | `		/* Default alias is the last component of the path */` |
|      64 | 3913 | `		sAlias = pLast->sData;` |
|       - | 3914 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      62 | 3915 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      41 | 3916 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 | 3917 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 | 3918 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 | 3919 | `				sAlias = pGen->pIn->sData;` |
|      18 | 3920 | `				pGen->pIn++;` |
|       8 | 3921 | `			}` |
|       8 | 3922 | `		}` |
|       - | 3923 | `		/* Check for duplicate import alias (per-type) */` |
|      64 | 3924 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 | 3925 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3926 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 | 3927 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 | 3928 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3929 | `				SyBlobRelease(&sPath);` |
|     ! 0 | 3930 | `				return SXERR_ABORT;` |
|       - | 3931 | `			}` |
|       2 | 3932 | `		}` |
|       - | 3933 | `		/* Register the import: alias -> FQN.` |
|       - | 3934 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - | 3935 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - | 3936 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      95 | 3937 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      62 | 3938 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      64 | 3939 | `		if( zDup ){` |
|      64 | 3940 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      64 | 3941 | `			if( pVmHash ){` |
|       - | 3942 | `				/* Class imports: populate VM table directly (class resolution` |
|       - | 3943 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      36 | 3944 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      36 | 3945 | `				if( zAliasDup ){` |
|      36 | 3946 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      17 | 3947 | `				}` |
|      17 | 3948 | `			}` |
|      64 | 3949 | `			if( iUseType == 2 ){` |
|       - | 3950 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - | 3951 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 | 3952 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 | 3953 | `				if( zAliasDup ){` |
|       - | 3954 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - | 3955 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - | 3956 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 | 3957 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 | 3958 | `					if( azPair ){` |
|      16 | 3959 | `						azPair[0] = zAliasDup;` |
|      16 | 3960 | `						azPair[1] = zDup;` |
|      16 | 3961 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 | 3962 | `					}` |
|       7 | 3963 | `				}` |
|       7 | 3964 | `			}` |
|      31 | 3965 | `		}` |
|       - | 3966 | `		/* Check for comma (multiple use declarations) */` |
|      64 | 3967 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3968 | `			pGen->pIn++;` |
|       2 | 3969 | `		}else{` |
|      32 | 3970 | `			break;` |
|       - | 3971 | `		}` |
|       1 | 3972 | `	}` |
|      66 | 3973 | `	SyBlobRelease(&sPath);` |
|      66 | 3974 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 3975 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 3976 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 3977 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3978 | `			return SXERR_ABORT;` |
|       - | 3979 | `		}` |
|       1 | 3980 | `	}` |
|      66 | 3981 | `	return SXRET_OK;` |
|      34 | 3982 |  |
|       - | 3983 | `/*` |
|       - | 3984 | ` * Compile the stupid 'declare' language construct.` |
|       - | 3985 | ` *` |
|       - | 3986 | ` * According to the PHP language reference manual.` |
|       - | 3987 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 3988 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 3989 | ` *  declare (directive)` |
|       - | 3990 | ` *   statement` |
|       - | 3991 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 3992 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 3993 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 3994 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 3995 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 3996 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 3997 | ` * <?php` |
|       - | 3998 | ` * // these are the same:` |
|       - | 3999 | ` * // you can use this:` |
|       - | 4000 | ` * declare(ticks=1) {` |
|       - | 4001 | ` *   // entire script here` |
|       - | 4002 | ` * }` |
|       - | 4003 | ` * // or you can use this:` |
|       - | 4004 | ` * declare(ticks=1);` |
|       - | 4005 | ` * // entire script here` |
|       - | 4006 | ` * ?>` |
|       - | 4007 | ` *` |
|       - | 4008 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 4009 | ` */` |
|       8 | 4010 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 4011 |  |
|       9 | 4012 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 4013 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 4014 | `	sxi32 rc;` |
|       9 | 4015 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 4016 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 4017 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 4018 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4019 | `			return SXERR_ABORT;` |
|       - | 4020 | `		}` |
|       5 | 4021 | `		goto Synchro;` |
|       - | 4022 | `	}` |
|       5 | 4023 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 4024 | `	/* Delimit the directive */` |
|       5 | 4025 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 4026 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 4027 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 4028 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4029 | `			return SXERR_ABORT;` |
|       - | 4030 | `		}` |
|     ! 0 | 4031 | `		return SXRET_OK;` |
|       - | 4032 | `	}` |
|       - | 4033 | `	/* Update the cursor */` |
|       5 | 4034 | `	pGen->pIn = &pEnd[1];` |
|       5 | 4035 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 4036 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 4037 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4038 | `			return SXERR_ABORT;` |
|       - | 4039 | `		}` |
|     ! 0 | 4040 | `	}` |
|       - | 4041 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 4042 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 4043 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 4044 | `		ph7_lib_version()` |
|       - | 4045 | `		);` |
|       - | 4046 | `	/*All done */` |
|       5 | 4047 | `	return SXRET_OK;` |
|       2 | 4048 | `Synchro:` |
|       - | 4049 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 4050 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 4051 | `		pGen->pIn++;` |
|       1 | 4052 | `	}` |
|       5 | 4053 | `	return SXRET_OK;` |
|       5 | 4054 |  |
|       - | 4055 | `/*` |
|       - | 4056 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 4057 | ` * as follows:` |
|       - | 4058 | ` * function makecoffee($type = "cappuccino")` |
|       - | 4059 | ` * {` |
|       - | 4060 | ` *   return "Making a cup of $type.\n";` |
|       - | 4061 | ` * }` |
|       - | 4062 | ` * Symisc eXtension.` |
|       - | 4063 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 4064 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 4065 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 4066 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 4067 | ` *      {` |
|       - | 4068 | ` *       var_dump($a);` |
|       - | 4069 | ` *      }` |
|       - | 4070 | ` *     //call test without args` |
|       - | 4071 | ` *      test();` |
|       - | 4072 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 4073 | ` *      Example:` |
|       - | 4074 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 4075 | ` * 3 -) Function overloading!!` |
|       - | 4076 | ` *      Example:` |
|       - | 4077 | ` *      function foo($a) {` |
|       - | 4078 | ` *   	  return $a.PHP_EOL;` |
|       - | 4079 | ` *	    }` |
|       - | 4080 | ` *	    function foo($a, $b) {` |
|       - | 4081 | ` *   	  return $a + $b;` |
|       - | 4082 | ` *	    }` |
|       - | 4083 | ` *	    echo foo(5); // Prints "5"` |
|       - | 4084 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 4085 | ` *      // Same arg` |
|       - | 4086 | ` *	   function foo(string $a)` |
|       - | 4087 | ` *	   {` |
|       - | 4088 | ` *	     echo "a is a string\n";` |
|       - | 4089 | ` *	     var_dump($a);` |
|       - | 4090 | ` *	   }` |
|       - | 4091 | ` *	  function foo(int $a)` |
|       - | 4092 | ` *	  {` |
|       - | 4093 | ` *	    echo "a is integer\n";` |
|       - | 4094 | ` *	    var_dump($a);` |
|       - | 4095 | ` *	  }` |
|       - | 4096 | ` *	  function foo(array $a)` |
|       - | 4097 | ` *	  {` |
|       - | 4098 | ` * 	    echo "a is an array\n";` |
|       - | 4099 | ` * 	    var_dump($a);` |
|       - | 4100 | ` *	  }` |
|       - | 4101 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 4102 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 4103 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 4104 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 4105 | ` * introduced by the PH7 engine.` |
|       - | 4106 | ` */` |
|   41482 | 4107 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 4108 |  |
|       - | 4109 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 4110 | `	SySet *pInstrContainer;` |
|       - | 4111 | `	sxi32 rc;` |
|       - | 4112 | `	/* Swap token stream */` |
|   41484 | 4113 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   41484 | 4114 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   41484 | 4115 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 4116 | `	/* Compile the expression holding the argument value */` |
|   41484 | 4117 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4118 | `	/* Emit the done instruction */` |
|   41484 | 4119 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   41484 | 4120 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   41484 | 4121 | `	RE_SWAP_DELIMITER(pGen);` |
|   41484 | 4122 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4123 | `		return SXERR_ABORT;` |
|       - | 4124 | `	}` |
|   41484 | 4125 | `	return SXRET_OK;` |
|   20743 | 4126 |  |
|       - | 4127 | `/*` |
|       - | 4128 | ` * Collect function arguments one after one.` |
|       - | 4129 | ` * According to the PHP language reference manual.` |
|       - | 4130 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 4131 | ` * list of expressions.` |
|       - | 4132 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 4133 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 4134 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 4135 | ` * for more information.` |
|       - | 4136 | ` * Example #1 Passing arrays to functions` |
|       - | 4137 | ` * <?php` |
|       - | 4138 | ` * function takes_array($input)` |
|       - | 4139 | ` * {` |
|       - | 4140 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 4141 | ` * }` |
|       - | 4142 | ` * ?>` |
|       - | 4143 | ` * Making arguments be passed by reference` |
|       - | 4144 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 4145 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 4146 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 4147 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 4148 | ` * to the argument name in the function definition:` |
|       - | 4149 | ` * Example #2 Passing function parameters by reference` |
|       - | 4150 | ` * <?php` |
|       - | 4151 | ` * function add_some_extra(&$string)` |
|       - | 4152 | ` * {` |
|       - | 4153 | ` *   $string .= 'and something extra.';` |
|       - | 4154 | ` * }` |
|       - | 4155 | ` * $str = 'This is a string, ';` |
|       - | 4156 | ` * add_some_extra($str);` |
|       - | 4157 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 4158 | ` * ?>` |
|       - | 4159 | ` *` |
|       - | 4160 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 4161 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 4162 | ` * on these extension.` |
|       - | 4163 | ` */` |
|   49806 | 4164 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 4165 |  |
|       - | 4166 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 4167 | `	SyToken *pIn;  /* Token stream */` |
|       - | 4168 | `	SyBlob sSig;         /* Function signature */` |
|       - | 4169 | `	char *zDup;          /* Copy of argument name */` |
|       - | 4170 | `	sxi32 rc;` |
|       - | 4171 |  |
|   49808 | 4172 | `	pIn = pGen->pIn;` |
|   49808 | 4173 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 4174 | `	/* Process arguments one after one */` |
|   63016 | 4175 | `	for(;;){` |
|  126034 | 4176 | `		if( pIn >= pEnd ){` |
|       - | 4177 | `			/* No more arguments to process */` |
|   49806 | 4178 | `			break;` |
|       - | 4179 | `		}` |
|   76230 | 4180 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   76230 | 4181 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 4182 | `		/* Detect nullable prefix '?' on type hints */` |
|   76230 | 4183 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      11 | 4184 | `			sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|      11 | 4185 | `			pIn++;` |
|       5 | 4186 | `		}` |
|       - | 4187 | `		/* Skip leading namespace separator '\' on FQN type hints like \Throwable */` |
|   76230 | 4188 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       5 | 4189 | `			pIn++;` |
|       2 | 4190 | `		}` |
|   76230 | 4191 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|   51872 | 4192 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   46684 | 4193 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   46684 | 4194 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 4195 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   46684 | 4196 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 4197 | `					sArg.nType = MEMOBJ_BOOL;` |
|   46684 | 4198 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   12970 | 4199 | `					sArg.nType = MEMOBJ_INT;` |
|   40200 | 4200 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   33714 | 4201 | `					sArg.nType = MEMOBJ_STRING;` |
|   16859 | 4202 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 4203 | `					sArg.nType = MEMOBJ_REAL;` |
|     ! 0 | 4204 | `				}else{` |
|       4 | 4205 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 4206 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 4207 | `						&pIn->sData);` |
|       - | 4208 | `				}` |
|   23343 | 4209 | `			}else{` |
|    5190 | 4210 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 4211 | `				char *zDupLocal;` |
|       - | 4212 | `				/* Argument must be a class instance,record that*/` |
|    5190 | 4213 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    5190 | 4214 | `				if( zDupLocal ){` |
|    5190 | 4215 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    5190 | 4216 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2594 | 4217 | `				}` |
|       - | 4218 | `			}` |
|   51872 | 4219 | `			pIn++;` |
|   25935 | 4220 | `		}` |
|   76230 | 4221 | `		if( pIn >= pEnd ){` |
|     ! 0 | 4222 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 4223 | `			return rc;` |
|       - | 4224 | `		}` |
|   76230 | 4225 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 4226 | `			/* Pass by reference,record that */` |
|    2618 | 4227 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    2618 | 4228 | `			pIn++;` |
|    1308 | 4229 | `		}` |
|   76230 | 4230 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - | 4231 | `			/* Variadic parameter: ...$args */` |
|      23 | 4232 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      23 | 4233 | `			pIn++;` |
|      11 | 4234 | `		}` |
|   76230 | 4235 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4236 | `			/* Invalid argument */` |
|     ! 0 | 4237 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 4238 | `			return rc;` |
|       - | 4239 | `		}` |
|   76230 | 4240 | `		pIn++; /* Jump the dollar sign */` |
|       - | 4241 | `		/* Copy argument name */` |
|   76230 | 4242 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   76230 | 4243 | `		if( zDup == 0 ){` |
|     ! 0 | 4244 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 4245 | `			return SXERR_ABORT;` |
|       - | 4246 | `		}` |
|   76230 | 4247 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   76230 | 4248 | `		pIn++;` |
|   76230 | 4249 | `		if( pIn < pEnd ){` |
|   47174 | 4250 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 4251 | `				SyToken *pDefend;` |
|   41486 | 4252 | `				sxi32 iNest = 0;` |
|   41486 | 4253 | `				pIn++; /* Jump the equal sign */` |
|   41486 | 4254 | `				pDefend = pIn;` |
|       - | 4255 | `				/* Process the default value associated with this argument */` |
|   88152 | 4256 | `				while( pDefend < pEnd ){` |
|   67404 | 4257 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   20738 | 4258 | `						break;` |
|       - | 4259 | `					}` |
|   46668 | 4260 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 4261 | `						/* Increment nesting level */` |
|    2594 | 4262 | `						iNest++;` |
|   45372 | 4263 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 4264 | `						/* Decrement nesting level */` |
|    2594 | 4265 | `						iNest--;` |
|    1296 | 4266 | `					}` |
|   46668 | 4267 | `					pDefend++;` |
|       2 | 4268 | `				}` |
|   41486 | 4269 | `				if( pIn >= pDefend ){` |
|       3 | 4270 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 4271 | `					return rc;` |
|       - | 4272 | `				}` |
|       - | 4273 | `				/* Process default value */` |
|   41484 | 4274 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   41484 | 4275 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 4276 | `					return rc;` |
|       - | 4277 | `				}` |
|       - | 4278 | `				/* Point beyond the default value */` |
|   41484 | 4279 | `				pIn = pDefend;` |
|   20741 | 4280 | `			}` |
|   47172 | 4281 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 4282 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 4283 | `				return rc;` |
|       - | 4284 | `			}` |
|   47172 | 4285 | `			pIn++; /* Jump the trailing comma */` |
|   23585 | 4286 | `		}` |
|       - | 4287 | `		/* Append argument signature */` |
|   76228 | 4288 | `		if( sArg.nType > 0 ){` |
|   51870 | 4289 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 4290 | `				/* Class name */` |
|    5190 | 4291 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2596 | 4292 | `			}else{` |
|       - | 4293 | `				int c;` |
|   46682 | 4294 | `				c = 'n'; /* cc warning */` |
|       - | 4295 | `				/* Type leading character */` |
|   46682 | 4296 | `				switch(sArg.nType){` |
|     ! 0 | 4297 | `				case MEMOBJ_HASHMAP:` |
|       - | 4298 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 4299 | `					c = 'h';` |
|     ! 0 | 4300 | `					break;` |
|    6484 | 4301 | `				case MEMOBJ_INT:` |
|       - | 4302 | `					/* Integer */` |
|   12970 | 4303 | `					c = 'i';` |
|   12970 | 4304 | `					break;` |
|     ! 0 | 4305 | `				case MEMOBJ_BOOL:` |
|       - | 4306 | `					/* Bool */` |
|     ! 0 | 4307 | `					c = 'b';` |
|     ! 0 | 4308 | `					break;` |
|     ! 0 | 4309 | `				case MEMOBJ_REAL:` |
|       - | 4310 | `					/* Float */` |
|     ! 0 | 4311 | `					c = 'f';` |
|     ! 0 | 4312 | `					break;` |
|   16856 | 4313 | `				case MEMOBJ_STRING:` |
|       - | 4314 | `					/* String */` |
|   33714 | 4315 | `					c = 's';` |
|   33712 | 4316 | `					break;` |
|     ! 0 | 4317 | `				default:` |
|     ! 0 | 4318 | `					break;` |
|       - | 4319 | `				}` |
|   46682 | 4320 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 4321 | `			}` |
|   25936 | 4322 | `		}else{` |
|       - | 4323 | `			/* No type is associated with this parameter which mean` |
|       - | 4324 | `			 * that this function is not condidate for overloading.` |
|       - | 4325 | `			 */` |
|   24360 | 4326 | `			SyBlobRelease(&sSig);` |
|       - | 4327 | `		}` |
|       - | 4328 | `		/* Save in the argument set */` |
|   76228 | 4329 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 4330 | `	}` |
|   49806 | 4331 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 4332 | `		/* Save function signature */` |
|   31128 | 4333 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   15563 | 4334 | `	}` |
|   49806 | 4335 | `	return SXRET_OK;` |
|   24905 | 4336 |  |
|       - | 4337 | `/*` |
|       - | 4338 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 4339 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 4340 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 4341 | ` */` |
|  138440 | 4342 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 4343 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 4344 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 4345 | `	)` |
|       2 | 4346 |  |
|       - | 4347 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 4348 | `	GenBlock *pBlock;` |
|       - | 4349 | `	sxu32 nGotoOfft;` |
|       - | 4350 | `	sxi32 rc;` |
|       - | 4351 | `	/* Attach the new function */` |
|  138442 | 4352 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  138442 | 4353 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4354 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 4355 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4356 | `		return SXERR_ABORT;` |
|       - | 4357 | `	}` |
|  138442 | 4358 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 4359 | `	/* Swap bytecode containers */` |
|  138442 | 4360 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  138442 | 4361 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 4362 | `	/* Compile the body */` |
|  138442 | 4363 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 4364 | `	/* Fix exception jumps now the destination is resolved */` |
|  138442 | 4365 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4366 | `	/* Emit the final return if not yet done */` |
|  138442 | 4367 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4368 | `	/* Fix gotos jumps now the destination is resolved */` |
|  138442 | 4369 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 4370 | `		rc = SXERR_ABORT;` |
|     ! 0 | 4371 | `	}` |
|  138442 | 4372 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 4373 | `	/* Restore the default container */` |
|  138442 | 4374 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4375 | `	/* Leave function block */` |
|  138442 | 4376 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  138442 | 4377 | `	if( rc == SXERR_ABORT ){` |
|       - | 4378 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4379 | `		return SXERR_ABORT;` |
|       - | 4380 | `	}` |
|       - | 4381 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - | 4382 | `	{` |
|  138442 | 4383 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - | 4384 | `		sxu32 i;` |
| 2874472 | 4385 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 2736048 | 4386 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      18 | 4387 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      18 | 4388 | `				break;` |
|       - | 4389 | `			}` |
| 1368017 | 4390 | `		}` |
|       - | 4391 | `	}` |
|       - | 4392 | `	/* All done, function body compiled */` |
|  138442 | 4393 | `	return SXRET_OK;` |
|   69222 | 4394 |  |
|       - | 4395 | `/*` |
|       - | 4396 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 4397 | ` * According to the PHP language reference manual.` |
|       - | 4398 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 4399 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 4400 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 4401 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4402 | ` *  Functions need not be defined before they are referenced.` |
|       - | 4403 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 4404 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 4405 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 4406 | ` *  calls with over 32-64 recursion levels.` |
|       - | 4407 | ` *` |
|       - | 4408 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 4409 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 4410 | ` * on these extension.` |
|       - | 4411 | ` */` |
|       - | 4412 | `/*` |
|       - | 4413 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - | 4414 | ` */` |
|       6 | 4415 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       1 | 4416 |  |
|       - | 4417 | `	sxu32 i;` |
|      31 | 4418 | `	for( i = 0; i < n; i++ ){` |
|      25 | 4419 | `		int a = zA[i], b = zB[i];` |
|      25 | 4420 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      25 | 4421 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      25 | 4422 | `		if( a != b ) return a - b;` |
|      13 | 4423 | `	}` |
|       7 | 4424 | `	return 0;` |
|       4 | 4425 |  |
|       - | 4426 | `/*` |
|       - | 4427 | ` * Helper: set the return type to a class/self/parent/static sentinel.` |
|       - | 4428 | ` */` |
|       2 | 4429 | `static void GenStateSetReturnClass(ph7_gen_state *pGen, ph7_vm_func *pFunc, const char *zName, sxu32 nByte)` |
|       1 | 4430 |  |
|       3 | 4431 | `	char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator, zName, nByte);` |
|       3 | 4432 | `	if( zDup ){` |
|       3 | 4433 | `		pFunc->nReturnType = SXU32_HIGH;` |
|       3 | 4434 | `		SyStringInitFromBuf(&pFunc->sReturnClass, zDup, nByte);` |
|       1 | 4435 | `	}` |
|       3 | 4436 |  |
|       - | 4437 | `/*` |
|       - | 4438 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - | 4439 | `` * pGen->pIn should point to the token after `)`.`` |
|       - | 4440 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - | 4441 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - | 4442 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, and nullable `: ?type`.`` |
|       - | 4443 | ` */` |
|  159224 | 4444 | `static void GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 | 4445 |  |
|  159226 | 4446 | `	SyToken *pCur = pGen->pIn;` |
|  159226 | 4447 | `	pFunc->nReturnType = 0;` |
|  159226 | 4448 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  159226 | 4449 | `	if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_COLON) == 0 ){` |
|  159172 | 4450 | `		return; /* No return type */` |
|       - | 4451 | `	}` |
|      55 | 4452 | `	pCur++; /* Skip ':' */` |
|      55 | 4453 | `	if( pCur >= pGen->pEnd ){` |
|     ! 0 | 4454 | `		pGen->pIn = pCur;` |
|     ! 0 | 4455 | `		return;` |
|       - | 4456 | `	}` |
|       - | 4457 | `	/* Handle nullable prefix '?' (tokenized as PH7_TK_OP with '?' operator) */` |
|      55 | 4458 | `	if( (pCur->nType & PH7_TK_OP) && pCur->sData.nByte == 1 && pCur->sData.zString[0] == '?' ){` |
|       7 | 4459 | `		pCur++;` |
|       7 | 4460 | `		if( pCur >= pGen->pEnd ){` |
|     ! 0 | 4461 | `			pGen->pIn = pCur;` |
|     ! 0 | 4462 | `			return;` |
|       - | 4463 | `		}` |
|       3 | 4464 | `	}` |
|      55 | 4465 | `	if( pCur->nType & PH7_TK_KEYWORD ){` |
|      49 | 4466 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pCur->pUserData));` |
|      49 | 4467 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|       3 | 4468 | `			pFunc->nReturnType = MEMOBJ_HASHMAP;` |
|      48 | 4469 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       3 | 4470 | `			pFunc->nReturnType = MEMOBJ_BOOL;` |
|      46 | 4471 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|      17 | 4472 | `			pFunc->nReturnType = MEMOBJ_INT;` |
|      37 | 4473 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|      25 | 4474 | `			pFunc->nReturnType = MEMOBJ_STRING;` |
|      17 | 4475 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       3 | 4476 | `			pFunc->nReturnType = MEMOBJ_REAL;` |
|       4 | 4477 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT \|\| nKey == PH7_TKWRD_STATIC ){` |
|       - | 4478 | `			/* self/parent/static — store as class sentinel */` |
|       3 | 4479 | `			GenStateSetReturnClass(pGen, pFunc, pCur->sData.zString, pCur->sData.nByte);` |
|       1 | 4480 | `		}` |
|      49 | 4481 | `		pCur++;` |
|      31 | 4482 | `	}else if( pCur->nType & PH7_TK_ID ){` |
|       7 | 4483 | `		SyString *pType = &pCur->sData;` |
|       7 | 4484 | `		if( pType->nByte == 4 && SyMemcmpNoCase(pType->zString, "void", 4) == 0 ){` |
|       7 | 4485 | `			pFunc->nReturnType = MEMOBJ_VOID;` |
|       4 | 4486 | `		}else{` |
|       - | 4487 | `			/* Class/interface name */` |
|     ! 0 | 4488 | `			GenStateSetReturnClass(pGen, pFunc, pType->zString, pType->nByte);` |
|       - | 4489 | `		}` |
|       7 | 4490 | `		pCur++;` |
|       3 | 4491 | `	}` |
|      55 | 4492 | `	pGen->pIn = pCur;` |
|   79614 | 4493 |  |
|       - | 4494 |  |
|   34360 | 4495 | `static sxi32 GenStateCompileFunc(` |
|       - | 4496 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4497 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 4498 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 4499 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 4500 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 4501 | `	)` |
|       2 | 4502 |  |
|       - | 4503 | `	ph7_vm_func *pFunc;` |
|       - | 4504 | `	SyToken *pEnd;` |
|       - | 4505 | `	sxu32 nLine;` |
|       - | 4506 | `	char *zName;` |
|       - | 4507 | `	sxi32 rc;` |
|       - | 4508 | `	/* Extract line number */` |
|   34362 | 4509 | `	nLine = pGen->pIn->nLine;` |
|       - | 4510 | `	/* Jump the left parenthesis '(' */` |
|   34362 | 4511 | `	pGen->pIn++;` |
|       - | 4512 | `	/* Delimit the function signature */` |
|   34362 | 4513 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   34362 | 4514 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4515 | `		/* Syntax error */` |
|       7 | 4516 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 4517 | `		if( rc == SXERR_ABORT ){` |
|       - | 4518 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4519 | `			return SXERR_ABORT;` |
|       - | 4520 | `		}` |
|       7 | 4521 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 4522 | `		return SXRET_OK;` |
|       - | 4523 | `	}` |
|       - | 4524 | `	/* Create the function state */` |
|   34356 | 4525 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   34356 | 4526 | `	if( pFunc == 0 ){` |
|     ! 0 | 4527 | `		goto OutOfMem;` |
|       - | 4528 | `	}` |
|       - | 4529 | `	/* Build the function name, prepending namespace if active */` |
|   34363 | 4530 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 4531 | `		SyBlob sFQN;` |
|       - | 4532 | `		sxu32 nLen;` |
|      16 | 4533 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 | 4534 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 | 4535 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 | 4536 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 | 4537 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 | 4538 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 | 4539 | `		SyBlobRelease(&sFQN);` |
|      16 | 4540 | `		if( zName == 0 ){` |
|     ! 0 | 4541 | `			goto OutOfMem;` |
|       - | 4542 | `		}` |
|      16 | 4543 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 | 4544 | `	}else{` |
|   34342 | 4545 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   34342 | 4546 | `		if( zName == 0 ){` |
|     ! 0 | 4547 | `			goto OutOfMem;` |
|       - | 4548 | `		}` |
|   34342 | 4549 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 4550 | `	}` |
|   34356 | 4551 | `	if( pGen->pIn < pEnd ){` |
|       - | 4552 | `		/* Collect function arguments */` |
|   23808 | 4553 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   23808 | 4554 | `		if( rc == SXERR_ABORT ){` |
|       - | 4555 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4556 | `			return SXERR_ABORT;` |
|       - | 4557 | `		}` |
|   11903 | 4558 | `	}` |
|       - | 4559 | `	/* Point past ')' and parse optional return type ': type' */` |
|   34356 | 4560 | `	pGen->pIn = &pEnd[1];` |
|   34356 | 4561 | `	GenStateParseReturnType(pGen, pFunc);` |
|   34356 | 4562 | `	if( bHandleClosure ){` |
|       - | 4563 | `		ph7_vm_func_closure_env sEnv;` |
|     168 | 4564 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     166 | 4565 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      91 | 4566 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      14 | 4567 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 4568 | `				/* Closure,record environment variable */` |
|      14 | 4569 | `				pGen->pIn++;` |
|      14 | 4570 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 4571 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 4572 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4573 | `						return SXERR_ABORT;` |
|       - | 4574 | `					}` |
|     ! 0 | 4575 | `				}` |
|      14 | 4576 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 4577 | `				/* Compile until we hit the first closing parenthesis */` |
|      28 | 4578 | `				while( pGen->pIn < pGen->pEnd ){` |
|      28 | 4579 | `					int iFlagsLocal = 0;` |
|      28 | 4580 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      14 | 4581 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      14 | 4582 | `						break;` |
|       - | 4583 | `					}` |
|      16 | 4584 | `					nLineLocal = pGen->pIn->nLine;` |
|      16 | 4585 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 4586 | `						/* Pass by reference,record that */` |
|     ! 0 | 4587 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 4588 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 4589 | `							);` |
|     ! 0 | 4590 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 4591 | `						pGen->pIn++;` |
|     ! 0 | 4592 | `					}` |
|      14 | 4593 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      16 | 4594 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 4595 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 4596 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 4597 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4598 | `								return SXERR_ABORT;` |
|       - | 4599 | `							}` |
|       - | 4600 | `							/* Find the closing parenthesis */` |
|     ! 0 | 4601 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 4602 | `								pGen->pIn++;` |
|     ! 0 | 4603 | `							}` |
|     ! 0 | 4604 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 4605 | `								pGen->pIn++;` |
|     ! 0 | 4606 | `							}` |
|     ! 0 | 4607 | `							break;` |
|       - | 4608 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 4609 | `					}else{` |
|       - | 4610 | `						SyString *pNameLocal;` |
|       - | 4611 | `						char *zDup;` |
|       - | 4612 | `						/* Duplicate variable name */` |
|      16 | 4613 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      16 | 4614 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      16 | 4615 | `						if( zDup ){` |
|       - | 4616 | `							/* Zero the structure */` |
|      16 | 4617 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 | 4618 | `							sEnv.iFlags = iFlagsLocal;` |
|      16 | 4619 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 | 4620 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      16 | 4621 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 4622 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 4623 | `									got_this = 1;` |
|     ! 0 | 4624 | `							}` |
|       - | 4625 | `							/* Save imported variable */` |
|      16 | 4626 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       9 | 4627 | `						}else{` |
|     ! 0 | 4628 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4629 | `							 return SXERR_ABORT;` |
|       - | 4630 | `						}` |
|       - | 4631 | `					}` |
|      16 | 4632 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      18 | 4633 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4634 | `						/* Ignore trailing commas */` |
|       3 | 4635 | `						pGen->pIn++;` |
|       1 | 4636 | `					}` |
|       2 | 4637 | `				}` |
|      14 | 4638 | `				if( !got_this ){` |
|       - | 4639 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 4640 | `					 * available to the closure environment.` |
|       - | 4641 | `					 */` |
|      14 | 4642 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      14 | 4643 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      14 | 4644 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      14 | 4645 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      14 | 4646 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       6 | 4647 | `				}` |
|      14 | 4648 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 4649 | `					/* Mark as closure */` |
|      14 | 4650 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       6 | 4651 | `				}` |
|       6 | 4652 | `		}` |
|      83 | 4653 | `	}` |
|       - | 4654 | `	/* Compile the body */` |
|   34356 | 4655 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   34356 | 4656 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4657 | `		return SXERR_ABORT;` |
|       - | 4658 | `	}` |
|   34356 | 4659 | `	if( ppFunc ){` |
|     168 | 4660 | `		*ppFunc = pFunc;` |
|      83 | 4661 | `	}` |
|   34356 | 4662 | `	rc = SXRET_OK;` |
|   34356 | 4663 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 4664 | `		/* Finally register the function */` |
|   34344 | 4665 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   17171 | 4666 | `	}` |
|   34356 | 4667 | `	if( rc == SXRET_OK ){` |
|   34356 | 4668 | `		return SXRET_OK;` |
|       - | 4669 | `	}` |
|       - | 4670 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 4671 | `OutOfMem:` |
|       - | 4672 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 4673 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 4674 | `	 */` |
|     ! 0 | 4675 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 4676 | `	return SXERR_ABORT;` |
|   17182 | 4677 |  |
|       - | 4678 | `/*` |
|       - | 4679 | ` * Compile a standard PHP function.` |
|       - | 4680 | ` *  Refer to the block-comment above for more information.` |
|       - | 4681 | ` */` |
|   34200 | 4682 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 4683 |  |
|       - | 4684 | `	SyString *pName;` |
|       - | 4685 | `	sxi32 iFlags;` |
|       - | 4686 | `	sxu32 nLine;` |
|       - | 4687 | `	sxi32 rc;` |
|       - | 4688 |  |
|   34202 | 4689 | `	nLine = pGen->pIn->nLine;` |
|   34202 | 4690 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   34202 | 4691 | `	iFlags = 0;` |
|   34202 | 4692 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4693 | `		/* Return by reference,remember that */` |
|       7 | 4694 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4695 | `		/* Jump the '&' token */` |
|       7 | 4696 | `		pGen->pIn++;` |
|       3 | 4697 | `	}` |
|   34202 | 4698 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4699 | `		/* Invalid function name */` |
|       5 | 4700 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 4701 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4702 | `			return SXERR_ABORT;` |
|       - | 4703 | `		}` |
|       - | 4704 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 4705 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 4706 | `			pGen->pIn++;` |
|       1 | 4707 | `		}` |
|       5 | 4708 | `		return SXRET_OK;` |
|       - | 4709 | `	}` |
|   34198 | 4710 | `	pName = &pGen->pIn->sData;` |
|   34198 | 4711 | `	nLine = pGen->pIn->nLine;` |
|       - | 4712 | `	/* Jump the function name */` |
|   34198 | 4713 | `	pGen->pIn++;` |
|   34198 | 4714 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4715 | `		/* Syntax error */` |
|       3 | 4716 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 4717 | `		if( rc == SXERR_ABORT ){` |
|       - | 4718 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4719 | `			return SXERR_ABORT;` |
|       - | 4720 | `		}` |
|       - | 4721 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 4722 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4723 | `			pGen->pIn++;` |
|     ! 0 | 4724 | `		}` |
|       3 | 4725 | `		return SXRET_OK;` |
|       - | 4726 | `	}` |
|       - | 4727 | `	/* Compile function body */` |
|   34196 | 4728 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   34196 | 4729 | `	return rc;` |
|   17102 | 4730 |  |
|       - | 4731 | `/*` |
|       - | 4732 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 4733 | ` * According to the PHP language reference manual` |
|       - | 4734 | ` *  Visibility:` |
|       - | 4735 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 4736 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 4737 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 4738 | ` *  Members declared protected can be accessed only within the class` |
|       - | 4739 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 4740 | ` *  may only be accessed by the class that defines the member.` |
|       - | 4741 | ` */` |
|  158790 | 4742 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 4743 |  |
|  158792 | 4744 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    7838 | 4745 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  150956 | 4746 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   18174 | 4747 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 4748 | `	}` |
|       - | 4749 | `	/* Assume public by default */` |
|  132784 | 4750 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   79397 | 4751 |  |
|       - | 4752 | `/*` |
|       - | 4753 | ` * Compile a class constant.` |
|       - | 4754 | ` * According to the PHP language reference manual` |
|       - | 4755 | ` *  Class Constants` |
|       - | 4756 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 4757 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 4758 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 4759 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 4760 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 4761 | ` *   It's also possible for interfaces to have constants.` |
|       - | 4762 | ` * Symisc eXtension.` |
|       - | 4763 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 4764 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4765 | ` *  Example:` |
|       - | 4766 | ` *   class Test{` |
|       - | 4767 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4768 | ` *   };` |
|       - | 4769 | ` *   var_dump(TEST::MyConst);` |
|       - | 4770 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4771 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4772 | ` */` |
|      10 | 4773 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4774 |  |
|      12 | 4775 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4776 | `	SySet *pInstrContainer;` |
|       - | 4777 | `	ph7_class_attr *pCons;` |
|       - | 4778 | `	SyString *pName;` |
|       - | 4779 | `	sxi32 rc;` |
|       - | 4780 | `	/* Extract visibility level */` |
|      12 | 4781 | `	iProtection = GetProtectionLevel(iProtection);` |
|      12 | 4782 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       5 | 4783 | `loop:` |
|       - | 4784 | `	/* Mark as constant */` |
|      12 | 4785 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      12 | 4786 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4787 | `		/* Invalid constant name */` |
|     ! 0 | 4788 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 4789 | `		if( rc == SXERR_ABORT ){` |
|       - | 4790 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4791 | `			return SXERR_ABORT;` |
|       - | 4792 | `		}` |
|     ! 0 | 4793 | `		goto Synchronize;` |
|       - | 4794 | `	}` |
|       - | 4795 | `	/* Peek constant name */` |
|      12 | 4796 | `	pName = &pGen->pIn->sData;` |
|       - | 4797 | `	/* Make sure the constant name isn't reserved */` |
|      12 | 4798 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 4799 | `		/* Reserved constant name */` |
|     ! 0 | 4800 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 4801 | `		if( rc == SXERR_ABORT ){` |
|       - | 4802 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4803 | `			return SXERR_ABORT;` |
|       - | 4804 | `		}` |
|     ! 0 | 4805 | `		goto Synchronize;` |
|       - | 4806 | `	}` |
|       - | 4807 | `	/* Advance the stream cursor */` |
|      12 | 4808 | `	pGen->pIn++;` |
|      12 | 4809 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 4810 | `		/* Invalid declaration */` |
|     ! 0 | 4811 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 4812 | `		if( rc == SXERR_ABORT ){` |
|       - | 4813 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4814 | `			return SXERR_ABORT;` |
|       - | 4815 | `		}` |
|     ! 0 | 4816 | `		goto Synchronize;` |
|       - | 4817 | `	}` |
|      12 | 4818 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 4819 | `	/* Allocate a new class attribute */` |
|      12 | 4820 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      12 | 4821 | `	if( pCons == 0 ){` |
|     ! 0 | 4822 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4823 | `		return SXERR_ABORT;` |
|       - | 4824 | `	}` |
|       - | 4825 | `	/* Swap bytecode container */` |
|      12 | 4826 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      12 | 4827 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 4828 | `	/* Compile constant value.` |
|       - | 4829 | `	 */` |
|      12 | 4830 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      12 | 4831 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 4832 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 4833 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4834 | `			return SXERR_ABORT;` |
|       - | 4835 | `		}` |
|       1 | 4836 | `	}` |
|       - | 4837 | `	/* Emit the done instruction */` |
|      12 | 4838 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      12 | 4839 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      12 | 4840 | `	if( rc == SXERR_ABORT ){` |
|       - | 4841 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4842 | `		return SXERR_ABORT;` |
|       - | 4843 | `	}` |
|       - | 4844 | `	/* All done,install the constant */` |
|      12 | 4845 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      12 | 4846 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4847 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4848 | `		return SXERR_ABORT;` |
|       - | 4849 | `	}` |
|      12 | 4850 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4851 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 4852 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4853 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 4854 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4855 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4856 | `				pTok--;` |
|     ! 0 | 4857 | `			}` |
|     ! 0 | 4858 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4859 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 4860 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4861 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4862 | `				return SXERR_ABORT;` |
|       - | 4863 | `			}` |
|     ! 0 | 4864 | `		}else{` |
|     ! 0 | 4865 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 4866 | `				goto loop;` |
|       - | 4867 | `			}` |
|       - | 4868 | `		}` |
|     ! 0 | 4869 | `	}` |
|      12 | 4870 | `	return SXRET_OK;` |
|     ! 0 | 4871 | `Synchronize:` |
|       - | 4872 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4873 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 4874 | `		pGen->pIn++;` |
|     ! 0 | 4875 | `	}` |
|     ! 0 | 4876 | `	return SXERR_CORRUPT;` |
|       7 | 4877 |  |
|       - | 4878 | `/*` |
|       - | 4879 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 4880 | ` * According to the PHP language reference manual` |
|       - | 4881 | ` *  Properties` |
|       - | 4882 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 4883 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 4884 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 4885 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 4886 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 4887 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 4888 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 4889 | ` * Symisc eXtension.` |
|       - | 4890 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 4891 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4892 | ` *  Example:` |
|       - | 4893 | ` *   class Test{` |
|       - | 4894 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4895 | ` *   };` |
|       - | 4896 | ` *   var_dump(TEST::myVar);` |
|       - | 4897 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4898 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4899 | ` */` |
|   33908 | 4900 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4901 |  |
|   33910 | 4902 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4903 | `	ph7_class_attr *pAttr;` |
|       - | 4904 | `	SyString *pName;` |
|       - | 4905 | `	sxi32 rc;` |
|       - | 4906 | `	/* Extract visibility level */` |
|   33910 | 4907 | `	iProtection = GetProtectionLevel(iProtection);` |
|   16954 | 4908 | `loop:` |
|   33910 | 4909 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   33910 | 4910 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 4911 | `		/* Invalid attribute name */` |
|     ! 0 | 4912 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 4913 | `		if( rc == SXERR_ABORT ){` |
|       - | 4914 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4915 | `			return SXERR_ABORT;` |
|       - | 4916 | `		}` |
|     ! 0 | 4917 | `		goto Synchronize;` |
|       - | 4918 | `	}` |
|       - | 4919 | `	/* Peek attribute name */` |
|   33910 | 4920 | `	pName = &pGen->pIn->sData;` |
|       - | 4921 | `	/* Advance the stream cursor */` |
|   33910 | 4922 | `	pGen->pIn++;` |
|   33910 | 4923 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 4924 | `		/* Invalid declaration */` |
|       3 | 4925 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 4926 | `		if( rc == SXERR_ABORT ){` |
|       - | 4927 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4928 | `			return SXERR_ABORT;` |
|       - | 4929 | `		}` |
|       3 | 4930 | `		goto Synchronize;` |
|       - | 4931 | `	}` |
|       - | 4932 | `	/* Allocate a new class attribute */` |
|   33908 | 4933 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   33908 | 4934 | `	if( pAttr == 0 ){` |
|     ! 0 | 4935 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 4936 | `		return SXERR_ABORT;` |
|       - | 4937 | `	}` |
|   33908 | 4938 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 4939 | `		SySet *pInstrContainer;` |
|   10530 | 4940 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 4941 | `		/* Swap bytecode container */` |
|   10530 | 4942 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   10530 | 4943 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 4944 | `		/* Compile attribute value.` |
|       - | 4945 | `		 */` |
|   10530 | 4946 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   10530 | 4947 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 4948 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 4949 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4950 | `				return SXERR_ABORT;` |
|       - | 4951 | `			}` |
|     ! 0 | 4952 | `		}` |
|       - | 4953 | `		/* Emit the done instruction */` |
|   10530 | 4954 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   10530 | 4955 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5264 | 4956 | `	}` |
|       - | 4957 | `	/* All done,install the attribute */` |
|   33908 | 4958 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   33908 | 4959 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4960 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4961 | `		return SXERR_ABORT;` |
|       - | 4962 | `	}` |
|   33908 | 4963 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4964 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 4965 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4966 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 4967 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4968 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4969 | `				pTok--;` |
|     ! 0 | 4970 | `			}` |
|     ! 0 | 4971 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4972 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4973 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4974 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4975 | `				return SXERR_ABORT;` |
|       - | 4976 | `			}` |
|     ! 0 | 4977 | `		}else{` |
|     ! 0 | 4978 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 4979 | `				goto loop;` |
|       - | 4980 | `			}` |
|       - | 4981 | `		}` |
|     ! 0 | 4982 | `	}` |
|   33908 | 4983 | `	return SXRET_OK;` |
|       1 | 4984 | `Synchronize:` |
|       - | 4985 | `	/* Synchronize with the first semi-colon */` |
|       5 | 4986 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 4987 | `		pGen->pIn++;` |
|       1 | 4988 | `	}` |
|       3 | 4989 | `	return SXERR_CORRUPT;` |
|   16956 | 4990 |  |
|       - | 4991 | `/*` |
|       - | 4992 | ` * Compile a class method.` |
|       - | 4993 | ` *` |
|       - | 4994 | ` * Refer to the official documentation for more information` |
|       - | 4995 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 4996 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 4997 | ` * overloading and many more.` |
|       - | 4998 | ` */` |
|  124872 | 4999 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 5000 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 5001 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 5002 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 5003 | `	int doBody,          /* TRUE to process method body */` |
|       - | 5004 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 5005 | `	)` |
|       2 | 5006 |  |
|  124874 | 5007 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5008 | `	ph7_class_method *pMeth;` |
|       - | 5009 | `	sxi32 iFuncFlags;` |
|       - | 5010 | `	SyString *pName;` |
|       - | 5011 | `	SyToken *pEnd;` |
|       - | 5012 | `	sxi32 rc;` |
|       - | 5013 | `	/* Extract visibility level */` |
|  124874 | 5014 | `	iProtection = GetProtectionLevel(iProtection);` |
|  124874 | 5015 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  124874 | 5016 | `	iFuncFlags = 0;` |
|  124874 | 5017 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5018 | `		/* Invalid method name */` |
|     ! 0 | 5019 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 5020 | `		if( rc == SXERR_ABORT ){` |
|       - | 5021 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5022 | `			return SXERR_ABORT;` |
|       - | 5023 | `		}` |
|     ! 0 | 5024 | `		goto Synchronize;` |
|       - | 5025 | `	}` |
|  124874 | 5026 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 5027 | `		/* Return by reference,remember that */` |
|     ! 0 | 5028 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 5029 | `		/* Jump the '&' token */` |
|     ! 0 | 5030 | `		pGen->pIn++;` |
|     ! 0 | 5031 | `	}` |
|  124874 | 5032 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5033 | `		/* Invalid method name */` |
|     ! 0 | 5034 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 5035 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5036 | `			return SXERR_ABORT;` |
|       - | 5037 | `		}` |
|     ! 0 | 5038 | `		goto Synchronize;` |
|       - | 5039 | `	}` |
|       - | 5040 | `	/* Peek method name */` |
|  124874 | 5041 | `	pName = &pGen->pIn->sData;` |
|  124874 | 5042 | `	nLine = pGen->pIn->nLine;` |
|       - | 5043 | `	/* Jump the method name */` |
|  124874 | 5044 | `	pGen->pIn++;` |
|  124874 | 5045 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 5046 | `		/* Abstract method */` |
|   20786 | 5047 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 5048 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5049 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 5050 | `				&pClass->sName,pName);` |
|     ! 0 | 5051 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5052 | `				return SXERR_ABORT;` |
|       - | 5053 | `			}` |
|     ! 0 | 5054 | `		}` |
|       - | 5055 | `		/* Assemble method signature only */` |
|   20786 | 5056 | `		doBody = FALSE;` |
|   10392 | 5057 | `	}` |
|  124874 | 5058 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 5059 | `		/* Syntax error */` |
|     ! 0 | 5060 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 5061 | `		if( rc == SXERR_ABORT ){` |
|       - | 5062 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5063 | `			return SXERR_ABORT;` |
|       - | 5064 | `		}` |
|     ! 0 | 5065 | `		goto Synchronize;` |
|       - | 5066 | `	}` |
|       - | 5067 | `	/* Allocate a new class_method instance */` |
|  124874 | 5068 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  124874 | 5069 | `	if( pMeth == 0 ){` |
|     ! 0 | 5070 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5071 | `		return SXERR_ABORT;` |
|       - | 5072 | `	}` |
|       - | 5073 | `	/* Jump the left parenthesis '(' */` |
|  124874 | 5074 | `	pGen->pIn++;` |
|  124874 | 5075 | `	pEnd = 0; /* cc warning */` |
|       - | 5076 | `	/* Delimit the method signature */` |
|  124874 | 5077 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  124874 | 5078 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5079 | `		/* Syntax error */` |
|       3 | 5080 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 5081 | `		if( rc == SXERR_ABORT ){` |
|       - | 5082 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5083 | `			return SXERR_ABORT;` |
|       - | 5084 | `		}` |
|       3 | 5085 | `		goto Synchronize;` |
|       - | 5086 | `	}` |
|  124872 | 5087 | `	if( pGen->pIn < pEnd ){` |
|       - | 5088 | `		/* Collect method arguments */` |
|   26002 | 5089 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   26002 | 5090 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5091 | `			return SXERR_ABORT;` |
|       - | 5092 | `		}` |
|   13000 | 5093 | `	}` |
|       - | 5094 | `	/* Point past ')' and parse optional return type ': type' */` |
|  124872 | 5095 | `	pGen->pIn = &pEnd[1];` |
|  124872 | 5096 | `	GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  124872 | 5097 | `	if( doBody ){` |
|       - | 5098 | `		/* Compile method body */` |
|  104088 | 5099 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  104088 | 5100 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5101 | `			return SXERR_ABORT;` |
|       - | 5102 | `		}` |
|   52045 | 5103 | `	}else{` |
|       - | 5104 | `		/* Only method signature is allowed */` |
|   20786 | 5105 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 5106 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5107 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 5108 | `				if( rc == SXERR_ABORT ){` |
|       - | 5109 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5110 | `					return SXERR_ABORT;` |
|       - | 5111 | `				}` |
|     ! 0 | 5112 | `				return SXERR_CORRUPT;` |
|       - | 5113 | `			}` |
|       - | 5114 | `	}` |
|       - | 5115 | `	/* All done,install the method */` |
|  124872 | 5116 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  124872 | 5117 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5118 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5119 | `		return SXERR_ABORT;` |
|       - | 5120 | `	}` |
|  124872 | 5121 | `	return SXRET_OK;` |
|       1 | 5122 | `Synchronize:` |
|       - | 5123 | `	/* Synchronize with the first semi-colon */` |
|       7 | 5124 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 5125 | `		pGen->pIn++;` |
|       1 | 5126 | `	}` |
|       3 | 5127 | `	return SXERR_CORRUPT;` |
|   62438 | 5128 |  |
|       - | 5129 | `/*` |
|       - | 5130 | ` * Compile an object interface.` |
|       - | 5131 | ` *  According to the PHP language reference manual` |
|       - | 5132 | ` *   Object Interfaces:` |
|       - | 5133 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 5134 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 5135 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 5136 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 5137 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 5138 | ` */` |
|    7812 | 5139 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 5140 |  |
|    7814 | 5141 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5142 | `	ph7_class *pClass,*pBase;` |
|       - | 5143 | `	SyToken *pEnd,*pTmp;` |
|       - | 5144 | `	SyString *pName;` |
|       - | 5145 | `	sxi32 nKwrd;` |
|       - | 5146 | `	sxi32 rc;` |
|       - | 5147 | `	/* Jump the 'interface' keyword */` |
|    7814 | 5148 | `	pGen->pIn++;` |
|       - | 5149 | `	/* Extract interface name */` |
|    7814 | 5150 | `	pName = &pGen->pIn->sData;` |
|       - | 5151 | `	/* Advance the stream cursor */` |
|    7814 | 5152 | `	pGen->pIn++;` |
|       - | 5153 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5154 | `		SyBlob sFQN;` |
|       - | 5155 | `		SyString sFQNStr;` |
|    7814 | 5156 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    7814 | 5157 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    7814 | 5158 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    7814 | 5159 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    7814 | 5160 | `		SyBlobRelease(&sFQN);` |
|       - | 5161 | `	}` |
|    7814 | 5162 | `	if( pClass == 0 ){` |
|     ! 0 | 5163 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5164 | `		return SXERR_ABORT;` |
|       - | 5165 | `	}` |
|       - | 5166 | `	/* Mark as an interface */` |
|    7814 | 5167 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 5168 | `	/* Assume no base class is given */` |
|    7814 | 5169 | `	pBase = 0;` |
|    7814 | 5170 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 5171 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 5172 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 5173 | `			SyString *pBaseName;` |
|       - | 5174 | `			/* Extract base interface */` |
|       3 | 5175 | `			pGen->pIn++;` |
|       3 | 5176 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5177 | `				/* Syntax error */` |
|     ! 0 | 5178 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5179 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 5180 | `					pName);` |
|     ! 0 | 5181 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5182 | `				if( rc == SXERR_ABORT ){` |
|       - | 5183 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5184 | `					return SXERR_ABORT;` |
|       - | 5185 | `				}` |
|     ! 0 | 5186 | `				return SXRET_OK;` |
|       - | 5187 | `			}` |
|       3 | 5188 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5189 | `			{` |
|       - | 5190 | `				SyBlob sResolved;` |
|       3 | 5191 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 5192 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 | 5193 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 5194 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 5195 | `				SyBlobRelease(&sResolved);` |
|       - | 5196 | `			}` |
|       - | 5197 | `			/* Only interfaces is allowed */` |
|       3 | 5198 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5199 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5200 | `			}` |
|       3 | 5201 | `			if( pBase == 0 ){` |
|       - | 5202 | `				/* Inexistant interface */` |
|     ! 0 | 5203 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 5204 | `				if( rc == SXERR_ABORT ){` |
|       - | 5205 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5206 | `					return SXERR_ABORT;` |
|       - | 5207 | `				}` |
|     ! 0 | 5208 | `			}` |
|       - | 5209 | `			/* Advance the stream cursor */` |
|       3 | 5210 | `			pGen->pIn++;` |
|       1 | 5211 | `		}` |
|       1 | 5212 | `	}` |
|    7814 | 5213 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5214 | `		/* Syntax error */` |
|     ! 0 | 5215 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 5216 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5217 | `		if( rc == SXERR_ABORT ){` |
|       - | 5218 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5219 | `			return SXERR_ABORT;` |
|       - | 5220 | `		}` |
|     ! 0 | 5221 | `		return SXRET_OK;` |
|       - | 5222 | `	}` |
|    7814 | 5223 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    7814 | 5224 | `	pEnd = 0; /* cc warning */` |
|       - | 5225 | `	/* Delimit the interface body */` |
|    7814 | 5226 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    7814 | 5227 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5228 | `		/* Syntax error */` |
|     ! 0 | 5229 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 5230 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5231 | `		if( rc == SXERR_ABORT ){` |
|       - | 5232 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5233 | `			return SXERR_ABORT;` |
|       - | 5234 | `		}` |
|     ! 0 | 5235 | `		return SXRET_OK;` |
|       - | 5236 | `	}` |
|       - | 5237 | `	/* Swap token stream */` |
|    7814 | 5238 | `	pTmp = pGen->pEnd;` |
|    7814 | 5239 | `	pGen->pEnd = pEnd;` |
|       - | 5240 | `	/* Start the parse process` |
|       - | 5241 | `	 * Note (According to the PHP reference manual):` |
|       - | 5242 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 5243 | `	 *  Only 'public' visibility is allowed.` |
|       - | 5244 | `	 */` |
|   14293 | 5245 | `	for(;;){` |
|       - | 5246 | `		/* Jump leading/trailing semi-colons */` |
|   49362 | 5247 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   20776 | 5248 | `			pGen->pIn++;` |
|       2 | 5249 | `		}` |
|   28588 | 5250 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5251 | `			/* End of interface body */` |
|    7814 | 5252 | `			break;` |
|       - | 5253 | `		}` |
|   20776 | 5254 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5255 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5256 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 5257 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5258 | `			if( rc == SXERR_ABORT ){` |
|       - | 5259 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5260 | `				return SXERR_ABORT;` |
|       - | 5261 | `			}` |
|     ! 0 | 5262 | `			goto done;` |
|       - | 5263 | `		}` |
|       - | 5264 | `		/* Extract the current keyword */` |
|   20776 | 5265 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   20776 | 5266 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 5267 | `			/* Emit a warning and switch to public visibility */` |
|     ! 0 | 5268 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"interface: Access type must be public");` |
|     ! 0 | 5269 | `			nKwrd = PH7_TKWRD_PUBLIC;` |
|     ! 0 | 5270 | `		}` |
|   20776 | 5271 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5272 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5273 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5274 | `			if( rc == SXERR_ABORT ){` |
|       - | 5275 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5276 | `				return SXERR_ABORT;` |
|       - | 5277 | `			}` |
|     ! 0 | 5278 | `			goto done;` |
|       - | 5279 | `		}` |
|   20776 | 5280 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 5281 | `			/* Advance the stream cursor */` |
|   20772 | 5282 | `			pGen->pIn++;` |
|   20772 | 5283 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5284 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5285 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5286 | `				if( rc == SXERR_ABORT ){` |
|       - | 5287 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5288 | `					return SXERR_ABORT;` |
|       - | 5289 | `				}` |
|     ! 0 | 5290 | `				goto done;` |
|       - | 5291 | `			}` |
|   20772 | 5292 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   20772 | 5293 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5294 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5295 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5296 | `				if( rc == SXERR_ABORT ){` |
|       - | 5297 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5298 | `					return SXERR_ABORT;` |
|       - | 5299 | `				}` |
|     ! 0 | 5300 | `				goto done;` |
|       - | 5301 | `			}` |
|   10385 | 5302 | `		}` |
|   20776 | 5303 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5304 | `			/* Parse constant */` |
|       3 | 5305 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 5306 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5307 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5308 | `					return SXERR_ABORT;` |
|       - | 5309 | `				}` |
|     ! 0 | 5310 | `				goto done;` |
|       - | 5311 | `			}` |
|       2 | 5312 | `		}else{` |
|   20774 | 5313 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   20774 | 5314 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5315 | `				/* Static method,record that */` |
|     ! 0 | 5316 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 5317 | `				/* Advance the stream cursor */` |
|     ! 0 | 5318 | `				pGen->pIn++;` |
|     ! 0 | 5319 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 5320 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5321 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5322 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5323 | `						if( rc == SXERR_ABORT ){` |
|       - | 5324 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5325 | `							return SXERR_ABORT;` |
|       - | 5326 | `						}` |
|     ! 0 | 5327 | `						goto done;` |
|       - | 5328 | `				}` |
|     ! 0 | 5329 | `			}` |
|       - | 5330 | `			/* Process method signature (no body for interface methods) */` |
|   20774 | 5331 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   20774 | 5332 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5333 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5334 | `					return SXERR_ABORT;` |
|       - | 5335 | `				}` |
|     ! 0 | 5336 | `				goto done;` |
|       - | 5337 | `			}` |
|       - | 5338 | `		}` |
|       2 | 5339 | `	}` |
|       - | 5340 | `	/* Install the interface */` |
|    7814 | 5341 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    7814 | 5342 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 5343 | `		/* Inherit from the base interface */` |
|       3 | 5344 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 5345 | `	}` |
|    7814 | 5346 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5347 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5348 | `		return SXERR_ABORT;` |
|       - | 5349 | `	}` |
|    3906 | 5350 | `done:` |
|       - | 5351 | `	/* Point beyond the interface body */` |
|    7814 | 5352 | `	pGen->pIn  = &pEnd[1];` |
|    7814 | 5353 | `	pGen->pEnd = pTmp;` |
|    7814 | 5354 | `	return PH7_OK;` |
|    3908 | 5355 |  |
|       - | 5356 | `/*` |
|       - | 5357 | ` * Compile a user-defined class.` |
|       - | 5358 | ` * According to the PHP language reference manual` |
|       - | 5359 | ` *  class` |
|       - | 5360 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 5361 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 5362 | ` *  of the properties and methods belonging to the class.` |
|       - | 5363 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 5364 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 5365 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 5366 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 5367 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 5368 | ` *  (called "methods").` |
|       - | 5369 | ` */` |
|       - | 5370 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 5371 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 5372 | `struct TraitUseEntry {` |
|       - | 5373 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 5374 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 5375 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 5376 | `};` |
|       - | 5377 | `/*` |
|       - | 5378 | ` * Validate that methods implementing interface contracts have compatible` |
|       - | 5379 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - | 5380 | ` */` |
|   36698 | 5381 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5382 |  |
|       - | 5383 | `	ph7_class **apIface;` |
|       - | 5384 | `	sxu32 nIface,i;` |
|       - | 5385 | `	sxi32 rc;` |
|   36700 | 5386 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 | 5387 | `		return SXRET_OK;` |
|       - | 5388 | `	}` |
|   36700 | 5389 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   36700 | 5390 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   39330 | 5391 | `	for(i = 0; i < nIface; i++){` |
|    2632 | 5392 | `		ph7_class *pIface = apIface[i];` |
|       - | 5393 | `		SyHashEntry *pEntry;` |
|    2632 | 5394 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   15670 | 5395 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   13040 | 5396 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - | 5397 | `			ph7_class_method *pImplMeth;` |
|   13040 | 5398 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - | 5399 | `			/* Find the implementing method in the class */` |
|   13040 | 5400 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   13040 | 5401 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 | 5402 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - | 5403 | `			}` |
|       - | 5404 | `			/* Check visibility: interface methods must be implemented as public */` |
|   13026 | 5405 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 | 5406 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5407 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 | 5408 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 | 5409 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5410 | `					return SXERR_ABORT;` |
|       - | 5411 | `				}` |
|       1 | 5412 | `			}` |
|       - | 5413 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - | 5414 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - | 5415 | `			 */` |
|       - | 5416 | `			{` |
|   13026 | 5417 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   13026 | 5418 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   13026 | 5419 | `				int sigError = 0;` |
|   13026 | 5420 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 | 5421 | `					sigError = 1;` |
|   13025 | 5422 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - | 5423 | `					/* Extra parameters must all have default values */` |
|       5 | 5424 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - | 5425 | `					sxu32 k;` |
|       7 | 5426 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 | 5427 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 | 5428 | `							sigError = 1;` |
|       3 | 5429 | `							break;` |
|       - | 5430 | `						}` |
|       2 | 5431 | `					}` |
|       2 | 5432 | `				}` |
|   13026 | 5433 | `				if( sigError ){` |
|       - | 5434 | `					SyBlob sImplSig, sIfaceSig;` |
|       - | 5435 | `					ph7_vm_func_arg *aArgs;` |
|       - | 5436 | `					sxu32 j;` |
|       5 | 5437 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 | 5438 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - | 5439 | `					/* Build implementing method signature */` |
|       5 | 5440 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 | 5441 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 | 5442 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 | 5443 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 | 5444 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5445 | `					}` |
|       - | 5446 | `					/* Build interface method signature */` |
|       5 | 5447 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 | 5448 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 | 5449 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 | 5450 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 | 5451 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5452 | `					}` |
|       7 | 5453 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5454 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 | 5455 | `						&pClass->sName,pMName,` |
|       4 | 5456 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 | 5457 | `						&pIface->sName,pMName,` |
|       4 | 5458 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 | 5459 | `					SyBlobRelease(&sImplSig);` |
|       5 | 5460 | `					SyBlobRelease(&sIfaceSig);` |
|       5 | 5461 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5462 | `						return SXERR_ABORT;` |
|       - | 5463 | `					}` |
|       2 | 5464 | `				}` |
|       - | 5465 | `			}` |
|       2 | 5466 | `		}` |
|    1317 | 5467 | `	}` |
|   36700 | 5468 | `	return SXRET_OK;` |
|   18351 | 5469 |  |
|       - | 5470 | `/*` |
|       - | 5471 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - | 5472 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - | 5473 | ` */` |
|   36698 | 5474 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5475 |  |
|       - | 5476 | `	ph7_class_method *pMeth;` |
|       - | 5477 | `	SyHashEntry *pEntry;` |
|       - | 5478 | `	sxu32 nAbstract;` |
|       - | 5479 | `	SyBlob sMsg;` |
|       - | 5480 | `	sxi32 rc;` |
|       - | 5481 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   36700 | 5482 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      20 | 5483 | `		return SXRET_OK;` |
|       - | 5484 | `	}` |
|       - | 5485 | `	/* Count abstract methods */` |
|   36682 | 5486 | `	nAbstract = 0;` |
|   36682 | 5487 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  348222 | 5488 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  311542 | 5489 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  311542 | 5490 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 | 5491 | `			nAbstract++;` |
|       8 | 5492 | `		}` |
|       2 | 5493 | `	}` |
|   36682 | 5494 | `	if( nAbstract == 0 ){` |
|   36668 | 5495 | `		return SXRET_OK;` |
|       - | 5496 | `	}` |
|       - | 5497 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 | 5498 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 | 5499 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - | 5500 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 | 5501 | `		&pClass->sName,nAbstract,` |
|       7 | 5502 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 | 5503 | `		(nAbstract > 1 ? "s" : ""));` |
|       - | 5504 | `	/* Second pass: list methods with origins */` |
|       - | 5505 | `	{` |
|      15 | 5506 | `		sxu32 nListed = 0;` |
|      15 | 5507 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 | 5508 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 | 5509 | `			ph7_class *pOrigin = 0;` |
|       - | 5510 | `			SyString *pMName;` |
|      19 | 5511 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 | 5512 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 | 5513 | `				continue;` |
|       - | 5514 | `			}` |
|      17 | 5515 | `			pMName = &pMeth->sFunc.sName;` |
|      17 | 5516 | `			if( nListed > 0 ){` |
|       3 | 5517 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 | 5518 | `			}` |
|       - | 5519 | `			/* Find the origin of this abstract method.` |
|       - | 5520 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - | 5521 | `			 * inheritance chains) take precedence for interface-declared` |
|       - | 5522 | `			 * methods. Abstract class methods only win when the class` |
|       - | 5523 | `			 * itself declared the abstract method (not inherited from` |
|       - | 5524 | `			 * an interface). Trait methods are adopted into the using` |
|       - | 5525 | `			 * class's namespace.` |
|       - | 5526 | `			 */` |
|       - | 5527 | `			{` |
|       - | 5528 | `				ph7_class **apIface;` |
|       - | 5529 | `				ph7_class **apTrait;` |
|       - | 5530 | `				ph7_class *pWalk;` |
|       - | 5531 | `				sxu32 i;` |
|       - | 5532 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - | 5533 | `				 * (one that was written in the class body, not inherited from an` |
|       - | 5534 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - | 5535 | `				 */` |
|      17 | 5536 | `				if( pClass->pBase ){` |
|       9 | 5537 | `					pWalk = pClass->pBase;` |
|      17 | 5538 | `					while( pWalk ){` |
|       - | 5539 | `						ph7_class_method *pParentMeth;` |
|      11 | 5540 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 | 5541 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - | 5542 | `							/* Exclude methods that came from an interface anywhere` |
|       - | 5543 | `							 * in this class's ancestor chain.` |
|       - | 5544 | `							 */` |
|      11 | 5545 | `							int fromIface = 0;` |
|      11 | 5546 | `							ph7_class *pAnc = pWalk;` |
|      15 | 5547 | `							while( pAnc ){` |
|       - | 5548 | `								ph7_class **apPI;` |
|       - | 5549 | `								sxu32 j;` |
|      13 | 5550 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 | 5551 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 | 5552 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 | 5553 | `										fromIface = 1;` |
|       9 | 5554 | `										break;` |
|       - | 5555 | `									}` |
|     ! 0 | 5556 | `								}` |
|      13 | 5557 | `								if( fromIface ) break;` |
|       5 | 5558 | `								pAnc = pAnc->pBase;` |
|       1 | 5559 | `							}` |
|      11 | 5560 | `							if( !fromIface ){` |
|       3 | 5561 | `								pOrigin = pWalk;` |
|       3 | 5562 | `								break;` |
|       - | 5563 | `							}` |
|       4 | 5564 | `						}` |
|       9 | 5565 | `						pWalk = pWalk->pBase;` |
|       1 | 5566 | `					}` |
|       4 | 5567 | `				}` |
|       - | 5568 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - | 5569 | `				 * each interface's own parent chain for the deepest origin.` |
|       - | 5570 | `				 */` |
|      17 | 5571 | `				if( !pOrigin ){` |
|      15 | 5572 | `					pWalk = pClass;` |
|      37 | 5573 | `					while( pWalk && !pOrigin ){` |
|      23 | 5574 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 | 5575 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 | 5576 | `							ph7_class *pIface = apIface[i];` |
|      13 | 5577 | `							ph7_class *pDeepest = 0;` |
|      25 | 5578 | `							while( pIface ){` |
|      13 | 5579 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 | 5580 | `									pDeepest = pIface;` |
|       6 | 5581 | `								}` |
|      13 | 5582 | `								pIface = pIface->pBase;` |
|       1 | 5583 | `							}` |
|      13 | 5584 | `							if( pDeepest ){` |
|      13 | 5585 | `								pOrigin = pDeepest;` |
|      13 | 5586 | `								break;` |
|       - | 5587 | `							}` |
|     ! 0 | 5588 | `						}` |
|      23 | 5589 | `						pWalk = pWalk->pBase;` |
|       1 | 5590 | `					}` |
|       7 | 5591 | `				}` |
|       - | 5592 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 | 5593 | `				if( !pOrigin ){` |
|       3 | 5594 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 | 5595 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 | 5596 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 | 5597 | `							pOrigin = pClass;` |
|       3 | 5598 | `							break;` |
|       - | 5599 | `						}` |
|     ! 0 | 5600 | `					}` |
|       1 | 5601 | `				}` |
|       - | 5602 | `			}` |
|      17 | 5603 | `			if( pOrigin ){` |
|      17 | 5604 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 | 5605 | `			}else{` |
|       - | 5606 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 | 5607 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - | 5608 | `			}` |
|      17 | 5609 | `			nListed++;` |
|       1 | 5610 | `		}` |
|       - | 5611 | `	}` |
|      15 | 5612 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 | 5613 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 | 5614 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 | 5615 | `	SyBlobRelease(&sMsg);` |
|      15 | 5616 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5617 | `		return SXERR_ABORT;` |
|       - | 5618 | `	}` |
|      15 | 5619 | `	return SXRET_OK;` |
|   18351 | 5620 |  |
|   36702 | 5621 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 5622 |  |
|   36704 | 5623 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5624 | `	ph7_class *pClass,*pBase;` |
|       - | 5625 | `	SyToken *pEnd,*pTmp;` |
|       - | 5626 | `	sxi32 iProtection;` |
|       - | 5627 | `	SySet aInterfaces;` |
|       - | 5628 | `	SySet aUseEntries;` |
|       - | 5629 | `	sxi32 iAttrflags;` |
|       - | 5630 | `	SyString *pName;` |
|       - | 5631 | `	sxi32 nKwrd;` |
|       - | 5632 | `	sxi32 rc;` |
|       - | 5633 | `	/* Jump the 'class' keyword */` |
|   36704 | 5634 | `	pGen->pIn++;` |
|   36704 | 5635 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5636 | `		/* Syntax error */` |
|     ! 0 | 5637 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 5638 | `		if( rc == SXERR_ABORT ){` |
|       - | 5639 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5640 | `			return SXERR_ABORT;` |
|       - | 5641 | `		}` |
|       - | 5642 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 5643 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 5644 | `			pGen->pIn++;` |
|     ! 0 | 5645 | `		}` |
|     ! 0 | 5646 | `		return SXRET_OK;` |
|       - | 5647 | `	}` |
|       - | 5648 | `	/* Extract class name */` |
|   36704 | 5649 | `	pName = &pGen->pIn->sData;` |
|       - | 5650 | `	/* Advance the stream cursor */` |
|   36704 | 5651 | `	pGen->pIn++;` |
|       - | 5652 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5653 | `		SyBlob sFQN;` |
|       - | 5654 | `		SyString sFQNStr;` |
|   36704 | 5655 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   36704 | 5656 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   36704 | 5657 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   36704 | 5658 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   36704 | 5659 | `		SyBlobRelease(&sFQN);` |
|       - | 5660 | `	}` |
|   36704 | 5661 | `	if( pClass == 0 ){` |
|     ! 0 | 5662 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5663 | `		return SXERR_ABORT;` |
|       - | 5664 | `	}` |
|       - | 5665 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   36704 | 5666 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   36704 | 5667 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 5668 | `	/* Assume a standalone class */` |
|   36704 | 5669 | `	pBase = 0;` |
|   36704 | 5670 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5671 | `		SyString *pBaseName;` |
|   26024 | 5672 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   26024 | 5673 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   23396 | 5674 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   23396 | 5675 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5676 | `				/* Syntax error */` |
|     ! 0 | 5677 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5678 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 5679 | `					pName);` |
|     ! 0 | 5680 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5681 | `				if( rc == SXERR_ABORT ){` |
|       - | 5682 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5683 | `					return SXERR_ABORT;` |
|       - | 5684 | `				}` |
|     ! 0 | 5685 | `				return SXRET_OK;` |
|       - | 5686 | `			}` |
|       - | 5687 | `			/* Extract base class name and resolve through namespace/imports */` |
|   23396 | 5688 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5689 | `			{` |
|       - | 5690 | `				SyBlob sResolved;` |
|   23396 | 5691 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   23396 | 5692 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   35093 | 5693 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   23394 | 5694 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   23396 | 5695 | `				SyBlobRelease(&sResolved);` |
|       - | 5696 | `			}` |
|       - | 5697 | `			/* Interfaces are not allowed */` |
|   23396 | 5698 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 5699 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5700 | `			}` |
|   23396 | 5701 | `			if( pBase == 0 ){` |
|       - | 5702 | `				/* Inexistant base class */` |
|     ! 0 | 5703 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 5704 | `				if( rc == SXERR_ABORT ){` |
|       - | 5705 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5706 | `					return SXERR_ABORT;` |
|       - | 5707 | `				}` |
|     ! 0 | 5708 | `			}else{` |
|   23396 | 5709 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 5710 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 5711 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 5712 | `					if( rc == SXERR_ABORT ){` |
|       - | 5713 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5714 | `						return SXERR_ABORT;` |
|       - | 5715 | `					}` |
|     ! 0 | 5716 | `				}` |
|       - | 5717 | `			}` |
|       - | 5718 | `			/* Advance the stream cursor */` |
|   23396 | 5719 | `			pGen->pIn++;` |
|   11697 | 5720 | `		}` |
|   26024 | 5721 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 5722 | `			ph7_class *pInterface;` |
|       - | 5723 | `			SyString *pIntName;` |
|       - | 5724 | `			/* Interface implementation */` |
|    2632 | 5725 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    1315 | 5726 | `			for(;;){` |
|    2632 | 5727 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5728 | `					/* Syntax error */` |
|     ! 0 | 5729 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5730 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 5731 | `						pName);` |
|     ! 0 | 5732 | `					if( rc == SXERR_ABORT ){` |
|       - | 5733 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5734 | `						return SXERR_ABORT;` |
|       - | 5735 | `					}` |
|     ! 0 | 5736 | `					break;` |
|       - | 5737 | `				}` |
|       - | 5738 | `				/* Extract interface name and resolve through namespace/imports */` |
|    2632 | 5739 | `				pIntName = &pGen->pIn->sData;` |
|       - | 5740 | `				{` |
|       - | 5741 | `					SyBlob sResolved;` |
|    2632 | 5742 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    2632 | 5743 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|    5262 | 5744 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    2630 | 5745 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    2632 | 5746 | `					SyBlobRelease(&sResolved);` |
|       - | 5747 | `				}` |
|       - | 5748 | `				/* Only interfaces are allowed */` |
|    2632 | 5749 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5750 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 5751 | `				}` |
|    2632 | 5752 | `				if( pInterface == 0 ){` |
|       - | 5753 | `					/* Inexistant interface */` |
|     ! 0 | 5754 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 5755 | `					if( rc == SXERR_ABORT ){` |
|       - | 5756 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5757 | `						return SXERR_ABORT;` |
|       - | 5758 | `					}` |
|     ! 0 | 5759 | `				}else{` |
|       - | 5760 | `					/* Register interface */` |
|    2632 | 5761 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 5762 | `				}` |
|       - | 5763 | `				/* Advance the stream cursor */` |
|    2632 | 5764 | `				pGen->pIn++;` |
|    2632 | 5765 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    1317 | 5766 | `					break;` |
|       - | 5767 | `				}` |
|     ! 0 | 5768 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 5769 | `			}` |
|    1315 | 5770 | `		}` |
|   13011 | 5771 | `	}` |
|   36704 | 5772 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5773 | `		/* Syntax error */` |
|     ! 0 | 5774 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 5775 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5776 | `		if( rc == SXERR_ABORT ){` |
|       - | 5777 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5778 | `			return SXERR_ABORT;` |
|       - | 5779 | `		}` |
|     ! 0 | 5780 | `		return SXRET_OK;` |
|       - | 5781 | `	}` |
|   36704 | 5782 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   36704 | 5783 | `	pEnd = 0; /* cc warning */` |
|       - | 5784 | `	/* Delimit the class body */` |
|   36704 | 5785 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   36704 | 5786 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5787 | `		/* Syntax error */` |
|     ! 0 | 5788 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 5789 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5790 | `		if( rc == SXERR_ABORT ){` |
|       - | 5791 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5792 | `			return SXERR_ABORT;` |
|       - | 5793 | `		}` |
|     ! 0 | 5794 | `		return SXRET_OK;` |
|       - | 5795 | `	}` |
|       - | 5796 | `	/* Swap token stream */` |
|   36704 | 5797 | `	pTmp = pGen->pEnd;` |
|   36704 | 5798 | `	pGen->pEnd = pEnd;` |
|       - | 5799 | `	/* Set the inherited flags */` |
|   36704 | 5800 | `	pClass->iFlags = iFlags;` |
|       - | 5801 | `	/* Start the parse process */` |
|   70380 | 5802 | `	for(;;){` |
|       - | 5803 | `		/* Jump leading/trailing semi-colons */` |
|  208632 | 5804 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   33944 | 5805 | `			pGen->pIn++;` |
|       2 | 5806 | `		}` |
|  174690 | 5807 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5808 | `			/* End of class body */` |
|   36700 | 5809 | `			break;` |
|       - | 5810 | `		}` |
|  137992 | 5811 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5812 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5813 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5814 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5815 | `			if( rc == SXERR_ABORT ){` |
|       - | 5816 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5817 | `				return SXERR_ABORT;` |
|       - | 5818 | `			}` |
|     ! 0 | 5819 | `			goto done;` |
|       - | 5820 | `		}` |
|       - | 5821 | `		/* Assume public visibility */` |
|  137992 | 5822 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  137992 | 5823 | `		iAttrflags = 0;` |
|  137992 | 5824 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 5825 | `			/* Extract the current keyword */` |
|  137992 | 5826 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  137992 | 5827 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5828 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 5829 | `				TraitUseEntry sUse;` |
|      41 | 5830 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      41 | 5831 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      41 | 5832 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      28 | 5833 | `				for(;;){` |
|       - | 5834 | `					ph7_class *pTrait;` |
|       - | 5835 | `					SyString *pTraitName;` |
|      49 | 5836 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5837 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5838 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 5839 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5840 | `							return SXERR_ABORT;` |
|       - | 5841 | `						}` |
|     ! 0 | 5842 | `						break;` |
|       - | 5843 | `					}` |
|      49 | 5844 | `					pTraitName = &pGen->pIn->sData;` |
|       - | 5845 | `					/* Resolve trait name through namespace/imports */ {` |
|       - | 5846 | `						SyBlob sResolved;` |
|      49 | 5847 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      49 | 5848 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      97 | 5849 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      48 | 5850 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      49 | 5851 | `						SyBlobRelease(&sResolved);` |
|       - | 5852 | `					}` |
|       - | 5853 | `					/* Only traits are allowed */` |
|      49 | 5854 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5855 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 5856 | `					}` |
|      49 | 5857 | `					if( pTrait == 0 ){` |
|     ! 0 | 5858 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5859 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 | 5860 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5861 | `							return SXERR_ABORT;` |
|       - | 5862 | `						}` |
|     ! 0 | 5863 | `					}else{` |
|      49 | 5864 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 5865 | `					}` |
|      49 | 5866 | `					pGen->pIn++; /* Advance past trait name */` |
|      49 | 5867 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      21 | 5868 | `						break;` |
|       - | 5869 | `					}` |
|       9 | 5870 | `					pGen->pIn++; /* Jump the comma */` |
|       1 | 5871 | `				}` |
|       - | 5872 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      41 | 5873 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 5874 | `					SyToken *pBlock;` |
|       9 | 5875 | `					pGen->pIn++; /* Jump '{' */` |
|       9 | 5876 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 | 5877 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 | 5878 | `					sUse.pResolvEnd = pBlock;` |
|       9 | 5879 | `					if( pBlock < pGen->pEnd ){` |
|       9 | 5880 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 | 5881 | `					}else{` |
|     ! 0 | 5882 | `						pGen->pIn = pGen->pEnd;` |
|       - | 5883 | `					}` |
|       4 | 5884 | `				}` |
|      41 | 5885 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 5886 | `				/* The semicolon will be consumed by the outer loop */` |
|      41 | 5887 | `				continue;` |
|       - | 5888 | `			}` |
|  137952 | 5889 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  135254 | 5890 | `				iProtection = nKwrd;` |
|  135254 | 5891 | `				pGen->pIn++; /* Jump the visibility token */` |
|  135254 | 5892 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5893 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5894 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5895 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5896 | `					if( rc == SXERR_ABORT ){` |
|       - | 5897 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5898 | `						return SXERR_ABORT;` |
|       - | 5899 | `					}` |
|     ! 0 | 5900 | `					goto done;` |
|       - | 5901 | `				}` |
|  135254 | 5902 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5903 | `					/* Attribute declaration */` |
|   33888 | 5904 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   33888 | 5905 | `					if( rc != SXRET_OK ){` |
|       3 | 5906 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5907 | `							return SXERR_ABORT;` |
|       - | 5908 | `						}` |
|       3 | 5909 | `						goto done;` |
|       - | 5910 | `					}` |
|   33886 | 5911 | `					continue;` |
|       - | 5912 | `				}` |
|       - | 5913 | `				/* Extract the keyword */` |
|  101368 | 5914 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   50683 | 5915 | `			}` |
|  104066 | 5916 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5917 | `				/* Process constant declaration */` |
|      10 | 5918 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 | 5919 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5920 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5921 | `						return SXERR_ABORT;` |
|       - | 5922 | `					}` |
|     ! 0 | 5923 | `					goto done;` |
|       - | 5924 | `				}` |
|       6 | 5925 | `			}else{` |
|  104058 | 5926 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5927 | `					/* Static method or attribute,record that */` |
|    2616 | 5928 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2616 | 5929 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2616 | 5930 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5931 | `						/* Extract the keyword */` |
|    2612 | 5932 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2612 | 5933 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 5934 | `							iProtection = nKwrd;` |
|     ! 0 | 5935 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 5936 | `						}` |
|    1305 | 5937 | `					}` |
|    2616 | 5938 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5939 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5940 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 5941 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5942 | `						if( rc == SXERR_ABORT ){` |
|       - | 5943 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5944 | `							return SXERR_ABORT;` |
|       - | 5945 | `						}` |
|     ! 0 | 5946 | `						goto done;` |
|       - | 5947 | `					}` |
|    2616 | 5948 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5949 | `						/* Attribute declaration */` |
|       5 | 5950 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 5951 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 5952 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5953 | `								return SXERR_ABORT;` |
|       - | 5954 | `							}` |
|     ! 0 | 5955 | `							goto done;` |
|       - | 5956 | `						}` |
|       5 | 5957 | `						continue;` |
|       - | 5958 | `					}` |
|       - | 5959 | `					/* Extract the keyword */` |
|    2612 | 5960 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  102749 | 5961 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 5962 | `					/* Abstract method,record that */` |
|      10 | 5963 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 5964 | `					/* Mark the whole class as abstract */` |
|      10 | 5965 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 5966 | `					/* Advance the stream cursor */` |
|      10 | 5967 | `					pGen->pIn++;` |
|      10 | 5968 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      10 | 5969 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      10 | 5970 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 | 5971 | `							iProtection = nKwrd;` |
|       8 | 5972 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 | 5973 | `						}` |
|       4 | 5974 | `					}` |
|      10 | 5975 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 5976 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5977 | `							/* Static method */` |
|     ! 0 | 5978 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5979 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5980 | `					}` |
|      10 | 5981 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       8 | 5982 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5983 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5984 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 5985 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5986 | `							if( rc == SXERR_ABORT ){` |
|       - | 5987 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5988 | `								return SXERR_ABORT;` |
|       - | 5989 | `							}` |
|     ! 0 | 5990 | `							goto done;` |
|       - | 5991 | `					}` |
|      10 | 5992 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  101440 | 5993 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 5994 | `					/* final method ,record that */` |
|       5 | 5995 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 5996 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 5997 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5998 | `						/* Extract the keyword */` |
|       5 | 5999 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6000 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6001 | `							iProtection = nKwrd;` |
|       5 | 6002 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 6003 | `						}` |
|       2 | 6004 | `					}` |
|       5 | 6005 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 6006 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 6007 | `							/* Static method */` |
|     ! 0 | 6008 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 6009 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 6010 | `					}` |
|       5 | 6011 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6012 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6013 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6014 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 6015 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 6016 | `							if( rc == SXERR_ABORT ){` |
|       - | 6017 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 6018 | `								return SXERR_ABORT;` |
|       - | 6019 | `							}` |
|     ! 0 | 6020 | `							goto done;` |
|       - | 6021 | `					}` |
|       5 | 6022 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6023 | `				}` |
|  104054 | 6024 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 6025 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6026 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 6027 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6028 | `						if( rc == SXERR_ABORT ){` |
|       - | 6029 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6030 | `							return SXERR_ABORT;` |
|       - | 6031 | `						}` |
|     ! 0 | 6032 | `						goto done;` |
|       - | 6033 | `				}` |
|  104054 | 6034 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 6035 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 6036 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 6037 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6038 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 6039 | `						if( rc == SXERR_ABORT ){` |
|       - | 6040 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6041 | `							return SXERR_ABORT;` |
|       - | 6042 | `						}` |
|     ! 0 | 6043 | `						goto done;` |
|       - | 6044 | `					}` |
|       - | 6045 | `					/* Attribute declaration */` |
|       7 | 6046 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 6047 | `				}else{` |
|       - | 6048 | `					/* Process method declaration */` |
|  104048 | 6049 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 6050 | `				}` |
|  104054 | 6051 | `				if( rc != SXRET_OK ){` |
|       3 | 6052 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6053 | `						return SXERR_ABORT;` |
|       - | 6054 | `					}` |
|       3 | 6055 | `					goto done;` |
|       - | 6056 | `				}` |
|       - | 6057 | `			}` |
|   52031 | 6058 | `		}else{` |
|       - | 6059 | `			/* Attribute declaration */` |
|     ! 0 | 6060 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6061 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6062 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6063 | `					return SXERR_ABORT;` |
|       - | 6064 | `				}` |
|     ! 0 | 6065 | `				goto done;` |
|       - | 6066 | `			}` |
|       - | 6067 | `		}` |
|       2 | 6068 | `	}` |
|       - | 6069 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 6070 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 6071 | `	 */` |
|       - | 6072 | `	{` |
|       - | 6073 | `		TraitUseEntry *apUse;` |
|       - | 6074 | `		sxu32 nU;` |
|   36700 | 6075 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   36740 | 6076 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      41 | 6077 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      41 | 6078 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      41 | 6079 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      41 | 6080 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 6081 | `			sxu32 nT;` |
|      41 | 6082 | `			if( !hasResolution ){` |
|       - | 6083 | `				/* No conflict resolution block: use standard trait application */` |
|      71 | 6084 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      39 | 6085 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      39 | 6086 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6087 | `						break;` |
|       - | 6088 | `					}` |
|      20 | 6089 | `				}` |
|      17 | 6090 | `			}else{` |
|       - | 6091 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 6092 | `				 * then use the block to resolve method conflicts.` |
|       - | 6093 | `				 */` |
|       - | 6094 | `				SyToken *pR;` |
|      19 | 6095 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 | 6096 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 6097 | `					ph7_class_attr *pAR;` |
|       - | 6098 | `					SyHashEntry *pER;` |
|       - | 6099 | `					SyString *pNR;` |
|      11 | 6100 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 | 6101 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 6102 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 6103 | `						pNR = &pAR->sName;` |
|     ! 0 | 6104 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 6105 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 6106 | `						}` |
|     ! 0 | 6107 | `					}` |
|      11 | 6108 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 | 6109 | `				}` |
|       - | 6110 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 | 6111 | `				pR = pUse->pResolvStart;` |
|      21 | 6112 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6113 | `					SyString sTrait,sMethod;` |
|       - | 6114 | `					ph7_class *pSrcTrait;` |
|       - | 6115 | `					ph7_class_method *pMeth;` |
|       - | 6116 | `					sxi32 nRKwrd;` |
|      33 | 6117 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6118 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6119 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6120 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6121 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6122 | `					sMethod = pR->sData;` |
|      13 | 6123 | `					pR++;` |
|      13 | 6124 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6125 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6126 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6127 | `							sTrait = sMethod;` |
|       7 | 6128 | `							pR++;` |
|       7 | 6129 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6130 | `							sMethod = pR->sData;` |
|       7 | 6131 | `							pR++;` |
|       3 | 6132 | `						}` |
|       3 | 6133 | `					}` |
|      13 | 6134 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6135 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6136 | `						continue;` |
|       - | 6137 | `					}` |
|      13 | 6138 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6139 | `					pR++;` |
|      13 | 6140 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 | 6141 | `						pSrcTrait = 0;` |
|       7 | 6142 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 | 6143 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 | 6144 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 | 6145 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 | 6146 | `								pSrcTrait = apTrait[nT];` |
|       5 | 6147 | `								break;` |
|       - | 6148 | `							}` |
|       2 | 6149 | `						}` |
|       5 | 6150 | `						if( pSrcTrait ){` |
|       5 | 6151 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 | 6152 | `							if( pMeth ){` |
|       5 | 6153 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 | 6154 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 | 6155 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 | 6156 | `								}` |
|       2 | 6157 | `							}` |
|       2 | 6158 | `						}` |
|       2 | 6159 | `					}` |
|      29 | 6160 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6161 | `				}` |
|       - | 6162 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 | 6163 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 6164 | `					ph7_class_method *pMR;` |
|       - | 6165 | `					SyHashEntry *pER;` |
|       - | 6166 | `					SyString *pNR;` |
|      11 | 6167 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 | 6168 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 | 6169 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 | 6170 | `						pNR = &pMR->sFunc.sName;` |
|      19 | 6171 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 | 6172 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 | 6173 | `						}` |
|       1 | 6174 | `					}` |
|       6 | 6175 | `				}` |
|       - | 6176 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 | 6177 | `				pR = pUse->pResolvStart;` |
|      21 | 6178 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6179 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 6180 | `					ph7_class *pSrcTrait;` |
|       - | 6181 | `					ph7_class_method *pMeth;` |
|      21 | 6182 | `					int hasQual = 0;` |
|       - | 6183 | `					sxi32 nRKwrd;` |
|      33 | 6184 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6185 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6186 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6187 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6188 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 | 6189 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6190 | `					sMethod = pR->sData;` |
|      13 | 6191 | `					pR++;` |
|      13 | 6192 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6193 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6194 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6195 | `							sTrait = sMethod;` |
|       7 | 6196 | `							hasQual = 1;` |
|       7 | 6197 | `							pR++;` |
|       7 | 6198 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6199 | `							sMethod = pR->sData;` |
|       7 | 6200 | `							pR++;` |
|       3 | 6201 | `						}` |
|       3 | 6202 | `					}` |
|      13 | 6203 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6204 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6205 | `						continue;` |
|       - | 6206 | `					}` |
|      13 | 6207 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6208 | `					pR++;` |
|      13 | 6209 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 | 6210 | `						sxi32 iNewVis = -1;` |
|       9 | 6211 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 | 6212 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 | 6213 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 | 6214 | `								iNewVis = nAK;` |
|       7 | 6215 | `								pR++;` |
|       3 | 6216 | `							}` |
|       3 | 6217 | `						}` |
|       9 | 6218 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 | 6219 | `							sAlias = pR->sData;` |
|       7 | 6220 | `							pR++;` |
|       3 | 6221 | `						}` |
|       9 | 6222 | `						pMeth = 0;` |
|       9 | 6223 | `						if( hasQual ){` |
|       3 | 6224 | `							pSrcTrait = 0;` |
|       5 | 6225 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 | 6226 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 | 6227 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 | 6228 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 | 6229 | `									pSrcTrait = apTrait[nT];` |
|       3 | 6230 | `									break;` |
|       - | 6231 | `								}` |
|       2 | 6232 | `							}` |
|       3 | 6233 | `							if( pSrcTrait ){` |
|       3 | 6234 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 | 6235 | `							}` |
|       2 | 6236 | `						}else{` |
|       7 | 6237 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 6238 | `						}` |
|       9 | 6239 | `						if( pMeth ){` |
|       9 | 6240 | `							if( sAlias.nByte > 0 ){` |
|       - | 6241 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 6242 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 6243 | `								 */` |
|       - | 6244 | `								ph7_class_method *pAlias;` |
|       - | 6245 | `								char *zAliasDup;` |
|       7 | 6246 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 | 6247 | `								if( pAlias ){` |
|       7 | 6248 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 | 6249 | `									if( iNewVis >= 0 ){` |
|       5 | 6250 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6251 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6252 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 | 6253 | `									}` |
|       7 | 6254 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 | 6255 | `									if( zAliasDup ){` |
|       7 | 6256 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 | 6257 | `									}` |
|       4 | 6258 | `								}` |
|       6 | 6259 | `							}else if( iNewVis >= 0 ){` |
|       - | 6260 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 6261 | `								ph7_class_method *pCopy;` |
|       3 | 6262 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 6263 | `								if( pCopy ){` |
|       3 | 6264 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 6265 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 6266 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6267 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6268 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 6269 | `									/* Replace the method in the class hash */` |
|       3 | 6270 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 6271 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 6272 | `								}` |
|       1 | 6273 | `							}` |
|       4 | 6274 | `						}` |
|       4 | 6275 | `						SXUNUSED(hasQual);` |
|       4 | 6276 | `					}` |
|      17 | 6277 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6278 | `				}` |
|       - | 6279 | `			}` |
|      41 | 6280 | `			SySetRelease(&pUse->aTraits);` |
|      21 | 6281 | `		}` |
|       - | 6282 | `	}` |
|       - | 6283 | `	/* Install the class */` |
|   36700 | 6284 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   36700 | 6285 | `	if( rc == SXRET_OK ){` |
|       - | 6286 | `		ph7_class **apInterface;` |
|       - | 6287 | `		sxu32 n;` |
|   36700 | 6288 | `		if( pBase ){` |
|       - | 6289 | `			/* Inherit from base class and mark as a subclass */` |
|   23396 | 6290 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   11697 | 6291 | `		}` |
|   36700 | 6292 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   39330 | 6293 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 6294 | `			/* Implements one or more interface */` |
|    2632 | 6295 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    2632 | 6296 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6297 | `				break;` |
|       - | 6298 | `			}` |
|    1317 | 6299 | `		}` |
|       - | 6300 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   36700 | 6301 | `		if( rc == SXRET_OK ){` |
|   36700 | 6302 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   36700 | 6303 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6304 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6305 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6306 | `				return SXERR_ABORT;` |
|       - | 6307 | `			}` |
|   18349 | 6308 | `		}` |
|       - | 6309 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   36700 | 6310 | `		if( rc == SXRET_OK ){` |
|   36700 | 6311 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   36700 | 6312 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6313 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6314 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6315 | `				return SXERR_ABORT;` |
|       - | 6316 | `			}` |
|   18349 | 6317 | `		}` |
|   18349 | 6318 | `	}` |
|   36700 | 6319 | `	SySetRelease(&aUseEntries);` |
|   36700 | 6320 | `	SySetRelease(&aInterfaces);` |
|   36700 | 6321 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6322 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6323 | `		return SXERR_ABORT;` |
|       - | 6324 | `	}` |
|   18349 | 6325 | `done:` |
|       - | 6326 | `	/* Point beyond the class body */` |
|   36704 | 6327 | `	pGen->pIn = &pEnd[1];` |
|   36704 | 6328 | `	pGen->pEnd = pTmp;` |
|   36704 | 6329 | `	return PH7_OK;` |
|   18353 | 6330 |  |
|       - | 6331 | `/*` |
|       - | 6332 | ` * Compile a user-defined abstract class.` |
|       - | 6333 | ` *  According to the PHP language reference manual` |
|       - | 6334 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 6335 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 6336 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 6337 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 6338 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 6339 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 6340 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 6341 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 6342 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 6343 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 6344 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 6345 | ` *   could differ.` |
|       - | 6346 | ` */` |
|      16 | 6347 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 6348 |  |
|       - | 6349 | `	sxi32 rc;` |
|      18 | 6350 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      18 | 6351 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      18 | 6352 | `	return rc;` |
|       2 | 6353 |  |
|       - | 6354 | `/*` |
|       - | 6355 | ` * Compile a user-defined final class.` |
|       - | 6356 | ` *  According to the PHP language reference manual` |
|       - | 6357 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 6358 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 6359 | ` *    final then it cannot be extended.` |
|       - | 6360 | ` */` |
|       2 | 6361 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 6362 |  |
|       - | 6363 | `	sxi32 rc;` |
|       3 | 6364 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 6365 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 6366 | `	return rc;` |
|       1 | 6367 |  |
|       - | 6368 | `/*` |
|       - | 6369 | ` * Compile a user-defined trait.` |
|       - | 6370 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 6371 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 6372 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 6373 | ` */` |
|      52 | 6374 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 | 6375 |  |
|      54 | 6376 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6377 | `	ph7_class *pClass;` |
|       - | 6378 | `	SyToken *pEnd,*pTmp;` |
|       - | 6379 | `	sxi32 iProtection;` |
|       - | 6380 | `	sxi32 iAttrflags;` |
|       - | 6381 | `	SyString *pName;` |
|       - | 6382 | `	sxi32 nKwrd;` |
|       - | 6383 | `	sxi32 rc;` |
|       - | 6384 | `	/* Jump the 'trait' keyword */` |
|      54 | 6385 | `	pGen->pIn++;` |
|      54 | 6386 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6387 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 6388 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6389 | `			return SXERR_ABORT;` |
|       - | 6390 | `		}` |
|     ! 0 | 6391 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 6392 | `			pGen->pIn++;` |
|     ! 0 | 6393 | `		}` |
|     ! 0 | 6394 | `		return SXRET_OK;` |
|       - | 6395 | `	}` |
|       - | 6396 | `	/* Extract trait name */` |
|      54 | 6397 | `	pName = &pGen->pIn->sData;` |
|      54 | 6398 | `	pGen->pIn++;` |
|       - | 6399 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 6400 | `		SyBlob sFQN;` |
|       - | 6401 | `		SyString sFQNStr;` |
|      54 | 6402 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      54 | 6403 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      54 | 6404 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      54 | 6405 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      54 | 6406 | `		SyBlobRelease(&sFQN);` |
|       - | 6407 | `	}` |
|      54 | 6408 | `	if( pClass == 0 ){` |
|     ! 0 | 6409 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6410 | `		return SXERR_ABORT;` |
|       - | 6411 | `	}` |
|       - | 6412 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      54 | 6413 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 6414 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 6415 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6416 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6417 | `			return SXERR_ABORT;` |
|       - | 6418 | `		}` |
|     ! 0 | 6419 | `		return SXRET_OK;` |
|       - | 6420 | `	}` |
|      54 | 6421 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      54 | 6422 | `	pEnd = 0;` |
|      54 | 6423 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      54 | 6424 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 6425 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 6426 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6427 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6428 | `			return SXERR_ABORT;` |
|       - | 6429 | `		}` |
|     ! 0 | 6430 | `		return SXRET_OK;` |
|       - | 6431 | `	}` |
|       - | 6432 | `	/* Swap token stream */` |
|      54 | 6433 | `	pTmp = pGen->pEnd;` |
|      54 | 6434 | `	pGen->pEnd = pEnd;` |
|       - | 6435 | `	/* Mark as trait */` |
|      54 | 6436 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 6437 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      53 | 6438 | `	for(;;){` |
|     144 | 6439 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      21 | 6440 | `			pGen->pIn++;` |
|       1 | 6441 | `		}` |
|     124 | 6442 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      54 | 6443 | `			break;` |
|       - | 6444 | `		}` |
|      71 | 6445 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6446 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6447 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6448 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 6449 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6450 | `				return SXERR_ABORT;` |
|       - | 6451 | `			}` |
|     ! 0 | 6452 | `			goto done;` |
|       - | 6453 | `		}` |
|      71 | 6454 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      71 | 6455 | `		iAttrflags = 0;` |
|      71 | 6456 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      71 | 6457 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      71 | 6458 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 6459 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 6460 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 6461 | `				for(;;){` |
|       - | 6462 | `					ph7_class *pUsedTrait;` |
|       - | 6463 | `					SyString *pUsedName;` |
|       5 | 6464 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6465 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6466 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 6467 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6468 | `							return SXERR_ABORT;` |
|       - | 6469 | `						}` |
|     ! 0 | 6470 | `						break;` |
|       - | 6471 | `					}` |
|       5 | 6472 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 6473 | `					{` |
|       - | 6474 | `						SyBlob sResolved;` |
|       5 | 6475 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 6476 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 6477 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 6478 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 6479 | `						SyBlobRelease(&sResolved);` |
|       - | 6480 | `					}` |
|       5 | 6481 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 6482 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 6483 | `					}` |
|       5 | 6484 | `					if( pUsedTrait == 0 ){` |
|       4 | 6485 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 6486 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 6487 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6488 | `							return SXERR_ABORT;` |
|       - | 6489 | `						}` |
|       2 | 6490 | `					}else{` |
|       3 | 6491 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 6492 | `					}` |
|       5 | 6493 | `					pGen->pIn++;` |
|       5 | 6494 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 6495 | `						break;` |
|       - | 6496 | `					}` |
|     ! 0 | 6497 | `					pGen->pIn++;` |
|     ! 0 | 6498 | `				}` |
|       5 | 6499 | `				continue;` |
|       - | 6500 | `			}` |
|      67 | 6501 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      63 | 6502 | `				iProtection = nKwrd;` |
|      63 | 6503 | `				pGen->pIn++;` |
|      63 | 6504 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6505 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6506 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6507 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6508 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6509 | `						return SXERR_ABORT;` |
|       - | 6510 | `					}` |
|     ! 0 | 6511 | `					goto done;` |
|       - | 6512 | `				}` |
|      63 | 6513 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 | 6514 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 | 6515 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6516 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6517 | `							return SXERR_ABORT;` |
|       - | 6518 | `						}` |
|     ! 0 | 6519 | `						goto done;` |
|       - | 6520 | `					}` |
|      11 | 6521 | `					continue;` |
|       - | 6522 | `				}` |
|      53 | 6523 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 | 6524 | `			}` |
|      57 | 6525 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 6526 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6527 | `					"Traits cannot have constants");` |
|     ! 0 | 6528 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6529 | `					return SXERR_ABORT;` |
|       - | 6530 | `				}` |
|     ! 0 | 6531 | `				goto done;` |
|     ! 0 | 6532 | `			}else{` |
|      57 | 6533 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 6534 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 6535 | `					pGen->pIn++;` |
|       5 | 6536 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 6537 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 6538 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6539 | `							iProtection = nKwrd;` |
|     ! 0 | 6540 | `							pGen->pIn++;` |
|     ! 0 | 6541 | `						}` |
|       1 | 6542 | `					}` |
|       5 | 6543 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6544 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6545 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 6546 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6547 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6548 | `							return SXERR_ABORT;` |
|       - | 6549 | `						}` |
|     ! 0 | 6550 | `						goto done;` |
|       - | 6551 | `					}` |
|       5 | 6552 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 6553 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 6554 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6555 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6556 | `								return SXERR_ABORT;` |
|       - | 6557 | `							}` |
|     ! 0 | 6558 | `							goto done;` |
|       - | 6559 | `						}` |
|       3 | 6560 | `						continue;` |
|       - | 6561 | `					}` |
|       3 | 6562 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 | 6563 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 | 6564 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 | 6565 | `					pGen->pIn++;` |
|       5 | 6566 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 6567 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6568 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6569 | `							iProtection = nKwrd;` |
|       5 | 6570 | `							pGen->pIn++;` |
|       2 | 6571 | `						}` |
|       2 | 6572 | `					}` |
|       5 | 6573 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6574 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6575 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6576 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 6577 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6578 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6579 | `							return SXERR_ABORT;` |
|       - | 6580 | `						}` |
|     ! 0 | 6581 | `						goto done;` |
|       - | 6582 | `					}` |
|       5 | 6583 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6584 | `				}` |
|      55 | 6585 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 6586 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6587 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 6588 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6589 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6590 | `						return SXERR_ABORT;` |
|       - | 6591 | `					}` |
|     ! 0 | 6592 | `					goto done;` |
|       - | 6593 | `				}` |
|      55 | 6594 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 6595 | `					pGen->pIn++;` |
|     ! 0 | 6596 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 6597 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6598 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 6599 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6600 | `							return SXERR_ABORT;` |
|       - | 6601 | `						}` |
|     ! 0 | 6602 | `						goto done;` |
|       - | 6603 | `					}` |
|     ! 0 | 6604 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6605 | `				}else{` |
|      55 | 6606 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 6607 | `				}` |
|      55 | 6608 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6609 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6610 | `						return SXERR_ABORT;` |
|       - | 6611 | `					}` |
|     ! 0 | 6612 | `					goto done;` |
|       - | 6613 | `				}` |
|       - | 6614 | `			}` |
|      28 | 6615 | `		}else{` |
|     ! 0 | 6616 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6617 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6618 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6619 | `					return SXERR_ABORT;` |
|       - | 6620 | `				}` |
|     ! 0 | 6621 | `				goto done;` |
|       - | 6622 | `			}` |
|       - | 6623 | `		}` |
|       1 | 6624 | `	}` |
|       - | 6625 | `	/* Install the trait */` |
|      54 | 6626 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      54 | 6627 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6628 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6629 | `		return SXERR_ABORT;` |
|       - | 6630 | `	}` |
|      26 | 6631 | `done:` |
|       - | 6632 | `	/* Point beyond the trait body */` |
|      54 | 6633 | `	pGen->pIn = &pEnd[1];` |
|      54 | 6634 | `	pGen->pEnd = pTmp;` |
|      54 | 6635 | `	return PH7_OK;` |
|      28 | 6636 |  |
|       - | 6637 | `/*` |
|       - | 6638 | ` * Compile a user-defined class.` |
|       - | 6639 | ` *  According to the PHP language reference manual` |
|       - | 6640 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 6641 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 6642 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 6643 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 6644 | ` *   and functions (called "methods").` |
|       - | 6645 | ` */` |
|   36684 | 6646 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 6647 |  |
|       - | 6648 | `	sxi32 rc;` |
|   36686 | 6649 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   36686 | 6650 | `	return rc;` |
|       2 | 6651 |  |
|       - | 6652 | `/*` |
|       - | 6653 | ` * Exception handling.` |
|       - | 6654 | ` *  According to the PHP language reference manual` |
|       - | 6655 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 6656 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 6657 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 6658 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 6659 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 6660 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 6661 | ` *    (or re-thrown) within a catch block.` |
|       - | 6662 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 6663 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 6664 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 6665 | ` *    been defined with set_exception_handler().` |
|       - | 6666 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 6667 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 6668 | ` */` |
|       - | 6669 | `/*` |
|       - | 6670 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 6671 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 6672 | ` * indicates failure.` |
|       - | 6673 | ` */` |
|    7808 | 6674 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 6675 |  |
|    7810 | 6676 | `	sxi32 rc = SXRET_OK;` |
|    7810 | 6677 | `	if( pRoot->pOp ){` |
|    7806 | 6678 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    3905 | 6679 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 6680 | `			/* Unexpected expression */` |
|     ! 0 | 6681 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6682 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 6683 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 6684 | `				rc = SXERR_INVALID;` |
|     ! 0 | 6685 | `			}` |
|       2 | 6686 | `		}` |
|    3906 | 6687 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 6688 | `		/* Unexpected expression */` |
|     ! 0 | 6689 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6690 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 6691 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 6692 | `			rc = SXERR_INVALID;` |
|     ! 0 | 6693 | `		}` |
|     ! 0 | 6694 | `	}` |
|    7810 | 6695 | `	return rc;` |
|       2 | 6696 |  |
|       - | 6697 | `/*` |
|       - | 6698 | ` * Compile a 'throw' statement.` |
|       - | 6699 | ` * throw: This is how you trigger an exception.` |
|       - | 6700 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 6701 | ` */` |
|    7808 | 6702 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 6703 |  |
|    7810 | 6704 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6705 | `	GenBlock *pBlock;` |
|       - | 6706 | `	sxu32 nIdx;` |
|       - | 6707 | `	sxi32 rc;` |
|    7810 | 6708 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 6709 | `	/* Compile the expression */` |
|    7810 | 6710 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    7810 | 6711 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 6712 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 6713 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6714 | `			return SXERR_ABORT;` |
|       - | 6715 | `		}` |
|     ! 0 | 6716 | `		return SXRET_OK;` |
|       - | 6717 | `	}` |
|    7810 | 6718 | `	pBlock = pGen->pCurrent;` |
|       - | 6719 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   36354 | 6720 | `	while(pBlock->pParent){` |
|   36350 | 6721 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    7806 | 6722 | `			break;` |
|       - | 6723 | `		}` |
|       - | 6724 | `		/* Point to the parent block */` |
|   28546 | 6725 | `		pBlock = pBlock->pParent;` |
|       2 | 6726 | `	}` |
|       - | 6727 | `	/* Emit the throw instruction */` |
|    7810 | 6728 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 6729 | `	/* Emit the jump */` |
|    7810 | 6730 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    7810 | 6731 | `	return SXRET_OK;` |
|    3906 | 6732 |  |
|       - | 6733 | `/*` |
|       - | 6734 | ` * Compile a 'catch' block.` |
|       - | 6735 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 6736 | ` * an object containing the exception information.` |
|       - | 6737 | ` */` |
|      56 | 6738 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 6739 |  |
|      58 | 6740 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6741 | `	ph7_exception_block sCatch;` |
|       - | 6742 | `	SySet *pInstrContainer;` |
|       - | 6743 | `	GenBlock *pCatch;` |
|       - | 6744 | `	SyToken *pToken;` |
|       - | 6745 | `	SyString *pName;` |
|       - | 6746 | `	char *zDup;` |
|       - | 6747 | `	sxi32 rc;` |
|      58 | 6748 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 6749 | `	/* Zero the structure */` |
|      58 | 6750 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 6751 | `	/* Initialize fields */` |
|      58 | 6752 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|      84 | 6753 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ \|\|` |
|      58 | 6754 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6755 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6756 | `			pToken = pGen->pIn;` |
|     ! 0 | 6757 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6758 | `				pToken--;` |
|     ! 0 | 6759 | `			}` |
|     ! 0 | 6760 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6761 | `				"Catch: Unexpected token '%z',excpecting class name",&pToken->sData);` |
|     ! 0 | 6762 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6763 | `				return SXERR_ABORT;` |
|       - | 6764 | `			}` |
|     ! 0 | 6765 | `			return SXERR_INVALID;` |
|       - | 6766 | `	}` |
|       - | 6767 | `	/* Extract the exception class */` |
|      58 | 6768 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|       - | 6769 | `	/* Duplicate class name */` |
|      58 | 6770 | `	pName = &pGen->pIn->sData;` |
|      58 | 6771 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      58 | 6772 | `	if( zDup == 0 ){` |
|     ! 0 | 6773 | `		goto Mem;` |
|       - | 6774 | `	}` |
|      58 | 6775 | `	SyStringInitFromBuf(&sCatch.sClass,zDup,pName->nByte);` |
|      58 | 6776 | `	pGen->pIn++;` |
|      84 | 6777 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      58 | 6778 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6779 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6780 | `			pToken = pGen->pIn;` |
|     ! 0 | 6781 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6782 | `				pToken--;` |
|     ! 0 | 6783 | `			}` |
|     ! 0 | 6784 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6785 | `				"Catch: Unexpected token '%z',expecting variable name",&pToken->sData);` |
|     ! 0 | 6786 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6787 | `				return SXERR_ABORT;` |
|       - | 6788 | `			}` |
|     ! 0 | 6789 | `			return SXERR_INVALID;` |
|       - | 6790 | `	}` |
|      58 | 6791 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 6792 | `	/* Duplicate instance name */` |
|      58 | 6793 | `	pName = &pGen->pIn->sData;` |
|      58 | 6794 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      58 | 6795 | `	if( zDup == 0 ){` |
|     ! 0 | 6796 | `		goto Mem;` |
|       - | 6797 | `	}` |
|      58 | 6798 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      58 | 6799 | `	pGen->pIn++;` |
|      58 | 6800 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 6801 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 6802 | `		pToken = pGen->pIn;` |
|     ! 0 | 6803 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6804 | `			pToken--;` |
|     ! 0 | 6805 | `		}` |
|     ! 0 | 6806 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6807 | `			"Catch: Unexpected token '%z',expecting right parenthesis ')'",&pToken->sData);` |
|     ! 0 | 6808 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6809 | `			return SXERR_ABORT;` |
|       - | 6810 | `		}` |
|     ! 0 | 6811 | `		return SXERR_INVALID;` |
|       - | 6812 | `	}` |
|       - | 6813 | `	/* Compile the block */` |
|      58 | 6814 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 6815 | `	/* Create the catch block */` |
|      58 | 6816 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      58 | 6817 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6818 | `		return SXERR_ABORT;` |
|       - | 6819 | `	}` |
|       - | 6820 | `	/* Swap bytecode container */` |
|      58 | 6821 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      58 | 6822 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 6823 | `	/* Compile the block */` |
|      58 | 6824 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 6825 | `	/* Fix forward jumps now the destination is resolved  */` |
|      58 | 6826 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6827 | `	/* Emit the DONE instruction */` |
|      58 | 6828 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6829 | `	/* Leave the block */` |
|      58 | 6830 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6831 | `	/* Restore the default container */` |
|      58 | 6832 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 6833 | `	/* Install the catch block */` |
|      58 | 6834 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      58 | 6835 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6836 | `		goto Mem;` |
|       - | 6837 | `	}` |
|      58 | 6838 | `	return SXRET_OK;` |
|     ! 0 | 6839 | `Mem:` |
|     ! 0 | 6840 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6841 | `	return SXERR_ABORT;` |
|      30 | 6842 |  |
|       - | 6843 | `/*` |
|       - | 6844 | ` * Compile a 'try' block.` |
|       - | 6845 | ` * A function using an exception should be in a "try" block.` |
|       - | 6846 | ` * If the exception does not trigger, the code will continue` |
|       - | 6847 | ` * as normal. However if the exception triggers, an exception` |
|       - | 6848 | ` * is "thrown".` |
|       - | 6849 | ` */` |
|      68 | 6850 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 6851 |  |
|       - | 6852 | `	ph7_exception *pException;` |
|      70 | 6853 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6854 | `	GenBlock *pTry;` |
|       - | 6855 | `	sxu32 nJmpIdx;` |
|       - | 6856 | `	sxi32 rc;` |
|       - | 6857 | `	/* Create the exception container */` |
|      70 | 6858 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|      70 | 6859 | `	if( pException == 0 ){` |
|     ! 0 | 6860 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 6861 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6862 | `		return SXERR_ABORT;` |
|       - | 6863 | `	}` |
|       - | 6864 | `	/* Zero the structure */` |
|      70 | 6865 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 6866 | `	/* Initialize fields */` |
|      70 | 6867 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|      70 | 6868 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      70 | 6869 | `	pException->iHasFinally = 0;` |
|      70 | 6870 | `	pException->iFinallyDone = 0;` |
|      70 | 6871 | `	pException->pVm = pGen->pVm;` |
|       - | 6872 | `	/* Create the try block */` |
|      70 | 6873 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      70 | 6874 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6875 | `		return SXERR_ABORT;` |
|       - | 6876 | `	}` |
|       - | 6877 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|      70 | 6878 | `	pTry->pUserData = pException;` |
|       - | 6879 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|      70 | 6880 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 6881 | `	/* Fix the jump later when the destination is resolved */` |
|      70 | 6882 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|      70 | 6883 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 6884 | `	/* Compile the block */` |
|      70 | 6885 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      70 | 6886 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6887 | `		return SXERR_ABORT;` |
|       - | 6888 | `	}` |
|       - | 6889 | `	/* Fix forward jumps now the destination is resolved */` |
|      70 | 6890 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6891 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|      70 | 6892 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 6893 | `	/* Leave the block */` |
|      70 | 6894 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6895 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|      70 | 6896 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      66 | 6897 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 6898 | `		/* Compile one or more catch blocks */` |
|      56 | 6899 | `		for(;;){` |
|     112 | 6900 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      90 | 6901 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      30 | 6902 | `					break;` |
|       - | 6903 | `			}` |
|      58 | 6904 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|      58 | 6905 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6906 | `				return SXERR_ABORT;` |
|       - | 6907 | `			}` |
|       2 | 6908 | `		}` |
|      28 | 6909 | `	}` |
|       - | 6910 | `	/* Compile optional finally block */` |
|      70 | 6911 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      36 | 6912 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 6913 | `		SySet *pInstrContainer;` |
|       - | 6914 | `		GenBlock *pFinBlock;` |
|      28 | 6915 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 6916 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      28 | 6917 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      28 | 6918 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6919 | `			return SXERR_ABORT;` |
|       - | 6920 | `		}` |
|       - | 6921 | `		/* Swap bytecode container */` |
|      28 | 6922 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      28 | 6923 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 6924 | `		/* Compile the finally body */` |
|      28 | 6925 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      28 | 6926 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6927 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 6928 | `			return SXERR_ABORT;` |
|       - | 6929 | `		}` |
|       - | 6930 | `		/* Fix forward jumps now the destination is resolved */` |
|      28 | 6931 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6932 | `		/* Emit DONE to terminate the finally block */` |
|      28 | 6933 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6934 | `		/* Leave the block */` |
|      28 | 6935 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6936 | `		/* Restore the default container */` |
|      28 | 6937 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      28 | 6938 | `		pException->iHasFinally = 1;` |
|      13 | 6939 | `	}` |
|       - | 6940 | `	/* Must have at least one catch or finally */` |
|      70 | 6941 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       3 | 6942 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 6943 | `			"Cannot use try without catch or finally");` |
|       3 | 6944 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6945 | `			return SXERR_ABORT;` |
|       - | 6946 | `		}` |
|       1 | 6947 | `	}` |
|      70 | 6948 | `	return SXRET_OK;` |
|      36 | 6949 |  |
|       - | 6950 | `/*` |
|       - | 6951 | ` * Compile a switch block.` |
|       - | 6952 | ` *  (See block-comment below for more information)` |
|       - | 6953 | ` */` |
|      98 | 6954 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 6955 |  |
|     100 | 6956 | `	sxi32 rc = SXRET_OK;` |
|     100 | 6957 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 6958 | `		/* Unexpected token */` |
|     ! 0 | 6959 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 6960 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6961 | `			return SXERR_ABORT;` |
|       - | 6962 | `		}` |
|     ! 0 | 6963 | `		pGen->pIn++;` |
|     ! 0 | 6964 | `	}` |
|     100 | 6965 | `	pGen->pIn++;` |
|       - | 6966 | `	/* First instruction to execute in this block. */` |
|     100 | 6967 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 6968 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 6969 | `	 * or the '}' token */` |
|     172 | 6970 | `	for(;;){` |
|     346 | 6971 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6972 | `			/* No more input to process */` |
|     ! 0 | 6973 | `			break;` |
|       - | 6974 | `		}` |
|     346 | 6975 | `		rc = SXRET_OK;` |
|     346 | 6976 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      68 | 6977 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      26 | 6978 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 6979 | `					/* Unexpected token */` |
|     ! 0 | 6980 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 6981 | `						&pGen->pIn->sData);` |
|     ! 0 | 6982 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6983 | `						return SXERR_ABORT;` |
|       - | 6984 | `					}` |
|       - | 6985 | `					/* FALL THROUGH */` |
|     ! 0 | 6986 | `				}` |
|      26 | 6987 | `				rc = SXERR_EOF;` |
|      26 | 6988 | `				break;` |
|       - | 6989 | `			}` |
|      23 | 6990 | `		}else{` |
|       - | 6991 | `			sxi32 nKwrd;` |
|       - | 6992 | `			/* Extract the keyword */` |
|     280 | 6993 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     280 | 6994 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      38 | 6995 | `				break;` |
|       - | 6996 | `			}` |
|     208 | 6997 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 6998 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 6999 | `					/* Unexpected token */` |
|     ! 0 | 7000 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 7001 | `						&pGen->pIn->sData);` |
|     ! 0 | 7002 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7003 | `						return SXERR_ABORT;` |
|       - | 7004 | `					}` |
|       - | 7005 | `					/* FALL THROUGH */` |
|     ! 0 | 7006 | `				}` |
|       - | 7007 | `				/* Block compiled */` |
|       3 | 7008 | `				break;` |
|       - | 7009 | `			}` |
|       - | 7010 | `		}` |
|       - | 7011 | `		/* Compile block */` |
|     248 | 7012 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     248 | 7013 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7014 | `			return SXERR_ABORT;` |
|       - | 7015 | `		}` |
|       2 | 7016 | `	}` |
|     100 | 7017 | `	return rc;` |
|      51 | 7018 |  |
|       - | 7019 | `/*` |
|       - | 7020 | ` * Compile a case eXpression.` |
|       - | 7021 | ` *  (See block-comment below for more information)` |
|       - | 7022 | ` */` |
|      80 | 7023 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 7024 |  |
|       - | 7025 | `	SySet *pInstrContainer;` |
|       - | 7026 | `	SyToken *pEnd,*pTmp;` |
|      82 | 7027 | `	sxi32 iNest = 0;` |
|       - | 7028 | `	sxi32 rc;` |
|       - | 7029 | `	/* Delimit the expression */` |
|      82 | 7030 | `	pEnd = pGen->pIn;` |
|     170 | 7031 | `	while( pEnd < pGen->pEnd ){` |
|     170 | 7032 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 7033 | `			/* Increment nesting level */` |
|       3 | 7034 | `			iNest++;` |
|     169 | 7035 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 7036 | `			/* Decrement nesting level */` |
|       3 | 7037 | `			iNest--;` |
|     167 | 7038 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      82 | 7039 | `			break;` |
|       - | 7040 | `		}` |
|      90 | 7041 | `		pEnd++;` |
|       2 | 7042 | `	}` |
|      82 | 7043 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 7044 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 7045 | `		if( rc == SXERR_ABORT ){` |
|       - | 7046 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7047 | `			return SXERR_ABORT;` |
|       - | 7048 | `		}` |
|     ! 0 | 7049 | `	}` |
|       - | 7050 | `	/* Swap token stream */` |
|      82 | 7051 | `	pTmp = pGen->pEnd;` |
|      82 | 7052 | `	pGen->pEnd = pEnd;` |
|      82 | 7053 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      82 | 7054 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      82 | 7055 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 7056 | `	/* Emit the done instruction */` |
|      82 | 7057 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      82 | 7058 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 7059 | `	/* Update token stream */` |
|      82 | 7060 | `	pGen->pIn  = pEnd;` |
|      82 | 7061 | `	pGen->pEnd = pTmp;` |
|      82 | 7062 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 7063 | `		return SXERR_ABORT;` |
|       - | 7064 | `	}` |
|      82 | 7065 | `	return SXRET_OK;` |
|      42 | 7066 |  |
|       - | 7067 | `/*` |
|       - | 7068 | ` * Compile the smart switch statement.` |
|       - | 7069 | ` * According to the PHP language reference manual` |
|       - | 7070 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 7071 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 7072 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 7073 | ` *  This is exactly what the switch statement is for.` |
|       - | 7074 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 7075 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 7076 | ` *  of the outer loop, use continue 2.` |
|       - | 7077 | ` *  Note that switch/case does loose comparision.` |
|       - | 7078 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 7079 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 7080 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 7081 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 7082 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 7083 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 7084 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 7085 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 7086 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 7087 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 7088 | ` *  list for the next case.` |
|       - | 7089 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 7090 | ` *  or floating-point numbers and strings.` |
|       - | 7091 | ` */` |
|      26 | 7092 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 7093 |  |
|       - | 7094 | `	GenBlock *pSwitchBlock;` |
|       - | 7095 | `	SyToken *pTmp,*pEnd;` |
|       - | 7096 | `	ph7_switch *pSwitch;` |
|       - | 7097 | `	sxu32 nToken;` |
|       - | 7098 | `	sxu32 nLine;` |
|       - | 7099 | `	sxi32 rc;` |
|      28 | 7100 | `	nLine = pGen->pIn->nLine;` |
|       - | 7101 | `	/* Jump the 'switch' keyword */` |
|      28 | 7102 | `	pGen->pIn++;` |
|      28 | 7103 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 7104 | `		/* Syntax error */` |
|     ! 0 | 7105 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 7106 | `		if( rc == SXERR_ABORT ){` |
|       - | 7107 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7108 | `			return SXERR_ABORT;` |
|       - | 7109 | `		}` |
|     ! 0 | 7110 | `		goto Synchronize;` |
|       - | 7111 | `	}` |
|       - | 7112 | `	/* Jump the left parenthesis '(' */` |
|      28 | 7113 | `	pGen->pIn++;` |
|      28 | 7114 | `	pEnd = 0; /* cc warning */` |
|       - | 7115 | `	/* Create the loop block */` |
|      41 | 7116 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      13 | 7117 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      28 | 7118 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7119 | `		return SXERR_ABORT;` |
|       - | 7120 | `	}` |
|       - | 7121 | `	/* Delimit the condition */` |
|      28 | 7122 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      28 | 7123 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 7124 | `		/* Empty expression */` |
|     ! 0 | 7125 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 7126 | `		if( rc == SXERR_ABORT ){` |
|       - | 7127 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7128 | `			return SXERR_ABORT;` |
|       - | 7129 | `		}` |
|     ! 0 | 7130 | `	}` |
|       - | 7131 | `	/* Swap token streams */` |
|      28 | 7132 | `	pTmp = pGen->pEnd;` |
|      28 | 7133 | `	pGen->pEnd = pEnd;` |
|       - | 7134 | `	/* Compile the expression */` |
|      28 | 7135 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      28 | 7136 | `	if( rc == SXERR_ABORT ){` |
|       - | 7137 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 7138 | `		return SXERR_ABORT;` |
|       - | 7139 | `	}` |
|       - | 7140 | `	/* Update token stream */` |
|      28 | 7141 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 7142 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 7143 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 7144 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7145 | `			return SXERR_ABORT;` |
|       - | 7146 | `		}` |
|     ! 0 | 7147 | `		pGen->pIn++;` |
|     ! 0 | 7148 | `	}` |
|      28 | 7149 | `	pGen->pIn  = &pEnd[1];` |
|      28 | 7150 | `	pGen->pEnd = pTmp;` |
|      28 | 7151 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      26 | 7152 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 7153 | `			pTmp = pGen->pIn;` |
|     ! 0 | 7154 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 7155 | `				pTmp--;` |
|     ! 0 | 7156 | `			}` |
|       - | 7157 | `			/* Unexpected token */` |
|     ! 0 | 7158 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 7159 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7160 | `				return SXERR_ABORT;` |
|       - | 7161 | `			}` |
|     ! 0 | 7162 | `			goto Synchronize;` |
|       - | 7163 | `	}` |
|       - | 7164 | `	/* Set the delimiter token */` |
|      28 | 7165 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 7166 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 7167 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 7168 | `	}else{` |
|      26 | 7169 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 7170 | `	}` |
|      28 | 7171 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 7172 | `	/* Create the switch blocks container */` |
|      28 | 7173 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      28 | 7174 | `	if( pSwitch == 0 ){` |
|       - | 7175 | `		/* Abort compilation */` |
|     ! 0 | 7176 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 7177 | `		return SXERR_ABORT;` |
|       - | 7178 | `	}` |
|       - | 7179 | `	/* Zero the structure */` |
|      28 | 7180 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 7181 | `	/* Initialize fields */` |
|      28 | 7182 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 7183 | `	/* Emit the switch instruction */` |
|      28 | 7184 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 7185 | `	/* Compile case blocks */` |
|      87 | 7186 | `	for(;;){` |
|       - | 7187 | `		sxu32 nKwrd;` |
|     102 | 7188 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7189 | `			/* No more input to process */` |
|     ! 0 | 7190 | `			break;` |
|       - | 7191 | `		}` |
|     102 | 7192 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 7193 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 7194 | `				/* Unexpected token */` |
|     ! 0 | 7195 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7196 | `					&pGen->pIn->sData);` |
|     ! 0 | 7197 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7198 | `					return SXERR_ABORT;` |
|       - | 7199 | `				}` |
|       - | 7200 | `				/* FALL THROUGH */` |
|     ! 0 | 7201 | `			}` |
|       - | 7202 | `			/* Block compiled */` |
|     ! 0 | 7203 | `			break;` |
|       - | 7204 | `		}` |
|       - | 7205 | `		/* Extract the keyword */` |
|     102 | 7206 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     102 | 7207 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 7208 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 7209 | `				/* Unexpected token */` |
|     ! 0 | 7210 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7211 | `					&pGen->pIn->sData);` |
|     ! 0 | 7212 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7213 | `					return SXERR_ABORT;` |
|       - | 7214 | `				}` |
|       - | 7215 | `				/* FALL THROUGH */` |
|     ! 0 | 7216 | `			}` |
|       - | 7217 | `			/* Block compiled */` |
|       3 | 7218 | `			break;` |
|       - | 7219 | `		}` |
|     100 | 7220 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 7221 | `			/*` |
|       - | 7222 | `			 * Accroding to the PHP language reference manual` |
|       - | 7223 | `			 *  A special case is the default case. This case matches anything` |
|       - | 7224 | `			 *  that wasn't matched by the other cases.` |
|       - | 7225 | `			 */` |
|      20 | 7226 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 7227 | `				/* Default case already compiled */` |
|     ! 0 | 7228 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 7229 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7230 | `					return SXERR_ABORT;` |
|       - | 7231 | `				}` |
|     ! 0 | 7232 | `			}` |
|      20 | 7233 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 7234 | `			/* Compile the default block */` |
|      20 | 7235 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      20 | 7236 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7237 | `				return SXERR_ABORT;` |
|      20 | 7238 | `			}else if( rc == SXERR_EOF ){` |
|      18 | 7239 | `				break;` |
|       1 | 7240 | `			}` |
|      83 | 7241 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 7242 | `			ph7_case_expr sCase;` |
|       - | 7243 | `			/* Standard case block */` |
|      82 | 7244 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 7245 | `			/* initialize the structure */` |
|      82 | 7246 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 7247 | `			/* Compile the case expression */` |
|      82 | 7248 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      82 | 7249 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7250 | `				return SXERR_ABORT;` |
|       - | 7251 | `			}` |
|       - | 7252 | `			/* Compile the case block */` |
|      82 | 7253 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 7254 | `			/* Insert in the switch container */` |
|      82 | 7255 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      82 | 7256 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7257 | `				return SXERR_ABORT;` |
|      82 | 7258 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 7259 | `				break;` |
|       - | 7260 | `			}` |
|      38 | 7261 | `		}else{` |
|       - | 7262 | `			/* Unexpected token */` |
|     ! 0 | 7263 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7264 | `				&pGen->pIn->sData);` |
|     ! 0 | 7265 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7266 | `				return SXERR_ABORT;` |
|       - | 7267 | `			}` |
|     ! 0 | 7268 | `			break;` |
|       - | 7269 | `		}` |
|       2 | 7270 | `	}` |
|       - | 7271 | `	/* Fix all jumps now the destination is resolved */` |
|      28 | 7272 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 7273 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7274 | `	/* Release the loop block */` |
|      28 | 7275 | `	GenStateLeaveBlock(pGen,0);` |
|      28 | 7276 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 7277 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      28 | 7278 | `		pGen->pIn++;` |
|      13 | 7279 | `	}` |
|       - | 7280 | `	/* Statement successfully compiled */` |
|      28 | 7281 | `	return SXRET_OK;` |
|     ! 0 | 7282 | `Synchronize:` |
|       - | 7283 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 7284 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 7285 | `		pGen->pIn++;` |
|     ! 0 | 7286 | `	}` |
|     ! 0 | 7287 | `	return SXRET_OK;` |
|      15 | 7288 |  |
|       - | 7289 | `/*` |
|       - | 7290 | ` * Generate bytecode for a given expression tree.` |
|       - | 7291 | ` * If something goes wrong while generating bytecode` |
|       - | 7292 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 7293 | ` * this function takes care of generating the appropriate` |
|       - | 7294 | ` * error message.` |
|       - | 7295 | ` */` |
| 2332150 | 7296 | `static sxi32 GenStateEmitExprCode(` |
|       - | 7297 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7298 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 7299 | `	sxi32 iFlags /* Control flags */` |
|       - | 7300 | `	)` |
|       2 | 7301 |  |
|       - | 7302 | `	VmInstr *pInstr;` |
|       - | 7303 | `	sxu32 nJmpIdx;` |
| 2332152 | 7304 | `	sxi32 iP1 = 0;` |
| 2332152 | 7305 | `	sxu32 iP2 = 0;` |
| 2332152 | 7306 | `	void *p3  = 0;` |
|       - | 7307 | `	sxi32 iVmOp;` |
|       - | 7308 | `	sxi32 rc;` |
| 2332152 | 7309 | `	if( pNode->xCode ){` |
|       - | 7310 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 7311 | `		/* Compile node */` |
| 1445636 | 7312 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1445636 | 7313 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1445636 | 7314 | `		RE_SWAP_DELIMITER(pGen);` |
| 1445636 | 7315 | `		return rc;` |
|       - | 7316 | `	}` |
|  886518 | 7317 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 7318 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 7319 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 7320 | `		return SXERR_ABORT;` |
|       - | 7321 | `	}` |
|  886518 | 7322 | `	iVmOp = pNode->pOp->iVmOp;` |
|  886518 | 7323 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 7324 | `		sxu32 nJz,nJmp;` |
|       - | 7325 | `		/* Ternary operator require special handling */` |
|       - | 7326 | `		/* Phase#1: Compile the condition */` |
|    1880 | 7327 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1880 | 7328 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7329 | `			return rc;` |
|       - | 7330 | `		}` |
|    1880 | 7331 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1880 | 7332 | `		if( pNode->pLeft ){` |
|       - | 7333 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 7334 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1812 | 7335 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7336 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1812 | 7337 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1812 | 7338 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7339 | `				return rc;` |
|       - | 7340 | `			}` |
|     907 | 7341 | `		}else{` |
|       - | 7342 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 7343 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 7344 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 7345 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 7346 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7347 | `		}` |
|       - | 7348 | `		/* Phase#4: Emit the unconditional jump */` |
|    1880 | 7349 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 7350 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1880 | 7351 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1880 | 7352 | `		if( pInstr ){` |
|    1880 | 7353 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     939 | 7354 | `		}` |
|    1880 | 7355 | `		if( !pNode->pLeft ){` |
|       - | 7356 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 7357 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 7358 | `		}` |
|       - | 7359 | `		/* Phase#6: Compile the 'else' expression */` |
|    1880 | 7360 | `		if( pNode->pRight ){` |
|    1880 | 7361 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1880 | 7362 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7363 | `				return rc;` |
|       - | 7364 | `			}` |
|     939 | 7365 | `		}` |
|    1880 | 7366 | `		if( nJmp > 0 ){` |
|       - | 7367 | `			/* Phase#7: Fix the unconditional jump */` |
|    1880 | 7368 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1880 | 7369 | `			if( pInstr ){` |
|    1880 | 7370 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     939 | 7371 | `			}` |
|     939 | 7372 | `		}` |
|       - | 7373 | `		/* All done */` |
|    1880 | 7374 | `		return SXRET_OK;` |
|       - | 7375 | `	}` |
|       - | 7376 | `	/* Generate code for the left tree */` |
|  884640 | 7377 | `	if( pNode->pLeft ){` |
|  884620 | 7378 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 7379 | `			ph7_expr_node **apNode;` |
|  297128 | 7380 | `			int hasSpread = 0;` |
|       - | 7381 | `			sxi32 n;` |
|       - | 7382 | `			/* Recurse and generate bytecodes for function arguments */` |
|  297128 | 7383 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 7384 | `			/* Read-only load */` |
|  297128 | 7385 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  593804 | 7386 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  296678 | 7387 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  296678 | 7388 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7389 | `					return rc;` |
|       - | 7390 | `				}` |
|  296678 | 7391 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 7392 | `					/* Emit spread opcode to unpack this array argument */` |
|      15 | 7393 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      15 | 7394 | `					hasSpread = 1;` |
|       7 | 7395 | `				}` |
|  148340 | 7396 | `			}` |
|       - | 7397 | `			/* Total number of given arguments */` |
|  297128 | 7398 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|  297128 | 7399 | `			iP2 = hasSpread;` |
|       - | 7400 | `			/* Remove stale flags now */` |
|  297128 | 7401 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  148563 | 7402 | `		}` |
|  884620 | 7403 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  884620 | 7404 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7405 | `			return rc;` |
|       - | 7406 | `		}` |
|  884620 | 7407 | `		if( iVmOp == PH7_OP_CALL ){` |
|  297128 | 7408 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  297128 | 7409 | `			if( pInstr ){` |
|  297128 | 7410 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  296614 | 7411 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 7412 | `					sxu32 nQual;` |
|       - | 7413 | `					/* Prevent constant expansion */` |
|  296614 | 7414 | `					pInstr->iP1 = 0;` |
|       - | 7415 | `					/* Namespace-qualify the function name for CALL.` |
|       - | 7416 | `					 * Only check function imports — class imports must NOT` |
|       - | 7417 | ``					 * affect function resolution.  For `new Foo()`, the CALL`` |
|       - | 7418 | `					 * handler fires before NEW; we store the original literal` |
|       - | 7419 | `					 * index in the CALL instruction's iP2 so the NEW handler` |
|       - | 7420 | `					 * can recover the unqualified name and re-qualify with` |
|       - | 7421 | `					 * class imports. */ {` |
|  296614 | 7422 | `						int fromImport = 0;` |
|  296614 | 7423 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  296614 | 7424 | `						pInstr->iP2 = (sxi32)nQual;` |
|  296614 | 7425 | `						if( nQual != nOrig ){` |
|       - | 7426 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 7427 | `							 * NEW handler can recover the unqualified name. */` |
|      62 | 7428 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      62 | 7429 | `							if( !fromImport ){` |
|      52 | 7430 | `								p3 = (void *)1;` |
|      25 | 7431 | `							}` |
|      32 | 7432 | `						}` |
|       - | 7433 | `					}` |
|  148822 | 7434 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 7435 | `					/* Method call,flag that */` |
|     494 | 7436 | `					pInstr->iP2 = 1;` |
|     246 | 7437 | `				}` |
|  148565 | 7438 | `			}` |
|  736057 | 7439 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 7440 | `			ph7_expr_node **apNode;` |
|       - | 7441 | `			sxi32 n;` |
|       - | 7442 | `			/* Recurse and generate bytecodes for array index */` |
|   66560 | 7443 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  120100 | 7444 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   53542 | 7445 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   53542 | 7446 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7447 | `					return rc;` |
|       - | 7448 | `				}` |
|   26772 | 7449 | `			}` |
|   66560 | 7450 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   53542 | 7451 | `				iP1 = 1; /* Node have an index associated with it */` |
|   26770 | 7452 | `			}` |
|   66560 | 7453 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 7454 | `				/* Create an empty entry when the desired index is not found */` |
|   26280 | 7455 | `				iP2 = 1;` |
|   13141 | 7456 | `			}` |
|  554215 | 7457 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 7458 | `			/* POP the left node */` |
|      32 | 7459 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 7460 | `		}` |
|  442309 | 7461 | `	}` |
|  884640 | 7462 | `	rc = SXRET_OK;` |
|  884640 | 7463 | `	nJmpIdx = 0;` |
|       - | 7464 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 7465 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 7466 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  884640 | 7467 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     172 | 7468 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     172 | 7469 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     172 | 7470 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     172 | 7471 | `			int isSpecial = 0;` |
|     172 | 7472 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     120 | 7473 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     120 | 7474 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     126 | 7475 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     111 | 7476 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      56 | 7477 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      50 | 7478 | `					isSpecial = 1;` |
|      24 | 7479 | `				}` |
|      72 | 7480 | `			}` |
|     198 | 7481 | `			pInstr->iP1 = 0;` |
|     198 | 7482 | `			if( !isSpecial ){` |
|      98 | 7483 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      48 | 7484 | `			}` |
|       - | 7485 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 7486 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     146 | 7487 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|      98 | 7488 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|      98 | 7489 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 7490 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 | 7491 | `					return SXRET_OK;` |
|       - | 7492 | `				}` |
|      27 | 7493 | `			}` |
|      51 | 7494 | `		}` |
|      91 | 7495 | `	}` |
|       - | 7496 | `	/* Generate code for the right tree */` |
|  884580 | 7497 | `	if( pNode->pRight ){` |
|  462052 | 7498 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 7499 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    8194 | 7500 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  457956 | 7501 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 7502 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2738 | 7503 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  452492 | 7504 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 7505 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      32 | 7506 | `			iVmOp = 0; /* No binary operator to emit */` |
|      32 | 7507 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  451109 | 7508 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  201712 | 7509 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  100855 | 7510 | `		}` |
|  462052 | 7511 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  462052 | 7512 | `		if( iVmOp == PH7_OP_STORE ){` |
|  198954 | 7513 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  198954 | 7514 | `			if( pInstr ){` |
|  198954 | 7515 | `				if( pInstr->iOp == PH7_OP_LOAD_LIST ){` |
|       - | 7516 | `					/* Hide the STORE instruction */` |
|      26 | 7517 | `					iVmOp = 0;` |
|  198942 | 7518 | `				}else if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 7519 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   44152 | 7520 | `					iP2 = 1;` |
|   22077 | 7521 | `				}else{` |
|  154780 | 7522 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7523 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   26242 | 7524 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   26242 | 7525 | `						iP1 = pInstr->iP1;` |
|   13122 | 7526 | `					}else{` |
|  128540 | 7527 | `						p3 = pInstr->p3;` |
|       - | 7528 | `					}` |
|       - | 7529 | `					/* POP the last dynamic load instruction */` |
|  154780 | 7530 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 7531 | `				}` |
|   99478 | 7532 | `			}` |
|  362576 | 7533 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      46 | 7534 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      46 | 7535 | `			if( pInstr ){` |
|      46 | 7536 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7537 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 7538 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 7539 | `					 */` |
|      15 | 7540 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 7541 | `					iP1 = pInstr->iP1;` |
|      15 | 7542 | `					iP2 = pInstr->iP2;` |
|      15 | 7543 | `					p3  = pInstr->p3;` |
|       8 | 7544 | `				}else{` |
|      32 | 7545 | `					p3 = pInstr->p3;` |
|       - | 7546 | `				}` |
|      22 | 7547 | `			}` |
|      22 | 7548 | `		}` |
|  231025 | 7549 | `	}` |
|  884580 | 7550 | `	if( iVmOp > 0 ){` |
|  884496 | 7551 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   10602 | 7552 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 7553 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    7784 | 7554 | `				iP1 = 1;` |
|    3893 | 7555 | `			}` |
|  879196 | 7556 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 7557 | `			/* Namespace-qualify the class name for NEW */ {` |
|   13346 | 7558 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   13346 | 7559 | `				VmInstr *pCallInstr = 0;` |
|   13346 | 7560 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   13330 | 7561 | `					pCallInstr = pPeek;` |
|   13330 | 7562 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    6664 | 7563 | `				}` |
|   13346 | 7564 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 7565 | `					sxu32 nLitForClass;` |
|       - | 7566 | `					/* If the CALL handler already qualified the name using` |
|       - | 7567 | `					 * function imports, recover the original unqualified` |
|       - | 7568 | `					 * literal so we can re-qualify with class imports. */` |
|   13344 | 7569 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      26 | 7570 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      14 | 7571 | `					}else{` |
|   13320 | 7572 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 7573 | `					}` |
|   13344 | 7574 | `					pPeek->iP1 = 0;` |
|   13344 | 7575 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    6671 | 7576 | `				}` |
|       - | 7577 | `			}` |
|   13346 | 7578 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   13346 | 7579 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 7580 | `				VmInstr *pPrev;` |
|   13330 | 7581 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   13330 | 7582 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 7583 | `					/* Pop the call instruction */` |
|   13330 | 7584 | `					iP1 = pInstr->iP1;` |
|   13330 | 7585 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    6664 | 7586 | `				}` |
|    6666 | 7587 | `			}` |
|  867224 | 7588 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 7589 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 7590 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 7591 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 7592 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 7593 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 7594 | `				int isSpecialIs = 0;` |
|      50 | 7595 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 7596 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 7597 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 7598 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 7599 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 7600 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 7601 | `						isSpecialIs = 1;` |
|       5 | 7602 | `					}` |
|      23 | 7603 | `				}` |
|      52 | 7604 | `				pInstr->iP1 = 0;` |
|      52 | 7605 | `				if( !isSpecialIs ){` |
|      38 | 7606 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      18 | 7607 | `				}` |
|      25 | 7608 | `			}` |
|  860531 | 7609 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 7610 | `			/* Prevent constant expansion for member/property names.` |
|       - | 7611 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 7612 | `			 * should not trigger constant lookup. */` |
|   99362 | 7613 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   99362 | 7614 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   99346 | 7615 | `				pInstr->iP1 = 0;` |
|   49672 | 7616 | `			}` |
|   99362 | 7617 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 7618 | `				/* Static member access,remember that */` |
|     112 | 7619 | `				iP1 = 1;` |
|     112 | 7620 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     112 | 7621 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      10 | 7622 | `					p3 = pInstr->p3;` |
|      10 | 7623 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       4 | 7624 | `				}` |
|      55 | 7625 | `			}` |
|   49680 | 7626 | `		}` |
|       - | 7627 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  884494 | 7628 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  442246 | 7629 | `	}` |
|  884578 | 7630 | `	if( nJmpIdx > 0 ){` |
|       - | 7631 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   10960 | 7632 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   10960 | 7633 | `		if( pInstr ){` |
|   10960 | 7634 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5479 | 7635 | `		}` |
|    5479 | 7636 | `	}` |
|  884578 | 7637 | `	return rc;` |
| 1166067 | 7638 |  |
|       - | 7639 | `/*` |
|       - | 7640 | ` * Compile a PHP expression.` |
|       - | 7641 | ` * According to the PHP language reference manual:` |
|       - | 7642 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 7643 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 7644 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 7645 | ` *  is "anything that has a value".` |
|       - | 7646 | ` * If something goes wrong while compiling the expression,this` |
|       - | 7647 | ` * function takes care of generating the appropriate error` |
|       - | 7648 | ` * message.` |
|       - | 7649 | ` */` |
|  629846 | 7650 | `static sxi32 PH7_CompileExpr(` |
|       - | 7651 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7652 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 7653 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 7654 | `	)` |
|       2 | 7655 |  |
|       - | 7656 | `	ph7_expr_node *pRoot;` |
|       - | 7657 | `	SySet sExprNode;` |
|       - | 7658 | `	SyToken *pEnd;` |
|       - | 7659 | `	sxi32 nExpr;` |
|       - | 7660 | `	sxi32 iNest;` |
|       - | 7661 | `	sxi32 rc;` |
|       - | 7662 | `	/* Initialize worker variables */` |
|  629848 | 7663 | `	nExpr = 0;` |
|  629848 | 7664 | `	pRoot = 0;` |
|  629848 | 7665 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  629848 | 7666 | `	SySetAlloc(&sExprNode,0x10);` |
|  629848 | 7667 | `	rc = SXRET_OK;` |
|       - | 7668 | `	/* Delimit the expression */` |
|  629848 | 7669 | `	pEnd = pGen->pIn;` |
|  629848 | 7670 | `	iNest = 0;` |
| 4246676 | 7671 | `	while( pEnd < pGen->pEnd ){` |
| 4027014 | 7672 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7673 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     230 | 7674 | `			iNest++;` |
| 4026900 | 7675 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     238 | 7676 | `			iNest--;` |
| 4026668 | 7677 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  410386 | 7678 | `			if( iNest <= 0 ){` |
|  410186 | 7679 | `				break;` |
|       - | 7680 | `			}` |
|     100 | 7681 | `		}` |
| 3616830 | 7682 | `		pEnd++;` |
|       2 | 7683 | `	}` |
|  629848 | 7684 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   10540 | 7685 | `		SyToken *pEnd2 = pGen->pIn;` |
|   10540 | 7686 | `		iNest = 0;` |
|       - | 7687 | `		/* Stop at the first comma */` |
|   21102 | 7688 | `		while( pEnd2 < pEnd ){` |
|   10564 | 7689 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       6 | 7690 | `				iNest++;` |
|   10562 | 7691 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       6 | 7692 | `				iNest--;` |
|   10558 | 7693 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 7694 | `				if( iNest <= 0 ){` |
|     ! 0 | 7695 | `					break;` |
|       - | 7696 | `				}` |
|       2 | 7697 | `			}` |
|   10564 | 7698 | `			pEnd2++;` |
|       2 | 7699 | `		}` |
|   10540 | 7700 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 7701 | `			pEnd = pEnd2;` |
|     ! 0 | 7702 | `		}` |
|    5269 | 7703 | `	}` |
|  629848 | 7704 | `	if( pEnd > pGen->pIn ){` |
|  629838 | 7705 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 7706 | `		/* Swap delimiter */` |
|  629838 | 7707 | `		pGen->pEnd = pEnd;` |
|       - | 7708 | `		/* Try to get an expression tree */` |
|  629838 | 7709 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  629838 | 7710 | `		if( rc == SXRET_OK && pRoot ){` |
|  629682 | 7711 | `			rc = SXRET_OK;` |
|  629682 | 7712 | `			if( xTreeValidator ){` |
|       - | 7713 | `				/* Call the upper layer validator callback */` |
|   13484 | 7714 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    6741 | 7715 | `			}` |
|  629682 | 7716 | `			if( rc != SXERR_ABORT ){` |
|       - | 7717 | `				/* Generate code for the given tree */` |
|  629682 | 7718 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  314840 | 7719 | `			}` |
|  629682 | 7720 | `			nExpr = 1;` |
|  314840 | 7721 | `		}` |
|       - | 7722 | `		/* Release the whole tree */` |
|  629838 | 7723 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 7724 | `		/* Synchronize token stream */` |
|  629838 | 7725 | `		pGen->pEnd = pTmp;` |
|  629838 | 7726 | `		pGen->pIn  = pEnd;` |
|  629838 | 7727 | `		if( rc == SXERR_ABORT ){` |
|       3 | 7728 | `			SySetRelease(&sExprNode);` |
|       3 | 7729 | `			return SXERR_ABORT;` |
|       - | 7730 | `		}` |
|  314917 | 7731 | `	}` |
|  629846 | 7732 | `	SySetRelease(&sExprNode);` |
|  629846 | 7733 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  314925 | 7734 |  |
|       - | 7735 | `/*` |
|       - | 7736 | ` * Return a pointer to the node construct handler associated` |
|       - | 7737 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 7738 | ` */` |
|  156986 | 7739 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 7740 |  |
|  156988 | 7741 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 7742 | `		/* Numeric literal: Either real or integer */` |
|   85736 | 7743 | `		return PH7_CompileNumLiteral;` |
|   71254 | 7744 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 7745 | `		/* Double quoted string */` |
|   15102 | 7746 | `		return PH7_CompileString;` |
|   56154 | 7747 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 7748 | `		/* Single quoted string */` |
|   56094 | 7749 | `		return PH7_CompileSimpleString;` |
|      62 | 7750 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 7751 | `		/* Heredoc */` |
|      28 | 7752 | `		return PH7_CompileHereDoc;` |
|      36 | 7753 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 7754 | `		/* Nowdoc */` |
|      29 | 7755 | `		return PH7_CompileNowDoc;` |
|       7 | 7756 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 7757 | `		/* Backtick quoted string */` |
|       5 | 7758 | `		return PH7_CompileBacktic;` |
|       - | 7759 | `	}` |
|       3 | 7760 | `	return 0;` |
|   78495 | 7761 |  |
|       - | 7762 | `/*` |
|       - | 7763 | ` * Compile an unset() statement.` |
|       - | 7764 | ` * unset($var, $arr[$key], ...);` |
|       - | 7765 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 7766 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 7767 | ` * parent array before extracting the element to unset.` |
|       - | 7768 | ` */` |
|    2548 | 7769 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 7770 |  |
|    2550 | 7771 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2550 | 7772 | `	sxu32 nIdx = 0;` |
|       - | 7773 | `	SyString sName;` |
|       - | 7774 | `	sxi32 rc;` |
|       - | 7775 | `	/* Jump the 'unset' keyword */` |
|    2550 | 7776 | `	pGen->pIn++;` |
|       - | 7777 | `	/* Save delimiter */` |
|    2550 | 7778 | `	pTmp = pGen->pEnd;` |
|       - | 7779 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2550 | 7780 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2550 | 7781 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 7782 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 7783 | `		SyToken *pClose;` |
|    2550 | 7784 | `		pGen->pIn++;   /* Skip '(' */` |
|    2550 | 7785 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2550 | 7786 | `		pEnd = pClose; /* Stop at ')' */` |
|    1274 | 7787 | `	}` |
|    2550 | 7788 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 7789 | `	/* Resolve the 'unset' builtin name once */` |
|    2550 | 7790 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     300 | 7791 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     300 | 7792 | `		if( pObj == 0 ){` |
|     ! 0 | 7793 | `			return SXERR_ABORT;` |
|       - | 7794 | `		}` |
|     300 | 7795 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     300 | 7796 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     149 | 7797 | `	}` |
|       - | 7798 | `	/* Compile each comma-separated argument */` |
|    8480 | 7799 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    5932 | 7800 | `		if( pGen->pIn < pNext ){` |
|    5932 | 7801 | `			pGen->pEnd = pNext;` |
|    5932 | 7802 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 7803 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,0);` |
|    5932 | 7804 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7805 | `				return SXERR_ABORT;` |
|       - | 7806 | `			}` |
|    5932 | 7807 | `			if( rc != SXERR_EMPTY ){` |
|       - | 7808 | `				/* Emit call for this single argument */` |
|    5930 | 7809 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5930 | 7810 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    5930 | 7811 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    2964 | 7812 | `			}` |
|    2965 | 7813 | `		}` |
|       - | 7814 | `		/* Jump trailing commas */` |
|    9314 | 7815 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3384 | 7816 | `			pNext++;` |
|       2 | 7817 | `		}` |
|    5932 | 7818 | `		pGen->pIn = pNext;` |
|       2 | 7819 | `	}` |
|       - | 7820 | `	/* Skip past the closing ')' if present */` |
|    2550 | 7821 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2550 | 7822 | `		pGen->pIn++;` |
|    1274 | 7823 | `	}` |
|       - | 7824 | `	/* Restore token stream */` |
|    2550 | 7825 | `	pGen->pEnd = pTmp;` |
|    2550 | 7826 | `	return SXRET_OK;` |
|    1276 | 7827 |  |
|       - | 7828 | `/*` |
|       - | 7829 | ` * PHP Language construct table.` |
|       - | 7830 | ` */` |
|       - | 7831 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 7832 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 7833 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 7834 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 7835 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 7836 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 7837 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 7838 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 7839 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 7840 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 7841 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 7842 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 7843 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 7844 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 7845 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 7846 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 7847 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 7848 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 7849 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 7850 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 7851 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 7852 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 7853 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 7854 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 7855 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 7856 | `};` |
|       - | 7857 | `/*` |
|       - | 7858 | ` * Return a pointer to the statement handler routine associated` |
|       - | 7859 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 7860 | ` */` |
|  382040 | 7861 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 7862 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 7863 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 7864 | `	)` |
|       2 | 7865 |  |
|  382042 | 7866 | `	sxu32 n = 0;` |
| 1605442 | 7867 | `	for(;;){` |
| 3210886 | 7868 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   44702 | 7869 | `			break;` |
|       - | 7870 | `		}` |
| 3166186 | 7871 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  337342 | 7872 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 7873 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 7874 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 7875 | `					/* 'static' (class context),return null */` |
|     ! 0 | 7876 | `					return 0;` |
|       - | 7877 | `				}` |
|     ! 0 | 7878 | `			}` |
|       - | 7879 | `			/* Return a pointer to the handler.` |
|       - | 7880 | `			*/` |
|  337342 | 7881 | `			return aLangConstruct[n].xConstruct;` |
|       - | 7882 | `		}` |
| 2828846 | 7883 | `		n++;` |
|       2 | 7884 | `	}` |
|   44702 | 7885 | `	if( pLookahed ){` |
|   44702 | 7886 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    7814 | 7887 | `			return PH7_CompileClassInterface;` |
|   36890 | 7888 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   36686 | 7889 | `			return PH7_CompileClass;` |
|     206 | 7890 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      54 | 7891 | `			return PH7_CompileTrait;` |
|     152 | 7892 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      19 | 7893 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      18 | 7894 | `				return PH7_CompileAbstractClass;` |
|     136 | 7895 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 7896 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 7897 | `				return PH7_CompileFinalClass;` |
|       - | 7898 | `		}` |
|      67 | 7899 | `	}` |
|       - | 7900 | `	/* Not a language construct */` |
|     136 | 7901 | `	return 0;` |
|  191022 | 7902 |  |
|       - | 7903 | `/*` |
|       - | 7904 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 7905 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 7906 | ` */` |
|     134 | 7907 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 7908 |  |
|       - | 7909 | `	int rc;` |
|     136 | 7910 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     136 | 7911 | `	if( rc == FALSE ){` |
|      40 | 7912 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      38 | 7913 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 7914 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 7915 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 7916 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 7917 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 7918 | `			*/` |
|       - | 7919 | `			){` |
|      34 | 7920 | `				rc = TRUE;` |
|      16 | 7921 | `		}` |
|      20 | 7922 | `	}` |
|     136 | 7923 | `	return rc;` |
|       2 | 7924 |  |
|       - | 7925 | `/*` |
|       - | 7926 | ` * Compile a PHP chunk.` |
|       - | 7927 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 7928 | ` * takes care of generating the appropriate error message.` |
|       - | 7929 | ` */` |
|  513212 | 7930 | `static sxi32 GenStateCompileChunk(` |
|       - | 7931 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7932 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 7933 | `	)` |
|       2 | 7934 |  |
|       - | 7935 | `	ProcLangConstruct xCons;` |
|       - | 7936 | `	sxi32 rc;` |
|  513214 | 7937 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  306607 | 7938 | `	for(;;){` |
|  613216 | 7939 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7940 | `			/* No more input to process */` |
|   11288 | 7941 | `			break;` |
|       - | 7942 | `		}` |
|  601930 | 7943 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7944 | `			/* Compile block */` |
|      12 | 7945 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 7946 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7947 | `				break;` |
|       - | 7948 | `			}` |
|       7 | 7949 | `		}else{` |
|  601920 | 7950 | `			xCons = 0;` |
|  601920 | 7951 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  382042 | 7952 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 7953 | `				/* Try to extract a language construct handler */` |
|  382042 | 7954 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  382042 | 7955 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 7956 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7957 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 7958 | `						&pGen->pIn->sData);` |
|       9 | 7959 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7960 | `						break;` |
|       - | 7961 | `					}` |
|       - | 7962 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 7963 | `					 * this erroneous statement.` |
|       - | 7964 | `					 */` |
|       9 | 7965 | `					xCons = PH7_ErrorRecover;` |
|       4 | 7966 | `				}` |
|  410900 | 7967 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   38526 | 7968 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 7969 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 7970 | `				xCons = PH7_CompileLabel;` |
|      56 | 7971 | `			}` |
|  601920 | 7972 | `			if( xCons == 0 ){` |
|       - | 7973 | `				/* Assume an expression an try to compile it */` |
|  219894 | 7974 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  219894 | 7975 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 7976 | `					/* Pop l-value */` |
|  219770 | 7977 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  109884 | 7978 | `				}` |
|  109948 | 7979 | `			}else{` |
|       - | 7980 | `				/* Go compile the sucker */` |
|  382028 | 7981 | `				rc = xCons(&(*pGen));` |
|       - | 7982 | `			}` |
|  601920 | 7983 | `			if( rc == SXERR_ABORT ){` |
|       - | 7984 | `				/* Request to abort compilation */` |
|       3 | 7985 | `				break;` |
|       - | 7986 | `			}` |
|       - | 7987 | `		}` |
|       - | 7988 | `		/* Ignore trailing semi-colons ';' */` |
|  996952 | 7989 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  395026 | 7990 | `			pGen->pIn++;` |
|       2 | 7991 | `		}` |
|  601928 | 7992 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 7993 | `			/* Compile a single statement and return */` |
|  501926 | 7994 | `			break;` |
|       - | 7995 | `		}` |
|       - | 7996 | `		/* LOOP ONE */` |
|       - | 7997 | `		/* LOOP TWO */` |
|       - | 7998 | `		/* LOOP THREE */` |
|       - | 7999 | `		/* LOOP FOUR */` |
|       2 | 8000 | `	}` |
|       - | 8001 | `	/* Return compilation status */` |
|  513214 | 8002 | `	return rc;` |
|       2 | 8003 |  |
|       - | 8004 | `/*` |
|       - | 8005 | ` * Compile a Raw PHP chunk.` |
|       - | 8006 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 8007 | ` * takes care of generating the appropriate error message.` |
|       - | 8008 | ` */` |
|   11290 | 8009 | `static sxi32 PH7_CompilePHP(` |
|       - | 8010 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 8011 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 8012 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 8013 | `	)` |
|       2 | 8014 |  |
|   11292 | 8015 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 8016 | `	sxi32 rc;` |
|       - | 8017 | `	/* Reset the token set */` |
|   11292 | 8018 | `	SySetReset(&(*pTokenSet));` |
|       - | 8019 | `	/* Mark as the default token set */` |
|   11292 | 8020 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 8021 | `	/* Advance the stream cursor */` |
|   11292 | 8022 | `	pGen->pRawIn++;` |
|       - | 8023 | `	/* Tokenize the PHP chunk first */` |
|   11292 | 8024 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 8025 | `	/* Point to the head and tail of the token stream. */` |
|   11292 | 8026 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   11292 | 8027 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   11292 | 8028 | `	if( is_expr ){` |
|     ! 0 | 8029 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 8030 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 8031 | `			/* A simple expression,compile it */` |
|     ! 0 | 8032 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 8033 | `		}` |
|       - | 8034 | `		/* Emit the DONE instruction */` |
|     ! 0 | 8035 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 8036 | `		return SXRET_OK;` |
|       - | 8037 | `	}` |
|   11292 | 8038 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 8039 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 8040 | `		/*` |
|       - | 8041 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 8042 | `		 * According to the PHP reference manual:` |
|       - | 8043 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 8044 | `		 *  immediately follow` |
|       - | 8045 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 8046 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 8047 | `		 * Symisc extension:` |
|       - | 8048 | `		 *   This short syntax works with all PHP opening` |
|       - | 8049 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 8050 | `		 *   only short tag.` |
|       - | 8051 | `		 */` |
|       - | 8052 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 8053 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 8054 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 8055 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 8056 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 8057 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 8058 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 8059 | `		}` |
|       3 | 8060 | `		return SXRET_OK;` |
|       - | 8061 | `	}` |
|       - | 8062 | `	/* Compile the PHP chunk */` |
|   11290 | 8063 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 8064 | `	/* Fix exceptions jumps */` |
|   11290 | 8065 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 8066 | `	/* Fix gotos now, the jump destination is resolved */` |
|   11290 | 8067 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 8068 | `		rc = SXERR_ABORT;` |
|       1 | 8069 | `	}` |
|       - | 8070 | `	/* Reset container */` |
|   11290 | 8071 | `	SySetReset(&pGen->aGoto);` |
|   11290 | 8072 | `	SySetReset(&pGen->aLabel);` |
|       - | 8073 | `	/* Compilation result */` |
|   11290 | 8074 | `	return rc;` |
|    5647 | 8075 |  |
|       - | 8076 | `/*` |
|       - | 8077 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 8078 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 8079 | ` * This is the only compile interface exported from this file.` |
|       - | 8080 | ` */` |
|   13312 | 8081 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 8082 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 8083 | `	SyString *pScript,  /* Script to compile */` |
|       - | 8084 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 8085 | `	)` |
|       2 | 8086 |  |
|       - | 8087 | `	SySet aPhpToken,aRawToken;` |
|       - | 8088 | `	ph7_gen_state *pCodeGen;` |
|       - | 8089 | `	ph7_value *pRawObj;` |
|       - | 8090 | `	sxu32 nObjIdx;` |
|       - | 8091 | `	sxi32 nRawObj;` |
|       - | 8092 | `	int is_expr;` |
|       - | 8093 | `	sxi32 rc;` |
|   13314 | 8094 | `	if( pScript->nByte < 1 ){` |
|       - | 8095 | `		/* Nothing to compile */` |
|     ! 0 | 8096 | `		return PH7_OK;` |
|       - | 8097 | `	}` |
|       - | 8098 | `	/* Initialize the tokens containers */` |
|   13314 | 8099 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13314 | 8100 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13314 | 8101 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   13314 | 8102 | `	is_expr = 0;` |
|   13314 | 8103 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 8104 | `		SyToken sTmp;` |
|       - | 8105 | `		/* PHP only: -*/` |
|    2622 | 8106 | `		sTmp.nLine = 1;` |
|    2622 | 8107 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2622 | 8108 | `		sTmp.pUserData = 0;` |
|    2622 | 8109 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2622 | 8110 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2622 | 8111 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 8112 | `			/* A simple PHP expression */` |
|     ! 0 | 8113 | `			is_expr = 1;` |
|     ! 0 | 8114 | `		}` |
|    1312 | 8115 | `	}else{` |
|       - | 8116 | `		/* Tokenize raw text */` |
|   10694 | 8117 | `		SySetAlloc(&aRawToken,32);` |
|   10694 | 8118 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 8119 | `	}` |
|   13314 | 8120 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 8121 | `	/* Process high-level tokens */` |
|   13314 | 8122 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   13314 | 8123 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   13314 | 8124 | `	rc = PH7_OK;` |
|   13314 | 8125 | `	if( is_expr ){` |
|       - | 8126 | `		/* Compile the expression */` |
|     ! 0 | 8127 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 8128 | `		goto cleanup;` |
|       - | 8129 | `	}` |
|   13314 | 8130 | `	nObjIdx = 0;` |
|       - | 8131 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 8132 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 8133 | `	 * preventing namespace bleeding across include()d files. */` |
|   13314 | 8134 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 8135 | `	/* Start the compilation process */` |
|   12006 | 8136 | `	for(;;){` |
|   35300 | 8137 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   13310 | 8138 | `			break; /* No more tokens to process */` |
|       - | 8139 | `		}` |
|   21992 | 8140 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 8141 | `			/* Compile the PHP chunk */` |
|   11292 | 8142 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   11292 | 8143 | `			if( rc == SXERR_ABORT ){` |
|       5 | 8144 | `				break;` |
|       - | 8145 | `			}` |
|   11288 | 8146 | `			continue;` |
|       - | 8147 | `		}` |
|       - | 8148 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   10702 | 8149 | `		nRawObj = 0;` |
|   21402 | 8150 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 8151 | `			/* Consume the raw chunk without any processing */` |
|   10702 | 8152 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   10702 | 8153 | `			if( pRawObj == 0 ){` |
|     ! 0 | 8154 | `				rc = SXERR_MEM;` |
|     ! 0 | 8155 | `				break;` |
|       - | 8156 | `			}` |
|       - | 8157 | `			/* Mark as constant and emit the load constant instruction */` |
|   10702 | 8158 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   10702 | 8159 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   10702 | 8160 | `			++nRawObj;` |
|   10702 | 8161 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 8162 | `		}` |
|   10702 | 8163 | `		if( nRawObj > 0 ){` |
|       - | 8164 | `			/* Emit the consume instruction */` |
|   10702 | 8165 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5350 | 8166 | `		}` |
|    6658 | 8167 | `	}` |
|    6656 | 8168 | `cleanup:` |
|   13314 | 8169 | `	SySetRelease(&aRawToken);` |
|   13314 | 8170 | `	SySetRelease(&aPhpToken);` |
|   13314 | 8171 | `	return rc;` |
|    6658 | 8172 |  |
|       - | 8173 | `/*` |
|       - | 8174 | ` * Utility routines.Initialize the code generator.` |
|       - | 8175 | ` */` |
|    2592 | 8176 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 8177 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8178 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8179 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8180 | `	)` |
|       2 | 8181 |  |
|    2594 | 8182 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8183 | `	/* Zero the structure */` |
|    2594 | 8184 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 8185 | `	/* Initial state */` |
|    2594 | 8186 | `	pGen->pVm  = &(*pVm);` |
|    2594 | 8187 | `	pGen->xErr = xErr;` |
|    2594 | 8188 | `	pGen->pErrData = pErrData;` |
|    2594 | 8189 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2594 | 8190 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2594 | 8191 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2594 | 8192 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 8193 | `	/* Error log buffer */` |
|    2594 | 8194 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 8195 | `	/* General purpose working buffer */` |
|    2594 | 8196 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 8197 | `	/* Namespace state */` |
|    2594 | 8198 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2594 | 8199 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    2594 | 8200 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    2594 | 8201 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 8202 | `	/* Create the global scope */` |
|    2594 | 8203 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 8204 | `	/* Point to the global scope */` |
|    2594 | 8205 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2594 | 8206 | `	return SXRET_OK;` |
|       2 | 8207 |  |
|       - | 8208 | `/*` |
|       - | 8209 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 8210 | ` */` |
|   15644 | 8211 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 8212 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8213 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8214 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8215 | `	)` |
|       2 | 8216 |  |
|   15646 | 8217 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8218 | `	GenBlock *pBlock,*pParent;` |
|       - | 8219 | `	/* Reset state */` |
|   15646 | 8220 | `	SySetReset(&pGen->aLabel);` |
|   15646 | 8221 | `	SySetReset(&pGen->aGoto);` |
|   15646 | 8222 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   15646 | 8223 | `	SyBlobRelease(&pGen->sWorker);` |
|   15646 | 8224 | `	SyBlobRelease(&pGen->sNamespace);` |
|   15646 | 8225 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   15646 | 8226 | `	SyHashRelease(&pGen->hUseImports);` |
|   15646 | 8227 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   15646 | 8228 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   15646 | 8229 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   15646 | 8230 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   15646 | 8231 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 8232 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 8233 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 8234 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 8235 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 8236 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 8237 | `	 * number of unique names, which is acceptable. */` |
|       - | 8238 | `	/* Point to the global scope */` |
|   15646 | 8239 | `	pBlock = pGen->pCurrent;` |
|   15646 | 8240 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 8241 | `		pParent = pBlock->pParent;` |
|     ! 0 | 8242 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 8243 | `		pBlock = pParent;` |
|     ! 0 | 8244 | `	}` |
|   15646 | 8245 | `	pGen->xErr = xErr;` |
|   15646 | 8246 | `	pGen->pErrData = pErrData;` |
|   15646 | 8247 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   15646 | 8248 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   15646 | 8249 | `	pGen->pIn = pGen->pEnd = 0;` |
|   15646 | 8250 | `	pGen->nErr = 0;` |
|   15646 | 8251 | `	return SXRET_OK;` |
|       2 | 8252 |  |
|       - | 8253 | `/*` |
|       - | 8254 | ` * Generate a compile-time error message.` |
|       - | 8255 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 8256 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 8257 | ` * abort compilation immediately.` |
|       - | 8258 | ` */` |
|     452 | 8259 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 8260 |  |
|     454 | 8261 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     454 | 8262 | `	const char *zErr = "Error";` |
|       - | 8263 | `	SyString *pFile;` |
|       - | 8264 | `	va_list ap;` |
|       - | 8265 | `	sxi32 rc;` |
|       - | 8266 | `	/* Reset the working buffer */` |
|     454 | 8267 | `	SyBlobReset(pWorker);` |
|       - | 8268 | `	/* Peek the processed file path if available */` |
|     454 | 8269 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     454 | 8270 | `	if( nErrType == E_ERROR ){` |
|       - | 8271 | `		/* Increment the error counter */` |
|     412 | 8272 | `		pGen->nErr++;` |
|     412 | 8273 | `		if( pGen->nErr > 15 ){` |
|       - | 8274 | `			/* Error count limit reached */` |
|       5 | 8275 | `			if( pGen->xErr ){` |
|       5 | 8276 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 8277 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 8278 | `				if( pFile ){` |
|       5 | 8279 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 8280 | `				}` |
|       5 | 8281 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 8282 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 8283 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 8284 | `				}` |
|       2 | 8285 | `			}` |
|       - | 8286 | `			/* Abort immediately */` |
|       5 | 8287 | `			return SXERR_ABORT;` |
|       - | 8288 | `		}` |
|     203 | 8289 | `	}` |
|     450 | 8290 | `	if( pGen->xErr == 0 ){` |
|       - | 8291 | `		/* No available error consumer,return immediately */` |
|       3 | 8292 | `		return SXRET_OK;` |
|       - | 8293 | `	}` |
|     447 | 8294 | `	switch(nErrType){` |
|     405 | 8295 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      29 | 8296 | `	case E_WARNING: zErr = "Warning";     break;` |
|       7 | 8297 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 8298 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 8299 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 8300 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 8301 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 8302 | `	default:` |
|     ! 0 | 8303 | `		break;` |
|       - | 8304 | `	}` |
|     447 | 8305 | `	rc = SXRET_OK;` |
|       - | 8306 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     447 | 8307 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     447 | 8308 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     447 | 8309 | `	va_start(ap,zFormat);` |
|     447 | 8310 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     447 | 8311 | `	va_end(ap);` |
|     447 | 8312 | `	if( pFile ){` |
|     447 | 8313 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     223 | 8314 | `	}` |
|       - | 8315 | `	/* Append a new line */` |
|     447 | 8316 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     447 | 8317 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 8318 | `		/* Consume the generated error message */` |
|     447 | 8319 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     223 | 8320 | `	}` |
|     447 | 8321 | `	return rc;` |
|     228 | 8322 |  |
|       - | 8323 |  |
