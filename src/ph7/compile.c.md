# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2644/3572 lines (74.02%)

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
|    3786 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    3788 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    9069 |  131 | `	for(;;){` |
|   18140 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    3676 |  133 | `			iCount--; /* Decrement nesting level */` |
|    3676 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    3654 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      11 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   14488 |  140 | `		pBlock = pBlock->pParent;` |
|   14488 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1895 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  303944 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  303946 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  303946 |  162 | `	pBlock->pUserData   = pUserData;` |
|  303946 |  163 | `	pBlock->pGen        = pGen;` |
|  303946 |  164 | `	pBlock->iFlags      = iType;` |
|  303946 |  165 | `	pBlock->pParent     = 0;` |
|  303946 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  303946 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  303946 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  302170 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  302172 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  302172 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  302172 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  302172 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  302172 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  302172 |  200 | `	pGen->pCurrent = pBlock;` |
|  302172 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  147692 |  203 | `		*ppBlock = pBlock;` |
|   73845 |  204 | `	}` |
|  302172 |  205 | `	return SXRET_OK;` |
|  151087 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  302164 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  302166 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  302166 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  302166 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  302164 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  302166 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  302166 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  302166 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  302166 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  302164 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  302166 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  302166 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  302166 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  302166 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  302166 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  302166 |  244 | `	return SXRET_OK;` |
|  151084 |  245 |  |
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
|  103690 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  103692 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  103692 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  103692 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  103692 |  265 | `	return rc;` |
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
|  230948 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  230950 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  423604 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  192656 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|   77534 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  115124 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   11436 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  103690 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  103690 |  298 | `		if( pInstr ){` |
|  103690 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  103690 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  103690 |  302 | `			aFix[n].nJumpType = -1;` |
|   51844 |  303 | `		}` |
|   51846 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  230950 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|   70246 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|   70248 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|   70394 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|   70246 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|   70378 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|   70246 |  358 | `	return SXRET_OK;` |
|   35125 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  281078 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  281080 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  281080 |  367 | `	if( pEntry == 0 ){` |
|  125788 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  155294 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  155294 |  371 | `	return SXRET_OK;` |
|  140541 |  372 |  |
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
|  125786 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  125788 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  125788 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   62893 |  387 | `	}` |
|  125788 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   58684 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   58686 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   58686 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   58686 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   58686 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   58686 |  408 | `	return pObj;` |
|   29344 |  409 |  |
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
|   59078 |  431 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  432 |  |
|   59080 |  433 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   59080 |  434 | `	sxu32 nIdx = 0;` |
|   59080 |  435 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  436 | `		ph7_value *pObj;` |
|       - |  437 | `		sxi64 iValue;` |
|   58686 |  438 | `		iValue = PH7_TokenValueToInt64(&pToken->sData);` |
|   58686 |  439 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   58686 |  440 | `		if( pObj == 0 ){` |
|     ! 0 |  441 | `			SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  442 | `			return SXERR_ABORT;` |
|       - |  443 | `		}` |
|   58686 |  444 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   29344 |  445 | `	}else{` |
|       - |  446 | `		/* Real number */` |
|       - |  447 | `		ph7_value *pObj;` |
|       - |  448 | `		/* Reserve a new constant */` |
|     395 |  449 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     395 |  450 | `		if( pObj == 0 ){` |
|     ! 0 |  451 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  452 | `			return SXERR_ABORT;` |
|       - |  453 | `		}` |
|     395 |  454 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&pToken->sData);` |
|     395 |  455 | `		PH7_MemObjToReal(pObj);` |
|       - |  456 | `	}` |
|       - |  457 | `	/* Emit the load constant instruction */` |
|   59080 |  458 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  459 | `	/* Node successfully compiled */` |
|   59080 |  460 | `	return SXRET_OK;` |
|   29541 |  461 |  |
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
|   34836 |  473 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  474 |  |
|   34838 |  475 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  476 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  477 | `	ph7_value *pObj;` |
|       - |  478 | `	sxu32 nIdx;` |
|   34838 |  479 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  480 | `	/* Delimit the string */` |
|   34838 |  481 | `	zIn  = pStr->zString;` |
|   34838 |  482 | `	zEnd = &zIn[pStr->nByte];` |
|   34838 |  483 | `	if( zIn >= zEnd ){` |
|       - |  484 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  485 | `		 * rather than reserving a new object each time. */` |
|     108 |  486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     108 |  487 | `		return SXRET_OK;` |
|       - |  488 | `	}` |
|   34732 |  489 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  490 | `		/* Already processed,emit the load constant instruction` |
|       - |  491 | `		 * and return.` |
|       - |  492 | `		 */` |
|   10950 |  493 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   10950 |  494 | `		return SXRET_OK;` |
|       - |  495 | `	}` |
|       - |  496 | `	/* Reserve a new constant */` |
|   23784 |  497 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   23784 |  498 | `	if( pObj == 0 ){` |
|     ! 0 |  499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  500 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  501 | `		return SXERR_ABORT;` |
|       - |  502 | `	}` |
|   23784 |  503 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  504 | `	/* Compile the node */` |
|   23799 |  505 | `	for(;;){` |
|   47600 |  506 | `		if( zIn >= zEnd ){` |
|       - |  507 | `			/* End of input */` |
|   23784 |  508 | `			break;` |
|       - |  509 | `		}` |
|   23818 |  510 | `		zCur = zIn;` |
|  323428 |  511 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  299612 |  512 | `			zIn++;` |
|       2 |  513 | `		}` |
|   23818 |  514 | `		if( zIn > zCur ){` |
|       - |  515 | `			/* Append raw contents*/` |
|   23800 |  516 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   11899 |  517 | `		}` |
|   23818 |  518 | `		zIn++;` |
|   23818 |  519 | `		if( zIn < zEnd ){` |
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
|   23818 |  534 | `		zIn++;` |
|       2 |  535 | `	}` |
|       - |  536 | `	/* Emit the load constant instruction */` |
|   23784 |  537 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   23784 |  538 | `	if( pStr->nByte < 1024 ){` |
|       - |  539 | `		/* Install in the literal table */` |
|   23784 |  540 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   11891 |  541 | `	}` |
|       - |  542 | `	/* Node successfully compiled */` |
|   23784 |  543 | `	return SXRET_OK;` |
|   17420 |  544 |  |
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
|    1472 |  606 | `static sxi32 GenStateProcessStringExpression(` |
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
|    1474 |  617 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  618 | `	/* Preallocate some slots */` |
|    1474 |  619 | `	SySetAlloc(&sToken,0x08);` |
|       - |  620 | `	/* Tokenize the text */` |
|    1474 |  621 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  622 | `	/* Swap delimiter */` |
|    1474 |  623 | `	pTmpIn  = pGen->pIn;` |
|    1474 |  624 | `	pTmpEnd = pGen->pEnd;` |
|    1474 |  625 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1474 |  626 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  627 | `	/* Compile the expression */` |
|    1474 |  628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  629 | `	/* Restore token stream */` |
|    1474 |  630 | `	pGen->pIn  = pTmpIn;` |
|    1474 |  631 | `	pGen->pEnd = pTmpEnd;` |
|       - |  632 | `	/* Release the token set */` |
|    1474 |  633 | `	SySetRelease(&sToken);` |
|       - |  634 | `	/* Compilation result */` |
|    1474 |  635 | `	return rc;` |
|       2 |  636 |  |
|       - |  637 | `/*` |
|       - |  638 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  639 | ` */` |
|   13670 |  640 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  641 |  |
|       - |  642 | `	ph7_value *pConstObj;` |
|   13672 |  643 | `	sxu32 nIdx = 0;` |
|       - |  644 | `	/* Reserve a new constant */` |
|   13672 |  645 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   13672 |  646 | `	if( pConstObj == 0 ){` |
|     ! 0 |  647 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  648 | `		return 0;` |
|       - |  649 | `	}` |
|   13672 |  650 | `	(*pCount)++;` |
|   13672 |  651 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  652 | `	/* Emit the load constant instruction */` |
|   13672 |  653 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   13672 |  654 | `	return pConstObj;` |
|    6837 |  655 |  |
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
|   12582 |  694 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  695 |  |
|   12584 |  696 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  697 | `	const char *zIn,*zCur,*zEnd;` |
|   12584 |  698 | `	ph7_value *pObj = 0;` |
|       - |  699 | `	sxi32 iCons;` |
|       - |  700 | `	sxi32 rc;` |
|       - |  701 | `	/* Delimit the string */` |
|   12584 |  702 | `	zIn  = pStr->zString;` |
|   12584 |  703 | `	zEnd = &zIn[pStr->nByte];` |
|   12584 |  704 | `	if( zIn >= zEnd ){` |
|       - |  705 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  706 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  707 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  708 | `		 */` |
|     216 |  709 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     216 |  710 | `		return SXRET_OK;` |
|       - |  711 | `	}` |
|   12370 |  712 | `	zCur = 0;` |
|       - |  713 | `	/* Compile the node */` |
|   12370 |  714 | `	iCons = 0;` |
|    6920 |  715 | `	for(;;){` |
|   20766 |  716 | `		zCur = zIn;` |
|  125234 |  717 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  105942 |  718 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      44 |  719 | `				break;` |
|  105858 |  720 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1390 |  721 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     695 |  722 | `					break;` |
|       - |  723 | `			}` |
|  104470 |  724 | `			zIn++;` |
|       2 |  725 | `		}` |
|   20766 |  726 | `		if( zIn > zCur ){` |
|   10482 |  727 | `			if( pObj == 0 ){` |
|   10224 |  728 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   10224 |  729 | `				if( pObj == 0 ){` |
|     ! 0 |  730 | `					return SXERR_ABORT;` |
|       - |  731 | `				}` |
|    5111 |  732 | `			}` |
|   10482 |  733 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    5240 |  734 | `		}` |
|   20766 |  735 | `		if( zIn >= zEnd ){` |
|   12370 |  736 | `			break;` |
|       - |  737 | `		}` |
|    8398 |  738 | `		if( zIn[0] == '\\' ){` |
|    6926 |  739 | `			const char *zPtr = 0;` |
|       - |  740 | `			sxu32 n;` |
|    6926 |  741 | `			zIn++;` |
|    6926 |  742 | `			if( zIn >= zEnd ){` |
|     ! 0 |  743 | `				break;` |
|       - |  744 | `			}` |
|    6926 |  745 | `			if( pObj == 0 ){` |
|    3450 |  746 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    3450 |  747 | `				if( pObj == 0 ){` |
|     ! 0 |  748 | `					return SXERR_ABORT;` |
|       - |  749 | `				}` |
|    1724 |  750 | `			}` |
|    6926 |  751 | `			n = sizeof(char); /* size of conversion */` |
|    6926 |  752 | `			switch( zIn[0] ){` |
|       3 |  753 | `			case '$':` |
|       - |  754 | `				/* Dollar sign */` |
|       7 |  755 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       7 |  756 | `				break;` |
|      32 |  757 | `			case '\\':` |
|       - |  758 | `				/* A literal backslash */` |
|      66 |  759 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      66 |  760 | `				break;` |
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
|    3106 |  773 | `			case 'n':` |
|       - |  774 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    6214 |  775 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    6214 |  776 | `				break;` |
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
|    6926 |  844 | `			zIn += n;` |
|    6926 |  845 | `			continue;` |
|       - |  846 | `		}` |
|    1474 |  847 | `		if( zIn[0] == '{' ){` |
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
|    1388 |  881 | `			const char *zExpr = zIn;` |
|       - |  882 | `			/* Assemble variable name */` |
|     693 |  883 | `			for(;;){` |
|       - |  884 | `				/* Jump leading dollars */` |
|    2774 |  885 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1388 |  886 | `					zIn++;` |
|       2 |  887 | `				}` |
|     693 |  888 | `				for(;;){` |
|    9319 |  889 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7240 |  890 | `						zIn++;` |
|       2 |  891 | `					}` |
|    1388 |  892 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  893 | `						/* UTF-8 stream */` |
|     ! 0 |  894 | `						zIn++;` |
|     ! 0 |  895 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  896 | `							zIn++;` |
|     ! 0 |  897 | `						}` |
|     ! 0 |  898 | `						continue;` |
|       - |  899 | `					}` |
|    1388 |  900 | `					break;` |
|     ! 0 |  901 | `				}` |
|    1388 |  902 | `				if( zIn >= zEnd ){` |
|      79 |  903 | `					break;` |
|       - |  904 | `				}` |
|    1310 |  905 | `				if( zIn[0] == '[' ){` |
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
|    1302 |  923 | `				}else if(zIn[0] == '{' ){` |
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
|    1298 |  941 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  942 | `					/* Member access operator '->' */` |
|     ! 0 |  943 | `					zIn += 2;` |
|    1298 |  944 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  945 | `					/* Static member access operator '::' */` |
|     ! 0 |  946 | `					zIn += 2;` |
|     ! 0 |  947 | `				}else{` |
|     650 |  948 | `					break;` |
|       - |  949 | `				}` |
|     ! 0 |  950 | `			}` |
|       - |  951 | `			/* Process the expression */` |
|    1388 |  952 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1388 |  953 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  954 | `				return SXERR_ABORT;` |
|       - |  955 | `			}` |
|    1388 |  956 | `			if( rc != SXERR_EMPTY ){` |
|    1386 |  957 | `				++iCons;` |
|     692 |  958 | `			}` |
|       - |  959 | `		}` |
|       - |  960 | `		/* Invalidate the previously used constant */` |
|    1474 |  961 | `		pObj = 0;` |
|       2 |  962 | `	}/*for(;;)*/` |
|   12370 |  963 | `	if( iCons > 1 ){` |
|       - |  964 | `		/* Concatenate all compiled constants */` |
|    1148 |  965 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     573 |  966 | `	}` |
|       - |  967 | `	/* Node successfully compiled */` |
|   12370 |  968 | `	return SXRET_OK;` |
|    6293 |  969 |  |
|       - |  970 | `/*` |
|       - |  971 | ` * Compile a double quoted string.` |
|       - |  972 | ` *  See the block-comment above for more information.` |
|       - |  973 | ` */` |
|   12556 |  974 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  975 |  |
|       - |  976 | `	sxi32 rc;` |
|   12558 |  977 | `	rc = GenStateCompileString(&(*pGen));` |
|    6278 |  978 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  979 | `	/* Compilation result */` |
|   12558 |  980 | `	return rc;` |
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
|    3924 | 1012 | `static sxi32 GenStateCompileArrayEntry(` |
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
|    3926 | 1023 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1024 | `	/* Compile the expression*/` |
|    3926 | 1025 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1026 | `	/* Restore token stream */` |
|    3926 | 1027 | `	RE_SWAP_DELIMITER(pGen);` |
|    3926 | 1028 | `	return rc;` |
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
|    6814 | 1071 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1072 |  |
|       - | 1073 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1074 | `	SyToken *pKey,*pCur;` |
|    6816 | 1075 | `	sxi32 iEmitRef = 0;` |
|    6816 | 1076 | `	sxi32 nPair = 0;` |
|       - | 1077 | `	sxi32 iNest;` |
|       - | 1078 | `	sxi32 rc;` |
|       - | 1079 | `	/* Jump the 'array' keyword,the leading left parenthesis and the trailing parenthesis.` |
|       - | 1080 | `	 */` |
|    6816 | 1081 | `	pGen->pIn += 2;` |
|    6816 | 1082 | `	pGen->pEnd--;` |
|    6816 | 1083 | `	xValidator = 0;` |
|    3407 | 1084 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|    4855 | 1085 | `	for(;;){` |
|       - | 1086 | `		/* Jump leading commas */` |
|   11412 | 1087 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    1702 | 1088 | `			pGen->pIn++;` |
|       2 | 1089 | `		}` |
|    9712 | 1090 | `		pCur = pGen->pIn;` |
|    9712 | 1091 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1092 | `			/* No more entry to process */` |
|    6804 | 1093 | `			break;` |
|       - | 1094 | `		}` |
|    2910 | 1095 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1096 | `			continue;` |
|       - | 1097 | `		}` |
|       - | 1098 | `		/* Compile the key if available */` |
|    2910 | 1099 | `		pKey = pCur;` |
|    2910 | 1100 | `		iNest = 0;` |
|    6224 | 1101 | `		while( pCur < pGen->pIn ){` |
|    4304 | 1102 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|     990 | 1103 | `				break;` |
|       - | 1104 | `			}` |
|    3316 | 1105 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      58 | 1106 | `				iNest++;` |
|    3288 | 1107 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1108 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1109 | `				 * parser will shortly detect any syntax error.` |
|       - | 1110 | `				 */` |
|      58 | 1111 | `				iNest--;` |
|      28 | 1112 | `			}` |
|    3316 | 1113 | `			pCur++;` |
|       2 | 1114 | `		}` |
|    2910 | 1115 | `		rc = SXERR_EMPTY;` |
|    2910 | 1116 | `		if( pCur < pGen->pIn ){` |
|     990 | 1117 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - | 1118 | `				/* Missing value */` |
|      11 | 1119 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 | 1120 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1121 | `					return SXERR_ABORT;` |
|       - | 1122 | `				}` |
|      11 | 1123 | `				return SXRET_OK;` |
|       - | 1124 | `			}` |
|       - | 1125 | `			/* Compile the expression holding the key */` |
|     980 | 1126 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - | 1127 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|     980 | 1128 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1129 | `				return SXERR_ABORT;` |
|       - | 1130 | `			}` |
|     980 | 1131 | `			pCur++; /* Jump the '=>' operator */` |
|    2411 | 1132 | `		}else if( pKey == pCur ){` |
|       - | 1133 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1134 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1135 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1136 | `		}else{` |
|       - | 1137 | `			/* Reset back the cursor and point to the entry value */` |
|    1922 | 1138 | `			pCur = pKey;` |
|       - | 1139 | `		}` |
|    2900 | 1140 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1141 | `			/* No available key,load NULL */` |
|    1924 | 1142 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|     961 | 1143 | `		}` |
|    2900 | 1144 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|    2898 | 1159 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|    2898 | 1160 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1161 | `			return SXERR_ABORT;` |
|       - | 1162 | `		}` |
|    2898 | 1163 | `		if( iEmitRef ){` |
|       - | 1164 | `			/* Emit the load reference instruction */` |
|      32 | 1165 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1166 | `		}` |
|    2898 | 1167 | `		xValidator = 0;` |
|    2898 | 1168 | `		iEmitRef = 0;` |
|    2898 | 1169 | `		nPair++;` |
|       2 | 1170 | `	}` |
|       - | 1171 | `	/* Emit the load map instruction */` |
|    6804 | 1172 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1173 | `	/* Node successfully compiled */` |
|    6804 | 1174 | `	return SXRET_OK;` |
|    3409 | 1175 |  |
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
|       - | 1252 | `/* Forward declaration */` |
|       - | 1253 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - | 1254 | `/*` |
|       - | 1255 | ` * Compile an annoynmous function or a closure.` |
|       - | 1256 | ` * According to the PHP language reference` |
|       - | 1257 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - | 1258 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - | 1259 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - | 1260 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - | 1261 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - | 1262 | ` *  Example Anonymous function variable assignment example` |
|       - | 1263 | ` * <?php` |
|       - | 1264 | ` * $greet = function($name)` |
|       - | 1265 | ` * {` |
|       - | 1266 | ` *    printf("Hello %s\r\n", $name);` |
|       - | 1267 | ` * };` |
|       - | 1268 | ` * $greet('World');` |
|       - | 1269 | ` * $greet('PHP');` |
|       - | 1270 | ` * ?>` |
|       - | 1271 | ` * Note that the implementation of annoynmous function and closure under` |
|       - | 1272 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - | 1273 | ` */` |
|      92 | 1274 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1275 |  |
|       - | 1276 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - | 1277 | `	char zName[512];         /* Unique lambda name */` |
|       - | 1278 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - | 1279 | `							  * one thread is allowed to compile the script.` |
|       - | 1280 | `						      */` |
|       - | 1281 | `	ph7_value *pObj;` |
|       - | 1282 | `	SyString sName;` |
|       - | 1283 | `	sxu32 nIdx;` |
|       - | 1284 | `	sxu32 nLen;` |
|       - | 1285 | `	sxi32 rc;` |
|       - | 1286 |  |
|      94 | 1287 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|      94 | 1288 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 | 1289 | `		pGen->pIn++;` |
|     ! 0 | 1290 | `	}` |
|       - | 1291 | `	/* Reserve a constant for the lambda */` |
|      94 | 1292 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      94 | 1293 | `	if( pObj == 0 ){` |
|     ! 0 | 1294 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1295 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1296 | `		return SXERR_ABORT;` |
|       - | 1297 | `	}` |
|       - | 1298 | `	/* Generate a unique name */` |
|      94 | 1299 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - | 1300 | `	/* Make sure the generated name is unique */` |
|      94 | 1301 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 1302 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 | 1303 | `	}` |
|      94 | 1304 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|      94 | 1305 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - | 1306 | `	/* Compile the lambda body */` |
|      94 | 1307 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|      94 | 1308 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1309 | `		return SXERR_ABORT;` |
|       - | 1310 | `	}` |
|      94 | 1311 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1312 | `		/* Emit the load closure instruction */` |
|      10 | 1313 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       6 | 1314 | `	}else{` |
|       - | 1315 | `		/* Emit the load constant instruction */` |
|      86 | 1316 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1317 | `	}` |
|       - | 1318 | `	/* Node successfully compiled */` |
|      94 | 1319 | `	return SXRET_OK;` |
|      48 | 1320 |  |
|       - | 1321 | `/*` |
|       - | 1322 | ` * Compile a backtick quoted string.` |
|       - | 1323 | ` */` |
|       4 | 1324 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1325 |  |
|       - | 1326 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - | 1327 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - | 1328 | `	 */` |
|       7 | 1329 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - | 1330 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 | 1331 | `		ph7_lib_version()` |
|       - | 1332 | `		);` |
|       - | 1333 | `	/* Load NULL */` |
|       5 | 1334 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1335 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1336 | `	/* Node successfully compiled */` |
|       5 | 1337 | `	return SXRET_OK;` |
|       1 | 1338 |  |
|       - | 1339 | `/*` |
|       - | 1340 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - | 1341 | ` * construct.` |
|       - | 1342 | ` */` |
|      70 | 1343 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1344 |  |
|       - | 1345 | `	SyString *pName;` |
|       - | 1346 | `	sxu32 nKeyID;` |
|       - | 1347 | `	sxi32 rc;` |
|       - | 1348 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      72 | 1349 | `	pName = &pGen->pIn->sData;` |
|      72 | 1350 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      72 | 1351 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      72 | 1352 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 | 1353 | `		SyToken *pTmp,*pNext = 0;` |
|       - | 1354 | `		/* Compile arguments one after one */` |
|       9 | 1355 | `		pTmp = pGen->pEnd;` |
|       - | 1356 | `		/* Symisc eXtension to the PHP programming language:` |
|       - | 1357 | `		 * 'echo' can be used in the context of a function which` |
|       - | 1358 | `		 *  mean that the following expression is valid:` |
|       - | 1359 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - | 1360 | `		 */` |
|       9 | 1361 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 | 1362 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 | 1363 | `			if( pGen->pIn < pNext ){` |
|       9 | 1364 | `				pGen->pEnd = pNext;` |
|       9 | 1365 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 | 1366 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1367 | `					return SXERR_ABORT;` |
|       - | 1368 | `				}` |
|       9 | 1369 | `				if( rc != SXERR_EMPTY ){` |
|       - | 1370 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - | 1371 | `					 * without the overhead of a function call.` |
|       - | 1372 | `					 * This is a very powerful optimization that improve` |
|       - | 1373 | `					 * performance greatly.` |
|       - | 1374 | `					 */` |
|       9 | 1375 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 | 1376 | `				}` |
|       4 | 1377 | `			}` |
|       - | 1378 | `			/* Jump trailing commas */` |
|       9 | 1379 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 1380 | `				pNext++;` |
|     ! 0 | 1381 | `			}` |
|       9 | 1382 | `			pGen->pIn = pNext;` |
|       1 | 1383 | `		}` |
|       - | 1384 | `		/* Restore token stream */` |
|       9 | 1385 | `		pGen->pEnd = pTmp;` |
|       5 | 1386 | `	}else{` |
|      64 | 1387 | `		sxi32 nArg = 0;` |
|      64 | 1388 | `		sxu32 nIdx = 0;` |
|      64 | 1389 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|      64 | 1390 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1391 | `			return SXERR_ABORT;` |
|      64 | 1392 | `		}else if(rc != SXERR_EMPTY ){` |
|      64 | 1393 | `			nArg = 1;` |
|      31 | 1394 | `		}` |
|      64 | 1395 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1396 | `			ph7_value *pObj;` |
|       - | 1397 | `			/* Emit the call instruction */` |
|      18 | 1398 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      18 | 1399 | `			if( pObj == 0 ){` |
|     ! 0 | 1400 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1401 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1402 | `				return SXERR_ABORT;` |
|       - | 1403 | `			}` |
|      18 | 1404 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1405 | `			/* Install in the literal table */` |
|      18 | 1406 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       8 | 1407 | `		}` |
|       - | 1408 | `		/* Emit the call instruction */` |
|      64 | 1409 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      64 | 1410 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - | 1411 | `	}` |
|       - | 1412 | `	/* Node successfully compiled */` |
|      72 | 1413 | `	return SXRET_OK;` |
|      37 | 1414 |  |
|       - | 1415 | `/*` |
|       - | 1416 | ` * Compile a node holding a variable declaration.` |
|       - | 1417 | ` * According to the PHP language reference` |
|       - | 1418 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 1419 | ` *  The variable name is case-sensitive.` |
|       - | 1420 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 1421 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1422 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 1423 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 1424 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 1425 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 1426 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 1427 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 1428 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 1429 | ` *  the chapter on Expressions.` |
|       - | 1430 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 1431 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 1432 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 1433 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 1434 | ` *  is being assigned (the source variable).` |
|       - | 1435 | ` */` |
|  443234 | 1436 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1437 |  |
|  443236 | 1438 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1439 | `	sxi32 iVv;` |
|       - | 1440 | `	sxi32 iP1;` |
|       - | 1441 | `	void *p3;` |
|       - | 1442 | `	sxi32 rc;` |
|  443236 | 1443 | `	iVv = -1; /* Variable variable counter */` |
|  886482 | 1444 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  443248 | 1445 | `		pGen->pIn++;` |
|  443248 | 1446 | `		iVv++;` |
|       2 | 1447 | `	}` |
|  443236 | 1448 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1449 | `		/* Invalid variable name */` |
|       3 | 1450 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1451 | `		if( rc == SXERR_ABORT ){` |
|       - | 1452 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1453 | `			return SXERR_ABORT;` |
|       - | 1454 | `		}` |
|       3 | 1455 | `		return SXRET_OK;` |
|       - | 1456 | `	}` |
|  443234 | 1457 | `	p3  = 0;` |
|  443234 | 1458 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 1459 | `		/* Dynamic variable creation */` |
|      18 | 1460 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 | 1461 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 | 1462 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1463 | `			/* Empty expression */` |
|       3 | 1464 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1465 | `			return SXRET_OK;` |
|       - | 1466 | `		}` |
|       - | 1467 | `		/* Compile the expression holding the variable name */` |
|      16 | 1468 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 | 1469 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1470 | `			return SXERR_ABORT;` |
|      16 | 1471 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 1472 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 | 1473 | `			return SXRET_OK;` |
|       - | 1474 | `		}` |
|       7 | 1475 | `	}else{` |
|       - | 1476 | `		SyHashEntry *pEntry;` |
|       - | 1477 | `		SyString *pName;` |
|  443218 | 1478 | `		char *zName = 0;` |
|       - | 1479 | `		/* Extract variable name */` |
|  443218 | 1480 | `		pName = &pGen->pIn->sData;` |
|       - | 1481 | `		/* Advance the stream cursor */` |
|  443218 | 1482 | `		pGen->pIn++;` |
|  443218 | 1483 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  443218 | 1484 | `		if( pEntry == 0 ){` |
|       - | 1485 | `			/* Duplicate name */` |
|   67584 | 1486 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   67584 | 1487 | `			if( zName == 0 ){` |
|     ! 0 | 1488 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1489 | `				return SXERR_ABORT;` |
|       - | 1490 | `			}` |
|       - | 1491 | `			/* Install in the hashtable */` |
|   67584 | 1492 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   33793 | 1493 | `		}else{` |
|       - | 1494 | `			/* Name already available */` |
|  375636 | 1495 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1496 | `		}` |
|  443218 | 1497 | `		p3 = (void *)zName;` |
|       - | 1498 | `	}` |
|  443230 | 1499 | `	iP1 = 0;` |
|  443230 | 1500 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  139956 | 1501 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1502 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  139952 | 1503 | `			iP1 = 1;` |
|   69975 | 1504 | `		}` |
|   69977 | 1505 | `	}` |
|       - | 1506 | `	/* Emit the load instruction */` |
|  443230 | 1507 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  443242 | 1508 | `	while( iVv > 0 ){` |
|      13 | 1509 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1510 | `		iVv--;` |
|       1 | 1511 | `	}` |
|       - | 1512 | `	/* Node successfully compiled */` |
|  443230 | 1513 | `	return SXRET_OK;` |
|  221619 | 1514 |  |
|       - | 1515 | `/*` |
|       - | 1516 | ` * Load a literal.` |
|       - | 1517 | ` */` |
|  297194 | 1518 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 1519 |  |
|  297196 | 1520 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1521 | `	ph7_value *pObj;` |
|       - | 1522 | `	SyString *pStr;` |
|       - | 1523 | `	sxu32 nIdx;` |
|       - | 1524 | `	/* Extract token value */` |
|  297196 | 1525 | `	pStr = &pToken->sData;` |
|       - | 1526 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  297196 | 1527 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   59754 | 1528 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1529 | `			/* NULL constant are always indexed at 0 */` |
|   23326 | 1530 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   23326 | 1531 | `			return SXRET_OK;` |
|   36430 | 1532 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1533 | `			/* TRUE constant are always indexed at 1 */` |
|     450 | 1534 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     450 | 1535 | `			return SXRET_OK;` |
|       2 | 1536 | `		}` |
|  284936 | 1537 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   59004 | 1538 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1539 | `			/* FALSE constant are always indexed at 2 */` |
|   23578 | 1540 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   23578 | 1541 | `			return SXRET_OK;` |
|  234629 | 1542 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   41522 | 1543 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1544 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    3552 | 1545 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    3552 | 1546 | `			if( pObj == 0 ){` |
|     ! 0 | 1547 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1548 | `				return SXERR_ABORT;` |
|       - | 1549 | `			}` |
|    3552 | 1550 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1551 | `			/* Emit the load constant instruction */` |
|    3552 | 1552 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    3552 | 1553 | `			return SXRET_OK;` |
|  217009 | 1554 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    5758 | 1555 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  214124 | 1556 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    7630 | 1557 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 | 1558 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 1559 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 | 1560 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - | 1561 | `				/* Point to the upper block */` |
|      11 | 1562 | `				pBlock = pBlock->pParent;` |
|       1 | 1563 | `			}` |
|      11 | 1564 | `			if( pBlock == 0 ){` |
|       - | 1565 | `				/* Called in the global scope,load NULL */` |
|       5 | 1566 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1567 | `			}else{` |
|       - | 1568 | `				/* Extract the target function/method */` |
|       7 | 1569 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 | 1570 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - | 1571 | `					/* Not a class method,Load null */` |
|       3 | 1572 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1573 | `				}else{` |
|       5 | 1574 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 | 1575 | `					if( pObj == 0 ){` |
|     ! 0 | 1576 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1577 | `						return SXERR_ABORT;` |
|       - | 1578 | `					}` |
|       5 | 1579 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - | 1580 | `					/* Emit the load constant instruction */` |
|       5 | 1581 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1582 | `				}` |
|       - | 1583 | `			}` |
|      11 | 1584 | `			return SXRET_OK;` |
|       - | 1585 | `	}` |
|       - | 1586 | `	/* Query literal table */` |
|  246288 | 1587 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 1588 | `		ph7_value *pLitObj;` |
|       - | 1589 | `		/* Unknown literal,install it in the literal table */` |
|  101990 | 1590 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  101990 | 1591 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1592 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1593 | `			return SXERR_ABORT;` |
|       - | 1594 | `		}` |
|  101990 | 1595 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  101990 | 1596 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   50994 | 1597 | `	}` |
|       - | 1598 | `	/* Emit the load constant instruction */` |
|  246288 | 1599 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  246288 | 1600 | `	return SXRET_OK;` |
|  148599 | 1601 |  |
|       - | 1602 | `/*` |
|       - | 1603 | ` * Resolve a namespace path or simply load a literal:` |
|       - | 1604 | ` * As of this version namespace support is disabled. If you need` |
|       - | 1605 | ` * a working version that implement namespace,please contact` |
|       - | 1606 | ` * symisc systems via contact@symisc.net` |
|       - | 1607 | ` */` |
|  297194 | 1608 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 1609 |  |
|  297196 | 1610 | `	int emit = 0;` |
|       - | 1611 | `	sxi32 rc;` |
|  297214 | 1612 | `	while( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 1613 | `		/* Emit a warning */` |
|      19 | 1614 | `		if( !emit ){` |
|       4 | 1615 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 1616 | `				"Namespace support is disabled in the current release of the PH7(%s) engine",` |
|       1 | 1617 | `				ph7_lib_version()` |
|       - | 1618 | `				);` |
|       3 | 1619 | `			emit = 1;` |
|       1 | 1620 | `		}` |
|      19 | 1621 | `		pGen->pIn++; /* Ignore the token */` |
|       1 | 1622 | `	}` |
|       - | 1623 | `	/* Load literal */` |
|  297196 | 1624 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  297196 | 1625 | `	return rc;` |
|       2 | 1626 |  |
|       - | 1627 | `/*` |
|       - | 1628 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1629 | ` */` |
|  297194 | 1630 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1631 |  |
|       - | 1632 | `	sxi32 rc;` |
|  297196 | 1633 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  297196 | 1634 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1635 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1636 | `		return rc;` |
|       - | 1637 | `	}` |
|       - | 1638 | `	/* Node successfully compiled */` |
|  297196 | 1639 | `	return SXRET_OK;` |
|  148599 | 1640 |  |
|       - | 1641 | `/*` |
|       - | 1642 | ` * Recover from a compile-time error. In other words synchronize` |
|       - | 1643 | ` * the token stream cursor with the first semi-colon seen.` |
|       - | 1644 | ` */` |
|       6 | 1645 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 | 1646 |  |
|       - | 1647 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      13 | 1648 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       7 | 1649 | `		pGen->pIn++;` |
|       1 | 1650 | `	}` |
|       7 | 1651 | `	return SXRET_OK;` |
|       1 | 1652 |  |
|       - | 1653 | `/*` |
|       - | 1654 | ` * Check if the given identifier name is reserved or not.` |
|       - | 1655 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - | 1656 | ` */` |
|      30 | 1657 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 | 1658 |  |
|      32 | 1659 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      12 | 1660 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 | 1661 | `			return TRUE;` |
|      10 | 1662 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 | 1663 | `			return TRUE;` |
|       1 | 1664 | `		}` |
|      24 | 1665 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 | 1666 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 | 1667 | `			return TRUE;` |
|       - | 1668 | `		}` |
|     ! 0 | 1669 | `	}` |
|       - | 1670 | `	/* Not a reserved constant */` |
|      24 | 1671 | `	return FALSE;` |
|      17 | 1672 |  |
|       - | 1673 | `/*` |
|       - | 1674 | ` * Compile the 'const' statement.` |
|       - | 1675 | ` * According to the PHP language reference` |
|       - | 1676 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - | 1677 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - | 1678 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - | 1679 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - | 1680 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1681 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - | 1682 | ` *  Syntax` |
|       - | 1683 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - | 1684 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - | 1685 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - | 1686 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - | 1687 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - | 1688 | ` *  to get a list of all defined constants.` |
|       - | 1689 | ` *` |
|       - | 1690 | ` * Symisc eXtension.` |
|       - | 1691 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - | 1692 | ` *  would allow only simple scalar value.` |
|       - | 1693 | ` *  Example` |
|       - | 1694 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 1695 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 1696 | ` */` |
|      26 | 1697 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 | 1698 |  |
|       - | 1699 | `	SySet *pConsCode,*pInstrContainer;` |
|      28 | 1700 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1701 | `	SyString *pName;` |
|       - | 1702 | `	sxi32 rc;` |
|      28 | 1703 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      28 | 1704 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 1705 | `		/* Invalid constant name */` |
|       7 | 1706 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 | 1707 | `		if( rc == SXERR_ABORT ){` |
|       - | 1708 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1709 | `			return SXERR_ABORT;` |
|       - | 1710 | `		}` |
|       7 | 1711 | `		goto Synchronize;` |
|       - | 1712 | `	}` |
|       - | 1713 | `	/* Peek constant name */` |
|      22 | 1714 | `	pName = &pGen->pIn->sData;` |
|       - | 1715 | `	/* Make sure the constant name isn't reserved */` |
|      22 | 1716 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 1717 | `		/* Reserved constant */` |
|       9 | 1718 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 | 1719 | `		if( rc == SXERR_ABORT ){` |
|       - | 1720 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1721 | `			return SXERR_ABORT;` |
|       - | 1722 | `		}` |
|       9 | 1723 | `		goto Synchronize;` |
|       - | 1724 | `	}` |
|      14 | 1725 | `	pGen->pIn++;` |
|      14 | 1726 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 1727 | `		/* Invalid statement*/` |
|       5 | 1728 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 | 1729 | `		if( rc == SXERR_ABORT ){` |
|       - | 1730 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1731 | `			return SXERR_ABORT;` |
|       - | 1732 | `		}` |
|       5 | 1733 | `		goto Synchronize;` |
|       - | 1734 | `	}` |
|       9 | 1735 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - | 1736 | `	/* Allocate a new constant value container */` |
|       9 | 1737 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       9 | 1738 | `	if( pConsCode == 0 ){` |
|     ! 0 | 1739 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1740 | `		return SXERR_ABORT;` |
|       - | 1741 | `	}` |
|       9 | 1742 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 1743 | `	/* Swap bytecode container */` |
|       9 | 1744 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       9 | 1745 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - | 1746 | `	/* Compile constant value */` |
|       9 | 1747 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 1748 | `	/* Emit the done instruction */` |
|       9 | 1749 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       9 | 1750 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       9 | 1751 | `	if( rc == SXERR_ABORT ){` |
|       - | 1752 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1753 | `		return SXERR_ABORT;` |
|       - | 1754 | `	}` |
|       9 | 1755 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - | 1756 | `	/* Register the constant */` |
|       9 | 1757 | `	rc = PH7_VmRegisterConstant(pGen->pVm,pName,PH7_VmExpandConstantValue,pConsCode);` |
|       9 | 1758 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1759 | `		SySetRelease(pConsCode);` |
|     ! 0 | 1760 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 | 1761 | `	}` |
|       9 | 1762 | `	return SXRET_OK;` |
|       9 | 1763 | `Synchronize:` |
|       - | 1764 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 | 1765 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 | 1766 | `		pGen->pIn++;` |
|       1 | 1767 | `	}` |
|      19 | 1768 | `	return SXRET_OK;` |
|      15 | 1769 |  |
|       - | 1770 | `/*` |
|       - | 1771 | ` * Compile the 'continue' statement.` |
|       - | 1772 | ` * According to the PHP language reference` |
|       - | 1773 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - | 1774 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - | 1775 | ` *  iteration.` |
|       - | 1776 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - | 1777 | ` *  the purposes of continue.` |
|       - | 1778 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - | 1779 | ` *  of enclosing loops it should skip to the end of.` |
|       - | 1780 | ` *  Note:` |
|       - | 1781 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - | 1782 | ` */` |
|    3586 | 1783 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 1784 |  |
|       - | 1785 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1786 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1787 | `	sxu32 nLineLocal;` |
|       - | 1788 | `	sxi32 rc;` |
|    3588 | 1789 | `	nLineLocal = pGen->pIn->nLine;` |
|    3588 | 1790 | `	iLevel = 0;` |
|       - | 1791 | `	/* Jump the 'continue' keyword */` |
|    3588 | 1792 | `	pGen->pIn++;` |
|    3588 | 1793 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 1794 | `		/* optional numeric argument which tells us how many levels` |
|       - | 1795 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 1796 | `		 */` |
|      12 | 1797 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      12 | 1798 | `		if( iLevel < 2 ){` |
|     ! 0 | 1799 | `			iLevel = 0;` |
|     ! 0 | 1800 | `		}` |
|      12 | 1801 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 1802 | `	}` |
|       - | 1803 | `	/* Point to the target loop */` |
|    3588 | 1804 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3588 | 1805 | `	if( pLoop == 0 ){` |
|       - | 1806 | `		/* Illegal continue */` |
|      11 | 1807 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 1808 | `		if( rc == SXERR_ABORT ){` |
|       - | 1809 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1810 | `			return SXERR_ABORT;` |
|       - | 1811 | `		}` |
|       6 | 1812 | `	}else{` |
|    3578 | 1813 | `		sxu32 nInstrIdx = 0;` |
|    3578 | 1814 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - | 1815 | `			/* According to the PHP language reference manual` |
|       - | 1816 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - | 1817 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - | 1818 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - | 1819 | `			 */` |
|       5 | 1820 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 | 1821 | `			if( rc == SXRET_OK ){` |
|       5 | 1822 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 | 1823 | `			}` |
|       3 | 1824 | `		}else{` |
|       - | 1825 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3574 | 1826 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3574 | 1827 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 1828 | `				JumpFixup sJumpFix;` |
|       - | 1829 | `				/* Post-continue */` |
|    1782 | 1830 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|    1782 | 1831 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|    1782 | 1832 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|     890 | 1833 | `			}` |
|       - | 1834 | `		}` |
|       - | 1835 | `	}` |
|    3588 | 1836 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1837 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 1838 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 1839 | `	}` |
|       - | 1840 | `	/* Statement successfully compiled */` |
|    3588 | 1841 | `	return SXRET_OK;` |
|    1795 | 1842 |  |
|       - | 1843 | `/*` |
|       - | 1844 | ` * Compile the 'break' statement.` |
|       - | 1845 | ` * According to the PHP language reference` |
|       - | 1846 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - | 1847 | ` *  structure.` |
|       - | 1848 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - | 1849 | ` *  enclosing structures are to be broken out of.` |
|       - | 1850 | ` */` |
|      88 | 1851 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 | 1852 |  |
|       - | 1853 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1854 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1855 | `	sxi32 rc;` |
|      90 | 1856 | `	iLevel = 0;` |
|       - | 1857 | `	/* Jump the 'break' keyword */` |
|      90 | 1858 | `	pGen->pIn++;` |
|      90 | 1859 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 1860 | `		/* optional numeric argument which tells us how many levels` |
|       - | 1861 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 1862 | `		 */` |
|      12 | 1863 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      12 | 1864 | `		if( iLevel < 2 ){` |
|     ! 0 | 1865 | `			iLevel = 0;` |
|     ! 0 | 1866 | `		}` |
|      12 | 1867 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 1868 | `	}` |
|       - | 1869 | `	/* Extract the target loop */` |
|      90 | 1870 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|      90 | 1871 | `	if( pLoop == 0 ){` |
|       - | 1872 | `		/* Illegal break */` |
|      17 | 1873 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 | 1874 | `		if( rc == SXERR_ABORT ){` |
|       - | 1875 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1876 | `			return SXERR_ABORT;` |
|       - | 1877 | `		}` |
|       9 | 1878 | `	}else{` |
|       - | 1879 | `		sxu32 nInstrIdx;` |
|      74 | 1880 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      74 | 1881 | `		if( rc == SXRET_OK ){` |
|       - | 1882 | `			/* Fix the jump later when the jump destination is resolved */` |
|      74 | 1883 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      36 | 1884 | `		}` |
|       - | 1885 | `	}` |
|      90 | 1886 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1887 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 1888 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 | 1889 | `	}` |
|       - | 1890 | `	/* Statement successfully compiled */` |
|      90 | 1891 | `	return SXRET_OK;` |
|      46 | 1892 |  |
|       - | 1893 | `/*` |
|       - | 1894 | ` * Compile or record a label.` |
|       - | 1895 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - | 1896 | ` * Example` |
|       - | 1897 | ` *  goto LABEL;` |
|       - | 1898 | ` *   echo 'Foo';` |
|       - | 1899 | ` *  LABEL:` |
|       - | 1900 | ` *   echo 'Bar';` |
|       - | 1901 | ` */` |
|     112 | 1902 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 | 1903 |  |
|       - | 1904 | `	GenBlock *pBlock;` |
|       - | 1905 | `	Label sLabel;` |
|       - | 1906 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 | 1907 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 | 1908 | `	if( pBlock ){` |
|       - | 1909 | `		sxi32 rc;` |
|       7 | 1910 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 | 1911 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 | 1912 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1913 | `			return SXERR_ABORT;` |
|       - | 1914 | `		}` |
|       3 | 1915 | `	}else{` |
|     110 | 1916 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 1917 | `		char *zDup;` |
|       - | 1918 | `		/* Initialize label fields */` |
|     110 | 1919 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - | 1920 | `		/* Duplicate label name */` |
|     110 | 1921 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 | 1922 | `		if( zDup == 0 ){` |
|     ! 0 | 1923 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 1924 | `			return SXERR_ABORT;` |
|       - | 1925 | `		}` |
|     110 | 1926 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 | 1927 | `		sLabel.bRef  = FALSE;` |
|     110 | 1928 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 | 1929 | `		pBlock = pGen->pCurrent;` |
|     218 | 1930 | `		while( pBlock ){` |
|     130 | 1931 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 | 1932 | `				break;` |
|       - | 1933 | `			}` |
|       - | 1934 | `			/* Point to the upper block */` |
|     110 | 1935 | `			pBlock = pBlock->pParent;` |
|       2 | 1936 | `		}` |
|     110 | 1937 | `		if( pBlock ){` |
|      22 | 1938 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 | 1939 | `		}else{` |
|      90 | 1940 | `			sLabel.pFunc = 0;` |
|       - | 1941 | `		}` |
|       - | 1942 | `		/* Insert in label set */` |
|     110 | 1943 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - | 1944 | `	}` |
|     114 | 1945 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 | 1946 | `	return SXRET_OK;` |
|      58 | 1947 |  |
|       - | 1948 | `/*` |
|       - | 1949 | ` * Compile the so hated 'goto' statement.` |
|       - | 1950 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - | 1951 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - | 1952 | ` * a compiler it has to do this.` |
|       - | 1953 | ` * According to the PHP language reference manual` |
|       - | 1954 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - | 1955 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - | 1956 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - | 1957 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - | 1958 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - | 1959 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - | 1960 | ` *   of a multi-level break` |
|       - | 1961 | ` */` |
|     152 | 1962 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 | 1963 |  |
|       - | 1964 | `	JumpFixup sJump;` |
|       - | 1965 | `	sxi32 rc;` |
|     154 | 1966 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 | 1967 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1968 | `		/* Missing label */` |
|     ! 0 | 1969 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 | 1970 | `		if( rc == SXERR_ABORT ){` |
|       - | 1971 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1972 | `			return SXERR_ABORT;` |
|       - | 1973 | `		}` |
|     ! 0 | 1974 | `		return SXRET_OK;` |
|       - | 1975 | `	}` |
|     154 | 1976 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 | 1977 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 | 1978 | `		if( rc == SXERR_ABORT ){` |
|       - | 1979 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1980 | `			return SXERR_ABORT;` |
|       - | 1981 | `		}` |
|       3 | 1982 | `	}else{` |
|     150 | 1983 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 1984 | `		GenBlock *pBlock;` |
|       - | 1985 | `		char *zDup;` |
|       - | 1986 | `		/* Prepare the jump destination */` |
|     150 | 1987 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 | 1988 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - | 1989 | `		/* Duplicate label name */` |
|     150 | 1990 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 | 1991 | `		if( zDup == 0 ){` |
|     ! 0 | 1992 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 1993 | `			return SXERR_ABORT;` |
|       - | 1994 | `		}` |
|     150 | 1995 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 | 1996 | `		pBlock = pGen->pCurrent;` |
|     312 | 1997 | `		while( pBlock ){` |
|     196 | 1998 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 | 1999 | `				break;` |
|       - | 2000 | `			}` |
|       - | 2001 | `			/* Point to the upper block */` |
|     164 | 2002 | `			pBlock = pBlock->pParent;` |
|       2 | 2003 | `		}` |
|     150 | 2004 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 | 2005 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 | 2006 | `			if( rc == SXERR_ABORT ){` |
|       - | 2007 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2008 | `				return SXERR_ABORT;` |
|       - | 2009 | `			}` |
|       3 | 2010 | `		}` |
|     150 | 2011 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 | 2012 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 | 2013 | `		}else{` |
|     124 | 2014 | `			sJump.pFunc = 0;` |
|       - | 2015 | `		}` |
|       - | 2016 | `		/* Emit the unconditional jump */` |
|     150 | 2017 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 | 2018 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 | 2019 | `		}` |
|       - | 2020 | `	}` |
|     154 | 2021 | `	pGen->pIn++; /* Jump the label name */` |
|     154 | 2022 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 | 2023 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 | 2024 | `	}` |
|       - | 2025 | `	/* Statement successfully compiled */` |
|     154 | 2026 | `	return SXRET_OK;` |
|      78 | 2027 |  |
|       - | 2028 | `/*` |
|       - | 2029 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - | 2030 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - | 2031 | ` * failure.` |
|       - | 2032 | ` */` |
|      20 | 2033 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 | 2034 |  |
|       - | 2035 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - | 2036 | `	sxu32 nRawObj;` |
|      10 | 2037 | `	sxu32 nObjIdx;` |
|       - | 2038 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - | 2039 | `	 * a PHP block.` |
|       - | 2040 | `	 */` |
|      10 | 2041 | `Consume:` |
|      21 | 2042 | `	nRawObj = nObjIdx = 0;` |
|      21 | 2043 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 | 2044 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 | 2045 | `		if( pRawObj == 0 ){` |
|     ! 0 | 2046 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2047 | `			return SXERR_ABORT;` |
|       - | 2048 | `		}` |
|       - | 2049 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 | 2050 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 | 2051 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 | 2052 | `		++nRawObj;` |
|     ! 0 | 2053 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 | 2054 | `	}` |
|      21 | 2055 | `	if( nRawObj > 0 ){` |
|       - | 2056 | `		/* Emit the consume instruction */` |
|     ! 0 | 2057 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 | 2058 | `	}` |
|      21 | 2059 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 | 2060 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - | 2061 | `		/* Reset the token set */` |
|     ! 0 | 2062 | `		SySetReset(pTokenSet);` |
|       - | 2063 | `		/* Tokenize input */` |
|     ! 0 | 2064 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 | 2065 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - | 2066 | `		/* Point to the fresh token stream */` |
|     ! 0 | 2067 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 | 2068 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - | 2069 | `		/* Advance the stream cursor */` |
|     ! 0 | 2070 | `		pGen->pRawIn++;` |
|       - | 2071 | `		/* TICKET 1433-011 */` |
|     ! 0 | 2072 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 2073 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 2074 | `			sxi32 rc;` |
|       - | 2075 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 | 2076 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 | 2077 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 | 2078 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 | 2079 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 2080 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2081 | `				return SXERR_ABORT;` |
|     ! 0 | 2082 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 | 2083 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 2084 | `			}` |
|     ! 0 | 2085 | `			goto Consume;` |
|       - | 2086 | `		}` |
|     ! 0 | 2087 | `	}else{` |
|       - | 2088 | `		/* No more chunks to process */` |
|      21 | 2089 | `		pGen->pIn = pGen->pEnd;` |
|      21 | 2090 | `		return SXERR_EOF;` |
|       - | 2091 | `	}` |
|     ! 0 | 2092 | `	return SXRET_OK;` |
|      11 | 2093 |  |
|       - | 2094 | `/*` |
|       - | 2095 | ` * Compile a PHP block.` |
|       - | 2096 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - | 2097 | ` * optionally delimited by braces {}.` |
|       - | 2098 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 2099 | ` * and this function takes care of generating the appropriate error` |
|       - | 2100 | ` * message.` |
|       - | 2101 | ` */` |
|  155754 | 2102 | `static sxi32 PH7_CompileBlock(` |
|       - | 2103 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2104 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2105 | `	)` |
|       2 | 2106 |  |
|       - | 2107 | `	sxi32 rc;` |
|       - | 2108 | `	sxu32 nLine;` |
|  155756 | 2109 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  154482 | 2110 | `		nLine = pGen->pIn->nLine;` |
|  154482 | 2111 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  154482 | 2112 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2113 | `			return SXERR_ABORT;` |
|       - | 2114 | `		}` |
|  154482 | 2115 | `		pGen->pIn++;` |
|       - | 2116 | `		/* Compile until we hit the closing braces '}' */` |
|  226848 | 2117 | `		for(;;){` |
|  453698 | 2118 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 | 2119 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 | 2120 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2121 | `			 	   return SXERR_ABORT;` |
|       - | 2122 | `				}` |
|      21 | 2123 | `				if( rc == SXERR_EOF ){` |
|       - | 2124 | `					/* No more token to process. Missing closing braces */` |
|      21 | 2125 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 | 2126 | `					break;` |
|       - | 2127 | `				}` |
|     ! 0 | 2128 | `			}` |
|  453678 | 2129 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2130 | `				/* Closing braces found,break immediately*/` |
|  154462 | 2131 | `				pGen->pIn++;` |
|  154462 | 2132 | `				break;` |
|       - | 2133 | `			}` |
|       - | 2134 | `			/* Compile a single statement */` |
|  299218 | 2135 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  299218 | 2136 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2137 | `				return SXERR_ABORT;` |
|       - | 2138 | `			}` |
|       2 | 2139 | `		}` |
|  154482 | 2140 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   78516 | 2141 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 | 2142 | `		pGen->pIn++;` |
|     ! 0 | 2143 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 | 2144 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2145 | `			return SXERR_ABORT;` |
|       - | 2146 | `		}` |
|       - | 2147 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 | 2148 | `		for(;;){` |
|     ! 0 | 2149 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2150 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 | 2151 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2152 | `			 	   return SXERR_ABORT;` |
|       - | 2153 | `				}` |
|     ! 0 | 2154 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - | 2155 | `					/* No more token to process */` |
|     ! 0 | 2156 | `					if( rc == SXERR_EOF ){` |
|     ! 0 | 2157 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - | 2158 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 | 2159 | `					}` |
|     ! 0 | 2160 | `					break;` |
|       - | 2161 | `				}` |
|     ! 0 | 2162 | `			}` |
|     ! 0 | 2163 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 2164 | `				sxi32 nKwrd;` |
|       - | 2165 | `				/* Keyword found */` |
|     ! 0 | 2166 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 | 2167 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 | 2168 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - | 2169 | `						/* Delimiter keyword found,break */` |
|     ! 0 | 2170 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 | 2171 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 | 2172 | `						}` |
|     ! 0 | 2173 | `						break;` |
|       - | 2174 | `				}` |
|     ! 0 | 2175 | `			}` |
|       - | 2176 | `			/* Compile a single statement */` |
|     ! 0 | 2177 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 | 2178 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2179 | `				return SXERR_ABORT;` |
|       - | 2180 | `			}` |
|     ! 0 | 2181 | `		}` |
|     ! 0 | 2182 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 | 2183 | `	}else{` |
|       - | 2184 | `		/* Compile a single statement */` |
|    1276 | 2185 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1276 | 2186 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2187 | `			return SXERR_ABORT;` |
|       - | 2188 | `		}` |
|       - | 2189 | `	}` |
|       - | 2190 | `	/* Jump trailing semi-colons ';' */` |
|  155756 | 2191 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2192 | `		pGen->pIn++;` |
|     ! 0 | 2193 | `	}` |
|  155756 | 2194 | `	return SXRET_OK;` |
|   77879 | 2195 |  |
|       - | 2196 | `/*` |
|       - | 2197 | ` * Compile the gentle 'while' statement.` |
|       - | 2198 | ` * According to the PHP language reference` |
|       - | 2199 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - | 2200 | ` *  The basic form of a while statement is:` |
|       - | 2201 | ` *  while (expr)` |
|       - | 2202 | ` *   statement` |
|       - | 2203 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - | 2204 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - | 2205 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - | 2206 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - | 2207 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - | 2208 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - | 2209 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - | 2210 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - | 2211 | ` *  while (expr):` |
|       - | 2212 | ` *    statement` |
|       - | 2213 | ` *   endwhile;` |
|       - | 2214 | ` */` |
|    7172 | 2215 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 2216 |  |
|    7174 | 2217 | `	GenBlock *pWhileBlock = 0;` |
|    7174 | 2218 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2219 | `	sxu32 nFalseJump;` |
|       - | 2220 | `	sxu32 nLine;` |
|       - | 2221 | `	sxi32 rc;` |
|    7174 | 2222 | `	nLine = pGen->pIn->nLine;` |
|       - | 2223 | `	/* Jump the 'while' keyword */` |
|    7174 | 2224 | `	pGen->pIn++;` |
|    7174 | 2225 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2226 | `		/* Syntax error */` |
|     ! 0 | 2227 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2228 | `		if( rc == SXERR_ABORT ){` |
|       - | 2229 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2230 | `			return SXERR_ABORT;` |
|       - | 2231 | `		}` |
|     ! 0 | 2232 | `		goto Synchronize;` |
|       - | 2233 | `	}` |
|       - | 2234 | `	/* Jump the left parenthesis '(' */` |
|    7174 | 2235 | `	pGen->pIn++;` |
|       - | 2236 | `	/* Create the loop block */` |
|    7174 | 2237 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    7174 | 2238 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2239 | `		return SXERR_ABORT;` |
|       - | 2240 | `	}` |
|       - | 2241 | `	/* Delimit the condition */` |
|    7174 | 2242 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    7174 | 2243 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2244 | `		/* Empty expression */` |
|       3 | 2245 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2246 | `		if( rc == SXERR_ABORT ){` |
|       - | 2247 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2248 | `			return SXERR_ABORT;` |
|       - | 2249 | `		}` |
|       1 | 2250 | `	}` |
|       - | 2251 | `	/* Swap token streams */` |
|    7174 | 2252 | `	pTmp = pGen->pEnd;` |
|    7174 | 2253 | `	pGen->pEnd = pEnd;` |
|       - | 2254 | `	/* Compile the expression */` |
|    7174 | 2255 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    7174 | 2256 | `	if( rc == SXERR_ABORT ){` |
|       - | 2257 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2258 | `		return SXERR_ABORT;` |
|       - | 2259 | `	}` |
|       - | 2260 | `	/* Update token stream */` |
|    7174 | 2261 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2262 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2263 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2264 | `			return SXERR_ABORT;` |
|       - | 2265 | `		}` |
|     ! 0 | 2266 | `		pGen->pIn++;` |
|     ! 0 | 2267 | `	}` |
|       - | 2268 | `	/* Synchronize pointers */` |
|    7174 | 2269 | `	pGen->pIn  = &pEnd[1];` |
|    7174 | 2270 | `	pGen->pEnd = pTmp;` |
|       - | 2271 | `	/* Emit the false jump */` |
|    7174 | 2272 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2273 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    7174 | 2274 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2275 | `	/* Compile the loop body */` |
|    7174 | 2276 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    7174 | 2277 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2278 | `		return SXERR_ABORT;` |
|       - | 2279 | `	}` |
|       - | 2280 | `	/* Emit the unconditional jump to the start of the loop */` |
|    7174 | 2281 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2282 | `	/* Fix all jumps now the destination is resolved */` |
|    7174 | 2283 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2284 | `	/* Release the loop block */` |
|    7174 | 2285 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2286 | `	/* Statement successfully compiled */` |
|    7174 | 2287 | `	return SXRET_OK;` |
|     ! 0 | 2288 | `Synchronize:` |
|       - | 2289 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2290 | `	 * compiling this erroneous block.` |
|       - | 2291 | `	 */` |
|     ! 0 | 2292 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2293 | `		pGen->pIn++;` |
|     ! 0 | 2294 | `	}` |
|     ! 0 | 2295 | `	return SXRET_OK;` |
|    3588 | 2296 |  |
|       - | 2297 | `/*` |
|       - | 2298 | ` * Compile the ugly do..while() statement.` |
|       - | 2299 | ` * According to the PHP language reference` |
|       - | 2300 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - | 2301 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - | 2302 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - | 2303 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - | 2304 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - | 2305 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - | 2306 | ` *  would end immediately).` |
|       - | 2307 | ` *  There is just one syntax for do-while loops:` |
|       - | 2308 | ` *  <?php` |
|       - | 2309 | ` *  $i = 0;` |
|       - | 2310 | ` *  do {` |
|       - | 2311 | ` *   echo $i;` |
|       - | 2312 | ` *  } while ($i > 0);` |
|       - | 2313 | ` * ?>` |
|       - | 2314 | ` */` |
|       2 | 2315 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 | 2316 |  |
|       3 | 2317 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 | 2318 | `	GenBlock *pDoBlock = 0;` |
|       - | 2319 | `	sxu32 nLine;` |
|       - | 2320 | `	sxi32 rc;` |
|       3 | 2321 | `	nLine = pGen->pIn->nLine;` |
|       - | 2322 | `	/* Jump the 'do' keyword */` |
|       3 | 2323 | `	pGen->pIn++;` |
|       - | 2324 | `	/* Create the loop block */` |
|       3 | 2325 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 | 2326 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2327 | `		return SXERR_ABORT;` |
|       - | 2328 | `	}` |
|       - | 2329 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 | 2330 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 | 2331 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 | 2332 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2333 | `		return SXERR_ABORT;` |
|       - | 2334 | `	}` |
|       3 | 2335 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2336 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 | 2337 | `	}` |
|       3 | 2338 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 | 2339 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - | 2340 | `			/* Missing 'while' statement */` |
|       3 | 2341 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 | 2342 | `			if( rc == SXERR_ABORT ){` |
|       - | 2343 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2344 | `				return SXERR_ABORT;` |
|       - | 2345 | `			}` |
|       3 | 2346 | `			goto Synchronize;` |
|       - | 2347 | `	}` |
|       - | 2348 | `	/* Jump the 'while' keyword */` |
|     ! 0 | 2349 | `	pGen->pIn++;` |
|     ! 0 | 2350 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2351 | `		/* Syntax error */` |
|     ! 0 | 2352 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2353 | `		if( rc == SXERR_ABORT ){` |
|       - | 2354 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2355 | `			return SXERR_ABORT;` |
|       - | 2356 | `		}` |
|     ! 0 | 2357 | `		goto Synchronize;` |
|       - | 2358 | `	}` |
|       - | 2359 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 | 2360 | `	pGen->pIn++;` |
|       - | 2361 | `	/* Delimit the condition */` |
|     ! 0 | 2362 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 | 2363 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2364 | `		/* Empty expression */` |
|     ! 0 | 2365 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 | 2366 | `		if( rc == SXERR_ABORT ){` |
|       - | 2367 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2368 | `			return SXERR_ABORT;` |
|       - | 2369 | `		}` |
|     ! 0 | 2370 | `		goto Synchronize;` |
|       - | 2371 | `	}` |
|       - | 2372 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 | 2373 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - | 2374 | `		JumpFixup *aPost;` |
|       - | 2375 | `		VmInstr *pInstr;` |
|       - | 2376 | `		sxu32 nJumpDest;` |
|       - | 2377 | `		sxu32 n;` |
|     ! 0 | 2378 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 | 2379 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 | 2380 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 | 2381 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 | 2382 | `			if( pInstr ){` |
|       - | 2383 | `				/* Fix */` |
|     ! 0 | 2384 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 | 2385 | `			}` |
|     ! 0 | 2386 | `		}` |
|     ! 0 | 2387 | `	}` |
|       - | 2388 | `	/* Swap token streams */` |
|     ! 0 | 2389 | `	pTmp = pGen->pEnd;` |
|     ! 0 | 2390 | `	pGen->pEnd = pEnd;` |
|       - | 2391 | `	/* Compile the expression */` |
|     ! 0 | 2392 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 2393 | `	if( rc == SXERR_ABORT ){` |
|       - | 2394 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2395 | `		return SXERR_ABORT;` |
|       - | 2396 | `	}` |
|       - | 2397 | `	/* Update token stream */` |
|     ! 0 | 2398 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2399 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2400 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2401 | `			return SXERR_ABORT;` |
|       - | 2402 | `		}` |
|     ! 0 | 2403 | `		pGen->pIn++;` |
|     ! 0 | 2404 | `	}` |
|     ! 0 | 2405 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 | 2406 | `	pGen->pEnd = pTmp;` |
|       - | 2407 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 | 2408 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - | 2409 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 | 2410 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2411 | `	/* Release the loop block */` |
|     ! 0 | 2412 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2413 | `	/* Statement successfully compiled */` |
|     ! 0 | 2414 | `	return SXRET_OK;` |
|       1 | 2415 | `Synchronize:` |
|       - | 2416 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2417 | `	 * compiling this erroneous block.` |
|       - | 2418 | `	 */` |
|       3 | 2419 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2420 | `		pGen->pIn++;` |
|     ! 0 | 2421 | `	}` |
|       3 | 2422 | `	return SXRET_OK;` |
|       2 | 2423 |  |
|       - | 2424 | `/*` |
|       - | 2425 | ` * Compile the complex and powerful 'for' statement.` |
|       - | 2426 | ` * According to the PHP language reference` |
|       - | 2427 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - | 2428 | ` *  The syntax of a for loop is:` |
|       - | 2429 | ` *  for (expr1; expr2; expr3)` |
|       - | 2430 | ` *   statement` |
|       - | 2431 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - | 2432 | ` *  the beginning of the loop.` |
|       - | 2433 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - | 2434 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - | 2435 | ` *  to FALSE, the execution of the loop ends.` |
|       - | 2436 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - | 2437 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - | 2438 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - | 2439 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - | 2440 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - | 2441 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - | 2442 | ` *  of using the for truth expression.` |
|       - | 2443 | ` */` |
|    7174 | 2444 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 2445 |  |
|    7176 | 2446 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    7176 | 2447 | `	GenBlock *pForBlock = 0;` |
|       - | 2448 | `	sxu32 nFalseJump;` |
|       - | 2449 | `	sxu32 nLine;` |
|       - | 2450 | `	sxi32 rc;` |
|    7176 | 2451 | `	nLine = pGen->pIn->nLine;` |
|       - | 2452 | `	/* Jump the 'for' keyword */` |
|    7176 | 2453 | `	pGen->pIn++;` |
|    7176 | 2454 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2455 | `		/* Syntax error */` |
|     ! 0 | 2456 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2457 | `		if( rc == SXERR_ABORT ){` |
|       - | 2458 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2459 | `			return SXERR_ABORT;` |
|       - | 2460 | `		}` |
|     ! 0 | 2461 | `		return SXRET_OK;` |
|       - | 2462 | `	}` |
|       - | 2463 | `	/* Jump the left parenthesis '(' */` |
|    7176 | 2464 | `	pGen->pIn++;` |
|       - | 2465 | `	/* Delimit the init-expr;condition;post-expr */` |
|    7176 | 2466 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    7176 | 2467 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2468 | `		/* Empty expression */` |
|     ! 0 | 2469 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 | 2470 | `		if( rc == SXERR_ABORT ){` |
|       - | 2471 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2472 | `			return SXERR_ABORT;` |
|       - | 2473 | `		}` |
|       - | 2474 | `		/* Synchronize */` |
|     ! 0 | 2475 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2476 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2477 | `			pGen->pIn++;` |
|     ! 0 | 2478 | `		}` |
|     ! 0 | 2479 | `		return SXRET_OK;` |
|       - | 2480 | `	}` |
|       - | 2481 | `	/* Swap token streams */` |
|    7176 | 2482 | `	pTmp = pGen->pEnd;` |
|    7176 | 2483 | `	pGen->pEnd = pEnd;` |
|       - | 2484 | `	/* Compile initialization expressions if available */` |
|    7176 | 2485 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2486 | `	/* Pop operand lvalues */` |
|    7176 | 2487 | `	if( rc == SXERR_ABORT ){` |
|       - | 2488 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2489 | `		return SXERR_ABORT;` |
|    7176 | 2490 | `	}else if( rc != SXERR_EMPTY ){` |
|    7174 | 2491 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3586 | 2492 | `	}` |
|    7176 | 2493 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2494 | `		/* Syntax error */` |
|     ! 0 | 2495 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2496 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 | 2497 | `		if( rc == SXERR_ABORT ){` |
|       - | 2498 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2499 | `			return SXERR_ABORT;` |
|       - | 2500 | `		}` |
|     ! 0 | 2501 | `		return SXRET_OK;` |
|       - | 2502 | `	}` |
|       - | 2503 | `	/* Jump the trailing ';' */` |
|    7176 | 2504 | `	pGen->pIn++;` |
|       - | 2505 | `	/* Create the loop block */` |
|    7176 | 2506 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    7176 | 2507 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2508 | `		return SXERR_ABORT;` |
|       - | 2509 | `	}` |
|       - | 2510 | `	/* Deffer continue jumps */` |
|    7176 | 2511 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 2512 | `	/* Compile the condition */` |
|    7176 | 2513 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    7176 | 2514 | `	if( rc == SXERR_ABORT ){` |
|       - | 2515 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2516 | `		return SXERR_ABORT;` |
|    7176 | 2517 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 2518 | `		/* Emit the false jump */` |
|    7174 | 2519 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2520 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    7174 | 2521 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    3586 | 2522 | `	}` |
|    7176 | 2523 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2524 | `		/* Syntax error */` |
|       5 | 2525 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2526 | `			"for: Expected ';' after conditionals expressions");` |
|       5 | 2527 | `		if( rc == SXERR_ABORT ){` |
|       - | 2528 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2529 | `			return SXERR_ABORT;` |
|       - | 2530 | `		}` |
|       5 | 2531 | `		return SXRET_OK;` |
|       - | 2532 | `	}` |
|       - | 2533 | `	/* Jump the trailing ';' */` |
|    7172 | 2534 | `	pGen->pIn++;` |
|       - | 2535 | `	/* Save the post condition stream */` |
|    7172 | 2536 | `	pPostStart = pGen->pIn;` |
|       - | 2537 | `	/* Compile the loop body */` |
|    7172 | 2538 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    7172 | 2539 | `	pGen->pEnd = pTmp;` |
|    7172 | 2540 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    7172 | 2541 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2542 | `		return SXERR_ABORT;` |
|       - | 2543 | `	}` |
|       - | 2544 | `	/* Fix post-continue jumps */` |
|    7172 | 2545 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 2546 | `		JumpFixup *aPost;` |
|       - | 2547 | `		VmInstr *pInstr;` |
|       - | 2548 | `		sxu32 nJumpDest;` |
|       - | 2549 | `		sxu32 n;` |
|    1782 | 2550 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|    1782 | 2551 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|    3562 | 2552 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|    1782 | 2553 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|    1782 | 2554 | `			if( pInstr ){` |
|       - | 2555 | `				/* Fix jump */` |
|    1782 | 2556 | `				pInstr->iP2 = nJumpDest;` |
|     890 | 2557 | `			}` |
|     892 | 2558 | `		}` |
|     890 | 2559 | `	}` |
|       - | 2560 | `	/* compile the post-expressions if available */` |
|    7172 | 2561 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2562 | `		pPostStart++;` |
|     ! 0 | 2563 | `	}` |
|    7172 | 2564 | `	if( pPostStart < pEnd ){` |
|       - | 2565 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    7172 | 2566 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    7172 | 2567 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    7172 | 2568 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 2569 | `			/* Syntax error */` |
|     ! 0 | 2570 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 2571 | `			if( rc == SXERR_ABORT ){` |
|       - | 2572 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2573 | `				return SXERR_ABORT;` |
|       - | 2574 | `			}` |
|     ! 0 | 2575 | `			return SXRET_OK;` |
|       - | 2576 | `		}` |
|    7172 | 2577 | `		RE_SWAP_DELIMITER(pGen);` |
|    7172 | 2578 | `		if( rc == SXERR_ABORT ){` |
|       - | 2579 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2580 | `			return SXERR_ABORT;` |
|    7172 | 2581 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 2582 | `			/* Pop operand lvalue */` |
|    7172 | 2583 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3585 | 2584 | `		}` |
|    3585 | 2585 | `	}` |
|       - | 2586 | `	/* Emit the unconditional jump to the start of the loop */` |
|    7172 | 2587 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 2588 | `	/* Fix all jumps now the destination is resolved */` |
|    7172 | 2589 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2590 | `	/* Release the loop block */` |
|    7172 | 2591 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2592 | `	/* Statement successfully compiled */` |
|    7172 | 2593 | `	return SXRET_OK;` |
|    3589 | 2594 |  |
|       - | 2595 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 2596 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 2597 | ` * are allowed.` |
|       - | 2598 | ` */` |
|    3808 | 2599 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 2600 |  |
|    3810 | 2601 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    3810 | 2602 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 2603 | `		/* Unexpected expression */` |
|     ! 0 | 2604 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 2605 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 2606 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 2607 | `			rc = SXERR_INVALID;` |
|     ! 0 | 2608 | `		}` |
|     ! 0 | 2609 | `	}` |
|    3810 | 2610 | `	return rc;` |
|       2 | 2611 |  |
|       - | 2612 | `/*` |
|       - | 2613 | ` * Compile the 'foreach' statement.` |
|       - | 2614 | ` * According to the PHP language reference` |
|       - | 2615 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - | 2616 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - | 2617 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - | 2618 | ` *  is a minor but useful extension of the first:` |
|       - | 2619 | ` *  foreach (array_expression as $value)` |
|       - | 2620 | ` *    statement` |
|       - | 2621 | ` *  foreach (array_expression as $key => $value)` |
|       - | 2622 | ` *   statement` |
|       - | 2623 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - | 2624 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - | 2625 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - | 2626 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - | 2627 | ` *  to the variable $key on each loop.` |
|       - | 2628 | ` *  Note:` |
|       - | 2629 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - | 2630 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - | 2631 | ` *  Note:` |
|       - | 2632 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - | 2633 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - | 2634 | ` *  or after the foreach without resetting it.` |
|       - | 2635 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - | 2636 | ` *  of copying the value.` |
|       - | 2637 | ` */` |
|    1926 | 2638 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 2639 |  |
|    1928 | 2640 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    1928 | 2641 | `	GenBlock *pForeachBlock = 0;` |
|       - | 2642 | `	ph7_foreach_info *pInfo;` |
|       - | 2643 | `	sxu32 nFalseJump;` |
|       - | 2644 | `	VmInstr *pInstr;` |
|       - | 2645 | `	sxu32 nLine;` |
|       - | 2646 | `	sxi32 rc;` |
|    1928 | 2647 | `	nLine = pGen->pIn->nLine;` |
|       - | 2648 | `	/* Jump the 'foreach' keyword */` |
|    1928 | 2649 | `	pGen->pIn++;` |
|    1928 | 2650 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2651 | `		/* Syntax error */` |
|     ! 0 | 2652 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 2653 | `		if( rc == SXERR_ABORT ){` |
|       - | 2654 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2655 | `			return SXERR_ABORT;` |
|       - | 2656 | `		}` |
|     ! 0 | 2657 | `		goto Synchronize;` |
|       - | 2658 | `	}` |
|       - | 2659 | `	/* Jump the left parenthesis '(' */` |
|    1928 | 2660 | `	pGen->pIn++;` |
|       - | 2661 | `	/* Create the loop block */` |
|    1928 | 2662 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    1928 | 2663 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2664 | `		return SXERR_ABORT;` |
|       - | 2665 | `	}` |
|       - | 2666 | `	/* Delimit the expression */` |
|    1928 | 2667 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    1928 | 2668 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2669 | `		/* Empty expression */` |
|     ! 0 | 2670 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 2671 | `		if( rc == SXERR_ABORT ){` |
|       - | 2672 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2673 | `			return SXERR_ABORT;` |
|       - | 2674 | `		}` |
|       - | 2675 | `		/* Synchronize */` |
|     ! 0 | 2676 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2677 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2678 | `			pGen->pIn++;` |
|     ! 0 | 2679 | `		}` |
|     ! 0 | 2680 | `		return SXRET_OK;` |
|       - | 2681 | `	}` |
|       - | 2682 | `	/* Compile the array expression */` |
|    1928 | 2683 | `	pCur = pGen->pIn;` |
|   12908 | 2684 | `	while( pCur < pEnd ){` |
|   12908 | 2685 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    1938 | 2686 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    1938 | 2687 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 2688 | `				/* Break with the first 'as' found */` |
|    1928 | 2689 | `				break;` |
|       - | 2690 | `			}` |
|       5 | 2691 | `		}` |
|       - | 2692 | `		/* Advance the stream cursor */` |
|   10982 | 2693 | `		pCur++;` |
|       2 | 2694 | `	}` |
|    1928 | 2695 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 2696 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2697 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 2698 | `		if( rc == SXERR_ABORT ){` |
|       - | 2699 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2700 | `			return SXERR_ABORT;` |
|       - | 2701 | `		}` |
|     ! 0 | 2702 | `		goto Synchronize;` |
|       - | 2703 | `	}` |
|       - | 2704 | `	/* Swap token streams */` |
|    1928 | 2705 | `	pTmp = pGen->pEnd;` |
|    1928 | 2706 | `	pGen->pEnd = pCur;` |
|    1928 | 2707 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    1928 | 2708 | `	if( rc == SXERR_ABORT ){` |
|       - | 2709 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2710 | `		return SXERR_ABORT;` |
|       - | 2711 | `	}` |
|       - | 2712 | `	/* Update token stream */` |
|    1928 | 2713 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 2714 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2715 | `		if( rc == SXERR_ABORT ){` |
|       - | 2716 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2717 | `			return SXERR_ABORT;` |
|       - | 2718 | `		}` |
|     ! 0 | 2719 | `		pGen->pIn++;` |
|     ! 0 | 2720 | `	}` |
|    1928 | 2721 | `	pCur++; /* Jump the 'as' keyword */` |
|    1928 | 2722 | `	pGen->pIn = pCur;` |
|    1928 | 2723 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2724 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 2725 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2726 | `			return SXERR_ABORT;` |
|       - | 2727 | `		}` |
|     ! 0 | 2728 | `	}` |
|       - | 2729 | `	/* Create the foreach context */` |
|    1928 | 2730 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    1928 | 2731 | `	if( pInfo == 0 ){` |
|     ! 0 | 2732 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 2733 | `		return SXERR_ABORT;` |
|       - | 2734 | `	}` |
|       - | 2735 | `	/* Zero the structure */` |
|    1928 | 2736 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 2737 | `	/* Initialize structure fields */` |
|    1928 | 2738 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 2739 | `	/* Check if we have a key field */` |
|    5782 | 2740 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    3856 | 2741 | `		pCur++;` |
|       2 | 2742 | `	}` |
|    1928 | 2743 | `	if( pCur < pEnd ){` |
|       - | 2744 | `		/* Compile the expression holding the key name */` |
|    1884 | 2745 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 2746 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 2747 | `			if( rc == SXERR_ABORT ){` |
|       - | 2748 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2749 | `				return SXERR_ABORT;` |
|       - | 2750 | `			}` |
|     ! 0 | 2751 | `		}else{` |
|    1884 | 2752 | `			pGen->pEnd = pCur;` |
|    1884 | 2753 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    1884 | 2754 | `			if( rc == SXERR_ABORT ){` |
|       - | 2755 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2756 | `				return SXERR_ABORT;` |
|       - | 2757 | `			}` |
|    1884 | 2758 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    1884 | 2759 | `			if( pInstr->p3 ){` |
|       - | 2760 | `				/* Record key name */` |
|    1884 | 2761 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     941 | 2762 | `			}` |
|    1884 | 2763 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 2764 | `		}` |
|    1884 | 2765 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|     941 | 2766 | `	}` |
|    1928 | 2767 | `	pGen->pEnd = pEnd;` |
|    1928 | 2768 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2769 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 2770 | `		if( rc == SXERR_ABORT ){` |
|       - | 2771 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2772 | `			return SXERR_ABORT;` |
|       - | 2773 | `		}` |
|     ! 0 | 2774 | `		goto Synchronize;` |
|       - | 2775 | `	}` |
|    1928 | 2776 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       7 | 2777 | `		pGen->pIn++;` |
|       - | 2778 | `		/* Pass by reference  */` |
|       7 | 2779 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       3 | 2780 | `	}` |
|       - | 2781 | `	/* Compile the expression holding the value name */` |
|    1928 | 2782 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    1928 | 2783 | `	if( rc == SXERR_ABORT ){` |
|       - | 2784 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2785 | `		return SXERR_ABORT;` |
|       - | 2786 | `	}` |
|    1928 | 2787 | `	pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    1928 | 2788 | `	if( pInstr->p3 ){` |
|       - | 2789 | `		/* Record value name */` |
|    1928 | 2790 | `		SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     963 | 2791 | `	}` |
|       - | 2792 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    1928 | 2793 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 2794 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    1928 | 2795 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 2796 | `	/* Record the first instruction to execute */` |
|    1928 | 2797 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2798 | `	/* Emit the FOREACH_STEP instruction */` |
|    1928 | 2799 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 2800 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    1928 | 2801 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 2802 | `	/* Compile the loop body */` |
|    1928 | 2803 | `	pGen->pIn = &pEnd[1];` |
|    1928 | 2804 | `	pGen->pEnd = pTmp;` |
|    1928 | 2805 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    1928 | 2806 | `	if( rc == SXERR_ABORT ){` |
|       - | 2807 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2808 | `		return SXERR_ABORT;` |
|       - | 2809 | `	}` |
|       - | 2810 | `	/* Emit the unconditional jump to the start of the loop */` |
|    1928 | 2811 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 2812 | `	/* Fix all jumps now the destination is resolved */` |
|    1928 | 2813 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2814 | `	/* Release the loop block */` |
|    1928 | 2815 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2816 | `	/* Statement successfully compiled */` |
|    1928 | 2817 | `	return SXRET_OK;` |
|     ! 0 | 2818 | `Synchronize:` |
|       - | 2819 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2820 | `	 * compiling this erroneous block.` |
|       - | 2821 | `	 */` |
|     ! 0 | 2822 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2823 | `		pGen->pIn++;` |
|     ! 0 | 2824 | `	}` |
|     ! 0 | 2825 | `	return SXRET_OK;` |
|     965 | 2826 |  |
|       - | 2827 | `/*` |
|       - | 2828 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 2829 | ` * According to the PHP language reference` |
|       - | 2830 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 2831 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 2832 | ` *  that is similar to that of C:` |
|       - | 2833 | ` *  if (expr)` |
|       - | 2834 | ` *   statement` |
|       - | 2835 | ` *  else construct:` |
|       - | 2836 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 2837 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 2838 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 2839 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 2840 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 2841 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 2842 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 2843 | ` *  elseif` |
|       - | 2844 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 2845 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 2846 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 2847 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 2848 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 2849 | ` *   <?php` |
|       - | 2850 | ` *    if ($a > $b) {` |
|       - | 2851 | ` *     echo "a is bigger than b";` |
|       - | 2852 | ` *    } elseif ($a == $b) {` |
|       - | 2853 | ` *     echo "a is equal to b";` |
|       - | 2854 | ` *    } else {` |
|       - | 2855 | ` *     echo "a is smaller than b";` |
|       - | 2856 | ` *    }` |
|       - | 2857 | ` *    ?>` |
|       - | 2858 | ` */` |
|   70392 | 2859 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 2860 |  |
|   70394 | 2861 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   70394 | 2862 | `	GenBlock *pCondBlock = 0;` |
|       - | 2863 | `	sxu32 nJumpIdx;` |
|       - | 2864 | `	sxu32 nKeyID;` |
|       - | 2865 | `	sxi32 rc;` |
|       - | 2866 | `	/* Jump the 'if' keyword */` |
|   70394 | 2867 | `	pGen->pIn++;` |
|   70394 | 2868 | `	pToken = pGen->pIn;` |
|       - | 2869 | `	/* Create the conditional block */` |
|   70394 | 2870 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   70394 | 2871 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2872 | `		return SXERR_ABORT;` |
|       - | 2873 | `	}` |
|       - | 2874 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   36980 | 2875 | `	for(;;){` |
|   73962 | 2876 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2877 | `			/* Syntax error */` |
|     ! 0 | 2878 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 2879 | `				pToken--;` |
|     ! 0 | 2880 | `			}` |
|     ! 0 | 2881 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 2882 | `			if( rc == SXERR_ABORT ){` |
|       - | 2883 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2884 | `				return SXERR_ABORT;` |
|       - | 2885 | `			}` |
|     ! 0 | 2886 | `			goto Synchronize;` |
|       - | 2887 | `		}` |
|       - | 2888 | `		/* Jump the left parenthesis '(' */` |
|   73962 | 2889 | `		pToken++;` |
|       - | 2890 | `		/* Delimit the condition */` |
|   73962 | 2891 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   73962 | 2892 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 2893 | `			/* Syntax error */` |
|     ! 0 | 2894 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 2895 | `				pToken--;` |
|     ! 0 | 2896 | `			}` |
|     ! 0 | 2897 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 2898 | `			if( rc == SXERR_ABORT ){` |
|       - | 2899 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2900 | `				return SXERR_ABORT;` |
|       - | 2901 | `			}` |
|     ! 0 | 2902 | `			goto Synchronize;` |
|       - | 2903 | `		}` |
|       - | 2904 | `		/* Swap token streams */` |
|   73962 | 2905 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 2906 | `		/* Compile the condition */` |
|   73962 | 2907 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2908 | `		/* Update token stream */` |
|   73962 | 2909 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 2910 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2911 | `			pGen->pIn++;` |
|     ! 0 | 2912 | `		}` |
|   73962 | 2913 | `		pGen->pIn  = &pEnd[1];` |
|   73962 | 2914 | `		pGen->pEnd = pTmp;` |
|   73962 | 2915 | `		if( rc == SXERR_ABORT ){` |
|       - | 2916 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2917 | `			return SXERR_ABORT;` |
|       - | 2918 | `		}` |
|       - | 2919 | `		/* Emit the false jump */` |
|   73962 | 2920 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 2921 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   73962 | 2922 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 2923 | `		/* Compile the body */` |
|   73962 | 2924 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   73962 | 2925 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2926 | `			return SXERR_ABORT;` |
|       - | 2927 | `		}` |
|   73962 | 2928 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   21391 | 2929 | `			break;` |
|       - | 2930 | `		}` |
|       - | 2931 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   31184 | 2932 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   31184 | 2933 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   23324 | 2934 | `			break;` |
|       - | 2935 | `		}` |
|       - | 2936 | `		/* Emit the unconditional jump */` |
|    7862 | 2937 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 2938 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    7862 | 2939 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|    7862 | 2940 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|    7842 | 2941 | `			pToken = &pGen->pIn[1];` |
|    7842 | 2942 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    3578 | 2943 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    2148 | 2944 | `					break;` |
|       - | 2945 | `			}` |
|    3550 | 2946 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    1774 | 2947 | `		}` |
|    3570 | 2948 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 2949 | `		/* Synchronize cursors */` |
|    3570 | 2950 | `		pToken = pGen->pIn;` |
|       - | 2951 | `		/* Fix the false jump */` |
|    3570 | 2952 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 2953 | `	} /* For(;;) */` |
|       - | 2954 | `	/* Fix the false jump */` |
|   70394 | 2955 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   70394 | 2956 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   27614 | 2957 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 2958 | `			/* Compile the else block */` |
|    4294 | 2959 | `			pGen->pIn++;` |
|    4294 | 2960 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    4294 | 2961 | `			if( rc == SXERR_ABORT ){` |
|       - | 2962 |  |
|     ! 0 | 2963 | `				return SXERR_ABORT;` |
|       - | 2964 | `			}` |
|    2146 | 2965 | `	}` |
|   70394 | 2966 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2967 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   70394 | 2968 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 2969 | `	/* Release the conditional block */` |
|   70394 | 2970 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2971 | `	/* Statement successfully compiled */` |
|   70394 | 2972 | `	return SXRET_OK;` |
|     ! 0 | 2973 | `Synchronize:` |
|       - | 2974 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 2975 | `	 */` |
|     ! 0 | 2976 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2977 | `		pGen->pIn++;` |
|     ! 0 | 2978 | `	}` |
|     ! 0 | 2979 | `	return SXRET_OK;` |
|   35198 | 2980 |  |
|       - | 2981 | `/*` |
|       - | 2982 | ` * Compile the global construct.` |
|       - | 2983 | ` * According to the PHP language reference` |
|       - | 2984 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 2985 | ` *  to be used in that function.` |
|       - | 2986 | ` *  Example #1 Using global` |
|       - | 2987 | ` *  <?php` |
|       - | 2988 | ` *   $a = 1;` |
|       - | 2989 | ` *   $b = 2;` |
|       - | 2990 | ` *   function Sum()` |
|       - | 2991 | ` *   {` |
|       - | 2992 | ` *    global $a, $b;` |
|       - | 2993 | ` *    $b = $a + $b;` |
|       - | 2994 | ` *   }` |
|       - | 2995 | ` *   Sum();` |
|       - | 2996 | ` *   echo $b;` |
|       - | 2997 | ` *  ?>` |
|       - | 2998 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 2999 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 3000 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 3001 | ` */` |
|      14 | 3002 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       1 | 3003 |  |
|      15 | 3004 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3005 | `	sxi32 nExpr;` |
|       - | 3006 | `	sxi32 rc;` |
|       - | 3007 | `	/* Jump the 'global' keyword */` |
|      15 | 3008 | `	pGen->pIn++;` |
|      15 | 3009 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 3010 | `		/* Nothing to process */` |
|     ! 0 | 3011 | `		return SXRET_OK;` |
|       - | 3012 | `	}` |
|      15 | 3013 | `	pTmp = pGen->pEnd;` |
|      15 | 3014 | `	nExpr = 0;` |
|      31 | 3015 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      17 | 3016 | `		if( pGen->pIn < pNext ){` |
|      17 | 3017 | `			pGen->pEnd = pNext;` |
|      17 | 3018 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3019 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 3020 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 3021 | `					return SXERR_ABORT;` |
|       - | 3022 | `				}` |
|     ! 0 | 3023 | `			}else{` |
|      17 | 3024 | `				pGen->pIn++;` |
|      17 | 3025 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3026 | `					/* Emit a warning */` |
|     ! 0 | 3027 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 3028 | `				}else{` |
|      17 | 3029 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      17 | 3030 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3031 | `						return SXERR_ABORT;` |
|      17 | 3032 | `					}else if(rc != SXERR_EMPTY ){` |
|      17 | 3033 | `						nExpr++;` |
|       8 | 3034 | `					}` |
|       - | 3035 | `				}` |
|       - | 3036 | `			}` |
|       8 | 3037 | `		}` |
|       - | 3038 | `		/* Next expression in the stream */` |
|      17 | 3039 | `		pGen->pIn = pNext;` |
|       - | 3040 | `		/* Jump trailing commas */` |
|      19 | 3041 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3042 | `			pGen->pIn++;` |
|       1 | 3043 | `		}` |
|       1 | 3044 | `	}` |
|       - | 3045 | `	/* Restore token stream */` |
|      15 | 3046 | `	pGen->pEnd = pTmp;` |
|      15 | 3047 | `	if( nExpr > 0 ){` |
|       - | 3048 | `		/* Emit the uplink instruction */` |
|      15 | 3049 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       7 | 3050 | `	}` |
|      15 | 3051 | `	return SXRET_OK;` |
|       8 | 3052 |  |
|       - | 3053 | `/*` |
|       - | 3054 | ` * Compile the return statement.` |
|       - | 3055 | ` * According to the PHP language reference` |
|       - | 3056 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 3057 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 3058 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 3059 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 3060 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 3061 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 3062 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 3063 | ` *  from within the main script file, then script execution end.` |
|       - | 3064 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 3065 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 3066 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 3067 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 3068 | ` */` |
|   74836 | 3069 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 3070 |  |
|   74838 | 3071 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3072 | `	sxi32 rc;` |
|       - | 3073 | `	/* Jump the 'return' keyword */` |
|   74838 | 3074 | `	pGen->pIn++;` |
|   74838 | 3075 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3076 | `		/* Compile the expression */` |
|   74816 | 3077 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   74816 | 3078 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3079 | `			return SXERR_ABORT;` |
|   74816 | 3080 | `		}else if(rc != SXERR_EMPTY ){` |
|   74816 | 3081 | `			nRet = 1;` |
|   37407 | 3082 | `		}` |
|   37407 | 3083 | `	}` |
|       - | 3084 | `	/* Emit the done instruction */` |
|   74838 | 3085 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|   74838 | 3086 | `	return SXRET_OK;` |
|   37420 | 3087 |  |
|       - | 3088 | `/*` |
|       - | 3089 | ` * Compile the die/exit language construct.` |
|       - | 3090 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 3091 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 3092 | ` */` |
|      88 | 3093 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 | 3094 |  |
|      90 | 3095 | `	sxi32 nExpr = 0;` |
|       - | 3096 | `	sxi32 rc;` |
|       - | 3097 | `	/* Jump the die/exit keyword */` |
|      90 | 3098 | `	pGen->pIn++;` |
|      90 | 3099 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3100 | `		/* Compile the expression */` |
|      90 | 3101 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 | 3102 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3103 | `			return SXERR_ABORT;` |
|      90 | 3104 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 | 3105 | `			nExpr = 1;` |
|      44 | 3106 | `		}` |
|      44 | 3107 | `	}` |
|       - | 3108 | `	/* Emit the HALT instruction */` |
|      90 | 3109 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 | 3110 | `	return SXRET_OK;` |
|      46 | 3111 |  |
|       - | 3112 | `/*` |
|       - | 3113 | ` * Compile the 'echo' language construct.` |
|       - | 3114 | ` */` |
|    8880 | 3115 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 3116 |  |
|    8882 | 3117 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3118 | `	sxi32 rc;` |
|       - | 3119 | `	/* Jump the 'echo' keyword */` |
|    8882 | 3120 | `	pGen->pIn++;` |
|       - | 3121 | `	/* Compile arguments one after one */` |
|    8882 | 3122 | `	pTmp = pGen->pEnd;` |
|   17814 | 3123 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    8934 | 3124 | `		if( pGen->pIn < pNext ){` |
|    8934 | 3125 | `			pGen->pEnd = pNext;` |
|    8934 | 3126 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    8934 | 3127 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3128 | `				return SXERR_ABORT;` |
|    8934 | 3129 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3130 | `				/* Emit the consume instruction */` |
|    8910 | 3131 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    4454 | 3132 | `			}` |
|    4466 | 3133 | `		}` |
|       - | 3134 | `		/* Jump trailing commas */` |
|    8986 | 3135 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      54 | 3136 | `			pNext++;` |
|       2 | 3137 | `		}` |
|    8934 | 3138 | `		pGen->pIn = pNext;` |
|       2 | 3139 | `	}` |
|       - | 3140 | `	/* Restore token stream */` |
|    8882 | 3141 | `	pGen->pEnd = pTmp;` |
|    8882 | 3142 | `	return SXRET_OK;` |
|    4442 | 3143 |  |
|       - | 3144 | `/*` |
|       - | 3145 | ` * Compile the static statement.` |
|       - | 3146 | ` * According to the PHP language reference` |
|       - | 3147 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 3148 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 3149 | ` *  when program execution leaves this scope.` |
|       - | 3150 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 3151 | ` * Symisc eXtension.` |
|       - | 3152 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 3153 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3154 | ` *  Example` |
|       - | 3155 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 3156 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 3157 | ` */` |
|       2 | 3158 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 | 3159 |  |
|       - | 3160 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 3161 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 3162 | `	GenBlock *pBlock;` |
|       - | 3163 | `	SyString *pName;` |
|       - | 3164 | `	char *zDup;` |
|       - | 3165 | `	sxu32 nLine;` |
|       - | 3166 | `	sxi32 rc;` |
|       - | 3167 | `	/* Jump the static keyword */` |
|       3 | 3168 | `	nLine = pGen->pIn->nLine;` |
|       3 | 3169 | `	pGen->pIn++;` |
|       - | 3170 | `	/* Extract the enclosing function if any */` |
|       3 | 3171 | `	pBlock = pGen->pCurrent;` |
|       5 | 3172 | `	while( pBlock ){` |
|       5 | 3173 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 | 3174 | `			break;` |
|       - | 3175 | `		}` |
|       - | 3176 | `		/* Point to the upper block */` |
|       3 | 3177 | `		pBlock = pBlock->pParent;` |
|       1 | 3178 | `	}` |
|       3 | 3179 | `	if( pBlock == 0 ){` |
|       - | 3180 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 3181 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3182 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 | 3183 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3184 | `				return SXERR_ABORT;` |
|       - | 3185 | `			}` |
|     ! 0 | 3186 | `			goto Synchronize;` |
|       - | 3187 | `		}` |
|       - | 3188 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 3189 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 3190 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3191 | `			return SXERR_ABORT;` |
|     ! 0 | 3192 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 3193 | `			/* Emit the POP instruction */` |
|     ! 0 | 3194 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 3195 | `		}` |
|     ! 0 | 3196 | `		return SXRET_OK;` |
|       - | 3197 | `	}` |
|       3 | 3198 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 3199 | `	/* Make sure we are dealing with a valid statement */` |
|       3 | 3200 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 | 3201 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 | 3202 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 | 3203 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3204 | `				return SXERR_ABORT;` |
|       - | 3205 | `			}` |
|       3 | 3206 | `			goto Synchronize;` |
|       - | 3207 | `	}` |
|     ! 0 | 3208 | `	pGen->pIn++;` |
|       - | 3209 | `	/* Extract variable name */` |
|     ! 0 | 3210 | `	pName = &pGen->pIn->sData;` |
|     ! 0 | 3211 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 | 3212 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 3213 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3214 | `		goto Synchronize;` |
|       - | 3215 | `	}` |
|       - | 3216 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 | 3217 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 | 3218 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 3219 | `	/* Duplicate variable name */` |
|     ! 0 | 3220 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 | 3221 | `	if( zDup == 0 ){` |
|     ! 0 | 3222 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3223 | `		return SXERR_ABORT;` |
|       - | 3224 | `	}` |
|     ! 0 | 3225 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 3226 | `	/* Check if we have an expression to compile */` |
|     ! 0 | 3227 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 3228 | `		SySet *pInstrContainer;` |
|       - | 3229 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 3230 | `		 * Static variable can take any complex expression including function` |
|       - | 3231 | `		 * call as their initialization value.` |
|       - | 3232 | `		 * Example:` |
|       - | 3233 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 3234 | `		 */` |
|     ! 0 | 3235 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 3236 | `		/* Swap bytecode container */` |
|     ! 0 | 3237 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 | 3238 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 3239 | `		/* Compile the expression */` |
|     ! 0 | 3240 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3241 | `		/* Emit the done instruction */` |
|     ! 0 | 3242 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 3243 | `		/* Restore default bytecode container */` |
|     ! 0 | 3244 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 3245 | `	}` |
|       - | 3246 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 | 3247 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 | 3248 | `	return SXRET_OK;` |
|       1 | 3249 | `Synchronize:` |
|       - | 3250 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 3251 | `	 * statement.` |
|       - | 3252 | `	 */` |
|       5 | 3253 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 | 3254 | `		pGen->pIn++;` |
|       1 | 3255 | `	}` |
|       3 | 3256 | `	return SXRET_OK;` |
|       2 | 3257 |  |
|       - | 3258 | `/*` |
|       - | 3259 | ` * Compile the var statement.` |
|       - | 3260 | ` * Symisc Extension:` |
|       - | 3261 | ` *      var statement can be used outside of a class definition.` |
|       - | 3262 | ` */` |
|       4 | 3263 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 3264 |  |
|       - | 3265 | `	sxu32 nLine;` |
|       - | 3266 | `	sxi32 rc;` |
|       5 | 3267 | `	nLine = pGen->pIn->nLine;` |
|       - | 3268 | `	/* Jump the 'var' keyword */` |
|       5 | 3269 | `	pGen->pIn++;` |
|       5 | 3270 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 3271 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - | 3272 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 | 3273 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 | 3274 | `			pGen->pIn++;` |
|     ! 0 | 3275 | `		}` |
|     ! 0 | 3276 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3277 | `			return SXERR_ABORT;` |
|       - | 3278 | `		}` |
|     ! 0 | 3279 | `	}else{` |
|       - | 3280 | `		/* Compile the expression */` |
|       5 | 3281 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 | 3282 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3283 | `			return SXERR_ABORT;` |
|       5 | 3284 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 | 3285 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 3286 | `		}` |
|       - | 3287 | `	}` |
|       5 | 3288 | `	return SXRET_OK;` |
|       3 | 3289 |  |
|       - | 3290 | `/*` |
|       - | 3291 | ` * Compile a namespace statement` |
|       - | 3292 | ` * According to the PHP language reference manual` |
|       - | 3293 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 3294 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 3295 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 3296 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 3297 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 3298 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 3299 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 3300 | ` *  programming world.` |
|       - | 3301 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 3302 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 3303 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 3304 | ` *  classes/functions/constants.` |
|       - | 3305 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 3306 | ` *  readability of source code.` |
|       - | 3307 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 3308 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 3309 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 3310 | ` *       class MyClass {}` |
|       - | 3311 | ` *       function myfunction() {}` |
|       - | 3312 | ` *       const MYCONST = 1;` |
|       - | 3313 | ` *       $a = new MyClass;` |
|       - | 3314 | ` *       $c = new \my\name\MyClass;` |
|       - | 3315 | ` *       $a = strlen('hi');` |
|       - | 3316 | ` *       $d = namespace\MYCONST;` |
|       - | 3317 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 3318 | ` *       echo constant($d);` |
|       - | 3319 | ` * NOTE` |
|       - | 3320 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3321 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3322 | ` */` |
|       6 | 3323 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       1 | 3324 |  |
|       - | 3325 | `	sxu32 nLine;` |
|       7 | 3326 | `	nLine = pGen->pIn->nLine;` |
|       - | 3327 | `	sxi32 rc;` |
|       7 | 3328 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       7 | 3329 | `	if( pGen->pIn >= pGen->pEnd \|\|` |
|       6 | 3330 | `		(pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       3 | 3331 | `			SyToken *pTok = pGen->pIn;` |
|       3 | 3332 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 3333 | `				pTok--;` |
|     ! 0 | 3334 | `			}` |
|       - | 3335 | `			/* Unexpected token */` |
|       3 | 3336 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Namespace: Unexpected token '%z'",&pTok->sData);` |
|       3 | 3337 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3338 | `				return SXERR_ABORT;` |
|       - | 3339 | `			}` |
|       1 | 3340 | `	}` |
|       - | 3341 | `	/* Ignore the path */` |
|      19 | 3342 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP/*'\'*/\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 | 3343 | `		pGen->pIn++;` |
|       1 | 3344 | `	}` |
|       7 | 3345 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 3346 | `		/* Unexpected token */` |
|       7 | 3347 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       4 | 3348 | `			"Namespace: Unexpected token '%z',expecting ';' or '{'",&pGen->pIn->sData);` |
|       5 | 3349 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3350 | `			return SXERR_ABORT;` |
|       - | 3351 | `		}` |
|       2 | 3352 | `	}` |
|       - | 3353 | `	/* Emit a warning */` |
|      10 | 3354 | `	PH7_GenCompileError(&(*pGen),E_WARNING,nLine,` |
|       3 | 3355 | `		"Namespace support is disabled in the current release of the PH7(%s) engine",ph7_lib_version());` |
|       7 | 3356 | `	return SXRET_OK;` |
|       4 | 3357 |  |
|       - | 3358 | `/*` |
|       - | 3359 | ` * Compile the 'use' statement` |
|       - | 3360 | ` * According to the PHP language reference manual` |
|       - | 3361 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 3362 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 3363 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 3364 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 3365 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 3366 | ` *  a function or constant is not supported.` |
|       - | 3367 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 3368 | ` * NOTE` |
|       - | 3369 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3370 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3371 | ` */` |
|       8 | 3372 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       1 | 3373 |  |
|       - | 3374 | `	sxu32 nLine;` |
|       9 | 3375 | `	nLine = pGen->pIn->nLine;` |
|       - | 3376 | `	sxi32 rc;` |
|       9 | 3377 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - | 3378 | `	/* Assemeble one or more real namespace path */` |
|       4 | 3379 | `	for(;;){` |
|       9 | 3380 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3381 | `			break;` |
|       - | 3382 | `		}` |
|       - | 3383 | `		/* Ignore the path */` |
|      21 | 3384 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID))  ){` |
|      13 | 3385 | `			pGen->pIn++;` |
|       1 | 3386 | `		}` |
|       9 | 3387 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA/*','*/) ){` |
|     ! 0 | 3388 | `			pGen->pIn++; /* Jump the comma and process the next path */` |
|     ! 0 | 3389 | `		}else{` |
|       5 | 3390 | `			break;` |
|       - | 3391 | `		}` |
|     ! 0 | 3392 | `	}` |
|       9 | 3393 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       3 | 3394 | `		pGen->pIn++; /* Jump the 'as' keyword */` |
|       - | 3395 | `		/* Compile one or more aliasses */` |
|       1 | 3396 | `		for(;;){` |
|       3 | 3397 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3398 | `				break;` |
|       - | 3399 | `			}` |
|       5 | 3400 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|       3 | 3401 | `				pGen->pIn++;` |
|       1 | 3402 | `			}` |
|       3 | 3403 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA/*','*/) ){` |
|     ! 0 | 3404 | `				pGen->pIn++; /* Jump the comma and process the next alias */` |
|     ! 0 | 3405 | `			}else{` |
|       2 | 3406 | `				break;` |
|       - | 3407 | `			}` |
|     ! 0 | 3408 | `		}` |
|       1 | 3409 | `	}` |
|       9 | 3410 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       - | 3411 | `		/* Unexpected token */` |
|       4 | 3412 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"use statement: Unexpected token '%z',expecting ';'",` |
|       2 | 3413 | `			&pGen->pIn->sData);` |
|       3 | 3414 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3415 | `			return SXERR_ABORT;` |
|       - | 3416 | `		}` |
|       1 | 3417 | `	}` |
|       - | 3418 | `	/* Emit a notice */` |
|      13 | 3419 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - | 3420 | `		"Namespace support is disabled in the current release of the PH7(%s) engine",` |
|       4 | 3421 | `		ph7_lib_version()` |
|       - | 3422 | `		);` |
|       9 | 3423 | `	return SXRET_OK;` |
|       5 | 3424 |  |
|       - | 3425 | `/*` |
|       - | 3426 | ` * Compile the stupid 'declare' language construct.` |
|       - | 3427 | ` *` |
|       - | 3428 | ` * According to the PHP language reference manual.` |
|       - | 3429 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 3430 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 3431 | ` *  declare (directive)` |
|       - | 3432 | ` *   statement` |
|       - | 3433 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 3434 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 3435 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 3436 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 3437 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 3438 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 3439 | ` * <?php` |
|       - | 3440 | ` * // these are the same:` |
|       - | 3441 | ` * // you can use this:` |
|       - | 3442 | ` * declare(ticks=1) {` |
|       - | 3443 | ` *   // entire script here` |
|       - | 3444 | ` * }` |
|       - | 3445 | ` * // or you can use this:` |
|       - | 3446 | ` * declare(ticks=1);` |
|       - | 3447 | ` * // entire script here` |
|       - | 3448 | ` * ?>` |
|       - | 3449 | ` *` |
|       - | 3450 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 3451 | ` */` |
|       8 | 3452 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 3453 |  |
|       9 | 3454 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 3455 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 3456 | `	sxi32 rc;` |
|       9 | 3457 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 3458 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 3459 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 3460 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3461 | `			return SXERR_ABORT;` |
|       - | 3462 | `		}` |
|       5 | 3463 | `		goto Synchro;` |
|       - | 3464 | `	}` |
|       5 | 3465 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 3466 | `	/* Delimit the directive */` |
|       5 | 3467 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 3468 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 3469 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 3470 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3471 | `			return SXERR_ABORT;` |
|       - | 3472 | `		}` |
|     ! 0 | 3473 | `		return SXRET_OK;` |
|       - | 3474 | `	}` |
|       - | 3475 | `	/* Update the cursor */` |
|       5 | 3476 | `	pGen->pIn = &pEnd[1];` |
|       5 | 3477 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 3478 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 3479 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3480 | `			return SXERR_ABORT;` |
|       - | 3481 | `		}` |
|     ! 0 | 3482 | `	}` |
|       - | 3483 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 3484 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 3485 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 3486 | `		ph7_lib_version()` |
|       - | 3487 | `		);` |
|       - | 3488 | `	/*All done */` |
|       5 | 3489 | `	return SXRET_OK;` |
|       2 | 3490 | `Synchro:` |
|       - | 3491 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 3492 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 3493 | `		pGen->pIn++;` |
|       1 | 3494 | `	}` |
|       5 | 3495 | `	return SXRET_OK;` |
|       5 | 3496 |  |
|       - | 3497 | `/*` |
|       - | 3498 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 3499 | ` * as follows:` |
|       - | 3500 | ` * function makecoffee($type = "cappuccino")` |
|       - | 3501 | ` * {` |
|       - | 3502 | ` *   return "Making a cup of $type.\n";` |
|       - | 3503 | ` * }` |
|       - | 3504 | ` * Symisc eXtension.` |
|       - | 3505 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 3506 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 3507 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 3508 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 3509 | ` *      {` |
|       - | 3510 | ` *       var_dump($a);` |
|       - | 3511 | ` *      }` |
|       - | 3512 | ` *     //call test without args` |
|       - | 3513 | ` *      test();` |
|       - | 3514 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 3515 | ` *      Example:` |
|       - | 3516 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 3517 | ` * 3 -) Function overloading!!` |
|       - | 3518 | ` *      Example:` |
|       - | 3519 | ` *      function foo($a) {` |
|       - | 3520 | ` *   	  return $a.PHP_EOL;` |
|       - | 3521 | ` *	    }` |
|       - | 3522 | ` *	    function foo($a, $b) {` |
|       - | 3523 | ` *   	  return $a + $b;` |
|       - | 3524 | ` *	    }` |
|       - | 3525 | ` *	    echo foo(5); // Prints "5"` |
|       - | 3526 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 3527 | ` *      // Same arg` |
|       - | 3528 | ` *	   function foo(string $a)` |
|       - | 3529 | ` *	   {` |
|       - | 3530 | ` *	     echo "a is a string\n";` |
|       - | 3531 | ` *	     var_dump($a);` |
|       - | 3532 | ` *	   }` |
|       - | 3533 | ` *	  function foo(int $a)` |
|       - | 3534 | ` *	  {` |
|       - | 3535 | ` *	    echo "a is integer\n";` |
|       - | 3536 | ` *	    var_dump($a);` |
|       - | 3537 | ` *	  }` |
|       - | 3538 | ` *	  function foo(array $a)` |
|       - | 3539 | ` *	  {` |
|       - | 3540 | ` * 	    echo "a is an array\n";` |
|       - | 3541 | ` * 	    var_dump($a);` |
|       - | 3542 | ` *	  }` |
|       - | 3543 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 3544 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 3545 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 3546 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 3547 | ` * introduced by the PH7 engine.` |
|       - | 3548 | ` */` |
|   23070 | 3549 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 3550 |  |
|       - | 3551 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 3552 | `	SySet *pInstrContainer;` |
|       - | 3553 | `	sxi32 rc;` |
|       - | 3554 | `	/* Swap token stream */` |
|   23072 | 3555 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   23072 | 3556 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   23072 | 3557 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 3558 | `	/* Compile the expression holding the argument value */` |
|   23072 | 3559 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3560 | `	/* Emit the done instruction */` |
|   23072 | 3561 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   23072 | 3562 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   23072 | 3563 | `	RE_SWAP_DELIMITER(pGen);` |
|   23072 | 3564 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3565 | `		return SXERR_ABORT;` |
|       - | 3566 | `	}` |
|   23072 | 3567 | `	return SXRET_OK;` |
|   11537 | 3568 |  |
|       - | 3569 | `/*` |
|       - | 3570 | ` * Collect function arguments one after one.` |
|       - | 3571 | ` * According to the PHP language reference manual.` |
|       - | 3572 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 3573 | ` * list of expressions.` |
|       - | 3574 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 3575 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 3576 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 3577 | ` * for more information.` |
|       - | 3578 | ` * Example #1 Passing arrays to functions` |
|       - | 3579 | ` * <?php` |
|       - | 3580 | ` * function takes_array($input)` |
|       - | 3581 | ` * {` |
|       - | 3582 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 3583 | ` * }` |
|       - | 3584 | ` * ?>` |
|       - | 3585 | ` * Making arguments be passed by reference` |
|       - | 3586 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 3587 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 3588 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 3589 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 3590 | ` * to the argument name in the function definition:` |
|       - | 3591 | ` * Example #2 Passing function parameters by reference` |
|       - | 3592 | ` * <?php` |
|       - | 3593 | ` * function add_some_extra(&$string)` |
|       - | 3594 | ` * {` |
|       - | 3595 | ` *   $string .= 'and something extra.';` |
|       - | 3596 | ` * }` |
|       - | 3597 | ` * $str = 'This is a string, ';` |
|       - | 3598 | ` * add_some_extra($str);` |
|       - | 3599 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 3600 | ` * ?>` |
|       - | 3601 | ` *` |
|       - | 3602 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 3603 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 3604 | ` * on these extension.` |
|       - | 3605 | ` */` |
|   27000 | 3606 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 3607 |  |
|       - | 3608 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 3609 | `	SyToken *pIn;  /* Token stream */` |
|       - | 3610 | `	SyBlob sSig;         /* Function signature */` |
|       - | 3611 | `	char *zDup;          /* Copy of argument name */` |
|       - | 3612 | `	sxi32 rc;` |
|       - | 3613 |  |
|   27002 | 3614 | `	pIn = pGen->pIn;` |
|   27002 | 3615 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 3616 | `	/* Process arguments one after one */` |
|   36962 | 3617 | `	for(;;){` |
|   73926 | 3618 | `		if( pIn >= pEnd ){` |
|       - | 3619 | `			/* No more arguments to process */` |
|   27000 | 3620 | `			break;` |
|       - | 3621 | `		}` |
|   46928 | 3622 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   46928 | 3623 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   46928 | 3624 | `		if( pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   35484 | 3625 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   31936 | 3626 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   31936 | 3627 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 3628 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   31936 | 3629 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 3630 | `					sArg.nType = MEMOBJ_BOOL;` |
|   31936 | 3631 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|    8872 | 3632 | `					sArg.nType = MEMOBJ_INT;` |
|   27501 | 3633 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   23064 | 3634 | `					sArg.nType = MEMOBJ_STRING;` |
|   11534 | 3635 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 3636 | `					sArg.nType = MEMOBJ_REAL;` |
|     ! 0 | 3637 | `				}else{` |
|       4 | 3638 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 3639 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 3640 | `						&pIn->sData);` |
|       - | 3641 | `				}` |
|   15969 | 3642 | `			}else{` |
|    3550 | 3643 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 3644 | `				char *zDupLocal;` |
|       - | 3645 | `				/* Argument must be a class instance,record that*/` |
|    3550 | 3646 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    3550 | 3647 | `				if( zDupLocal ){` |
|    3550 | 3648 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    3550 | 3649 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    1774 | 3650 | `				}` |
|       - | 3651 | `			}` |
|   35484 | 3652 | `			pIn++;` |
|   17741 | 3653 | `		}` |
|   46928 | 3654 | `		if( pIn >= pEnd ){` |
|     ! 0 | 3655 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 3656 | `			return rc;` |
|       - | 3657 | `		}` |
|   46928 | 3658 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 3659 | `			/* Pass by reference,record that */` |
|    1794 | 3660 | `			sArg.iFlags = VM_FUNC_ARG_BY_REF;` |
|    1794 | 3661 | `			pIn++;` |
|     896 | 3662 | `		}` |
|   46928 | 3663 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 3664 | `			/* Invalid argument */` |
|     ! 0 | 3665 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 3666 | `			return rc;` |
|       - | 3667 | `		}` |
|   46928 | 3668 | `		pIn++; /* Jump the dollar sign */` |
|       - | 3669 | `		/* Copy argument name */` |
|   46928 | 3670 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   46928 | 3671 | `		if( zDup == 0 ){` |
|     ! 0 | 3672 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 3673 | `			return SXERR_ABORT;` |
|       - | 3674 | `		}` |
|   46928 | 3675 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   46928 | 3676 | `		pIn++;` |
|   46928 | 3677 | `		if( pIn < pEnd ){` |
|   28810 | 3678 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 3679 | `				SyToken *pDefend;` |
|   23074 | 3680 | `				sxi32 iNest = 0;` |
|   23074 | 3681 | `				pIn++; /* Jump the equal sign */` |
|   23074 | 3682 | `				pDefend = pIn;` |
|       - | 3683 | `				/* Process the default value associated with this argument */` |
|   49692 | 3684 | `				while( pDefend < pEnd ){` |
|   40812 | 3685 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   14194 | 3686 | `						break;` |
|       - | 3687 | `					}` |
|   26620 | 3688 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 3689 | `						/* Increment nesting level */` |
|    1776 | 3690 | `						iNest++;` |
|   25733 | 3691 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 3692 | `						/* Decrement nesting level */` |
|    1776 | 3693 | `						iNest--;` |
|     887 | 3694 | `					}` |
|   26620 | 3695 | `					pDefend++;` |
|       2 | 3696 | `				}` |
|   23074 | 3697 | `				if( pIn >= pDefend ){` |
|       3 | 3698 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 3699 | `					return rc;` |
|       - | 3700 | `				}` |
|       - | 3701 | `				/* Process default value */` |
|   23072 | 3702 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   23072 | 3703 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 3704 | `					return rc;` |
|       - | 3705 | `				}` |
|       - | 3706 | `				/* Point beyond the default value */` |
|   23072 | 3707 | `				pIn = pDefend;` |
|   11535 | 3708 | `			}` |
|   28808 | 3709 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 3710 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 3711 | `				return rc;` |
|       - | 3712 | `			}` |
|   28808 | 3713 | `			pIn++; /* Jump the trailing comma */` |
|   14403 | 3714 | `		}` |
|       - | 3715 | `		/* Append argument signature */` |
|   46926 | 3716 | `		if( sArg.nType > 0 ){` |
|   35482 | 3717 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 3718 | `				/* Class name */` |
|    3550 | 3719 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    1776 | 3720 | `			}else{` |
|       - | 3721 | `				int c;` |
|   31934 | 3722 | `				c = 'n'; /* cc warning */` |
|       - | 3723 | `				/* Type leading character */` |
|   31934 | 3724 | `				switch(sArg.nType){` |
|     ! 0 | 3725 | `				case MEMOBJ_HASHMAP:` |
|       - | 3726 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 3727 | `					c = 'h';` |
|     ! 0 | 3728 | `					break;` |
|    4435 | 3729 | `				case MEMOBJ_INT:` |
|       - | 3730 | `					/* Integer */` |
|    8872 | 3731 | `					c = 'i';` |
|    8872 | 3732 | `					break;` |
|     ! 0 | 3733 | `				case MEMOBJ_BOOL:` |
|       - | 3734 | `					/* Bool */` |
|     ! 0 | 3735 | `					c = 'b';` |
|     ! 0 | 3736 | `					break;` |
|     ! 0 | 3737 | `				case MEMOBJ_REAL:` |
|       - | 3738 | `					/* Float */` |
|     ! 0 | 3739 | `					c = 'f';` |
|     ! 0 | 3740 | `					break;` |
|   11531 | 3741 | `				case MEMOBJ_STRING:` |
|       - | 3742 | `					/* String */` |
|   23064 | 3743 | `					c = 's';` |
|   23062 | 3744 | `					break;` |
|     ! 0 | 3745 | `				default:` |
|     ! 0 | 3746 | `					break;` |
|       - | 3747 | `				}` |
|   31934 | 3748 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 3749 | `			}` |
|   17742 | 3750 | `		}else{` |
|       - | 3751 | `			/* No type is associated with this parameter which mean` |
|       - | 3752 | `			 * that this function is not condidate for overloading.` |
|       - | 3753 | `			 */` |
|   11446 | 3754 | `			SyBlobRelease(&sSig);` |
|       - | 3755 | `		}` |
|       - | 3756 | `		/* Save in the argument set */` |
|   46926 | 3757 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 3758 | `	}` |
|   27000 | 3759 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 3760 | `		/* Save function signature */` |
|   21290 | 3761 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   10644 | 3762 | `	}` |
|   27000 | 3763 | `	return SXRET_OK;` |
|   13502 | 3764 |  |
|       - | 3765 | `/*` |
|       - | 3766 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 3767 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 3768 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 3769 | ` */` |
|   60942 | 3770 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 3771 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 3772 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 3773 | `	)` |
|       2 | 3774 |  |
|       - | 3775 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 3776 | `	GenBlock *pBlock;` |
|       - | 3777 | `	sxu32 nGotoOfft;` |
|       - | 3778 | `	sxi32 rc;` |
|       - | 3779 | `	/* Attach the new function */` |
|   60944 | 3780 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   60944 | 3781 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3782 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 3783 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3784 | `		return SXERR_ABORT;` |
|       - | 3785 | `	}` |
|   60944 | 3786 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 3787 | `	/* Swap bytecode containers */` |
|   60944 | 3788 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   60944 | 3789 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 3790 | `	/* Compile the body */` |
|   60944 | 3791 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 3792 | `	/* Fix exception jumps now the destination is resolved */` |
|   60944 | 3793 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3794 | `	/* Emit the final return if not yet done */` |
|   60944 | 3795 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 3796 | `	/* Fix gotos jumps now the destination is resolved */` |
|   60944 | 3797 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 3798 | `		rc = SXERR_ABORT;` |
|     ! 0 | 3799 | `	}` |
|   60944 | 3800 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 3801 | `	/* Restore the default container */` |
|   60944 | 3802 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 3803 | `	/* Leave function block */` |
|   60944 | 3804 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   60944 | 3805 | `	if( rc == SXERR_ABORT ){` |
|       - | 3806 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3807 | `		return SXERR_ABORT;` |
|       - | 3808 | `	}` |
|       - | 3809 | `	/* All done, function body compiled */` |
|   60944 | 3810 | `	return SXRET_OK;` |
|   30473 | 3811 |  |
|       - | 3812 | `/*` |
|       - | 3813 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 3814 | ` * According to the PHP language reference manual.` |
|       - | 3815 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 3816 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 3817 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 3818 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 3819 | ` *  Functions need not be defined before they are referenced.` |
|       - | 3820 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 3821 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 3822 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 3823 | ` *  calls with over 32-64 recursion levels.` |
|       - | 3824 | ` *` |
|       - | 3825 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 3826 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 3827 | ` * on these extension.` |
|       - | 3828 | ` */` |
|   23500 | 3829 | `static sxi32 GenStateCompileFunc(` |
|       - | 3830 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 3831 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 3832 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 3833 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 3834 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 3835 | `	)` |
|       2 | 3836 |  |
|       - | 3837 | `	ph7_vm_func *pFunc;` |
|       - | 3838 | `	SyToken *pEnd;` |
|       - | 3839 | `	sxu32 nLine;` |
|       - | 3840 | `	char *zName;` |
|       - | 3841 | `	sxi32 rc;` |
|       - | 3842 | `	/* Extract line number */` |
|   23502 | 3843 | `	nLine = pGen->pIn->nLine;` |
|       - | 3844 | `	/* Jump the left parenthesis '(' */` |
|   23502 | 3845 | `	pGen->pIn++;` |
|       - | 3846 | `	/* Delimit the function signature */` |
|   23502 | 3847 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   23502 | 3848 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 3849 | `		/* Syntax error */` |
|       7 | 3850 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 3851 | `		if( rc == SXERR_ABORT ){` |
|       - | 3852 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3853 | `			return SXERR_ABORT;` |
|       - | 3854 | `		}` |
|       7 | 3855 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 3856 | `		return SXRET_OK;` |
|       - | 3857 | `	}` |
|       - | 3858 | `	/* Create the function state */` |
|   23496 | 3859 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   23496 | 3860 | `	if( pFunc == 0 ){` |
|     ! 0 | 3861 | `		goto OutOfMem;` |
|       - | 3862 | `	}` |
|       - | 3863 | `	/* function ID */` |
|   23496 | 3864 | `	zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   23496 | 3865 | `	if( zName == 0 ){` |
|       - | 3866 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3867 | `		goto OutOfMem;` |
|       - | 3868 | `	}` |
|       - | 3869 | `	/* Initialize the function state */` |
|   23496 | 3870 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|   23496 | 3871 | `	if( pGen->pIn < pEnd ){` |
|       - | 3872 | `		/* Collect function arguments */` |
|   18088 | 3873 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   18088 | 3874 | `		if( rc == SXERR_ABORT ){` |
|       - | 3875 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3876 | `			return SXERR_ABORT;` |
|       - | 3877 | `		}` |
|    9043 | 3878 | `	}` |
|       - | 3879 | `	/* Compile function body */` |
|   23496 | 3880 | `	pGen->pIn = &pEnd[1];` |
|   23496 | 3881 | `	if( bHandleClosure ){` |
|       - | 3882 | `		ph7_vm_func_closure_env sEnv;` |
|      94 | 3883 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|      92 | 3884 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      52 | 3885 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      10 | 3886 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 3887 | `				/* Closure,record environment variable */` |
|      10 | 3888 | `				pGen->pIn++;` |
|      10 | 3889 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 3890 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 3891 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3892 | `						return SXERR_ABORT;` |
|       - | 3893 | `					}` |
|     ! 0 | 3894 | `				}` |
|      10 | 3895 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 3896 | `				/* Compile until we hit the first closing parenthesis */` |
|      18 | 3897 | `				while( pGen->pIn < pGen->pEnd ){` |
|      18 | 3898 | `					int iFlagsLocal = 0;` |
|      18 | 3899 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      10 | 3900 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      10 | 3901 | `						break;` |
|       - | 3902 | `					}` |
|      10 | 3903 | `					nLineLocal = pGen->pIn->nLine;` |
|      10 | 3904 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 3905 | `						/* Pass by reference,record that */` |
|     ! 0 | 3906 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 3907 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 3908 | `							);` |
|     ! 0 | 3909 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 3910 | `						pGen->pIn++;` |
|     ! 0 | 3911 | `					}` |
|       8 | 3912 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      10 | 3913 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 3914 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 3915 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 3916 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 3917 | `								return SXERR_ABORT;` |
|       - | 3918 | `							}` |
|       - | 3919 | `							/* Find the closing parenthesis */` |
|     ! 0 | 3920 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 3921 | `								pGen->pIn++;` |
|     ! 0 | 3922 | `							}` |
|     ! 0 | 3923 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 3924 | `								pGen->pIn++;` |
|     ! 0 | 3925 | `							}` |
|     ! 0 | 3926 | `							break;` |
|       - | 3927 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 3928 | `					}else{` |
|       - | 3929 | `						SyString *pNameLocal;` |
|       - | 3930 | `						char *zDup;` |
|       - | 3931 | `						/* Duplicate variable name */` |
|      10 | 3932 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      10 | 3933 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      10 | 3934 | `						if( zDup ){` |
|       - | 3935 | `							/* Zero the structure */` |
|      10 | 3936 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      10 | 3937 | `							sEnv.iFlags = iFlagsLocal;` |
|      10 | 3938 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      10 | 3939 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      10 | 3940 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 3941 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 3942 | `									got_this = 1;` |
|     ! 0 | 3943 | `							}` |
|       - | 3944 | `							/* Save imported variable */` |
|      10 | 3945 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       6 | 3946 | `						}else{` |
|     ! 0 | 3947 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3948 | `							 return SXERR_ABORT;` |
|       - | 3949 | `						}` |
|       - | 3950 | `					}` |
|      10 | 3951 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      10 | 3952 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 3953 | `						/* Ignore trailing commas */` |
|     ! 0 | 3954 | `						pGen->pIn++;` |
|     ! 0 | 3955 | `					}` |
|       2 | 3956 | `				}` |
|      10 | 3957 | `				if( !got_this ){` |
|       - | 3958 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 3959 | `					 * available to the closure environment.` |
|       - | 3960 | `					 */` |
|      10 | 3961 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      10 | 3962 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      10 | 3963 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      10 | 3964 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      10 | 3965 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       4 | 3966 | `				}` |
|      10 | 3967 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 3968 | `					/* Mark as closure */` |
|      10 | 3969 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       4 | 3970 | `				}` |
|       4 | 3971 | `		}` |
|      46 | 3972 | `	}` |
|       - | 3973 | `	/* Compile the body */` |
|   23496 | 3974 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   23496 | 3975 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3976 | `		return SXERR_ABORT;` |
|       - | 3977 | `	}` |
|   23496 | 3978 | `	if( ppFunc ){` |
|      94 | 3979 | `		*ppFunc = pFunc;` |
|      46 | 3980 | `	}` |
|   23496 | 3981 | `	rc = SXRET_OK;` |
|   23496 | 3982 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 3983 | `		/* Finally register the function */` |
|   23488 | 3984 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   11743 | 3985 | `	}` |
|   23496 | 3986 | `	if( rc == SXRET_OK ){` |
|   23496 | 3987 | `		return SXRET_OK;` |
|       - | 3988 | `	}` |
|       - | 3989 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 3990 | `OutOfMem:` |
|       - | 3991 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 3992 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 3993 | `	 */` |
|     ! 0 | 3994 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 3995 | `	return SXERR_ABORT;` |
|   11752 | 3996 |  |
|       - | 3997 | `/*` |
|       - | 3998 | ` * Compile a standard PHP function.` |
|       - | 3999 | ` *  Refer to the block-comment above for more information.` |
|       - | 4000 | ` */` |
|   23414 | 4001 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 4002 |  |
|       - | 4003 | `	SyString *pName;` |
|       - | 4004 | `	sxi32 iFlags;` |
|       - | 4005 | `	sxu32 nLine;` |
|       - | 4006 | `	sxi32 rc;` |
|       - | 4007 |  |
|   23416 | 4008 | `	nLine = pGen->pIn->nLine;` |
|   23416 | 4009 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   23416 | 4010 | `	iFlags = 0;` |
|   23416 | 4011 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4012 | `		/* Return by reference,remember that */` |
|       7 | 4013 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4014 | `		/* Jump the '&' token */` |
|       7 | 4015 | `		pGen->pIn++;` |
|       3 | 4016 | `	}` |
|   23416 | 4017 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4018 | `		/* Invalid function name */` |
|       5 | 4019 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 4020 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4021 | `			return SXERR_ABORT;` |
|       - | 4022 | `		}` |
|       - | 4023 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 4024 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 4025 | `			pGen->pIn++;` |
|       1 | 4026 | `		}` |
|       5 | 4027 | `		return SXRET_OK;` |
|       - | 4028 | `	}` |
|   23412 | 4029 | `	pName = &pGen->pIn->sData;` |
|   23412 | 4030 | `	nLine = pGen->pIn->nLine;` |
|       - | 4031 | `	/* Jump the function name */` |
|   23412 | 4032 | `	pGen->pIn++;` |
|   23412 | 4033 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4034 | `		/* Syntax error */` |
|       3 | 4035 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 4036 | `		if( rc == SXERR_ABORT ){` |
|       - | 4037 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4038 | `			return SXERR_ABORT;` |
|       - | 4039 | `		}` |
|       - | 4040 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 4041 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4042 | `			pGen->pIn++;` |
|     ! 0 | 4043 | `		}` |
|       3 | 4044 | `		return SXRET_OK;` |
|       - | 4045 | `	}` |
|       - | 4046 | `	/* Compile function body */` |
|   23410 | 4047 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   23410 | 4048 | `	return rc;` |
|   11709 | 4049 |  |
|       - | 4050 | `/*` |
|       - | 4051 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 4052 | ` * According to the PHP language reference manual` |
|       - | 4053 | ` *  Visibility:` |
|       - | 4054 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 4055 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 4056 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 4057 | ` *  Members declared protected can be accessed only within the class` |
|       - | 4058 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 4059 | ` *  may only be accessed by the class that defines the member.` |
|       - | 4060 | ` */` |
|   69578 | 4061 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 4062 |  |
|   69580 | 4063 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|      40 | 4064 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   69542 | 4065 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   12446 | 4066 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 4067 | `	}` |
|       - | 4068 | `	/* Assume public by default */` |
|   57098 | 4069 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   34791 | 4070 |  |
|       - | 4071 | `/*` |
|       - | 4072 | ` * Compile a class constant.` |
|       - | 4073 | ` * According to the PHP language reference manual` |
|       - | 4074 | ` *  Class Constants` |
|       - | 4075 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 4076 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 4077 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 4078 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 4079 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 4080 | ` *   It's also possible for interfaces to have constants.` |
|       - | 4081 | ` * Symisc eXtension.` |
|       - | 4082 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 4083 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4084 | ` *  Example:` |
|       - | 4085 | ` *   class Test{` |
|       - | 4086 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4087 | ` *   };` |
|       - | 4088 | ` *   var_dump(TEST::MyConst);` |
|       - | 4089 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4090 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4091 | ` */` |
|      10 | 4092 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4093 |  |
|      12 | 4094 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4095 | `	SySet *pInstrContainer;` |
|       - | 4096 | `	ph7_class_attr *pCons;` |
|       - | 4097 | `	SyString *pName;` |
|       - | 4098 | `	sxi32 rc;` |
|       - | 4099 | `	/* Extract visibility level */` |
|      12 | 4100 | `	iProtection = GetProtectionLevel(iProtection);` |
|      12 | 4101 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       5 | 4102 | `loop:` |
|       - | 4103 | `	/* Mark as constant */` |
|      12 | 4104 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      12 | 4105 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4106 | `		/* Invalid constant name */` |
|     ! 0 | 4107 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 4108 | `		if( rc == SXERR_ABORT ){` |
|       - | 4109 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4110 | `			return SXERR_ABORT;` |
|       - | 4111 | `		}` |
|     ! 0 | 4112 | `		goto Synchronize;` |
|       - | 4113 | `	}` |
|       - | 4114 | `	/* Peek constant name */` |
|      12 | 4115 | `	pName = &pGen->pIn->sData;` |
|       - | 4116 | `	/* Make sure the constant name isn't reserved */` |
|      12 | 4117 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 4118 | `		/* Reserved constant name */` |
|     ! 0 | 4119 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 4120 | `		if( rc == SXERR_ABORT ){` |
|       - | 4121 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4122 | `			return SXERR_ABORT;` |
|       - | 4123 | `		}` |
|     ! 0 | 4124 | `		goto Synchronize;` |
|       - | 4125 | `	}` |
|       - | 4126 | `	/* Advance the stream cursor */` |
|      12 | 4127 | `	pGen->pIn++;` |
|      12 | 4128 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 4129 | `		/* Invalid declaration */` |
|     ! 0 | 4130 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 4131 | `		if( rc == SXERR_ABORT ){` |
|       - | 4132 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4133 | `			return SXERR_ABORT;` |
|       - | 4134 | `		}` |
|     ! 0 | 4135 | `		goto Synchronize;` |
|       - | 4136 | `	}` |
|      12 | 4137 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 4138 | `	/* Allocate a new class attribute */` |
|      12 | 4139 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      12 | 4140 | `	if( pCons == 0 ){` |
|     ! 0 | 4141 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4142 | `		return SXERR_ABORT;` |
|       - | 4143 | `	}` |
|       - | 4144 | `	/* Swap bytecode container */` |
|      12 | 4145 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      12 | 4146 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 4147 | `	/* Compile constant value.` |
|       - | 4148 | `	 */` |
|      12 | 4149 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      12 | 4150 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 4151 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 4152 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4153 | `			return SXERR_ABORT;` |
|       - | 4154 | `		}` |
|       1 | 4155 | `	}` |
|       - | 4156 | `	/* Emit the done instruction */` |
|      12 | 4157 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      12 | 4158 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      12 | 4159 | `	if( rc == SXERR_ABORT ){` |
|       - | 4160 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4161 | `		return SXERR_ABORT;` |
|       - | 4162 | `	}` |
|       - | 4163 | `	/* All done,install the constant */` |
|      12 | 4164 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      12 | 4165 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4166 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4167 | `		return SXERR_ABORT;` |
|       - | 4168 | `	}` |
|      12 | 4169 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4170 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 4171 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4172 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 4173 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4174 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4175 | `				pTok--;` |
|     ! 0 | 4176 | `			}` |
|     ! 0 | 4177 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4178 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 4179 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4180 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4181 | `				return SXERR_ABORT;` |
|       - | 4182 | `			}` |
|     ! 0 | 4183 | `		}else{` |
|     ! 0 | 4184 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 4185 | `				goto loop;` |
|       - | 4186 | `			}` |
|       - | 4187 | `		}` |
|     ! 0 | 4188 | `	}` |
|      12 | 4189 | `	return SXRET_OK;` |
|     ! 0 | 4190 | `Synchronize:` |
|       - | 4191 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4192 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 4193 | `		pGen->pIn++;` |
|     ! 0 | 4194 | `	}` |
|     ! 0 | 4195 | `	return SXERR_CORRUPT;` |
|       7 | 4196 |  |
|       - | 4197 | `/*` |
|       - | 4198 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 4199 | ` * According to the PHP language reference manual` |
|       - | 4200 | ` *  Properties` |
|       - | 4201 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 4202 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 4203 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 4204 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 4205 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 4206 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 4207 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 4208 | ` * Symisc eXtension.` |
|       - | 4209 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 4210 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4211 | ` *  Example:` |
|       - | 4212 | ` *   class Test{` |
|       - | 4213 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4214 | ` *   };` |
|       - | 4215 | ` *   var_dump(TEST::myVar);` |
|       - | 4216 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4217 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4218 | ` */` |
|   17914 | 4219 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4220 |  |
|   17916 | 4221 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4222 | `	ph7_class_attr *pAttr;` |
|       - | 4223 | `	SyString *pName;` |
|       - | 4224 | `	sxi32 rc;` |
|       - | 4225 | `	/* Extract visibility level */` |
|   17916 | 4226 | `	iProtection = GetProtectionLevel(iProtection);` |
|    8957 | 4227 | `loop:` |
|   17916 | 4228 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   17916 | 4229 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 4230 | `		/* Invalid attribute name */` |
|     ! 0 | 4231 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 4232 | `		if( rc == SXERR_ABORT ){` |
|       - | 4233 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4234 | `			return SXERR_ABORT;` |
|       - | 4235 | `		}` |
|     ! 0 | 4236 | `		goto Synchronize;` |
|       - | 4237 | `	}` |
|       - | 4238 | `	/* Peek attribute name */` |
|   17916 | 4239 | `	pName = &pGen->pIn->sData;` |
|       - | 4240 | `	/* Advance the stream cursor */` |
|   17916 | 4241 | `	pGen->pIn++;` |
|   17916 | 4242 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 4243 | `		/* Invalid declaration */` |
|       3 | 4244 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 4245 | `		if( rc == SXERR_ABORT ){` |
|       - | 4246 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4247 | `			return SXERR_ABORT;` |
|       - | 4248 | `		}` |
|       3 | 4249 | `		goto Synchronize;` |
|       - | 4250 | `	}` |
|       - | 4251 | `	/* Allocate a new class attribute */` |
|   17914 | 4252 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   17914 | 4253 | `	if( pAttr == 0 ){` |
|     ! 0 | 4254 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 4255 | `		return SXERR_ABORT;` |
|       - | 4256 | `	}` |
|   17914 | 4257 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 4258 | `		SySet *pInstrContainer;` |
|    7234 | 4259 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 4260 | `		/* Swap bytecode container */` |
|    7234 | 4261 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    7234 | 4262 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 4263 | `		/* Compile attribute value.` |
|       - | 4264 | `		 */` |
|    7234 | 4265 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    7234 | 4266 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 4267 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 4268 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4269 | `				return SXERR_ABORT;` |
|       - | 4270 | `			}` |
|     ! 0 | 4271 | `		}` |
|       - | 4272 | `		/* Emit the done instruction */` |
|    7234 | 4273 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    7234 | 4274 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    3616 | 4275 | `	}` |
|       - | 4276 | `	/* All done,install the attribute */` |
|   17914 | 4277 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   17914 | 4278 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4279 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4280 | `		return SXERR_ABORT;` |
|       - | 4281 | `	}` |
|   17914 | 4282 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4283 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 4284 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4285 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 4286 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4287 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4288 | `				pTok--;` |
|     ! 0 | 4289 | `			}` |
|     ! 0 | 4290 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4291 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4292 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4293 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4294 | `				return SXERR_ABORT;` |
|       - | 4295 | `			}` |
|     ! 0 | 4296 | `		}else{` |
|     ! 0 | 4297 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 4298 | `				goto loop;` |
|       - | 4299 | `			}` |
|       - | 4300 | `		}` |
|     ! 0 | 4301 | `	}` |
|   17914 | 4302 | `	return SXRET_OK;` |
|       1 | 4303 | `Synchronize:` |
|       - | 4304 | `	/* Synchronize with the first semi-colon */` |
|       5 | 4305 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 4306 | `		pGen->pIn++;` |
|       1 | 4307 | `	}` |
|       3 | 4308 | `	return SXERR_CORRUPT;` |
|    8959 | 4309 |  |
|       - | 4310 | `/*` |
|       - | 4311 | ` * Compile a class method.` |
|       - | 4312 | ` *` |
|       - | 4313 | ` * Refer to the official documentation for more information` |
|       - | 4314 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 4315 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 4316 | ` * overloading and many more.` |
|       - | 4317 | ` */` |
|   51654 | 4318 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 4319 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4320 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 4321 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 4322 | `	int doBody,          /* TRUE to process method body */` |
|       - | 4323 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 4324 | `	)` |
|       2 | 4325 |  |
|   51656 | 4326 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4327 | `	ph7_class_method *pMeth;` |
|       - | 4328 | `	sxi32 iFuncFlags;` |
|       - | 4329 | `	SyString *pName;` |
|       - | 4330 | `	SyToken *pEnd;` |
|       - | 4331 | `	sxi32 rc;` |
|       - | 4332 | `	/* Extract visibility level */` |
|   51656 | 4333 | `	iProtection = GetProtectionLevel(iProtection);` |
|   51656 | 4334 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   51656 | 4335 | `	iFuncFlags = 0;` |
|   51656 | 4336 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4337 | `		/* Invalid method name */` |
|     ! 0 | 4338 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4339 | `		if( rc == SXERR_ABORT ){` |
|       - | 4340 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4341 | `			return SXERR_ABORT;` |
|       - | 4342 | `		}` |
|     ! 0 | 4343 | `		goto Synchronize;` |
|       - | 4344 | `	}` |
|   51656 | 4345 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4346 | `		/* Return by reference,remember that */` |
|     ! 0 | 4347 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4348 | `		/* Jump the '&' token */` |
|     ! 0 | 4349 | `		pGen->pIn++;` |
|     ! 0 | 4350 | `	}` |
|   51656 | 4351 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID)) == 0 ){` |
|       - | 4352 | `		/* Invalid method name */` |
|     ! 0 | 4353 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4354 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4355 | `			return SXERR_ABORT;` |
|       - | 4356 | `		}` |
|     ! 0 | 4357 | `		goto Synchronize;` |
|       - | 4358 | `	}` |
|       - | 4359 | `	/* Peek method name */` |
|   51656 | 4360 | `	pName = &pGen->pIn->sData;` |
|   51656 | 4361 | `	nLine = pGen->pIn->nLine;` |
|       - | 4362 | `	/* Jump the method name */` |
|   51656 | 4363 | `	pGen->pIn++;` |
|   51656 | 4364 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 4365 | `		/* Abstract method */` |
|       8 | 4366 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 4367 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4368 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 4369 | `				&pClass->sName,pName);` |
|     ! 0 | 4370 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4371 | `				return SXERR_ABORT;` |
|       - | 4372 | `			}` |
|     ! 0 | 4373 | `		}` |
|       - | 4374 | `		/* Assemble method signature only */` |
|       8 | 4375 | `		doBody = FALSE;` |
|       3 | 4376 | `	}` |
|   51656 | 4377 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4378 | `		/* Syntax error */` |
|     ! 0 | 4379 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 4380 | `		if( rc == SXERR_ABORT ){` |
|       - | 4381 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4382 | `			return SXERR_ABORT;` |
|       - | 4383 | `		}` |
|     ! 0 | 4384 | `		goto Synchronize;` |
|       - | 4385 | `	}` |
|       - | 4386 | `	/* Allocate a new class_method instance */` |
|   51656 | 4387 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   51656 | 4388 | `	if( pMeth == 0 ){` |
|     ! 0 | 4389 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4390 | `		return SXERR_ABORT;` |
|       - | 4391 | `	}` |
|       - | 4392 | `	/* Jump the left parenthesis '(' */` |
|   51656 | 4393 | `	pGen->pIn++;` |
|   51656 | 4394 | `	pEnd = 0; /* cc warning */` |
|       - | 4395 | `	/* Delimit the method signature */` |
|   51656 | 4396 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   51656 | 4397 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4398 | `		/* Syntax error */` |
|       3 | 4399 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 4400 | `		if( rc == SXERR_ABORT ){` |
|       - | 4401 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4402 | `			return SXERR_ABORT;` |
|       - | 4403 | `		}` |
|       3 | 4404 | `		goto Synchronize;` |
|       - | 4405 | `	}` |
|   51654 | 4406 | `	if( pGen->pIn < pEnd ){` |
|       - | 4407 | `		/* Collect method arguments */` |
|    8916 | 4408 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|    8916 | 4409 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4410 | `			return SXERR_ABORT;` |
|       - | 4411 | `		}` |
|    4457 | 4412 | `	}` |
|       - | 4413 | `	/* Point beyond method signature */` |
|   51654 | 4414 | `	pGen->pIn = &pEnd[1];` |
|   51654 | 4415 | `	if( doBody ){` |
|       - | 4416 | `		/* Compile method body */` |
|   37450 | 4417 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   37450 | 4418 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4419 | `			return SXERR_ABORT;` |
|       - | 4420 | `		}` |
|   18726 | 4421 | `	}else{` |
|       - | 4422 | `		/* Only method signature is allowed */` |
|   14206 | 4423 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 4424 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4425 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 4426 | `				if( rc == SXERR_ABORT ){` |
|       - | 4427 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4428 | `					return SXERR_ABORT;` |
|       - | 4429 | `				}` |
|     ! 0 | 4430 | `				return SXERR_CORRUPT;` |
|       - | 4431 | `			}` |
|       - | 4432 | `	}` |
|       - | 4433 | `	/* All done,install the method */` |
|   51654 | 4434 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   51654 | 4435 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4436 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4437 | `		return SXERR_ABORT;` |
|       - | 4438 | `	}` |
|   51654 | 4439 | `	return SXRET_OK;` |
|       1 | 4440 | `Synchronize:` |
|       - | 4441 | `	/* Synchronize with the first semi-colon */` |
|       7 | 4442 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 4443 | `		pGen->pIn++;` |
|       1 | 4444 | `	}` |
|       3 | 4445 | `	return SXERR_CORRUPT;` |
|   25829 | 4446 |  |
|       - | 4447 | `/*` |
|       - | 4448 | ` * Compile an object interface.` |
|       - | 4449 | ` *  According to the PHP language reference manual` |
|       - | 4450 | ` *   Object Interfaces:` |
|       - | 4451 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 4452 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 4453 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 4454 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 4455 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 4456 | ` */` |
|    5332 | 4457 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 4458 |  |
|    5334 | 4459 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4460 | `	ph7_class *pClass,*pBase;` |
|       - | 4461 | `	SyToken *pEnd,*pTmp;` |
|       - | 4462 | `	SyString *pName;` |
|       - | 4463 | `	sxi32 nKwrd;` |
|       - | 4464 | `	sxi32 rc;` |
|       - | 4465 | `	/* Jump the 'interface' keyword */` |
|    5334 | 4466 | `	pGen->pIn++;` |
|       - | 4467 | `	/* Extract interface name */` |
|    5334 | 4468 | `	pName = &pGen->pIn->sData;` |
|       - | 4469 | `	/* Advance the stream cursor */` |
|    5334 | 4470 | `	pGen->pIn++;` |
|       - | 4471 | `	/* Obtain a raw class */` |
|    5334 | 4472 | `	pClass = PH7_NewRawClass(pGen->pVm,pName,nLine);` |
|    5334 | 4473 | `	if( pClass == 0 ){` |
|     ! 0 | 4474 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4475 | `		return SXERR_ABORT;` |
|       - | 4476 | `	}` |
|       - | 4477 | `	/* Mark as an interface */` |
|    5334 | 4478 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 4479 | `	/* Assume no base class is given */` |
|    5334 | 4480 | `	pBase = 0;` |
|    5334 | 4481 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 4482 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 4483 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 4484 | `			SyString *pBaseName;` |
|       - | 4485 | `			/* Extract base interface */` |
|       3 | 4486 | `			pGen->pIn++;` |
|       3 | 4487 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4488 | `				/* Syntax error */` |
|     ! 0 | 4489 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4490 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 4491 | `					pName);` |
|     ! 0 | 4492 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4493 | `				if( rc == SXERR_ABORT ){` |
|       - | 4494 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4495 | `					return SXERR_ABORT;` |
|       - | 4496 | `				}` |
|     ! 0 | 4497 | `				return SXRET_OK;` |
|       - | 4498 | `			}` |
|       3 | 4499 | `			pBaseName = &pGen->pIn->sData;` |
|       3 | 4500 | `			pBase = PH7_VmExtractClass(pGen->pVm,pBaseName->zString,pBaseName->nByte,FALSE,0);` |
|       - | 4501 | `			/* Only interfaces is allowed */` |
|       3 | 4502 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 4503 | `				pBase = pBase->pNextName;` |
|     ! 0 | 4504 | `			}` |
|       3 | 4505 | `			if( pBase == 0 ){` |
|       - | 4506 | `				/* Inexistant interface */` |
|     ! 0 | 4507 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 4508 | `				if( rc == SXERR_ABORT ){` |
|       - | 4509 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4510 | `					return SXERR_ABORT;` |
|       - | 4511 | `				}` |
|     ! 0 | 4512 | `			}` |
|       - | 4513 | `			/* Advance the stream cursor */` |
|       3 | 4514 | `			pGen->pIn++;` |
|       1 | 4515 | `		}` |
|       1 | 4516 | `	}` |
|    5334 | 4517 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 4518 | `		/* Syntax error */` |
|     ! 0 | 4519 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 4520 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4521 | `		if( rc == SXERR_ABORT ){` |
|       - | 4522 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4523 | `			return SXERR_ABORT;` |
|       - | 4524 | `		}` |
|     ! 0 | 4525 | `		return SXRET_OK;` |
|       - | 4526 | `	}` |
|    5334 | 4527 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    5334 | 4528 | `	pEnd = 0; /* cc warning */` |
|       - | 4529 | `	/* Delimit the interface body */` |
|    5334 | 4530 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    5334 | 4531 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4532 | `		/* Syntax error */` |
|     ! 0 | 4533 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 4534 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4535 | `		if( rc == SXERR_ABORT ){` |
|       - | 4536 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4537 | `			return SXERR_ABORT;` |
|       - | 4538 | `		}` |
|     ! 0 | 4539 | `		return SXRET_OK;` |
|       - | 4540 | `	}` |
|       - | 4541 | `	/* Swap token stream */` |
|    5334 | 4542 | `	pTmp = pGen->pEnd;` |
|    5334 | 4543 | `	pGen->pEnd = pEnd;` |
|       - | 4544 | `	/* Start the parse process` |
|       - | 4545 | `	 * Note (According to the PHP reference manual):` |
|       - | 4546 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 4547 | `	 *  Only 'public' visibility is allowed.` |
|       - | 4548 | `	 */` |
|    9766 | 4549 | `	for(;;){` |
|       - | 4550 | `		/* Jump leading/trailing semi-colons */` |
|   33734 | 4551 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   14202 | 4552 | `			pGen->pIn++;` |
|       2 | 4553 | `		}` |
|   19534 | 4554 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4555 | `			/* End of interface body */` |
|    5334 | 4556 | `			break;` |
|       - | 4557 | `		}` |
|   14202 | 4558 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4559 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4560 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 4561 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 4562 | `			if( rc == SXERR_ABORT ){` |
|       - | 4563 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4564 | `				return SXERR_ABORT;` |
|       - | 4565 | `			}` |
|     ! 0 | 4566 | `			goto done;` |
|       - | 4567 | `		}` |
|       - | 4568 | `		/* Extract the current keyword */` |
|   14202 | 4569 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   14202 | 4570 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 4571 | `			/* Emit a warning and switch to public visibility */` |
|     ! 0 | 4572 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"interface: Access type must be public");` |
|     ! 0 | 4573 | `			nKwrd = PH7_TKWRD_PUBLIC;` |
|     ! 0 | 4574 | `		}` |
|   14202 | 4575 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 4576 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4577 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 4578 | `			if( rc == SXERR_ABORT ){` |
|       - | 4579 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4580 | `				return SXERR_ABORT;` |
|       - | 4581 | `			}` |
|     ! 0 | 4582 | `			goto done;` |
|       - | 4583 | `		}` |
|   14202 | 4584 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 4585 | `			/* Advance the stream cursor */` |
|   14200 | 4586 | `			pGen->pIn++;` |
|   14200 | 4587 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4588 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4589 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 4590 | `				if( rc == SXERR_ABORT ){` |
|       - | 4591 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4592 | `					return SXERR_ABORT;` |
|       - | 4593 | `				}` |
|     ! 0 | 4594 | `				goto done;` |
|       - | 4595 | `			}` |
|   14200 | 4596 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   14200 | 4597 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 4598 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4599 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 4600 | `				if( rc == SXERR_ABORT ){` |
|       - | 4601 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4602 | `					return SXERR_ABORT;` |
|       - | 4603 | `				}` |
|     ! 0 | 4604 | `				goto done;` |
|       - | 4605 | `			}` |
|    7099 | 4606 | `		}` |
|   14202 | 4607 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 4608 | `			/* Parse constant */` |
|       3 | 4609 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 4610 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 4611 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4612 | `					return SXERR_ABORT;` |
|       - | 4613 | `				}` |
|     ! 0 | 4614 | `				goto done;` |
|       - | 4615 | `			}` |
|       2 | 4616 | `		}else{` |
|   14200 | 4617 | `			sxi32 iFlags = 0;` |
|   14200 | 4618 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 4619 | `				/* Static method,record that */` |
|     ! 0 | 4620 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 4621 | `				/* Advance the stream cursor */` |
|     ! 0 | 4622 | `				pGen->pIn++;` |
|     ! 0 | 4623 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 4624 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 4625 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4626 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 4627 | `						if( rc == SXERR_ABORT ){` |
|       - | 4628 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 4629 | `							return SXERR_ABORT;` |
|       - | 4630 | `						}` |
|     ! 0 | 4631 | `						goto done;` |
|       - | 4632 | `				}` |
|     ! 0 | 4633 | `			}` |
|       - | 4634 | `			/* Process method signature */` |
|   14200 | 4635 | `			rc = GenStateCompileClassMethod(&(*pGen),0,FALSE/* Only method signature*/,iFlags,pClass);` |
|   14200 | 4636 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 4637 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4638 | `					return SXERR_ABORT;` |
|       - | 4639 | `				}` |
|     ! 0 | 4640 | `				goto done;` |
|       - | 4641 | `			}` |
|       - | 4642 | `		}` |
|       2 | 4643 | `	}` |
|       - | 4644 | `	/* Install the interface */` |
|    5334 | 4645 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    5334 | 4646 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 4647 | `		/* Inherit from the base interface */` |
|       3 | 4648 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 4649 | `	}` |
|    5334 | 4650 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4651 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4652 | `		return SXERR_ABORT;` |
|       - | 4653 | `	}` |
|    2666 | 4654 | `done:` |
|       - | 4655 | `	/* Point beyond the interface body */` |
|    5334 | 4656 | `	pGen->pIn  = &pEnd[1];` |
|    5334 | 4657 | `	pGen->pEnd = pTmp;` |
|    5334 | 4658 | `	return PH7_OK;` |
|    2668 | 4659 |  |
|       - | 4660 | `/*` |
|       - | 4661 | ` * Compile a user-defined class.` |
|       - | 4662 | ` * According to the PHP language reference manual` |
|       - | 4663 | ` *  class` |
|       - | 4664 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 4665 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 4666 | ` *  of the properties and methods belonging to the class.` |
|       - | 4667 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 4668 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 4669 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 4670 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4671 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 4672 | ` *  (called "methods").` |
|       - | 4673 | ` */` |
|   14440 | 4674 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 4675 |  |
|   14442 | 4676 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4677 | `	ph7_class *pClass,*pBase;` |
|       - | 4678 | `	SyToken *pEnd,*pTmp;` |
|       - | 4679 | `	sxi32 iProtection;` |
|       - | 4680 | `	SySet aInterfaces;` |
|       - | 4681 | `	sxi32 iAttrflags;` |
|       - | 4682 | `	SyString *pName;` |
|       - | 4683 | `	sxi32 nKwrd;` |
|       - | 4684 | `	sxi32 rc;` |
|       - | 4685 | `	/* Jump the 'class' keyword */` |
|   14442 | 4686 | `	pGen->pIn++;` |
|   14442 | 4687 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4688 | `		/* Syntax error */` |
|     ! 0 | 4689 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 4690 | `		if( rc == SXERR_ABORT ){` |
|       - | 4691 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4692 | `			return SXERR_ABORT;` |
|       - | 4693 | `		}` |
|       - | 4694 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 4695 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 4696 | `			pGen->pIn++;` |
|     ! 0 | 4697 | `		}` |
|     ! 0 | 4698 | `		return SXRET_OK;` |
|       - | 4699 | `	}` |
|       - | 4700 | `	/* Extract class name */` |
|   14442 | 4701 | `	pName = &pGen->pIn->sData;` |
|       - | 4702 | `	/* Advance the stream cursor */` |
|   14442 | 4703 | `	pGen->pIn++;` |
|       - | 4704 | `	/* Obtain a raw class */` |
|   14442 | 4705 | `	pClass = PH7_NewRawClass(pGen->pVm,pName,nLine);` |
|   14442 | 4706 | `	if( pClass == 0 ){` |
|     ! 0 | 4707 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4708 | `		return SXERR_ABORT;` |
|       - | 4709 | `	}` |
|       - | 4710 | `	/* implemented interfaces container */` |
|   14442 | 4711 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|       - | 4712 | `	/* Assume a standalone class */` |
|   14442 | 4713 | `	pBase = 0;` |
|   14442 | 4714 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 4715 | `		SyString *pBaseName;` |
|    8914 | 4716 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    8914 | 4717 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|    8910 | 4718 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    8910 | 4719 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4720 | `				/* Syntax error */` |
|     ! 0 | 4721 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4722 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 4723 | `					pName);` |
|     ! 0 | 4724 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4725 | `				if( rc == SXERR_ABORT ){` |
|       - | 4726 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4727 | `					return SXERR_ABORT;` |
|       - | 4728 | `				}` |
|     ! 0 | 4729 | `				return SXRET_OK;` |
|       - | 4730 | `			}` |
|       - | 4731 | `			/* Extract base class name */` |
|    8910 | 4732 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 4733 | `			/* Perform the query */` |
|    8910 | 4734 | `			pBase = PH7_VmExtractClass(pGen->pVm,pBaseName->zString,pBaseName->nByte,FALSE,0);` |
|       - | 4735 | `			/* Interfaces are not allowed */` |
|    8910 | 4736 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 4737 | `				pBase = pBase->pNextName;` |
|     ! 0 | 4738 | `			}` |
|    8910 | 4739 | `			if( pBase == 0 ){` |
|       - | 4740 | `				/* Inexistant base class */` |
|     ! 0 | 4741 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 4742 | `				if( rc == SXERR_ABORT ){` |
|       - | 4743 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4744 | `					return SXERR_ABORT;` |
|       - | 4745 | `				}` |
|     ! 0 | 4746 | `			}else{` |
|    8910 | 4747 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 4748 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 4749 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 4750 | `					if( rc == SXERR_ABORT ){` |
|       - | 4751 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 4752 | `						return SXERR_ABORT;` |
|       - | 4753 | `					}` |
|     ! 0 | 4754 | `				}` |
|       - | 4755 | `			}` |
|       - | 4756 | `			/* Advance the stream cursor */` |
|    8910 | 4757 | `			pGen->pIn++;` |
|    4454 | 4758 | `		}` |
|    8914 | 4759 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 4760 | `			ph7_class *pInterface;` |
|       - | 4761 | `			SyString *pIntName;` |
|       - | 4762 | `			/* Interface implementation */` |
|       5 | 4763 | `			pGen->pIn++; /* Advance the stream cursor */` |
|       2 | 4764 | `			for(;;){` |
|       5 | 4765 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4766 | `					/* Syntax error */` |
|     ! 0 | 4767 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4768 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 4769 | `						pName);` |
|     ! 0 | 4770 | `					if( rc == SXERR_ABORT ){` |
|       - | 4771 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 4772 | `						return SXERR_ABORT;` |
|       - | 4773 | `					}` |
|     ! 0 | 4774 | `					break;` |
|       - | 4775 | `				}` |
|       - | 4776 | `				/* Extract interface name */` |
|       5 | 4777 | `				pIntName = &pGen->pIn->sData;` |
|       - | 4778 | `				/* Make sure the interface is already defined */` |
|       5 | 4779 | `				pInterface = PH7_VmExtractClass(pGen->pVm,pIntName->zString,pIntName->nByte,FALSE,0);` |
|       - | 4780 | `				/* Only interfaces are allowed */` |
|       5 | 4781 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 4782 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 4783 | `				}` |
|       5 | 4784 | `				if( pInterface == 0 ){` |
|       - | 4785 | `					/* Inexistant interface */` |
|     ! 0 | 4786 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 4787 | `					if( rc == SXERR_ABORT ){` |
|       - | 4788 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 4789 | `						return SXERR_ABORT;` |
|       - | 4790 | `					}` |
|     ! 0 | 4791 | `				}else{` |
|       - | 4792 | `					/* Register interface */` |
|       5 | 4793 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 4794 | `				}` |
|       - | 4795 | `				/* Advance the stream cursor */` |
|       5 | 4796 | `				pGen->pIn++;` |
|       5 | 4797 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 4798 | `					break;` |
|       - | 4799 | `				}` |
|     ! 0 | 4800 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 4801 | `			}` |
|       2 | 4802 | `		}` |
|    4456 | 4803 | `	}` |
|   14442 | 4804 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 4805 | `		/* Syntax error */` |
|     ! 0 | 4806 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 4807 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4808 | `		if( rc == SXERR_ABORT ){` |
|       - | 4809 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4810 | `			return SXERR_ABORT;` |
|       - | 4811 | `		}` |
|     ! 0 | 4812 | `		return SXRET_OK;` |
|       - | 4813 | `	}` |
|   14442 | 4814 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   14442 | 4815 | `	pEnd = 0; /* cc warning */` |
|       - | 4816 | `	/* Delimit the class body */` |
|   14442 | 4817 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   14442 | 4818 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4819 | `		/* Syntax error */` |
|     ! 0 | 4820 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 4821 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4822 | `		if( rc == SXERR_ABORT ){` |
|       - | 4823 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4824 | `			return SXERR_ABORT;` |
|       - | 4825 | `		}` |
|     ! 0 | 4826 | `		return SXRET_OK;` |
|       - | 4827 | `	}` |
|       - | 4828 | `	/* Swap token stream */` |
|   14442 | 4829 | `	pTmp = pGen->pEnd;` |
|   14442 | 4830 | `	pGen->pEnd = pEnd;` |
|       - | 4831 | `	/* Set the inherited flags */` |
|   14442 | 4832 | `	pClass->iFlags = iFlags;` |
|       - | 4833 | `	/* Start the parse process */` |
|   25954 | 4834 | `	for(;;){` |
|       - | 4835 | `		/* Jump leading/trailing semi-colons */` |
|   87742 | 4836 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   17928 | 4837 | `			pGen->pIn++;` |
|       2 | 4838 | `		}` |
|   69816 | 4839 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4840 | `			/* End of class body */` |
|   14438 | 4841 | `			break;` |
|       - | 4842 | `		}` |
|   55380 | 4843 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 4844 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4845 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4846 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 4847 | `			if( rc == SXERR_ABORT ){` |
|       - | 4848 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4849 | `				return SXERR_ABORT;` |
|       - | 4850 | `			}` |
|     ! 0 | 4851 | `			goto done;` |
|       - | 4852 | `		}` |
|       - | 4853 | `		/* Assume public visibility */` |
|   55380 | 4854 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   55380 | 4855 | `		iAttrflags = 0;` |
|   55380 | 4856 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 4857 | `			/* Extract the current keyword */` |
|   55380 | 4858 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   55380 | 4859 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|   53572 | 4860 | `				iProtection = nKwrd;` |
|   53572 | 4861 | `				pGen->pIn++; /* Jump the visibility token */` |
|   53572 | 4862 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 4863 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4864 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4865 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 4866 | `					if( rc == SXERR_ABORT ){` |
|       - | 4867 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 4868 | `						return SXERR_ABORT;` |
|       - | 4869 | `					}` |
|     ! 0 | 4870 | `					goto done;` |
|       - | 4871 | `				}` |
|   53572 | 4872 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 4873 | `					/* Attribute declaration */` |
|   17906 | 4874 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   17906 | 4875 | `					if( rc != SXRET_OK ){` |
|       3 | 4876 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 4877 | `							return SXERR_ABORT;` |
|       - | 4878 | `						}` |
|       3 | 4879 | `						goto done;` |
|       - | 4880 | `					}` |
|   17904 | 4881 | `					continue;` |
|       - | 4882 | `				}` |
|       - | 4883 | `				/* Extract the keyword */` |
|   35668 | 4884 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   17833 | 4885 | `			}` |
|   37476 | 4886 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 4887 | `				/* Process constant declaration */` |
|      10 | 4888 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 | 4889 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 4890 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4891 | `						return SXERR_ABORT;` |
|       - | 4892 | `					}` |
|     ! 0 | 4893 | `					goto done;` |
|       - | 4894 | `				}` |
|       6 | 4895 | `			}else{` |
|   37468 | 4896 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 4897 | `					/* Static method or attribute,record that */` |
|      23 | 4898 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      23 | 4899 | `					pGen->pIn++; /* Jump the static keyword */` |
|      23 | 4900 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 4901 | `						/* Extract the keyword */` |
|      19 | 4902 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      19 | 4903 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 4904 | `							iProtection = nKwrd;` |
|     ! 0 | 4905 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 4906 | `						}` |
|       9 | 4907 | `					}` |
|      23 | 4908 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 4909 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4910 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 4911 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 4912 | `						if( rc == SXERR_ABORT ){` |
|       - | 4913 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 4914 | `							return SXERR_ABORT;` |
|       - | 4915 | `						}` |
|     ! 0 | 4916 | `						goto done;` |
|       - | 4917 | `					}` |
|      23 | 4918 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 4919 | `						/* Attribute declaration */` |
|       5 | 4920 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 4921 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 4922 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4923 | `								return SXERR_ABORT;` |
|       - | 4924 | `							}` |
|     ! 0 | 4925 | `							goto done;` |
|       - | 4926 | `						}` |
|       5 | 4927 | `						continue;` |
|       - | 4928 | `					}` |
|       - | 4929 | `					/* Extract the keyword */` |
|      19 | 4930 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   37455 | 4931 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 4932 | `					/* Abstract method,record that */` |
|       8 | 4933 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 4934 | `					/* Mark the whole class as abstract */` |
|       8 | 4935 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 4936 | `					/* Advance the stream cursor */` |
|       8 | 4937 | `					pGen->pIn++;` |
|       8 | 4938 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       8 | 4939 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       8 | 4940 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 | 4941 | `							iProtection = nKwrd;` |
|       6 | 4942 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 4943 | `						}` |
|       3 | 4944 | `					}` |
|       8 | 4945 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       6 | 4946 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 4947 | `							/* Static method */` |
|     ! 0 | 4948 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 4949 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 4950 | `					}` |
|       8 | 4951 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       6 | 4952 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 4953 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4954 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 4955 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 4956 | `							if( rc == SXERR_ABORT ){` |
|       - | 4957 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 4958 | `								return SXERR_ABORT;` |
|       - | 4959 | `							}` |
|     ! 0 | 4960 | `							goto done;` |
|       - | 4961 | `					}` |
|       8 | 4962 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   37443 | 4963 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 4964 | `					/* final method ,record that */` |
|       5 | 4965 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 4966 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 4967 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 4968 | `						/* Extract the keyword */` |
|       5 | 4969 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 4970 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 4971 | `							iProtection = nKwrd;` |
|       5 | 4972 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 4973 | `						}` |
|       2 | 4974 | `					}` |
|       5 | 4975 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 4976 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 4977 | `							/* Static method */` |
|     ! 0 | 4978 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 4979 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 4980 | `					}` |
|       5 | 4981 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 4982 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 4983 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4984 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 4985 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 4986 | `							if( rc == SXERR_ABORT ){` |
|       - | 4987 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 4988 | `								return SXERR_ABORT;` |
|       - | 4989 | `							}` |
|     ! 0 | 4990 | `							goto done;` |
|       - | 4991 | `					}` |
|       5 | 4992 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 4993 | `				}` |
|   37464 | 4994 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 4995 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4996 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 4997 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 4998 | `						if( rc == SXERR_ABORT ){` |
|       - | 4999 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5000 | `							return SXERR_ABORT;` |
|       - | 5001 | `						}` |
|     ! 0 | 5002 | `						goto done;` |
|       - | 5003 | `				}` |
|   37464 | 5004 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 5005 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 5006 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 5007 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5008 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 5009 | `						if( rc == SXERR_ABORT ){` |
|       - | 5010 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5011 | `							return SXERR_ABORT;` |
|       - | 5012 | `						}` |
|     ! 0 | 5013 | `						goto done;` |
|       - | 5014 | `					}` |
|       - | 5015 | `					/* Attribute declaration */` |
|       7 | 5016 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 5017 | `				}else{` |
|       - | 5018 | `					/* Process method declaration */` |
|   37458 | 5019 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 5020 | `				}` |
|   37464 | 5021 | `				if( rc != SXRET_OK ){` |
|       3 | 5022 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5023 | `						return SXERR_ABORT;` |
|       - | 5024 | `					}` |
|       3 | 5025 | `					goto done;` |
|       - | 5026 | `				}` |
|       - | 5027 | `			}` |
|   18736 | 5028 | `		}else{` |
|       - | 5029 | `			/* Attribute declaration */` |
|     ! 0 | 5030 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5031 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5032 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5033 | `					return SXERR_ABORT;` |
|       - | 5034 | `				}` |
|     ! 0 | 5035 | `				goto done;` |
|       - | 5036 | `			}` |
|       - | 5037 | `		}` |
|       2 | 5038 | `	}` |
|       - | 5039 | `	/* Install the class */` |
|   14438 | 5040 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   14438 | 5041 | `	if( rc == SXRET_OK ){` |
|       - | 5042 | `		ph7_class **apInterface;` |
|       - | 5043 | `		sxu32 n;` |
|   14438 | 5044 | `		if( pBase ){` |
|       - | 5045 | `			/* Inherit from base class and mark as a subclass */` |
|    8910 | 5046 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    4454 | 5047 | `		}` |
|   14438 | 5048 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   14442 | 5049 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 5050 | `			/* Implements one or more interface */` |
|       5 | 5051 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|       5 | 5052 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5053 | `				break;` |
|       - | 5054 | `			}` |
|       3 | 5055 | `		}` |
|    7218 | 5056 | `	}` |
|   14438 | 5057 | `	SySetRelease(&aInterfaces);` |
|   14438 | 5058 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5059 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5060 | `		return SXERR_ABORT;` |
|       - | 5061 | `	}` |
|    7218 | 5062 | `done:` |
|       - | 5063 | `	/* Point beyond the class body */` |
|   14442 | 5064 | `	pGen->pIn = &pEnd[1];` |
|   14442 | 5065 | `	pGen->pEnd = pTmp;` |
|   14442 | 5066 | `	return PH7_OK;` |
|    7222 | 5067 |  |
|       - | 5068 | `/*` |
|       - | 5069 | ` * Compile a user-defined abstract class.` |
|       - | 5070 | ` *  According to the PHP language reference manual` |
|       - | 5071 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 5072 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 5073 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 5074 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 5075 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 5076 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 5077 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 5078 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 5079 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 5080 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 5081 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 5082 | ` *   could differ.` |
|       - | 5083 | ` */` |
|       4 | 5084 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 5085 |  |
|       - | 5086 | `	sxi32 rc;` |
|       6 | 5087 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|       6 | 5088 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|       6 | 5089 | `	return rc;` |
|       2 | 5090 |  |
|       - | 5091 | `/*` |
|       - | 5092 | ` * Compile a user-defined final class.` |
|       - | 5093 | ` *  According to the PHP language reference manual` |
|       - | 5094 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 5095 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 5096 | ` *    final then it cannot be extended.` |
|       - | 5097 | ` */` |
|       2 | 5098 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 5099 |  |
|       - | 5100 | `	sxi32 rc;` |
|       3 | 5101 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 5102 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 5103 | `	return rc;` |
|       1 | 5104 |  |
|       - | 5105 | `/*` |
|       - | 5106 | ` * Compile a user-defined class.` |
|       - | 5107 | ` *  According to the PHP language reference manual` |
|       - | 5108 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 5109 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 5110 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 5111 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 5112 | ` *   and functions (called "methods").` |
|       - | 5113 | ` */` |
|   14434 | 5114 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 5115 |  |
|       - | 5116 | `	sxi32 rc;` |
|   14436 | 5117 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   14436 | 5118 | `	return rc;` |
|       2 | 5119 |  |
|       - | 5120 | `/*` |
|       - | 5121 | ` * Exception handling.` |
|       - | 5122 | ` *  According to the PHP language reference manual` |
|       - | 5123 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 5124 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 5125 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 5126 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 5127 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 5128 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 5129 | ` *    (or re-thrown) within a catch block.` |
|       - | 5130 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 5131 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 5132 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 5133 | ` *    been defined with set_exception_handler().` |
|       - | 5134 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 5135 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 5136 | ` */` |
|       - | 5137 | `/*` |
|       - | 5138 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 5139 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 5140 | ` * indicates failure.` |
|       - | 5141 | ` */` |
|    3566 | 5142 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 5143 |  |
|    3568 | 5144 | `	sxi32 rc = SXRET_OK;` |
|    3568 | 5145 | `	if( pRoot->pOp ){` |
|    3564 | 5146 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    1784 | 5147 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 5148 | `			/* Unexpected expression */` |
|     ! 0 | 5149 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 5150 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 5151 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 5152 | `				rc = SXERR_INVALID;` |
|     ! 0 | 5153 | `			}` |
|       2 | 5154 | `		}` |
|    1785 | 5155 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 5156 | `		/* Unexpected expression */` |
|     ! 0 | 5157 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 5158 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 5159 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 5160 | `			rc = SXERR_INVALID;` |
|     ! 0 | 5161 | `		}` |
|     ! 0 | 5162 | `	}` |
|    3568 | 5163 | `	return rc;` |
|       2 | 5164 |  |
|       - | 5165 | `/*` |
|       - | 5166 | ` * Compile a 'throw' statement.` |
|       - | 5167 | ` * throw: This is how you trigger an exception.` |
|       - | 5168 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 5169 | ` */` |
|    3566 | 5170 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 5171 |  |
|    3568 | 5172 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5173 | `	GenBlock *pBlock;` |
|       - | 5174 | `	sxu32 nIdx;` |
|       - | 5175 | `	sxi32 rc;` |
|    3568 | 5176 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 5177 | `	/* Compile the expression */` |
|    3568 | 5178 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    3568 | 5179 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 5180 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 5181 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5182 | `			return SXERR_ABORT;` |
|       - | 5183 | `		}` |
|     ! 0 | 5184 | `		return SXRET_OK;` |
|       - | 5185 | `	}` |
|    3568 | 5186 | `	pBlock = pGen->pCurrent;` |
|       - | 5187 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   14230 | 5188 | `	while(pBlock->pParent){` |
|   14226 | 5189 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    3564 | 5190 | `			break;` |
|       - | 5191 | `		}` |
|       - | 5192 | `		/* Point to the parent block */` |
|   10664 | 5193 | `		pBlock = pBlock->pParent;` |
|       2 | 5194 | `	}` |
|       - | 5195 | `	/* Emit the throw instruction */` |
|    3568 | 5196 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 5197 | `	/* Emit the jump */` |
|    3568 | 5198 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    3568 | 5199 | `	return SXRET_OK;` |
|    1785 | 5200 |  |
|       - | 5201 | `/*` |
|       - | 5202 | ` * Compile a 'catch' block.` |
|       - | 5203 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 5204 | ` * an object containing the exception information.` |
|       - | 5205 | ` */` |
|      30 | 5206 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 5207 |  |
|      32 | 5208 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5209 | `	ph7_exception_block sCatch;` |
|       - | 5210 | `	SySet *pInstrContainer;` |
|       - | 5211 | `	GenBlock *pCatch;` |
|       - | 5212 | `	SyToken *pToken;` |
|       - | 5213 | `	SyString *pName;` |
|       - | 5214 | `	char *zDup;` |
|       - | 5215 | `	sxi32 rc;` |
|      32 | 5216 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 5217 | `	/* Zero the structure */` |
|      32 | 5218 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 5219 | `	/* Initialize fields */` |
|      32 | 5220 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|      45 | 5221 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ \|\|` |
|      32 | 5222 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5223 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 5224 | `			pToken = pGen->pIn;` |
|     ! 0 | 5225 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 5226 | `				pToken--;` |
|     ! 0 | 5227 | `			}` |
|     ! 0 | 5228 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 5229 | `				"Catch: Unexpected token '%z',excpecting class name",&pToken->sData);` |
|     ! 0 | 5230 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5231 | `				return SXERR_ABORT;` |
|       - | 5232 | `			}` |
|     ! 0 | 5233 | `			return SXERR_INVALID;` |
|       - | 5234 | `	}` |
|       - | 5235 | `	/* Extract the exception class */` |
|      32 | 5236 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|       - | 5237 | `	/* Duplicate class name */` |
|      32 | 5238 | `	pName = &pGen->pIn->sData;` |
|      32 | 5239 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      32 | 5240 | `	if( zDup == 0 ){` |
|     ! 0 | 5241 | `		goto Mem;` |
|       - | 5242 | `	}` |
|      32 | 5243 | `	SyStringInitFromBuf(&sCatch.sClass,zDup,pName->nByte);` |
|      32 | 5244 | `	pGen->pIn++;` |
|      45 | 5245 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      32 | 5246 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5247 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 5248 | `			pToken = pGen->pIn;` |
|     ! 0 | 5249 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 5250 | `				pToken--;` |
|     ! 0 | 5251 | `			}` |
|     ! 0 | 5252 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 5253 | `				"Catch: Unexpected token '%z',expecting variable name",&pToken->sData);` |
|     ! 0 | 5254 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5255 | `				return SXERR_ABORT;` |
|       - | 5256 | `			}` |
|     ! 0 | 5257 | `			return SXERR_INVALID;` |
|       - | 5258 | `	}` |
|      32 | 5259 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 5260 | `	/* Duplicate instance name */` |
|      32 | 5261 | `	pName = &pGen->pIn->sData;` |
|      32 | 5262 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      32 | 5263 | `	if( zDup == 0 ){` |
|     ! 0 | 5264 | `		goto Mem;` |
|       - | 5265 | `	}` |
|      32 | 5266 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      32 | 5267 | `	pGen->pIn++;` |
|      32 | 5268 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 5269 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 5270 | `		pToken = pGen->pIn;` |
|     ! 0 | 5271 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 5272 | `			pToken--;` |
|     ! 0 | 5273 | `		}` |
|     ! 0 | 5274 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 5275 | `			"Catch: Unexpected token '%z',expecting right parenthesis ')'",&pToken->sData);` |
|     ! 0 | 5276 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5277 | `			return SXERR_ABORT;` |
|       - | 5278 | `		}` |
|     ! 0 | 5279 | `		return SXERR_INVALID;` |
|       - | 5280 | `	}` |
|       - | 5281 | `	/* Compile the block */` |
|      32 | 5282 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 5283 | `	/* Create the catch block */` |
|      32 | 5284 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      32 | 5285 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5286 | `		return SXERR_ABORT;` |
|       - | 5287 | `	}` |
|       - | 5288 | `	/* Swap bytecode container */` |
|      32 | 5289 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 | 5290 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 5291 | `	/* Compile the block */` |
|      32 | 5292 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 5293 | `	/* Fix forward jumps now the destination is resolved  */` |
|      32 | 5294 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 5295 | `	/* Emit the DONE instruction */` |
|      32 | 5296 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 5297 | `	/* Leave the block */` |
|      32 | 5298 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 5299 | `	/* Restore the default container */` |
|      32 | 5300 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 5301 | `	/* Install the catch block */` |
|      32 | 5302 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      32 | 5303 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5304 | `		goto Mem;` |
|       - | 5305 | `	}` |
|      32 | 5306 | `	return SXRET_OK;` |
|     ! 0 | 5307 | `Mem:` |
|     ! 0 | 5308 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5309 | `	return SXERR_ABORT;` |
|      17 | 5310 |  |
|       - | 5311 | `/*` |
|       - | 5312 | ` * Compile a 'try' block.` |
|       - | 5313 | ` * A function using an exception should be in a "try" block.` |
|       - | 5314 | ` * If the exception does not trigger, the code will continue` |
|       - | 5315 | ` * as normal. However if the exception triggers, an exception` |
|       - | 5316 | ` * is "thrown".` |
|       - | 5317 | ` */` |
|      32 | 5318 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 5319 |  |
|       - | 5320 | `	ph7_exception *pException;` |
|       - | 5321 | `	GenBlock *pTry;` |
|       - | 5322 | `	sxu32 nJmpIdx;` |
|       - | 5323 | `	sxi32 rc;` |
|       - | 5324 | `	/* Create the exception container */` |
|      34 | 5325 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|      34 | 5326 | `	if( pException == 0 ){` |
|     ! 0 | 5327 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 5328 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5329 | `		return SXERR_ABORT;` |
|       - | 5330 | `	}` |
|       - | 5331 | `	/* Zero the structure */` |
|      34 | 5332 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 5333 | `	/* Initialize fields */` |
|      34 | 5334 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|      34 | 5335 | `	pException->pVm = pGen->pVm;` |
|       - | 5336 | `	/* Create the try block */` |
|      34 | 5337 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      34 | 5338 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5339 | `		return SXERR_ABORT;` |
|       - | 5340 | `	}` |
|       - | 5341 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|      34 | 5342 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 5343 | `	/* Fix the jump later when the destination is resolved */` |
|      34 | 5344 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|      34 | 5345 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 5346 | `	/* Compile the block */` |
|      34 | 5347 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      34 | 5348 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5349 | `		return SXERR_ABORT;` |
|       - | 5350 | `	}` |
|       - | 5351 | `	/* Fix forward jumps now the destination is resolved */` |
|      34 | 5352 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 5353 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|      34 | 5354 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 5355 | `	/* Leave the block */` |
|      34 | 5356 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 5357 | `	/* Compile the catch block */` |
|      34 | 5358 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      30 | 5359 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       3 | 5360 | `			SyToken *pTok = pGen->pIn;` |
|       3 | 5361 | `			if( pTok >= pGen->pEnd ){` |
|       3 | 5362 | `				pTok--; /* Point back */` |
|       1 | 5363 | `			}` |
|       - | 5364 | `			/* Unexpected token */` |
|       4 | 5365 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTok->nLine,` |
|       1 | 5366 | `				"Try: Unexpected token '%z',expecting 'catch' block",&pTok->sData);` |
|       3 | 5367 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5368 | `				return SXERR_ABORT;` |
|       - | 5369 | `			}` |
|       3 | 5370 | `			return SXRET_OK;` |
|       - | 5371 | `	}` |
|       - | 5372 | `	/* Compile one or more catch blocks */` |
|      30 | 5373 | `	for(;;){` |
|      60 | 5374 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      43 | 5375 | `			\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       - | 5376 | `				/* No more blocks */` |
|      17 | 5377 | `				break;` |
|       - | 5378 | `		}` |
|       - | 5379 | `		/* Compile the catch block */` |
|      32 | 5380 | `		rc = PH7_CompileCatch(&(*pGen),pException);` |
|      32 | 5381 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5382 | `			return SXERR_ABORT;` |
|       - | 5383 | `		}` |
|       2 | 5384 | ` 	}` |
|      32 | 5385 | `	return SXRET_OK;` |
|      18 | 5386 |  |
|       - | 5387 | `/*` |
|       - | 5388 | ` * Compile a switch block.` |
|       - | 5389 | ` *  (See block-comment below for more information)` |
|       - | 5390 | ` */` |
|      84 | 5391 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 5392 |  |
|      86 | 5393 | `	sxi32 rc = SXRET_OK;` |
|      86 | 5394 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 5395 | `		/* Unexpected token */` |
|     ! 0 | 5396 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 5397 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5398 | `			return SXERR_ABORT;` |
|       - | 5399 | `		}` |
|     ! 0 | 5400 | `		pGen->pIn++;` |
|     ! 0 | 5401 | `	}` |
|      86 | 5402 | `	pGen->pIn++;` |
|       - | 5403 | `	/* First instruction to execute in this block. */` |
|      86 | 5404 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 5405 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 5406 | `	 * or the '}' token */` |
|     151 | 5407 | `	for(;;){` |
|     304 | 5408 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5409 | `			/* No more input to process */` |
|     ! 0 | 5410 | `			break;` |
|       - | 5411 | `		}` |
|     304 | 5412 | `		rc = SXRET_OK;` |
|     304 | 5413 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      62 | 5414 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      20 | 5415 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 5416 | `					/* Unexpected token */` |
|     ! 0 | 5417 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 5418 | `						&pGen->pIn->sData);` |
|     ! 0 | 5419 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5420 | `						return SXERR_ABORT;` |
|       - | 5421 | `					}` |
|       - | 5422 | `					/* FALL THROUGH */` |
|     ! 0 | 5423 | `				}` |
|      20 | 5424 | `				rc = SXERR_EOF;` |
|      20 | 5425 | `				break;` |
|       - | 5426 | `			}` |
|      23 | 5427 | `		}else{` |
|       - | 5428 | `			sxi32 nKwrd;` |
|       - | 5429 | `			/* Extract the keyword */` |
|     244 | 5430 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     244 | 5431 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      34 | 5432 | `				break;` |
|       - | 5433 | `			}` |
|     180 | 5434 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 5435 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 5436 | `					/* Unexpected token */` |
|     ! 0 | 5437 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 5438 | `						&pGen->pIn->sData);` |
|     ! 0 | 5439 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5440 | `						return SXERR_ABORT;` |
|       - | 5441 | `					}` |
|       - | 5442 | `					/* FALL THROUGH */` |
|     ! 0 | 5443 | `				}` |
|       - | 5444 | `				/* Block compiled */` |
|       3 | 5445 | `				break;` |
|       - | 5446 | `			}` |
|       - | 5447 | `		}` |
|       - | 5448 | `		/* Compile block */` |
|     220 | 5449 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     220 | 5450 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5451 | `			return SXERR_ABORT;` |
|       - | 5452 | `		}` |
|       2 | 5453 | `	}` |
|      86 | 5454 | `	return rc;` |
|      44 | 5455 |  |
|       - | 5456 | `/*` |
|       - | 5457 | ` * Compile a case eXpression.` |
|       - | 5458 | ` *  (See block-comment below for more information)` |
|       - | 5459 | ` */` |
|      70 | 5460 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 5461 |  |
|       - | 5462 | `	SySet *pInstrContainer;` |
|       - | 5463 | `	SyToken *pEnd,*pTmp;` |
|      72 | 5464 | `	sxi32 iNest = 0;` |
|       - | 5465 | `	sxi32 rc;` |
|       - | 5466 | `	/* Delimit the expression */` |
|      72 | 5467 | `	pEnd = pGen->pIn;` |
|     150 | 5468 | `	while( pEnd < pGen->pEnd ){` |
|     150 | 5469 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 5470 | `			/* Increment nesting level */` |
|       3 | 5471 | `			iNest++;` |
|     149 | 5472 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 5473 | `			/* Decrement nesting level */` |
|       3 | 5474 | `			iNest--;` |
|     147 | 5475 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      72 | 5476 | `			break;` |
|       - | 5477 | `		}` |
|      80 | 5478 | `		pEnd++;` |
|       2 | 5479 | `	}` |
|      72 | 5480 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 5481 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 5482 | `		if( rc == SXERR_ABORT ){` |
|       - | 5483 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5484 | `			return SXERR_ABORT;` |
|       - | 5485 | `		}` |
|     ! 0 | 5486 | `	}` |
|       - | 5487 | `	/* Swap token stream */` |
|      72 | 5488 | `	pTmp = pGen->pEnd;` |
|      72 | 5489 | `	pGen->pEnd = pEnd;` |
|      72 | 5490 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      72 | 5491 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      72 | 5492 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 5493 | `	/* Emit the done instruction */` |
|      72 | 5494 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      72 | 5495 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 5496 | `	/* Update token stream */` |
|      72 | 5497 | `	pGen->pIn  = pEnd;` |
|      72 | 5498 | `	pGen->pEnd = pTmp;` |
|      72 | 5499 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5500 | `		return SXERR_ABORT;` |
|       - | 5501 | `	}` |
|      72 | 5502 | `	return SXRET_OK;` |
|      37 | 5503 |  |
|       - | 5504 | `/*` |
|       - | 5505 | ` * Compile the smart switch statement.` |
|       - | 5506 | ` * According to the PHP language reference manual` |
|       - | 5507 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 5508 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 5509 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 5510 | ` *  This is exactly what the switch statement is for.` |
|       - | 5511 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 5512 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 5513 | ` *  of the outer loop, use continue 2.` |
|       - | 5514 | ` *  Note that switch/case does loose comparision.` |
|       - | 5515 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 5516 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 5517 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 5518 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 5519 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 5520 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 5521 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 5522 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 5523 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 5524 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 5525 | ` *  list for the next case.` |
|       - | 5526 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 5527 | ` *  or floating-point numbers and strings.` |
|       - | 5528 | ` */` |
|      20 | 5529 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 5530 |  |
|       - | 5531 | `	GenBlock *pSwitchBlock;` |
|       - | 5532 | `	SyToken *pTmp,*pEnd;` |
|       - | 5533 | `	ph7_switch *pSwitch;` |
|       - | 5534 | `	sxu32 nToken;` |
|       - | 5535 | `	sxu32 nLine;` |
|       - | 5536 | `	sxi32 rc;` |
|      22 | 5537 | `	nLine = pGen->pIn->nLine;` |
|       - | 5538 | `	/* Jump the 'switch' keyword */` |
|      22 | 5539 | `	pGen->pIn++;` |
|      22 | 5540 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 5541 | `		/* Syntax error */` |
|     ! 0 | 5542 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 5543 | `		if( rc == SXERR_ABORT ){` |
|       - | 5544 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5545 | `			return SXERR_ABORT;` |
|       - | 5546 | `		}` |
|     ! 0 | 5547 | `		goto Synchronize;` |
|       - | 5548 | `	}` |
|       - | 5549 | `	/* Jump the left parenthesis '(' */` |
|      22 | 5550 | `	pGen->pIn++;` |
|      22 | 5551 | `	pEnd = 0; /* cc warning */` |
|       - | 5552 | `	/* Create the loop block */` |
|      32 | 5553 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      10 | 5554 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      22 | 5555 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5556 | `		return SXERR_ABORT;` |
|       - | 5557 | `	}` |
|       - | 5558 | `	/* Delimit the condition */` |
|      22 | 5559 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      22 | 5560 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 5561 | `		/* Empty expression */` |
|     ! 0 | 5562 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 5563 | `		if( rc == SXERR_ABORT ){` |
|       - | 5564 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5565 | `			return SXERR_ABORT;` |
|       - | 5566 | `		}` |
|     ! 0 | 5567 | `	}` |
|       - | 5568 | `	/* Swap token streams */` |
|      22 | 5569 | `	pTmp = pGen->pEnd;` |
|      22 | 5570 | `	pGen->pEnd = pEnd;` |
|       - | 5571 | `	/* Compile the expression */` |
|      22 | 5572 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      22 | 5573 | `	if( rc == SXERR_ABORT ){` |
|       - | 5574 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 5575 | `		return SXERR_ABORT;` |
|       - | 5576 | `	}` |
|       - | 5577 | `	/* Update token stream */` |
|      22 | 5578 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 5579 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5580 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 5581 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5582 | `			return SXERR_ABORT;` |
|       - | 5583 | `		}` |
|     ! 0 | 5584 | `		pGen->pIn++;` |
|     ! 0 | 5585 | `	}` |
|      22 | 5586 | `	pGen->pIn  = &pEnd[1];` |
|      22 | 5587 | `	pGen->pEnd = pTmp;` |
|      22 | 5588 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      20 | 5589 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 5590 | `			pTmp = pGen->pIn;` |
|     ! 0 | 5591 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 5592 | `				pTmp--;` |
|     ! 0 | 5593 | `			}` |
|       - | 5594 | `			/* Unexpected token */` |
|     ! 0 | 5595 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 5596 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5597 | `				return SXERR_ABORT;` |
|       - | 5598 | `			}` |
|     ! 0 | 5599 | `			goto Synchronize;` |
|       - | 5600 | `	}` |
|       - | 5601 | `	/* Set the delimiter token */` |
|      22 | 5602 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 5603 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 5604 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 5605 | `	}else{` |
|      20 | 5606 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 5607 | `	}` |
|      22 | 5608 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 5609 | `	/* Create the switch blocks container */` |
|      22 | 5610 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      22 | 5611 | `	if( pSwitch == 0 ){` |
|       - | 5612 | `		/* Abort compilation */` |
|     ! 0 | 5613 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5614 | `		return SXERR_ABORT;` |
|       - | 5615 | `	}` |
|       - | 5616 | `	/* Zero the structure */` |
|      22 | 5617 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 5618 | `	/* Initialize fields */` |
|      22 | 5619 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 5620 | `	/* Emit the switch instruction */` |
|      22 | 5621 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 5622 | `	/* Compile case blocks */` |
|      76 | 5623 | `	for(;;){` |
|       - | 5624 | `		sxu32 nKwrd;` |
|      88 | 5625 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5626 | `			/* No more input to process */` |
|     ! 0 | 5627 | `			break;` |
|       - | 5628 | `		}` |
|      88 | 5629 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5630 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 5631 | `				/* Unexpected token */` |
|     ! 0 | 5632 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 5633 | `					&pGen->pIn->sData);` |
|     ! 0 | 5634 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5635 | `					return SXERR_ABORT;` |
|       - | 5636 | `				}` |
|       - | 5637 | `				/* FALL THROUGH */` |
|     ! 0 | 5638 | `			}` |
|       - | 5639 | `			/* Block compiled */` |
|     ! 0 | 5640 | `			break;` |
|       - | 5641 | `		}` |
|       - | 5642 | `		/* Extract the keyword */` |
|      88 | 5643 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      88 | 5644 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 5645 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 5646 | `				/* Unexpected token */` |
|     ! 0 | 5647 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 5648 | `					&pGen->pIn->sData);` |
|     ! 0 | 5649 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5650 | `					return SXERR_ABORT;` |
|       - | 5651 | `				}` |
|       - | 5652 | `				/* FALL THROUGH */` |
|     ! 0 | 5653 | `			}` |
|       - | 5654 | `			/* Block compiled */` |
|       3 | 5655 | `			break;` |
|       - | 5656 | `		}` |
|      86 | 5657 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 5658 | `			/*` |
|       - | 5659 | `			 * Accroding to the PHP language reference manual` |
|       - | 5660 | `			 *  A special case is the default case. This case matches anything` |
|       - | 5661 | `			 *  that wasn't matched by the other cases.` |
|       - | 5662 | `			 */` |
|      16 | 5663 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 5664 | `				/* Default case already compiled */` |
|     ! 0 | 5665 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 5666 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5667 | `					return SXERR_ABORT;` |
|       - | 5668 | `				}` |
|     ! 0 | 5669 | `			}` |
|      16 | 5670 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 5671 | `			/* Compile the default block */` |
|      16 | 5672 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      16 | 5673 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 5674 | `				return SXERR_ABORT;` |
|      16 | 5675 | `			}else if( rc == SXERR_EOF ){` |
|      14 | 5676 | `				break;` |
|       1 | 5677 | `			}` |
|      73 | 5678 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 5679 | `			ph7_case_expr sCase;` |
|       - | 5680 | `			/* Standard case block */` |
|      72 | 5681 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 5682 | `			/* initialize the structure */` |
|      72 | 5683 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 5684 | `			/* Compile the case expression */` |
|      72 | 5685 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      72 | 5686 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5687 | `				return SXERR_ABORT;` |
|       - | 5688 | `			}` |
|       - | 5689 | `			/* Compile the case block */` |
|      72 | 5690 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 5691 | `			/* Insert in the switch container */` |
|      72 | 5692 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      72 | 5693 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 5694 | `				return SXERR_ABORT;` |
|      72 | 5695 | `			}else if( rc == SXERR_EOF ){` |
|       7 | 5696 | `				break;` |
|       - | 5697 | `			}` |
|      34 | 5698 | `		}else{` |
|       - | 5699 | `			/* Unexpected token */` |
|     ! 0 | 5700 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 5701 | `				&pGen->pIn->sData);` |
|     ! 0 | 5702 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5703 | `				return SXERR_ABORT;` |
|       - | 5704 | `			}` |
|     ! 0 | 5705 | `			break;` |
|       - | 5706 | `		}` |
|       2 | 5707 | `	}` |
|       - | 5708 | `	/* Fix all jumps now the destination is resolved */` |
|      22 | 5709 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      22 | 5710 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 5711 | `	/* Release the loop block */` |
|      22 | 5712 | `	GenStateLeaveBlock(pGen,0);` |
|      22 | 5713 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 5714 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      22 | 5715 | `		pGen->pIn++;` |
|      10 | 5716 | `	}` |
|       - | 5717 | `	/* Statement successfully compiled */` |
|      22 | 5718 | `	return SXRET_OK;` |
|     ! 0 | 5719 | `Synchronize:` |
|       - | 5720 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 5721 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 5722 | `		pGen->pIn++;` |
|     ! 0 | 5723 | `	}` |
|     ! 0 | 5724 | `	return SXRET_OK;` |
|      12 | 5725 |  |
|       - | 5726 | `/*` |
|       - | 5727 | ` * Generate bytecode for a given expression tree.` |
|       - | 5728 | ` * If something goes wrong while generating bytecode` |
|       - | 5729 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 5730 | ` * this function takes care of generating the appropriate` |
|       - | 5731 | ` * error message.` |
|       - | 5732 | ` */` |
| 1389792 | 5733 | `static sxi32 GenStateEmitExprCode(` |
|       - | 5734 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 5735 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 5736 | `	sxi32 iFlags /* Control flags */` |
|       - | 5737 | `	)` |
|       2 | 5738 |  |
|       - | 5739 | `	VmInstr *pInstr;` |
|       - | 5740 | `	sxu32 nJmpIdx;` |
| 1389794 | 5741 | `	sxi32 iP1 = 0;` |
| 1389794 | 5742 | `	sxu32 iP2 = 0;` |
| 1389794 | 5743 | `	void *p3  = 0;` |
|       - | 5744 | `	sxi32 iVmOp;` |
|       - | 5745 | `	sxi32 rc;` |
| 1389794 | 5746 | `	if( pNode->xCode ){` |
|       - | 5747 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 5748 | `		/* Compile node */` |
|  853958 | 5749 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  853958 | 5750 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  853958 | 5751 | `		RE_SWAP_DELIMITER(pGen);` |
|  853958 | 5752 | `		return rc;` |
|       - | 5753 | `	}` |
|  535838 | 5754 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 5755 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 5756 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 5757 | `		return SXERR_ABORT;` |
|       - | 5758 | `	}` |
|  535838 | 5759 | `	iVmOp = pNode->pOp->iVmOp;` |
|  535838 | 5760 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 5761 | `		sxu32 nJz,nJmp;` |
|       - | 5762 | `		/* Ternary operator require special handling */` |
|       - | 5763 | `		/* Phase#1: Compile the condition */` |
|    1596 | 5764 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1596 | 5765 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 5766 | `			return rc;` |
|       - | 5767 | `		}` |
|    1596 | 5768 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|       - | 5769 | `		/* Phase#2: Emit the false jump */` |
|    1596 | 5770 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|    1596 | 5771 | `		if( pNode->pLeft ){` |
|       - | 5772 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1596 | 5773 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1596 | 5774 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5775 | `				return rc;` |
|       - | 5776 | `			}` |
|     797 | 5777 | `		}` |
|       - | 5778 | `		/* Phase#4: Emit the unconditional jump */` |
|    1596 | 5779 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 5780 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1596 | 5781 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1596 | 5782 | `		if( pInstr ){` |
|    1596 | 5783 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     797 | 5784 | `		}` |
|       - | 5785 | `		/* Phase#6: Compile the 'else' expression */` |
|    1596 | 5786 | `		if( pNode->pRight ){` |
|    1596 | 5787 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1596 | 5788 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5789 | `				return rc;` |
|       - | 5790 | `			}` |
|     797 | 5791 | `		}` |
|    1596 | 5792 | `		if( nJmp > 0 ){` |
|       - | 5793 | `			/* Phase#7: Fix the unconditional jump */` |
|    1596 | 5794 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1596 | 5795 | `			if( pInstr ){` |
|    1596 | 5796 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     797 | 5797 | `			}` |
|     797 | 5798 | `		}` |
|       - | 5799 | `		/* All done */` |
|    1596 | 5800 | `		return SXRET_OK;` |
|       - | 5801 | `	}` |
|       - | 5802 | `	/* Generate code for the left tree */` |
|  534244 | 5803 | `	if( pNode->pLeft ){` |
|  534244 | 5804 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 5805 | `			ph7_expr_node **apNode;` |
|       - | 5806 | `			sxi32 n;` |
|       - | 5807 | `			/* Recurse and generate bytecodes for function arguments */` |
|  155746 | 5808 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 5809 | `			/* Read-only load */` |
|  155746 | 5810 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  304216 | 5811 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  148472 | 5812 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  148472 | 5813 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5814 | `					return rc;` |
|       - | 5815 | `				}` |
|   74237 | 5816 | `			}` |
|       - | 5817 | `			/* Total number of given arguments */` |
|  155746 | 5818 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 5819 | `			/* Remove stale flags now */` |
|  155746 | 5820 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   77872 | 5821 | `		}` |
|  534244 | 5822 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  534244 | 5823 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 5824 | `			return rc;` |
|       - | 5825 | `		}` |
|  534244 | 5826 | `		if( iVmOp == PH7_OP_CALL ){` |
|  155746 | 5827 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  155746 | 5828 | `			if( pInstr ){` |
|  155746 | 5829 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|       - | 5830 | `					/* Prevent constant expansion */` |
|  155586 | 5831 | `					pInstr->iP1 = 0;` |
|   77954 | 5832 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 5833 | `					/* Method call,flag that */` |
|     152 | 5834 | `					pInstr->iP2 = 1;` |
|      75 | 5835 | `				}` |
|   77874 | 5836 | `			}` |
|  456372 | 5837 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 5838 | `			ph7_expr_node **apNode;` |
|       - | 5839 | `			sxi32 n;` |
|       - | 5840 | `			/* Recurse and generate bytecodes for array index */` |
|   42120 | 5841 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   75322 | 5842 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   33204 | 5843 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   33204 | 5844 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5845 | `					return rc;` |
|       - | 5846 | `				}` |
|   16603 | 5847 | `			}` |
|   42120 | 5848 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   33204 | 5849 | `				iP1 = 1; /* Node have an index associated with it */` |
|   16601 | 5850 | `			}` |
|   42120 | 5851 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 5852 | `				/* Create an empty entry when the desired index is not found */` |
|   12702 | 5853 | `				iP2 = 1;` |
|    6352 | 5854 | `			}` |
|  357441 | 5855 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 5856 | `			/* POP the left node */` |
|      32 | 5857 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 5858 | `		}` |
|  267121 | 5859 | `	}` |
|  534244 | 5860 | `	rc = SXRET_OK;` |
|  534244 | 5861 | `	nJmpIdx = 0;` |
|       - | 5862 | `	/* Generate code for the right tree */` |
|  534244 | 5863 | `	if( pNode->pRight ){` |
|  297604 | 5864 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 5865 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    3924 | 5866 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  295643 | 5867 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 5868 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|     142 | 5869 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  293612 | 5870 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  130480 | 5871 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|   65239 | 5872 | `		}` |
|  297604 | 5873 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  297604 | 5874 | `		if( iVmOp == PH7_OP_STORE ){` |
|  128598 | 5875 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  128598 | 5876 | `			if( pInstr ){` |
|  128598 | 5877 | `				if( pInstr->iOp == PH7_OP_LOAD_LIST ){` |
|       - | 5878 | `					/* Hide the STORE instruction */` |
|      26 | 5879 | `					iVmOp = 0;` |
|  128586 | 5880 | `				}else if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 5881 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   30222 | 5882 | `					iP2 = 1;` |
|   15112 | 5883 | `				}else{` |
|   98354 | 5884 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 5885 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   12700 | 5886 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   12700 | 5887 | `						iP1 = pInstr->iP1;` |
|    6351 | 5888 | `					}else{` |
|   85656 | 5889 | `						p3 = pInstr->p3;` |
|       - | 5890 | `					}` |
|       - | 5891 | `					/* POP the last dynamic load instruction */` |
|   98354 | 5892 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 5893 | `				}` |
|   64300 | 5894 | `			}` |
|  233306 | 5895 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      42 | 5896 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      42 | 5897 | `			if( pInstr ){` |
|      42 | 5898 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 5899 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 5900 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 5901 | `					 */` |
|      13 | 5902 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      13 | 5903 | `					iP1 = pInstr->iP1;` |
|      13 | 5904 | `					iP2 = pInstr->iP2;` |
|      13 | 5905 | `					p3  = pInstr->p3;` |
|       7 | 5906 | `				}else{` |
|      30 | 5907 | `					p3 = pInstr->p3;` |
|       - | 5908 | `				}` |
|      20 | 5909 | `			}` |
|      20 | 5910 | `		}` |
|  148801 | 5911 | `	}` |
|  534244 | 5912 | `	if( iVmOp > 0 ){` |
|  534190 | 5913 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    7292 | 5914 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 5915 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    5330 | 5916 | `				iP1 = 1;` |
|    2666 | 5917 | `			}` |
|  530545 | 5918 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|    7328 | 5919 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    7328 | 5920 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 5921 | `				VmInstr *pPrev;` |
|    7320 | 5922 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    7320 | 5923 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 5924 | `					/* Pop the call instruction */` |
|    7320 | 5925 | `					iP1 = pInstr->iP1;` |
|    7320 | 5926 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    3659 | 5927 | `				}` |
|    3661 | 5928 | `			}` |
|  523237 | 5929 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|   67818 | 5930 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 5931 | `				/* Static member access,remember that */` |
|      53 | 5932 | `				iP1 = 1;` |
|      53 | 5933 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      53 | 5934 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|       3 | 5935 | `					p3 = pInstr->p3;` |
|       3 | 5936 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       1 | 5937 | `				}` |
|      26 | 5938 | `			}` |
|   33908 | 5939 | `		}` |
|       - | 5940 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  534190 | 5941 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  534190 | 5942 | `		if( nJmpIdx > 0 ){` |
|       - | 5943 | `			/* Fix short-circuited jumps now the destination is resolved */` |
|    4064 | 5944 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    4064 | 5945 | `			if( pInstr ){` |
|    4064 | 5946 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    2031 | 5947 | `			}` |
|    2031 | 5948 | `		}` |
|  267094 | 5949 | `	}` |
|  534244 | 5950 | `	return rc;` |
|  694898 | 5951 |  |
|       - | 5952 | `/*` |
|       - | 5953 | ` * Compile a PHP expression.` |
|       - | 5954 | ` * According to the PHP language reference manual:` |
|       - | 5955 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 5956 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 5957 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 5958 | ` *  is "anything that has a value".` |
|       - | 5959 | ` * If something goes wrong while compiling the expression,this` |
|       - | 5960 | ` * function takes care of generating the appropriate error` |
|       - | 5961 | ` * message.` |
|       - | 5962 | ` */` |
|  371664 | 5963 | `static sxi32 PH7_CompileExpr(` |
|       - | 5964 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 5965 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 5966 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 5967 | `	)` |
|       2 | 5968 |  |
|       - | 5969 | `	ph7_expr_node *pRoot;` |
|       - | 5970 | `	SySet sExprNode;` |
|       - | 5971 | `	SyToken *pEnd;` |
|       - | 5972 | `	sxi32 nExpr;` |
|       - | 5973 | `	sxi32 iNest;` |
|       - | 5974 | `	sxi32 rc;` |
|       - | 5975 | `	/* Initialize worker variables */` |
|  371666 | 5976 | `	nExpr = 0;` |
|  371666 | 5977 | `	pRoot = 0;` |
|  371666 | 5978 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  371666 | 5979 | `	SySetAlloc(&sExprNode,0x10);` |
|  371666 | 5980 | `	rc = SXRET_OK;` |
|       - | 5981 | `	/* Delimit the expression */` |
|  371666 | 5982 | `	pEnd = pGen->pIn;` |
|  371666 | 5983 | `	iNest = 0;` |
| 2474306 | 5984 | `	while( pEnd < pGen->pEnd ){` |
| 2342658 | 5985 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 5986 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     148 | 5987 | `			iNest++;` |
| 2342585 | 5988 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     156 | 5989 | `			iNest--;` |
| 2342435 | 5990 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  240132 | 5991 | `			if( iNest <= 0 ){` |
|  240018 | 5992 | `				break;` |
|       - | 5993 | `			}` |
|      57 | 5994 | `		}` |
| 2102642 | 5995 | `		pEnd++;` |
|       2 | 5996 | `	}` |
|  371666 | 5997 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    7244 | 5998 | `		SyToken *pEnd2 = pGen->pIn;` |
|    7244 | 5999 | `		iNest = 0;` |
|       - | 6000 | `		/* Stop at the first comma */` |
|   14506 | 6001 | `		while( pEnd2 < pEnd ){` |
|    7264 | 6002 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       3 | 6003 | `				iNest++;` |
|    7263 | 6004 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       3 | 6005 | `				iNest--;` |
|    7261 | 6006 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 6007 | `				if( iNest <= 0 ){` |
|     ! 0 | 6008 | `					break;` |
|       - | 6009 | `				}` |
|       2 | 6010 | `			}` |
|    7264 | 6011 | `			pEnd2++;` |
|       2 | 6012 | `		}` |
|    7244 | 6013 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 6014 | `			pEnd = pEnd2;` |
|     ! 0 | 6015 | `		}` |
|    3621 | 6016 | `	}` |
|  371666 | 6017 | `	if( pEnd > pGen->pIn ){` |
|  371658 | 6018 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 6019 | `		/* Swap delimiter */` |
|  371658 | 6020 | `		pGen->pEnd = pEnd;` |
|       - | 6021 | `		/* Try to get an expression tree */` |
|  371658 | 6022 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  371658 | 6023 | `		if( rc == SXRET_OK && pRoot ){` |
|  371496 | 6024 | `			rc = SXRET_OK;` |
|  371496 | 6025 | `			if( xTreeValidator ){` |
|       - | 6026 | `				/* Call the upper layer validator callback */` |
|    7456 | 6027 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    3727 | 6028 | `			}` |
|  371496 | 6029 | `			if( rc != SXERR_ABORT ){` |
|       - | 6030 | `				/* Generate code for the given tree */` |
|  371496 | 6031 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  185747 | 6032 | `			}` |
|  371496 | 6033 | `			nExpr = 1;` |
|  185747 | 6034 | `		}` |
|       - | 6035 | `		/* Release the whole tree */` |
|  371658 | 6036 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 6037 | `		/* Synchronize token stream */` |
|  371658 | 6038 | `		pGen->pEnd = pTmp;` |
|  371658 | 6039 | `		pGen->pIn  = pEnd;` |
|  371658 | 6040 | `		if( rc == SXERR_ABORT ){` |
|       3 | 6041 | `			SySetRelease(&sExprNode);` |
|       3 | 6042 | `			return SXERR_ABORT;` |
|       - | 6043 | `		}` |
|  185827 | 6044 | `	}` |
|  371664 | 6045 | `	SySetRelease(&sExprNode);` |
|  371664 | 6046 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  185834 | 6047 |  |
|       - | 6048 | `/*` |
|       - | 6049 | ` * Return a pointer to the node construct handler associated` |
|       - | 6050 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 6051 | ` */` |
|  106610 | 6052 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 6053 |  |
|  106612 | 6054 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 6055 | `		/* Numeric literal: Either real or integer */` |
|   59154 | 6056 | `		return PH7_CompileNumLiteral;` |
|   47460 | 6057 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 6058 | `		/* Double quoted string */` |
|   12564 | 6059 | `		return PH7_CompileString;` |
|   34898 | 6060 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 6061 | `		/* Single quoted string */` |
|   34838 | 6062 | `		return PH7_CompileSimpleString;` |
|      62 | 6063 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 6064 | `		/* Heredoc */` |
|      28 | 6065 | `		return PH7_CompileHereDoc;` |
|      36 | 6066 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 6067 | `		/* Nowdoc */` |
|      29 | 6068 | `		return PH7_CompileNowDoc;` |
|       7 | 6069 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 6070 | `		/* Backtick quoted string */` |
|       5 | 6071 | `		return PH7_CompileBacktic;` |
|       - | 6072 | `	}` |
|       3 | 6073 | `	return 0;` |
|   53307 | 6074 |  |
|       - | 6075 | `/*` |
|       - | 6076 | ` * PHP Language construct table.` |
|       - | 6077 | ` */` |
|       - | 6078 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 6079 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 6080 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 6081 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 6082 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 6083 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 6084 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 6085 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 6086 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 6087 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 6088 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 6089 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 6090 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 6091 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 6092 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 6093 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 6094 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 6095 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 6096 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 6097 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 6098 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 6099 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 6100 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 6101 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  }   /* declare statement */` |
|       - | 6102 | `};` |
|       - | 6103 | `/*` |
|       - | 6104 | ` * Return a pointer to the statement handler routine associated` |
|       - | 6105 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 6106 | ` */` |
|  223498 | 6107 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 6108 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 6109 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 6110 | `	)` |
|       2 | 6111 |  |
|  223500 | 6112 | `	sxu32 n = 0;` |
|  823205 | 6113 | `	for(;;){` |
| 1646412 | 6114 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   22104 | 6115 | `			break;` |
|       - | 6116 | `		}` |
| 1624310 | 6117 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  201398 | 6118 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 6119 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 6120 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 6121 | `					/* 'static' (class context),return null */` |
|     ! 0 | 6122 | `					return 0;` |
|       - | 6123 | `				}` |
|     ! 0 | 6124 | `			}` |
|       - | 6125 | `			/* Return a pointer to the handler.` |
|       - | 6126 | `			*/` |
|  201398 | 6127 | `			return aLangConstruct[n].xConstruct;` |
|       - | 6128 | `		}` |
| 1422914 | 6129 | `		n++;` |
|       2 | 6130 | `	}` |
|   22104 | 6131 | `	if( pLookahed ){` |
|   22104 | 6132 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    5334 | 6133 | `			return PH7_CompileClassInterface;` |
|   16772 | 6134 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   14436 | 6135 | `			return PH7_CompileClass;` |
|    2336 | 6136 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       7 | 6137 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       6 | 6138 | `				return PH7_CompileAbstractClass;` |
|    2332 | 6139 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 6140 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 6141 | `				return PH7_CompileFinalClass;` |
|       - | 6142 | `		}` |
|    1165 | 6143 | `	}` |
|       - | 6144 | `	/* Not a language construct */` |
|    2332 | 6145 | `	return 0;` |
|  111751 | 6146 |  |
|       - | 6147 | `/*` |
|       - | 6148 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 6149 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 6150 | ` */` |
|    2330 | 6151 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 6152 |  |
|       - | 6153 | `	int rc;` |
|    2332 | 6154 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|    2332 | 6155 | `	if( rc == FALSE ){` |
|      10 | 6156 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|       - | 6157 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 6158 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 6159 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 6160 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 6161 | `			*/` |
|       - | 6162 | `			){` |
|       3 | 6163 | `				rc = TRUE;` |
|       1 | 6164 | `		}` |
|       4 | 6165 | `	}` |
|    2332 | 6166 | `	return rc;` |
|       2 | 6167 |  |
|       - | 6168 | `/*` |
|       - | 6169 | ` * Compile a PHP chunk.` |
|       - | 6170 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 6171 | ` * takes care of generating the appropriate error message.` |
|       - | 6172 | ` */` |
|  309794 | 6173 | `static sxi32 GenStateCompileChunk(` |
|       - | 6174 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 6175 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 6176 | `	)` |
|       2 | 6177 |  |
|       - | 6178 | `	ProcLangConstruct xCons;` |
|       - | 6179 | `	sxi32 rc;` |
|  309796 | 6180 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  185281 | 6181 | `	for(;;){` |
|  370564 | 6182 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6183 | `			/* No more input to process */` |
|    9304 | 6184 | `			break;` |
|       - | 6185 | `		}` |
|  361262 | 6186 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 6187 | `			/* Compile block */` |
|      12 | 6188 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 6189 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6190 | `				break;` |
|       - | 6191 | `			}` |
|       7 | 6192 | `		}else{` |
|  361252 | 6193 | `			xCons = 0;` |
|  361252 | 6194 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  223500 | 6195 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 6196 | `				/* Try to extract a language construct handler */` |
|  223500 | 6197 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  223500 | 6198 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      10 | 6199 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6200 | `						"Syntax error: Unexpected keyword '%z'",` |
|       6 | 6201 | `						&pGen->pIn->sData);` |
|       7 | 6202 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6203 | `						break;` |
|       - | 6204 | `					}` |
|       - | 6205 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 6206 | `					 * this erroneous statement.` |
|       - | 6207 | `					 */` |
|       7 | 6208 | `					xCons = PH7_ErrorRecover;` |
|       3 | 6209 | `				}` |
|  249503 | 6210 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   21160 | 6211 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 6212 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 6213 | `				xCons = PH7_CompileLabel;` |
|      56 | 6214 | `			}` |
|  361252 | 6215 | `			if( xCons == 0 ){` |
|       - | 6216 | `				/* Assume an expression an try to compile it */` |
|  139966 | 6217 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  139966 | 6218 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 6219 | `					/* Pop l-value */` |
|  139836 | 6220 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   69917 | 6221 | `				}` |
|   69984 | 6222 | `			}else{` |
|       - | 6223 | `				/* Go compile the sucker */` |
|  221288 | 6224 | `				rc = xCons(&(*pGen));` |
|       - | 6225 | `			}` |
|  361252 | 6226 | `			if( rc == SXERR_ABORT ){` |
|       - | 6227 | `				/* Request to abort compilation */` |
|       3 | 6228 | `				break;` |
|       - | 6229 | `			}` |
|       - | 6230 | `		}` |
|       - | 6231 | `		/* Ignore trailing semi-colons ';' */` |
|  592452 | 6232 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  231194 | 6233 | `			pGen->pIn++;` |
|       2 | 6234 | `		}` |
|  361260 | 6235 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 6236 | `			/* Compile a single statement and return */` |
|  300492 | 6237 | `			break;` |
|       - | 6238 | `		}` |
|       - | 6239 | `		/* LOOP ONE */` |
|       - | 6240 | `		/* LOOP TWO */` |
|       - | 6241 | `		/* LOOP THREE */` |
|       - | 6242 | `		/* LOOP FOUR */` |
|       2 | 6243 | `	}` |
|       - | 6244 | `	/* Return compilation status */` |
|  309796 | 6245 | `	return rc;` |
|       2 | 6246 |  |
|       - | 6247 | `/*` |
|       - | 6248 | ` * Compile a Raw PHP chunk.` |
|       - | 6249 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 6250 | ` * takes care of generating the appropriate error message.` |
|       - | 6251 | ` */` |
|    9310 | 6252 | `static sxi32 PH7_CompilePHP(` |
|       - | 6253 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 6254 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 6255 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 6256 | `	)` |
|       2 | 6257 |  |
|    9312 | 6258 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 6259 | `	sxi32 rc;` |
|       - | 6260 | `	/* Reset the token set */` |
|    9312 | 6261 | `	SySetReset(&(*pTokenSet));` |
|       - | 6262 | `	/* Mark as the default token set */` |
|    9312 | 6263 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 6264 | `	/* Advance the stream cursor */` |
|    9312 | 6265 | `	pGen->pRawIn++;` |
|       - | 6266 | `	/* Tokenize the PHP chunk first */` |
|    9312 | 6267 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 6268 | `	/* Point to the head and tail of the token stream. */` |
|    9312 | 6269 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|    9312 | 6270 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|    9312 | 6271 | `	if( is_expr ){` |
|       5 | 6272 | `		rc = SXERR_EMPTY;` |
|       5 | 6273 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 6274 | `			/* A simple expression,compile it */` |
|       5 | 6275 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       2 | 6276 | `		}` |
|       - | 6277 | `		/* Emit the DONE instruction */` |
|       5 | 6278 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       5 | 6279 | `		return SXRET_OK;` |
|       - | 6280 | `	}` |
|    9308 | 6281 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 6282 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 6283 | `		/*` |
|       - | 6284 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 6285 | `		 * According to the PHP reference manual:` |
|       - | 6286 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 6287 | `		 *  immediately follow` |
|       - | 6288 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 6289 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 6290 | `		 * Symisc extension:` |
|       - | 6291 | `		 *   This short syntax works with all PHP opening` |
|       - | 6292 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 6293 | `		 *   only short tag.` |
|       - | 6294 | `		 */` |
|       - | 6295 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 6296 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 6297 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 6298 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 6299 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 6300 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 6301 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 6302 | `		}` |
|       3 | 6303 | `		return SXRET_OK;` |
|       - | 6304 | `	}` |
|       - | 6305 | `	/* Compile the PHP chunk */` |
|    9306 | 6306 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 6307 | `	/* Fix exceptions jumps */` |
|    9306 | 6308 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6309 | `	/* Fix gotos now, the jump destination is resolved */` |
|    9306 | 6310 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 6311 | `		rc = SXERR_ABORT;` |
|       1 | 6312 | `	}` |
|       - | 6313 | `	/* Reset container */` |
|    9306 | 6314 | `	SySetReset(&pGen->aGoto);` |
|    9306 | 6315 | `	SySetReset(&pGen->aLabel);` |
|       - | 6316 | `	/* Compilation result */` |
|    9306 | 6317 | `	return rc;` |
|    4657 | 6318 |  |
|       - | 6319 | `/*` |
|       - | 6320 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 6321 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 6322 | ` * This is the only compile interface exported from this file.` |
|       - | 6323 | ` */` |
|   10830 | 6324 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 6325 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 6326 | `	SyString *pScript,  /* Script to compile */` |
|       - | 6327 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 6328 | `	)` |
|       2 | 6329 |  |
|       - | 6330 | `	SySet aPhpToken,aRawToken;` |
|       - | 6331 | `	ph7_gen_state *pCodeGen;` |
|       - | 6332 | `	ph7_value *pRawObj;` |
|       - | 6333 | `	sxu32 nObjIdx;` |
|       - | 6334 | `	sxi32 nRawObj;` |
|       - | 6335 | `	int is_expr;` |
|       - | 6336 | `	sxi32 rc;` |
|   10832 | 6337 | `	if( pScript->nByte < 1 ){` |
|       - | 6338 | `		/* Nothing to compile */` |
|     ! 0 | 6339 | `		return PH7_OK;` |
|       - | 6340 | `	}` |
|       - | 6341 | `	/* Initialize the tokens containers */` |
|   10832 | 6342 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   10832 | 6343 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   10832 | 6344 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   10832 | 6345 | `	is_expr = 0;` |
|   10832 | 6346 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 6347 | `		SyToken sTmp;` |
|       - | 6348 | `		/* PHP only: -*/` |
|    1802 | 6349 | `		sTmp.nLine = 1;` |
|    1802 | 6350 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    1802 | 6351 | `		sTmp.pUserData = 0;` |
|    1802 | 6352 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    1802 | 6353 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    1802 | 6354 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 6355 | `			/* A simple PHP expression */` |
|       5 | 6356 | `			is_expr = 1;` |
|       2 | 6357 | `		}` |
|     902 | 6358 | `	}else{` |
|       - | 6359 | `		/* Tokenize raw text */` |
|    9032 | 6360 | `		SySetAlloc(&aRawToken,32);` |
|    9032 | 6361 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 6362 | `	}` |
|   10832 | 6363 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 6364 | `	/* Process high-level tokens */` |
|   10832 | 6365 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   10832 | 6366 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   10832 | 6367 | `	rc = PH7_OK;` |
|   10832 | 6368 | `	if( is_expr ){` |
|       - | 6369 | `		/* Compile the expression */` |
|       5 | 6370 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       5 | 6371 | `		goto cleanup;` |
|       - | 6372 | `	}` |
|   10828 | 6373 | `	nObjIdx = 0;` |
|       - | 6374 | `	/* Start the compilation process */` |
|    9932 | 6375 | `	for(;;){` |
|   29168 | 6376 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   10824 | 6377 | `			break; /* No more tokens to process */` |
|       - | 6378 | `		}` |
|   18346 | 6379 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 6380 | `			/* Compile the PHP chunk */` |
|    9308 | 6381 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|    9308 | 6382 | `			if( rc == SXERR_ABORT ){` |
|       5 | 6383 | `				break;` |
|       - | 6384 | `			}` |
|    9304 | 6385 | `			continue;` |
|       - | 6386 | `		}` |
|       - | 6387 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    9040 | 6388 | `		nRawObj = 0;` |
|   18078 | 6389 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 6390 | `			/* Consume the raw chunk without any processing */` |
|    9040 | 6391 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    9040 | 6392 | `			if( pRawObj == 0 ){` |
|     ! 0 | 6393 | `				rc = SXERR_MEM;` |
|     ! 0 | 6394 | `				break;` |
|       - | 6395 | `			}` |
|       - | 6396 | `			/* Mark as constant and emit the load constant instruction */` |
|    9040 | 6397 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    9040 | 6398 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    9040 | 6399 | `			++nRawObj;` |
|    9040 | 6400 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 6401 | `		}` |
|    9040 | 6402 | `		if( nRawObj > 0 ){` |
|       - | 6403 | `			/* Emit the consume instruction */` |
|    9040 | 6404 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    4519 | 6405 | `		}` |
|    5415 | 6406 | `	}` |
|    5415 | 6407 | `cleanup:` |
|   10832 | 6408 | `	SySetRelease(&aRawToken);` |
|   10832 | 6409 | `	SySetRelease(&aPhpToken);` |
|   10832 | 6410 | `	return rc;` |
|    5417 | 6411 |  |
|       - | 6412 | `/*` |
|       - | 6413 | ` * Utility routines.Initialize the code generator.` |
|       - | 6414 | ` */` |
|    1774 | 6415 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 6416 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 6417 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 6418 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 6419 | `	)` |
|       2 | 6420 |  |
|    1776 | 6421 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 6422 | `	/* Zero the structure */` |
|    1776 | 6423 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 6424 | `	/* Initial state */` |
|    1776 | 6425 | `	pGen->pVm  = &(*pVm);` |
|    1776 | 6426 | `	pGen->xErr = xErr;` |
|    1776 | 6427 | `	pGen->pErrData = pErrData;` |
|    1776 | 6428 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    1776 | 6429 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    1776 | 6430 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    1776 | 6431 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 6432 | `	/* Error log buffer */` |
|    1776 | 6433 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 6434 | `	/* General purpose working buffer */` |
|    1776 | 6435 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 6436 | `	/* Create the global scope */` |
|    1776 | 6437 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 6438 | `	/* Point to the global scope */` |
|    1776 | 6439 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    1776 | 6440 | `	return SXRET_OK;` |
|       2 | 6441 |  |
|       - | 6442 | `/*` |
|       - | 6443 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 6444 | ` */` |
|   12346 | 6445 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 6446 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 6447 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 6448 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 6449 | `	)` |
|       2 | 6450 |  |
|   12348 | 6451 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 6452 | `	GenBlock *pBlock,*pParent;` |
|       - | 6453 | `	/* Reset state */` |
|   12348 | 6454 | `	SySetReset(&pGen->aLabel);` |
|   12348 | 6455 | `	SySetReset(&pGen->aGoto);` |
|   12348 | 6456 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   12348 | 6457 | `	SyBlobRelease(&pGen->sWorker);` |
|       - | 6458 | `	/* Point to the global scope */` |
|   12348 | 6459 | `	pBlock = pGen->pCurrent;` |
|   12348 | 6460 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 6461 | `		pParent = pBlock->pParent;` |
|     ! 0 | 6462 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 6463 | `		pBlock = pParent;` |
|     ! 0 | 6464 | `	}` |
|   12348 | 6465 | `	pGen->xErr = xErr;` |
|   12348 | 6466 | `	pGen->pErrData = pErrData;` |
|   12348 | 6467 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   12348 | 6468 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   12348 | 6469 | `	pGen->pIn = pGen->pEnd = 0;` |
|   12348 | 6470 | `	pGen->nErr = 0;` |
|   12348 | 6471 | `	return SXRET_OK;` |
|       2 | 6472 |  |
|       - | 6473 | `/*` |
|       - | 6474 | ` * Generate a compile-time error message.` |
|       - | 6475 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 6476 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 6477 | ` * abort compilation immediately.` |
|       - | 6478 | ` */` |
|     468 | 6479 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 6480 |  |
|     470 | 6481 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     470 | 6482 | `	const char *zErr = "Error";` |
|       - | 6483 | `	SyString *pFile;` |
|       - | 6484 | `	va_list ap;` |
|       - | 6485 | `	sxi32 rc;` |
|       - | 6486 | `	/* Reset the working buffer */` |
|     470 | 6487 | `	SyBlobReset(pWorker);` |
|       - | 6488 | `	/* Peek the processed file path if available */` |
|     470 | 6489 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     470 | 6490 | `	if( pFile && pGen->xErr ){` |
|       - | 6491 | `		/* Append file name */` |
|     467 | 6492 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|     467 | 6493 | `		SyBlobAppend(pWorker,(const void *)": ",sizeof(": ")-1);` |
|     233 | 6494 | `	}` |
|     470 | 6495 | `	if( nErrType == E_ERROR ){` |
|       - | 6496 | `		/* Increment the error counter */` |
|     416 | 6497 | `		pGen->nErr++;` |
|     416 | 6498 | `		if( pGen->nErr > 15 ){` |
|       - | 6499 | `			/* Error count limit reached */` |
|       5 | 6500 | `			if( pGen->xErr ){` |
|       5 | 6501 | `				SyBlobFormat(pWorker,"%u Error count limit reached,PH7 is aborting compilation\n",nLine);` |
|       5 | 6502 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       - | 6503 | `					/* Consume the generated error message */` |
|       5 | 6504 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 6505 | `				}` |
|       2 | 6506 | `			}` |
|       - | 6507 | `			/* Abort immediately */` |
|       5 | 6508 | `			return SXERR_ABORT;` |
|       - | 6509 | `		}` |
|     205 | 6510 | `	}` |
|     466 | 6511 | `	if( pGen->xErr == 0 ){` |
|       - | 6512 | `		/* No available error consumer,return immediately */` |
|       3 | 6513 | `		return SXRET_OK;` |
|       - | 6514 | `	}` |
|     463 | 6515 | `	switch(nErrType){` |
|      39 | 6516 | `	case E_WARNING: zErr = "Warning";     break;` |
|     ! 0 | 6517 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      17 | 6518 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 6519 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 6520 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 6521 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     204 | 6522 | `	default:` |
|     408 | 6523 | `		break;` |
|       - | 6524 | `	}` |
|     463 | 6525 | `	rc = SXRET_OK;` |
|       - | 6526 | `	/* Format the error message */` |
|     463 | 6527 | `	SyBlobFormat(pWorker,"%u %s:  ",nLine,zErr);` |
|     463 | 6528 | `	va_start(ap,zFormat);` |
|     463 | 6529 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     463 | 6530 | `	va_end(ap);` |
|       - | 6531 | `	/* Append a new line */` |
|     463 | 6532 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     463 | 6533 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 6534 | `		/* Consume the generated error message */` |
|     463 | 6535 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     231 | 6536 | `	}` |
|     463 | 6537 | `	return rc;` |
|     236 | 6538 |  |
|       - | 6539 |  |
